///////////// Copyright 2026 GoldenEye: Source. All rights reserved. /////////////
//
//   Project     : Client (GES)
//   File        : ge_splitscreen_client.cpp
//   Description :
//      Client side of local split-screen multiplayer. See ge_splitscreen_client.h
//
////////////////////////////////////////////////////////////////////////////

#include "cbase.h"
#include "ge_splitscreen_client.h"
#include "ge_splitscreen_gamepad.h"
#include "ge_splitscreen_profiles.h"
#include "ge_splitscreen_match.h"
#include "ge_musicmanager.h"
#include "ge_character_data.h"
#include "ge_customchar.h"
#include "c_gemp_player.h"
#include "c_hl2mp_player.h"
#include "usercmd.h"
#include "in_buttons.h"
#include "gamerules.h"
#include "view.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IInput.h"
#include "vgui/IInputInternal.h"
#include "vgui/ILocalize.h"

// ge_splitscreen_hud.cpp: shows / hides the mouse catcher over player 1's START menu
void GESS_UpdateMenuMouse( bool bWant );
#include "vgui_controls/Controls.h"
#include "ienginevgui.h"
#include "game/client/iviewport.h"
#include "viewport_panel_names.h"
#include "gemp_gamerules.h"
#include "ge_shareddefs.h"
#include "inetchannelinfo.h"
#include "inputsystem/iinputsystem.h"
#include "clientsteamcontext.h"
#include "steam/steam_api.h"

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#undef GetCurrentTime
	#undef CreateEvent
	#undef PlaySound
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern vgui::IInputInternal *g_InputInternal;
extern ConVar default_fov;

ConVar ges_ss_enable( "ges_ss_enable", "1", FCVAR_ARCHIVE, "Enable local split screen players (controllers press START to join)." );
ConVar ges_ss_host_pad( "ges_ss_host_pad", "1", FCVAR_ARCHIVE, "1: the first controller to press a button is player 1 (the main player, drives the menus), the next ones players 2-8. 0: player 1 uses keyboard/mouse only and every controller is a guest." );
ConVar ges_ss_dualscreen( "ges_ss_dualscreen", "2", FCVAR_ARCHIVE, "Second screen for players 5-8. 0: never, 1: always split the window into two screens, 2: auto when the window spans two monitors." );
ConVar ges_ss_dualscreen_aspect( "ges_ss_dualscreen_aspect", "2.6", FCVAR_ARCHIVE, "Window aspect ratio above which ges_ss_dualscreen 2 treats the window as two monitors side by side." );
ConVar ges_ss_look_yaw( "ges_ss_look_yaw", "200", FCVAR_ARCHIVE, "Controller look speed (degrees per second at full stick) before the profile's sensitivity." );
ConVar ges_ss_look_pitch( "ges_ss_look_pitch", "140", FCVAR_ARCHIVE, "Controller look up/down speed (degrees per second at full stick) before the profile's sensitivity." );
static ConVar cl_ges_ss_pad( "cl_ges_ss_pad", "0", FCVAR_USERINFO | FCVAR_DONTRECORD, "Set by split screen: player 1 plays this game with a controller (the server gives controller players aim assist)." );
ConVar ges_ss_look_aim_scale( "ges_ss_look_aim_scale", "0.5", FCVAR_ARCHIVE, "Controller look speed multiplier while in aim mode." );
ConVar ges_ss_padmouse( "ges_ss_padmouse", "1", FCVAR_ARCHIVE, "Let controller 1 move the mouse cursor in menus. The right trigger is the left mouse button." );
ConVar ges_ss_padmouse_speed( "ges_ss_padmouse_speed", "1100", FCVAR_ARCHIVE, "Cursor speed in pixels per second (at 1080p) when moving it with controller 1." );
ConVar ges_ss_kbm_off( "ges_ss_kbm_off", "1", FCVAR_ARCHIVE, "Turn keyboard and mouse game input off during split screen matches, so only controllers play (Esc, the console and menus still work)." );
static void GESS_CleanName( char *dst, int len, const char *src );

// Player 1's own name while their profile name is in use (saved so it can be
// put back even if the game closed mid-match)
ConVar ges_ss_host_realname( "ges_ss_host_realname", "", FCVAR_ARCHIVE | FCVAR_HIDDEN, "Player 1's own name, saved while a split screen profile name is in use." );
ConVar ges_ss_host_renamed( "ges_ss_host_renamed", "0", FCVAR_ARCHIVE | FCVAR_HIDDEN, "1 while player 1 is using a split screen profile name." );
ConVar ges_ss_rejoin_timeout( "ges_ss_rejoin_timeout", "5", 0, "Seconds to wait for the server to create a split screen player before asking again." );

static ConVarRef cl_forwardspeed( "cl_forwardspeed" );
static ConVarRef cl_sidespeed( "cl_sidespeed" );

static CGESplitScreenClient g_GESplitScreenClient;
CGESplitScreenClient *GESplitScreenClient() { return &g_GESplitScreenClient; }

bool GESS_IsSplitActive() { return g_GESplitScreenClient.IsSplitActive(); }
bool GESS_ShouldHideForCurrentView( C_BaseEntity *pEntity ) { return g_GESplitScreenClient.ShouldHideForCurrentView( pEntity ); }
bool GESS_ShouldHideParticlesForCurrentView( C_BaseEntity *pOwner ) { return g_GESplitScreenClient.ShouldHideParticlesForCurrentView( pOwner ); }
bool GESS_ShouldHideLightsForCurrentView()
{
	static ConVarRef cl_ge_disablebloodscreen( "cl_ge_disablebloodscreen" );
	if ( cl_ge_disablebloodscreen.IsValid() && cl_ge_disablebloodscreen.GetBool() )
		return false;

	// The player whose view this is (player 1 outside split screen)
	C_BasePlayer *pViewer = ToBasePlayer( g_GESplitScreenClient.GetRenderViewer() );
	if ( !pViewer )
		pViewer = C_BasePlayer::GetLocalPlayer();
	if ( !pViewer || pViewer->IsAlive() || pViewer->IsObserver() )
		return false;

	// Only while their blood is up (GE plays it on their second viewmodel)
	C_BaseViewModel *pBlood = pViewer->GetViewModel( 1 );
	return pBlood && !pBlood->IsEffectActive( EF_NODRAW ) && pBlood->GetSequence() > 0;
}
C_BaseEntity *GESS_GetRenderViewer() { return g_GESplitScreenClient.GetRenderViewer(); }
bool GESS_ShouldSuppressGameMenus() { return g_GESplitScreenClient.ShouldSuppressGameMenus(); }

bool GESS_IsLocalGuest( C_BaseEntity *pEntity )
{
	C_GEMPPlayer *pPlayer = ToGEMPPlayer( pEntity );
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	return pPlayer && pLocal && pPlayer != pLocal && pPlayer->IsSplitGuest() && pPlayer->GetSplitOwnerIndex() == pLocal->entindex();
}

bool GESS_GetPlayerViewSetup( C_BaseEntity *pPlayer, Vector &origin, QAngle &angles, float &fov, float &fovViewmodel )
{
	if ( !pPlayer || !g_GESplitScreenClient.IsSplitActive() )
		return false;

	int slot = -1;
	if ( pPlayer == C_BasePlayer::GetLocalPlayer() )
		slot = 0;
	else if ( GESS_IsLocalGuest( pPlayer ) )
		slot = ToGEMPPlayer( pPlayer )->GetSplitSlot();
	return slot >= 0 && g_GESplitScreenClient.GetSlotViewSetup( slot, origin, angles, fov, fovViewmodel );
}

void CGESplitScreenClient::SetSlotViewSetup( int slot, const Vector &origin, const QAngle &angles, float fov, float fovViewmodel )
{
	if ( slot < 0 || slot >= GES_SS_MAX_PLAYERS )
		return;
	SlotView_t &v = m_SlotViews[slot];
	v.origin = origin;
	v.angles = angles;
	v.fov = fov;
	v.fovViewmodel = fovViewmodel;
	v.frame = gpGlobals->framecount;
}

bool CGESplitScreenClient::GetSlotViewSetup( int slot, Vector &origin, QAngle &angles, float &fov, float &fovViewmodel ) const
{
	if ( slot < 0 || slot >= GES_SS_MAX_PLAYERS )
		return false;
	const SlotView_t &v = m_SlotViews[slot];
	// Drawn this frame or the one before (effects are created before the views are drawn)
	if ( v.frame <= 0 || gpGlobals->framecount - v.frame > 2 )
		return false;
	origin = v.origin;
	angles = v.angles;
	fov = v.fov;
	fovViewmodel = v.fovViewmodel;
	return true;
}

void CGESplitScreenClient::SetFrameGuestView( int slot, const Vector &origin, const QAngle &angles, float fov )
{
	if ( slot < 0 || slot >= GES_SS_MAX_PLAYERS )
		return;
	SlotView_t &v = m_FrameGuestViews[slot];
	v.origin = origin;
	v.angles = angles;
	v.fov = fov;
	v.fovViewmodel = 0;
	v.frame = gpGlobals->framecount;
}

bool CGESplitScreenClient::GetFrameGuestView( int slot, Vector &origin, QAngle &angles, float &fov ) const
{
	if ( slot < 0 || slot >= GES_SS_MAX_PLAYERS )
		return false;
	const SlotView_t &v = m_FrameGuestViews[slot];
	if ( v.frame != gpGlobals->framecount )
		return false;
	origin = v.origin;
	angles = v.angles;
	fov = v.fov;
	return true;
}

//-----------------------------------------------------------------------------
// Guests' first person guns
//
// Player 1's gun is placed in CViewRender::SetUpViews, before entities simulate:
// animation events (firing), muzzle flashes and particles read the gun's
// attachments then, which sets up its bones, and those are kept for the rest of
// the frame. Guests' guns used to be placed only when their views were drawn,
// after that, so whenever something read their attachments first, the gun was
// drawn with bones from wherever it was before: last frame's view, or a stale
// interpolated position, off to one side, turned, or out of sight.
//-----------------------------------------------------------------------------
static ConVar ges_ss_vm_early( "ges_ss_vm_early", "1", FCVAR_DONTRECORD, "Split screen guests' first person guns are placed with player 1's, before entities simulate (0: when their view is drawn, the old way; for testing)." );
static ConVar ges_ss_debug_vm( "ges_ss_debug_vm", "0", FCVAR_DONTRECORD, "Testing: log where split screen player <1-8>'s first person gun is relative to their view, every frame (0: off)." );

void CGESplitScreenClient::DebugLook( int slot, float yawSpeed, float pitchSpeed, float seconds )
{
	if ( slot < 1 || slot >= GES_SS_MAX_PLAYERS )
		return;
	m_flDebugLookYaw[slot] = yawSpeed;
	m_flDebugLookPitch[slot] = pitchSpeed;
	m_flDebugLookUntil[slot] = Plat_FloatTime() + seconds;
	Msg( "ges_ss_debug_look: player %d (%s) turns %.0f / %.0f degrees a second for %.1f seconds\n", slot + 1,
		m_Slots[slot].state == GESS_SLOT_JOINED ? "playing" : "not playing", yawSpeed, pitchSpeed, seconds );
}

void CGESplitScreenClient::UpdateDebugLook( float dt )
{
	const double now = Plat_FloatTime();
	for ( int slot = 1; slot < GES_SS_MAX_PLAYERS; slot++ )
	{
		LocalSlot_t &s = m_Slots[slot];
		if ( now >= m_flDebugLookUntil[slot] || s.state != GESS_SLOT_JOINED )
			continue;
		s.angles[YAW] = AngleNormalize( s.angles[YAW] + m_flDebugLookYaw[slot] * dt );
		s.angles[PITCH] = clamp( s.angles[PITCH] + m_flDebugLookPitch[slot] * dt, -89.0f, 89.0f );
	}
}

CON_COMMAND_F( ges_ss_debug_look, "Testing: ges_ss_debug_look <player 2-8> <yaw degrees/s> <pitch degrees/s, + is down> [seconds] turns that player", FCVAR_DONTRECORD )
{
	if ( args.ArgC() < 4 )
		return;
	GESplitScreenClient()->DebugLook( atoi( args[1] ) - 1, atof( args[2] ), atof( args[3] ), args.ArgC() > 4 ? atof( args[4] ) : 1.0f );
}

bool GESS_PlaceGuestViewModelsEarly()
{
	return ges_ss_vm_early.GetBool();
}

bool GESS_DebugViewModelActive()
{
	return ges_ss_debug_vm.GetInt() > 0 && g_GESplitScreenClient.IsSplitActive();
}

// Where the logged gun's bones were last set up from
static Vector s_vecDebugBoneOrigin;
static QAngle s_angDebugBoneAngles;
static int s_iDebugBoneFrame = -1;

void GESS_DebugViewModel( const char *pszStage, C_BaseEntity *pEnt )
{
	if ( !GESS_DebugViewModelActive() || !pEnt )
		return;

	C_BaseViewModel *pVM = dynamic_cast<C_BaseViewModel*>( pEnt );
	if ( !pVM || pVM->ViewModelIndex() != 0 )
		return;

	const int slot = ges_ss_debug_vm.GetInt() - 1;
	C_BasePlayer *pOwner = ToBasePlayer( pVM->GetOwner() );
	if ( !pOwner || pOwner != g_GESplitScreenClient.GetSlotPlayer( slot ) )
		return;

	const bool bBones = !Q_strcmp( pszStage, "bones" );
	if ( bBones )
	{
		s_vecDebugBoneOrigin = pVM->GetRenderOrigin();
		s_angDebugBoneAngles = pVM->GetRenderAngles();
		s_iDebugBoneFrame = gpGlobals->framecount;
	}

	// This frame's view of that player
	Vector viewOrigin;
	QAngle viewAngles;
	float fov;
	if ( slot == 0 )
	{
		viewOrigin = MainViewOrigin();
		viewAngles = MainViewAngles();
	}
	else if ( !g_GESplitScreenClient.GetFrameGuestView( slot, viewOrigin, viewAngles, fov ) &&
		!g_GESplitScreenClient.GetGuestView( slot, viewOrigin, viewAngles, fov ) )
	{
		return;
	}

	Vector forward, right, up;
	AngleVectors( viewAngles, &forward, &right, &up );

	// The gun relative to the view: offset (right, up, forward) and turn
	const Vector &origin = bBones ? s_vecDebugBoneOrigin : pVM->GetAbsOrigin();
	const QAngle &angles = bBones ? s_angDebugBoneAngles : pVM->GetAbsAngles();
	Vector d = origin - viewOrigin;
	char szLine[384];
	int len = Q_snprintf( szLine, sizeof( szLine ), "[vm] f%d t%.3f %-8s rs%d seq%d view(%.1f %.1f) org(r%6.2f u%6.2f f%6.2f) ang(%5.1f %5.1f %5.1f)",
		gpGlobals->framecount, gpGlobals->curtime, pszStage, g_GESplitScreenClient.GetRenderSlot(), pVM->GetSequence(), viewAngles[PITCH], viewAngles[YAW],
		right.Dot( d ), up.Dot( d ), forward.Dot( d ),
		AngleDiff( angles[PITCH], viewAngles[PITCH] ), AngleDiff( angles[YAW], viewAngles[YAW] ), AngleDiff( angles[ROLL], viewAngles[ROLL] ) );

	// When drawn: where its bones were set up from (what's actually drawn)
	if ( !bBones && !Q_strcmp( pszStage, "draw" ) )
	{
		if ( s_iDebugBoneFrame == gpGlobals->framecount )
		{
			d = s_vecDebugBoneOrigin - viewOrigin;
			Q_snprintf( szLine + len, sizeof( szLine ) - len, " bones(r%6.2f u%6.2f f%6.2f ang %5.1f %5.1f)%s",
				right.Dot( d ), up.Dot( d ), forward.Dot( d ),
				AngleDiff( s_angDebugBoneAngles[PITCH], viewAngles[PITCH] ), AngleDiff( s_angDebugBoneAngles[YAW], viewAngles[YAW] ),
				( s_vecDebugBoneOrigin - pVM->GetAbsOrigin() ).Length() > 0.01f || fabs( AngleDiff( s_angDebugBoneAngles[YAW], angles[YAW] ) ) > 0.01f || fabs( AngleDiff( s_angDebugBoneAngles[PITCH], angles[PITCH] ) ) > 0.01f ? " STALE" : "" );
		}
		else
		{
			Q_snprintf( szLine + len, sizeof( szLine ) - len, " bones(from frame %d)", s_iDebugBoneFrame );
		}
		Msg( "%s\n", szLine );

		// Its animation, and where its bones ended up in front of the view
		CStudioHdr *pHdr = pVM->GetModelPtr();
		Vector mins( FLT_MAX, FLT_MAX, FLT_MAX ), maxs( -FLT_MAX, -FLT_MAX, -FLT_MAX );
		const int numBones = pHdr ? pHdr->numbones() : 0;
		for ( int i = 0; i < numBones; i++ )
		{
			matrix3x4_t m;
			pVM->GetCachedBoneMatrix( i, m );
			Vector p;
			MatrixPosition( m, p );
			p -= viewOrigin;
			const Vector v( right.Dot( p ), up.Dot( p ), forward.Dot( p ) );
			mins = mins.Min( v );
			maxs = maxs.Max( v );
		}
		Msg( "[vm]       seq %d %s cycle %.3f animtime %.3f (now %.3f) rate %.2f fx %d bones %d r[%.1f %.1f] u[%.1f %.1f] f[%.1f %.1f]\n",
			pVM->GetSequence(), pHdr ? pVM->GetSequenceName( pVM->GetSequence() ) : "?", pVM->GetCycle(), pVM->m_flAnimTime, gpGlobals->curtime,
			pVM->GetPlaybackRate(), pVM->GetFxBlend(), numBones, mins.x, maxs.x, mins.y, maxs.y, mins.z, maxs.z );

		// Which gun it shows, against the weapon it belongs to and the one in their hands
		C_BaseCombatWeapon *pVMWeapon = pVM->GetWeapon();
		C_BaseCombatWeapon *pActive = pOwner->GetActiveWeapon();
		const char *pszModel = modelinfo->GetModelName( pVM->GetModel() );
		const char *pszActiveVM = pActive ? pActive->GetViewModel() : "";
		Msg( "[vm]       ent %d model %s weapon %s active %s (%s)%s\n", pVM->entindex(), pszModel ? pszModel : "-",
			pVMWeapon ? pVMWeapon->GetClassname() : "-", pActive ? pActive->GetClassname() : "-", pszActiveVM,
			( pszModel && pszActiveVM[0] && Q_stricmp( pszModel, pszActiveVM ) ) ? " MISMATCH" : "" );
		return;
	}
	Msg( "%s\n", szLine );
}

// Pause menu entries for a joined player
enum
{
	GESS_PAUSE_RESUME = 0,
	GESS_PAUSE_PROFILE,
	GESS_PAUSE_LEAVE,
	GESS_PAUSE_COUNT
};

static const float GESS_MENU_REPEAT = 0.22f;

