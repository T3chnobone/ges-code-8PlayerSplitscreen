//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "usercmd.h"
#include "bitbuf.h"
#include "checksum_md5.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// TF2 specific, need enough space for OBJ_LAST items from tf_shareddefs.h
#define WEAPON_SUBTYPE_BITS	6

#ifdef GE_DLL
//-----------------------------------------------------------------------------
// Split screen guest input serialization helpers
//-----------------------------------------------------------------------------
static void WriteWeaponCycle( bf_write *buf, signed char cycle )
{
	buf->WriteUBitLong( cycle > 0 ? 1 : ( cycle < 0 ? 2 : 0 ), 2 );
}

static signed char ReadWeaponCycle( bf_read *buf )
{
	int v = buf->ReadUBitLong( 2 );
	return v == 1 ? 1 : ( v == 2 ? -1 : 0 );
}

static void WriteDeltaFloat( bf_write *buf, float to, float from, bool bDelta )
{
	if ( bDelta && to == from )
	{
		buf->WriteOneBit( 0 );
	}
	else
	{
		buf->WriteOneBit( 1 );
		buf->WriteFloat( to );
	}
}

static void WriteSplitGuests( bf_write *buf, const CUserCmd *to, const CUserCmd *from )
{
	WriteWeaponCycle( buf, to->hostweaponcycle );

	int mask = 0;
	for ( int i = 0; i < GES_SS_MAX_GUESTS; i++ )
	{
		if ( to->splitguests[i].active )
			mask |= (1 << i);
	}

	if ( !mask )
	{
		buf->WriteOneBit( 0 );
		return;
	}

	buf->WriteOneBit( 1 );
	buf->WriteUBitLong( mask, GES_SS_MAX_GUESTS );

	for ( int i = 0; i < GES_SS_MAX_GUESTS; i++ )
	{
		if ( !(mask & (1 << i)) )
			continue;

		const CGESplitGuestCmd &g = to->splitguests[i];
		const CGESplitGuestCmd &f = from->splitguests[i];
		const bool bDelta = f.active;

		WriteDeltaFloat( buf, g.viewangles[PITCH], f.viewangles[PITCH], bDelta );
		WriteDeltaFloat( buf, g.viewangles[YAW], f.viewangles[YAW], bDelta );
		WriteDeltaFloat( buf, g.forwardmove, f.forwardmove, bDelta );
		WriteDeltaFloat( buf, g.sidemove, f.sidemove, bDelta );

		if ( bDelta && g.buttons == f.buttons )
		{
			buf->WriteOneBit( 0 );
		}
		else
		{
			buf->WriteOneBit( 1 );
			buf->WriteUBitLong( g.buttons, 32 );
		}

		WriteWeaponCycle( buf, g.weaponcycle );
		buf->WriteUBitLong( g.anglereset & GES_SS_ANGLERESET_MASK, GES_SS_ANGLERESET_BITS );
	}
}

static void ReadSplitGuests( bf_read *buf, CUserCmd *move, CUserCmd *from )
{
	move->hostweaponcycle = ReadWeaponCycle( buf );

	int mask = 0;
	if ( buf->ReadOneBit() )
		mask = buf->ReadUBitLong( GES_SS_MAX_GUESTS );

	for ( int i = 0; i < GES_SS_MAX_GUESTS; i++ )
	{
		CGESplitGuestCmd &g = move->splitguests[i];

		if ( !(mask & (1 << i)) )
		{
			g.Reset();
			continue;
		}

		// Start from the previous command if it had this guest, otherwise from nothing
		if ( from->splitguests[i].active )
			g = from->splitguests[i];
		else
			g.Reset();

		g.active = true;

		if ( buf->ReadOneBit() )
			g.viewangles[PITCH] = buf->ReadFloat();
		if ( buf->ReadOneBit() )
			g.viewangles[YAW] = buf->ReadFloat();
		g.viewangles[ROLL] = 0;
		if ( buf->ReadOneBit() )
			g.forwardmove = buf->ReadFloat();
		if ( buf->ReadOneBit() )
			g.sidemove = buf->ReadFloat();
		if ( buf->ReadOneBit() )
			g.buttons = buf->ReadUBitLong( 32 );

		g.weaponcycle = ReadWeaponCycle( buf );
		g.anglereset = buf->ReadUBitLong( GES_SS_ANGLERESET_BITS );
	}
}
#endif // GE_DLL

