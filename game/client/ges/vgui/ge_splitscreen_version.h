//-----------------------------------------------------------------------------
// GoldenEye: Source - Split Screen Mod version.
//
// Bump GES_SS_VERSION with every release, to the same number as the GitHub
// release tag without the "v" (tag v0.3 -> "0.3"). The main menu shows it in
// the bottom right corner and compares it with the newest release on GitHub
// (see ge_splitscreen_version.cpp).
//-----------------------------------------------------------------------------
#ifndef GE_SPLITSCREEN_VERSION_H
#define GE_SPLITSCREEN_VERSION_H
#ifdef _WIN32
#pragma once
#endif

#define GES_SS_VERSION			"0.2"

// GitHub API: the newest published release (drafts and pre-releases are skipped)
#define GES_SS_LATEST_RELEASE_API	"https://api.github.com/repos/T3chnobone/ges-code-8PlayerSplitscreen/releases/latest"
// Opened when the player clicks "Update available"
#define GES_SS_RELEASES_URL		"https://github.com/T3chnobone/ges-code-8PlayerSplitscreen/releases/latest"

#endif // GE_SPLITSCREEN_VERSION_H
