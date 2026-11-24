///////////// Copyright 2026 GoldenEye: Source. All rights reserved. /////////////
//
//   Project     : Client (GES)
//   File        : ge_customhead.cpp
//   Description :
//      see ge_customhead.h
//
////////////////////////////////////////////////////////////////////////////
#include "cbase.h"
#include "ge_customhead.h"
#include "bone_setup.h"
#include "filesystem.h"
#include "c_hl2mp_player.h"
#include "ge_customhead_import.h"
#include "c_ge_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void GECustomHead_ImportedModelPath( const char *headFile, char *pszModel, int len )
{
	// The head's current compiled version (ge_customhead_import.cpp)
	char model[GE_CUSTOMHEAD_NAME_LEN];
	GECustomHead_ResolveModelName( headFile, model, sizeof( model ) );
	Q_snprintf( pszModel, len, "%s/%s.mdl", GE_CUSTOMHEAD_DIR, model );
}

bool GECustomHead_GetModel( int headChar, int headSkin, const char *headFile, char *pszModel, int len, int &skin )
{
	pszModel[0] = '\0';
	skin = 0;

	if ( headFile && headFile[0] )
	{
		GECustomHead_ImportedModelPath( headFile, pszModel, len );
		return filesystem->FileExists( pszModel, "GAME" );
	}

	const CGECharData *pChar = headChar >= 0 ? GECharacters()->Get( headChar ) : NULL;
	if ( !pChar || pChar->m_pSkins.Count() == 0 )
		return false;
	const GECharSkin *pSkin = pChar->m_pSkins[clamp( headSkin, 0, pChar->m_pSkins.Count() - 1 )];
	if ( !pSkin->szModel[0] )
		return false;

	Q_strncpy( pszModel, pSkin->szModel, len );
	skin = pSkin->iWorldSkin;
	return true;
}

//-----------------------------------------------------------------------------

C_GECustomHead::C_GECustomHead()
{
	m_szModel[0] = '\0';
	m_pMapHdr = m_pMapSrcHdr = NULL;
	m_iHeadBone = m_iSrcHeadBone = -1;
}

C_GECustomHead::~C_GECustomHead()
{
}

C_GECustomHead *C_GECustomHead::Create( C_BaseAnimating *pSource, const char *pszModel, int skin )
{
	if ( !pSource || !pszModel || !pszModel[0] )
		return NULL;

	// Character heads are precached by the map; an imported head isn't, so load it
	// now and give it a client-side model index
	int modelIndex = modelinfo->GetModelIndex( pszModel );
	if ( modelIndex == -1 )
	{
		if ( modelinfo->FindOrLoadModel( pszModel ) )
			modelIndex = modelinfo->RegisterDynamicModel( pszModel, true );
		if ( modelIndex == -1 || !modelinfo->GetModel( modelIndex ) )
		{
			Warning( "Custom head %s couldn't be loaded\n", pszModel );
			return NULL;
		}
	}

	C_GECustomHead *pHead = new C_GECustomHead;
	if ( !pHead->InitializeAsClientEntityByIndex( modelIndex, RENDER_GROUP_OPAQUE_ENTITY ) )
	{
		pHead->Release();
		return NULL;
	}

	// Needs the head bone, or there's nothing to put on the body (an imported
	// head may still be loading; it's drawn once it has)
	CStudioHdr *pHdr = pHead->GetModelPtr();
	if ( !pHead->IsDynamicModelLoading() && ( !pHdr || Studio_BoneIndexByName( pHdr, GE_CUSTOMHEAD_BONE ) < 0 ) )
	{
		Warning( "Custom head %s has no %s bone\n", pszModel, GE_CUSTOMHEAD_BONE );
		pHead->Release();
		return NULL;
	}

	Q_strncpy( pHead->m_szModel, pszModel, sizeof( pHead->m_szModel ) );
	pHead->m_hSource = pSource;
	pHead->m_nSkin = skin;
	pHead->SetSequence( 0 );
	pHead->SetCycle( 0 );
	pHead->SetPlaybackRate( 0 );
	pHead->DontRecordInTools();

	// Parented, so it moves with the body and the split screen hides it in its
	// own player's view like the rest of their body (GESS_ShouldHideForCurrentView)
	pHead->SetParent( pSource );
	pHead->SetLocalOrigin( vec3_origin );
	pHead->SetLocalAngles( vec3_angle );

	pSource->GESetHideHead( true );
	pHead->SetNextClientThink( CLIENT_THINK_ALWAYS );
	return pHead;
}

void C_GECustomHead::Destroy()
{
	C_BaseAnimating *pSource = m_hSource.Get();
	if ( pSource )
		pSource->GESetHideHead( false );
	m_hSource = NULL;
	SetParent( NULL );
	Release();
}

void C_GECustomHead::ClientThink()
{
	C_BaseAnimating *pSource = m_hSource.Get();
	if ( !pSource )
	{
		// The body is gone (ragdoll faded, player left)
		SetParent( NULL );
		Release();
		return;
	}

	// The player died: their ragdoll gets a copy of this head (normally done
	// when the ragdoll is made, GECustomHead_RagdollCreated; this catches the rest)
	C_HL2MP_Player *pPlayer = dynamic_cast<C_HL2MP_Player *>( m_hOwnerPlayer.Get() );
	if ( pPlayer )
	{
		C_BaseAnimating *pRagdoll = pPlayer->GetRagdoll();
		if ( pRagdoll && pRagdoll->GetModelPtr() )
			CopyToRagdoll( pRagdoll );
	}
}

void C_GECustomHead::CopyToRagdoll( C_BaseAnimating *pRagdoll )
{
	if ( !pRagdoll || pRagdoll == m_hLastRagdoll.Get() || pRagdoll->GEIsHidingHead() )
		return;
	m_hLastRagdoll = pRagdoll;
	C_GECustomHead::Create( pRagdoll, m_szModel, m_nSkin );
}

