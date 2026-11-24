///////////// Copyright 2026 GoldenEye: Source. All rights reserved. /////////////
//
//   Project     : Client (GES)
//   File        : ge_customhead_import.cpp
//   Description :
//      see ge_customhead_import.h
//
////////////////////////////////////////////////////////////////////////////
#include "cbase.h"

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#undef CreateEvent
	#undef GetObject
	#undef PlaySound
#endif

#include "ge_customhead_import.h"
#include "ge_customchar.h"
#include "filesystem.h"
#include "utlbuffer.h"
#include "KeyValues.h"
#include "studio.h"
#include "vtf/vtf.h"
#include "bitmap/imageformat.h"

// Image decoding (png / jpg / tga / bmp), public domain: thirdparty/stb/stb_image.h
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_TGA
#define STBI_ONLY_BMP
#define STBI_NO_STDIO
#define STBI_NO_SIMD
#define STBI_NO_FAILURE_STRINGS
#define STBI_ASSERT( x )
#include "stb/stb_image.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define GE_HEADS_MANIFEST		"cfg/ges_custom_heads.txt"
#define GE_HEAD_TARGET_HEIGHT	11.0f		// Units from chin (or neck) to the top of the head
#define GE_HEAD_MAX_TEXTURE		1024

static ConVar ge_customhead_studiomdl( "ge_customhead_studiomdl", "", FCVAR_ARCHIVE, "Full path of studiomdl.exe for importing custom heads (empty: the one next to the game's engine)" );

//-----------------------------------------------------------------------------
// Manifest: cfg/ges_custom_heads.txt
//   "CustomHeads" { "<head name>" { "source" "<path>" "model" "<file>" "version" "3" "scale" ... } }
//-----------------------------------------------------------------------------
static KeyValues *s_pManifest = NULL;

static KeyValues *Manifest()
{
	if ( !s_pManifest )
	{
		s_pManifest = new KeyValues( "CustomHeads" );
		s_pManifest->LoadFromFile( filesystem, GE_HEADS_MANIFEST, "MOD" );
	}
	return s_pManifest;
}

static void SaveManifest()
{
	Manifest()->SaveToFile( filesystem, GE_HEADS_MANIFEST, "MOD" );
}

void GECustomHead_ResolveModelName( const char *headName, char *out, int len )
{
	char clean[GE_CUSTOMHEAD_NAME_LEN];
	GECustomChar_CleanHeadName( headName, clean, sizeof( clean ) );
	KeyValues *pHead = Manifest()->FindKey( clean );
	Q_strncpy( out, ( pHead && pHead->GetString( "model" )[0] ) ? pHead->GetString( "model" ) : clean, len );
}

bool GECustomHead_GetImportInfo( const char *headName, char *srcPath, int srcLen, GEHeadFit_t &fit )
{
	KeyValues *pHead = Manifest()->FindKey( headName );
	const char *ext = pHead ? V_GetFileExtension( pHead->GetString( "source" ) ) : NULL;
	if ( !ext || Q_stricmp( ext, "obj" ) )
		return false;

	Q_strncpy( srcPath, pHead->GetString( "source" ), srcLen );
	fit.scale = pHead->GetFloat( "scale", 1.0f );
	fit.height = pHead->GetFloat( "height", 0.0f );
	fit.forward = pHead->GetFloat( "forward", 0.0f );
	fit.turn = pHead->GetInt( "turn", 0 ) & 3;
	fit.zUp = pHead->GetInt( "z_up", 0 ) != 0;
	return true;
}

void GECustomHead_ListImported( CUtlVector<CUtlString> &names )
{
	names.RemoveAll();

	// From the manifest
	for ( KeyValues *pHead = Manifest()->GetFirstTrueSubKey(); pHead; pHead = pHead->GetNextTrueSubKey() )
	{
		char path[MAX_PATH];
		Q_snprintf( path, sizeof( path ), "%s/%s.mdl", GE_CUSTOMHEAD_DIR, pHead->GetString( "model", pHead->GetName() ) );
		if ( filesystem->FileExists( path, "GAME" ) )
			names.AddToTail( pHead->GetName() );
	}

	// And any .mdl someone put in the folder by hand
	FileFindHandle_t hFind;
	const char *pszFile = filesystem->FindFirstEx( GE_CUSTOMHEAD_DIR "/*.mdl", "GAME", &hFind );
	while ( pszFile )
	{
		char name[GE_CUSTOMHEAD_NAME_LEN];
		char base[MAX_PATH];
		V_FileBase( pszFile, base, sizeof( base ) );
		GECustomChar_CleanHeadName( base, name, sizeof( name ) );

		// Usable by name (no spaces etc.), and not a compiled version of a head above
		bool bSkip = Q_stricmp( name, base ) != 0;
		for ( KeyValues *pHead = Manifest()->GetFirstTrueSubKey(); pHead && !bSkip; pHead = pHead->GetNextTrueSubKey() )
			bSkip = !Q_stricmp( pHead->GetString( "model", pHead->GetName() ), base ) || !Q_stricmp( pHead->GetName(), name );
		if ( !bSkip )
			names.AddToTail( name );

		pszFile = filesystem->FindNext( hFind );
	}
	filesystem->FindClose( hFind );
}

//-----------------------------------------------------------------------------
// Small helpers
//-----------------------------------------------------------------------------
static void SetMsg( char *msg, int len, const char *fmt, ... )
{
	if ( !msg || len <= 0 )
		return;
	va_list args;
	va_start( args, fmt );
	Q_vsnprintf( msg, len, fmt, args );
	va_end( args );
}

static void GameDirPath( const char *relative, char *out, int len )
{
	Q_snprintf( out, len, "%s/%s", engine->GetGameDirectory(), relative );
	Q_FixSlashes( out );
}

static bool WriteText( const char *absPath, const char *text )
{
	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	buf.PutString( text );
	return filesystem->WriteFile( absPath, NULL, buf );
}