//-----------------------------------------------------------------------------
// Purpose: Write a delta compressed user command.
// Input  : *buf - 
//			*to - 
//			*from - 
// Output : static
//-----------------------------------------------------------------------------
void WriteUsercmd( bf_write *buf, const CUserCmd *to, const CUserCmd *from )
{
	if ( to->command_number != ( from->command_number + 1 ) )
	{
		buf->WriteOneBit( 1 );
		buf->WriteUBitLong( to->command_number, 32 );
	}
	else
	{
		buf->WriteOneBit( 0 );
	}

	if ( to->tick_count != ( from->tick_count + 1 ) )
	{
		buf->WriteOneBit( 1 );
		buf->WriteUBitLong( to->tick_count, 32 );
	}
	else
	{
		buf->WriteOneBit( 0 );
	}


	if ( to->viewangles[ 0 ] != from->viewangles[ 0 ] )
	{
		buf->WriteOneBit( 1 );
		buf->WriteFloat( to->viewangles[ 0 ] );
	}
	else
	{
		buf->WriteOneBit( 0 );
	}

	if ( to->viewangles[ 1 ] != from->viewangles[ 1 ] )
	{
		buf->WriteOneBit( 1 );
		buf->WriteFloat( to->viewangles[ 1 ] );
	}
	else
	{
		buf->WriteOneBit( 0 );
	}

	if ( to->viewangles[ 2 ] != from->viewangles[ 2 ] )
	{
		buf->WriteOneBit( 1 );
		buf->WriteFloat( to->viewangles[ 2 ] );
	}
	else
	{
		buf->WriteOneBit( 0 );
	}

	if ( to->forwardmove != from->forwardmove )
	{
		buf->WriteOneBit( 1 );
		buf->WriteFloat( to->forwardmove );
	}
	else
	{
		buf->WriteOneBit( 0 );
	}

	if ( to->sidemove != from->sidemove )
	{
		buf->WriteOneBit( 1 );
		buf->WriteFloat( to->sidemove );
	}
	else
	{
		buf->WriteOneBit( 0 );
	}

	if ( to->upmove != from->upmove )
	{
		buf->WriteOneBit( 1 );
		buf->WriteFloat( to->upmove );
	}
	else
	{
		buf->WriteOneBit( 0 );
	}

	if ( to->buttons != from->buttons )
	{
		buf->WriteOneBit( 1 );
	  	buf->WriteUBitLong( to->buttons, 32 );
 	}
	else
	{
		buf->WriteOneBit( 0 );
	}

	if ( to->impulse != from->impulse )
	{
		buf->WriteOneBit( 1 );
	    buf->WriteUBitLong( to->impulse, 8 );
	}
	else
	{
		buf->WriteOneBit( 0 );
	}


	if ( to->weaponselect != from->weaponselect )
	{
		buf->WriteOneBit( 1 );
		buf->WriteUBitLong( to->weaponselect, MAX_EDICT_BITS );

		if ( to->weaponsubtype != from->weaponsubtype )
		{
			buf->WriteOneBit( 1 );
			buf->WriteUBitLong( to->weaponsubtype, WEAPON_SUBTYPE_BITS );
		}
		else
		{
			buf->WriteOneBit( 0 );
		}
	}
	else
	{
		buf->WriteOneBit( 0 );
	}


	// TODO: Can probably get away with fewer bits.
	if ( to->mousedx != from->mousedx )
	{
		buf->WriteOneBit( 1 );
		buf->WriteShort( to->mousedx );
	}
	else
	{
		buf->WriteOneBit( 0 );
	}

	if ( to->mousedy != from->mousedy )
	{
		buf->WriteOneBit( 1 );
		buf->WriteShort( to->mousedy );
	}
	else
	{
		buf->WriteOneBit( 0 );
	}

#if defined( HL2_CLIENT_DLL )
	if ( to->entitygroundcontact.Count() != 0 )
	{
		buf->WriteOneBit( 1 );
		buf->WriteShort( to->entitygroundcontact.Count() );
		int i;
		for (i = 0; i < to->entitygroundcontact.Count(); i++)
		{
			buf->WriteUBitLong( to->entitygroundcontact[i].entindex, MAX_EDICT_BITS );
			buf->WriteBitCoord( to->entitygroundcontact[i].minheight );
			buf->WriteBitCoord( to->entitygroundcontact[i].maxheight );
		}
	}
	else
	{
		buf->WriteOneBit( 0 );
	}
#endif

#ifdef GE_DLL
	WriteSplitGuests( buf, to, from );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Read in a delta compressed usercommand.
// Input  : *buf - 
//			*move - 
//			*from - 
// Output : static void ReadUsercmd
//-----------------------------------------------------------------------------
void ReadUsercmd( bf_read *buf, CUserCmd *move, CUserCmd *from )
{
	// Assume no change
	*move = *from;

	if ( buf->ReadOneBit() )
	{
		move->command_number = buf->ReadUBitLong( 32 );
	}
	else
	{
		// Assume steady increment
		move->command_number = from->command_number + 1;
	}

	if ( buf->ReadOneBit() )
	{
		move->tick_count = buf->ReadUBitLong( 32 );
	}
	else
	{
		// Assume steady increment
		move->tick_count = from->tick_count + 1;
	}

	// Read direction
	if ( buf->ReadOneBit() )
	{
		move->viewangles[0] = buf->ReadFloat();
	}

	if ( buf->ReadOneBit() )
	{
		move->viewangles[1] = buf->ReadFloat();
	}

	if ( buf->ReadOneBit() )
	{
		move->viewangles[2] = buf->ReadFloat();
	}

	// Moved value validation and clamping to CBasePlayer::ProcessUsercmds()

	// Read movement
	if ( buf->ReadOneBit() )
	{
		move->forwardmove = buf->ReadFloat();
	}

	if ( buf->ReadOneBit() )
	{
		move->sidemove = buf->ReadFloat();
	}

	if ( buf->ReadOneBit() )
	{
		move->upmove = buf->ReadFloat();
	}

	// read buttons
	if ( buf->ReadOneBit() )
	{
		move->buttons = buf->ReadUBitLong( 32 );
	}

	if ( buf->ReadOneBit() )
	{
		move->impulse = buf->ReadUBitLong( 8 );
	}


	if ( buf->ReadOneBit() )
	{
		move->weaponselect = buf->ReadUBitLong( MAX_EDICT_BITS );
		if ( buf->ReadOneBit() )
		{
			move->weaponsubtype = buf->ReadUBitLong( WEAPON_SUBTYPE_BITS );
		}
	}


	move->random_seed = MD5_PseudoRandom( move->command_number ) & 0x7fffffff;

	if ( buf->ReadOneBit() )
	{
		move->mousedx = buf->ReadShort();
	}

	if ( buf->ReadOneBit() )
	{
		move->mousedy = buf->ReadShort();
	}

#if defined( HL2_DLL )
	if ( buf->ReadOneBit() )
	{
		move->entitygroundcontact.SetCount( buf->ReadShort() );

		int i;
		for (i = 0; i < move->entitygroundcontact.Count(); i++)
		{
			move->entitygroundcontact[i].entindex = buf->ReadUBitLong( MAX_EDICT_BITS );
			move->entitygroundcontact[i].minheight = buf->ReadBitCoord( );
			move->entitygroundcontact[i].maxheight = buf->ReadBitCoord( );
		}
	}
#endif

#ifdef GE_DLL
	ReadSplitGuests( buf, move, from );
#endif
}