//-----------------------------------------------------------------------------
// Setup
//-----------------------------------------------------------------------------
CGESplitScreenClient::CGESplitScreenClient() : CAutoGameSystemPerFrame( "CGESplitScreenClient" )
{
	for ( int i = 0; i < GES_SS_MAX_PLAYERS; i++ )
	{
		LocalSlot_t &s = m_Slots[i];
		s.state = GESS_SLOT_EMPTY;
		s.menu = GESS_MENU_NONE;
		s.menuSel = 0;
		s.menuSub = 0;
		s.menuConfirm = GESS_CONFIRM_NONE;
		s.menuConfirmSel = 0;
		s.profile = -1;
		s.angles.Init();
		s.angleResetSeen = -1;
		s.joinSentTime = 0;
		s.pendingCycle = 0;
		s.menuRepeatTime = 0;
		s.profileChosen = false;
		s.debugNoPad = false;
		s.team = 0;
		s.ctrlRow = GESS_CTRLROW_PAGE;
		s.calOpen = false;
		s.calRow = GESS_CALROW_REMEASURE;
		s.calPad = -1;
		memset( s.calBackup, 0, sizeof( s.calBackup ) );
		m_GuestProfile[i] = CGESSProfiles::DefaultProfile();
		m_iSlotPad[i] = -1;
		m_bSlotHadPad[i] = false;
		m_SlotViews[i].frame = 0;
		m_FrameGuestViews[i].frame = 0;
		m_flDebugLookYaw[i] = m_flDebugLookPitch[i] = 0.0f;
		m_flDebugLookUntil[i] = 0.0;
		m_bViewportValid[i] = false;
		m_Viewports[i].x = m_Viewports[i].y = m_Viewports[i].w = m_Viewports[i].h = 0;
	}

	m_bSplitActive = false;
	m_bDualScreen = false;
	m_iSpareCells = 0;
	m_flExitToMenuTime = 0;
	m_bPregame = false;
	m_iPregameSlot = -1;
	m_iPregameStage = GESS_PREGAME_CLAIM;
	m_iPregameTeam = TEAM_MI6;
	m_flPregameStickTime = 0;
	for ( int i = 0; i < GES_SS_MAX_PLAYERS; i++ )
	{
		m_bPregameJoined[i] = false;
		m_iPregameProfile[i] = -2;
	}
	m_iRenderSlot = -1;
	m_bRenderViewerFirstPerson = false;
	m_flCursorX = m_flCursorY = 0;
	m_bPadMouseDown = false;
	m_bCursorWasVisible = false;
	m_iMenuNavPad = -1;
	for ( int i = 0; i < GES_SS_MAX_PADS; i++ )
	{
		m_flPadStartTime[i] = -1000.0;
		m_bPadStartUnseen[i] = false;
	}
	m_iMenuNavDir = -1;
	m_flMenuNavRepeat = 0;
	m_bMenuNavADown = m_bMenuNavBDown = false;
	m_bMenuMouseHidden = false;
	m_bApplyHostProfile = false;
	m_bHostRenamed = false;
	m_bHostChosen = false;
	m_bHostAppliedThisLevel = false;
	m_flHidePanelsUntil = 0;
	m_bTeamplaySynced = false;
}

bool CGESplitScreenClient::Init()
{
	GESSProfiles()->Load();
	GESSGamepads()->Init();
	// Split screen's own text (the START menu's suicide button), easy to change there
	if ( g_pVGuiLocalize )
		g_pVGuiLocalize->AddFile( "resource/ges_splitscreen_%language%.txt", "MOD" );
	return true;
}

void CGESplitScreenClient::RestoreHostName()
{
	if ( ( m_bHostRenamed || ges_ss_host_renamed.GetBool() ) && ges_ss_host_realname.GetString()[0] )
	{
		char szName[GES_SS_PROFILE_NAME_LEN];
		GESS_CleanName( szName, sizeof( szName ), ges_ss_host_realname.GetString() );
		engine->ClientCmd_Unrestricted( VarArgs( "name \"%s\"\n", szName ) );
	}
	m_bHostRenamed = false;
	ges_ss_host_renamed.SetValue( 0 );
}

void CGESplitScreenClient::Shutdown()
{
	RestoreHostName();
	GESSGamepads()->Shutdown();
}

void CGESplitScreenClient::LevelInitPostEntity()
{
	// Re-apply the split screen match limits after the scenario config ran
	GESSMatch()->OnLevelInit();

	// Player 1 plays as their profile, not their Steam name (once they've picked it,
	// see UpdateSlot), and their team / character are sent again on the new map
	m_bApplyHostProfile = true;
	m_bHostAppliedThisLevel = false;
	m_Slots[0].menu = GESS_MENU_NONE;
	m_bTeamplaySynced = false;

	// Guests that were playing (or got ready in the menus) join the new game
	for ( int i = 1; i < GES_SS_MAX_PLAYERS; i++ )
	{
		LocalSlot_t &s = m_Slots[i];
		s.menu = GESS_MENU_NONE;
		s.angleResetSeen = -1;
		if ( s.state == GESS_SLOT_JOINED || s.state == GESS_SLOT_JOINING )
		{
			s.state = GESS_SLOT_JOINING;
			s.joinSentTime = -1000.0f;	// send right away
		}
		else
		{
			s.state = GESS_SLOT_EMPTY;
		}
	}

	// The first map of a match from the Split Screen menu: the players take their
	// screens before the match starts
	m_bPregame = false;
	if ( GESSMatch()->IsPregamePending() && IsSplitMatch() )
		BeginPregame();
}

void CGESplitScreenClient::LevelShutdownPreEntity()
{
	m_bSplitActive = false;
	m_bPregame = false;
	SetRenderSlot( -1 );
	for ( int i = 0; i < GES_SS_MAX_PLAYERS; i++ )
		m_Slots[i].menu = GESS_MENU_NONE;
}

//-----------------------------------------------------------------------------
// Queries
//-----------------------------------------------------------------------------
bool CGESplitScreenClient::IsInGame() const
{
	return engine->IsInGame() && !engine->IsLevelMainMenuBackground() && C_BasePlayer::GetLocalPlayer() != NULL;
}

bool CGESplitScreenClient::IsGameUIVisible() const
{
	return enginevgui->IsGameUIVisible() || vgui::surface()->IsCursorVisible();
}

int CGESplitScreenClient::GetSlotPad( int slot ) const
{
	if ( slot < 0 || slot >= GES_SS_MAX_PLAYERS )
		return -1;
	return m_iSlotPad[slot];
}

int CGESplitScreenClient::GetPadSlot( int pad ) const
{
	if ( pad < 0 )
		return -1;
	for ( int i = 0; i < GES_SS_MAX_PLAYERS; i++ )
	{
		if ( m_iSlotPad[i] == pad )
			return i;
	}
	return -1;
}

bool CGESplitScreenClient::IsWaitingForHostPad() const
{
	return ges_ss_enable.GetBool() && ges_ss_host_pad.GetBool() && m_iSlotPad[0] < 0 && HasAnyPadConnected();
}

bool CGESplitScreenClient::HasSparePad() const
{
	for ( int pad = 0; pad < GES_SS_MAX_PADS; pad++ )
	{
		if ( !GESSGamepads()->GetPad( pad ).connected )
			continue;
		const int slot = GetPadSlot( pad );
		if ( slot < 0 || ( slot > 0 && m_Slots[slot].state == GESS_SLOT_EMPTY ) )
			return true;
	}
	return false;
}

bool CGESplitScreenClient::IsSlotPadDisconnected( int slot ) const
{
	if ( slot < 0 || slot >= GES_SS_MAX_PLAYERS || !IsInGame() || m_iSlotPad[slot] >= 0 || !m_bSlotHadPad[slot] )
		return false;
	return slot == 0 ? ges_ss_host_pad.GetBool() : m_Slots[slot].state != GESS_SLOT_EMPTY;
}

bool CGESplitScreenClient::IsControllerOnly() const
{
	return ges_ss_kbm_off.GetBool() && ShouldSuppressGameMenus();
}

// Player 1's controller is read here too (UpdateHostLook, the menu navigation), so
// whenever split screen has a controller backend, the engine's copy of an Xbox pad
// (whichever player that pad belongs to, it runs player 1's bindings) is only a duplicate.
// Without a backend (no SDL2 or XInput) the engine's controller support still works.
static ConVar ges_ss_block_engine_pad( "ges_ss_block_engine_pad", "1", FCVAR_ARCHIVE, "1: while split screen reads the controllers, the engine's own controller buttons (Xbox pads) don't run key bindings, so a guest's Xbox pad never also fires or jumps for player 1." );

bool CGESplitScreenClient::ShouldBlockEnginePadButtons() const
{
	return ges_ss_block_engine_pad.GetBool() && ges_ss_enable.GetBool() && GESSGamepads()->GetMaxPads() > 0;
}

// Controllers belong to players in the order they press a button. The first one
// is player 1 and drives the menus; the rest join as players 2-8. A controller
// only ever drives one player. Unplugged controllers free their player: player 1
// goes back to keyboard and mouse until another controller presses a button, a
// guest who is playing waits (idle) for the next spare controller.
static ConVar ges_ss_debug_pads( "ges_ss_debug_pads", "0", FCVAR_DONTRECORD, "Testing: log every controller button press and which player it belongs to." );

void CGESplitScreenClient::UpdatePadAssignment()
{
	const bool bHostPad = ges_ss_host_pad.GetBool();
	if ( !bHostPad && m_iSlotPad[0] >= 0 )
		m_iSlotPad[0] = -1;

	for ( int slot = 0; slot < GES_SS_MAX_PLAYERS; slot++ )
	{
		const int pad = m_iSlotPad[slot];
		if ( pad >= 0 && !GESSGamepads()->GetPad( pad ).connected )
		{
			m_iSlotPad[slot] = -1;
			Msg( "Split screen: player %d's controller was disconnected\n", slot + 1 );

			// During a match the player keeps their place ("Controller disconnected"
			// in their view) until a spare controller presses a button

			// Was ready in the menus: not any more
			if ( slot > 0 && !IsInGame() && m_Slots[slot].state != GESS_SLOT_EMPTY )
				LeaveSlot( slot );
		}
	}

	if ( !ges_ss_enable.GetBool() )
		return;

	// Before the match, screens are taken with the right trigger (UpdatePregame)
	if ( m_bPregame && IsInGame() )
		return;

	const bool bInGame = IsInGame();
	// In the menus any free player slot: players per display follows the players
	// who joined there, so it can't also limit which controllers get a player (a
	// second controller never got one, so only player 1 could ever join)
	const int maxSlots = bInGame ? GetMaxSlots() : GES_SS_MAX_PLAYERS;
	for ( int pad = 0; pad < GES_SS_MAX_PADS; pad++ )
	{
		const GESSPadState_t &state = GESSGamepads()->GetPad( pad );
		if ( !state.connected || !state.pressed )
			continue;

		if ( ges_ss_debug_pads.GetBool() )
			Msg( "Split screen: controller %d pressed buttons %04x (player %d, %s)\n", pad + 1, state.buttons, GetPadSlot( pad ) + 1, bInGame ? "in game" : "menus" );

		const int current = GetPadSlot( pad );
		if ( current == 0 )
			continue;

		// In a match, player 1 playing on keyboard and mouse (never had a controller)
		// doesn't take a controller that's replacing a guest's lost one
		bool bGuestWaiting = false;
		if ( bInGame && !m_bSlotHadPad[0] )
		{
			for ( int i = 1; i < maxSlots && !bGuestWaiting; i++ )
				bGuestWaiting = m_iSlotPad[i] < 0 && m_bSlotHadPad[i] && m_Slots[i].state != GESS_SLOT_EMPTY;
		}

		int slot = -1;
		if ( bHostPad && m_iSlotPad[0] < 0 && ( !bInGame || current < 0 ) && !bGuestWaiting )
		{
			// Nobody is player 1. In the menus the next controller to press a button
			// takes over (a spare one, or one whose player hasn't got ready yet). In a
			// match only a spare controller can: player 1's own one plugged back in
			// (or the first controller ever, if the game started straight into a map).
			// Another player's controller never drives player 1 mid-match.
			if ( current > 0 && m_Slots[current].state != GESS_SLOT_EMPTY )
				continue;
			slot = 0;
		}
		else if ( current > 0 )
		{
			continue;
		}
		else
		{
			// A guest who is still playing but lost their controller gets it first,
			// then the first free player
			for ( int i = 1; i < maxSlots && slot < 0; i++ )
			{
				if ( m_iSlotPad[i] < 0 && m_Slots[i].state != GESS_SLOT_EMPTY )
					slot = i;
			}
			for ( int i = 1; i < maxSlots && slot < 0; i++ )
			{
				if ( m_iSlotPad[i] < 0 && m_Slots[i].state == GESS_SLOT_EMPTY )
					slot = i;
			}
		}

		if ( slot < 0 )
			continue;

		if ( current > 0 )
			m_iSlotPad[current] = -1;
		m_iSlotPad[slot] = pad;
		m_bSlotHadPad[slot] = true;
		Msg( "Split screen: controller %d is player %d\n", pad + 1, slot + 1 );

		// The press that claimed player 1 doesn't also click or open anything
		if ( slot == 0 )
			GESSGamepads()->ConsumeEdges( pad );
	}
}

bool CGESplitScreenClient::HasAnyPadConnected() const
{
	return GESSGamepads()->GetConnectedCount() > 0;
}

C_BasePlayer *CGESplitScreenClient::GetSlotPlayer( int slot ) const
{
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	if ( slot == 0 || !pLocal )
		return pLocal;

	const int localIndex = pLocal->entindex();
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		C_GEMPPlayer *pPlayer = ToGEMPPlayer( UTIL_PlayerByIndex( i ) );
		if ( pPlayer && pPlayer->IsSplitGuest() && pPlayer->GetSplitOwnerIndex() == localIndex && pPlayer->GetSplitSlot() == slot )
			return pPlayer;
	}
	return NULL;
}

const GESSProfile_t &CGESplitScreenClient::GetSlotProfileData( int slot ) const
{
	const GESSProfile_t *p = GESSProfiles()->Get( m_Slots[slot].profile );
	return p ? *p : m_GuestProfile[slot];
}

GESSProfile_t *CGESplitScreenClient::GetSlotProfileForEdit( int slot )
{
	GESSProfile_t *p = GESSProfiles()->Get( m_Slots[slot].profile );
	return p ? p : &m_GuestProfile[slot];
}

const char *CGESplitScreenClient::GetSlotName( int slot ) const
{
	if ( m_Slots[slot].profile >= 0 )
		return GetSlotProfileData( slot ).name;

	static char s_szName[GES_SS_MAX_PLAYERS][32];
	if ( slot == 0 )
	{
		static ConVarRef name( "name" );
		Q_strncpy( s_szName[slot], name.IsValid() ? name.GetString() : "Player 1", sizeof( s_szName[slot] ) );
	}
	else
	{
		Q_snprintf( s_szName[slot], sizeof( s_szName[slot] ), "Player %d", slot + 1 );
	}
	return s_szName[slot];
}

// The next profile after "current" that no other player is using,
// wrapping round to "Guest" (-1) after the last one
int CGESplitScreenClient::NextFreeProfile( int slot, int current ) const
{
	const int count = GetMenuProfileCount();
	int start = 0;
	for ( int e = 0; current >= 0 && e < count; e++ )	// after Guest, start again at the first
	{
		if ( GetMenuProfileIndex( e ) == current )
		{
			start = e + 1;
			break;
		}
	}

	for ( int e = start; e < count; e++ )
	{
		const int idx = GetMenuProfileIndex( e );
		if ( idx < 0 || !IsSlotProfileInUse( idx, slot ) )
			return idx;
	}
	return -1;
}

int CGESplitScreenClient::CycleProfile( int slot, int current, int dir ) const
{
	const int count = GetMenuProfileCount();	// every profile, then Guest
	if ( count <= 0 )
		return current;

	int at = count - 1;	// Guest
	for ( int e = 0; e < count; e++ )
	{
		if ( GetMenuProfileIndex( e ) == current )
		{
			at = e;
			break;
		}
	}

	for ( int step = 1; step <= count; step++ )
	{
		const int e = ( ( at + dir * step ) % count + count ) % count;
		const int idx = GetMenuProfileIndex( e );
		if ( !IsSlotProfileInUse( idx, slot ) )
			return idx;
	}
	return current;
}

bool CGESplitScreenClient::IsSlotProfileInUse( int profile, int ignoreSlot ) const
{
	if ( profile < 0 )
		return false;	// Guest can be used many times

	for ( int i = 0; i < GES_SS_MAX_PLAYERS; i++ )
	{
		if ( i != ignoreSlot && m_Slots[i].profile == profile && ( i == 0 || m_Slots[i].state != GESS_SLOT_EMPTY ) )
			return true;
	}
	return false;
}

int CGESplitScreenClient::GetMenuProfileCount() const
{
	return GESSProfiles()->Count() + 1;	// Every profile, then Guest
}

int CGESplitScreenClient::GetMenuProfileIndex( int entry ) const
{
	if ( entry >= 0 && entry < GESSProfiles()->Count() )
		return entry;
	return -1;
}

int CGESplitScreenClient::GetPauseMenuCount() const
{
	return GESS_PAUSE_COUNT;
}

const char *CGESplitScreenClient::GetPauseMenuLabel( int entry ) const
{
	switch ( entry )
	{
	case GESS_PAUSE_RESUME:		return "Resume";
	case GESS_PAUSE_PROFILE:	return "Change profile";
	case GESS_PAUSE_LEAVE:		return "Leave game";
	}
	return "";
}

const char *CGESplitScreenClient::GetTeamMenuLabel( int entry ) const
{
	switch ( entry )
	{
	case GESS_TEAMMENU_AUTO:	return "Auto assign";
	case GESS_TEAMMENU_MI6:		return "MI6";
	case GESS_TEAMMENU_JANUS:	return "Janus";
	}
	return "";
}

bool CGESplitScreenClient::IsTeamplay() const
{
	return GEMPRules() && GEMPRules()->IsTeamplay();
}

bool CGESplitScreenClient::HasLocalGuests() const
{
	for ( int i = 1; i < GES_SS_MAX_PLAYERS; i++ )
	{
		if ( m_Slots[i].state != GESS_SLOT_EMPTY )
			return true;
	}
	return false;
}

bool CGESplitScreenClient::IsSplitMatch() const
{
	if ( !GESSMatch()->IsMatchActive() )
		return false;

	INetChannelInfo *pNet = engine->GetNetChannelInfo();
	return pNet && pNet->IsLoopback();
}

// A split screen game (one started from the Split Screen menu, from the moment
// its map loads, or any game with local guests): GoldenEye's own team / character
// menus are replaced by the profile (and team) choice in each player's viewport
bool CGESplitScreenClient::ShouldSuppressGameMenus() const
{
	return ges_ss_enable.GetBool() && IsInGame() && ( HasLocalGuests() || IsSplitMatch() );
}

//-----------------------------------------------------------------------------
// Layout
//-----------------------------------------------------------------------------
void CGESplitScreenClient::GridForCount( int n, int &cols, int &rows )
{
	if ( n <= 1 )		{ cols = 1; rows = 1; }
	else if ( n == 2 )	{ cols = 1; rows = 2; }	// top / bottom halves
	else if ( n <= 4 )	{ cols = 2; rows = 2; }
	else if ( n <= 6 )	{ cols = 3; rows = 2; }
	else				{ cols = 4; rows = 2; }
}

int CGESplitScreenClient::GetDisplayCount() const
{
	if ( ges_ss_dualscreen.GetInt() == 1 )
		return 2;
	if ( ges_ss_dualscreen.GetInt() == 2 )
	{
		int w, h;
		engine->GetScreenSize( w, h );
		if ( h > 0 && (float) w / (float) h >= ges_ss_dualscreen_aspect.GetFloat() )
			return 2;
	}
	return 1;
}

