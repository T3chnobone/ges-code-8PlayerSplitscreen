///////////// Copyright 2026 GoldenEye: Source. All rights reserved. /////////////
//
//   Project     : Client (GES)
//   File        : ge_customhead.h
//   Description :
//      Draws a custom character's head (ge_customchar.h) on a body.
//
//      Every GoldenEye: Source character is one model with the head built in,
//      on the same ValveBiped skeleton. So a head is "swapped" at run time:
//        * the body hides its own head: ValveBiped.Bip01_Head1 (and the bones
//          under it, hair, ponytails) shrinks to nothing (C_BaseAnimating::GESetHideHead);
//        * a second model, C_GECustomHead, is drawn with its head bone placed on
//          the body's head bone and every other bone shrunk away, so only its
//          head shows. That model is either another character (its head) or an
//          imported head (models/ges_custom_heads/<name>.mdl, only a head bone).
//
//      Players get one from C_GEPlayer when their custom head is networked,
//      and a death ragdoll gets its own copy for as long as it lies there.
//
////////////////////////////////////////////////////////////////////////////
#ifndef GE_CUSTOMHEAD_H
#define GE_CUSTOMHEAD_H
#ifdef _WIN32
#pragma once
#endif

#include "c_baseanimating.h"
#include "ge_customchar.h"

class C_GECustomHead : public C_BaseAnimating
{
public:
	DECLARE_CLASS( C_GECustomHead, C_BaseAnimating );

	C_GECustomHead();
	virtual ~C_GECustomHead();

	// Puts the head model on pSource (which then hides its own head).
	// NULL when the model can't be loaded (e.g. an imported head this PC doesn't have).
	static C_GECustomHead *Create( C_BaseAnimating *pSource, const char *pszModel, int skin );

	C_BaseAnimating *GetSource() const { return m_hSource.Get(); }
	const char *GetHeadModel() const { return m_szModel; }

	// A player's head also puts a copy on each death ragdoll of that player
	void SetOwnerPlayer( C_BasePlayer *pPlayer ) { m_hOwnerPlayer = pPlayer; }

	// Removes the head and gives the source its own head back
	void Destroy();

	virtual void BuildTransformations( CStudioHdr *pStudioHdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed );
	virtual bool ShouldDraw();
	virtual void GetRenderBounds( Vector& theMins, Vector& theMaxs );
	virtual void ClientThink();

private:
	void UpdateBoneMap( CStudioHdr *pHdr, CStudioHdr *pSrcHdr );

	CHandle<C_BaseAnimating>	m_hSource;
	CHandle<C_BasePlayer>		m_hOwnerPlayer;		// Player heads only
	EHANDLE						m_hLastRagdoll;		// Last ragdoll a copy was made for
	char						m_szModel[MAX_PATH];

	// Bone map, rebuilt when either model changes
	const studiohdr_t			*m_pMapHdr;
	const studiohdr_t			*m_pMapSrcHdr;
	int							m_iHeadBone;		// Ours
	int							m_iSrcHeadBone;		// The body's
	CUtlVector<short>			m_SrcBone;			// Our bone -> the body's bone of the same name (-1)
	CUtlVector<bool>			m_bUnderHead;		// Our bone is the head or under it
};

// Works out the model for a networked custom head. False when there's none or
// it isn't available on this PC (the body then keeps its own head).
bool GECustomHead_GetModel( int headChar, int headSkin, const char *headFile, char *pszModel, int len, int &skin );

// Path of an imported head's model
void GECustomHead_ImportedModelPath( const char *headFile, char *pszModel, int len );

#endif // GE_CUSTOMHEAD_H
