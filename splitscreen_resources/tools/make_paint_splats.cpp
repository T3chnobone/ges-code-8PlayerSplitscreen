// Makes the Paintball cheat's paint splats: 8 variants, 128x128 BGRA8888 VTF (with mipmaps)
// plus their .vmt files, in <out>/materials/decals/paintball/.
//
//   cl /O2 /EHsc make_paint_splats.cpp && make_paint_splats.exe <out folder>
//
// Alpha: a spiky star (base radius 0.5 plus two wobbles and 18-27 Gaussian spikes),
// softened by a small blur. Colour: 3-4 bright colours mixed from Gaussian blobs.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <random>
#include <string>
#include <vector>
#include <direct.h>

static const int N = 128;
static const double PI = 3.14159265358979323846;

struct RGB { double r, g, b; };
static const RGB kColours[8] = {
	{ 255, 60, 170 }, { 60, 230, 90 }, { 50, 140, 255 }, { 255, 150, 30 },
	{ 255, 235, 40 }, { 170, 80, 255 }, { 40, 230, 230 }, { 255, 60, 60 },
};

static void Blur( std::vector<double> &a, double sigma )
{
	const int rad = (int)std::ceil( sigma * 3 );
	std::vector<double> k( rad * 2 + 1 );
	double sum = 0;
	for ( int i = -rad; i <= rad; i++ ) sum += k[i + rad] = std::exp( -( i * i ) / ( 2 * sigma * sigma ) );
	for ( double &v : k ) v /= sum;
	std::vector<double> t( a.size() );
	for ( int y = 0; y < N; y++ )
		for ( int x = 0; x < N; x++ )
		{
			double s = 0;
			for ( int i = -rad; i <= rad; i++ ) { int xx = std::min( N - 1, std::max( 0, x + i ) ); s += a[y * N + xx] * k[i + rad]; }
			t[y * N + x] = s;
		}
	for ( int y = 0; y < N; y++ )
		for ( int x = 0; x < N; x++ )
		{
			double s = 0;
			for ( int i = -rad; i <= rad; i++ ) { int yy = std::min( N - 1, std::max( 0, y + i ) ); s += t[yy * N + x] * k[i + rad]; }
			a[y * N + x] = s;
		}
}

static void Put32( std::vector<uint8_t> &b, uint32_t v ) { for ( int i = 0; i < 4; i++ ) b.push_back( ( v >> ( 8 * i ) ) & 0xFF ); }
static void Put16( std::vector<uint8_t> &b, uint16_t v ) { b.push_back( v & 0xFF ); b.push_back( v >> 8 ); }
static void PutF( std::vector<uint8_t> &b, float f ) { uint32_t v; memcpy( &v, &f, 4 ); Put32( b, v ); }

// VTF 7.2, BGRA8888, all mipmaps (stored smallest first), no low-res thumbnail
static bool WriteVTF( const std::string &path, const std::vector<uint8_t> &bgra )
{
	std::vector<std::vector<uint8_t>> mips;
	mips.push_back( bgra );
	int size = N;
	while ( size > 1 )
	{
		const std::vector<uint8_t> &src = mips.back();
		const int ns = size / 2;
		std::vector<uint8_t> dst( ns * ns * 4 );
		for ( int y = 0; y < ns; y++ )
			for ( int x = 0; x < ns; x++ )
				for ( int c = 0; c < 4; c++ )
				{
					int s = src[( ( 2 * y ) * size + 2 * x ) * 4 + c] + src[( ( 2 * y ) * size + 2 * x + 1 ) * 4 + c] +
						src[( ( 2 * y + 1 ) * size + 2 * x ) * 4 + c] + src[( ( 2 * y + 1 ) * size + 2 * x + 1 ) * 4 + c];
					dst[( y * ns + x ) * 4 + c] = (uint8_t)( ( s + 2 ) / 4 );
				}
		mips.push_back( dst );
		size = ns;
	}

	std::vector<uint8_t> h;
	h.insert( h.end(), { 'V', 'T', 'F', 0 } );
	Put32( h, 7 ); Put32( h, 2 );			// version 7.2
	Put32( h, 80 );							// header size
	Put16( h, N ); Put16( h, N );
	Put32( h, 0x0004 | 0x0008 | 0x2000 );	// CLAMPS | CLAMPT | EIGHTBITALPHA
	Put16( h, 1 ); Put16( h, 0 );			// frames, first frame
	Put32( h, 0 );							// padding
	PutF( h, 0.5f ); PutF( h, 0.5f ); PutF( h, 0.5f );	// reflectivity
	Put32( h, 0 );							// padding
	PutF( h, 1.0f );						// bumpmap scale
	Put32( h, 12 );							// IMAGE_FORMAT_BGRA8888
	h.push_back( (uint8_t) mips.size() );	// mipmap count
	Put32( h, 0xFFFFFFFF );					// no low-res image
	h.push_back( 0 ); h.push_back( 0 );
	Put16( h, 1 );							// depth
	while ( h.size() < 80 ) h.push_back( 0 );

	FILE *f = fopen( path.c_str(), "wb" );
	if ( !f ) return false;
	fwrite( h.data(), 1, h.size(), f );
	for ( int m = (int) mips.size() - 1; m >= 0; m-- )
		fwrite( mips[m].data(), 1, mips[m].size(), f );
	fclose( f );
	return true;
}

