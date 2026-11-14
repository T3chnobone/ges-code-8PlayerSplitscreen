//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "cbase.h"
#include "fx.h"
#include "c_te_effect_dispatch.h"
#include "basecombatweapon_shared.h"
#include "baseviewmodel_shared.h"
#include "particles_new.h"
#ifdef GE_DLL
#include "ge_splitscreen_client.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar r_drawtracers( "r_drawtracers", "1", FCVAR_CHEAT );
ConVar r_drawtracers_firstperson( "r_drawtracers_firstperson", "1", FCVAR_ARCHIVE, "Toggle visibility of first person weapon tracers" );

#define	TRACER_SPEED			5000 

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Vector GetTracerOrigin( const CEffectData &data )
{
	Vector vecStart = data.m_vStart;
	QAngle vecAngles;

	int iAttachment = data.m_nAttachmentIndex;

	// Attachment?
	if ( data.m_fFlags & TRACER_FLAG_USEATTACHMENT )
	{
		C_BaseViewModel *pViewModel = NULL;

		// If the entity specified is a weapon being carried by this player, use the viewmodel instead
		IClientRenderable *pRenderable = data.GetRenderable();
		if ( !pRenderable )
			return vecStart;

		C_BaseEntity *pEnt = data.GetEntity();

// This check should probably be for all multiplayer games, investigate later
#if defined( HL2MP ) || defined( TF_CLIENT_DLL )
		if ( pEnt && pEnt->IsDormant() )
			return vecStart;
#endif

		C_BaseCombatWeapon *pWpn = dynamic_cast<C_BaseCombatWeapon *>( pEnt );
		if ( pWpn && pWpn->ShouldDrawUsingViewModel() )
		{
			C_BasePlayer *player = ToBasePlayer( pWpn->GetOwner() );

			// Use GetRenderedWeaponModel() instead?
			pViewModel = player ? player->GetViewModel( 0 ) : NULL;
			if ( pViewModel )
			{
				// Get the viewmodel and use it instead
				pRenderable = pViewModel;
			}
		}

		// Get the attachment origin
		if ( !pRenderable->GetAttachment( iAttachment, vecStart, vecAngles ) )
		{
			DevMsg( "GetTracerOrigin: Couldn't find attachment %d on model %s\n", iAttachment, 
				modelinfo->GetModelName( pRenderable->GetModel() ) );
		}
	}

	return vecStart;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TracerCallback( const CEffectData &data )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return;

	if ( !r_drawtracers.GetBool() )
		return;

	if ( !r_drawtracers_firstperson.GetBool() )
	{
		C_BasePlayer *pPlayer = dynamic_cast<C_BasePlayer*>( data.GetEntity() );

		if ( pPlayer && !pPlayer->ShouldDrawThisPlayer() )
			return;
	}

	// Grab the data
	Vector vecStart = GetTracerOrigin( data );
	float flVelocity = data.m_flScale;
	bool bWhiz = (data.m_fFlags & TRACER_FLAG_WHIZ);
	int iEntIndex = data.entindex();

	if ( iEntIndex && iEntIndex == player->index )
	{
		Vector	foo = data.m_vStart;
		QAngle	vangles;
		Vector	vforward, vright, vup;

		engine->GetViewAngles( vangles );
		AngleVectors( vangles, &vforward, &vright, &vup );

		VectorMA( data.m_vStart, 4, vright, foo );
		foo[2] -= 0.5f;

		FX_PlayerTracer( foo, (Vector&)data.m_vOrigin );
		return;
	}
	
	// Use default velocity if none specified
	if ( !flVelocity )
	{
		flVelocity = TRACER_SPEED;
	}

	// Do tracer effect
	FX_Tracer( (Vector&)vecStart, (Vector&)data.m_vOrigin, flVelocity, bWhiz );
}

DECLARE_CLIENT_EFFECT( "Tracer", TracerCallback );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ParticleTracerCallback( const CEffectData &data )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return;

	if ( !r_drawtracers.GetBool() )
		return;

	if ( !r_drawtracers_firstperson.GetBool() )
	{
		C_BasePlayer *pPlayer = dynamic_cast<C_BasePlayer*>( data.GetEntity() );

		if ( pPlayer && !pPlayer->ShouldDrawThisPlayer() )
			return;
	}

	// Grab the data
	Vector vecStart = GetTracerOrigin( data );
	Vector vecEnd = data.m_vOrigin;

	// Adjust view model tracers
	C_BaseEntity *pEntity = data.GetEntity();
	if ( data.entindex() && data.entindex() == player->index )
	{
		QAngle	vangles;
		Vector	vforward, vright, vup;

		engine->GetViewAngles( vangles );
		AngleVectors( vangles, &vforward, &vright, &vup );

		VectorMA( data.m_vStart, 4, vright, vecStart );
		vecStart[2] -= 0.5f;
	}

