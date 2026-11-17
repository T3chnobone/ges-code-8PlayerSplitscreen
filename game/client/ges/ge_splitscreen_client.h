///////////// Copyright 2026 GoldenEye: Source. All rights reserved. /////////////
//
//   Project     : Client (GES)
//   File        : ge_splitscreen_client.h
//   Description :
//      Client side of local split-screen multiplayer.
//
//      Slot 0 is the normal local player. Slots 1-7 are guests: they press
//      START on their controller, pick a profile and join as server-side
//      players owned by this client. Their input rides in our usercmds and
//      their views are rendered into quarters of the screen. Players 5-8 go
//      on a second screen when the game window spans two monitors.
//
////////////////////////////////////////////////////////////////////////////

#ifndef GE_SPLITSCREEN_CLIENT_H
#define GE_SPLITSCREEN_CLIENT_H
#ifdef _WIN32
#pragma once
#endif

#include "igamesystem.h"
#include "ge_splitscreen_shared.h"
#include "ge_splitscreen_gamepad.h"
#include "ge_splitscreen_profiles.h"

class C_BaseEntity;
class C_BasePlayer;
class C_GEMPPlayer;
class CUserCmd;
struct GESSPadState_t;

enum GESSSlotState_t
{
	GESS_SLOT_EMPTY = 0,	// Nobody playing in this slot
	GESS_SLOT_CHOOSING,		// Picking a profile (join menu showing in their viewport)
	GESS_SLOT_JOINING,		// Join sent, waiting for the server to create the player
	GESS_SLOT_JOINED,		// Playing
};

enum GESSSlotMenu_t
{
	GESS_MENU_NONE = 0,
	GESS_MENU_PROFILE,		// Profile list (join / change profile)
	GESS_MENU_PAUSE,		// In-game options for a joined player
	GESS_MENU_TEAM,			// Team choice after the profile, in team games
};

// Pages of the START menu during a match (left / right cycles them)
enum
{
	GESS_GAMEPAGE_LEADERBOARD = 0,
	GESS_GAMEPAGE_TEAM,			// Swap team (team games only)
	GESS_GAMEPAGE_CONTROLLER,	// Invert look, look sensitivity, Calibrate Sticks
	GESS_GAMEPAGE_EXIT,			// Exit game, and Commit Suicide under it
	GESS_GAMEPAGE_COUNT
};

// The Controller page's rows. The first is the page title: left / right there change
// page as on the other pages; on the settings below they change the setting.
enum
{
	GESS_CTRLROW_PAGE = 0,
	GESS_CTRLROW_INVERT,
	GESS_CTRLROW_SENS,
	GESS_CTRLROW_CALIBRATE,
	GESS_CTRLROW_COUNT
};

// The Calibrate Sticks screen's rows
enum
{
	GESS_CALROW_REMEASURE = 0,	// The lobby's R3 measurement (R3 works anywhere on the screen)
	GESS_CALROW_LEFT,			// Left stick dead zone
	GESS_CALROW_RIGHT,			// Right stick dead zone
	GESS_CALROW_DONE,			// Keep the changes
	GESS_CALROW_BACK,			// Put the sticks back as they were
	GESS_CALROW_COUNT
};

// Look sensitivity steps 1-10 in the START menu: 5 is the normal look speed (1.0x),
// 1 is 0.4x, 10 is 2x (0.15 a step below 5, 0.2 a step above)
#define GESS_SENS_STEPS		10
#define GESS_SENS_DEFAULT	5
inline float GESS_SensFromStep( int step )
{
	step = step < 1 ? 1 : ( step > GESS_SENS_STEPS ? GESS_SENS_STEPS : step );
	return step <= GESS_SENS_DEFAULT ? 1.0f - ( GESS_SENS_DEFAULT - step ) * 0.15f : 1.0f + ( step - GESS_SENS_DEFAULT ) * 0.2f;
}
inline int GESS_StepFromSens( float sens )
{
	int best = 1;
	for ( int s = 2; s <= GESS_SENS_STEPS; s++ )
	{
		const float d = GESS_SensFromStep( s ) - sens, db = GESS_SensFromStep( best ) - sens;
		if ( d * d < db * db )
			best = s;
	}
	return best;
}

