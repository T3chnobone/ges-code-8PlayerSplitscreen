///////////// Copyright 2026 GoldenEye: Source. All rights reserved. /////////////
//
//   Project     : Client (GES)
//   File        : ge_customchar_picker.cpp
//   Description :
//      see ge_customchar_picker.h
//
////////////////////////////////////////////////////////////////////////////
#include "cbase.h"
#include <vgui/ISurface.h>
#include <vgui/IInput.h>
#include <vgui/ILocalize.h>
#include <vgui_controls/FileOpenDialog.h>
#include "matsys_controls/mdlpanel.h"
#include "istudiorender.h"
#include "bone_setup.h"
#include "studio.h"
#include "KeyValues.h"

#include "ge_panelhelper.h"
#include "ge_customchar_picker.h"
#include "ge_customhead.h"
#include "ge_splitscreen_padreader.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

#define GECP_PX( v ) GEMenuScaled( v )

static ConVar ge_customhead_lastdir( "ge_customhead_lastdir", "", FCVAR_ARCHIVE, "Folder the custom head file browser last opened" );

//-----------------------------------------------------------------------------
// 3D preview: the body with the chosen head on it, the same way the game
// draws it (body's head shrunk away, the other model's head bone on the body's)
//-----------------------------------------------------------------------------
class CGECustomCharPreview : public CMDLPanel
{
	DECLARE_CLASS_SIMPLE( CGECustomCharPreview, CMDLPanel );

public:
	CGECustomCharPreview( Panel *pParent ) : BaseClass( pParent, "CustomCharPreview" )
	{
		m_bHasHead = false;
		m_flStart = Plat_FloatTime();
		SetLockView( true );
		SetBackgroundColor( 24, 24, 24 );
		memset( m_flPose, 0, sizeof( m_flPose ) );
	}

	void SetBody( const char *pszModel, int skin )
	{
		SetModel( m_Body, pszModel, skin );

		// GoldenEye's character select pose: standing with a pistol
		m_Body.m_nSequence = 0;
		memset( m_flPose, 0, sizeof( m_flPose ) );
		studiohdr_t *pHdr = m_Body.GetMDL() != MDLHANDLE_INVALID ? m_Body.GetStudioHdr() : NULL;
		if ( pHdr )
		{
			CStudioHdr hdr( pHdr, mdlcache );
			for ( int i = 0; i < hdr.GetNumSeq(); i++ )
			{
				if ( !Q_stricmp( hdr.pSeqdesc( i ).pszActivityName(), "ACT_GES_IDLE_PISTOL" ) )
				{
					m_Body.m_nSequence = i;
					break;
				}
			}
			for ( int i = 0; i < hdr.GetNumPoseParameters() && i < MAXSTUDIOPOSEPARAM; i++ )
				Studio_SetPoseParameter( &hdr, i, 0.0f, m_flPose[i] );
		}
	}

	// "" = the body's own head
	void SetHead( const char *pszModel, int skin )
	{
		m_bHasHead = pszModel && pszModel[0];
		SetModel( m_Head, m_bHasHead ? pszModel : NULL, skin );
		if ( m_Head.GetMDL() == MDLHANDLE_INVALID )
			m_bHasHead = false;
	}

	virtual void PerformLayout()
	{
		BaseClass::PerformLayout();
		// Head and shoulders down to the waist
		LookAt( Vector( 0, 0, 46 ), 26.0f );
	}

private:
	void SetModel( CMDL &mdl, const char *pszModel, int skin )
	{
		MDLHandle_t h = ( pszModel && pszModel[0] ) ? mdlcache->FindMDL( pszModel ) : MDLHANDLE_INVALID;
		if ( h != MDLHANDLE_INVALID && mdlcache->IsErrorModel( h ) )
		{
			mdlcache->Release( h );
			h = MDLHANDLE_INVALID;
		}
		mdl.SetMDL( h );
		if ( h != MDLHANDLE_INVALID )
			mdlcache->Release( h );
		mdl.m_nSkin = skin;
		mdl.m_nSequence = 0;
		mdl.m_flTime = 0;
	}

