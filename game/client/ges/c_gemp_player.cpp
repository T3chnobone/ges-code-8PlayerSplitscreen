///////////// Copyright © 2008 LodleNet. All rights reserved. /////////////
//
//   Project     : Client (GES)
//   File        : c_gemp_player.cpp
//   Description :
//      [TODO: Write the purpose of c_gemp_player.cpp.]
//
//   Created On: 2/20/2009 5:38:42 PM
//   Created By: Mark Chandler <mailto:admin@lodle.net>
////////////////////////////////////////////////////////////////////////////

#include "cbase.h"
#include "c_gemp_player.h"
#include "c_gebot_player.h"
#include "takedamageinfo.h"
#include "gemp_gamerules.h"
#include "view.h"
#include "soundinfo.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


// Don't alias here
#if defined( CGEMPPlayer )
#undef CGEMPPlayer	
#endif

LINK_ENTITY_TO_CLASS( mp_player, C_GEMPPlayer );

IMPLEMENT_CLIENTCLASS_DT(C_GEMPPlayer, DT_GEMP_Player, CGEMPPlayer)
	RecvPropFloat( RECVINFO( m_flStartJumpZ ) ),
	RecvPropFloat( RECVINFO( m_flNextJumpTime ) ),
	RecvPropFloat( RECVINFO( m_flLastJumpTime )),
	RecvPropFloat( RECVINFO( m_flJumpPenalty )),
	RecvPropFloat( RECVINFO( m_flLastLandVelocity )),
	RecvPropFloat( RECVINFO( m_flRunTime )),
	RecvPropInt( RECVINFO(m_flRunCode) ),
	RecvPropInt( RECVINFO(m_iSteamIDHash) ),
	RecvPropArray3(RECVINFO_ARRAY(m_iWeaponSkinInUse), RecvPropInt(RECVINFO(m_iWeaponSkinInUse[0]))),
	RecvPropInt( RECVINFO( m_iSplitOwner ) ),
	RecvPropInt( RECVINFO( m_iSplitSlot ) ),
	RecvPropInt( RECVINFO( m_nSplitAngleReset ) ),
	RecvPropQAngles( RECVINFO( m_angSplitReset ) ),
	RecvPropInt( RECVINFO( m_iSplitClip ) ),
	RecvPropInt( RECVINFO( m_iSplitAmmo ) ),
	RecvPropInt( RECVINFO( m_iMatchKills ) ),
	RecvPropInt( RECVINFO( m_iNetMatchDeaths ) ),
	RecvPropInt( RECVINFO( m_iMatchSuicides ) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_GEMPPlayer )
	DEFINE_PRED_FIELD( m_flNextJumpTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flStartJumpZ, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flLastJumpTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE),
	DEFINE_PRED_FIELD( m_flJumpPenalty, FIELD_FLOAT, FTYPEDESC_INSENDTABLE),
	DEFINE_PRED_FIELD( m_flLastLandVelocity, FIELD_FLOAT, FTYPEDESC_INSENDTABLE),
	DEFINE_PRED_FIELD( m_flRunTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE),
	DEFINE_PRED_FIELD(m_flRunCode, FIELD_INTEGER, FTYPEDESC_INSENDTABLE),
END_PREDICTION_DATA()

void SpawnBlood (Vector vecSpot, const Vector &vecDir, int bloodColor, float flDamage);

C_GEMPPlayer::C_GEMPPlayer()
{
	m_flStartJumpZ = 0;
	m_flNextJumpTime = 1.0f;
	m_flLastJumpTime = 0;
	m_flJumpPenalty = 0;
	m_flLastLandVelocity = 0;
	m_flRunTime = 0;
	m_flRunCode = 0;

	memset(m_iWeaponSkinInUse, 0, WEAPON_RANDOM);

	m_iSplitOwner = 0;
	m_iSplitSlot = 0;
	m_nSplitAngleReset = 0;
	m_angSplitReset.Init();
	m_iSplitClip = 0;
	m_iSplitAmmo = 0;
	m_iMatchKills = 0;
	m_iNetMatchDeaths = 0;
	m_iMatchSuicides = 0;
}

void C_GEMPPlayer::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
	Vector vecOrigin = ptr->endpos - vecDir * 4;
	float flDistance = 0.0f;
	
	if ( info.GetAttacker() )
	{
		flDistance = (ptr->endpos - info.GetAttacker()->GetAbsOrigin()).Length();
	}

	if ( m_takedamage )
	{
		AddMultiDamage( info, this );

		int blood = BloodColor();
		
		CBaseEntity *pAttacker = info.GetAttacker();
		if ( pAttacker )
		{
			if ( GEMPRules()->IsTeamplay() && pAttacker->InSameTeam( this ) == true )
				return;
		}

		if ( blood != DONT_BLEED )
		{
			SpawnBlood( vecOrigin, vecDir, blood, flDistance );// a little surface blood.
			TraceBleed( flDistance, vecDir, ptr, info.GetDamageType() );
		}
	}
}

Vector C_GEMPPlayer::GetChaseCamViewOffset( CBaseEntity *target )
{
	// If we are specing an alive NPC return the player height always
	if ( target && target->IsNPC() && target->IsAlive() )
		return VEC_VIEW;

	return BaseClass::GetChaseCamViewOffset( target );
}

#include "c_npc_gebase.h"

C_GEMPPlayer* ToGEMPPlayer( CBaseEntity *pEntity )
{
	if ( !pEntity )
		return NULL;

	if ( pEntity->IsNPC() )
	{
#ifdef _DEBUG
		return dynamic_cast<C_GEMPPlayer*>( ((C_NPC_GEBase*)pEntity)->GetBotPlayer() );
#else
		return static_cast<C_GEMPPlayer*>( ((C_NPC_GEBase*)pEntity)->GetBotPlayer() );
#endif
	}
	else if ( pEntity->IsPlayer() )
	{
#ifdef _DEBUG
		return dynamic_cast<C_GEMPPlayer*>( pEntity );
#else
		return static_cast<C_GEMPPlayer*>( pEntity );
#endif
	}

	return NULL;
}

bool C_GEMPPlayer::GetSoundSpatialization( SpatializationInfo_t& info )
{
	if ( !BaseClass::GetSoundSpatialization( info ) )
		return false;

	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	if ( info.pOrigin && pLocal && pLocal != this && IsSplitGuest() && GetSplitOwnerIndex() == pLocal->entindex() )
		*info.pOrigin = MainViewOrigin();

	return true;
}
