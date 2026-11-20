//-----------------------------------------------------------------------------
// Main menu: Split Screen Mod version in the bottom right corner, and a check
// for a newer release on GitHub.
//
// Once per game start the newest GitHub release is asked for through Steam's
// HTTP client (ISteamHTTP does HTTPS; the old curl.lib in lib/common can't talk
// to GitHub). Its tag ("v0.3") is compared with GES_SS_VERSION. When it is
// newer a second line says "Update available: v0.3 - click to download"; a
// click opens the release page, where the setup is that updates the mod.
// No network, no Steam or a GitHub error leave just the version showing.
//
// ges_ss_update_check 0 turns the check off.
// ges_ss_check_update [version] checks again; with a version it pretends that
// is the newest release, to see the message without publishing one.
//-----------------------------------------------------------------------------
#include "cbase.h"
#include <vgui/IVGui.h>
#include <vgui/ISurface.h>
#include <vgui/IInput.h>
#include <vgui/ISystem.h>
#include <vgui/Cursor.h>
#include "ienginevgui.h"
#include "ge_panelhelper.h"
#include "ge_splitscreen_version.h"
#include "steam/steam_api.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar ges_ss_update_check( "ges_ss_update_check", "1", FCVAR_ARCHIVE, "Check GitHub for a newer Split Screen Mod release on the main menu" );

// "v0.2.1" / "0.2" -> {0,2,1} / {0,2,0}. Stops at the first character that isn't a digit or a dot.
static void GESS_ParseVersion( const char *psz, int parts[3] )
{
	parts[0] = parts[1] = parts[2] = 0;
	if ( !psz )
		return;
	while ( *psz == 'v' || *psz == 'V' || *psz == ' ' )
		++psz;
	for ( int i = 0; i < 3 && *psz >= '0' && *psz <= '9'; ++i )
	{
		parts[i] = atoi( psz );
		while ( *psz >= '0' && *psz <= '9' )
			++psz;
		if ( *psz != '.' )
			break;
		++psz;
	}
}

// > 0 when a is newer than b
static int GESS_CompareVersions( const char *a, const char *b )
{
	int va[3], vb[3];
	GESS_ParseVersion( a, va );
	GESS_ParseVersion( b, vb );
	for ( int i = 0; i < 3; ++i )
	{
		if ( va[i] != vb[i] )
			return va[i] - vb[i];
	}
	return 0;
}

// Pulls "key": "value" out of GitHub's JSON. Enough for tag_name and html_url,
// which never hold escaped quotes.
static bool GESS_JsonString( const char *json, const char *key, char *out, int outSize )
{
	char needle[64];
	Q_snprintf( needle, sizeof( needle ), "\"%s\"", key );
	const char *p = Q_strstr( json, needle );
	if ( !p )
		return false;
	p += Q_strlen( needle );
	while ( *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ':' )
		++p;
	if ( *p != '"' )
		return false;
	++p;
	int n = 0;
	while ( *p && *p != '"' && n < outSize - 1 )
		out[n++] = *p++;
	out[n] = '\0';
	return n > 0;
}