	virtual void OnPaint3D()
	{
		if ( m_Body.GetMDL() == MDLHANDLE_INVALID )
			return;
		studiohdr_t *pBodyHdr = m_Body.GetStudioHdr();
		if ( !pBodyHdr )
			return;

		// Turns a little from side to side, facing the camera
		const float t = Plat_FloatTime() - m_flStart;
		matrix3x4_t root;
		AngleMatrix( QAngle( 0, 340.0f + 30.0f * sinf( t * 0.5f ), 0 ), vec3_origin, root );
		m_Body.m_flTime = t;

		CStudioHdr bodyHdr( pBodyHdr, mdlcache );
		const int numBodyBones = Min( pBodyHdr->numbones, MAXSTUDIOBONES );
		const int iBodyHead = Studio_BoneIndexByName( &bodyHdr, GE_CUSTOMHEAD_BONE );
		const bool bSwap = m_bHasHead && iBodyHead >= 0;

		static matrix3x4_t s_Body[MAXSTUDIOBONES];
		matrix3x4_t *pBones = g_pStudioRender->LockBoneMatrices( numBodyBones );
		m_Body.SetUpBones( root, numBodyBones, pBones, m_flPose );
		memcpy( s_Body, pBones, sizeof( matrix3x4_t ) * numBodyBones );
		if ( bSwap )
		{
			// The body's head (and hair under it) shrinks away
			for ( int i = 0; i < numBodyBones; i++ )
			{
				int b = i;
				while ( b >= 0 && b != iBodyHead )
					b = bodyHdr.boneParent( b );
				if ( b == iBodyHead )
				{
					for ( int r = 0; r < 3; r++ )
						pBones[i][r][0] = pBones[i][r][1] = pBones[i][r][2] = 0.0f;
				}
			}
		}
		g_pStudioRender->UnlockBoneMatrices();
		m_Body.Draw( root, pBones );

		if ( !bSwap )
			return;

		studiohdr_t *pHeadHdr = m_Head.GetStudioHdr();
		if ( !pHeadHdr )
			return;
		CStudioHdr headHdr( pHeadHdr, mdlcache );
		const int numHeadBones = Min( pHeadHdr->numbones, MAXSTUDIOBONES );
		const int iHead = Studio_BoneIndexByName( &headHdr, GE_CUSTOMHEAD_BONE );
		if ( iHead < 0 )
			return;

		static matrix3x4_t s_HeadPose[MAXSTUDIOBONES];
		matrix3x4_t *pHead = g_pStudioRender->LockBoneMatrices( numHeadBones );
		m_Head.SetUpBones( root, numHeadBones, pHead );
		memcpy( s_HeadPose, pHead, sizeof( matrix3x4_t ) * numHeadBones );

		Vector headPos;
		MatrixGetColumn( s_Body[iBodyHead], 3, headPos );
		for ( int i = 0; i < numHeadBones; i++ )
		{
			int b = i;
			while ( b >= 0 && b != iHead )
				b = headHdr.boneParent( b );

			if ( i == iHead )
			{
				MatrixCopy( s_Body[iBodyHead], pHead[i] );
			}
			else if ( b == iHead )
			{
				// Keep its pose relative to its parent
				const int parent = headHdr.boneParent( i );
				matrix3x4_t invParent, local;
				MatrixInvert( s_HeadPose[parent], invParent );
				ConcatTransforms( invParent, s_HeadPose[i], local );
				ConcatTransforms( pHead[parent], local, pHead[i] );
			}
			else
			{
				Vector at = headPos;
				const int iBody = Studio_BoneIndexByName( &bodyHdr, headHdr.pBone( i )->pszName() );
				if ( iBody >= 0 && iBody < numBodyBones )
					MatrixGetColumn( s_Body[iBody], 3, at );
				pHead[i].Init( vec3_origin, vec3_origin, vec3_origin, at );
			}
		}
		g_pStudioRender->UnlockBoneMatrices();
		m_Head.Draw( root, pHead );
	}

	CMDL m_Body;
	CMDL m_Head;
	bool m_bHasHead;
	float m_flStart;
	float m_flPose[MAXSTUDIOPOSEPARAM];
};

//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------
static void LocalizedName( const CGECharData *pChar, char *out, int len )
{
	const char *text = pChar->szPrintName[0] ? pChar->szPrintName : pChar->szIdentifier;
	const wchar_t *pw = text[0] == '#' ? g_pVGuiLocalize->Find( text ) : NULL;
	if ( pw )
		g_pVGuiLocalize->ConvertUnicodeToANSI( pw, out, len );
	else
		Q_strncpy( out, text, len );
}

static bool IsUsableChar( const CGECharData *pChar )
{
	return pChar && pChar != GECharacters()->GetRandomChar() && Q_stricmp( pChar->szIdentifier, CHAR_RANDOM_IDENT )
		&& pChar->m_pSkins.Count() > 0 && pChar->m_pSkins[0]->szModel[0];
}

//-----------------------------------------------------------------------------
// Picker
//-----------------------------------------------------------------------------
CGECustomCharPicker::CGECustomCharPicker( Panel *pParent ) : BaseClass( pParent, "GECustomCharPicker" )
{
	m_pPreview = new CGECustomCharPreview( this );
	m_iBody = m_iHead = 0;
	m_iFocus = ROW_BODY;
	m_bCanFit = false;
	m_szFitSource[0] = '\0';
	m_bImporting = false;
	m_bTextureDialog = false;
	m_bReimportPending = false;
	m_szStatus[0] = '\0';
	m_pOwnPads = new CGESSMenuPadReader;
	m_flLastPadsFromOwner = 0;
	m_hFont = m_hSmallFont = INVALID_FONT;

	SetVisible( false );
	SetMouseInputEnabled( true );
	SetKeyBoardInputEnabled( true );
	SetPaintBackgroundEnabled( false );
}