static C_GECustomHead *GECustomHead_ForPlayer( C_BaseAnimating *pPlayer )
{
	C_GEPlayer *pGEPlayer = dynamic_cast<C_GEPlayer *>( pPlayer );
	return pGEPlayer ? pGEPlayer->GetCustomHead() : NULL;
}

bool GECustomHead_PreRagdoll( C_BaseAnimating *pPlayer )
{
	if ( !pPlayer || !pPlayer->GEIsHidingHead() )
		return false;
	pPlayer->GESetHideHead( false );
	pPlayer->InvalidateBoneCache();
	return true;
}

void GECustomHead_RagdollCreated( C_BaseAnimating *pPlayer, C_BaseAnimating *pRagdoll, bool bWasHiding )
{
	if ( !pPlayer )
		return;
	if ( bWasHiding )
	{
		pPlayer->GESetHideHead( true );
		pPlayer->InvalidateBoneCache();
	}

	C_GECustomHead *pHead = GECustomHead_ForPlayer( pPlayer );
	if ( pHead && pRagdoll && pRagdoll->GetModelPtr() )
		pHead->CopyToRagdoll( pRagdoll );
}

bool C_GECustomHead::ShouldDraw()
{
	C_BaseAnimating *pSource = m_hSource.Get();
	if ( !pSource || pSource->IsDormant() || pSource->IsEffectActive( EF_NODRAW ) )
		return false;

	// A dead player's head is on their ragdoll now (its own copy); the player's
	// one would otherwise hang in the air where they died until they respawn
	C_BasePlayer *pPlayer = ToBasePlayer( pSource );
	if ( pPlayer && ( !pPlayer->IsAlive() || pPlayer->IsObserver() ) )
		return false;

	// Drawn when the body is (not in first person, not dead...)
	return pSource->ShouldDraw();
}

void C_GECustomHead::GetRenderBounds( Vector& theMins, Vector& theMaxs )
{
	// The head is wherever the body's head bone is: a box that holds a standing
	// player or a ragdoll lying around its origin
	theMins.Init( -80, -80, -80 );
	theMaxs.Init( 80, 80, 96 );
}

void C_GECustomHead::UpdateBoneMap( CStudioHdr *pHdr, CStudioHdr *pSrcHdr )
{
	if ( m_pMapHdr == pHdr->GetRenderHdr() && m_pMapSrcHdr == pSrcHdr->GetRenderHdr() )
		return;

	m_pMapHdr = pHdr->GetRenderHdr();
	m_pMapSrcHdr = pSrcHdr->GetRenderHdr();
	m_iHeadBone = Studio_BoneIndexByName( pHdr, GE_CUSTOMHEAD_BONE );
	m_iSrcHeadBone = Studio_BoneIndexByName( pSrcHdr, GE_CUSTOMHEAD_BONE );

	const int numBones = pHdr->numbones();
	m_SrcBone.SetCount( numBones );
	m_bUnderHead.SetCount( numBones );
	for ( int i = 0; i < numBones; i++ )
	{
		m_SrcBone[i] = (short)Studio_BoneIndexByName( pSrcHdr, pHdr->pBone( i )->pszName() );

		int b = i;
		while ( b >= 0 && b != m_iHeadBone )
			b = pHdr->boneParent( b );
		m_bUnderHead[i] = m_iHeadBone >= 0 && b == m_iHeadBone;
	}
}

void C_GECustomHead::BuildTransformations( CStudioHdr *hdr, Vector *pos, Quaternion q[], const matrix3x4_t &cameraTransform, int boneMask, CBoneBitList &boneComputed )
{
	C_BaseAnimating *pSource = m_hSource.Get();
	CStudioHdr *pSrcHdr = pSource ? pSource->GetModelPtr() : NULL;
	if ( !hdr || !pSrcHdr )
	{
		BaseClass::BuildTransformations( hdr, pos, q, cameraTransform, boneMask, boneComputed );
		return;
	}

	UpdateBoneMap( hdr, pSrcHdr );
	if ( m_iHeadBone < 0 || m_iSrcHeadBone < 0 )
	{
		BaseClass::BuildTransformations( hdr, pos, q, cameraTransform, boneMask, boneComputed );
		return;
	}

	// The body's head bone as animated (it hides its own head after that)
	pSource->SetupBones( NULL, -1, BONE_USED_BY_ANYTHING, gpGlobals->curtime );
	matrix3x4_t headMat;
	MatrixCopy( pSource->GEGetUnhiddenBone( m_iSrcHeadBone ), headMat );
	Vector headPos;
	MatrixGetColumn( headMat, 3, headPos );

	const int numBones = hdr->numbones();
	for ( int i = 0; i < numBones; i++ )
	{
		if ( !( hdr->boneFlags( i ) & boneMask ) )
			continue;

		matrix3x4_t &out = GetBoneForWrite( i );
		if ( i == m_iHeadBone )
		{
			MatrixCopy( headMat, out );
		}
		else if ( m_bUnderHead[i] )
		{
			// Hair etc. keep their own pose on the new head
			matrix3x4_t local;
			QuaternionMatrix( q[i], pos[i], local );
			ConcatTransforms( GetBone( hdr->boneParent( i ) ), local, out );
		}
		else
		{
			// The rest of this character shrinks to nothing, at the body's
			// matching bone so the neck tapers into the body's neck
			Vector at = headPos;
			if ( m_SrcBone[i] >= 0 )
				MatrixGetColumn( pSource->GetBone( m_SrcBone[i] ), 3, at );
			out.Init( vec3_origin, vec3_origin, vec3_origin, at );
		}
	}
}