// The Exit page's buttons (top to bottom), and what a confirmation box is asking
enum
{
	GESS_EXITBTN_EXIT = 0,
	GESS_EXITBTN_SUICIDE,
	GESS_EXITBTN_COUNT
};
enum
{
	GESS_CONFIRM_NONE = 0,
	GESS_CONFIRM_EXIT,
	GESS_CONFIRM_SUICIDE,
};

// What a mouse click on player 1's START menu hit (the HUD records where it drew them)
enum
{
	GESS_MENUHIT_NONE = 0,
	GESS_MENUHIT_PREV,		// < : the page to the left
	GESS_MENUHIT_NEXT,		// > : the page to the right
	GESS_MENUHIT_BUTTON,	// A button on the page (arg: which)
	GESS_MENUHIT_CONFIRM,	// A confirmation box answer (arg: 0 cancel, 1 confirm)
	GESS_MENUHIT_TEAM,		// Swap team
	GESS_MENUHIT_ROW,		// A row of the Controller page or Calibrate Sticks screen (arg: which)
	GESS_MENUHIT_SLIDER,	// Its slider (arg: the row; where along it: frac 0-1)
};

// Team menu entries
enum
{
	GESS_TEAMMENU_AUTO = 0,
	GESS_TEAMMENU_MI6,
	GESS_TEAMMENU_JANUS,
	GESS_TEAMMENU_COUNT
};

struct GESSViewport_t
{
	int x, y, w, h;
};

class CGESplitScreenClient : public CAutoGameSystemPerFrame
{
public:
	CGESplitScreenClient();

	// CAutoGameSystemPerFrame
	virtual bool Init();
	virtual void Shutdown();
	virtual void LevelInitPostEntity();
	virtual void LevelShutdownPreEntity();
	virtual void Update( float frametime );

	// ---- Slots ----
	GESSSlotState_t GetSlotState( int slot ) const	{ return m_Slots[slot].state; }
	GESSSlotMenu_t GetSlotMenu( int slot ) const		{ return m_Slots[slot].menu; }
	int  GetSlotMenuSelection( int slot ) const		{ return m_Slots[slot].menuSel; }
	int  GetSlotMenuSub( int slot ) const				{ return m_Slots[slot].menuSub; }	// Exit page: the highlighted button (GESS_EXITBTN_)
	// A confirmation box over the page (GESS_CONFIRM_), and its highlighted answer (0 cancel, 1 confirm)
	int  GetSlotMenuConfirm( int slot ) const			{ return m_Slots[slot].menuConfirm; }
	int  GetSlotMenuConfirmSel( int slot ) const		{ return m_Slots[slot].menuConfirmSel; }
	bool CanSlotSuicide( int slot ) const;	// Alive: the Commit Suicide button works
	// Player 1's START menu takes the mouse (a cursor shows; clicks don't fire)
	bool WantsMenuMouse() const;
	// The mouse over (bClick false) or clicking (true) something in player 1's START menu
	// (frac: how far along a slider, 0-1)
	void OnMenuMouse( int hit, int arg, bool bClick, float frac = 0.0f );
	// The Controller page: the highlighted row (GESS_CTRLROW_), whether the Calibrate
	// Sticks screen is open, its highlighted row (GESS_CALROW_)
	int  GetSlotCtrlRow( int slot ) const				{ return m_Slots[slot].ctrlRow; }
	bool IsSlotCalibrateOpen( int slot ) const			{ return m_Slots[slot].calOpen; }
	int  GetSlotCalRow( int slot ) const				{ return m_Slots[slot].calRow; }
	// The player's look settings (their profile's; a Guest's last until they leave)
	bool GetSlotInvert( int slot ) const;
	int  GetSlotSensStep( int slot ) const;
	// Right click: back (closes a confirmation box, else the menu)
	void OnMenuMouseBack();
	bool IsGameMenuPageAvailable( int page ) const;
	// Testing: what a button does in the START menu
	void DebugGameMenu( int slot, const char *what );
	int  GetSlotProfile( int slot ) const				{ return m_Slots[slot].profile; }
	int  GetSlotTeam( int slot ) const					{ return m_Slots[slot].team; }
	const char *GetSlotName( int slot ) const;
	// Controllers belong to players in the order they press a button: the first
	// one is player 1 (and drives the menus), later ones players 2-8.
	// -1 = no controller (player 1 can always use keyboard and mouse).
	int  GetSlotPad( int slot ) const;
	int  GetPadSlot( int pad ) const;
	// No controller has claimed player 1 yet (the main menu shows a hint)
	bool IsWaitingForHostPad() const;
	// A connected controller that isn't playing yet (can press START to join)
	bool HasSparePad() const;
	// Playing, but their controller was unplugged (waiting for one to press a button)
	bool IsSlotPadDisconnected( int slot ) const;
	// Split screen match: only controllers play (keyboard and mouse game input is off)
	bool IsControllerOnly() const;
	// Split screen reads the controllers: the engine's own controller buttons
	// (Xbox pads through XInput) must not run key bindings for player 1
	bool ShouldBlockEnginePadButtons() const;
	C_BasePlayer *GetSlotPlayer( int slot ) const;
	bool IsSlotProfileInUse( int profile, int ignoreSlot ) const;
	int  NextFreeProfile( int slot, int current ) const;
	// The next (dir 1) or previous (dir -1) invited profile or Guest that no other
	// player is using, wrapping round (LB / RB in the lobby)
	int  CycleProfile( int slot, int current, int dir ) const;