void CGECustomCharPicker::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	m_hFont = pScheme->GetFont( "DefaultLarge", false );
	if ( m_hFont == INVALID_FONT )
		m_hFont = pScheme->GetFont( "Default", false );
	m_hSmallFont = pScheme->GetFont( "Default", false );
}

void CGECustomCharPicker::PerformLayout()
{
	BaseClass::PerformLayout();
	const int w = GetWide(), h = GetTall();
	const int px = w * 55 / 100;
	// Below it: the status text (Paint), then the controller hint
	m_pPreview->SetBounds( px, GECP_PX( 34 ), w - px - GECP_PX( 14 ), h - GECP_PX( 34 + 40 + 56 ) );
}

void CGECustomCharPicker::Rebuild()
{
	GECharacters_EnsureParsed();

	m_Looks.RemoveAll();
	for ( int i = 0; i < GECharacters()->Count(); i++ )
	{
		const CGECharData *pChar = GECharacters()->Get( i );
		if ( !IsUsableChar( pChar ) )
			continue;
		for ( int s = 0; s < pChar->m_pSkins.Count(); s++ )
		{
			Look_t look = { i, s };
			m_Looks.AddToTail( look );
		}
	}

	GECustomHead_ListImported( m_Imported );
}

void CGECustomCharPicker::Open( const char *spec )
{
	Rebuild();
	m_iBody = m_iHead = 0;

	GECustomChar_t c;
	if ( GECustomChar_Parse( spec, c ) )
	{
		const int body = GECharacters()->GetIndex( c.body );
		const int head = GECharacters()->GetIndex( c.head );
		for ( int i = 0; i < m_Looks.Count(); i++ )
		{
			if ( m_Looks[i].character == body && m_Looks[i].skin == c.bodySkin )
				m_iBody = i;
			if ( m_Looks[i].character == head && m_Looks[i].skin == c.headSkin )
				m_iHead = i;
		}
		if ( c.IsImportedHead() )
		{
			m_iHead = m_iBody;
			for ( int i = 0; i < m_Imported.Count(); i++ )
			{
				if ( !Q_stricmp( m_Imported[i].Get(), c.headFile ) )
					m_iHead = m_Looks.Count() + i;
			}
		}
	}

	m_iFocus = ROW_BODY;
	m_szStatus[0] = '\0';
	m_pOwnPads->Reset();
	LoadFitForHead();
	UpdatePreview();

	SetVisible( true );
	SetZPos( 1000 );	// Over the character select's grid
	MoveToFront();
	RequestFocus();
	InvalidateLayout();
}

const char *CGECustomCharPicker::ImportedHeadName() const
{
	const int i = m_iHead - m_Looks.Count();
	return m_Imported.IsValidIndex( i ) ? m_Imported[i].Get() : "";
}

void CGECustomCharPicker::BuildSpec( char *out, int len ) const
{
	GECustomChar_t c;
	if ( m_Looks.IsValidIndex( m_iBody ) )
	{
		const CGECharData *pBody = GECharacters()->Get( m_Looks[m_iBody].character );
		Q_strncpy( c.body, pBody->szIdentifier, sizeof( c.body ) );
		c.bodySkin = m_Looks[m_iBody].skin;
	}
	if ( IsImportedHead() )
	{
		Q_strncpy( c.headFile, ImportedHeadName(), sizeof( c.headFile ) );
	}
	else if ( m_Looks.IsValidIndex( m_iHead ) )
	{
		const CGECharData *pHead = GECharacters()->Get( m_Looks[m_iHead].character );
		Q_strncpy( c.head, pHead->szIdentifier, sizeof( c.head ) );
		c.headSkin = m_Looks[m_iHead].skin;
	}
	GECustomChar_Format( c, out, len );
}

void CGECustomCharPicker::UpdatePreview()
{
	if ( !m_Looks.IsValidIndex( m_iBody ) )
		return;

	const CGECharData *pBody = GECharacters()->Get( m_Looks[m_iBody].character );
	const GECharSkin *pBodySkin = pBody->m_pSkins[m_Looks[m_iBody].skin];
	m_pPreview->SetBody( pBodySkin->szModel, pBodySkin->iWorldSkin );

	char model[MAX_PATH];
	int skin = 0;
	model[0] = '\0';
	if ( IsImportedHead() )
	{
		GECustomHead_ImportedModelPath( ImportedHeadName(), model, sizeof( model ) );
	}
	else if ( m_Looks.IsValidIndex( m_iHead ) && m_iHead != m_iBody )
	{
		GECustomHead_GetModel( m_Looks[m_iHead].character, m_Looks[m_iHead].skin, "", model, sizeof( model ), skin );
	}
	m_pPreview->SetHead( model, skin );
}

