///////////// Copyright 2026 GoldenEye: Source. All rights reserved. /////////////
//
//   Project     : Client (GES)
//   File        : ge_customchar_picker.h
//   Description :
//      The window the "Custom" character opens: pick any character's body and
//      any character's head (or import your own head, .obj or .mdl) and see it
//      in 3D. Works with the mouse, keyboard and controller, like the rest of
//      the Split Screen menu.
//
//      Posts "CustomCharPicked" { "accepted" 0/1 "spec" "_custom/..." } to its
//      action signal targets when closed.
//
////////////////////////////////////////////////////////////////////////////
#ifndef GE_CUSTOMCHAR_PICKER_H
#define GE_CUSTOMCHAR_PICKER_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/EditablePanel.h>
#include "ge_customchar.h"
#include "ge_customhead_import.h"

class CGESSMenuPadReader;
class CGECustomCharPreview;

class CGECustomCharPicker : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CGECustomCharPicker, vgui::EditablePanel );

public:
	CGECustomCharPicker( vgui::Panel *pParent );

	// Opens on a spec (or the first character when it isn't one)
	void Open( const char *spec );
	bool IsOpen() const { return const_cast<CGECustomCharPicker *>( this )->IsVisible(); }

	// From a menu that reads the controllers itself; otherwise the picker reads them
	void HandlePads( const CGESSMenuPadReader &pads );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void Paint();
	virtual void OnThink();
	virtual void OnKeyCodePressed( vgui::KeyCode code );
	virtual void OnMousePressed( vgui::MouseCode code );
	virtual void OnMouseWheeled( int delta );

	MESSAGE_FUNC_CHARPTR( OnFileSelected, "FileSelected", fullpath );

	enum Row_t
	{
		ROW_BODY = 0,
		ROW_HEAD,
		ROW_IMPORT,
		ROW_TEXTURE,	// Image for the imported head (re-imports it)
		ROW_TEXMAP,		// The image on the file's UVs, or wrapped around the head
		ROW_SIZE,
		ROW_HEIGHT,
		ROW_FORWARD,
		ROW_TURN,
		ROW_UPAXIS,
		ROW_APPLY,
		ROW_DONE,
		ROW_CANCEL,
		ROW_COUNT
	};

private:
	struct Look_t { int character; int skin; };

	void Rebuild();						// Lists of looks and imported heads
	void Change( int row, int dir );
	void Activate( int row );
	void MoveFocus( int dir );
	bool IsRowUsable( int row ) const;
	void Finish( bool bAccepted );
	void BuildSpec( char *out, int len ) const;
	void UpdatePreview();
	void LoadFitForHead();
	void StartImport( const char *path );
	void BrowseForHead();
	void BrowseForTexture();

	void RowRect( int row, int &x, int &y, int &w, int &h );
	int  HitRow( int &side );
	void RowText( int row, char *label, int labelLen, char *value, int valueLen ) const;
	void DrawString( vgui::HFont font, int x, int y, Color col, const char *text );
	int  TextWide( vgui::HFont font, const char *text );

	bool IsImportedHead() const { return m_iHead >= m_Looks.Count(); }
	const char *ImportedHeadName() const;

	CGECustomCharPreview *m_pPreview;
	CUtlVector<Look_t> m_Looks;
	CUtlVector<CUtlString> m_Imported;
	int m_iBody;
	int m_iHead;		// < m_Looks.Count(): a character's head, else m_Imported[m_iHead - m_Looks.Count()]
	int m_iFocus;

	// Fit of the selected imported .obj head (re-imported with Apply)
	bool m_bCanFit;
	char m_szFitSource[MAX_PATH];
	GEHeadFit_t m_Fit;

	bool m_bImporting;
	char m_szStatus[512];
	Color m_StatusColor;

	CGESSMenuPadReader *m_pOwnPads;		// When nobody passes the pads in
	float m_flLastPadsFromOwner;

	vgui::DHANDLE<vgui::Panel> m_hFileDialog;
	bool m_bReimportPending;	// Convert the head again on the next think (not mid input handling)
	bool m_bTextureDialog;		// m_hFileDialog picks a texture, not a head
	vgui::HFont m_hFont, m_hSmallFont;
};

#endif // GE_CUSTOMCHAR_PICKER_H