	// Testing: join a guest as if it pressed START (needs its controller connected)
	void DebugJoin( int slot );
	void DebugCycleProfile( int slot, int dir );

	// Profile list entries offered in the join menu: invited profiles followed by "Guest"
	int  GetMenuProfileCount() const;
	int  GetMenuProfileIndex( int entry ) const;	// -1 = Guest/default
	int  GetPauseMenuCount() const;
	const char *GetPauseMenuLabel( int entry ) const;
	const char *GetTeamMenuLabel( int entry ) const;

	// ---- Layout ----
	// True when more than one local player is active and the screen is split
	bool IsSplitActive() const		{ return m_bSplitActive; }
	bool IsDualScreen() const		{ return m_bDualScreen; }
	int  GetScreenCount() const		{ return m_bSplitActive && m_bDualScreen ? 2 : 1; }
	// Pixel rect (in the full window) for a slot's viewport
	bool GetViewport( int slot, GESSViewport_t &vp ) const;
	bool IsSlotVisible( int slot ) const;
	// Grid cells nobody plays in (e.g. the 4th quarter with 3 players): the first
	// shows the scoreboard, the rest the GoldenEye logo
	int  GetSpareCellCount() const	{ return m_iSpareCells; }
	bool GetSpareCell( int i, GESSViewport_t &vp ) const;

	// Every view is the same size: an equal grid for n views on one display
	// (1: whole screen, 2: top / bottom, 3-4: 2x2, 5-6: 3x2, 7-8: 4x2)
	static void GridForCount( int n, int &cols, int &rows );
	// 1, or 2 when the game window spans two monitors (ges_ss_dualscreen)
	int  GetDisplayCount() const;
	// Players per display: the Split Screen menu's setting for our split screen
	// matches (and in the menus), otherwise 4 on two displays / 8 on one
	int  GetPerDisplay() const;
	// How many players can play: players per display x displays, at most 8
	int  GetMaxSlots() const;

	// ---- Joining before the match starts ----
	// When a split screen match's map loads, the screen splits into its final layout
	// and the game waits (no timers, music or movement). One by one each screen asks
	// for a player: the right trigger takes it, then in team games the left stick
	// picks MI6 / Janus and the right trigger confirms. When every screen has a
	// player (or player 1 presses START), the match starts.
	enum { GESS_PREGAME_CLAIM = 0, GESS_PREGAME_TEAM };
	bool IsPregame() const				{ return m_bPregame; }
	int  GetPregameSlot() const			{ return m_iPregameSlot; }	// Screen being offered, -1 = none
	int  GetPregameStage() const		{ return m_iPregameStage; }
	int  GetPregameTeam() const			{ return m_iPregameTeam; }
	bool IsPregameJoined( int slot ) const	{ return slot >= 0 && slot < GES_SS_MAX_PLAYERS && m_bPregameJoined[slot]; }
	void PregameClaim( int pad );			// Right trigger on this controller
	void PregameTeam( int dir );			// Left stick: -1 MI6, 1 Janus
	void PregameConfirmTeam();				// Right trigger in the team choice
	void PregameLeave( int slot );			// B: give the screen back
	void FinishPregame();					// Everyone is in (or START): start the match
	void DebugPregameClaimNoPad();			// Testing: a player without a controller