void CGECustomCharPicker::LoadFitForHead()
{
	m_bCanFit = IsImportedHead() && GECustomHead_GetImportInfo( ImportedHeadName(), m_szFitSource, sizeof( m_szFitSource ), m_Fit );
	if ( !m_bCanFit )
	{
		m_Fit = GEHeadFit_t();
		m_szFitSource[0] = '\0';
	}
}

bool CGECustomCharPicker::IsRowUsable( int row ) const
{
	switch ( row )
	{
	case ROW_TEXMAP:
		return m_bCanFit && !m_bImporting && m_Fit.texture[0];
	case ROW_TEXTURE: case ROW_SIZE: case ROW_HEIGHT: case ROW_FORWARD: case ROW_TURN: case ROW_UPAXIS: case ROW_APPLY:
		return m_bCanFit && !m_bImporting;
	case ROW_IMPORT:
		return !m_bImporting;
	default:
		return true;
	}
}

void CGECustomCharPicker::MoveFocus( int dir )
{
	int row = m_iFocus;
	for ( int n = 0; n < ROW_COUNT; n++ )
	{
		row = ( row + dir + ROW_COUNT ) % ROW_COUNT;
		if ( IsRowUsable( row ) )
		{
			m_iFocus = row;
			return;
		}
	}
}

void CGECustomCharPicker::Change( int row, int dir )
{
	if ( !IsRowUsable( row ) )
		return;

	const int numLooks = m_Looks.Count();
	switch ( row )
	{
	case ROW_BODY:
		if ( numLooks > 0 )
		{
			const bool bSameHead = m_iHead == m_iBody;
			m_iBody = ( m_iBody + dir + numLooks ) % numLooks;
			if ( bSameHead )
				m_iHead = m_iBody;
			UpdatePreview();
		}
		break;
	case ROW_HEAD:
	{
		const int count = numLooks + m_Imported.Count();
		if ( count > 0 )
		{
			m_iHead = ( m_iHead + dir + count ) % count;
			LoadFitForHead();
			UpdatePreview();
		}
		break;
	}
	case ROW_SIZE:		m_Fit.scale = clamp( m_Fit.scale + 0.05f * dir, 0.3f, 3.0f ); break;
	case ROW_HEIGHT:	m_Fit.height = clamp( m_Fit.height + 0.5f * dir, -12.0f, 12.0f ); break;
	case ROW_FORWARD:	m_Fit.forward = clamp( m_Fit.forward + 0.5f * dir, -12.0f, 12.0f ); break;
	case ROW_TURN:		m_Fit.turn = ( m_Fit.turn + dir + 4 ) & 3; break;
	case ROW_UPAXIS:	m_Fit.zUp = !m_Fit.zUp; break;
	case ROW_TEXMAP:
		m_Fit.wrap = !m_Fit.wrap;
		m_bReimportPending = true;
		return;
	case ROW_TEXTURE:
		// Left / right: back to the .obj's own (.mtl) textures
		if ( m_Fit.texture[0] )
		{
			m_Fit.texture[0] = '\0';
			m_bReimportPending = true;
		}
		return;
	default:
		return;		// Buttons don't change with left / right
	}

	if ( row >= ROW_SIZE && row <= ROW_UPAXIS )
	{
		Q_strncpy( m_szStatus, "Press Apply fit to see the change", sizeof( m_szStatus ) );
		m_StatusColor = Color( 230, 200, 80, 255 );
	}
}

void CGECustomCharPicker::Activate( int row )
{
	if ( !IsRowUsable( row ) )
		return;

	switch ( row )
	{
	case ROW_BODY:
	case ROW_HEAD:
		Change( row, 1 );
		break;
	case ROW_IMPORT:
		BrowseForHead();
		break;
	case ROW_TEXTURE:
		BrowseForTexture();
		break;
	case ROW_APPLY:
		StartImport( m_szFitSource );
		break;
	case ROW_DONE:
		Finish( true );
		break;
	case ROW_CANCEL:
		Finish( false );
		break;
	default:
		Change( row, 1 );
		break;
	}
}

void CGECustomCharPicker::Finish( bool bAccepted )
{
	if ( m_hFileDialog.Get() )
		m_hFileDialog->MarkForDeletion();

	SetVisible( false );
	char spec[GE_CUSTOMCHAR_SPEC_LEN];
	BuildSpec( spec, sizeof( spec ) );
	KeyValues *kv = new KeyValues( "CustomCharPicked" );
	kv->SetInt( "accepted", bAccepted ? 1 : 0 );
	kv->SetString( "spec", spec );
	PostActionSignal( kv );
}

