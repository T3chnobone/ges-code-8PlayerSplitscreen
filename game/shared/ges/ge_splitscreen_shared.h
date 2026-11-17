///////////// Copyright 2026 GoldenEye: Source. All rights reserved. /////////////
//
//   Project     : Shared (GES)
//   File        : ge_splitscreen_shared.h
//   Description :
//      Shared definitions for local split-screen multiplayer.
//
//      Source SDK 2013 only supports a single local player per client. Split
//      screen is implemented on top of that:
//        * Slot 0 is the normal local player (the "host" of the split session).
//        * Slots 1-7 are "guest" players. They are fake clients created on the
//          server and owned by the host client. Their input is gathered by the
//          host client from game controllers and sent to the server inside the
//          host's own CUserCmd stream (see CGESplitGuestCmd below).
//        * The host client renders one viewport per slot.
//
////////////////////////////////////////////////////////////////////////////

#ifndef GE_SPLITSCREEN_SHARED_H
#define GE_SPLITSCREEN_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#include "mathlib/vector.h"

// Total number of local players supported (host + guests)
#define GES_SS_MAX_PLAYERS		8
// Number of guest slots (everyone except the host)
#define GES_SS_MAX_GUESTS		(GES_SS_MAX_PLAYERS - 1)
// Players per screen (2x2 grid)
#define GES_SS_PLAYERS_PER_SCREEN 4

// Bits used to send the guest slot over the network
#define GES_SS_SLOT_BITS		3

// Angle reset handshake: the server bumps a serial when it forces a guest's view
// angles (spawn, teleport). The client echoes the serial back once it adopted them.
#define GES_SS_ANGLERESET_BITS	4
#define GES_SS_ANGLERESET_MASK	((1 << GES_SS_ANGLERESET_BITS) - 1)

// Server-side command names (sent by the host client)
#define GES_SS_CMD_JOIN			"ges_ss_join"
#define GES_SS_CMD_LEAVE		"ges_ss_leave"
#define GES_SS_CMD_SETCHAR		"ges_ss_update"	// <slot> <name> <character>
#define GES_SS_CMD_HOST			"ges_ss_host"		// <1 = split screen host, 0 = normal player> <character>
#define GES_SS_CMD_TEAM			"ges_ss_team"		// <slot> <team: TEAM_MI6, TEAM_JANUS or MAX_GE_TEAMS for auto>
#define GES_SS_CMD_SUICIDE		"ges_ss_suicide"	// <slot: 0 = player 1>: that player commits suicide (the START menu)

//-----------------------------------------------------------------------------
// Input for one guest player, carried inside the host's CUserCmd.
// Slot index into CUserCmd::splitguests is (guest slot - 1).
//-----------------------------------------------------------------------------
class CGESplitGuestCmd
{
public:
	CGESplitGuestCmd() { Reset(); }

	void Reset()
	{
		active = false;
		viewangles.Init();
		forwardmove = 0.0f;
		sidemove = 0.0f;
		buttons = 0;
		weaponcycle = 0;
		anglereset = 0;
	}

	bool operator==( const CGESplitGuestCmd &o ) const
	{
		return active == o.active && viewangles == o.viewangles && forwardmove == o.forwardmove &&
			sidemove == o.sidemove && buttons == o.buttons && weaponcycle == o.weaponcycle && anglereset == o.anglereset;
	}

	bool	active;			// Is this guest slot in use this command?
	QAngle	viewangles;		// Guest view angles (pitch, yaw; roll unused)
	float	forwardmove;
	float	sidemove;
	int		buttons;		// IN_* bits
	signed char weaponcycle;	// -1 previous weapon, +1 next weapon, 0 none (event, not state)
	unsigned char anglereset;	// Last angle reset serial the client has applied
};

#endif // GE_SPLITSCREEN_SHARED_H