	// ---- Rendering ----
	// The camera each player's view is drawn with this frame
	void SetSlotViewSetup( int slot, const Vector &origin, const QAngle &angles, float fov, float fovViewmodel );
	bool GetSlotViewSetup( int slot, Vector &origin, QAngle &angles, float &fov, float &fovViewmodel ) const;
	void SetRenderSlot( int slot );
	int  GetRenderSlot() const			{ return m_iRenderSlot; }
	C_BaseEntity *GetRenderViewer() const;
	// Hide the viewer's own body/weapon/hat in their own viewport
	bool ShouldHideForCurrentView( C_BaseEntity *pEntity ) const;
	bool ShouldHideParticlesForCurrentView( C_BaseEntity *pOwner ) const;
	// Eye position/angles/fov for a guest's view (local angles for responsive aiming)
	bool GetGuestView( int slot, Vector &origin, QAngle &angles, float &fov ) const;
	// A guest's view for this frame, worked out once with player 1's (before
	// entities simulate) so their gun and their camera always agree
	void SetFrameGuestView( int slot, const Vector &origin, const QAngle &angles, float fov );
	bool GetFrameGuestView( int slot, Vector &origin, QAngle &angles, float &fov ) const;
	// Testing (ges_ss_debug_look): turn a guest without a controller
	void DebugLook( int slot, float yawSpeed, float pitchSpeed, float seconds );
	void UpdateDebugLook( float dt );
	float	m_flDebugLookYaw[GES_SS_MAX_PLAYERS], m_flDebugLookPitch[GES_SS_MAX_PLAYERS];
	double	m_flDebugLookUntil[GES_SS_MAX_PLAYERS];

	// ---- Input (called from CInput::CreateMove) ----
	void HostCreateMove( float frametime, CUserCmd *cmd, QAngle &viewangles, bool active );
	void FillGuestCmds( CUserCmd *cmd );

	// ---- HUD ----
	bool ShouldHideHudElement( const char *pszName ) const;
	bool HasAnyPadConnected() const;

	// Console helpers
	void PrintStatus();
	void SpanMonitors();

private:
	struct LocalSlot_t
	{
		GESSSlotState_t state;
		GESSSlotMenu_t menu;
		int		menuSel;
		int		menuSub;			// START menu exit page: the highlighted button (GESS_EXITBTN_)
		int		menuConfirm;		// Confirmation box open (GESS_CONFIRM_)
		int		menuConfirmSel;		// Its highlighted answer: 0 cancel, 1 confirm
		int		profile;			// Profile index, -1 = default guest profile
		QAngle	angles;				// Locally integrated view angles for guests
		int		angleResetSeen;		// Last server angle reset serial applied (-1 = none yet)
		float	joinSentTime;
		signed char pendingCycle;	// Weapon cycle waiting for the next usercmd
		bool	profileChosen;		// Player 1 picked a profile (or Guest) themselves
		bool	debugNoPad;		// Joined with ges_ss_debug_join: works without a controller
		float	menuRepeatTime;		// Stick navigation repeat timer
		int		team;				// Chosen team (TEAM_MI6 / TEAM_JANUS / MAX_GE_TEAMS = auto), 0 = not asked yet
		// START menu Controller page
		int		ctrlRow;			// Highlighted row (GESS_CTRLROW_)
		bool	calOpen;			// Calibrate Sticks screen open
		int		calRow;				// Its highlighted row (GESS_CALROW_)
		int		calPad;				// The controller it opened for (-1 none)
		CGESSGamepads::StickCal_t calBackup[2];	// The sticks as they were (Back puts them back)
	};