void CGECustomCharPicker::BrowseForHead()
{
	if ( m_hFileDialog.Get() )
		m_hFileDialog->MarkForDeletion();

	FileOpenDialog *pDialog = new FileOpenDialog( this, "Import a head (.obj or .mdl)", true );
	pDialog->AddFilter( "*.obj;*.mdl", "Heads (*.obj, *.mdl)", true );
	pDialog->AddFilter( "*.obj", "Wavefront OBJ (*.obj)", false );
	pDialog->AddFilter( "*.mdl", "Compiled Source model (*.mdl)", false );
	if ( ge_customhead_lastdir.GetString()[0] )
		pDialog->SetStartDirectory( ge_customhead_lastdir.GetString() );
	pDialog->AddActionSignalTarget( this );
	pDialog->DoModal( false );
	m_hFileDialog = pDialog;
	m_bTextureDialog = false;
}

void CGECustomCharPicker::BrowseForTexture()
{
	if ( m_hFileDialog.Get() )
		m_hFileDialog->MarkForDeletion();

	FileOpenDialog *pDialog = new FileOpenDialog( this, "Pick a texture for the head", true );
	pDialog->AddFilter( "*.png;*.jpg;*.jpeg;*.tga;*.bmp", "Images (*.png, *.jpg, *.tga, *.bmp)", true );
	pDialog->AddFilter( "*.png", "PNG (*.png)", false );
	pDialog->AddFilter( "*.jpg;*.jpeg", "JPEG (*.jpg)", false );
	pDialog->AddFilter( "*.tga", "Targa (*.tga)", false );
	pDialog->AddFilter( "*.bmp", "Bitmap (*.bmp)", false );
	char dir[MAX_PATH] = "";
	if ( m_Fit.texture[0] )
		Q_ExtractFilePath( m_Fit.texture, dir, sizeof( dir ) );
	else if ( m_szFitSource[0] )
		Q_ExtractFilePath( m_szFitSource, dir, sizeof( dir ) );
	else
		Q_strncpy( dir, ge_customhead_lastdir.GetString(), sizeof( dir ) );
	if ( dir[0] )
		pDialog->SetStartDirectory( dir );
	pDialog->AddActionSignalTarget( this );
	pDialog->DoModal( false );
	m_hFileDialog = pDialog;
	m_bTextureDialog = true;
}

void CGECustomCharPicker::OnFileSelected( const char *fullpath )
{
	if ( m_bTextureDialog )
	{
		// The head is made again with this image on it (same fit)
		m_bTextureDialog = false;
		if ( m_bCanFit && fullpath && fullpath[0] )
		{
			// A picked image is usually not laid out for the file's UVs (a photo
			// of a face): wrapped around the head first, the next row changes it
			if ( !m_Fit.texture[0] )
				m_Fit.wrap = true;
			Q_strncpy( m_Fit.texture, fullpath, sizeof( m_Fit.texture ) );
			m_bReimportPending = true;
		}
		RequestFocus();
		return;
	}

	char dir[MAX_PATH];
	Q_ExtractFilePath( fullpath, dir, sizeof( dir ) );
	ge_customhead_lastdir.SetValue( dir );

	// A new file starts with the automatic fit
	m_Fit = GEHeadFit_t();
	StartImport( fullpath );
	RequestFocus();
}

void CGECustomCharPicker::StartImport( const char *path )
{
	char err[512];
	if ( !path || !path[0] || !GECustomHead_StartImport( path, m_Fit, err, sizeof( err ) ) )
	{
		Q_strncpy( m_szStatus, ( path && path[0] ) ? err : "No file", sizeof( m_szStatus ) );
		m_StatusColor = Color( 255, 110, 90, 255 );
		return;
	}

	m_bImporting = true;
	Q_snprintf( m_szStatus, sizeof( m_szStatus ), "Converting %s...", V_UnqualifiedFileName( path ) );
	m_StatusColor = Color( 230, 200, 80, 255 );
}