int CGESplitScreenClient::GetPerDisplay() const
{
	// Our own split screen match (and the menus, where it's being set up) use
	// the Split Screen menu's setting
	if ( IsSplitMatch() || !engine->IsInGame() )
	{
		GESSMatch()->EnsureLoaded();
		return clamp( GESSMatch()->Settings().perdisplay, 1, GES_SS_MAX_PLAYERS );
	}
	return GetDisplayCount() == 2 ? GES_SS_PLAYERS_PER_SCREEN : GES_SS_MAX_PLAYERS;
}

int CGESplitScreenClient::GetMaxSlots() const
{
	return MIN( GES_SS_MAX_PLAYERS, GetPerDisplay() * GetDisplayCount() );
}

bool CGESplitScreenClient::GetSpareCell( int i, GESSViewport_t &vp ) const
{
	if ( i < 0 || i >= m_iSpareCells )
		return false;
	vp = m_SpareCells[i];
	return true;
}

void CGESplitScreenClient::UpdateLayout()
{
	for ( int i = 0; i < GES_SS_MAX_PLAYERS; i++ )
		m_bViewportValid[i] = false;
	m_iSpareCells = 0;

	int count = 1;
	for ( int i = 1; i < GES_SS_MAX_PLAYERS; i++ )
	{
		if ( m_Slots[i].state != GESS_SLOT_EMPTY )
			count++;
	}

	const int maxSlots = GetMaxSlots();
	m_bSplitActive = ges_ss_enable.GetBool() && IsInGame() && ( count > 1 || ( m_bPregame && maxSlots > 1 ) );

	int w, h;
	engine->GetScreenSize( w, h );

	m_bDualScreen = m_bSplitActive && GetDisplayCount() == 2;

	if ( !m_bSplitActive )
	{
		m_bViewportValid[0] = true;
		m_Viewports[0].x = 0; m_Viewports[0].y = 0; m_Viewports[0].w = w; m_Viewports[0].h = h;
		return;
	}

	// Each display holds "players per display" players: 1 to N on the first,
	// the rest on the second when the window spans two monitors
	const int numScreens = m_bDualScreen ? 2 : 1;
	const int perDisplay = m_bDualScreen ? GetPerDisplay() : maxSlots;
	for ( int screen = 0; screen < numScreens; screen++ )
	{
		const int regionX = m_bDualScreen ? screen * ( w / 2 ) : 0;
		const int regionW = m_bDualScreen ? ( screen ? w - w / 2 : w / 2 ) : w;
		const int firstSlot = screen * perDisplay;
		const int lastSlot = MIN( firstSlot + perDisplay, maxSlots );

		// Before the match every screen is shown (players take them one by one),
		// afterwards the players who are in
		int active[GES_SS_MAX_PLAYERS], numActive = 0;
		for ( int slot = firstSlot; slot < lastSlot; slot++ )
		{
			if ( m_bPregame || slot == 0 || m_Slots[slot].state != GESS_SLOT_EMPTY )
				active[numActive++] = slot;
		}
		if ( numActive == 0 )
			continue;

		// An equal grid: every view is the same size. Before the match both
		// displays use the grid for the chosen number, so the views match.
		int cols, rows;
		GridForCount( m_bPregame ? perDisplay : numActive, cols, rows );

		int next = 0;
		for ( int r = 0; r < rows; r++ )
		{
			const int cellY = ( r * h ) / rows;
			const int cellH = ( ( r + 1 ) * h ) / rows - cellY;
			for ( int c = 0; c < cols; c++ )
			{
				GESSViewport_t cell;
				cell.x = regionX + ( c * regionW ) / cols;
				cell.w = regionX + ( ( c + 1 ) * regionW ) / cols - cell.x;
				cell.y = cellY;
				cell.h = cellH;

				if ( next < numActive )
				{
					const int slot = active[next++];
					m_Viewports[slot] = cell;
					m_bViewportValid[slot] = true;
				}
				else if ( m_iSpareCells < ARRAYSIZE( m_SpareCells ) )
				{
					// Nobody plays here: scoreboard / logo
					m_SpareCells[m_iSpareCells++] = cell;
				}
			}
		}
	}
}

bool CGESplitScreenClient::IsSlotVisible( int slot ) const
{
	return slot >= 0 && slot < GES_SS_MAX_PLAYERS && m_bViewportValid[slot];
}

bool CGESplitScreenClient::GetViewport( int slot, GESSViewport_t &vp ) const
{
	if ( !IsSlotVisible( slot ) )
		return false;
	vp = m_Viewports[slot];
	return true;
}

//-----------------------------------------------------------------------------
// Rendering helpers
//-----------------------------------------------------------------------------
void CGESplitScreenClient::SetRenderSlot( int slot )
{
	m_iRenderSlot = slot;

	C_BasePlayer *pViewer = slot >= 0 ? GetSlotPlayer( slot ) : NULL;
	m_hRenderViewer = pViewer;
	m_bRenderViewerFirstPerson = pViewer && pViewer->IsAlive() && !pViewer->IsObserver();
}

C_BaseEntity *CGESplitScreenClient::GetRenderViewer() const
{
	if ( !m_bSplitActive || m_iRenderSlot < 0 )
		return NULL;
	return m_hRenderViewer.Get();
}

bool CGESplitScreenClient::ShouldHideForCurrentView( C_BaseEntity *pEntity ) const
{
	if ( !m_bSplitActive || m_iRenderSlot < 0 || !pEntity || !m_bRenderViewerFirstPerson )
		return false;

	C_BaseEntity *pViewer = m_hRenderViewer.Get();
	if ( !pViewer )
		return false;

	// The viewer's own body and anything attached to it (weapons, hat, tokens)
	return pEntity == pViewer || pEntity->GetMoveParent() == pViewer || pEntity->GetOwnerEntity() == pViewer && pEntity->IsBaseCombatWeapon();
}

bool CGESplitScreenClient::ShouldHideParticlesForCurrentView( C_BaseEntity *pOwner ) const
{
	if ( !m_bSplitActive || m_iRenderSlot < 0 || !pOwner )
		return false;

	C_BaseEntity *pViewer = m_hRenderViewer.Get();

	// On a first person gun: only in its player's own (first person) view
	C_BaseViewModel *pVM = dynamic_cast<C_BaseViewModel*>( pOwner );
	if ( pVM )
		return !pViewer || !m_bRenderViewerFirstPerson || pVM->GetOwner() != pViewer;

	// On the viewer's own body or the gun in their hand, which they don't see
	return ShouldHideForCurrentView( pOwner );
}

bool CGESplitScreenClient::GetGuestView( int slot, Vector &origin, QAngle &angles, float &fov ) const
{
	C_BasePlayer *pPlayer = GetSlotPlayer( slot );
	if ( !pPlayer )
		return false;

	fov = pPlayer->GetFOV();
	if ( fov <= 0 )
		fov = default_fov.GetFloat();

	// Spectating someone (e.g. waiting to spawn)
	if ( pPlayer->IsObserver() )
	{
		C_BaseEntity *pTarget = pPlayer->GetObserverTarget();
		if ( pTarget && pTarget != pPlayer && pPlayer->GetObserverMode() == OBS_MODE_IN_EYE )
		{
			origin = pTarget->EyePosition();
			angles = pTarget->EyeAngles();
		}
		else
		{
			origin = pPlayer->GetAbsOrigin();
			angles = pPlayer->EyeAngles();
		}
		return true;
	}

	// Dead: look through the ragdoll's eyes like the main player does
	if ( !pPlayer->IsAlive() )
	{
		C_HL2MP_Player *pHL2 = dynamic_cast<C_HL2MP_Player*>( pPlayer );
		C_BaseAnimating *pRagdoll = pHL2 ? pHL2->GetRagdoll() : NULL;
		if ( pRagdoll )
		{
			int iEyes = pRagdoll->LookupAttachment( "eyes" );
			if ( iEyes > 0 && pRagdoll->GetAttachment( iEyes, origin, angles ) )
				return true;
		}
		// No body yet (it comes a moment after the death): stay where the view
		// was, not down at the floor
		const SlotView_t &last = m_FrameGuestViews[slot];
		if ( last.frame > 0 && gpGlobals->framecount - last.frame <= 2 )
		{
			origin = last.origin;
			angles = last.angles;
			return true;
		}
		const CViewVectors *pDeadVV = g_pGameRules ? g_pGameRules->GetViewVectors() : NULL;
		origin = pPlayer->GetAbsOrigin() + ( pDeadVV ? pDeadVV->m_vView : Vector( 0, 0, 64 ) );
		angles = m_Slots[slot].angles;
		return true;
	}

	// Alive: networked position, local angles so aiming responds instantly
	const CViewVectors *pVV = g_pGameRules ? g_pGameRules->GetViewVectors() : NULL;
	Vector offset = pVV ? ( ( pPlayer->GetFlags() & FL_DUCKING ) ? pVV->m_vDuckView : pVV->m_vView ) : Vector( 0, 0, 64 );
	origin = pPlayer->GetAbsOrigin() + offset;
	angles = m_Slots[slot].angles;
	return true;
}