class CGESSVersionLabel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CGESSVersionLabel, vgui::Panel );
public:
	CGESSVersionLabel( vgui::VPANEL parent ) : BaseClass( NULL, "GESSVersionLabel" )
	{
		SetParent( parent );
		SetMouseInputEnabled( false );
		SetKeyBoardInputEnabled( false );
		SetPaintBackgroundEnabled( false );
		SetVisible( false );
		m_hFont = vgui::INVALID_FONT;
		m_hSmallFont = vgui::INVALID_FONT;
		m_iFontScreenTall = 0;
		m_bChecked = false;
		m_bUpdate = false;
		m_hRequest = INVALID_HTTPREQUEST_HANDLE;
		m_szLatest[0] = '\0';
		Q_strncpy( m_szReleaseURL, GES_SS_RELEASES_URL, sizeof( m_szReleaseURL ) );
		vgui::ivgui()->AddTickSignal( GetVPanel(), 100 );
	}

	virtual ~CGESSVersionLabel()
	{
		m_CallResult.Cancel();
		if ( m_hRequest != INVALID_HTTPREQUEST_HANDLE && steamapicontext && steamapicontext->SteamHTTP() )
			steamapicontext->SteamHTTP()->ReleaseHTTPRequest( m_hRequest );
	}

	// ges_ss_check_update: check again, or pretend pszFakeLatest is the newest release
	void CheckAgain( const char *pszFakeLatest )
	{
		if ( pszFakeLatest && pszFakeLatest[0] )
		{
			SetLatest( pszFakeLatest, NULL );
			return;
		}
		m_bChecked = false;
		m_bUpdate = false;
		m_szLatest[0] = '\0';
		StartCheck();
	}

	void PrintStatus()
	{
		if ( m_bUpdate )
			Msg( "Split Screen Mod v%s - update available: %s (%s)\n", GES_SS_VERSION, m_szLatest, m_szReleaseURL );
		else if ( m_szLatest[0] )
			Msg( "Split Screen Mod v%s - up to date (newest release %s)\n", GES_SS_VERSION, m_szLatest );
		else
			Msg( "Split Screen Mod v%s - newest release not known yet\n", GES_SS_VERSION );
	}

	virtual void OnTick()
	{
		UpdateFonts();

		// Ask GitHub once, the first time Steam is up (the panel is made before it is)
		if ( !m_bChecked )
			StartCheck();

		// Only on the main menu, not in a game or while a map loads
		const bool bShow = enginevgui->IsGameUIVisible() && !engine->IsDrawingLoadingImage() &&
			( !engine->IsInGame() || engine->IsLevelMainMenuBackground() );

		if ( bShow != IsVisible() )
		{
			SetVisible( bShow );
			if ( bShow && !vgui::input()->GetAppModalSurface() )
				MoveToFront();
		}

		if ( bShow )
		{
			// Bottom right corner, a margin of a third of a line from the edges
			int sw, sh;
			vgui::surface()->GetScreenSize( sw, sh );
			const int tall = LineTall( m_hFont ) + ( m_bUpdate ? LineTall( m_hSmallFont ) : 0 );
			const int wide = Max( TextWide( m_hFont, VersionText() ), m_bUpdate ? TextWide( m_hSmallFont, UpdateText() ) : 0 );
			const int margin = Max( 6, sh / 60 );
			SetBounds( sw - wide - margin, sh - tall - margin, wide, tall );

			// Only clickable while it offers an update, so it never eats a click otherwise
			SetMouseInputEnabled( m_bUpdate );
			SetCursor( m_bUpdate ? vgui::dc_hand : vgui::dc_arrow );
		}
	}

	virtual void OnMousePressed( vgui::MouseCode code )
	{
		if ( !m_bUpdate || code != MOUSE_LEFT )
			return;
		vgui::system()->ShellExecute( "open", m_szReleaseURL );
	}

	virtual void Paint()
	{
		int w, h;
		GetSize( w, h );
		int y = 0;
		DrawRightAligned( m_hFont, VersionText(), w, y, Color( 255, 255, 255, 230 ) );
		y += LineTall( m_hFont );
		if ( m_bUpdate )
		{
			// Gold, gently pulsing so it's noticed
			const int alpha = 200 + (int)( 55.0f * sinf( (float) Plat_FloatTime() * 3.0f ) );
			const bool bHover = IsCursorOver();
			DrawRightAligned( m_hSmallFont, UpdateText(), w, y, bHover ? Color( 255, 255, 255, 255 ) : Color( 255, 204, 64, alpha ) );
		}
	}