void CGECustomCharPicker::OnThink()
{
	BaseClass::OnThink();
	if ( !IsVisible() )
		return;

	if ( m_bReimportPending && !m_bImporting )
	{
		m_bReimportPending = false;
		StartImport( m_szFitSource );
	}

	// Import finished?
	if ( m_bImporting )
	{
		char name[GE_CUSTOMHEAD_NAME_LEN], msg[512];
		const GEHeadImportState_t state = GECustomHead_PollImport( name, sizeof( name ), msg, sizeof( msg ) );
		if ( state == GE_HEADIMPORT_DONE )
		{
			m_bImporting = false;
			GECustomHead_ListImported( m_Imported );
			for ( int i = 0; i < m_Imported.Count(); i++ )
			{
				if ( !Q_stricmp( m_Imported[i].Get(), name ) )
					m_iHead = m_Looks.Count() + i;
			}
			LoadFitForHead();
			UpdatePreview();
			Q_snprintf( m_szStatus, sizeof( m_szStatus ), "Head \"%s\" is ready%s", name, m_bCanFit ? ". Adjust the fit below if needed." : "" );
			m_StatusColor = Color( 140, 220, 120, 255 );
		}
		else if ( state == GE_HEADIMPORT_FAILED || state == GE_HEADIMPORT_IDLE )
		{
			m_bImporting = false;
			Q_strncpy( m_szStatus, msg[0] ? msg : "The import failed", sizeof( m_szStatus ) );
			m_StatusColor = Color( 255, 110, 90, 255 );
		}
	}

	// Controllers (not while the file browser is up)
	if ( !m_hFileDialog.Get() || !m_hFileDialog->IsVisible() )
	{
		m_pOwnPads->Update();
		if ( m_pOwnPads->Any() )
			HandlePads( *m_pOwnPads );
	}
}

void CGECustomCharPicker::HandlePads( const CGESSMenuPadReader &pads )
{
	if ( pads.Dir( GESS_DIR_UP ) )		MoveFocus( -1 );
	if ( pads.Dir( GESS_DIR_DOWN ) )	MoveFocus( 1 );
	if ( pads.Dir( GESS_DIR_LEFT ) )	Change( m_iFocus, -1 );
	if ( pads.Dir( GESS_DIR_RIGHT ) )	Change( m_iFocus, 1 );

	if ( pads.Pressed( GESS_PAD_START ) )
		Finish( true );
	else if ( pads.Pressed( GESS_PAD_A ) )
		Activate( m_iFocus );
	else if ( pads.Pressed( GESS_PAD_B ) )
		Finish( false );
}

void CGECustomCharPicker::OnKeyCodePressed( KeyCode code )
{
	if ( code == KEY_ESCAPE )							Finish( false );
	else if ( code == KEY_ENTER || code == KEY_PAD_ENTER )	Activate( m_iFocus );
	else if ( code == KEY_UP )							MoveFocus( -1 );
	else if ( code == KEY_DOWN )						MoveFocus( 1 );
	else if ( code == KEY_LEFT )						Change( m_iFocus, -1 );
	else if ( code == KEY_RIGHT )						Change( m_iFocus, 1 );
	else BaseClass::OnKeyCodePressed( code );
}

void CGECustomCharPicker::RowRect( int row, int &x, int &y, int &w, int &h )
{
	const int width = GetWide() * 55 / 100 - GECP_PX( 28 );
	x = GECP_PX( 14 );
	w = width;
	const int fullH = GECP_PX( 30 );
	const int gap = GECP_PX( 4 );

	// Rows down to Apply fit share the room above Done / Cancel, smaller when
	// the window is short (split screen)
	const int top = GECP_PX( 40 ), sectionGap = GECP_PX( 10 );
	const int room = GetTall() - GECP_PX( 14 ) - fullH - gap - top - sectionGap;
	const int step = clamp( room / ( ROW_APPLY + 1 ), GECP_PX( 16 ), fullH + gap );
	h = Min( fullH, step - Max( 1, gap * step / ( fullH + gap ) ) );
	y = top + row * step;
	if ( row >= ROW_SIZE )
		y += sectionGap;		// Fit rows a little apart
	if ( row >= ROW_DONE )
	{
		// Done / Cancel side by side at the bottom
		h = fullH;
		y = GetTall() - GECP_PX( 14 ) - h;
		w = ( width - gap ) / 2;
		x = row == ROW_DONE ? x : x + w + gap;
	}
}

int CGECustomCharPicker::HitRow( int &side )
{
	int mx, my;
	input()->GetCursorPos( mx, my );
	ScreenToLocal( mx, my );
	for ( int row = 0; row < ROW_COUNT; row++ )
	{
		int x, y, w, h;
		RowRect( row, x, y, w, h );
		if ( mx >= x && mx < x + w && my >= y && my < y + h )
		{
			// Left / right arrows of a setting
			side = mx < x + w * 45 / 100 ? 0 : ( mx < x + w * 72 / 100 ? -1 : 1 );
			return row;
		}
	}
	return -1;
}

void CGECustomCharPicker::OnMousePressed( MouseCode code )
{
	if ( code != MOUSE_LEFT )
		return;
	int side = 0;
	const int row = HitRow( side );
	if ( row < 0 || !IsRowUsable( row ) )
		return;
	m_iFocus = row;

	const bool bSetting = row == ROW_BODY || row == ROW_HEAD || row == ROW_TEXMAP || ( row >= ROW_SIZE && row <= ROW_UPAXIS );
	if ( bSetting )
		Change( row, side < 0 ? -1 : 1 );
	else
		Activate( row );
}