// Next token on a line (whitespace separated)
static const char *NextToken( const char *p, char *tok, int len )
{
	while ( *p == ' ' || *p == '\t' )
		p++;
	int n = 0;
	while ( *p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' )
	{
		if ( n < len - 1 )
			tok[n++] = *p;
		p++;
	}
	tok[n] = '\0';
	return p;
}

// The rest of a line, trimmed
static void RestOfLine( const char *p, char *out, int len )
{
	while ( *p == ' ' || *p == '\t' )
		p++;
	Q_strncpy( out, p, len );
	int n = Q_strlen( out );
	while ( n > 0 && ( out[n - 1] == ' ' || out[n - 1] == '\t' || out[n - 1] == '\r' || out[n - 1] == '\n' ) )
		out[--n] = '\0';
}

//-----------------------------------------------------------------------------
// OBJ / MTL
//-----------------------------------------------------------------------------
struct ObjVert_t
{
	int v, t, n;	// 0-based, -1 = none
};

struct ObjMaterial_t
{
	CUtlString name;		// Clean name, also the texture / material file name
	CUtlString texture;		// Absolute path of map_Kd ("" = none)
	Vector color;			// Kd
	CUtlVector<ObjVert_t> tris;	// 3 per triangle
};

struct ObjMesh_t
{
	CUtlVector<Vector> pos;
	CUtlVector<Vector2D> uv;
	CUtlVector<Vector> nrm;
	CUtlVector<ObjMaterial_t*> mats;

	~ObjMesh_t() { mats.PurgeAndDeleteElements(); }

	int FindMat( const char *name )
	{
		for ( int i = 0; i < mats.Count(); i++ )
		{
			if ( !Q_stricmp( mats[i]->name.Get(), name ) )
				return i;
		}
		return -1;
	}

	int AddMat( const char *rawName )
	{
		char clean[64];
		GECustomChar_CleanHeadName( rawName, clean, sizeof( clean ) );
		if ( !clean[0] )
			Q_strncpy( clean, "head", sizeof( clean ) );
		int i = FindMat( clean );
		if ( i >= 0 )
			return i;
		ObjMaterial_t *pMat = new ObjMaterial_t;
		pMat->name = clean;
		pMat->color.Init( 0.8f, 0.8f, 0.8f );
		return mats.AddToTail( pMat );
	}
};

static int ObjIndex( int i, int count )
{
	// 1-based, or negative = counted back from the end
	if ( i > 0 )
		return i - 1 < count ? i - 1 : -1;
	if ( i < 0 )
		return count + i >= 0 ? count + i : -1;
	return -1;
}

static void ParseMtl( const char *mtlPath, ObjMesh_t &mesh )
{
	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( !filesystem->ReadFile( mtlPath, NULL, buf ) )
		return;
	buf.PutChar( '\0' );

	char dir[MAX_PATH];
	Q_ExtractFilePath( mtlPath, dir, sizeof( dir ) );

	ObjMaterial_t *pCur = NULL;
	const char *p = (const char *)buf.Base();
	while ( *p )
	{
		const char *lineEnd = p;
		while ( *lineEnd && *lineEnd != '\n' )
			lineEnd++;
		char line[1024];
		Q_strncpy( line, p, Min( (int)sizeof( line ), (int)( lineEnd - p ) + 1 ) );
		p = *lineEnd ? lineEnd + 1 : lineEnd;

		char tok[64];
		const char *rest = NextToken( line, tok, sizeof( tok ) );
		if ( !Q_stricmp( tok, "newmtl" ) )
		{
			char name[256];
			RestOfLine( rest, name, sizeof( name ) );
			pCur = mesh.mats[mesh.AddMat( name )];
		}
		else if ( pCur && !Q_stricmp( tok, "Kd" ) )
		{
			float r = 0.8f, g = 0.8f, b = 0.8f;
			sscanf( rest, "%f %f %f", &r, &g, &b );
			pCur->color.Init( r, g, b );
		}
		else if ( pCur && !Q_stricmp( tok, "map_Kd" ) )
		{
			// Options (-s 1 1 1 etc.) come first: the file name is the last part.
			// Names with spaces: try the whole rest first.
			char rel[512];
			RestOfLine( rest, rel, sizeof( rel ) );
			char full[MAX_PATH];
			if ( V_IsAbsolutePath( rel ) )
				Q_strncpy( full, rel, sizeof( full ) );
			else
				Q_snprintf( full, sizeof( full ), "%s%s", dir, rel );
			Q_FixSlashes( full );
			if ( !filesystem->FileExists( full, NULL ) )
			{
				const char *last = Q_strrchr( rel, ' ' );
				if ( last )
				{
					Q_snprintf( full, sizeof( full ), "%s%s", V_IsAbsolutePath( last + 1 ) ? "" : dir, last + 1 );
					Q_FixSlashes( full );
				}
			}
			pCur->texture = full;
		}
	}
}

static bool ParseObj( const char *objPath, ObjMesh_t &mesh, char *err, int errLen )
{
	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( !filesystem->ReadFile( objPath, NULL, buf ) )
	{
		SetMsg( err, errLen, "Couldn't read %s", objPath );
		return false;
	}
	buf.PutChar( '\0' );

	char dir[MAX_PATH];
	Q_ExtractFilePath( objPath, dir, sizeof( dir ) );

	int curMat = -1;
	const char *p = (const char *)buf.Base();
	CUtlVector<ObjVert_t> face;
	while ( *p )
	{
		const char *lineEnd = p;
		while ( *lineEnd && *lineEnd != '\n' )
			lineEnd++;
		char line[4096];
		Q_strncpy( line, p, Min( (int)sizeof( line ), (int)( lineEnd - p ) + 1 ) );
		p = *lineEnd ? lineEnd + 1 : lineEnd;

		char tok[64];
		const char *rest = NextToken( line, tok, sizeof( tok ) );
		if ( !Q_strcmp( tok, "v" ) )
		{
			Vector v( 0, 0, 0 );
			sscanf( rest, "%f %f %f", &v.x, &v.y, &v.z );
			mesh.pos.AddToTail( v );
		}
		else if ( !Q_strcmp( tok, "vt" ) )
		{
			Vector2D t( 0, 0 );
			sscanf( rest, "%f %f", &t.x, &t.y );
			mesh.uv.AddToTail( t );
		}
		else if ( !Q_strcmp( tok, "vn" ) )
		{
			Vector n( 0, 0, 1 );
			sscanf( rest, "%f %f %f", &n.x, &n.y, &n.z );
			mesh.nrm.AddToTail( n );
		}
		else if ( !Q_strcmp( tok, "mtllib" ) )
		{
			char rel[512], full[MAX_PATH];
			RestOfLine( rest, rel, sizeof( rel ) );
			Q_snprintf( full, sizeof( full ), "%s%s", V_IsAbsolutePath( rel ) ? "" : dir, rel );
			Q_FixSlashes( full );
			ParseMtl( full, mesh );
		}
		else if ( !Q_strcmp( tok, "usemtl" ) )
		{
			char name[256];
			RestOfLine( rest, name, sizeof( name ) );
			curMat = mesh.AddMat( name );
		}
		else if ( !Q_strcmp( tok, "f" ) )
		{
			face.RemoveAll();
			char vtx[128];
			const char *q = rest;
			for ( ;; )
			{
				q = NextToken( q, vtx, sizeof( vtx ) );
				if ( !vtx[0] )
					break;
				int vi = 0, ti = 0, ni = 0;
				// v, v/t, v//n, v/t/n
				vi = atoi( vtx );
				const char *s1 = strchr( vtx, '/' );
				if ( s1 )
				{
					if ( s1[1] != '/' )
						ti = atoi( s1 + 1 );
					const char *s2 = strchr( s1 + 1, '/' );
					if ( s2 )
						ni = atoi( s2 + 1 );
				}
				ObjVert_t ov;
				ov.v = ObjIndex( vi, mesh.pos.Count() );
				ov.t = ObjIndex( ti, mesh.uv.Count() );
				ov.n = ObjIndex( ni, mesh.nrm.Count() );
				if ( ov.v >= 0 )
					face.AddToTail( ov );
			}

			if ( face.Count() >= 3 )
			{
				if ( curMat < 0 )
					curMat = mesh.AddMat( "head" );
				ObjMaterial_t *pMat = mesh.mats[curMat];
				// Fan
				for ( int k = 1; k + 1 < face.Count(); k++ )
				{
					pMat->tris.AddToTail( face[0] );
					pMat->tris.AddToTail( face[k] );
					pMat->tris.AddToTail( face[k + 1] );
				}
			}
		}
	}

	// Drop materials nobody uses
	for ( int i = mesh.mats.Count() - 1; i >= 0; i-- )
	{
		if ( mesh.mats[i]->tris.Count() == 0 )
		{
			delete mesh.mats[i];
			mesh.mats.Remove( i );
		}
	}

	if ( mesh.mats.Count() == 0 || mesh.pos.Count() == 0 )
	{
		SetMsg( err, errLen, "%s has no faces", objPath );
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Textures: any image -> power of two RGBA -> VTF
//-----------------------------------------------------------------------------
static int PowerOfTwoBelow( int v )
{
	int p = 1;
	while ( p * 2 <= v && p * 2 <= GE_HEAD_MAX_TEXTURE )
		p *= 2;
	return p;
}

static void ResampleRGBA( const unsigned char *src, int sw, int sh, unsigned char *dst, int dw, int dh )
{
	for ( int y = 0; y < dh; y++ )
	{
		const float fy = ( y + 0.5f ) * sh / dh - 0.5f;
		const int y0 = clamp( (int)floorf( fy ), 0, sh - 1 ), y1 = Min( y0 + 1, sh - 1 );
		const float ty = clamp( fy - y0, 0.0f, 1.0f );
		for ( int x = 0; x < dw; x++ )
		{
			const float fx = ( x + 0.5f ) * sw / dw - 0.5f;
			const int x0 = clamp( (int)floorf( fx ), 0, sw - 1 ), x1 = Min( x0 + 1, sw - 1 );
			const float tx = clamp( fx - x0, 0.0f, 1.0f );
			for ( int c = 0; c < 4; c++ )
			{
				const float a = src[( y0 * sw + x0 ) * 4 + c] * ( 1 - tx ) + src[( y0 * sw + x1 ) * 4 + c] * tx;
				const float b = src[( y1 * sw + x0 ) * 4 + c] * ( 1 - tx ) + src[( y1 * sw + x1 ) * 4 + c] * tx;
				dst[( y * dw + x ) * 4 + c] = (unsigned char)clamp( (int)( a * ( 1 - ty ) + b * ty + 0.5f ), 0, 255 );
			}
		}
	}
}

// Writes <absDir>/<name>.vtf. bHasAlpha tells whether any pixel is see-through.
static bool WriteVTF( const char *absPath, const unsigned char *rgba, int w, int h, bool &bHasAlpha )
{
	const int dw = PowerOfTwoBelow( w ), dh = PowerOfTwoBelow( h );
	CUtlVector<unsigned char> resized;
	if ( dw != w || dh != h )
	{
		resized.SetCount( dw * dh * 4 );
		ResampleRGBA( rgba, w, h, resized.Base(), dw, dh );
		rgba = resized.Base();
	}

	bHasAlpha = false;
	for ( int i = 0; i < dw * dh && !bHasAlpha; i++ )
		bHasAlpha = rgba[i * 4 + 3] < 250;

	IVTFTexture *pTex = CreateVTFTexture();
	if ( !pTex->Init( dw, dh, 1, IMAGE_FORMAT_RGBA8888, bHasAlpha ? TEXTUREFLAGS_EIGHTBITALPHA : 0, 1 ) )
	{
		DestroyVTFTexture( pTex );
		return false;
	}
	memcpy( pTex->ImageData( 0, 0, 0 ), rgba, dw * dh * 4 );
	pTex->GenerateMipmaps();
	pTex->ConvertImageFormat( bHasAlpha ? IMAGE_FORMAT_BGRA8888 : IMAGE_FORMAT_BGR888, false );

	CUtlBuffer buf;
	const bool bOk = pTex->Serialize( buf ) && filesystem->WriteFile( absPath, NULL, buf );
	DestroyVTFTexture( pTex );
	return bOk;
}

static bool MakeTexture( const ObjMaterial_t &mat, const char *absVtf, bool &bHasAlpha, char *err, int errLen )
{
	bHasAlpha = false;

	if ( mat.texture.Length() > 0 )
	{
		CUtlBuffer file;
		if ( filesystem->ReadFile( mat.texture.Get(), NULL, file ) )
		{
			int w = 0, h = 0, comp = 0;
			unsigned char *pixels = stbi_load_from_memory( (const unsigned char *)file.Base(), file.TellPut(), &w, &h, &comp, 4 );
			if ( pixels && w > 0 && h > 0 )
			{
				const bool bOk = WriteVTF( absVtf, pixels, w, h, bHasAlpha );
				stbi_image_free( pixels );
				if ( !bOk )
					SetMsg( err, errLen, "Couldn't write texture %s", absVtf );
				return bOk;
			}
			if ( pixels )
				stbi_image_free( pixels );
			Warning( "Custom head: can't read image %s (png, jpg, tga and bmp work), using the material colour\n", mat.texture.Get() );
		}
		else
		{
			Warning( "Custom head: texture %s not found, using the material colour\n", mat.texture.Get() );
		}
	}

	// No (readable) texture: the material's colour
	unsigned char solid[4 * 4 * 4];
	for ( int i = 0; i < 16; i++ )
	{
		solid[i * 4 + 0] = (unsigned char)clamp( (int)( mat.color.x * 255 ), 0, 255 );
		solid[i * 4 + 1] = (unsigned char)clamp( (int)( mat.color.y * 255 ), 0, 255 );
		solid[i * 4 + 2] = (unsigned char)clamp( (int)( mat.color.z * 255 ), 0, 255 );
		solid[i * 4 + 3] = 255;
	}
	if ( !WriteVTF( absVtf, solid, 4, 4, bHasAlpha ) )
	{
		SetMsg( err, errLen, "Couldn't write texture %s", absVtf );
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Where the head bone is in a character's reference pose
//-----------------------------------------------------------------------------
static bool GetReferenceHeadBone( matrix3x4_t &poseToBone, Vector &headPos )
{
	for ( int i = 0; i < GECharacters()->Count(); i++ )
	{
		const CGECharData *pChar = GECharacters()->Get( i );
		if ( !pChar || pChar == GECharacters()->GetRandomChar() || pChar->m_pSkins.Count() == 0 || !pChar->m_pSkins[0]->szModel[0] )
			continue;

		const int idx = modelinfo->GetModelIndex( pChar->m_pSkins[0]->szModel );
		const model_t *pModel = idx != -1 ? modelinfo->GetModel( idx ) : NULL;
		studiohdr_t *pHdr = pModel ? modelinfo->GetStudiomodel( pModel ) : NULL;
		if ( !pHdr )
			continue;

		for ( int b = 0; b < pHdr->numbones; b++ )
		{
			if ( !Q_stricmp( pHdr->pBone( b )->pszName(), GE_CUSTOMHEAD_BONE ) )
			{
				poseToBone = pHdr->pBone( b )->poseToBone;
				matrix3x4_t boneToPose;
				MatrixInvert( poseToBone, boneToPose );
				MatrixGetColumn( boneToPose, 3, headPos );
				return true;
			}
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
// The running compile
//-----------------------------------------------------------------------------
static GEHeadImportState_t s_ImportState = GE_HEADIMPORT_IDLE;
static char s_szImportHead[GE_CUSTOMHEAD_NAME_LEN];
static char s_szImportModel[GE_CUSTOMHEAD_NAME_LEN];
static char s_szImportMsg[512];
static char s_szImportLog[MAX_PATH];
static int s_iImportVersion;
#ifdef _WIN32
static HANDLE s_hImportProcess = NULL;
#endif

static bool FindStudioMdl( char *out, int len )
{
	if ( ge_customhead_studiomdl.GetString()[0] )
	{
		Q_strncpy( out, ge_customhead_studiomdl.GetString(), len );
		return filesystem->FileExists( out, NULL );
	}

#ifdef _WIN32
	// The SDK's bin folder, next to engine.dll
	char engine[MAX_PATH];
	HMODULE hEngine = GetModuleHandleA( "engine.dll" );
	if ( hEngine && GetModuleFileNameA( hEngine, engine, sizeof( engine ) ) )
	{
		char dir[MAX_PATH];
		Q_ExtractFilePath( engine, dir, sizeof( dir ) );
		Q_snprintf( out, len, "%sstudiomdl.exe", dir );
		if ( filesystem->FileExists( out, NULL ) )
			return true;
	}
#endif
	return false;
}

// Removes a compiled head model (all its files) and its materials folder
static void RemoveCompiledHead( const char *modelName )
{
	static const char *s_Exts[] = { ".mdl", ".vvd", ".dx90.vtx", ".dx80.vtx", ".sw.vtx", ".phy", ".vtx" };
	for ( int i = 0; i < ARRAYSIZE( s_Exts ); i++ )
	{
		char rel[MAX_PATH];
		Q_snprintf( rel, sizeof( rel ), "%s/%s%s", GE_CUSTOMHEAD_DIR, modelName, s_Exts[i] );
		if ( filesystem->FileExists( rel, "MOD" ) )
			filesystem->RemoveFile( rel, "MOD" );
	}

	char matDir[MAX_PATH];
	Q_snprintf( matDir, sizeof( matDir ), "materials/%s/%s/*.*", GE_CUSTOMHEAD_DIR, modelName );
	FileFindHandle_t hFind;
	CUtlVector<CUtlString> files;
	for ( const char *f = filesystem->FindFirstEx( matDir, "MOD", &hFind ); f; f = filesystem->FindNext( hFind ) )
	{
		if ( f[0] != '.' )
			files.AddToTail( f );
	}
	filesystem->FindClose( hFind );
	for ( int i = 0; i < files.Count(); i++ )
	{
		char rel[MAX_PATH];
		Q_snprintf( rel, sizeof( rel ), "materials/%s/%s/%s", GE_CUSTOMHEAD_DIR, modelName, files[i].Get() );
		filesystem->RemoveFile( rel, "MOD" );
	}
}

static bool StartCompile( const char *qcAbs, char *err, int errLen )
{
#ifdef _WIN32
	char studiomdl[MAX_PATH];
	if ( !FindStudioMdl( studiomdl, sizeof( studiomdl ) ) )
	{
		SetMsg( err, errLen, "studiomdl.exe wasn't found next to the game's engine.dll (set ge_customhead_studiomdl to its full path)" );
		return false;
	}

	char gameDir[MAX_PATH];
	Q_strncpy( gameDir, engine->GetGameDirectory(), sizeof( gameDir ) );
	Q_FixSlashes( gameDir );

	char cmd[2048];
	Q_snprintf( cmd, sizeof( cmd ), "\"%s\" -nop4 -game \"%s\" \"%s\"", studiomdl, gameDir, qcAbs );

	// Output to a log next to the .qc (shown when it fails)
	SECURITY_ATTRIBUTES sa = { sizeof( sa ), NULL, TRUE };
	HANDLE hLog = CreateFileA( s_szImportLog, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );

	STARTUPINFOA si;
	memset( &si, 0, sizeof( si ) );
	si.cb = sizeof( si );
	si.dwFlags = STARTF_USESHOWWINDOW | ( hLog != INVALID_HANDLE_VALUE ? STARTF_USESTDHANDLES : 0 );
	si.wShowWindow = SW_HIDE;
	si.hStdOutput = si.hStdError = hLog;
	si.hStdInput = NULL;

	PROCESS_INFORMATION pi;
	memset( &pi, 0, sizeof( pi ) );

	char workDir[MAX_PATH];
	Q_ExtractFilePath( studiomdl, workDir, sizeof( workDir ) );

	const BOOL bOk = CreateProcessA( NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, workDir, &si, &pi );
	if ( hLog != INVALID_HANDLE_VALUE )
		CloseHandle( hLog );
	if ( !bOk )
	{
		SetMsg( err, errLen, "Couldn't run %s", studiomdl );
		return false;
	}

	CloseHandle( pi.hThread );
	s_hImportProcess = pi.hProcess;
	return true;
#else
	SetMsg( err, errLen, "Importing .obj heads needs Windows (studiomdl.exe)" );
	return false;
#endif
}

static void RememberHead( const char *headName, const char *srcPath, const char *modelName, int version, const GEHeadFit_t *pFit )
{
	KeyValues *pHead = Manifest()->FindKey( headName, true );
	pHead->SetString( "source", srcPath );
	pHead->SetString( "model", modelName );
	pHead->SetInt( "version", version );
	if ( pFit )
	{
		pHead->SetFloat( "scale", pFit->scale );
		pHead->SetFloat( "height", pFit->height );
		pHead->SetFloat( "forward", pFit->forward );
		pHead->SetInt( "turn", pFit->turn & 3 );
		pHead->SetInt( "z_up", pFit->zUp ? 1 : 0 );
	}
	SaveManifest();
}

//-----------------------------------------------------------------------------
// .mdl: copy it in
//-----------------------------------------------------------------------------
static bool ImportCompiledMdl( const char *srcPath, const char *headName, char *err, int errLen )
{
	CUtlBuffer mdl;
	if ( !filesystem->ReadFile( srcPath, NULL, mdl ) || mdl.TellPut() < (int)sizeof( studiohdr_t ) )
	{
		SetMsg( err, errLen, "Couldn't read %s", srcPath );
		return false;
	}

	// Check it's a model with a head bone
	const studiohdr_t *pHdr = (const studiohdr_t *)mdl.Base();
	if ( pHdr->id != IDSTUDIOHEADER )
	{
		SetMsg( err, errLen, "%s isn't a Source model", srcPath );
		return false;
	}
	bool bHead = false;
	for ( int b = 0; b < pHdr->numbones && pHdr->boneindex + ( b + 1 ) * (int)sizeof( mstudiobone_t ) <= mdl.TellPut(); b++ )
	{
		const mstudiobone_t *pBone = pHdr->pBone( b );
		if ( !Q_stricmp( pBone->pszName(), GE_CUSTOMHEAD_BONE ) )
			bHead = true;
	}
	if ( !bHead )
	{
		SetMsg( err, errLen, "The model has no %s bone, so it can't go on a body", GE_CUSTOMHEAD_BONE );
		return false;
	}

	// Copy the model's files: same name, other extensions
	char base[MAX_PATH];
	Q_StripExtension( srcPath, base, sizeof( base ) );

	char destDir[MAX_PATH];
	GameDirPath( GE_CUSTOMHEAD_DIR, destDir, sizeof( destDir ) );
	filesystem->CreateDirHierarchy( destDir, NULL );

	RemoveCompiledHead( headName );

	static const char *s_Exts[] = { ".mdl", ".vvd", ".dx90.vtx", ".dx80.vtx", ".sw.vtx", ".phy", ".vtx" };
	for ( int i = 0; i < ARRAYSIZE( s_Exts ); i++ )
	{
		char src[MAX_PATH], dest[MAX_PATH];
		Q_snprintf( src, sizeof( src ), "%s%s", base, s_Exts[i] );
		Q_snprintf( dest, sizeof( dest ), "%s/%s%s", destDir, headName, s_Exts[i] );
		Q_FixSlashes( dest );
		CUtlBuffer file;
		if ( filesystem->ReadFile( src, NULL, file ) )
			filesystem->WriteFile( dest, NULL, file );
		else if ( i < 2 )
		{
			SetMsg( err, errLen, "%s is missing (a compiled model needs its .vvd and .vtx files next to it)", src );
			return false;
		}
	}

	RememberHead( headName, srcPath, headName, 0, NULL );
	return true;
}

//-----------------------------------------------------------------------------
// .obj: fit, textures, SMD + QC, studiomdl
//-----------------------------------------------------------------------------
static bool ImportObj( const char *srcPath, const char *headName, const GEHeadFit_t &fit, char *err, int errLen )
{
	ObjMesh_t mesh;
	if ( !ParseObj( srcPath, mesh, err, errLen ) )
		return false;

	matrix3x4_t poseToBone;
	Vector headPos;
	if ( !GetReferenceHeadBone( poseToBone, headPos ) )
	{
		SetMsg( err, errLen, "Couldn't load a character model to fit the head to" );
		return false;
	}

	// The file's axes -> the game's (X forward, Y left, Z up). A Y up .obj faces +Z
	// (Blender's default export), a Z up one faces -Y.
	CUtlVector<Vector> pos;
	pos.SetCount( mesh.pos.Count() );
	const float turnRad = DEG2RAD( 90.0f * ( fit.turn & 3 ) );
	const float cs = cosf( turnRad ), sn = sinf( turnRad );
	Vector mins( FLT_MAX, FLT_MAX, FLT_MAX ), maxs( -FLT_MAX, -FLT_MAX, -FLT_MAX );
	for ( int i = 0; i < mesh.pos.Count(); i++ )
	{
		const Vector &o = mesh.pos[i];
		Vector v = fit.zUp ? Vector( -o.y, o.x, o.z ) : Vector( o.z, o.x, o.y );
		v = Vector( v.x * cs - v.y * sn, v.x * sn + v.y * cs, v.z );
		pos[i] = v;
		VectorMin( mins, v, mins );
		VectorMax( maxs, v, maxs );
	}

	// Size it like a head and sit it on the neck: the bottom just under the head
	// bone (at the top of the neck), centred a little in front of it
	const float height = Max( maxs.z - mins.z, 0.001f );
	const float scale = GE_HEAD_TARGET_HEIGHT * clamp( fit.scale, 0.1f, 10.0f ) / height;
	const Vector center( ( mins.x + maxs.x ) * 0.5f, ( mins.y + maxs.y ) * 0.5f, mins.z );
	const Vector target( headPos.x + 1.0f + fit.forward, headPos.y, headPos.z - 2.5f + fit.height );

	// Model space -> head bone space (the head is drawn on the body's head bone)
	for ( int i = 0; i < pos.Count(); i++ )
	{
		const Vector modelPos = ( pos[i] - center ) * scale + target;
		VectorTransform( modelPos, poseToBone, pos[i] );
	}

	// Normals: the file's, or smooth ones from the faces
	CUtlVector<Vector> nrm;
	const bool bFileNormals = mesh.nrm.Count() > 0;
	if ( bFileNormals )
	{
		nrm.SetCount( mesh.nrm.Count() );
		for ( int i = 0; i < mesh.nrm.Count(); i++ )
		{
			const Vector &o = mesh.nrm[i];
			Vector n = fit.zUp ? Vector( -o.y, o.x, o.z ) : Vector( o.z, o.x, o.y );
			n = Vector( n.x * cs - n.y * sn, n.x * sn + n.y * cs, n.z );
			VectorRotate( n, poseToBone, nrm[i] );
			VectorNormalize( nrm[i] );
		}
	}
	else
	{
		nrm.SetCount( pos.Count() );
		for ( int i = 0; i < nrm.Count(); i++ )
			nrm[i].Init();
		for ( int m = 0; m < mesh.mats.Count(); m++ )
		{
			const CUtlVector<ObjVert_t> &tris = mesh.mats[m]->tris;
			for ( int t = 0; t + 2 < tris.Count(); t += 3 )
			{
				const Vector &a = pos[tris[t].v], &b = pos[tris[t + 1].v], &c = pos[tris[t + 2].v];
				const Vector fn = CrossProduct( b - a, c - a );
				nrm[tris[t].v] += fn;
				nrm[tris[t + 1].v] += fn;
				nrm[tris[t + 2].v] += fn;
			}
		}
		for ( int i = 0; i < nrm.Count(); i++ )
		{
			if ( VectorNormalize( nrm[i] ) < 0.0001f )
				nrm[i].Init( 0, 0, 1 );
		}
	}

	// Next version (a model loaded this session can't be replaced under the same name)
	KeyValues *pOld = Manifest()->FindKey( headName );
	const int version = ( pOld ? pOld->GetInt( "version", 0 ) : 0 ) + 1;
	char modelName[GE_CUSTOMHEAD_NAME_LEN];
	char shortName[GE_CUSTOMHEAD_NAME_LEN - 6];
	Q_strncpy( shortName, headName, sizeof( shortName ) );
	Q_snprintf( modelName, sizeof( modelName ), "%s_v%d", shortName, version );

	// Folders
	char srcDir[MAX_PATH], matDirRel[MAX_PATH], matDir[MAX_PATH];
	Q_snprintf( matDirRel, sizeof( matDirRel ), "materials/%s/%s", GE_CUSTOMHEAD_DIR, modelName );
	GameDirPath( matDirRel, matDir, sizeof( matDir ) );
	char srcRel[MAX_PATH];
	Q_snprintf( srcRel, sizeof( srcRel ), "%s/src/%s", GE_CUSTOMHEAD_DIR, headName );
	GameDirPath( srcRel, srcDir, sizeof( srcDir ) );
	filesystem->CreateDirHierarchy( matDir, NULL );
	filesystem->CreateDirHierarchy( srcDir, NULL );

	// Textures and materials
	for ( int m = 0; m < mesh.mats.Count(); m++ )
	{
		const ObjMaterial_t &mat = *mesh.mats[m];
		char vtf[MAX_PATH], vmt[MAX_PATH];
		Q_snprintf( vtf, sizeof( vtf ), "%s/%s.vtf", matDir, mat.name.Get() );
		Q_snprintf( vmt, sizeof( vmt ), "%s/%s.vmt", matDir, mat.name.Get() );
		Q_FixSlashes( vtf );
		Q_FixSlashes( vmt );

		bool bAlpha = false;
		if ( !MakeTexture( mat, vtf, bAlpha, err, errLen ) )
			return false;

		char vmtText[512];
		Q_snprintf( vmtText, sizeof( vmtText ), "\"VertexLitGeneric\"\n{\n\t\"$basetexture\" \"%s/%s/%s\"\n%s}\n",
			GE_CUSTOMHEAD_DIR, modelName, mat.name.Get(), bAlpha ? "\t\"$alphatest\" \"1\"\n" : "" );
		if ( !WriteText( vmt, vmtText ) )
		{
			SetMsg( err, errLen, "Couldn't write %s", vmt );
			return false;
		}
	}

	// SMD: one bone (the head) at the origin, the mesh already in its space
	CUtlBuffer smd( 0, 0, CUtlBuffer::TEXT_BUFFER );
	smd.Printf( "version 1\nnodes\n0 \"%s\" -1\nend\nskeleton\ntime 0\n0 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000\nend\ntriangles\n", GE_CUSTOMHEAD_BONE );
	for ( int m = 0; m < mesh.mats.Count(); m++ )
	{
		const ObjMaterial_t &mat = *mesh.mats[m];
		for ( int t = 0; t + 2 < mat.tris.Count(); t += 3 )
		{
			smd.Printf( "%s\n", mat.name.Get() );
			for ( int k = 0; k < 3; k++ )
			{
				const ObjVert_t &ov = mat.tris[t + k];
				const Vector &p = pos[ov.v];
				const Vector &n = bFileNormals ? ( ov.n >= 0 ? nrm[ov.n] : Vector( 0, 0, 1 ) ) : nrm[ov.v];
				const Vector2D uv = ov.t >= 0 ? mesh.uv[ov.t] : Vector2D( 0, 0 );
				smd.Printf( "0 %f %f %f %f %f %f %f %f\n", p.x, p.y, p.z, n.x, n.y, n.z, uv.x, uv.y );
			}
		}
	}
	smd.Printf( "end\n" );

	char smdPath[MAX_PATH], qcPath[MAX_PATH];
	Q_snprintf( smdPath, sizeof( smdPath ), "%s/head.smd", srcDir );
	Q_snprintf( qcPath, sizeof( qcPath ), "%s/head.qc", srcDir );
	Q_snprintf( s_szImportLog, sizeof( s_szImportLog ), "%s/studiomdl.log", srcDir );
	Q_FixSlashes( smdPath );
	Q_FixSlashes( qcPath );
	Q_FixSlashes( s_szImportLog );
	if ( !filesystem->WriteFile( smdPath, NULL, smd ) )
	{
		SetMsg( err, errLen, "Couldn't write %s", smdPath );
		return false;
	}

	char qc[1024];
	Q_snprintf( qc, sizeof( qc ),
		"$modelname \"ges_custom_heads/%s.mdl\"\n"
		"$cdmaterials \"%s/%s/\"\n"
		"$surfaceprop \"flesh\"\n"
		"$body head \"head.smd\"\n"
		"$sequence idle \"head.smd\" fps 1\n",
		modelName, GE_CUSTOMHEAD_DIR, modelName );
	if ( !WriteText( qcPath, qc ) )
	{
		SetMsg( err, errLen, "Couldn't write %s", qcPath );
		return false;
	}

	if ( !StartCompile( qcPath, err, errLen ) )
		return false;

	Q_strncpy( s_szImportHead, headName, sizeof( s_szImportHead ) );
	Q_strncpy( s_szImportModel, modelName, sizeof( s_szImportModel ) );
	s_iImportVersion = version;
	s_ImportState = GE_HEADIMPORT_RUNNING;
	// Remembered once it compiles (PollImport)
	KeyValues *pPending = Manifest()->FindKey( "_pending", true );
	pPending->SetString( "source", srcPath );
	pPending->SetFloat( "scale", fit.scale );
	pPending->SetFloat( "height", fit.height );
	pPending->SetFloat( "forward", fit.forward );
	pPending->SetInt( "turn", fit.turn & 3 );
	pPending->SetInt( "z_up", fit.zUp ? 1 : 0 );
	return true;
}

bool GECustomHead_StartImport( const char *srcPath, const GEHeadFit_t &fit, char *err, int errLen )
{
	if ( s_ImportState == GE_HEADIMPORT_RUNNING )
	{
		SetMsg( err, errLen, "Still converting the last head" );
		return false;
	}

	s_ImportState = GE_HEADIMPORT_IDLE;
	s_szImportMsg[0] = '\0';

	char base[MAX_PATH], headName[GE_CUSTOMHEAD_NAME_LEN];
	V_FileBase( srcPath, base, sizeof( base ) );
	GECustomChar_CleanHeadName( base, headName, sizeof( headName ) );
	if ( !headName[0] || headName[0] == '_' )
		Q_snprintf( headName, sizeof( headName ), "head%s", headName );

	const char *ext = V_GetFileExtension( srcPath );
	if ( ext && !Q_stricmp( ext, "mdl" ) )
	{
		if ( !ImportCompiledMdl( srcPath, headName, err, errLen ) )
			return false;
		Q_strncpy( s_szImportHead, headName, sizeof( s_szImportHead ) );
		s_ImportState = GE_HEADIMPORT_DONE;
		return true;
	}

	if ( ext && !Q_stricmp( ext, "obj" ) )
		return ImportObj( srcPath, headName, fit, err, errLen );

	SetMsg( err, errLen, "Pick an .obj or .mdl file" );
	return false;
}

GEHeadImportState_t GECustomHead_PollImport( char *headName, int nameLen, char *msg, int msgLen )
{
#ifdef _WIN32
	if ( s_ImportState == GE_HEADIMPORT_RUNNING && s_hImportProcess )
	{
		if ( WaitForSingleObject( s_hImportProcess, 0 ) == WAIT_OBJECT_0 )
		{
			DWORD code = 1;
			GetExitCodeProcess( s_hImportProcess, &code );
			CloseHandle( s_hImportProcess );
			s_hImportProcess = NULL;

			char mdl[MAX_PATH];
			Q_snprintf( mdl, sizeof( mdl ), "%s/%s.mdl", GE_CUSTOMHEAD_DIR, s_szImportModel );
			if ( code == 0 && filesystem->FileExists( mdl, "GAME" ) )
			{
				// The old version goes
				KeyValues *pOld = Manifest()->FindKey( s_szImportHead );
				if ( pOld && pOld->GetString( "model" )[0] && Q_stricmp( pOld->GetString( "model" ), s_szImportModel ) )
					RemoveCompiledHead( pOld->GetString( "model" ) );

				KeyValues *pPending = Manifest()->FindKey( "_pending" );
				GEHeadFit_t fit;
				if ( pPending )
				{
					fit.scale = pPending->GetFloat( "scale", 1.0f );
					fit.height = pPending->GetFloat( "height" );
					fit.forward = pPending->GetFloat( "forward" );
					fit.turn = pPending->GetInt( "turn" );
					fit.zUp = pPending->GetInt( "z_up" ) != 0;
				}
				char src[MAX_PATH];
				Q_strncpy( src, pPending ? pPending->GetString( "source" ) : "", sizeof( src ) );
				if ( pPending )
				{
					Manifest()->RemoveSubKey( pPending );
					pPending->deleteThis();
				}
				RememberHead( s_szImportHead, src, s_szImportModel, s_iImportVersion, &fit );
				s_ImportState = GE_HEADIMPORT_DONE;
			}
			else
			{
				// Last lines of studiomdl's output say why
				CUtlBuffer log( 0, 0, CUtlBuffer::TEXT_BUFFER );
				char last[256] = "";
				if ( filesystem->ReadFile( s_szImportLog, NULL, log ) )
				{
					log.PutChar( '\0' );
					const char *text = (const char *)log.Base();
					const char *err = Q_stristr( text, "ERROR" );
					if ( err )
						RestOfLine( err, last, sizeof( last ) );
					char *nl = strchr( last, '\n' );
					if ( nl )
						*nl = '\0';
				}
				Q_snprintf( s_szImportMsg, sizeof( s_szImportMsg ), "studiomdl couldn't compile the head%s%s (log: %s)", last[0] ? ": " : "", last, s_szImportLog );
				s_ImportState = GE_HEADIMPORT_FAILED;
			}
		}
	}
#endif

	if ( headName && nameLen > 0 )
		Q_strncpy( headName, s_szImportHead, nameLen );
	if ( msg && msgLen > 0 )
		Q_strncpy( msg, s_szImportMsg, msgLen );
	return s_ImportState;
}

CON_COMMAND( ge_customhead_import, "Imports a head for the Custom character: ge_customhead_import <full path to .obj or .mdl> [scale] [height] [forward] [quarter turns] [z up 0/1]" )
{
	if ( args.ArgC() < 2 )
	{
		Msg( "%s\n", ge_customhead_import_command.GetHelpText() );
		return;
	}

	GEHeadFit_t fit;
	if ( args.ArgC() > 2 ) fit.scale = atof( args[2] );
	if ( args.ArgC() > 3 ) fit.height = atof( args[3] );
	if ( args.ArgC() > 4 ) fit.forward = atof( args[4] );
	if ( args.ArgC() > 5 ) fit.turn = atoi( args[5] ) & 3;
	if ( args.ArgC() > 6 ) fit.zUp = atoi( args[6] ) != 0;

	char err[512];
	if ( GECustomHead_StartImport( args[1], fit, err, sizeof( err ) ) )
		Msg( "Importing %s...\n", args[1] );
	else
		Warning( "Can't import: %s\n", err );
}

CON_COMMAND( ge_customhead_list, "Lists the imported heads for the Custom character" )
{
	char name[64], msg[512];
	const GEHeadImportState_t state = GECustomHead_PollImport( name, sizeof( name ), msg, sizeof( msg ) );
	if ( state == GE_HEADIMPORT_RUNNING )
		Msg( "Converting %s...\n", name );
	else if ( state == GE_HEADIMPORT_FAILED )
		Msg( "Last import failed: %s\n", msg );

	CUtlVector<CUtlString> names;
	GECustomHead_ListImported( names );
	for ( int i = 0; i < names.Count(); i++ )
	{
		char model[64];
		GECustomHead_ResolveModelName( names[i].Get(), model, sizeof( model ) );
		Msg( "  %s  (%s/%s.mdl)\n", names[i].Get(), GE_CUSTOMHEAD_DIR, model );
	}
	if ( names.Count() == 0 )
		Msg( "No imported heads\n" );
}