#ifdef GE_DLL
	// Split screen: a player of ours (player 1 or a guest) fires with their first
	// person gun, but the others see them in third person. Two tracers: from the
	// first person muzzle in the shooter's own view (owned by their viewmodel), and
	// from the gun in their hand in everyone else's view (owned by the weapon,
	// which the shooter doesn't see). See GESS_ShouldHideParticlesForCurrentView.
	C_BaseCombatWeapon *pSplitWpn = GESS_IsSplitActive() ? dynamic_cast<C_BaseCombatWeapon*>( pEntity ) : NULL;
	C_BasePlayer *pShooter = pSplitWpn ? ToBasePlayer( pSplitWpn->GetOwner() ) : NULL;
	C_BaseViewModel *pShooterVM = ( pShooter && pSplitWpn->ShouldDrawUsingViewModel() && ( data.m_fFlags & TRACER_FLAG_USEATTACHMENT ) ) ? pShooter->GetViewModel( 0 ) : NULL;
	if ( pShooterVM )
	{
		QAngle angFP;
		Vector vecDir = vecEnd - vecStart;
		VectorNormalize( vecDir );
		VectorAngles( vecDir, angFP );
		DispatchParticleEffect( data.m_nHitBox, vecStart, vecEnd, angFP, pShooterVM );

		// Third person: the weapon's own muzzle when it has its world model (a
		// guest's gun on our client), else in front of the shooter's right hand
		Vector vecWorldStart = data.m_vStart;
		QAngle angTmp;
		const int iWorldModel = pSplitWpn->GetWorldModelIndex();
		const int iMuzzle = ( iWorldModel > 0 && pSplitWpn->GetModelIndex() == iWorldModel ) ? pSplitWpn->LookupAttachment( "muzzle" ) : 0;
		if ( iMuzzle <= 0 || !pSplitWpn->GetAttachment( iMuzzle, vecWorldStart, angTmp ) )
		{
			const int iHand = pShooter->LookupAttachment( "anim_attachment_RH" );
			Vector vecForward;
			AngleVectors( pShooter->EyeAngles(), &vecForward );
			if ( iHand > 0 && pShooter->GetAttachment( iHand, vecWorldStart, angTmp ) )
				vecWorldStart += vecForward * 12.0f;
			else
				vecWorldStart = pShooter->EyePosition() + vecForward * 16.0f - Vector( 0, 0, 12.0f );
		}

		QAngle angTP;
		vecDir = vecEnd - vecWorldStart;
		VectorNormalize( vecDir );
		VectorAngles( vecDir, angTP );
		DispatchParticleEffect( data.m_nHitBox, vecWorldStart, vecEnd, angTP, pSplitWpn );

		if ( data.m_fFlags & TRACER_FLAG_WHIZ )
			FX_TracerSound( vecWorldStart, vecEnd, TRACER_TYPE_DEFAULT );
		return;
	}
#endif

	// Create the particle effect
	QAngle vecAngles;
	Vector vecToEnd = vecEnd - vecStart;
	VectorNormalize(vecToEnd);
	VectorAngles( vecToEnd, vecAngles );
	DispatchParticleEffect( data.m_nHitBox, vecStart, vecEnd, vecAngles, pEntity );

	if ( data.m_fFlags & TRACER_FLAG_WHIZ )
	{
		FX_TracerSound( vecStart, vecEnd, TRACER_TYPE_DEFAULT );	
	}
}

DECLARE_CLIENT_EFFECT( "ParticleTracer", ParticleTracerCallback );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TracerSoundCallback( const CEffectData &data )
{
	// Grab the data
	Vector vecStart = GetTracerOrigin( data );
	
	// Do tracer effect
	FX_TracerSound( vecStart, (Vector&)data.m_vOrigin, data.m_fFlags );
}

DECLARE_CLIENT_EFFECT( "TracerSound", TracerSoundCallback );