void CGECustomCharPicker::OnMouseWheeled( int delta )
{
	int side = 0;
	const int row = HitRow( side );
	if ( row >= 0 && IsRowUsable( row ) && ( row == ROW_BODY || row == ROW_HEAD || ( row >= ROW_SIZE && row <= ROW_UPAXIS ) ) )
	{
		m_iFocus = row;
		Change( row, delta > 0 ? -1 : 1 );
	}
}

void CGECustomCharPicker::RowText( int row, char *label, int labelLen, char *value, int valueLen ) const
{
	label[0] = value[0] = '\0';
	switch ( row )
	{
	case ROW_BODY:
	case ROW_HEAD:
	{
		Q_strncpy( label, row == ROW_BODY ? "Body" : "Head", labelLen );
		const int i = row == ROW_BODY ? m_iBody : m_iHead;
		if ( row == ROW_HEAD && IsImportedHead() )
		{
			Q_snprintf( value, valueLen, "Imported: %s", ImportedHeadName() );
		}
		else if ( m_Looks.IsValidIndex( i ) )
		{
			const CGECharData *pChar = GECharacters()->Get( m_Looks[i].character );
			char name[128];
			LocalizedName( pChar, name, sizeof( name ) );
			if ( pChar->m_pSkins.Count() > 1 )
				Q_snprintf( value, valueLen, "%s (look %d/%d)", name, m_Looks[i].skin + 1, pChar->m_pSkins.Count() );
			else
				Q_strncpy( value, name, valueLen );
		}
		break;
	}
	case ROW_IMPORT:	Q_strncpy( label, "Import your own head...", labelLen ); Q_strncpy( value, ".obj or .mdl", valueLen ); break;
	case ROW_TEXTURE:
		Q_strncpy( label, "Head texture...", labelLen );
		if ( !m_bCanFit )
			Q_strncpy( value, "for imported .obj heads", valueLen );
		else if ( m_Fit.texture[0] )
			Q_snprintf( value, valueLen, "%s  (< > to remove)", V_UnqualifiedFileName( m_Fit.texture ) );
		else
			Q_strncpy( value, "png, jpg, tga, bmp", valueLen );
		break;
	case ROW_TEXMAP:
		Q_strncpy( label, "Texture placement", labelLen );
		Q_strncpy( value, m_Fit.wrap ? "Wrapped around the head" : "The file's UV map", valueLen );
		break;
	case ROW_SIZE:		Q_strncpy( label, "Head size", labelLen ); Q_snprintf( value, valueLen, "%.2f", m_Fit.scale ); break;
	case ROW_HEIGHT:	Q_strncpy( label, "Up / down", labelLen ); Q_snprintf( value, valueLen, "%+.1f", m_Fit.height ); break;
	case ROW_FORWARD:	Q_strncpy( label, "Forward / back", labelLen ); Q_snprintf( value, valueLen, "%+.1f", m_Fit.forward ); break;
	case ROW_TURN:		Q_strncpy( label, "Turn", labelLen ); Q_snprintf( value, valueLen, "%d degrees", ( ( m_Fit.turn + 1 ) & 3 ) * 90 ); break;	// 0 = the usual way round
	case ROW_UPAXIS:	Q_strncpy( label, "File's up axis", labelLen ); Q_strncpy( value, m_Fit.zUp ? "Z up" : "Y up (Blender default)", valueLen ); break;
	case ROW_APPLY:		Q_strncpy( label, "Apply fit", labelLen ); break;
	case ROW_DONE:		Q_strncpy( label, "Done", labelLen ); break;
	case ROW_CANCEL:	Q_strncpy( label, "Cancel", labelLen ); break;
	}
}

int CGECustomCharPicker::TextWide( HFont font, const char *text )
{
	wchar_t wbuf[512];
	g_pVGuiLocalize->ConvertANSIToUnicode( text, wbuf, sizeof( wbuf ) );
	int w = 0, h = 0;
	surface()->GetTextSize( font, wbuf, w, h );
	return w;
}

void CGECustomCharPicker::DrawString( HFont font, int x, int y, Color col, const char *text )
{
	wchar_t wbuf[512];
	g_pVGuiLocalize->ConvertANSIToUnicode( text, wbuf, sizeof( wbuf ) );
	surface()->DrawSetTextFont( font );
	surface()->DrawSetTextColor( col );
	surface()->DrawSetTextPos( x, y );
	surface()->DrawPrintText( wbuf, wcslen( wbuf ) );
}