	const struct GESSProfile_t &GetSlotProfileData( int slot ) const;
	// Look settings changed in the START menu go here: the profile, or for Guest this slot's copy
	GESSProfile_t *GetSlotProfileForEdit( int slot );
	GESSProfile_t m_GuestProfile[GES_SS_MAX_PLAYERS];
	void SetSlotSensStep( int slot, int step );
	void ToggleSlotInvert( int slot );
	void OpenCalibrate( int slot );
	void CloseCalibrate( int slot, bool bKeep );
	void SetCalDeadzone( int slot, int stick, float inner );
	// The Controller page and Calibrate Sticks screen with a controller (false: not on them)
	bool UpdateControllerPage( int slot, const GESSPadState_t &pad, int vdir, float dt );
	bool IsInGame() const;
	bool IsGameUIVisible() const;

	void UpdatePadAssignment();
	void UpdatePadMouse( float frametime );
	void BeginPregame();
	void UpdatePregame();
	void ConfirmPregameSlot( int slot );
	void UpdateSlot( int slot, float frametime );
	void UpdateMenu( int slot, const GESSPadState_t &pad, float frametime );
	void UpdateGameMenu( int slot, const GESSPadState_t &pad, float frametime );
	void OpenGameMenu( int slot );
	void BlockPauseMenu();		// No game pause menu in a split screen match: player 1's START menu instead
	void CycleGameMenuPage( int slot, int dir );
	void ActivateGameMenuPage( int slot );
	void AnswerGameMenuConfirm( int slot, bool bYes );
	void UpdateGuestLook( int slot, const GESSPadState_t &pad, float frametime );
	void UpdateHostLook( float frametime );
	void UpdateLayout();

	void OpenProfileMenu( int slot );
	void ConfirmProfile( int slot, int entry );
	void ConfirmTeam( int slot, int entry );
	void FinishChoice( int slot );
	void ApplyHostChoice();
	bool IsTeamplay() const;
public:
	bool HasLocalGuests() const;
	// A split screen match started from the Split Screen menu is running on our own server
	bool IsSplitMatch() const;
	bool ShouldSuppressGameMenus() const;
private:
	void SendJoin( int slot );
	void LeaveSlot( int slot );
	void ApplyHostProfile();
	void RestoreHostName();

	int  ComputeButtons( const GESSPadState_t &pad ) const;
	void ComputeMove( const GESSPadState_t &pad, float &forward, float &side ) const;
	float LookScale( int slot, const GESSPadState_t &pad ) const;
	// Sticky aim / slowdown near a target (ge_ss_aimassist 2 / 3): turns angles to follow a
	// target and returns how much to scale this frame's look
	float ApplyAimAssist( int slot, C_BasePlayer *pPlayer, QAngle &angles );

	LocalSlot_t m_Slots[GES_SS_MAX_PLAYERS];
	int		m_iSlotPad[GES_SS_MAX_PLAYERS];	// Controller of each player, -1 = none
	bool	m_bSlotHadPad[GES_SS_MAX_PLAYERS];	// Had a controller since joining (shows "Controller disconnected")

	bool	m_bSplitActive;
	bool	m_bDualScreen;
	// Viewport of each slot, worked out in UpdateLayout from how many players are in
	bool	m_bViewportValid[GES_SS_MAX_PLAYERS];
	GESSViewport_t m_Viewports[GES_SS_MAX_PLAYERS];
	GESSViewport_t m_SpareCells[GES_SS_MAX_PLAYERS * 2];
	int		m_iSpareCells;
	struct SlotView_t
	{
		Vector	origin;
		QAngle	angles;
		float	fov, fovViewmodel;
		int		frame;		// gpGlobals->framecount it was set, 0 = never
	};
	SlotView_t m_SlotViews[GES_SS_MAX_PLAYERS];
	SlotView_t m_FrameGuestViews[GES_SS_MAX_PLAYERS];	// fovViewmodel unused
	float	m_flExitToMenuTime;		// Player 1 ended the match: back to the menu after the results

