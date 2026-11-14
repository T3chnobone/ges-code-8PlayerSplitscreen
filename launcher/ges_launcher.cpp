///////////// Copyright 2026 GoldenEye: Source. All rights reserved. /////////////
//
//   Project     : Launcher (GES)
//   File        : ges_launcher.cpp
//   Description :
//      GoldenEyeSource.exe: starts GoldenEye: Source like a standalone game.
//
//      GES is a Source SDK 2013 mod, so it runs inside Valve's free "Source SDK
//      Base 2013 Multiplayer" (Steam app 243750). Valve's engine can't be
//      shipped with the mod, so this launcher finds it and starts it with the
//      mod loaded, the way "Play" in the Steam library does:
//          hl2.exe -steam -game "<GES folder>" [your arguments]
//
//      The GES folder is the folder this exe is in when it holds gameinfo.txt,
//      otherwise <Steam>/steamapps/sourcemods/gesource.
//
//      Build (MinGW):  x86_64-w64-mingw32-g++ -O2 -municode -mwindows -static
//                      ges_launcher.cpp -o GoldenEyeSource.exe -lshlwapi
//      Build (MSVC):   cl /O2 /EHsc ges_launcher.cpp /link /SUBSYSTEM:WINDOWS
//                      shell32.lib user32.lib advapi32.lib shlwapi.lib
//
////////////////////////////////////////////////////////////////////////////

#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

#ifdef _MSC_VER
#pragma comment( lib, "shlwapi.lib" )
#pragma comment( lib, "shell32.lib" )
#pragma comment( lib, "advapi32.lib" )
#pragma comment( lib, "user32.lib" )
#endif

static const wchar_t *TITLE = L"GoldenEye: Source";
static const wchar_t *SDK_FOLDER = L"Source SDK Base 2013 Multiplayer";
static const wchar_t *SDK_APPID = L"243750";
static const wchar_t *MOD_FOLDER = L"gesource";

static bool FileExists( const std::wstring &path )
{
	DWORD attr = GetFileAttributesW( path.c_str() );
	return attr != INVALID_FILE_ATTRIBUTES && !( attr & FILE_ATTRIBUTE_DIRECTORY );
}

static std::wstring Join( const std::wstring &a, const std::wstring &b )
{
	if ( a.empty() )
		return b;
	wchar_t last = a[a.size() - 1];
	return ( last == L'\\' || last == L'/' ) ? a + b : a + L"\\" + b;
}

static std::wstring Normalize( std::wstring path )
{
	for ( size_t i = 0; i < path.size(); i++ )
	{
		if ( path[i] == L'/' )
			path[i] = L'\\';
	}
	// libraryfolders.vdf escapes backslashes
	std::wstring out;
	for ( size_t i = 0; i < path.size(); i++ )
	{
		if ( path[i] == L'\\' && i + 1 < path.size() && path[i + 1] == L'\\' )
			continue;
		out += path[i];
	}
	return out;
}