bool CGESplitScreenClient::ShouldHideHudElement( const char *pszName ) const
{
	if ( !m_bSplitActive || !pszName )
		return false;

	// The normal HUD belongs to player 1 and would cover everyone's view. Keep only
	// elements that make sense for the whole screen, the split HUD draws the rest per player.
	static const char *s_pszKeep[] =
	{
		"CHudChat",
		"CGEHudDeathNotice",
		"CHudVoiceStatus",
		"CHudVoiceSelfStatus",
		"CHudCloseCaption",
		"CHudMessage",
		"CHudMenu",
		"CGESplitScreenHud",
	};

	for ( int i = 0; i < ARRAYSIZE( s_pszKeep ); i++ )
	{
		if ( !Q_stricmp( pszName, s_pszKeep[i] ) )
			return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Per frame
//-----------------------------------------------------------------------------
void CGESplitScreenClient::Update( float frametime )
{
	// Use real time, game frametime is zero in menus and scaled by host_timescale
	static double s_flLast = Plat_FloatTime();
	const double now = Plat_FloatTime();
	const float dt = clamp( (float)( now - s_flLast ), 0.0f, 0.1f );
	s_flLast = now;

	UpdateDebugLook( dt );

	// Player 1's START menu takes the mouse while it's open (a cursor shows)
	GESS_UpdateMenuMouse( WantsMenuMouse() );

	GESSMatch()->Update();

	// Player 1 left the match: after the results, back to the Split Screen menu
	if ( m_flExitToMenuTime > 0 && ( !engine->IsConnected() || gpGlobals->realtime >= m_flExitToMenuTime ) )
	{
		const bool bConnected = engine->IsConnected();
		m_flExitToMenuTime = 0;
		if ( bConnected )
		{
			engine->ClientCmd_Unrestricted( "disconnect\n" );
			engine->ClientCmd_Unrestricted( "ges_splitscreen_match\n" );
		}
	}

	// Put player 1's own name back after leaving the game (or after a crash),
	// and use their profile name once they're in a game
	if ( !engine->IsConnected() )
	{
		if ( m_bHostRenamed || ges_ss_host_renamed.GetBool() )
			RestoreHostName();
	}
	else if ( m_bApplyHostProfile && C_BasePlayer::GetLocalPlayer() )
	{
		// Only in a split screen game: online, player 1 keeps their own name.
		// In a split screen game the profile is applied once player 1 has
		// picked it (UpdateSlot / ApplyHostChoice).
		m_bApplyHostProfile = false;
		if ( !ShouldSuppressGameMenus() && ( m_bHostRenamed || ges_ss_host_renamed.GetBool() ) )
			RestoreHostName();
	}

	// Player 1 picks their profile / team with our menus: keep the game's own
	// team and character menus out of the way meanwhile
	if ( gViewPortInterface && IsInGame() && ( ShouldSuppressGameMenus() || m_Slots[0].menu == GESS_MENU_PROFILE || m_Slots[0].menu == GESS_MENU_TEAM || gpGlobals->realtime < m_flHidePanelsUntil ) )
	{
		gViewPortInterface->ShowPanel( PANEL_TEAM, false );
		gViewPortInterface->ShowPanel( PANEL_CHARSELECT, false );
	}

	// The full-screen spectator bars would cover the other players' views
	if ( gViewPortInterface && ( m_bSplitActive || ShouldSuppressGameMenus() ) )
	{
		IViewPortPanel *pSpec = gViewPortInterface->FindPanelByName( PANEL_SPECGUI );
		if ( pSpec && pSpec->IsVisible() )
			gViewPortInterface->ShowPanel( pSpec, false );
	}

	// A split screen game on our own server plays with or without teams as set on
	// the Split Screen menu's Match page, even when it wasn't started from there
	// (teams only change at a round start, so restart the round once if needed)
	if ( !m_bTeamplaySynced && ShouldSuppressGameMenus() )
	{
		INetChannelInfo *pNet = engine->GetNetChannelInfo();
		if ( pNet && pNet->IsLoopback() )
		{
			m_bTeamplaySynced = true;
			GESSMatch()->EnsureLoaded();
			const bool bTeams = GESSMatch()->Settings().teams;
			if ( IsTeamplay() != bTeams )
			{
				engine->ClientCmd_Unrestricted( VarArgs( "ge_teamplay %d\n", bTeams ? 1 : 0 ) );
				// Quietly: no round report, scores or "round was a tie" for a round nobody played
				engine->ClientCmd_Unrestricted( "ge_endround 0\n" );
			}
		}
	}

	// We read every controller ourselves (player 1's too). The engine turns its own
	// joystick input on when it sees an Xbox pad, and would then also move player 1
	// with whichever pad it picked (another player's), so keep it off.
	if ( ges_ss_enable.GetBool() && HasAnyPadConnected() )
	{
		static ConVarRef joystick( "joystick" );
		if ( joystick.IsValid() && joystick.GetBool() )
		{
			joystick.SetValue( 0 );
			DevMsg( "Split screen: turned the engine's joystick input off (split screen reads the controllers)\n" );
		}
	}

	// A player with their START menu open isn't playing: their sticks can be measured
	// at rest (the Controller page's Calibrate Sticks) even in a match
	for ( int pad = 0; pad < GES_SS_MAX_PADS; pad++ )
	{
		const int slot = GetPadSlot( pad );
		GESSGamepads()->SetPadInMenu( pad, slot >= 0 && m_Slots[slot].menu != GESS_MENU_NONE );
	}

	GESSGamepads()->Poll();

	// Note every START press, and whether the game's pause menu was already up
	// (then our own START handling skips it, see BlockPauseMenu)
	{
		const bool bGameUI = enginevgui->IsGameUIVisible();
		for ( int pad = 0; pad < GES_SS_MAX_PADS; pad++ )
		{
			if ( GESSGamepads()->GetPad( pad ).WasPressed( GESS_PAD_START ) )
			{
				m_flPadStartTime[pad] = now;
				m_bPadStartUnseen[pad] = bGameUI;
			}
		}
	}

	// In the Split Screen lobby, R3 recalibrates that controller's sticks (its card in
	// the Players list shows how). R3 does nothing else there; in a match it's use (doors, buttons).
	// (The menu panel can stay "open" behind a match, so check the menu is on screen and
	// no match is being played.)
	const bool bInMatch = engine->IsInGame() && !engine->IsLevelMainMenuBackground();
	if ( GESS_IsSplitScreenMenuOpen() && enginevgui->IsGameUIVisible() && !bInMatch )
	{
		for ( int i = 0; i < GES_SS_MAX_PADS; i++ )
		{
			if ( GESSGamepads()->GetPad( i ).WasPressed( GESS_PAD_RSTICK ) )
				GESSGamepads()->RecalibratePad( i );
		}
	}

	UpdatePadAssignment();
	UpdateMenuPadNav( dt );
	UpdatePadMouse( dt );
	// Tell the server whether player 1 plays with a controller in this split screen
	// game (Classic aim assist is only for controller players)
	{
		const int hostPad = GetSlotPad( 0 );
		const bool bPad = ( IsSplitMatch() || HasLocalGuests() ) && hostPad >= 0 && GESSGamepads()->GetPad( hostPad ).connected;
		if ( cl_ges_ss_pad.GetBool() != bPad )
			cl_ges_ss_pad.SetValue( bPad ? 1 : 0 );
	}
	BlockPauseMenu();
	UpdateHostLook( dt );
	UpdatePregame();

	if ( ges_ss_enable.GetBool() )
	{
		for ( int i = 0; i < GES_SS_MAX_PLAYERS; i++ )
			UpdateSlot( i, dt );
	}

	UpdateLayout();
}

//-----------------------------------------------------------------------------
// The game's own menus with a controller
//
// The engine's joystick input is turned off while we read the controllers (see
// Update), and it only ever knew Xbox pads anyway, so nothing drove the main
// menu, the pause menu or their dialogs with a PlayStation or Switch pad.
// Player 1's controller (any kind SDL or XInput reads) now sends vgui the keys
// an Xbox pad would: D-pad / left stick = KEY_XBUTTON_UP..RIGHT (held: repeats),
// A / Cross = KEY_XBUTTON_A, B / Circle = KEY_XBUTTON_B. vgui's menus, buttons
// and dialogs already understand those.
//
// While a controller is connected, the main menu (with no dialog over it) and
// the Split Screen menu hide the mouse cursor and ignore the mouse. Dialogs such
// as Options or Create Server keep the cursor (and the stick moves it): parts of
// them can only be used with a pointer.
//-----------------------------------------------------------------------------
static ConVar ges_ss_menu_pad_nav( "ges_ss_menu_pad_nav", "1", FCVAR_ARCHIVE, "Player 1's controller drives the game's own menus (main menu, pause menu, dialogs): D-pad / left stick, A (Cross) to choose, B (Circle) to go back." );
static ConVar ges_ss_menu_hide_mouse( "ges_ss_menu_hide_mouse", "0", FCVAR_ARCHIVE, "1: while a controller is connected, hide the mouse cursor on the main menu and in the Split Screen menu, and ignore the mouse there." );

// (windows.h renames these; the vgui methods are meant here)
#undef GetClassName
#undef SendMessage

// The window that has the keyboard focus (the popup holding the focused panel)
static vgui::VPANEL GESS_FocusWindow()
{
	vgui::VPANEL p = vgui::input()->GetFocus();
	while ( p && !vgui::ipanel()->IsPopup( p ) )
		p = vgui::ipanel()->GetParent( p );
	return p;
}

static const char *GESS_FocusWindowName()
{
	vgui::VPANEL p = GESS_FocusWindow();
	return p ? vgui::ipanel()->GetName( p ) : "";
}

// A GameUI dialog (Options, Create Server, the server browser, the console, the
// quit confirmation...) is in front of the main menu: it has the keyboard focus.
// (Some, like the quit confirmation, have no name.)
static bool GESS_IsGameUIDialogOpen()
{
	if ( vgui::input()->GetAppModalSurface() )
		return true;
	vgui::VPANEL p = GESS_FocusWindow();
	if ( !p )
		return false;
	const char *pszName = vgui::ipanel()->GetName( p );
	return Q_stricmp( pszName, "GameMenu" ) && Q_stricmp( pszName, "GESSPlayerList" ) &&
		Q_stricmp( pszName, "GESSPadHint" ) && Q_stricmp( pszName, "GESplitScreenMenu" );
}

// Where player 1's controller buttons go: the main / pause menu (a vgui Menu,
// also a combo box's drop-down list) or a dialog
enum { GESS_NAV_NONE, GESS_NAV_MENU, GESS_NAV_DIALOG };
static int GESS_MenuNavTarget()
{
	vgui::VPANEL p = GESS_FocusWindow();
	if ( !p )
		return GESS_NAV_NONE;
	const char *pszClass = vgui::ipanel()->GetClassName( p );
	if ( !Q_stricmp( vgui::ipanel()->GetName( p ), "GameMenu" ) || !Q_stricmp( pszClass, "CGameMenu" ) ||
		!Q_stricmp( pszClass, "Menu" ) )
		return GESS_NAV_MENU;
	return GESS_NAV_DIALOG;
}

bool CGESplitScreenClient::ShouldHideMenuMouse() const
{
	if ( !ges_ss_menu_hide_mouse.GetBool() || !HasAnyPadConnected() || !engine->IsActiveApp() )
		return false;
	if ( GESS_IsSplitScreenMenuOpen() )
		return true;
	return enginevgui->IsGameUIVisible() && !GESS_IsGameUIDialogOpen();
}

//-----------------------------------------------------------------------------
// One controller press = one action. vgui handles a key twice: menus move and
// choose on the "typed" event, while its controller navigation (and buttons)
// act on the "pressed" event. Sending both made one D-pad press move two
// highlights on the main menu, and one A press choose two items (Split Screen
// also opened Create Server). So:
//  - main / pause menu, drop-down lists: "typed" only (B returns to the game from
//    the pause menu, and does nothing on the main menu)
//  - dialogs: the D-pad moves between controls (Tab / Shift+Tab), A presses the
//    focused button (it clicks on release), B cancels a confirmation box (and
//    closes other dialogs as before)
//-----------------------------------------------------------------------------
void CGESplitScreenClient::SendMenuKey( int code, bool bDown )
{
	if ( !g_InputInternal )
		return;
	static bool s_bPressSent[2] = { false, false };	// A, B: a dialog got the press, send it the release
	const vgui::KeyCode key = (vgui::KeyCode) code;
	const int ab = code == KEY_XBUTTON_A ? 0 : ( code == KEY_XBUTTON_B ? 1 : -1 );

	if ( !bDown )
	{
		g_InputInternal->SetKeyCodeState( key, false );
		if ( ab >= 0 && s_bPressSent[ab] )
		{
			s_bPressSent[ab] = false;
			g_InputInternal->InternalKeyCodeReleased( key );
		}
		return;
	}

	const int target = GESS_MenuNavTarget();
	if ( target == GESS_NAV_NONE )
		return;

	if ( target == GESS_NAV_MENU )
	{
		if ( ab == 1 )
		{
			// B: back to the game from the pause menu
			if ( engine->IsInGame() && !engine->IsLevelMainMenuBackground() )
				engine->ClientCmd_Unrestricted( "gameui_hide" );
			return;
		}
		g_InputInternal->SetKeyCodeState( key, true );
		g_InputInternal->InternalKeyCodeTyped( key );
		return;
	}

	if ( ab < 0 )
	{
		const bool bBack = code == KEY_XBUTTON_UP || code == KEY_XBUTTON_LEFT;
		if ( bBack )
			g_InputInternal->SetKeyCodeState( KEY_LSHIFT, true );
		g_InputInternal->SetKeyCodeState( KEY_TAB, true );
		g_InputInternal->InternalKeyCodeTyped( KEY_TAB );
		g_InputInternal->SetKeyCodeState( KEY_TAB, false );
		if ( bBack )
			g_InputInternal->SetKeyCodeState( KEY_LSHIFT, false );
		return;
	}

	const bool bConfirmBox = vgui::input()->GetAppModalSurface() != 0;
	g_InputInternal->SetKeyCodeState( key, true );
	g_InputInternal->InternalKeyCodePressed( key );
	s_bPressSent[ab] = true;
	if ( ab == 1 && !bConfirmBox )
		g_InputInternal->InternalKeyCodeTyped( key );
}

static ConVar ges_ss_debug_menunav( "ges_ss_debug_menunav", "0", FCVAR_DONTRECORD, "Testing: log the menu keys player 1's controller sends." );

void CGESplitScreenClient::UpdateMenuPadNav( float dt )
{
	// The mouse: hidden and parked in a corner where nothing can be clicked
	m_bMenuMouseHidden = ShouldHideMenuMouse();
	if ( m_bMenuMouseHidden )
	{
		int w, h;
		vgui::surface()->GetScreenSize( w, h );
		int cx, cy;
		vgui::input()->GetCursorPos( cx, cy );
		if ( cx != w - 1 || cy != h - 1 )
		{
			vgui::input()->SetCursorPos( w - 1, h - 1 );
			if ( g_InputInternal )
				g_InputInternal->InternalCursorMoved( w - 1, h - 1 );
		}
		vgui::surface()->SetCursor( vgui::dc_none );
	}

	static const int s_DirKeys[4] = { KEY_XBUTTON_UP, KEY_XBUTTON_DOWN, KEY_XBUTTON_LEFT, KEY_XBUTTON_RIGHT };
	const int padIndex = GetSlotPad( 0 );
	const GESSPadState_t &pad = GESSGamepads()->GetPad( padIndex );
	const bool bActive = ges_ss_menu_pad_nav.GetBool() && g_InputInternal && enginevgui->IsGameUIVisible() &&
		!GESS_IsSplitScreenMenuOpen() && padIndex >= 0 && pad.connected;

	if ( !bActive || padIndex != m_iMenuNavPad )
	{
		// Let go of anything held. A controller that just became player 1 starts
		// next frame: the press that claimed it does nothing else.
		if ( m_iMenuNavDir >= 0 )
			SendMenuKey( s_DirKeys[m_iMenuNavDir], false );
		if ( m_bMenuNavADown )
			SendMenuKey( KEY_XBUTTON_A, false );
		if ( m_bMenuNavBDown )
			SendMenuKey( KEY_XBUTTON_B, false );
		m_iMenuNavDir = -1;
		m_bMenuNavADown = m_bMenuNavBDown = false;
		m_iMenuNavPad = bActive ? padIndex : -1;
		return;
	}

	// Direction: D-pad, else the left stick
	int dir = -1;
	if ( pad.IsDown( GESS_PAD_DPAD_UP ) )			dir = 0;
	else if ( pad.IsDown( GESS_PAD_DPAD_DOWN ) )	dir = 1;
	else if ( pad.IsDown( GESS_PAD_DPAD_LEFT ) )	dir = 2;
	else if ( pad.IsDown( GESS_PAD_DPAD_RIGHT ) )	dir = 3;
	else if ( fabsf( pad.ly ) >= 0.6f && fabsf( pad.ly ) >= fabsf( pad.lx ) )	dir = pad.ly > 0 ? 0 : 1;
	else if ( fabsf( pad.lx ) >= 0.6f )			dir = pad.lx < 0 ? 2 : 3;

	const float now = gpGlobals->realtime;
	if ( dir != m_iMenuNavDir )
	{
		if ( m_iMenuNavDir >= 0 )
			SendMenuKey( s_DirKeys[m_iMenuNavDir], false );
		if ( dir >= 0 )
		{
			SendMenuKey( s_DirKeys[dir], true );
			m_flMenuNavRepeat = now + 0.45f;
		}
		m_iMenuNavDir = dir;
	}
	else if ( dir >= 0 && now >= m_flMenuNavRepeat )
	{
		SendMenuKey( s_DirKeys[dir], false );
		SendMenuKey( s_DirKeys[dir], true );
		m_flMenuNavRepeat = now + 0.12f;
	}

	// A / Cross chooses, B / Circle goes back
	const bool bA = pad.IsDown( GESS_PAD_A ), bB = pad.IsDown( GESS_PAD_B );
	if ( bA != m_bMenuNavADown )
	{
		SendMenuKey( KEY_XBUTTON_A, bA );
		m_bMenuNavADown = bA;
	}
	if ( bB != m_bMenuNavBDown )
	{
		SendMenuKey( KEY_XBUTTON_B, bB );
		m_bMenuNavBDown = bB;
	}

	if ( ges_ss_debug_menunav.GetBool() && ( pad.pressed || pad.released ) )
		Msg( "menu nav: controller %d buttons %04x dir %d A %d B %d\n", padIndex + 1, pad.buttons, dir, bA, bB );
}

void CGESplitScreenClient::DebugMenuButton( const char *pszButton )
{
	static const struct { const char *name; int key; } s_Buttons[] =
	{
		{ "up", KEY_XBUTTON_UP }, { "down", KEY_XBUTTON_DOWN }, { "left", KEY_XBUTTON_LEFT },
		{ "right", KEY_XBUTTON_RIGHT }, { "a", KEY_XBUTTON_A }, { "b", KEY_XBUTTON_B },
	};
	for ( int i = 0; i < ARRAYSIZE( s_Buttons ); i++ )
	{
		if ( Q_stricmp( pszButton, s_Buttons[i].name ) )
			continue;
		SendMenuKey( s_Buttons[i].key, true );
		SendMenuKey( s_Buttons[i].key, false );
		return;
	}

	// "click <x> <y>": a left click there, the way vgui gets one from the mouse
	int cx, cy;
	if ( sscanf( pszButton, "click %d %d", &cx, &cy ) == 2 && g_InputInternal )
	{
		vgui::input()->SetCursorPos( cx, cy );
		g_InputInternal->InternalCursorMoved( cx, cy );
		g_InputInternal->SetMouseCodeState( MOUSE_LEFT, vgui::BUTTON_PRESSED );
		g_InputInternal->InternalMousePressed( MOUSE_LEFT );
		g_InputInternal->SetMouseCodeState( MOUSE_LEFT, vgui::BUTTON_RELEASED );
		g_InputInternal->InternalMouseReleased( MOUSE_LEFT );
		vgui::VPANEL over = vgui::input()->GetMouseOver();
		Msg( "clicked %d %d on %s\n", cx, cy, over ? vgui::ipanel()->GetName( over ) : "nothing" );
		return;
	}

	// "gameui:<command>": send GameUI's panels a command (for example gameui:Quit opens the
	// quit confirmation), to test its dialogs
	if ( !Q_strnicmp( pszButton, "gameui:", 7 ) )
	{
		vgui::VPANEL root = enginevgui->GetPanel( PANEL_GAMEUIDLL );
		for ( int i = 0; root && i < vgui::ipanel()->GetChildCount( root ); i++ )
		{
			vgui::VPANEL child = vgui::ipanel()->GetChild( root, i );
			Msg( "GameUI child %d: %s (%s)\n", i, vgui::ipanel()->GetName( child ), vgui::ipanel()->GetClassName( child ) );
			if ( !Q_stricmp( vgui::ipanel()->GetName( child ), "BaseGameUIPanel" ) )
				vgui::ipanel()->SendMessage( child, new KeyValues( "Command", "command", pszButton + 7 ), NULL );
		}
		return;
	}

	// List what's open, to see what counts as a dialog
	Msg( "GameUI visible %d, focus in \"%s\", dialog open %d, mouse hidden %d, Split Screen menu %d, menu controller %d\n",
		enginevgui->IsGameUIVisible(), GESS_FocusWindowName(), GESS_IsGameUIDialogOpen(), m_bMenuMouseHidden,
		GESS_IsSplitScreenMenuOpen(), m_iMenuNavPad + 1 );
	vgui::VPANEL modal = vgui::input()->GetAppModalSurface();
	Msg( "  app modal surface: %s (%s)\n", modal ? vgui::ipanel()->GetName( modal ) : "none", modal ? vgui::ipanel()->GetClassName( modal ) : "" );
	if ( modal )
	{
		for ( int i = 0; i < vgui::ipanel()->GetChildCount( modal ); i++ )
		{
			vgui::VPANEL c = vgui::ipanel()->GetChild( modal, i );
			int x, y, w, h;
			vgui::ipanel()->GetAbsPos( c, x, y );
			vgui::ipanel()->GetSize( c, w, h );
			Msg( "    child %s (%s) %d,%d %dx%d%s%s%s\n", vgui::ipanel()->GetName( c ), vgui::ipanel()->GetClassName( c ), x, y, w, h,
				vgui::ipanel()->IsVisible( c ) ? "" : " hidden", vgui::ipanel()->IsMouseInputEnabled( c ) ? "" : " no-mouse",
				vgui::ipanel()->IsEnabled( c ) ? "" : " disabled" );
		}
	}
	vgui::VPANEL focus = vgui::input()->GetFocus();
	Msg( "  key focus: %s (%s)\n", focus ? vgui::ipanel()->GetName( focus ) : "none", focus ? vgui::ipanel()->GetClassName( focus ) : "" );
	for ( vgui::VPANEL p = vgui::input()->GetMouseOver(); p; p = vgui::ipanel()->GetParent( p ) )
		Msg( "  mouse over: %s (%s)%s%s\n", vgui::ipanel()->GetName( p ), vgui::ipanel()->GetClassName( p ),
			vgui::ipanel()->IsPopup( p ) ? " popup" : "", vgui::ipanel()->IsMouseInputEnabled( p ) ? "" : " no-mouse" );
	for ( int i = 0; i < vgui::surface()->GetPopupCount(); i++ )
	{
		vgui::VPANEL p = vgui::surface()->GetPopup( i );
		if ( vgui::ipanel()->IsVisible( p ) )
		{
			int x, y, w, h;
			vgui::ipanel()->GetAbsPos( p, x, y );
			vgui::ipanel()->GetSize( p, w, h );
			Msg( "  popup %d: %s (%s) %d,%d %dx%d%s%s\n", i, vgui::ipanel()->GetName( p ), vgui::ipanel()->GetClassName( p ),
				x, y, w, h, vgui::ipanel()->IsMouseInputEnabled( p ) ? "" : " no-mouse",
				vgui::ipanel()->IsKeyBoardInputEnabled( p ) ? "" : " no-keys" );
		}
	}
}

CON_COMMAND_F( ges_ss_debug_menubutton, "Testing: ges_ss_debug_menubutton up|down|left|right|a|b sends the game's menus a controller button; no argument lists the open popups", FCVAR_DONTRECORD )
{
	GESplitScreenClient()->DebugMenuButton( args.ArgC() > 1 ? args.ArgS() : "" );
}

void CGESplitScreenClient::UpdatePadMouse( float dt )
{
	// Player 1's controller moves the cursor
	const GESSPadState_t &pad = GESSGamepads()->GetPad( GetSlotPad( 0 ) );
	// (not where the mouse is hidden: the menus there are driven like a console's)
	// (nor while player 1's START menu has the cursor: the controller drives that menu itself)
	const bool bCursor = ges_ss_padmouse.GetBool() && pad.connected && vgui::surface()->IsCursorVisible() && !m_bMenuMouseHidden &&
		!WantsMenuMouse();

	if ( !bCursor )
	{
		if ( m_bPadMouseDown && g_InputInternal )
		{
			g_InputInternal->SetMouseCodeState( MOUSE_LEFT, vgui::BUTTON_RELEASED );
			g_InputInternal->InternalMouseReleased( MOUSE_LEFT );
		}
		m_bPadMouseDown = false;
		m_bCursorWasVisible = false;
		return;
	}

	int w, h;
	vgui::surface()->GetScreenSize( w, h );

	int cx, cy;
	vgui::input()->GetCursorPos( cx, cy );
	if ( !m_bCursorWasVisible )
	{
		m_flCursorX = cx;
		m_flCursorY = cy;
		m_bCursorWasVisible = true;
	}

	// A stick moves the cursor, with a curve for fine control: the right one where
	// the left stick moves through the game's menus (UpdateMenuPadNav), else either
	const bool bLeftNavigates = m_iMenuNavPad >= 0;
	float sx = clamp( ( bLeftNavigates ? 0.0f : pad.lx ) + pad.rx, -1.0f, 1.0f );
	float sy = clamp( ( bLeftNavigates ? 0.0f : pad.ly ) + pad.ry, -1.0f, 1.0f );
	sx *= fabsf( sx );
	sy *= fabsf( sy );

	if ( sx != 0.0f || sy != 0.0f )
	{
		const float speed = ges_ss_padmouse_speed.GetFloat() * ( h / 1080.0f );
		m_flCursorX = clamp( m_flCursorX + sx * speed * dt, 0.0f, (float)( w - 1 ) );
		m_flCursorY = clamp( m_flCursorY - sy * speed * dt, 0.0f, (float)( h - 1 ) );

		const int nx = (int) m_flCursorX, ny = (int) m_flCursorY;
		if ( nx != cx || ny != cy )
		{
			vgui::input()->SetCursorPos( nx, ny );
			if ( g_InputInternal )
				g_InputInternal->InternalCursorMoved( nx, ny );
		}
	}
	else
	{
		// Follow the real mouse when the stick is idle
		m_flCursorX = cx;
		m_flCursorY = cy;
	}

	if ( !g_InputInternal )
		return;

	// The trigger is the left mouse button
	if ( pad.WasPressed( GESS_PAD_RT ) && !m_bPadMouseDown )
	{
		m_bPadMouseDown = true;
		g_InputInternal->SetMouseCodeState( MOUSE_LEFT, vgui::BUTTON_PRESSED );
		g_InputInternal->InternalMousePressed( MOUSE_LEFT );
	}
	else if ( !pad.IsDown( GESS_PAD_RT ) && m_bPadMouseDown )
	{
		m_bPadMouseDown = false;
		g_InputInternal->SetMouseCodeState( MOUSE_LEFT, vgui::BUTTON_RELEASED );
		g_InputInternal->InternalMouseReleased( MOUSE_LEFT );
	}
}

void CGESplitScreenClient::UpdateSlot( int slot, float dt )
{
	LocalSlot_t &s = m_Slots[slot];
	const GESSPadState_t &pad = GESSGamepads()->GetPad( GetSlotPad( slot ) );
	const bool bInGame = IsInGame();

	// ---- Not in a game: guests can get ready from the menus ("lobby") ----
	if ( !bInGame )
	{
		// Profiles are chosen here, in the menus: LB / RB rotate through the
		// profiles (and Guest) nobody else is using. Player 1 is always in; the
		// others press START to join (and again to leave).
		const int dir = pad.connected ? ( pad.WasPressed( GESS_PAD_RB ) ? 1 : ( pad.WasPressed( GESS_PAD_LB ) ? -1 : 0 ) ) : 0;

		if ( slot == 0 )
		{
			// Player 1 isn't assumed to be the Steam account: they get a profile
			// like everyone else (the first free one)
			if ( s.profile < 0 && !s.profileChosen )
				s.profile = NextFreeProfile( slot, -1 );
			if ( dir )
			{
				s.profile = CycleProfile( slot, s.profile, dir );
				s.profileChosen = true;
			}
			return;
		}

		if ( !pad.connected )
			return;

		// (any slot: players per display follows the players who joined here, so it
		// can't also decide who may join, or only player 1 could ever be in)
		if ( pad.WasPressed( GESS_PAD_START ) && ( slot < GES_SS_MAX_PLAYERS || s.state != GESS_SLOT_EMPTY ) )
		{
			if ( s.state == GESS_SLOT_EMPTY )
			{
				// Ready up with the first profile nobody else is using
				s.profile = NextFreeProfile( slot, -1 );
				s.state = GESS_SLOT_JOINING;
				s.joinSentTime = -1000.0f;
			}
			else
			{
				LeaveSlot( slot );
			}
		}
		else if ( dir && s.state != GESS_SLOT_EMPTY )
		{
			s.profile = CycleProfile( slot, s.profile, dir );
		}
		return;
	}

	// ---- In game ----
	if ( slot > 0 && s.state != GESS_SLOT_EMPTY && !pad.connected && !s.debugNoPad )
	{
		// Controller unplugged while choosing: cancel. While playing: keep the player, idle.
		if ( s.state == GESS_SLOT_CHOOSING )
		{
			s.state = GESS_SLOT_EMPTY;
			s.menu = GESS_MENU_NONE;
		}
		return;
	}

	// Player 1 picks a profile (then a team) in their viewport like everyone else
	// in a split screen game, from the moment the map loads (GoldenEye's own
	// character menu never opens). Once they have a profile (picked here, or
	// with START in the menus), or without a controller, it's used as it is:
	// their character is sent right away so they spawn as it.
	if ( slot == 0 && !m_bPregame && ShouldSuppressGameMenus() && !m_bHostAppliedThisLevel && s.menu == GESS_MENU_NONE && !enginevgui->IsGameUIVisible() )
	{
		// Never a spectator, and no picker: play straight away with the profile chosen
		// in the Split Screen menu (or the first free one) and the team from
		// the profile menu (auto assign if none).
		if ( s.profile < 0 && !s.profileChosen && !m_bHostChosen )
			s.profile = NextFreeProfile( slot, -1 );
		ApplyHostChoice();
	}
	else if ( slot == 0 && !pad.connected && ( s.menu == GESS_MENU_PROFILE || s.menu == GESS_MENU_TEAM ) && ShouldSuppressGameMenus() )
	{
		// Controller unplugged while picking: play with the profile highlighted
		if ( s.menu == GESS_MENU_PROFILE && !IsSlotProfileInUse( GetMenuProfileIndex( s.menuSel ), slot ) )
			s.profile = GetMenuProfileIndex( s.menuSel );
		s.menu = GESS_MENU_NONE;
		ApplyHostChoice();
	}

	if ( s.state == GESS_SLOT_JOINING )
	{
		C_BasePlayer *pPlayer = GetSlotPlayer( slot );
		if ( pPlayer )
		{
			s.state = GESS_SLOT_JOINED;
		}
		else if ( gpGlobals->realtime - s.joinSentTime > ges_ss_rejoin_timeout.GetFloat() || gpGlobals->realtime < s.joinSentTime )
		{
			SendJoin( slot );
		}
	}
	else if ( s.state == GESS_SLOT_JOINED && slot > 0 && !GetSlotPlayer( slot ) )
	{
		// Lost our player (map change, kicked, server restarted): ask again
		s.state = GESS_SLOT_JOINING;
		s.joinSentTime = gpGlobals->realtime;
	}

	// Server forced our angles (spawn / teleport)
	if ( slot > 0 )
	{
		C_GEMPPlayer *pGuest = ToGEMPPlayer( GetSlotPlayer( slot ) );
		if ( pGuest && pGuest->GetSplitAngleReset() != s.angleResetSeen )
		{
			s.angles = pGuest->GetSplitResetAngles();
			if ( !s.angles.IsValid() )
				s.angles.Init();	// never look through a NaN / infinite view
			s.angles[ROLL] = 0;
			s.angleResetSeen = pGuest->GetSplitAngleReset();
		}
	}

	// Before the match: taking screens (UpdatePregame)
	if ( m_bPregame || !pad.connected )
		return;

	// The pause menu / scoreboard of the main game takes the controllers
	if ( enginevgui->IsGameUIVisible() )
		return;

	if ( s.menu != GESS_MENU_NONE )
	{
		UpdateMenu( slot, pad, dt );
		return;
	}

	if ( pad.WasPressed( GESS_PAD_START ) )
	{
		if ( slot > 0 && s.state == GESS_SLOT_EMPTY && slot < GetMaxSlots() )
		{
			// Join straight away with the next free profile (no picker)
			s.profile = NextFreeProfile( slot, -1 );
			s.state = GESS_SLOT_JOINING;
			SendJoin( slot );
		}
		else if ( slot == 0 || s.state == GESS_SLOT_JOINED )
		{
			OpenGameMenu( slot );
		}
		return;
	}

	if ( slot == 0 || s.state == GESS_SLOT_JOINED )
	{
		// Latched until the next usercmd so it happens exactly once
		if ( pad.WasPressed( GESS_PAD_Y ) )
			s.pendingCycle = 1;
		else if ( pad.WasPressed( GESS_PAD_LB ) )
			s.pendingCycle = -1;

		if ( slot > 0 )
			UpdateGuestLook( slot, pad, dt );
	}
}

bool CGESplitScreenClient::IsGameMenuPageAvailable( int page ) const
{
	if ( page == GESS_GAMEPAGE_TEAM )
		return IsTeamplay();
	return page >= 0 && page < GESS_GAMEPAGE_COUNT;
}

//-----------------------------------------------------------------------------
// No game pause menu in a split screen match
//
// The game's own pause menu (Resume Game, Disconnect, Find Servers...) covers
// every player's view and isn't made for controllers. It can open from Esc, from
// a controller button the engine itself reads (an Xbox pad's START / BACK, or any
// pad Steam Input presents as one), or the window losing and regaining focus.
// While a split screen match is on, it's closed as soon as it appears and player
// 1's own START menu opens instead (leaderboard, team, exit with a confirmation:
// player 1 exiting ends the match). The console (~) still opens.
// Back at the main menu everything works as usual. Not when player 1 has no
// controller (the START menu needs one), so there's always a way out.
//-----------------------------------------------------------------------------
static ConVar ges_ss_block_pausemenu( "ges_ss_block_pausemenu", "1", FCVAR_ARCHIVE, "In a split screen match, close the game's pause menu when it opens and open player 1's START menu instead." );

// A controller's START pressed this recently opened the pause menu
#define GESS_PAUSEMENU_START_WINDOW 0.5

void CGESplitScreenClient::BlockPauseMenu()
{
	if ( !ges_ss_block_pausemenu.GetBool() || !enginevgui->IsGameUIVisible() || !ShouldSuppressGameMenus() )
		return;
	if ( !engine->IsInGame() || engine->IsLevelMainMenuBackground() || engine->IsDrawingLoadingImage() || engine->Con_IsVisible() )
		return;

	// Was it a controller's START? (The engine reads Xbox pads itself, and its
	// START opens the pause menu, even for a guest's pad or when player 1 plays
	// with keyboard and mouse.) Then it's always closed, and the player whose
	// START it was gets their own START menu.
	const double now = Plat_FloatTime();
	int startPad = -1;
	for ( int pad = 0; pad < GES_SS_MAX_PADS; pad++ )
	{
		if ( now - m_flPadStartTime[pad] <= GESS_PAUSEMENU_START_WINDOW &&
			( startPad < 0 || m_flPadStartTime[pad] > m_flPadStartTime[startPad] ) )
			startPad = pad;
	}

	if ( startPad >= 0 )
	{
		engine->ClientCmd_Unrestricted( "gameui_hide\n" );
		// Our own START handling already saw this press (opened or closed that
		// player's START menu) unless the pause menu was up first
		if ( m_bPadStartUnseen[startPad] && !m_bPregame )
		{
			m_bPadStartUnseen[startPad] = false;
			const int slot = GetPadSlot( startPad );
			if ( slot >= 0 && ( slot == 0 || m_Slots[slot].state == GESS_SLOT_JOINED ) && m_Slots[slot].menu == GESS_MENU_NONE )
				OpenGameMenu( slot );
			GESSGamepads()->ConsumeEdges( startPad );
		}
		DevMsg( "Split screen: closed the game's pause menu (opened by controller %d's START)\n", startPad + 1 );
		return;
	}

	// Esc, or the window regaining focus: only when player 1 has a controller
	// (the START menu needs one), so a keyboard player 1 always has a way out
	const int hostPad = GetSlotPad( 0 );
	if ( hostPad < 0 || !GESSGamepads()->GetPad( hostPad ).connected )
		return;

	engine->ClientCmd_Unrestricted( "gameui_hide\n" );
	if ( m_Slots[0].menu == GESS_MENU_NONE && !m_bPregame )
		OpenGameMenu( 0 );
	DevMsg( "Split screen: closed the game's pause menu (player 1's START menu instead)\n" );
}

// The engine calls this (through the client's HandleUiToggle) when Esc, or an
// Xbox pad's START, would open or close its pause menu. In a split screen game
// a controller's START never does: each player's START opens their own menu.
bool CGESplitScreenClient::ShouldSwallowUiToggle() const
{
	if ( !ges_ss_block_pausemenu.GetBool() || !ShouldBlockEnginePadButtons() || !ShouldSuppressGameMenus() )
		return false;
	if ( engine->IsLevelMainMenuBackground() || engine->Con_IsVisible() )
		return false;
	if ( inputsystem && inputsystem->IsButtonDown( KEY_ESCAPE ) )
		return false;	// Esc always works

	if ( inputsystem && inputsystem->IsButtonDown( KEY_XBUTTON_START ) )
		return true;
	const double now = Plat_FloatTime();
	for ( int pad = 0; pad < GES_SS_MAX_PADS; pad++ )
	{
		if ( GESSGamepads()->GetPad( pad ).IsDown( GESS_PAD_START ) || now - m_flPadStartTime[pad] <= 0.25 )
			return true;
	}
	return false;
}

void CGESplitScreenClient::OpenGameMenu( int slot )
{
	LocalSlot_t &s = m_Slots[slot];
	s.menu = GESS_MENU_PAUSE;
	s.menuSel = GESS_GAMEPAGE_LEADERBOARD;
	s.menuSub = GESS_EXITBTN_EXIT;
	s.menuConfirm = GESS_CONFIRM_NONE;
	s.menuConfirmSel = 0;
	s.menuRepeatTime = 0;
	s.ctrlRow = GESS_CTRLROW_PAGE;
	s.calOpen = false;
}

void CGESplitScreenClient::CycleGameMenuPage( int slot, int dir )
{
	LocalSlot_t &s = m_Slots[slot];
	int page = s.menuSel;
	for ( int i = 0; i < GESS_GAMEPAGE_COUNT; i++ )
	{
		page = ( page + dir + GESS_GAMEPAGE_COUNT ) % GESS_GAMEPAGE_COUNT;
		if ( IsGameMenuPageAvailable( page ) )
			break;
	}
	s.menuSel = page;
	s.menuSub = GESS_EXITBTN_EXIT;
	s.menuConfirm = GESS_CONFIRM_NONE;
	s.ctrlRow = GESS_CTRLROW_PAGE;
}

// The suicide button's label: the localization string #GES_SS_Suicide
// (resource/ges_splitscreen_english.txt), this text if that file isn't there
const char *GESS_SuicideLabel()
{
	static char s_szLabel[64];
	const wchar_t *pwsz = g_pVGuiLocalize ? g_pVGuiLocalize->Find( "#GES_SS_Suicide" ) : NULL;
	if ( pwsz && pwsz[0] )
		g_pVGuiLocalize->ConvertUnicodeToANSI( pwsz, s_szLabel, sizeof( s_szLabel ) );
	else
		Q_strncpy( s_szLabel, "Commit Suicide and Respawn", sizeof( s_szLabel ) );
	return s_szLabel;
}

bool CGESplitScreenClient::CanSlotSuicide( int slot ) const
{
	C_BasePlayer *pPlayer = GetSlotPlayer( slot );
	return pPlayer && pPlayer->IsAlive() && !pPlayer->IsObserver();
}

// The confirmation box's answer: Cancel goes back to the page, Confirm does it
void CGESplitScreenClient::AnswerGameMenuConfirm( int slot, bool bYes )
{
	LocalSlot_t &s = m_Slots[slot];
	const int what = s.menuConfirm;
	s.menuConfirm = GESS_CONFIRM_NONE;
	if ( !bYes )
		return;

	if ( what == GESS_CONFIRM_SUICIDE )
	{
		// Only this player dies, through the server's normal suicide (the online
		// game's "kill"): the game mode scores it (DeathMatch: 1 point off)
		s.menu = GESS_MENU_NONE;
		engine->ServerCmd( VarArgs( "%s %d\n", GES_SS_CMD_SUICIDE, slot ) );
		return;
	}

	if ( what != GESS_CONFIRM_EXIT )
		return;

	s.menu = GESS_MENU_NONE;
	if ( slot > 0 )
	{
		// A guest leaves: their screen closes and the others share the space
		LeaveSlot( slot );
	}
	else
	{
		// Player 1 leaves: the match ends for everyone and shows the results,
		// then back to the Split Screen menu (before the next map loads)
		engine->ServerCmd( "ges_ss_endmatch\n" );
		static ConVarRef mp_chattime( "mp_chattime" );
		const float results = mp_chattime.IsValid() ? mp_chattime.GetFloat() - 1.0f : 9.0f;
		m_flExitToMenuTime = gpGlobals->realtime + clamp( results, 6.0f, 20.0f );
	}
}

// The page's action (A or the right trigger)
void CGESplitScreenClient::ActivateGameMenuPage( int slot )
{
	LocalSlot_t &s = m_Slots[slot];
	if ( s.menuConfirm != GESS_CONFIRM_NONE )
	{
		AnswerGameMenuConfirm( slot, s.menuConfirmSel == 1 );
		return;
	}

	switch ( s.menuSel )
	{
	case GESS_GAMEPAGE_TEAM:
		if ( IsTeamplay() )
		{
			// Swap to the other team: respawns at once, no suicide or lost point
			C_BasePlayer *pPlayer = GetSlotPlayer( slot );
			const int current = pPlayer ? pPlayer->GetTeamNumber() : s.team;
			s.team = ( current == TEAM_MI6 ) ? TEAM_JANUS : TEAM_MI6;
			s.menu = GESS_MENU_NONE;
			if ( slot == 0 )
				ApplyHostChoice();
			else
				engine->ServerCmd( VarArgs( "%s %d %d\n", GES_SS_CMD_TEAM, slot, s.team ) );
		}
		break;

	case GESS_GAMEPAGE_EXIT:
		// Each button asks first (starting on Cancel). Dead players can't commit suicide.
		if ( s.menuSub == GESS_EXITBTN_SUICIDE && !CanSlotSuicide( slot ) )
			break;
		s.menuConfirm = s.menuSub == GESS_EXITBTN_SUICIDE ? GESS_CONFIRM_SUICIDE : GESS_CONFIRM_EXIT;
		s.menuConfirmSel = 0;
		break;

	default:
		break;
	}
}

// START in a match: the leaderboard, with more pages to the left and right
// (swap team, exit / commit suicide). START or B closes it. The game carries on.
void CGESplitScreenClient::UpdateGameMenu( int slot, const GESSPadState_t &pad, float dt )
{
	LocalSlot_t &s = m_Slots[slot];
	if ( !IsGameMenuPageAvailable( s.menuSel ) )
		CycleGameMenuPage( slot, 1 );

	int vdir = 0;
	if ( pad.WasPressed( GESS_PAD_DPAD_UP ) )
		vdir = -1;
	else if ( pad.WasPressed( GESS_PAD_DPAD_DOWN ) )
		vdir = 1;
	const bool bStickUpDown = fabsf( pad.ly ) > 0.6f && fabsf( pad.lx ) < 0.3f;

	// A confirmation box: up / down between Cancel and Confirm, A answers, B cancels
	if ( s.menuConfirm != GESS_CONFIRM_NONE )
	{
		if ( vdir )
			s.menuConfirmSel = 1 - s.menuConfirmSel;
		else if ( bStickUpDown )
			s.menuConfirmSel = pad.ly > 0 ? 0 : 1;	// up: Cancel, down: Confirm

		if ( pad.WasPressed( GESS_PAD_A ) || pad.WasPressed( GESS_PAD_RT ) )
			AnswerGameMenuConfirm( slot, s.menuConfirmSel == 1 );
		else if ( pad.WasPressed( GESS_PAD_B ) )
			AnswerGameMenuConfirm( slot, false );
		else if ( pad.WasPressed( GESS_PAD_START ) )
		{
			s.menuConfirm = GESS_CONFIRM_NONE;
			s.menu = GESS_MENU_NONE;
		}

		GESSGamepads()->ConsumeEdges( GetSlotPad( slot ) );
		return;
	}

	// The Controller page's settings and its Calibrate Sticks screen
	if ( UpdateControllerPage( slot, pad, vdir, dt ) )
	{
		GESSGamepads()->ConsumeEdges( GetSlotPad( slot ) );
		return;
	}

	// Left / right: next page (d-pad or left stick, with repeat)
	int dir = 0;
	if ( pad.WasPressed( GESS_PAD_DPAD_LEFT ) )
		dir = -1;
	else if ( pad.WasPressed( GESS_PAD_DPAD_RIGHT ) )
		dir = 1;
	else if ( fabsf( pad.lx ) > 0.5f )
	{
		s.menuRepeatTime -= dt;
		if ( s.menuRepeatTime <= 0 )
		{
			dir = pad.lx > 0 ? 1 : -1;
			s.menuRepeatTime = GESS_MENU_REPEAT * 1.5f;
		}
	}
	else
	{
		s.menuRepeatTime = 0;
	}
	if ( dir )
		CycleGameMenuPage( slot, dir );

	// Exit page: up / down between Exit game and Commit Suicide under it
	if ( s.menuSel == GESS_GAMEPAGE_EXIT )
	{
		if ( vdir )
			s.menuSub = ( s.menuSub + vdir + GESS_EXITBTN_COUNT ) % GESS_EXITBTN_COUNT;
		else if ( bStickUpDown )
			s.menuSub = pad.ly > 0 ? GESS_EXITBTN_EXIT : GESS_EXITBTN_SUICIDE;
	}

	if ( pad.WasPressed( GESS_PAD_A ) || pad.WasPressed( GESS_PAD_RT ) )
		ActivateGameMenuPage( slot );
	else if ( pad.WasPressed( GESS_PAD_B ) || pad.WasPressed( GESS_PAD_START ) )
		s.menu = GESS_MENU_NONE;

	GESSGamepads()->ConsumeEdges( GetSlotPad( slot ) );
}

//-----------------------------------------------------------------------------
// The mouse in player 1's START menu
//
// There is one mouse and it's player 1's, so while player 1's START menu is open
// in a match a cursor shows and their menu can be clicked: < and > change page,
// the buttons choose, a confirmation box's Cancel / Confirm answer it, the right
// button goes back. Clicks don't also fire. The HUD records where it drew what.
//-----------------------------------------------------------------------------
static ConVar ges_ss_menu_mouse( "ges_ss_menu_mouse", "1", FCVAR_ARCHIVE, "In a split screen match, show a cursor while player 1's START menu is open, so it can be used with the mouse." );

bool CGESplitScreenClient::WantsMenuMouse() const
{
	return ges_ss_menu_mouse.GetBool() && ges_ss_enable.GetBool() && m_Slots[0].menu == GESS_MENU_PAUSE &&
		engine->IsInGame() && !engine->IsLevelMainMenuBackground() && !enginevgui->IsGameUIVisible() && !engine->Con_IsVisible();
}

void CGESplitScreenClient::OnMenuMouse( int hit, int arg, bool bClick, float frac )
{
	LocalSlot_t &s = m_Slots[0];
	if ( s.menu != GESS_MENU_PAUSE || hit == GESS_MENUHIT_NONE )
		return;

	switch ( hit )
	{
	case GESS_MENUHIT_ROW:
	case GESS_MENUHIT_SLIDER:
		if ( s.menuConfirm != GESS_CONFIRM_NONE || s.menuSel != GESS_GAMEPAGE_CONTROLLER )
			break;
		frac = clamp( frac, 0.0f, 1.0f );
		if ( s.calOpen )
		{
			s.calRow = clamp( arg, 0, GESS_CALROW_COUNT - 1 );	// Pointing at it highlights it
			if ( !bClick )
				break;
			if ( hit == GESS_MENUHIT_SLIDER && ( arg == GESS_CALROW_LEFT || arg == GESS_CALROW_RIGHT ) )
			{
				const float lo = CGESSGamepads::GetDeadzoneMin(), hi = CGESSGamepads::GetDeadzoneMax();
				SetCalDeadzone( 0, arg == GESS_CALROW_LEFT ? 0 : 1, lo + ( hi - lo ) * frac );
			}
			else if ( arg == GESS_CALROW_REMEASURE )
				GESSGamepads()->RecalibratePad( s.calPad );
			else if ( arg == GESS_CALROW_DONE || arg == GESS_CALROW_BACK )
				CloseCalibrate( 0, arg == GESS_CALROW_DONE );
			break;
		}
		s.ctrlRow = clamp( arg, 0, GESS_CTRLROW_COUNT - 1 );
		if ( !bClick )
			break;
		if ( arg == GESS_CTRLROW_INVERT )
			ToggleSlotInvert( 0 );
		else if ( arg == GESS_CTRLROW_SENS && hit == GESS_MENUHIT_SLIDER )
			SetSlotSensStep( 0, 1 + (int)( frac * ( GESS_SENS_STEPS - 1 ) + 0.5f ) );
		else if ( arg == GESS_CTRLROW_CALIBRATE )
			OpenCalibrate( 0 );
		break;

	case GESS_MENUHIT_PREV:
	case GESS_MENUHIT_NEXT:
		if ( bClick && s.menuConfirm == GESS_CONFIRM_NONE )
			CycleGameMenuPage( 0, hit == GESS_MENUHIT_PREV ? -1 : 1 );
		break;

	case GESS_MENUHIT_BUTTON:
		if ( s.menuConfirm != GESS_CONFIRM_NONE || s.menuSel != GESS_GAMEPAGE_EXIT )
			break;
		s.menuSub = clamp( arg, 0, GESS_EXITBTN_COUNT - 1 );	// Pointing at it highlights it
		if ( bClick )
			ActivateGameMenuPage( 0 );
		break;

	case GESS_MENUHIT_CONFIRM:
		if ( s.menuConfirm == GESS_CONFIRM_NONE )
			break;
		s.menuConfirmSel = arg ? 1 : 0;
		if ( bClick )
			AnswerGameMenuConfirm( 0, arg != 0 );
		break;

	case GESS_MENUHIT_TEAM:
		if ( bClick && s.menuConfirm == GESS_CONFIRM_NONE && s.menuSel == GESS_GAMEPAGE_TEAM )
			ActivateGameMenuPage( 0 );
		break;
	}
}

void CGESplitScreenClient::OnMenuMouseBack()
{
	LocalSlot_t &s = m_Slots[0];
	if ( s.menuConfirm != GESS_CONFIRM_NONE )
		s.menuConfirm = GESS_CONFIRM_NONE;
	else if ( s.calOpen )
		CloseCalibrate( 0, false );
	else
		s.menu = GESS_MENU_NONE;
}

//-----------------------------------------------------------------------------
// The START menu's Controller page
//
// Rows: the page title (left / right change page there, as on every page), Invert
// Look Up/Down (A or left / right toggle it), Look Sensitivity (left / right, 1-10,
// 5 = the normal look speed) and Calibrate Sticks (A opens it). Invert and
// sensitivity are the player's profile's (saved with it, used at once); a Guest's
// last until they leave.
//
// Calibrate Sticks shows both sticks live with their dead zone circles. Re-measure
// (or R3) is the lobby's R3 measurement: leave the sticks alone, then turn them
// round. The dead zone sliders set them by hand (0.01 steps). Done keeps it all
// (saved per controller in cfg/ges_splitscreen_pads.txt), Back (or B) puts the
// sticks back as they were when the screen opened.
//-----------------------------------------------------------------------------
bool CGESplitScreenClient::GetSlotInvert( int slot ) const
{
	return GetSlotProfileData( slot ).invertY;
}

int CGESplitScreenClient::GetSlotSensStep( int slot ) const
{
	return GESS_StepFromSens( GetSlotProfileData( slot ).sensitivity );
}

void CGESplitScreenClient::SetSlotSensStep( int slot, int step )
{
	GESSProfile_t *p = GetSlotProfileForEdit( slot );
	const float sens = clamp( GESS_SensFromStep( step ), GES_SS_SENS_MIN, GES_SS_SENS_MAX );
	if ( p->sensitivity == sens )
		return;
	p->sensitivity = sens;
	if ( m_Slots[slot].profile >= 0 )
		GESSProfiles()->Save();
}

void CGESplitScreenClient::ToggleSlotInvert( int slot )
{
	GESSProfile_t *p = GetSlotProfileForEdit( slot );
	p->invertY = !p->invertY;
	if ( m_Slots[slot].profile >= 0 )
		GESSProfiles()->Save();
}

void CGESplitScreenClient::OpenCalibrate( int slot )
{
	LocalSlot_t &s = m_Slots[slot];
	const int pad = GetSlotPad( slot );
	if ( pad < 0 || !GESSGamepads()->GetPad( pad ).connected )
		return;
	s.calOpen = true;
	s.calRow = GESS_CALROW_REMEASURE;
	s.calPad = pad;
	for ( int st = 0; st < 2; st++ )
		s.calBackup[st] = GESSGamepads()->GetStickCal( pad, st );
}

void CGESplitScreenClient::CloseCalibrate( int slot, bool bKeep )
{
	LocalSlot_t &s = m_Slots[slot];
	if ( !s.calOpen )
		return;
	s.calOpen = false;
	s.ctrlRow = GESS_CTRLROW_CALIBRATE;
	if ( s.calPad < 0 )
		return;
	if ( !bKeep )
	{
		for ( int st = 0; st < 2; st++ )
			GESSGamepads()->RestoreStickCal( s.calPad, st, s.calBackup[st] );
	}
	GESSGamepads()->SaveCalibration();
}

void CGESplitScreenClient::SetCalDeadzone( int slot, int stick, float inner )
{
	const LocalSlot_t &s = m_Slots[slot];
	// Whole hundredths, as the slider shows them
	GESSGamepads()->SetStickDeadzone( s.calPad, stick, floorf( inner * 100.0f + 0.5f ) / 100.0f );
}

bool CGESplitScreenClient::UpdateControllerPage( int slot, const GESSPadState_t &pad, int vdir, float dt )
{
	LocalSlot_t &s = m_Slots[slot];
	if ( s.menuSel != GESS_GAMEPAGE_CONTROLLER )
	{
		s.calOpen = false;
		return false;
	}

	// The controller went away (or changed) while calibrating: keep what was done
	if ( s.calOpen && s.calPad != GetSlotPad( slot ) )
		CloseCalibrate( slot, true );

	// On the title row left / right change page as usual; down goes into the settings
	if ( !s.calOpen && s.ctrlRow == GESS_CTRLROW_PAGE )
	{
		if ( vdir > 0 || ( pad.ly < -0.6f && fabsf( pad.lx ) < 0.3f ) )
		{
			s.ctrlRow = GESS_CTRLROW_INVERT;
			s.menuRepeatTime = GESS_MENU_REPEAT;
			return true;
		}
		return false;
	}

	// Left stick up / down / left / right with repeat, as well as the d-pad
	int hdir = pad.WasPressed( GESS_PAD_DPAD_LEFT ) ? -1 : ( pad.WasPressed( GESS_PAD_DPAD_RIGHT ) ? 1 : 0 );
	if ( !vdir && !hdir )
	{
		const bool bV = fabsf( pad.ly ) > 0.5f && fabsf( pad.ly ) >= fabsf( pad.lx );
		const bool bH = fabsf( pad.lx ) > 0.5f && fabsf( pad.lx ) > fabsf( pad.ly );
		if ( bV || bH )
		{
			s.menuRepeatTime -= dt;
			if ( s.menuRepeatTime <= 0 )
			{
				if ( bV )
					vdir = pad.ly > 0 ? -1 : 1;
				else
					hdir = pad.lx > 0 ? 1 : -1;
				s.menuRepeatTime = GESS_MENU_REPEAT;
			}
		}
		else
		{
			s.menuRepeatTime = 0;
		}
	}
	const bool bA = pad.WasPressed( GESS_PAD_A ) || pad.WasPressed( GESS_PAD_RT );

	if ( s.calOpen )
	{
		if ( vdir )
			s.calRow = clamp( s.calRow + vdir, 0, GESS_CALROW_COUNT - 1 );
		if ( pad.WasPressed( GESS_PAD_RSTICK ) || ( bA && s.calRow == GESS_CALROW_REMEASURE ) )
			GESSGamepads()->RecalibratePad( s.calPad );
		else if ( hdir && ( s.calRow == GESS_CALROW_LEFT || s.calRow == GESS_CALROW_RIGHT ) )
		{
			const int st = s.calRow == GESS_CALROW_LEFT ? 0 : 1;
			SetCalDeadzone( slot, st, GESSGamepads()->GetStickCal( s.calPad, st ).inner + hdir * 0.01f );
		}
		else if ( bA && ( s.calRow == GESS_CALROW_DONE || s.calRow == GESS_CALROW_BACK ) )
			CloseCalibrate( slot, s.calRow == GESS_CALROW_DONE );
		else if ( pad.WasPressed( GESS_PAD_B ) )
			CloseCalibrate( slot, false );
		else if ( pad.WasPressed( GESS_PAD_START ) )
		{
			CloseCalibrate( slot, true );
			s.menu = GESS_MENU_NONE;
		}
		return true;
	}

	if ( vdir )
		s.ctrlRow = clamp( s.ctrlRow + vdir, (int) GESS_CTRLROW_PAGE, GESS_CTRLROW_COUNT - 1 );
	else if ( s.ctrlRow == GESS_CTRLROW_INVERT && ( hdir || bA ) )
		ToggleSlotInvert( slot );
	else if ( s.ctrlRow == GESS_CTRLROW_SENS && hdir )
		SetSlotSensStep( slot, GetSlotSensStep( slot ) + hdir );
	else if ( s.ctrlRow == GESS_CTRLROW_CALIBRATE && bA )
		OpenCalibrate( slot );
	else if ( pad.WasPressed( GESS_PAD_B ) || pad.WasPressed( GESS_PAD_START ) )
		s.menu = GESS_MENU_NONE;
	return true;
}

void CGESplitScreenClient::UpdateMenu( int slot, const GESSPadState_t &pad, float dt )
{
	LocalSlot_t &s = m_Slots[slot];

	if ( s.menu == GESS_MENU_PAUSE )
	{
		UpdateGameMenu( slot, pad, dt );
		return;
	}

	int count;
	if ( s.menu == GESS_MENU_PROFILE )
		count = GetMenuProfileCount();
	else if ( s.menu == GESS_MENU_TEAM )
		count = GESS_TEAMMENU_COUNT;
	else
		count = slot == 0 ? GESS_PAUSE_LEAVE : GESS_PAUSE_COUNT;
	if ( count <= 0 )
		return;

	// Navigation with the d-pad or left stick (with auto repeat)
	int move = 0;
	if ( pad.WasPressed( GESS_PAD_DPAD_UP ) )
		move = -1;
	else if ( pad.WasPressed( GESS_PAD_DPAD_DOWN ) )
		move = 1;
	else if ( fabsf( pad.ly ) > 0.5f )
	{
		s.menuRepeatTime -= dt;
		if ( s.menuRepeatTime <= 0 )
		{
			move = pad.ly > 0 ? -1 : 1;
			s.menuRepeatTime = GESS_MENU_REPEAT;
		}
	}
	else
	{
		s.menuRepeatTime = 0;
	}

	if ( move )
		s.menuSel = ( s.menuSel + move + count ) % count;
	s.menuSel = clamp( s.menuSel, 0, count - 1 );

	const bool bConfirm = pad.WasPressed( GESS_PAD_A );
	const bool bBack = pad.WasPressed( GESS_PAD_B ) || pad.WasPressed( GESS_PAD_START );

	if ( s.menu == GESS_MENU_PROFILE )
	{
		if ( bConfirm )
		{
			int idx = GetMenuProfileIndex( s.menuSel );
			if ( !IsSlotProfileInUse( idx, slot ) )
				ConfirmProfile( slot, s.menuSel );
		}
		else if ( bBack )
		{
			s.menu = GESS_MENU_NONE;
			if ( s.state == GESS_SLOT_CHOOSING )
				s.state = GESS_SLOT_EMPTY;	// Changed their mind
		}
	}
	else if ( s.menu == GESS_MENU_TEAM )
	{
		if ( bConfirm )
			ConfirmTeam( slot, s.menuSel );
		else if ( bBack )
			OpenProfileMenu( slot );	// back to the profile choice
	}
	else if ( s.menu == GESS_MENU_PAUSE )
	{
		if ( bConfirm )
		{
			switch ( s.menuSel )
			{
			case GESS_PAUSE_RESUME:
				s.menu = GESS_MENU_NONE;
				break;
			case GESS_PAUSE_PROFILE:
				OpenProfileMenu( slot );
				break;
			case GESS_PAUSE_LEAVE:
				s.menu = GESS_MENU_NONE;
				if ( slot > 0 )
					LeaveSlot( slot );
				break;
			}
		}
		else if ( bBack )
		{
			s.menu = GESS_MENU_NONE;
		}
	}

	GESSGamepads()->ConsumeEdges( GetSlotPad( slot ) );
}

void CGESplitScreenClient::OpenProfileMenu( int slot )
{
	LocalSlot_t &s = m_Slots[slot];
	s.menu = GESS_MENU_PROFILE;
	s.menuSel = 0;

	// Start on the current profile, or the first free one
	for ( int e = 0; e < GetMenuProfileCount(); e++ )
	{
		int idx = GetMenuProfileIndex( e );
		if ( ( s.profile >= 0 && idx == s.profile ) || ( s.profile < 0 && !IsSlotProfileInUse( idx, slot ) ) )
		{
			s.menuSel = e;
			break;
		}
	}
}

static void GESS_CleanName( char *dst, int len, const char *src )
{
	Q_strncpy( dst, src, len );
	for ( char *c = dst; *c; c++ )
	{
		if ( *c == '"' || *c == ';' || *c == '\n' || *c == '\r' )
			*c = ' ';
	}
}

void CGESplitScreenClient::ConfirmProfile( int slot, int entry )
{
	LocalSlot_t &s = m_Slots[slot];
	s.profile = GetMenuProfileIndex( entry );
	s.menu = GESS_MENU_NONE;
	if ( slot == 0 )
		s.profileChosen = true;

	// In team games everyone picks a team after their profile
	if ( IsInGame() && IsTeamplay() )
	{
		s.menu = GESS_MENU_TEAM;
		s.menuSel = ( s.team == TEAM_MI6 ) ? GESS_TEAMMENU_MI6 : ( s.team == TEAM_JANUS ? GESS_TEAMMENU_JANUS : GESS_TEAMMENU_AUTO );
		return;
	}

	FinishChoice( slot );
}

void CGESplitScreenClient::ConfirmTeam( int slot, int entry )
{
	LocalSlot_t &s = m_Slots[slot];
	switch ( entry )
	{
	case GESS_TEAMMENU_MI6:		s.team = TEAM_MI6; break;
	case GESS_TEAMMENU_JANUS:	s.team = TEAM_JANUS; break;
	default:					s.team = MAX_GE_TEAMS; break;	// auto assign
	}
	s.menu = GESS_MENU_NONE;
	FinishChoice( slot );
}

// Profile (and team) picked: join, or update the player who is already in
void CGESplitScreenClient::FinishChoice( int slot )
{
	LocalSlot_t &s = m_Slots[slot];

	if ( slot == 0 )
	{
		m_bHostChosen = true;
		if ( IsInGame() )
			ApplyHostChoice();
		return;
	}

	if ( s.state == GESS_SLOT_CHOOSING )
	{
		s.state = GESS_SLOT_JOINING;
		SendJoin( slot );
	}
	else if ( s.state == GESS_SLOT_JOINING )
	{
		s.joinSentTime = -1000.0f;	// send right away
	}
	else if ( s.state == GESS_SLOT_JOINED )
	{
		char szName[GES_SS_PROFILE_NAME_LEN];
		GESS_CleanName( szName, sizeof( szName ), GetSlotName( slot ) );
		engine->ServerCmd( VarArgs( "%s %d \"%s\" %s\n", GES_SS_CMD_SETCHAR, slot, szName, GetSlotProfileData( slot ).character ) );
		if ( IsTeamplay() && s.team )
			engine->ServerCmd( VarArgs( "%s %d %d\n", GES_SS_CMD_TEAM, slot, s.team ) );
	}
}

// Player 1's profile name, team and character
void CGESplitScreenClient::ApplyHostChoice()
{
	LocalSlot_t &s = m_Slots[0];
	m_bHostAppliedThisLevel = true;
	m_flHidePanelsUntil = gpGlobals->realtime + 3.0f;

	ApplyHostProfile();

	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	if ( !pLocal )
		return;

	const int current = pLocal->GetTeamNumber();
	int team = TEAM_UNASSIGNED;
	bool bJoin;
	if ( IsTeamplay() )
	{
		team = s.team ? s.team : MAX_GE_TEAMS;
		bJoin = ( team == MAX_GE_TEAMS ) ? ( current != TEAM_MI6 && current != TEAM_JANUS ) : ( current != team );
	}
	else
	{
		// Only if they aren't playing yet: joining again would kill them
		bJoin = current == TEAM_SPECTATOR || pLocal->IsObserver();
	}

	// Tell the server we're a split screen host with this character: it keeps the
	// team / character menus closed for us and uses the character whenever we'd
	// have to pick one again (forced team changes, round restarts)
	const GESSProfile_t &p = GetSlotProfileData( 0 );
	const char *character = p.character[0] ? p.character : CHAR_RANDOM_IDENT;
	engine->ServerCmd( VarArgs( "%s 1 %s\n", GES_SS_CMD_HOST, character ) );

	if ( bJoin )
		engine->ServerCmd( VarArgs( "jointeam %d\n", team ) );

	// The profile's character, or a random one when it doesn't fit the team
	if ( IsTeamplay() )
	{
		const CGECharData *pChar = GECustomChar_GetBodyChar( character );
		if ( !pChar || team == MAX_GE_TEAMS || pChar->iTeam != team )
			character = CHAR_RANDOM_IDENT;
	}
	engine->ServerCmd( VarArgs( "joinclass %s\n", character ) );
}

void CGESplitScreenClient::SendJoin( int slot )
{
	LocalSlot_t &s = m_Slots[slot];
	s.joinSentTime = gpGlobals->realtime;

	// ges_ss_join <slot> <name> <character> <team: 0 = auto>
	char szName[GES_SS_PROFILE_NAME_LEN];
	GESS_CleanName( szName, sizeof( szName ), GetSlotName( slot ) );
	engine->ServerCmd( VarArgs( "%s %d \"%s\" %s %d\n", GES_SS_CMD_JOIN, slot, szName, GetSlotProfileData( slot ).character, s.team ) );
}

void CGESplitScreenClient::LeaveSlot( int slot )
{
	m_Slots[slot].debugNoPad = false;
	LocalSlot_t &s = m_Slots[slot];

	if ( IsInGame() && s.state != GESS_SLOT_EMPTY )
		engine->ServerCmd( VarArgs( "%s %d\n", GES_SS_CMD_LEAVE, slot ) );

	s.state = GESS_SLOT_EMPTY;
	s.menu = GESS_MENU_NONE;
	s.profile = -1;
	s.team = 0;
	s.calOpen = false;
	m_GuestProfile[slot] = CGESSProfiles::DefaultProfile();	// A Guest's look settings go with them
	m_bSlotHadPad[slot] = m_iSlotPad[slot] >= 0;
	s.angleResetSeen = -1;
	s.pendingCycle = 0;
}

void CGESplitScreenClient::ApplyHostProfile()
{
	// Guest profile: play under their own name
	if ( m_Slots[0].profile < 0 )
	{
		if ( m_bHostRenamed || ges_ss_host_renamed.GetBool() )
			RestoreHostName();
		return;
	}

	const GESSProfile_t &p = GetSlotProfileData( 0 );

	// Remember player 1's own name the first time it's replaced
	if ( !m_bHostRenamed && !ges_ss_host_renamed.GetBool() )
	{
		static ConVarRef name( "name" );
		if ( name.IsValid() )
			ges_ss_host_realname.SetValue( name.GetString() );
	}
	m_bHostRenamed = true;
	ges_ss_host_renamed.SetValue( 1 );

	char szName[GES_SS_PROFILE_NAME_LEN];
	GESS_CleanName( szName, sizeof( szName ), p.name );
	engine->ClientCmd_Unrestricted( VarArgs( "name \"%s\"\n", szName ) );
}

//-----------------------------------------------------------------------------
// Joining before the match starts
//-----------------------------------------------------------------------------
void CGESplitScreenClient::BeginPregame()
{
	GESSMatch()->ClearPregamePending();

	// Nothing to wait for without controllers (keyboard and mouse can't take screens)
	if ( !HasAnyPadConnected() )
		return;

	m_bPregame = true;
	m_iPregameSlot = -1;
	m_iPregameStage = GESS_PREGAME_CLAIM;
	m_iPregameTeam = TEAM_MI6;

	// Remember who picked which profile in the menus, by controller; everyone
	// takes their screen again here
	for ( int i = 0; i < GES_SS_MAX_PLAYERS; i++ )
	{
		m_iPregameProfile[i] = -2;
		m_bPregameJoined[i] = false;
	}
	for ( int slot = 0; slot < GES_SS_MAX_PLAYERS; slot++ )
	{
		const int pad = m_iSlotPad[slot];
		if ( pad >= 0 && pad < GES_SS_MAX_PLAYERS )
			m_iPregameProfile[pad] = m_Slots[slot].profile;

		if ( slot > 0 )
		{
			LocalSlot_t &s = m_Slots[slot];
			s.state = GESS_SLOT_EMPTY;
			s.menu = GESS_MENU_NONE;
			s.debugNoPad = false;
		}
	}
	m_Slots[0].menu = GESS_MENU_NONE;

	// Hold the match: no clocks, nobody moves
	engine->ClientCmd_Unrestricted( "ges_ss_pregame 1\n" );
	DevMsg( "Split screen: waiting for the players to take their screens\n" );
}

void CGESplitScreenClient::FinishPregame()
{
	if ( !m_bPregame )
		return;

	m_bPregame = false;
	m_iPregameSlot = -1;

	// Screens nobody took close: the layout is worked out again for the players who are in
	engine->ClientCmd_Unrestricted( "ges_ss_pregame 0\n" );
	if ( GEMusicManager() )
		GEMusicManager()->ResumeMusic();
	Msg( "Split screen: everyone is in, the match starts\n" );
}

void CGESplitScreenClient::ConfirmPregameSlot( int slot )
{
	LocalSlot_t &s = m_Slots[slot];
	m_bPregameJoined[slot] = true;
	m_iPregameStage = GESS_PREGAME_CLAIM;
	m_iPregameSlot = -1;

	if ( slot == 0 )
	{
		m_bHostChosen = true;
		ApplyHostChoice();		// Team and character while the match is still held
	}
	else
	{
		s.state = GESS_SLOT_JOINING;
		s.menu = GESS_MENU_NONE;
		SendJoin( slot );
	}
}

void CGESplitScreenClient::PregameClaim( int pad )
{
	if ( !m_bPregame || m_iPregameSlot < 0 || m_iPregameStage != GESS_PREGAME_CLAIM || pad < 0 || pad >= GES_SS_MAX_PADS )
		return;
	if ( !GESSGamepads()->GetPad( pad ).connected )
		return;

	const int slot = m_iPregameSlot;
	const int owner = GetPadSlot( pad );

	// A controller already playing can't take a second screen. Player 1's screen
	// belongs to player 1's controller (or any controller if they have none).
	if ( owner >= 0 && m_bPregameJoined[owner] )
		return;
	if ( slot == 0 && m_iSlotPad[0] >= 0 && owner != 0 )
		return;

	if ( owner >= 0 && owner != slot )
		m_iSlotPad[owner] = -1;
	m_iSlotPad[slot] = pad;
	m_bSlotHadPad[slot] = true;
	GESSGamepads()->ConsumeEdges( pad );

	// Their profile from the menus, if nobody has taken it meanwhile
	LocalSlot_t &s = m_Slots[slot];
	const int wanted = m_iPregameProfile[pad];
	if ( wanted >= -1 && !IsSlotProfileInUse( wanted, slot ) )
		s.profile = wanted;
	else if ( slot > 0 || s.profile < 0 || IsSlotProfileInUse( s.profile, slot ) )
		s.profile = NextFreeProfile( slot, -1 );

	Msg( "Split screen: controller %d took screen %d (%s)\n", pad + 1, slot + 1, GetSlotName( slot ) );

	if ( IsTeamplay() )
	{
		m_iPregameStage = GESS_PREGAME_TEAM;
		m_iPregameTeam = ( s.team == TEAM_JANUS ) ? TEAM_JANUS : TEAM_MI6;
		m_flPregameStickTime = 0;
	}
	else
	{
		ConfirmPregameSlot( slot );
	}
}

// Testing: take the offered screen (not player 1's) for a player with no controller
void CGESplitScreenClient::DebugPregameClaimNoPad()
{
	if ( !m_bPregame || m_iPregameSlot <= 0 || m_iPregameStage != GESS_PREGAME_CLAIM )
		return;

	const int slot = m_iPregameSlot;
	LocalSlot_t &s = m_Slots[slot];
	s.debugNoPad = true;
	s.profile = NextFreeProfile( slot, -1 );
	Msg( "Split screen: a player with no controller took screen %d (%s)\n", slot + 1, GetSlotName( slot ) );

	if ( IsTeamplay() )
	{
		m_iPregameStage = GESS_PREGAME_TEAM;
		m_iPregameTeam = TEAM_MI6;
	}
	else
	{
		ConfirmPregameSlot( slot );
	}
}

void CGESplitScreenClient::PregameTeam( int dir )
{
	if ( m_bPregame && m_iPregameStage == GESS_PREGAME_TEAM && dir )
		m_iPregameTeam = dir < 0 ? TEAM_MI6 : TEAM_JANUS;
}

void CGESplitScreenClient::PregameConfirmTeam()
{
	if ( !m_bPregame || m_iPregameStage != GESS_PREGAME_TEAM || m_iPregameSlot < 0 )
		return;
	m_Slots[m_iPregameSlot].team = m_iPregameTeam;
	ConfirmPregameSlot( m_iPregameSlot );
}

void CGESplitScreenClient::PregameLeave( int slot )
{
	if ( !m_bPregame || slot <= 0 || slot >= GES_SS_MAX_PLAYERS )
		return;

	if ( m_bPregameJoined[slot] )
	{
		m_bPregameJoined[slot] = false;
		LeaveSlot( slot );
	}
	else if ( slot == m_iPregameSlot && m_iPregameStage == GESS_PREGAME_TEAM )
	{
		m_iPregameStage = GESS_PREGAME_CLAIM;
	}
	else
	{
		return;
	}

	// The controller is spare again
	m_iSlotPad[slot] = -1;
	m_iPregameSlot = -1;
	m_iPregameStage = GESS_PREGAME_CLAIM;
}

void CGESplitScreenClient::UpdatePregame()
{
	if ( !m_bPregame || !IsInGame() )
		return;

	// No music until the match starts
	if ( GEMusicManager() )
		GEMusicManager()->PauseMusic();

	// The next screen nobody has taken
	const int maxSlots = GetMaxSlots();
	int next = -1;
	for ( int slot = 0; slot < maxSlots; slot++ )
	{
		if ( !m_bPregameJoined[slot] )
		{
			next = slot;
			break;
		}
	}
	if ( next < 0 )
	{
		FinishPregame();
		return;
	}
	if ( m_iPregameSlot != next )
	{
		m_iPregameSlot = next;
		m_iPregameStage = GESS_PREGAME_CLAIM;
	}

	// The game menu (Esc) has the controllers
	if ( enginevgui->IsGameUIVisible() )
		return;

	// START on player 1's controller: play with the players who are in
	const int hostPad = m_iSlotPad[0];
	if ( m_bPregameJoined[0] && hostPad >= 0 && GESSGamepads()->GetPad( hostPad ).WasPressed( GESS_PAD_START ) )
	{
		GESSGamepads()->ConsumeEdges( hostPad );
		FinishPregame();
		return;
	}

	// Players who are in: LB / RB change profile, B gives the screen back
	for ( int slot = 0; slot < maxSlots; slot++ )
	{
		if ( !m_bPregameJoined[slot] )
			continue;
		const GESSPadState_t &pad = GESSGamepads()->GetPad( m_iSlotPad[slot] );
		if ( !pad.connected )
			continue;

		const int dir = pad.WasPressed( GESS_PAD_RB ) ? 1 : ( pad.WasPressed( GESS_PAD_LB ) ? -1 : 0 );
		if ( dir )
		{
			m_Slots[slot].profile = CycleProfile( slot, m_Slots[slot].profile, dir );
			FinishChoice( slot );
		}
		else if ( slot > 0 && pad.WasPressed( GESS_PAD_B ) )
		{
			PregameLeave( slot );
			return;
		}
	}

	if ( m_iPregameStage == GESS_PREGAME_CLAIM )
	{
		// The right trigger takes the screen
		for ( int p = 0; p < GES_SS_MAX_PADS; p++ )
		{
			const GESSPadState_t &pad = GESSGamepads()->GetPad( p );
			if ( pad.connected && pad.WasPressed( GESS_PAD_RT ) )
			{
				PregameClaim( p );
				if ( m_iPregameStage == GESS_PREGAME_TEAM || m_bPregameJoined[next] )
					break;
			}
		}
	}
	else
	{
		// Team: left stick (or d-pad) left / right, right trigger confirms, B goes back
		const GESSPadState_t &pad = GESSGamepads()->GetPad( m_iSlotPad[next] );
		if ( !pad.connected )
		{
			if ( !m_Slots[next].debugNoPad )
				PregameLeave( next );
			return;
		}
		if ( pad.lx < -0.5f || pad.WasPressed( GESS_PAD_DPAD_LEFT ) )
			PregameTeam( -1 );
		else if ( pad.lx > 0.5f || pad.WasPressed( GESS_PAD_DPAD_RIGHT ) )
			PregameTeam( 1 );

		if ( pad.WasPressed( GESS_PAD_RT ) )
			PregameConfirmTeam();
		else if ( pad.WasPressed( GESS_PAD_B ) && next > 0 )
			PregameLeave( next );
	}
}

//-----------------------------------------------------------------------------
// Controller to game input
//-----------------------------------------------------------------------------
float CGESplitScreenClient::LookScale( int slot, const GESSPadState_t &pad ) const
{
	float scale = GetSlotProfileData( slot ).sensitivity;
	if ( pad.IsDown( GESS_PAD_LT ) )
		scale *= ges_ss_look_aim_scale.GetFloat();
	return scale;
}

//-----------------------------------------------------------------------------
// Aim assist for the players on controllers in a split screen match (the
// Match page's "Aim assist", ge_ss_aimassist). Each player's own view is used,
// so it works the same in every viewport; teammates are left alone in team games.
//  2 Sticky aim: while the crosshair is on (or just off) an enemy, the view turns
//    with them as they move (or as you strafe past them)
//  3 Slowdown: looking around slows down over or near an enemy
//  1 Classic GoldenEye is on the server (CGEPlayer::GetAutoaimVector): the shots
//    home in, the view stays put
//-----------------------------------------------------------------------------
extern ConVar ge_ss_aimassist;
static ConVar ges_ss_aim_sticky( "ges_ss_aim_sticky", "0.85", FCVAR_ARCHIVE, "Sticky aim: how much of a target's movement across the view the view follows (0-1)." );
static ConVar ges_ss_aim_slowdown( "ges_ss_aim_slowdown", "0.4", FCVAR_ARCHIVE, "Slowdown aim assist: look speed over a target (1 = no slowdown)." );
static ConVar ges_ss_aim_radius( "ges_ss_aim_radius", "1.5", FCVAR_ARCHIVE, "Aim assist: degrees added around a target's body that count as on target." );

static ConVar ges_ss_debug_aim( "ges_ss_debug_aim", "0", FCVAR_DONTRECORD, "Testing: log the aim assist target for each player." );
struct GESSAimMemory_t { int target; QAngle anglesTo; };
static GESSAimMemory_t s_AimMemory[GES_SS_MAX_PLAYERS];

float CGESplitScreenClient::ApplyAimAssist( int slot, C_BasePlayer *pPlayer, QAngle &angles )
{
	const int mode = ge_ss_aimassist.GetInt();
	GESSAimMemory_t &mem = s_AimMemory[clamp( slot, 0, GES_SS_MAX_PLAYERS - 1 )];
	if ( ( mode != 2 && mode != 3 ) || !pPlayer || !( IsSplitMatch() || HasLocalGuests() ) )
	{
		mem.target = 0;
		return 1.0f;
	}

	// Where this player looks from
	Vector eye = pPlayer->EyePosition();
	QAngle viewAngles;
	float fov, fovViewmodel;
	GESS_GetPlayerViewSetup( pPlayer, eye, viewAngles, fov, fovViewmodel );
	Vector forward;
	AngleVectors( angles, &forward );

	// The enemy nearest the crosshair, measured in "target sizes" from it
	const bool bTeams = GEMPRules() && GEMPRules()->IsTeamplay();
	const float maxRatio = mode == 3 ? 2.0f : 1.2f;
	float bestRatio = maxRatio;
	C_BasePlayer *pBest = NULL;
	QAngle bestAngles;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		C_BasePlayer *pOther = UTIL_PlayerByIndex( i );
		if ( !pOther || pOther == pPlayer || pOther->IsDormant() || !pOther->IsAlive() || pOther->IsObserver() )
			continue;
		if ( bTeams && pOther->GetTeamNumber() == pPlayer->GetTeamNumber() )
			continue;

		const Vector target = pOther->WorldSpaceCenter() + Vector( 0, 0, 8 );
		Vector dir = target - eye;
		const float dist = VectorNormalize( dir );
		if ( dist < 1.0f || dist > 2500.0f )
			continue;
		const float offset = RAD2DEG( acosf( clamp( DotProduct( dir, forward ), -1.0f, 1.0f ) ) );
		const float radius = RAD2DEG( atanf( 20.0f / dist ) ) + ges_ss_aim_radius.GetFloat();
		const float ratio = offset / radius;
		if ( ratio >= bestRatio )
			continue;

		trace_t tr;
		UTIL_TraceLine( eye, target, MASK_SHOT, pPlayer, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction < 1.0f && tr.m_pEnt != pOther )
			continue;

		pBest = pOther;
		bestRatio = ratio;
		VectorAngles( dir, bestAngles );
	}

	if ( !pBest )
	{
		mem.target = 0;
		return 1.0f;
	}

	static ConVarRef s_debug( "ges_ss_debug_aim" );
	static float s_flNextDebug[GES_SS_MAX_PLAYERS];
	if ( s_debug.IsValid() && s_debug.GetBool() && gpGlobals->realtime >= s_flNextDebug[slot] )
	{
		s_flNextDebug[slot] = gpGlobals->realtime + 0.5f;
		Msg( "aim assist: player %d, mode %d, target %s, %.2f target sizes off\n", slot + 1, mode, pBest->GetPlayerName(), bestRatio );
	}

	if ( mode == 3 )
	{
		// Full slowdown on the target, easing back to normal speed at twice its size
		mem.target = 0;
		const float t = clamp( bestRatio - 1.0f, 0.0f, 1.0f );
		return Lerp( t, ges_ss_aim_slowdown.GetFloat(), 1.0f );
	}

	// Sticky: follow how far the target moved across the view since the last frame
	if ( mem.target == pBest->entindex() )
	{
		const float dYaw = AngleDiff( bestAngles[YAW], mem.anglesTo[YAW] );
		const float dPitch = AngleDiff( bestAngles[PITCH], mem.anglesTo[PITCH] );
		if ( fabsf( dYaw ) < 10.0f && fabsf( dPitch ) < 10.0f )
		{
			const float k = clamp( ges_ss_aim_sticky.GetFloat(), 0.0f, 1.0f );
			angles[YAW] = AngleNormalize( angles[YAW] + dYaw * k );
			angles[PITCH] = clamp( angles[PITCH] + dPitch * k, -89.0f, 89.0f );
		}
	}
	mem.target = pBest->entindex();
	mem.anglesTo = bestAngles;
	return 1.0f;
}

void CGESplitScreenClient::UpdateGuestLook( int slot, const GESSPadState_t &pad, float dt )
{
	LocalSlot_t &s = m_Slots[slot];
	C_BasePlayer *pPlayer = GetSlotPlayer( slot );
	if ( !pPlayer || !pPlayer->IsAlive() || pPlayer->IsObserver() )
		return;

	const GESSProfile_t &prof = GetSlotProfileData( slot );
	const float scale = LookScale( slot, pad ) * ApplyAimAssist( slot, pPlayer, s.angles );

	// Slight curve so small stick movements aim precisely
	const float rx = pad.rx * fabsf( pad.rx );
	const float ry = pad.ry * fabsf( pad.ry );

	s.angles[YAW] -= rx * ges_ss_look_yaw.GetFloat() * scale * dt;
	s.angles[PITCH] -= ry * ges_ss_look_pitch.GetFloat() * scale * dt * ( prof.invertY ? -1.0f : 1.0f );
	s.angles[PITCH] = clamp( s.angles[PITCH], -89.0f, 89.0f );
	s.angles[YAW] = AngleNormalize( s.angles[YAW] );
	s.angles[ROLL] = 0;
}

// Player 1's right stick, every frame like the guests' (UpdateGuestLook), so
// everyone turns at the same speed. It used to be applied in CreateMove, which
// only runs once per server tick (66 a second) but moved by one frame's time,
// so at high frame rates player 1 turned several times slower than the guests.
void CGESplitScreenClient::UpdateHostLook( float dt )
{
	if ( !ges_ss_enable.GetBool() || !engine->IsInGame() || engine->IsPaused() || dt <= 0.0f )
		return;

	const int padIndex = GetSlotPad( 0 );
	if ( padIndex < 0 )
		return;

	const GESSPadState_t &pad = GESSGamepads()->GetPad( padIndex );
	if ( !pad.connected || m_Slots[0].menu != GESS_MENU_NONE || IsGameUIVisible() || m_bPregame )
		return;

	const GESSProfile_t &prof = GetSlotProfileData( 0 );
	QAngle viewangles;
	engine->GetViewAngles( viewangles );
	const QAngle before = viewangles;
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	const float assist = ( pLocal && pLocal->IsAlive() && !pLocal->IsObserver() ) ? ApplyAimAssist( 0, pLocal, viewangles ) : 1.0f;
	const float scale = LookScale( 0, pad ) * assist;
	const float rx = pad.rx * fabsf( pad.rx );
	const float ry = pad.ry * fabsf( pad.ry );
	if ( rx == 0.0f && ry == 0.0f && viewangles == before )
		return;

	viewangles[YAW] -= rx * ges_ss_look_yaw.GetFloat() * scale * dt;
	viewangles[PITCH] -= ry * ges_ss_look_pitch.GetFloat() * scale * dt * ( prof.invertY ? -1.0f : 1.0f );
	viewangles[PITCH] = clamp( viewangles[PITCH], -89.0f, 89.0f );
	viewangles[YAW] = AngleNormalize( viewangles[YAW] );
	engine->SetViewAngles( viewangles );
}

int CGESplitScreenClient::ComputeButtons( const GESSPadState_t &pad ) const
{
	int buttons = 0;
	if ( pad.IsDown( GESS_PAD_RT ) )		buttons |= IN_ATTACK;
	if ( pad.IsDown( GESS_PAD_LT ) )		buttons |= IN_AIMMODE;
	if ( pad.IsDown( GESS_PAD_RB ) )		buttons |= IN_ATTACK2;
	// Face buttons: A / Cross jumps, X / Square reloads, B / Circle crouches
	if ( pad.IsDown( GESS_PAD_A ) )			buttons |= IN_JUMP;
	if ( pad.IsDown( GESS_PAD_X ) )			buttons |= IN_RELOAD;
	if ( pad.IsDown( GESS_PAD_B ) )			buttons |= IN_DUCK;
	if ( pad.IsDown( GESS_PAD_LSTICK ) )	buttons |= IN_DUCK;
	if ( pad.IsDown( GESS_PAD_RSTICK ) )	buttons |= IN_USE;		// action / open doors
	// Back / Select does nothing: the leaderboard is on START
	return buttons;
}

void CGESplitScreenClient::ComputeMove( const GESSPadState_t &pad, float &forward, float &side ) const
{
	const float fwdSpeed = cl_forwardspeed.IsValid() ? cl_forwardspeed.GetFloat() : 450.0f;
	const float sideSpeed = cl_sidespeed.IsValid() ? cl_sidespeed.GetFloat() : 450.0f;
	forward = pad.ly * fwdSpeed;
	side = pad.lx * sideSpeed;
}

void CGESplitScreenClient::HostCreateMove( float frametime, CUserCmd *cmd, QAngle &viewangles, bool active )
{
	if ( !ges_ss_enable.GetBool() || !active )
		return;

	const int padIndex = GetSlotPad( 0 );
	if ( padIndex < 0 )
		return;

	const GESSPadState_t &pad = GESSGamepads()->GetPad( padIndex );
	if ( !pad.connected || m_Slots[0].menu != GESS_MENU_NONE || IsGameUIVisible() || m_bPregame )
		return;

	// Look: every frame in UpdateHostLook (this runs once per tick, not per frame)

	// Move (added to keyboard input)
	float forward, side;
	ComputeMove( pad, forward, side );
	cmd->forwardmove += forward;
	cmd->sidemove += side;

	cmd->buttons |= ComputeButtons( pad );

	cmd->hostweaponcycle = m_Slots[0].pendingCycle;
	m_Slots[0].pendingCycle = 0;
}

static ConVar ges_ss_debug_fire( "ges_ss_debug_fire", "0", FCVAR_DONTRECORD, "Testing: split screen player <2-8> holds fire (0: off)." );

void CGESplitScreenClient::FillGuestCmds( CUserCmd *cmd )
{
	for ( int slot = 1; slot < GES_SS_MAX_PLAYERS; slot++ )
	{
		CGESplitGuestCmd &g = cmd->splitguests[slot - 1];
		g.Reset();

		LocalSlot_t &s = m_Slots[slot];
		if ( !ges_ss_enable.GetBool() || s.state != GESS_SLOT_JOINED )
			continue;

		g.active = true;
		g.viewangles = s.angles;
		g.anglereset = (unsigned char)( s.angleResetSeen & GES_SS_ANGLERESET_MASK );

		// Testing: a player without a controller holds fire
		if ( ges_ss_debug_fire.GetInt() == slot + 1 && !m_bPregame )
			g.buttons |= IN_ATTACK;

		const GESSPadState_t &pad = GESSGamepads()->GetPad( GetSlotPad( slot ) );
		if ( !pad.connected || s.menu != GESS_MENU_NONE || enginevgui->IsGameUIVisible() || m_bPregame )
			continue;	// Stand still while in a menu (or waiting for the match to start)

		ComputeMove( pad, g.forwardmove, g.sidemove );
		g.buttons = ComputeButtons( pad );
		if ( g.forwardmove > 0 ) g.buttons |= IN_FORWARD;
		if ( g.forwardmove < 0 ) g.buttons |= IN_BACK;
		if ( g.sidemove > 0 ) g.buttons |= IN_MOVERIGHT;
		if ( g.sidemove < 0 ) g.buttons |= IN_MOVELEFT;

		g.weaponcycle = s.pendingCycle;
		s.pendingCycle = 0;
	}
}

//-----------------------------------------------------------------------------
// Console
//-----------------------------------------------------------------------------
void CGESplitScreenClient::PrintStatus()
{
	Msg( "Split screen: %s, controllers: %d connected (%s, max %d), layout: %s\n",
		m_bSplitActive ? "active" : "inactive", GESSGamepads()->GetConnectedCount(), GESSGamepads()->GetBackendName(),
		GESSGamepads()->GetMaxPads(), m_bDualScreen ? "two screens" : "one screen" );

	for ( int p = 0; p < GES_SS_MAX_PADS; p++ )
	{
		const GESSPadState_t &pad = GESSGamepads()->GetPad( p );
		if ( pad.connected )
		{
			const int padSlot = GetPadSlot( p );
			Msg( "  Controller %d (%s): left %.2f %.2f  right %.2f %.2f  triggers %.2f %.2f  buttons %04x\n", p + 1,
				padSlot >= 0 ? VarArgs( "player %d", padSlot + 1 ) : "spare, press a button", pad.lx, pad.ly, pad.rx, pad.ry, pad.lt, pad.rt, pad.buttons );
		}
	}

	static const char *s_pszStates[] = { "empty", "choosing profile", "joining", "playing" };
	for ( int i = 0; i < GES_SS_MAX_PLAYERS; i++ )
	{
		const int pad = GetSlotPad( i );
		Msg( "  Player %d: %-16s controller %s%s  profile: %s\n", i + 1,
			i == 0 ? "main player" : s_pszStates[m_Slots[i].state],
			pad >= 0 ? VarArgs( "%d", pad + 1 ) : ( i == 0 ? "none (keyboard/mouse)" : "none" ),
			( pad < 0 || GESSGamepads()->GetPad( pad ).connected ) ? "" : " (not connected)",
			GetSlotName( i ) );

		C_BasePlayer *pPlayer = GetSlotPlayer( i );
		if ( pPlayer )
		{
			Vector origin = pPlayer->EyePosition(); QAngle angles = pPlayer->EyeAngles(); float fov = 0;
			const bool bView = i == 0 || GetGuestView( i, origin, angles, fov );
			const Vector &pos = pPlayer->GetAbsOrigin();
			Msg( "    entity %d at %.0f %.0f %.0f%s%s%s, view %s %.0f %.0f %.0f angles %.1f %.1f %.1f fov %.1f server %.1f %.1f\n", pPlayer->entindex(), pos.x, pos.y, pos.z,
				pPlayer->IsDormant() ? " DORMANT" : "", pPlayer->IsAlive() ? "" : " dead", pPlayer->IsObserver() ? " observer" : "",
				bView ? "ok" : "none", origin.x, origin.y, origin.z, angles.x, angles.y, angles.z, fov, pPlayer->EyeAngles().x, pPlayer->EyeAngles().y );
			for ( int v = 0; v < MAX_VIEWMODELS; v++ )
			{
				C_BaseViewModel *pVM = pPlayer->GetViewModel( v, false );
				if ( !pVM )
				{
					Msg( "    viewmodel %d: none\n", v );
					continue;
				}
				const Vector &vp = pVM->GetAbsOrigin();
				Msg( "    viewmodel %d: entity %d model %s%s%s%s at %.0f %.0f %.0f\n", v, pVM->entindex(),
					pVM->GetModel() ? modelinfo->GetModelName( pVM->GetModel() ) : "(none)",
					pVM->IsEffectActive( EF_NODRAW ) ? " NODRAW" : "", pVM->IsDormant() ? " DORMANT" : "",
					pVM->ShouldDraw() ? "" : " shouldn't draw", vp.x, vp.y, vp.z );
			}
		}
	}
}

#ifdef _WIN32
struct GESSMonitorEnum_t
{
	int count;
	RECT rects[8];
};

static BOOL CALLBACK GESS_MonitorEnumProc( HMONITOR, HDC, LPRECT pRect, LPARAM data )
{
	GESSMonitorEnum_t *pEnum = (GESSMonitorEnum_t*) data;
	if ( pEnum->count < 8 )
		pEnum->rects[pEnum->count++] = *pRect;
	return TRUE;
}
#endif

void CGESplitScreenClient::SpanMonitors()
{
#ifdef _WIN32
	GESSMonitorEnum_t mon;
	mon.count = 0;
	EnumDisplayMonitors( NULL, NULL, GESS_MonitorEnumProc, (LPARAM) &mon );

	if ( mon.count < 2 )
	{
		Msg( "Split screen: only one monitor found, players 5-8 will share the screen (4x2).\n" );
		return;
	}

	// Use the first two monitors, left to right
	RECT a = mon.rects[0], b = mon.rects[1];
	if ( b.left < a.left )
	{
		RECT t = a; a = b; b = t;
	}

	const int width = ( a.right - a.left ) + ( b.right - b.left );
	const int height = MIN( a.bottom - a.top, b.bottom - b.top );

	// Borderless window covering both monitors
	engine->ClientCmd_Unrestricted( VarArgs( "mat_setvideomode %d %d 1\n", width, height ) );

	HWND hWnd = FindWindowA( "Valve001", NULL );
	if ( hWnd )
	{
		LONG style = GetWindowLong( hWnd, GWL_STYLE );
		style &= ~( WS_CAPTION | WS_THICKFRAME | WS_BORDER | WS_DLGFRAME );
		SetWindowLong( hWnd, GWL_STYLE, style | WS_POPUP );
		SetWindowPos( hWnd, HWND_TOP, a.left, MAX( a.top, b.top ), width, height, SWP_FRAMECHANGED | SWP_SHOWWINDOW );
	}

	ges_ss_dualscreen.SetValue( 2 );
	Msg( "Split screen: window spans two monitors (%dx%d). If it doesn't, start the game with -windowed -noborder -w %d -h %d\n", width, height, width, height );
#else
	Msg( "Split screen: start the game with -windowed -noborder -w <both monitors' width> -h <height> to use two screens.\n" );
#endif
}

// Opens the Steam overlay's friends list. While you are on a server the engine tells Steam
// how to reach it (rich presence "connect"), so friends running the mod get "Join Game" in
// their Steam friends list, and you get "Invite to Game" on a friend here.
CON_COMMAND( ges_invite_friends, "Open the Steam overlay friends list to invite friends to this game" )
{
	ISteamFriends *pFriends = steamapicontext ? steamapicontext->SteamFriends() : NULL;
	ISteamUtils *pUtils = steamapicontext ? steamapicontext->SteamUtils() : NULL;
	if ( !pFriends || !pUtils )
	{
		Msg( "Steam isn't available, so friends can't be invited.\n" );
		return;
	}

	if ( !engine->IsInGame() )
	{
		Msg( "Start or join a game first, then invite friends.\n" );
		return;
	}

	if ( !pUtils->IsOverlayEnabled() )
	{
		// No overlay (it is off in Steam's settings, or the game wasn't started from Steam):
		// open Steam's own friends window, where Invite to Game works the same way
		Msg( "The Steam overlay is off, so Steam's friends window opens instead. Right-click a friend and choose Invite to Game.\n" );
		vgui::system()->ShellExecute( "open", "steam://open/friends" );
		return;
	}

	pFriends->ActivateGameOverlay( "Friends" );
}
CON_COMMAND( ges_ss_status, "Show split screen players and controllers" )
{
	GESplitScreenClient()->PrintStatus();
}

CON_COMMAND( ges_ss_span_monitors, "Stretch the game window across two monitors so players 5-8 get their own screen" )
{
	GESplitScreenClient()->SpanMonitors();
}

void CGESplitScreenClient::DebugCycleProfile( int slot, int dir )
{
	if ( slot < 0 || slot >= GES_SS_MAX_PLAYERS || ( slot > 0 && m_Slots[slot].state == GESS_SLOT_EMPTY ) )
		return;
	m_Slots[slot].profile = CycleProfile( slot, m_Slots[slot].profile, dir );
	if ( slot == 0 )
		m_Slots[slot].profileChosen = true;
	Msg( "Player %d profile: %s\n", slot + 1, GetSlotName( slot ) );
}

void CGESplitScreenClient::DebugJoin( int slot )
{
	if ( slot <= 0 || slot >= GES_SS_MAX_PLAYERS )
		return;

	LocalSlot_t &s = m_Slots[slot];
	if ( s.state != GESS_SLOT_EMPTY )
		return;

	s.profile = NextFreeProfile( slot, -1 );
	s.state = GESS_SLOT_JOINING;
	s.debugNoPad = true;
	s.joinSentTime = -1000.0f;
}

CON_COMMAND_F( ges_ss_debug_join, "Testing: join split screen player <2-8> as if they pressed START", FCVAR_DONTRECORD )
{
	if ( args.ArgC() < 2 )
		return;
	GESplitScreenClient()->DebugJoin( atoi( args[1] ) - 1 );
}

void CGESplitScreenClient::DebugGameMenu( int slot, const char *what )
{
	if ( slot < 0 || slot >= GES_SS_MAX_PLAYERS )
		return;
	LocalSlot_t &s = m_Slots[slot];
	if ( !Q_stricmp( what, "open" ) )			OpenGameMenu( slot );
	else if ( s.menu != GESS_MENU_PAUSE )		return;
	else if ( !Q_stricmp( what, "left" ) )		CycleGameMenuPage( slot, -1 );
	else if ( !Q_stricmp( what, "right" ) )	CycleGameMenuPage( slot, 1 );
	else if ( !Q_stricmp( what, "down" ) || !Q_stricmp( what, "up" ) )
	{
		if ( s.menuConfirm != GESS_CONFIRM_NONE )
			s.menuConfirmSel = 1 - s.menuConfirmSel;
		else
			s.menuSub = 1 - s.menuSub;
	}
	else if ( !Q_stricmp( what, "a" ) )		ActivateGameMenuPage( slot );
	else if ( !Q_stricmp( what, "b" ) )
	{
		if ( s.menuConfirm != GESS_CONFIRM_NONE )
			s.menuConfirm = GESS_CONFIRM_NONE;
		else
			s.menu = GESS_MENU_NONE;
	}
	Msg( "Player %d START menu: %s, page %d, button %d, confirm %d (%s)\n", slot + 1, s.menu == GESS_MENU_PAUSE ? "open" : "closed",
		s.menuSel, s.menuSub, s.menuConfirm, s.menuConfirmSel ? "Confirm" : "Cancel" );
}

CON_COMMAND_F( ges_ss_debug_menu, "Testing: ges_ss_debug_menu <player 1-8> open|left|right|up|down|a|b (the in-match START menu)", FCVAR_DONTRECORD )
{
	if ( args.ArgC() < 3 )
		return;
	GESplitScreenClient()->DebugGameMenu( atoi( args[1] ) - 1, args[2] );
}

// Testing the joining before the match without pressing buttons:
//   ges_ss_debug_pregame claim <controller 1-8>   right trigger on that controller
//   ges_ss_debug_pregame team <-1|1>              left stick left (MI6) / right (Janus)
//   ges_ss_debug_pregame confirm                  right trigger in the team choice
//   ges_ss_debug_pregame leave <player 2-8>       B
//   ges_ss_debug_pregame start                    START: play with who is in
CON_COMMAND_F( ges_ss_debug_pregame, "Testing: ges_ss_debug_pregame claim <controller> | team <-1|1> | confirm | leave <player> | start", FCVAR_DONTRECORD )
{
	CGESplitScreenClient *pSS = GESplitScreenClient();
	if ( args.ArgC() < 2 || !pSS->IsPregame() )
	{
		Msg( "Not waiting for players\n" );
		return;
	}
	const char *what = args[1];
	const int value = args.ArgC() > 2 ? atoi( args[2] ) : 0;
	if ( !Q_stricmp( what, "claim" ) && value == 0 )	pSS->DebugPregameClaimNoPad();
	else if ( !Q_stricmp( what, "claim" ) )		pSS->PregameClaim( value - 1 );
	else if ( !Q_stricmp( what, "team" ) )		pSS->PregameTeam( value );
	else if ( !Q_stricmp( what, "confirm" ) )	pSS->PregameConfirmTeam();
	else if ( !Q_stricmp( what, "leave" ) )		pSS->PregameLeave( value - 1 );
	else if ( !Q_stricmp( what, "start" ) )		pSS->FinishPregame();
	Msg( "Pre-match: screen %d, stage %s, team %s\n", pSS->GetPregameSlot() + 1,
		pSS->GetPregameStage() == CGESplitScreenClient::GESS_PREGAME_TEAM ? "team" : "take screen",
		pSS->GetPregameTeam() == TEAM_JANUS ? "Janus" : "MI6" );
}

CON_COMMAND_F( ges_ss_debug_cycle, "Testing: ges_ss_debug_cycle <player 1-8> <1|-1>, as if they pressed RB / LB in the lobby", FCVAR_DONTRECORD )
{
	if ( args.ArgC() < 3 )
		return;
	GESplitScreenClient()->DebugCycleProfile( atoi( args[1] ) - 1, atoi( args[2] ) < 0 ? -1 : 1 );
}

CON_COMMAND( ges_ss_reload_profiles, "Reload split screen profiles from disk" )
{
	GESSProfiles()->Load();
}
CON_COMMAND_F( ges_ss_debug_viewmodels, "Testing: list every viewmodel the client has and its owner", FCVAR_DONTRECORD )
{
	for ( C_BaseEntity *pEnt = ClientEntityList().FirstBaseEntity(); pEnt; pEnt = ClientEntityList().NextBaseEntity( pEnt ) )
	{
		C_BaseViewModel *pVM = dynamic_cast<C_BaseViewModel*>( pEnt );
		if ( !pVM )
			continue;
		C_BaseEntity *pOwner = pVM->GetOwner();
		Msg( "viewmodel entity %d index %d owner %d model %s%s%s seq %d cycle %.3f rate %.2f pred %d\n", pVM->entindex(), pVM->ViewModelIndex(), pOwner ? pOwner->entindex() : -1,
			pVM->GetModel() ? modelinfo->GetModelName( pVM->GetModel() ) : "(none)",
			pVM->IsEffectActive( EF_NODRAW ) ? " NODRAW" : "", pVM->IsDormant() ? " DORMANT" : "",
			pVM->GetSequence(), pVM->GetCycle(), pVM->GetPlaybackRate(), pVM->GetPredictable() ? 1 : 0 );
	}
}