void CGECustomCharPicker::Paint()
{
	const int w = GetWide(), h = GetTall();

	surface()->DrawSetColor( Color( 20, 20, 20, 255 ) );
	surface()->DrawFilledRect( 0, 0, w, h );
	surface()->DrawSetColor( Color( 185, 168, 90, 255 ) );
	surface()->DrawOutlinedRect( 0, 0, w, h );
	DrawString( m_hSmallFont, GECP_PX( 12 ), GECP_PX( 8 ), Color( 230, 200, 80, 255 ), "Custom character: pick a body and a head" );

	const int fontTall = surface()->GetFontTall( m_hSmallFont );
	for ( int row = 0; row < ROW_COUNT; row++ )
	{
		int x, y, rw, rh;
		RowRect( row, x, y, rw, rh );
		const bool bUsable = IsRowUsable( row );
		const bool bFocus = row == m_iFocus;
		const bool bButton = row == ROW_IMPORT || row == ROW_TEXTURE || row == ROW_APPLY || row == ROW_DONE || row == ROW_CANCEL;

		surface()->DrawSetColor( bButton ? Color( 52, 52, 52, 255 ) : Color( 34, 34, 34, 255 ) );
		surface()->DrawFilledRect( x, y, x + rw, y + rh );
		surface()->DrawSetColor( bFocus ? Color( 230, 200, 80, 255 ) : Color( 70, 70, 70, 255 ) );
		surface()->DrawOutlinedRect( x, y, x + rw, y + rh );
		if ( bFocus )
			surface()->DrawOutlinedRect( x - 1, y - 1, x + rw + 1, y + rh + 1 );

		char label[128], value[160];
		RowText( row, label, sizeof( label ), value, sizeof( value ) );
		const Color textCol = bUsable ? Color( 255, 255, 255, 255 ) : Color( 110, 110, 110, 255 );
		const int ty = y + ( rh - fontTall ) / 2;
		if ( row >= ROW_DONE || row == ROW_APPLY )
			DrawString( m_hSmallFont, x + ( rw - TextWide( m_hSmallFont, label ) ) / 2, ty, textCol, label );
		else
			DrawString( m_hSmallFont, x + GECP_PX( 8 ), ty, textCol, label );

		if ( value[0] )
		{
			const bool bSetting = row != ROW_IMPORT && row != ROW_TEXTURE;
			const Color valCol = bUsable ? ( bSetting ? Color( 230, 200, 80, 255 ) : Color( 170, 170, 170, 255 ) ) : Color( 90, 90, 90, 255 );
			char shown[200];
			Q_snprintf( shown, sizeof( shown ), bSetting ? "<  %s  >" : "%s", value );
			DrawString( m_hSmallFont, x + rw - GECP_PX( 8 ) - TextWide( m_hSmallFont, shown ), ty, valCol, shown );
		}
	}

	// Status (import progress / errors), wrapped under the preview (left of
	// it there may be no room between Apply fit and Done / Cancel)
	if ( m_szStatus[0] )
	{
		const int x = w * 55 / 100;
		const int rw = w - x - GECP_PX( 14 );
		int ty = h - GECP_PX( 40 + 52 );
		const char *p = m_szStatus;
		char line[256];
		while ( *p )
		{
			int n = 0, lastSpace = -1;
			while ( p[n] )
			{
				Q_strncpy( line, p, Min( n + 2, (int)sizeof( line ) ) );
				if ( TextWide( m_hSmallFont, line ) > rw )
					break;
				if ( p[n] == ' ' )
					lastSpace = n;
				n++;
			}
			if ( p[n] && lastSpace > 0 )
				n = lastSpace;
			if ( n == 0 )
				n = 1;
			Q_strncpy( line, p, Min( n + 1, (int)sizeof( line ) ) );
			DrawString( m_hSmallFont, x, ty, m_StatusColor, line );
			ty += fontTall + 2;
			p += n;
			while ( *p == ' ' )
				p++;
		}
	}

	// Controller hint under the preview, on two lines when one doesn't fit
	{
		const int hx = w * 55 / 100;
		const int hw = w - hx - GECP_PX( 14 );
		const Color hintCol( 160, 160, 160, 255 );
		static const char *s_szHint = "D-pad: move / change   A: select   START: done   B: cancel";
		if ( TextWide( m_hSmallFont, s_szHint ) <= hw )
		{
			DrawString( m_hSmallFont, hx, h - GECP_PX( 30 ), hintCol, s_szHint );
		}
		else
		{
			DrawString( m_hSmallFont, hx, h - GECP_PX( 14 ) - 2 * fontTall - 2, hintCol, "D-pad: move / change   A: select" );
			DrawString( m_hSmallFont, hx, h - GECP_PX( 14 ) - fontTall, hintCol, "START: done   B: cancel" );
		}
	}
}
