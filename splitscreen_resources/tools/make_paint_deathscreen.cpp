// Makes the Paintball cheat's death screen: paint instead of blood running down the
// view. GoldenEye's death screen is a Refract material whose tint texture
// (VGUI/hud/bloodscreen/bloodeffect.vtf, 512x512 BGR888) is white with red blood.
// This keeps its shapes and shading and swaps the red for paint:
//   painteffect_mixed.vtf   the 8 paint colours in large soft blobs
//   painteffect_c1..c8.vtf  one colour each (for one random colour per death)
// plus a .vmt for each (a copy of bloodeffect.vmt with the new tint texture).
//
//   make_paint_deathscreen.exe <bloodeffect.vtf> <out folder>
// writes <out>/materials/VGUI/hud/paintscreen/.
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <random>
#include <string>
#include <vector>
#include <direct.h>

struct RGB { double r, g, b; };
static const RGB kColours[8] = {
	{ 255, 60, 170 }, { 60, 230, 90 }, { 50, 140, 255 }, { 255, 150, 30 },
	{ 255, 235, 40 }, { 170, 80, 255 }, { 40, 230, 230 }, { 255, 60, 60 },
};

static bool WriteVTF( const std::string &path, int w, int h, const std::vector<uint8_t> &bgr, const uint8_t *headerTemplate )
{
	FILE *f = fopen( path.c_str(), "wb" );
	if ( !f ) return false;
	fwrite( headerTemplate, 1, 80, f );		// same header as the blood texture (BGR888, one mip)
	fwrite( bgr.data(), 1, bgr.size(), f );
	fclose( f );
	return true;
}

static void WriteVMT( const std::string &path, const std::string &tint )
{
	FILE *f = fopen( path.c_str(), "wb" );
	fprintf( f,
		"\"Refract\"\r\n{\r\n"
		"\t\"$model\" \"1\"\r\n\t\"$bluramount\" \".2\"\r\n\t\"$refractamount\" \"-.15\"\r\n\t\"$refracttint\" \"{255 255 255}\"\r\n"
		"\t\"$scale\" \"[1 1]\"\r\n\t\"$dudvmap\" \"VGUI/hud/bloodscreen/bloodeffect_dudv\"\r\n"
		"\t\"$normalmap\" \"VGUI/hud/bloodscreen/bloodeffect_nrml\"\r\n\t\"$REFRACTTINTTEXTURE\" \"%s\"\r\n\t\"$ignorez\"\t\"1\"\r\n\r\n"
		"\t\"$envmap\" \"models/cubemaps/spheremap2\"\r\n\t\"$envmaptint\" \"[.25 .25 .25]\"\r\n\t\"$envmapcontrast\" \"1\"\r\n\t\"$envmapsaturation\" \"1\"\r\n\r\n"
		"\t\"Proxies\"\r\n\t{\r\n\t\t\"TextureScroll\"\r\n\t\t{\r\n\t\t\t\"texturescrollvar\" \"$bumptransform\"\r\n"
		"\t\t\t\"texturescrollrate\" .25\r\n\t\t\t\"texturescrollangle\" 90.00\r\n\t\t}\r\n\t}\r\n}\r\n", tint.c_str() );
	fclose( f );
}

int main( int argc, char **argv )
{
	if ( argc < 3 ) { printf( "make_paint_deathscreen <bloodeffect.vtf> <out folder>\n" ); return 1; }
	FILE *f = fopen( argv[1], "rb" );
	if ( !f ) { printf( "can't read %s\n", argv[1] ); return 1; }
	std::vector<uint8_t> file;
	{ uint8_t buf[65536]; size_t n; while ( ( n = fread( buf, 1, sizeof( buf ), f ) ) > 0 ) file.insert( file.end(), buf, buf + n ); }
	fclose( f );
	const int w = file[16] | ( file[17] << 8 ), h = file[18] | ( file[19] << 8 );
	const uint32_t fmt = file[52] | ( file[53] << 8 ) | ( file[54] << 16 ) | ( file[55] << 24 );
	if ( fmt != 3 || file[56] != 1 || file.size() < 80 + (size_t) w * h * 3 ) { printf( "unexpected format\n" ); return 1; }
	const uint8_t *src = &file[80];

	const std::string out = argv[2], dir = out + "/materials/VGUI/hud/paintscreen";
	_mkdir( ( out + "/materials" ).c_str() ); _mkdir( ( out + "/materials/VGUI" ).c_str() );
	_mkdir( ( out + "/materials/VGUI/hud" ).c_str() ); _mkdir( dir.c_str() );

	// How much blood each pixel has (0 = white, untinted; 1 = full blood) and how dark it is
	std::vector<double> amount( w * h ), shade( w * h );
	for ( int i = 0; i < w * h; i++ )
	{
		const double b = src[i * 3 + 0], g = src[i * 3 + 1], r = src[i * 3 + 2];
		amount[i] = std::min( 1.0, std::max( 0.0, ( r - ( g + b ) * 0.5 ) / 255.0 + ( 255.0 - r ) / 255.0 ) );
		shade[i] = std::max( 0.35, r / 255.0 );
	}

	auto make = [&]( const std::string &name, auto colourAt )
	{
		std::vector<uint8_t> bgr( w * h * 3 );
		for ( int y = 0; y < h; y++ )
			for ( int x = 0; x < w; x++ )
			{
				const int i = y * w + x;
				const RGB c = colourAt( x, y );
				const double a = amount[i], s = shade[i];
				bgr[i * 3 + 0] = (uint8_t) std::min( 255.0, 255.0 * ( 1 - a ) + c.b * s * a );
				bgr[i * 3 + 1] = (uint8_t) std::min( 255.0, 255.0 * ( 1 - a ) + c.g * s * a );
				bgr[i * 3 + 2] = (uint8_t) std::min( 255.0, 255.0 * ( 1 - a ) + c.r * s * a );
			}
		WriteVTF( dir + "/" + name + ".vtf", w, h, bgr, &file[0] );
		WriteVMT( dir + "/" + name + ".vmt", "VGUI/hud/paintscreen/" + name );
		printf( "%s\n", name.c_str() );
	};

	// Mixed: all 8 colours in large soft blobs
	std::mt19937 rng( 7 );
	std::uniform_real_distribution<double> U( 0.0, 1.0 );
	struct Blob { double x, y; RGB c; };
	std::vector<Blob> blobs;
	for ( int c = 0; c < 8; c++ )
		for ( int k = 0; k < 3; k++ )
			blobs.push_back( { U( rng ) * w, U( rng ) * h, kColours[c] } );
	const double sig = w / 9.0;
	make( "painteffect_mixed", [&]( int x, int y ) {
		double r = 0, g = 0, b = 0, s = 0;
		for ( const Blob &bl : blobs )
		{
			const double d2 = ( x - bl.x ) * ( x - bl.x ) + ( y - bl.y ) * ( y - bl.y );
			const double wt = std::exp( -d2 / ( 2 * sig * sig ) ) + 1e-12;
			r += bl.c.r * wt; g += bl.c.g * wt; b += bl.c.b * wt; s += wt;
		}
		return RGB{ r / s, g / s, b / s };
	} );

	// One colour each
	for ( int c = 0; c < 8; c++ )
		make( "painteffect_c" + std::to_string( c + 1 ), [&]( int, int ) { return kColours[c]; } );
	return 0;
}
