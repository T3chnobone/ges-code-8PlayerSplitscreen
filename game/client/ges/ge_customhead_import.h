///////////// Copyright 2026 GoldenEye: Source. All rights reserved. /////////////
//
//   Project     : Client (GES)
//   File        : ge_customhead_import.h
//   Description :
//      Imports a player's own head for the Custom character.
//
//      Source can only draw compiled models, so a picked file becomes one in
//      the mod's models/ges_custom_heads folder:
//        * .obj (with its .mtl and textures: png, jpg, tga, bmp, or one image
//          picked for it, GEHeadFit_t::texture): the mesh is
//          fitted on the head bone, the textures become VTF + VMT, and the SDK's
//          studiomdl.exe (next to the game's engine) compiles it. It runs in the
//          background; poll GECustomHead_PollImport().
//        * .mdl (already compiled, with its .vvd / .vtx next to it): copied in.
//          It needs a ValveBiped.Bip01_Head1 bone; a whole character model works
//          too (only its head is shown). Its materials must already be installed.
//
//      cfg/ges_custom_heads.txt remembers each head's source file and fit, so it
//      can be fitted again, and which compiled version is current.
//
////////////////////////////////////////////////////////////////////////////
#ifndef GE_CUSTOMHEAD_IMPORT_H
#define GE_CUSTOMHEAD_IMPORT_H
#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"
#include "utlstring.h"

// How an .obj head is placed on the head bone
struct GEHeadFit_t
{
	GEHeadFit_t() { scale = 1.0f; height = 0.0f; forward = 0.0f; turn = 0; zUp = false; texture[0] = '\0'; }

	float	scale;		// 1 = the usual head size (the mesh is sized to fit first)
	float	height;		// Units up / down
	float	forward;	// Units forward / back
	int		turn;		// Quarter turns to the left (0-3)
	bool	zUp;		// The file is Z up (most are Y up, like Blender's default .obj export)
	char	texture[260];	// Image put on the whole head instead of the .mtl's textures ("" = the .mtl's).
							// A mesh without UVs gets them projected around the head, the image's
							// middle on the face.
};

enum GEHeadImportState_t
{
	GE_HEADIMPORT_IDLE = 0,
	GE_HEADIMPORT_RUNNING,		// studiomdl is compiling
	GE_HEADIMPORT_DONE,
	GE_HEADIMPORT_FAILED,
};

// Starts importing srcPath (an absolute path to an .obj or .mdl). False (and why
// in err) when it can't start. An .mdl finishes right away (poll says DONE).
bool GECustomHead_StartImport( const char *srcPath, const GEHeadFit_t &fit, char *err, int errLen );

// State of the last import; when DONE, headName is the name to use in a custom
// spec ("@<name>"), when FAILED msg says why
GEHeadImportState_t GECustomHead_PollImport( char *headName, int nameLen, char *msg, int msgLen );

// Names of the imported heads on this PC
void GECustomHead_ListImported( CUtlVector<CUtlString> &names );

// The source file and fit an imported head was made with (false for unknown
// or copied .mdl heads, which can't be refitted)
bool GECustomHead_GetImportInfo( const char *headName, char *srcPath, int srcLen, GEHeadFit_t &fit );

// File name (without folder / extension) of a head's current compiled model
void GECustomHead_ResolveModelName( const char *headName, char *out, int len );

#endif // GE_CUSTOMHEAD_IMPORT_H