private:
	void StartCheck()
	{
		if ( !ges_ss_update_check.GetBool() )
		{
			m_bChecked = true;
			return;
		}
		if ( !steamapicontext || !steamapicontext->SteamHTTP() )
			return;	// Steam isn't up yet, OnTick tries again

		m_bChecked = true;

		ISteamHTTP *pHTTP = steamapicontext->SteamHTTP();
		m_hRequest = pHTTP->CreateHTTPRequest( k_EHTTPMethodGET, GES_SS_LATEST_RELEASE_API );
		if ( m_hRequest == INVALID_HTTPREQUEST_HANDLE )
			return;

		// GitHub wants a User-Agent on every API call
		pHTTP->SetHTTPRequestHeaderValue( m_hRequest, "User-Agent", "GES-SplitScreenMod/" GES_SS_VERSION );
		pHTTP->SetHTTPRequestHeaderValue( m_hRequest, "Accept", "application/vnd.github+json" );
		pHTTP->SetHTTPRequestNetworkActivityTimeout( m_hRequest, 15 );

		SteamAPICall_t hCall;
		if ( pHTTP->SendHTTPRequest( m_hRequest, &hCall ) )
		{
			m_CallResult.Set( hCall, this, &CGESSVersionLabel::OnHTTPRequestCompleted );
		}
		else
		{
			pHTTP->ReleaseHTTPRequest( m_hRequest );
			m_hRequest = INVALID_HTTPREQUEST_HANDLE;
		}
	}

	void OnHTTPRequestCompleted( HTTPRequestCompleted_t *pResult, bool bIOFailure )
	{
		ISteamHTTP *pHTTP = steamapicontext ? steamapicontext->SteamHTTP() : NULL;
		if ( !pHTTP )
			return;

		if ( bIOFailure || !pResult->m_bRequestSuccessful || pResult->m_eStatusCode != k_EHTTPStatusCode200OK )
		{
			DevMsg( "Split Screen Mod update check failed (HTTP %d)\n", bIOFailure ? 0 : (int) pResult->m_eStatusCode );
		}
		else
		{
			uint32 size = 0;
			if ( pHTTP->GetHTTPResponseBodySize( pResult->m_hRequest, &size ) && size > 0 && size < 4 * 1024 * 1024 )
			{
				CUtlVector<char> body;
				body.SetCount( size + 1 );
				if ( pHTTP->GetHTTPResponseBodyData( pResult->m_hRequest, (uint8*) body.Base(), size ) )
				{
					body[size] = '\0';
					char tag[64], url[256];
					if ( GESS_JsonString( body.Base(), "tag_name", tag, sizeof( tag ) ) )
						SetLatest( tag, GESS_JsonString( body.Base(), "html_url", url, sizeof( url ) ) ? url : NULL );
				}
			}
		}

		pHTTP->ReleaseHTTPRequest( pResult->m_hRequest );
		if ( pResult->m_hRequest == m_hRequest )
			m_hRequest = INVALID_HTTPREQUEST_HANDLE;
	}

	void SetLatest( const char *pszTag, const char *pszURL )
	{
		// Shown as "v0.3" whether the tag has the v or not
		const char *pszNum = pszTag;
		while ( *pszNum == 'v' || *pszNum == 'V' )
			++pszNum;
		Q_snprintf( m_szLatest, sizeof( m_szLatest ), "v%s", pszNum );

		// Only GitHub links are opened; a faked version gets the latest release page
		if ( pszURL && !Q_strncmp( pszURL, "https://github.com/", 19 ) )
			Q_strncpy( m_szReleaseURL, pszURL, sizeof( m_szReleaseURL ) );
		else
			Q_strncpy( m_szReleaseURL, GES_SS_RELEASES_URL, sizeof( m_szReleaseURL ) );

		m_bUpdate = GESS_CompareVersions( m_szLatest, GES_SS_VERSION ) > 0;
		PrintStatus();
	}

	// Readable from the sofa but quieter than the menu: 1/38 of the screen height,
	// the update line a little smaller
	void UpdateFonts()
	{
		int sw, sh;
		vgui::surface()->GetScreenSize( sw, sh );
		if ( sh == m_iFontScreenTall )
			return;
		m_iFontScreenTall = sh;
		const int flags = vgui::ISurface::FONTFLAG_ANTIALIAS | vgui::ISurface::FONTFLAG_DROPSHADOW;
		vgui::HFont font = vgui::surface()->CreateFont();
		if ( vgui::surface()->SetFontGlyphSet( font, "Verdana", Max( 12, sh / 38 ), 700, 0, 0, flags ) )
			m_hFont = font;
		font = vgui::surface()->CreateFont();
		if ( vgui::surface()->SetFontGlyphSet( font, "Verdana", Max( 11, sh / 46 ), 700, 0, 0, flags ) )
			m_hSmallFont = font;
	}

	const wchar_t *VersionText()
	{
		V_UTF8ToUnicode( "Split Screen Mod v" GES_SS_VERSION, m_wszVersion, sizeof( m_wszVersion ) );
		return m_wszVersion;
	}

	const wchar_t *UpdateText()
	{
		char szText[128];
		Q_snprintf( szText, sizeof( szText ), "Update available: %s - click to download", m_szLatest );
		V_UTF8ToUnicode( szText, m_wszUpdate, sizeof( m_wszUpdate ) );
		return m_wszUpdate;
	}

	static int LineTall( vgui::HFont font )
	{
		return font != vgui::INVALID_FONT ? vgui::surface()->GetFontTall( font ) : 0;
	}

	static int TextWide( vgui::HFont font, const wchar_t *psz )
	{
		if ( font == vgui::INVALID_FONT )
			return 0;
		int w, h;
		vgui::surface()->GetTextSize( font, psz, w, h );
		return w + 2;	// room for the shadow
	}

	void DrawRightAligned( vgui::HFont font, const wchar_t *psz, int w, int y, Color col )
	{
		if ( font == vgui::INVALID_FONT )
			return;
		const int x = w - TextWide( font, psz );
		vgui::surface()->DrawSetTextFont( font );
		vgui::surface()->DrawSetTextColor( Color( 0, 0, 0, col.a() ) );
		vgui::surface()->DrawSetTextPos( x + 2, y + 2 );
		vgui::surface()->DrawPrintText( psz, wcslen( psz ) );
		vgui::surface()->DrawSetTextColor( col );
		vgui::surface()->DrawSetTextPos( x, y );
		vgui::surface()->DrawPrintText( psz, wcslen( psz ) );
	}

	vgui::HFont m_hFont;
	vgui::HFont m_hSmallFont;
	int m_iFontScreenTall;

	bool m_bChecked;
	bool m_bUpdate;
	HTTPRequestHandle m_hRequest;
	CCallResult<CGESSVersionLabel, HTTPRequestCompleted_t> m_CallResult;

	char m_szLatest[64];
	char m_szReleaseURL[256];
	wchar_t m_wszVersion[64];
	wchar_t m_wszUpdate[128];
};

static GameUI<CGESSVersionLabel> g_GESSVersionLabel;

CON_COMMAND( ges_ss_version, "Show the Split Screen Mod version and whether an update is out" )
{
	if ( g_GESSVersionLabel.GetNativePanel() )
		g_GESSVersionLabel->PrintStatus();
	else
		Msg( "Split Screen Mod v%s\n", GES_SS_VERSION );
}

CON_COMMAND( ges_ss_check_update, "Check GitHub for a newer Split Screen Mod. ges_ss_check_update <version> pretends that version is the newest release" )
{
	if ( g_GESSVersionLabel.GetNativePanel() )
		g_GESSVersionLabel->CheckAgain( args.ArgC() > 1 ? args[1] : NULL );
}