int main( int argc, char **argv )
{
	const std::string out = argc > 1 ? argv[1] : ".";
	const std::string dir = out + "/materials/decals/paintball";
	_mkdir( ( out + "/materials" ).c_str() );
	_mkdir( ( out + "/materials/decals" ).c_str() );
	_mkdir( dir.c_str() );

	std::mt19937 rng( 64 );	// fixed seed: the same splats every time
	std::uniform_real_distribution<double> U( 0.0, 1.0 );

	for ( int v = 1; v <= 8; v++ )
	{
		// ---- Alpha: the spiky star ----
		const double ph1 = U( rng ) * 2 * PI, ph2 = U( rng ) * 2 * PI;
		const int spikes = 18 + (int)( U( rng ) * 10 );	// 18-27
		std::vector<double> amp( spikes ), w( spikes );
		const double spikeOffset = U( rng ) * 2 * PI;
		for ( int k = 0; k < spikes; k++ ) { amp[k] = 0.3 + 0.7 * U( rng ); w[k] = 0.06 + 0.08 * U( rng ); }
		const double scale = 0.85 + 0.15 * U( rng );

		std::vector<double> alpha( N * N ), r( N * N );
		for ( int y = 0; y < N; y++ )
			for ( int x = 0; x < N; x++ )
			{
				const double dx = ( x + 0.5 ) / ( N / 2.0 ) - 1.0, dy = ( y + 0.5 ) / ( N / 2.0 ) - 1.0;
				const double rr = std::sqrt( dx * dx + dy * dy ), th = std::atan2( dy, dx );
				double rad = 0.5 + 0.06 * std::sin( 7 * th + ph1 ) + 0.04 * std::sin( 13 * th + ph2 );
				for ( int k = 0; k < spikes; k++ )
				{
					double d = th - ( spikeOffset + 2 * PI * k / spikes );
					d = std::remainder( d, 2 * PI );
					rad += 0.32 * amp[k] * std::exp( -( d / w[k] ) * ( d / w[k] ) );
				}
				rad *= scale;
				alpha[y * N + x] = std::min( 1.0, std::max( 0.0, ( rad - rr ) * 14 ) );
				r[y * N + x] = rr;
			}
		Blur( alpha, 1.4 );

		// ---- Colour: 3-4 distinct colours, 2 blobs each ----
		int order[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
		for ( int i = 7; i > 0; i-- ) std::swap( order[i], order[(int)( U( rng ) * ( i + 1 ) )] );
		const int nCol = 3 + ( U( rng ) < 0.5 ? 1 : 0 );
		struct Blob { double x, y; RGB c; };
		std::vector<Blob> blobs;
		for ( int c = 0; c < nCol; c++ )
			for ( int b = 0; b < 2; b++ )
				blobs.push_back( { 16 + 96 * U( rng ), 16 + 96 * U( rng ), kColours[order[c]] } );
		const double sig = 18.0;

		std::vector<uint8_t> bgra( N * N * 4 );
		for ( int y = 0; y < N; y++ )
			for ( int x = 0; x < N; x++ )
			{
				double wr = 0, wg = 0, wb = 0, ws = 0;
				for ( const Blob &bl : blobs )
				{
					const double d2 = ( x - bl.x ) * ( x - bl.x ) + ( y - bl.y ) * ( y - bl.y );
					const double wt = std::exp( -d2 / ( 2 * sig * sig ) ) + 1e-9;
					wr += bl.c.r * wt; wg += bl.c.g * wt; wb += bl.c.b * wt; ws += wt;
				}
				const double shade = 0.85 + 0.3 * ( 1.0 - std::min( 1.0, r[y * N + x] ) );
				uint8_t *p = &bgra[( y * N + x ) * 4];
				p[0] = (uint8_t) std::min( 255.0, wb / ws * shade );
				p[1] = (uint8_t) std::min( 255.0, wg / ws * shade );
				p[2] = (uint8_t) std::min( 255.0, wr / ws * shade );
				p[3] = (uint8_t) std::min( 255.0, alpha[y * N + x] * 0.92 * 255.0 + 0.5 );
			}

		char name[64];
		snprintf( name, sizeof( name ), "splat%d", v );
		if ( !WriteVTF( dir + "/" + name + ".vtf", bgra ) )
		{
			printf( "can't write %s\n", name );
			return 1;
		}

		// Unlit, so the paint stays bright on dark walls; 9-14 units across (128 texels * decalscale)
		const double decalScale = 0.07 + 0.04 * ( v - 1 ) / 7.0;
		FILE *f = fopen( ( dir + "/" + name + ".vmt" ).c_str(), "wb" );
		fprintf( f, "\"UnlitGeneric\"\r\n{\r\n\t\"$basetexture\" \"decals/paintball/%s\"\r\n\t\"$decal\" \"1\"\r\n\t\"$decalscale\" \"%.3f\"\r\n"
			"\t\"$translucent\" \"1\"\r\n\t\"$vertexcolor\" \"1\"\r\n}\r\n", name, decalScale );
		fclose( f );
		printf( "%s: %d spikes, %d colours, decalscale %.3f\n", name, spikes, nCol, decalScale );
	}
	return 0;
}