	// Joining before the match starts
	bool	m_bPregame;
	int		m_iPregameSlot;
	int		m_iPregameStage;
	int		m_iPregameTeam;
	bool	m_bPregameJoined[GES_SS_MAX_PLAYERS];
	int		m_iPregameProfile[GES_SS_MAX_PLAYERS];	// Per controller: the profile picked in the menus (-2 = none)
	float	m_flPregameStickTime;
	int		m_iRenderSlot;
	EHANDLE	m_hRenderViewer;		// Player of m_iRenderSlot, cached while rendering
	bool	m_bRenderViewerFirstPerson;

	// Joystick-driven mouse
	float	m_flCursorX, m_flCursorY;
	bool	m_bPadMouseDown;
	bool	m_bCursorWasVisible;

	// The game's own menus (main menu, pause menu, dialogs) driven by player 1's
	// controller: key presses sent to vgui like an Xbox pad's
	void UpdateMenuPadNav( float frametime );
	bool ShouldHideMenuMouse() const;
	void SendMenuKey( int code, bool bDown );
	int		m_iMenuNavPad;			// Controller that drove it last frame (-1 = none)
	int		m_iMenuNavDir;			// Direction key held (-1 = none)
	float	m_flMenuNavRepeat;		// When the held direction repeats
	bool	m_bMenuNavADown, m_bMenuNavBDown;
	bool	m_bMenuMouseHidden;
public:
	// Testing: what a controller button does in the game's own menus
	void DebugMenuButton( const char *pszButton );
private:

	// Player 1's profile name is put in the name cvar while playing, and
	// their own name is put back when they leave the game
	bool	m_bApplyHostProfile;
	bool	m_bHostRenamed;

	// Player 1 picks a profile (and team) in their own viewport like everyone else
	// once a split screen game starts. Remembered across map changes.
	bool	m_bHostChosen;			// Picked a profile at least once
	bool	m_bHostAppliedThisLevel;	// Their team / character were sent this map
	float	m_flHidePanelsUntil;	// Keep the normal team / character menus closed for a bit

	// Teams on / off follow the split screen match settings (checked once per map)
	bool	m_bTeamplaySynced;
};

CGESplitScreenClient *GESplitScreenClient();

// The START menu's suicide button label (localization string #GES_SS_Suicide)
const char *GESS_SuicideLabel();

// Lightweight hooks used by engine-side client code
bool GESS_IsSplitActive();
bool GESS_ShouldHideForCurrentView( C_BaseEntity *pEntity );
// Particle effects (tracers, muzzle flashes) owned by pOwner: those on a player's
// first person gun only show in their own view, those on what the viewer can't
// see of themselves (their gun in their hand) don't show in theirs
bool GESS_ShouldHideParticlesForCurrentView( C_BaseEntity *pOwner );
// The view being drawn belongs to a dead player whose death blood covers it:
// light beams, glow sprites and light glows (very bright, drawn without depth
// testing) shone through the blood, so they're left out of that view
bool GESS_ShouldHideLightsForCurrentView();
C_BaseEntity *GESS_GetRenderViewer();
// True in a split screen game: GoldenEye's own team / character menus stay closed
bool GESS_ShouldSuppressGameMenus();
// The Split Screen menu is open (it reads the controllers itself)
bool GESS_IsSplitScreenMenuOpen();
// A split screen guest of this client (their gun is drawn with their viewmodel)
bool GESS_IsLocalGuest( C_BaseEntity *pEntity );
// The camera a player's view was last drawn with (split screen only). Viewmodel
// attachments (muzzle flashes, shells, tracers) are worked out in this view.
bool GESS_GetPlayerViewSetup( C_BaseEntity *pPlayer, Vector &origin, QAngle &angles, float &fov, float &fovViewmodel );
// Guests' guns are placed with player 1's, before entities simulate (ges_ss_vm_early)
bool GESS_PlaceGuestViewModelsEarly();
// Testing (ges_ss_debug_vm): log where a player's first person gun is relative to their view
bool GESS_DebugViewModelActive();
void GESS_DebugViewModel( const char *pszStage, C_BaseEntity *pViewModel );

#endif // GE_SPLITSCREEN_CLIENT_H