static std::wstring ReadSteamPath()
{
	wchar_t buf[MAX_PATH] = L"";
	DWORD size = sizeof( buf );
	if ( RegGetValueW( HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath", RRF_RT_REG_SZ, NULL, buf, &size ) == ERROR_SUCCESS )
		return Normalize( buf );

	size = sizeof( buf );
	if ( RegGetValueW( HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath", RRF_RT_REG_SZ, NULL, buf, &size ) == ERROR_SUCCESS )
		return Normalize( buf );

	return L"";
}

// Steam library folders: the Steam folder itself plus every "path" in libraryfolders.vdf
static std::vector<std::wstring> GetLibraryFolders( const std::wstring &steam )
{
	std::vector<std::wstring> libs;
	if ( steam.empty() )
		return libs;
	libs.push_back( steam );

	std::ifstream file( Join( steam, L"steamapps\\libraryfolders.vdf" ).c_str() );
	std::string line;
	while ( std::getline( file, line ) )
	{
		// Lines look like:   "path"   "D:\\SteamLibrary"
		size_t key = line.find( "\"path\"" );
		if ( key == std::string::npos )
			continue;
		size_t q1 = line.find( '"', key + 6 );
		size_t q2 = q1 == std::string::npos ? q1 : line.find( '"', q1 + 1 );
		if ( q2 == std::string::npos )
			continue;

		std::string value = line.substr( q1 + 1, q2 - q1 - 1 );
		int len = MultiByteToWideChar( CP_UTF8, 0, value.c_str(), -1, NULL, 0 );
		std::wstring wide( len, L'\0' );
		MultiByteToWideChar( CP_UTF8, 0, value.c_str(), -1, &wide[0], len );
		wide.resize( len - 1 );
		libs.push_back( Normalize( wide ) );
	}
	return libs;
}

static std::wstring FindEngine( const std::vector<std::wstring> &libs, std::wstring &engineDir )
{
	for ( size_t i = 0; i < libs.size(); i++ )
	{
		std::wstring dir = Join( Join( libs[i], L"steamapps\\common" ), SDK_FOLDER );
		// hl2.exe is the classic 32-bit engine; newer SDK 2013 updates also ship a 64-bit one
		const wchar_t *exes[] = { L"hl2.exe", L"hl2_win64.exe" };
		for ( int e = 0; e < 2; e++ )
		{
			std::wstring exe = Join( dir, exes[e] );
			if ( FileExists( exe ) )
			{
				engineDir = dir;
				return exe;
			}
		}
	}
	return L"";
}

static std::wstring FindMod( const std::wstring &steam )
{
	// Next to this exe
	wchar_t self[MAX_PATH];
	GetModuleFileNameW( NULL, self, MAX_PATH );
	PathRemoveFileSpecW( self );
	if ( FileExists( Join( self, L"gameinfo.txt" ) ) )
		return self;

	std::wstring sub = Join( self, MOD_FOLDER );
	if ( FileExists( Join( sub, L"gameinfo.txt" ) ) )
		return sub;

	// The usual mod install location
	if ( !steam.empty() )
	{
		std::wstring mod = Join( Join( steam, L"steamapps\\sourcemods" ), MOD_FOLDER );
		if ( FileExists( Join( mod, L"gameinfo.txt" ) ) )
			return mod;
	}
	return L"";
}

// Arguments passed to this exe, passed on to the engine (e.g. -windowed -w 3840 -h 1080)
static std::wstring PassThroughArgs()
{
	const wchar_t *cmd = GetCommandLineW();
	bool quoted = false;
	// Skip our own exe name
	while ( *cmd && ( quoted || ( *cmd != L' ' && *cmd != L'\t' ) ) )
	{
		if ( *cmd == L'"' )
			quoted = !quoted;
		cmd++;
	}
	while ( *cmd == L' ' || *cmd == L'\t' )
		cmd++;
	return cmd;
}

int WINAPI wWinMain( HINSTANCE, HINSTANCE, LPWSTR, int )
{
	const std::wstring steam = ReadSteamPath();
	if ( steam.empty() )
	{
		MessageBoxW( NULL, L"Steam is needed to play GoldenEye: Source.\n\nInstall Steam from store.steampowered.com, sign in, then run this again.", TITLE, MB_ICONERROR );
		return 1;
	}

	std::wstring engineDir;
	const std::wstring engine = FindEngine( GetLibraryFolders( steam ), engineDir );
	if ( engine.empty() )
	{
		if ( MessageBoxW( NULL, L"GoldenEye: Source runs on \"Source SDK Base 2013 Multiplayer\", a free download from Steam.\n\nInstall it now?",
			TITLE, MB_ICONQUESTION | MB_YESNO ) == IDYES )
		{
			ShellExecuteW( NULL, L"open", ( std::wstring( L"steam://install/" ) + SDK_APPID ).c_str(), NULL, NULL, SW_SHOWNORMAL );
		}
		return 1;
	}

	const std::wstring mod = FindMod( steam );
	if ( mod.empty() )
	{
		MessageBoxW( NULL, L"Couldn't find the GoldenEye: Source files (gameinfo.txt).\n\nPut this exe in the gesource folder, or install the mod to Steam\\steamapps\\sourcemods\\gesource.", TITLE, MB_ICONERROR );
		return 1;
	}

	std::wstring cmdline = L"\"" + engine + L"\" -steam -game \"" + mod + L"\"";
	const std::wstring extra = PassThroughArgs();
	if ( !extra.empty() )
		cmdline += L" " + extra;

	STARTUPINFOW si = { sizeof( si ) };
	PROCESS_INFORMATION pi = {};
	std::vector<wchar_t> buf( cmdline.begin(), cmdline.end() );
	buf.push_back( L'\0' );

	if ( !CreateProcessW( engine.c_str(), &buf[0], NULL, NULL, FALSE, 0, NULL, engineDir.c_str(), &si, &pi ) )
	{
		std::wstringstream msg;
		msg << L"Couldn't start the game engine (error " << GetLastError() << L"):\n" << engine;
		MessageBoxW( NULL, msg.str().c_str(), TITLE, MB_ICONERROR );
		return 1;
	}

	CloseHandle( pi.hThread );
	CloseHandle( pi.hProcess );
	return 0;
}
