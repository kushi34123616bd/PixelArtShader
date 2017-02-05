#include <windows.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include "EasyBMP\EasyBMP.h"
#include "CTecoDll.h"
#include "CPngRW.h"


///////////////////////////////////////////////////////////////////////////////

#define MAX_PALETTE	24	// シェーダーで扱えるマテリアル数の限界（もう1か2は増やせるかも）


#define WIDTH	256
#define HEIGHT	256

//#define POS_DARK_X	128	// 陰用パレットの開始座標

// palette.bmp
#define POS_LIGHT_X		( 64 * 0 )	// 明るさ情報の開始座標
#define POS_COL_X		( 64 * 2 )	// 色成分の開始座標
#define POS_COL_Y		( 2 )

#define POS_DARK_X		( 64 * 2 )	// 暗部情報の開始座標
#define POS_DARK_Y		( 0 )

#define POS_AA_X		( 64 * 2 )	// AA情報の開始座標
#define POS_AA_Y		( 1 )



#define POS_ADJACENT_X	( WIDTH - MAX_PALETTE )		// 隣接マテリアル情報の開始座標
#define POS_ADJACENT_Y	( HEIGHT - MAX_PALETTE )



// palette_debug.bmp
#define POS_DEBUG_X		( 0 )		// デバッグ表示の開始座標


// 命令の種類
enum
{
	COMMAND_END,		// スクリプトの終わり

	COMMAND_MAXPER,
	COMMAND_MATERIAL,
	COMMAND_COL,
	COMMAND_COLGRAD,
	COMMAND_COLMIX,
	COMMAND_PERCOL,
	COMMAND_PERCOLEND,
	COMMAND_DARK,
	COMMAND_EDGE,
	COMMAND_MATEEDGE,
	COMMAND_GUTTER,
	COMMAND_AATHRESHOLD,
	COMMAND_AASUBTRACTER,
	COMMAND_NOEDGE,
	COMMAND_ADJUST_RATE,
};

// 暗部モード
enum
{
	MODE_DARK_NORMAL,
	MODE_DARK_NONE,
	MODE_DARK_COL,
};

///////////////////////////////////////////////////////////////////////////////

// http://laputa.cs.shinshu-u.ac.jp/~gtakano/prog3.html

// R,G,B は 0 ～ 1 の値
// H は 0 ～ 360, S, V は 0 ～ 1 の値

double max_color( double r, double g, double b )
{
	double ret;
	if( r > g ){
		if( r > b )
			ret = r;
		else
			ret = b;
	}
	else{
		if( g > b )
			ret = g;
		else
			ret = b;
	}
	return ret;
}

double min_color( double r, double g, double b )
{
	double ret;
	if( r < g ){
		if( r < b )
			ret = r;
		else
			ret = b;
	}
	else{
		if( g < b )
			ret = g;
		else
			ret = b;
	}
	return ret;
}

template <typename T>
inline void Clamp01( T &n )
{
	if( n < 0 )
		n = 0;
	else if( 1 < n )
		n = 1;
}

template <typename T>
inline T Lerp( T a, T b, float s )
{
	return (T)( a + ( b - a ) * s );
}

void RGB2HSV( double *H, double *S, double *V, double R, double G, double B )
{
	double Z;
	double r,g,b;
	*V = max_color( R, G, B );
	Z  = min_color( R, G, B );
	if( *V != 0.0 )
		*S = ( *V - Z ) / *V;
	else
		*S = 0.0;
	if( ( *V - Z ) != 0 ){
		r = ( *V - R ) / ( *V - Z );
		g = ( *V - G ) / ( *V - Z );
		b = ( *V - B ) / ( *V - Z );
	}
	else{
		r = 0;
		g = 0;
		b = 0;
	}
	if( *V == R )
		*H = 60 * ( b - g );		// 60 = PI/3
	else if( *V == G )
		*H = 60 * ( 2 + r - b );
	else
		*H = 60 * ( 4 + g - r );
	if( *H < 0.0 )
		*H = *H + 360;
	else if( 360 <= *H )
		*H -= 360;
}

// H = [ 0, 360 ]	色相
// S = [ 0, 1 ]		彩度  黒から原色
// V = [ 0, 1 ]		明度  黒から白
void HSV2RGB( double *rr, double *gg, double *bb, double H, double S, double V )
{
	int in;
	double fl;
	double m, n;
	in = (int)floor( H / 60 );
	fl = ( H / 60 ) - in;
	if( !(in & 1)) fl = 1 - fl; // if i is even

	m = V * ( 1 - S );
	n = V * ( 1 - S * fl );
	switch( in ){
	   case 0: *rr = V; *gg = n; *bb = m; break;
	   case 1: *rr = n; *gg = V; *bb = m; break;
	   case 2: *rr = m; *gg = V; *bb = n; break;
	   case 3: *rr = m; *gg = n; *bb = V; break;
	   case 4: *rr = n; *gg = m; *bb = V; break;
	   case 5: *rr = V; *gg = m; *bb = n; break;
	}

	Clamp01( *rr );
	Clamp01( *gg );
	Clamp01( *bb );
}

///////////////////////////////////////////////////////////////////////////////

static int g_curPer = 0;
static int g_maxPer = 100;
static int g_preR = -1, g_preG = -1, g_preB = -1;
static int g_curColNum = 0;


void DrawColor( BMP *pDebug, BMP *pDst, int x, int divNum, int _r, int _g, int _b )
{
	if( divNum < 1 )
		divNum = 1;

	double r = _r / 255.0;
	double g = _g / 255.0;
	double b = _b / 255.0;

	double h, s, v;

	RGB2HSV( &h, &s, &v, r, g, b );


	int len = HEIGHT / divNum;	// 1色の長さ


	int alpha = 0;

	if( divNum % 2 == 0 )
	{
		alpha = 1;
	}

	for( int i = 0;  i < divNum;  i ++ )
	{
		double rate = 0;

		if( divNum != 1 )
		{
			rate = 1.0 - 2.0 * ( i + alpha ) / ( divNum + alpha - 1 );
		}


		double _v = v + rate * 0.4;	// v-0.4 ～ v+0.4
		Clamp01( _v );


		double _h = h + rate * 20;
		if( _h < 0 )
			_h += 360;
		else if( 360 <= _h )
			_h -= 360;


		double R, G, B;

		HSV2RGB( &R, &G, &B, _h, s, _v );

		// 色成分の描画
		{
			RGBApixel col;
			col.Red =	(ebmpBYTE)( R * 255 );
			col.Green =	(ebmpBYTE)( G * 255 );
			col.Blue =	(ebmpBYTE)( B * 255 );
			col.Alpha =	0xff;

			pDst->SetPixel( POS_COL_X + x, POS_COL_Y + i, col );
		}

		// 明るさ情報の描画
		{
			RGBApixel col;
			col.Red =	(ebmpBYTE)( i );
			col.Green =	0;		// unused
			col.Blue =	0;		// unused
			col.Alpha =	0xff;	// unused

			for( int y = i * HEIGHT / divNum;  y < ( i + 1 ) * HEIGHT / divNum;  y ++ )
			{
				pDst->SetPixel( POS_LIGHT_X + x, y, col );
			}
		}

		// 確認用に色付きで描画する
		{
			RGBApixel col;
			col.Red =	(ebmpBYTE)( R * 255 );
			col.Green =	(ebmpBYTE)( G * 255 );
			col.Blue =	(ebmpBYTE)( B * 255 );
			col.Alpha =	0xff;

			for( int y = i * HEIGHT / divNum;  y < ( i + 1 ) * HEIGHT / divNum;  y ++ )
			{
				for( int _x = 0;  _x < 8;  _x ++ )
				{
					pDebug->SetPixel( POS_DEBUG_X + x * 8 + _x, y, col );
				}
			}
		}
	}

	g_curColNum = divNum;
}


void DrawColorGradation( BMP *pDebug, BMP *pDst, int x, int divNum, int _r1, int _g1, int _b1, int _r2, int _g2, int _b2 )
{
	if( divNum < 2 )
		divNum = 2;

	for( int i = 0;  i < divNum;  i ++ )
	{
		RGBApixel dstCol;

		// 色成分の描画
		{
			float s = (float)i / ( divNum - 1 );

			dstCol.Red =	(ebmpBYTE)Lerp( _r1, _r2, s );
			dstCol.Green =	(ebmpBYTE)Lerp( _g1, _g2, s );
			dstCol.Blue =	(ebmpBYTE)Lerp( _b1, _b2, s );
			dstCol.Alpha =	0xff;

			pDst->SetPixel( POS_COL_X + x, POS_COL_Y + i, dstCol );
		}

		// 明るさ情報の描画
		{
			RGBApixel col;
			col.Red =	(ebmpBYTE)( i );
			col.Green =	0;		// unused
			col.Blue =	0;		// unused
			col.Alpha =	0xff;	// unused

			for( int y = i * HEIGHT / divNum;  y < ( i + 1 ) * HEIGHT / divNum;  y ++ )
			{
				pDst->SetPixel( POS_LIGHT_X + x, y, col );
			}
		}

		// 確認用に色付きで描画する
		{
			for( int y = i * HEIGHT / divNum;  y < ( i + 1 ) * HEIGHT / divNum;  y ++ )
			{
				for( int _x = 0;  _x < 8;  _x ++ )
				{
					pDebug->SetPixel( POS_DEBUG_X + x * 8 + _x, y, dstCol );
				}
			}
		}
	}

	g_curColNum = divNum;
}


void DrawColorMix( BMP *pDebug, BMP *pDst, int x, int divNum )
{
	if( divNum < 2 )
		divNum = 2;

/*
	int mixCol[][ 3 ] =
	{
		{ 0xff, 0xff, 0xff },
		{ 0x00, 0x00, 0xff },
		{ 0xff, 0x00, 0x00 },
		{ 0x00, 0xff, 0x00 },
		{ 0xff, 0x00, 0xff },
		{ 0xff, 0xff, 0x00 },
		{ 0x00, 0xff, 0xff },
		{ 0x80, 0x80, 0x80 },
		{ 0x00, 0x00, 0x80 },
		{ 0x80, 0x00, 0x00 },
		{ 0x00, 0x80, 0x00 },
		{ 0x80, 0x00, 0x80 },
		{ 0x80, 0x80, 0x00 },
		{ 0x00, 0x80, 0x80 },
	};

	int mixColNum = sizeof(mixCol) / ( sizeof(int) * 3 );
*/


	for( int i = 0;  i < divNum;  i ++ )
	{
		RGBApixel dstCol;

		// 色成分の描画
		{
			float s = (float)i / ( divNum - 1 );

#if 0
			int col1 = (int)( s * ( mixColNum - 1 ) );
			int col2 = col1 + 1;
			if( ( mixColNum - 1 ) < col2 )
				col2 = ( mixColNum - 1 );

			float s2 = s * ( mixColNum - 1 ) - col1;

			dstCol.Red =	(ebmpBYTE)Lerp( mixCol[ col1 ][ 0 ], mixCol[ col2 ][ 0 ], s2 );
			dstCol.Green =	(ebmpBYTE)Lerp( mixCol[ col1 ][ 1 ], mixCol[ col2 ][ 1 ], s2 );
			dstCol.Blue =	(ebmpBYTE)Lerp( mixCol[ col1 ][ 2 ], mixCol[ col2 ][ 2 ], s2 );
			dstCol.Alpha =	0xff;
#else
			double H = s * ( 360 * 2 );
			while( 360 <= H )
				H -= 360;

			double S = 1;
			double V = Lerp( 1.0, 0.5, s );

			double R, G, B;

			HSV2RGB( &R, &G, &B, H, S, V );

			dstCol.Red =	(ebmpBYTE)( R * 255 );
			dstCol.Green =	(ebmpBYTE)( G * 255 );
			dstCol.Blue =	(ebmpBYTE)( B * 255 );
			dstCol.Alpha =	0xff;
#endif

			pDst->SetPixel( POS_COL_X + x, POS_COL_Y + i, dstCol );
		}

		// 明るさ情報の描画
		{
			RGBApixel col;
			col.Red =	(ebmpBYTE)( i );
			col.Green =	0;		// unused
			col.Blue =	0;		// unused
			col.Alpha =	0xff;	// unused

			for( int y = i * HEIGHT / divNum;  y < ( i + 1 ) * HEIGHT / divNum;  y ++ )
			{
				pDst->SetPixel( POS_LIGHT_X + x, y, col );
			}
		}

		// 確認用に色付きで描画する
		{
			for( int y = i * HEIGHT / divNum;  y < ( i + 1 ) * HEIGHT / divNum;  y ++ )
			{
				for( int _x = 0;  _x < 8;  _x ++ )
				{
					pDebug->SetPixel( POS_DEBUG_X + x * 8 + _x, y, dstCol );
				}
			}
		}
	}

	g_curColNum = divNum;
}


void DrawPerColorInit( void )
{
	g_curPer = 0;
	g_curColNum = 0;

	g_preR = -1;
	g_preG = -1;
	g_preB = -1;
}


void DrawPerColor( BMP *pDebug, BMP *pDst, int x, int per, int r, int g, int b )
{
	if( g_maxPer < g_curPer )
		return;

	if( per <= 0 )
		return;

	if( r != g_preR  ||  g != g_preG  ||  b != g_preB )
	{
		// 色成分の描画

		RGBApixel col;
		col.Red =	(ebmpBYTE)( r );
		col.Green =	(ebmpBYTE)( g );
		col.Blue =	(ebmpBYTE)( b );
		col.Alpha =	0xff;

		pDst->SetPixel( POS_COL_X + x, POS_COL_Y + g_curColNum, col );

		g_curColNum ++;

		g_preR = r;
		g_preG = g;
		g_preB = b;
	}


	int nextPer = g_curPer + per;
	if( g_maxPer < nextPer )
	{
		nextPer = g_maxPer;
	}

	// 明るさ情報の描画
	{
		RGBApixel col;
		col.Red =	(ebmpBYTE)( g_curColNum - 1 );
		col.Green =	0;		// unused
		col.Blue =	0;		// unused
		col.Alpha =	0xff;	// unused

		for( int y = HEIGHT * g_curPer / g_maxPer;  y < HEIGHT * nextPer / g_maxPer;  y ++ )
		{
			pDst->SetPixel( POS_LIGHT_X + x, y, col );
		}
	}

	// 確認用に色付きで描画する
	{
		RGBApixel col;
		col.Red =	(ebmpBYTE)( r );
		col.Green =	(ebmpBYTE)( g );
		col.Blue =	(ebmpBYTE)( b );
		col.Alpha =	0xff;

		for( int y = HEIGHT * g_curPer / g_maxPer;  y < HEIGHT * nextPer / g_maxPer;  y ++ )
		{
			for( int _x = 0;  _x < 8;  _x ++ )
			{
				pDebug->SetPixel( POS_DEBUG_X + x * 8 + _x, y, col );
			}
		}
	}

	g_curPer = nextPer;
}


void DrawDark( BMP *pDst, int x, int mode, int r, int g, int b )
{
	// mode
	{
		RGBApixel col = pDst->GetPixel( POS_DARK_X + x, POS_DARK_Y );

		switch( mode )
		{
		case MODE_DARK_NORMAL:	col.Red =	(ebmpBYTE)( 1 );	break;	// 色成分を1つずらす
		case MODE_DARK_NONE:	col.Red =	(ebmpBYTE)( 0 );	break;	// 色成分を変化させない
		case MODE_DARK_COL:		col.Red =	(ebmpBYTE)( 255 );	break;	// ダークカラーを使用する
		}

		pDst->SetPixel( POS_DARK_X + x, POS_DARK_Y, col );
	}

	// color
	{
		RGBApixel col;

		col.Red		= (ebmpBYTE)( r );
		col.Green	= (ebmpBYTE)( g );
		col.Blue	= (ebmpBYTE)( b );
		col.Alpha	= 0xff;

		pDst->SetPixel( POS_DARK_X + x, POS_COL_Y + g_curColNum, col );	// ダークカラーは色成分の最後に追加する
	}

}


// EMG情報を描画
void DrawEMG( BMP *pDst, int x, int edge, int mateedge, int gutter )
{
	RGBApixel col = pDst->GetPixel( POS_DARK_X + x, POS_DARK_Y );

	ebmpBYTE green = 0;

	if( edge )		green |= 1;
	if( mateedge )	green |= 2;
	if( gutter )	green |= 4;

	col.Green = green;

	pDst->SetPixel( POS_DARK_X + x, POS_DARK_Y, col );
}


// AA情報を描画
void DrawAA( BMP *pDst, int x, float AAThreshold, float AASubtracter )
{
	Clamp01( AAThreshold );
	Clamp01( AASubtracter );

/*
	if( AAThreshold < 0 )
		AAThreshold = 0;
	else if( 1 < AAThreshold )
		AAThreshold = 1;

	if( AASubtracter < 0 )
		AASubtracter = 0;
	else if( 1 < AASubtracter )
		AASubtracter = 1;
*/

	RGBApixel col = pDst->GetPixel( POS_AA_X + x, POS_AA_Y );

	col.Red		= (ebmpBYTE)( AAThreshold * 255 + 0.5f );
	col.Green	= (ebmpBYTE)( AASubtracter * 255 + 0.5f );

	pDst->SetPixel( POS_AA_X + x, POS_AA_Y, col );
}


// 隣接マテリアル情報を描画
void DrawAdjacent( BMP *pDst, int x, int *paNo_edge )
{
	RGBApixel col;

	for( int y = 0;  y < MAX_PALETTE;  y ++ )
	{
		ebmpBYTE red = 0;

		if( paNo_edge[ y ] )	red = 255;

		col.Red		= red;
		col.Green	= 0;
		col.Blue	= 0;
		col.Alpha	= 0;

		pDst->SetPixel( POS_ADJACENT_X + x, POS_ADJACENT_Y + y, col );
	}
}


/*
// 陰用のパレット描画
void DrawShade( BMP *pDst, int x_src, int x_dst )
{
	RGBApixel col_prev = *(*pDst)( x_src, 0 );

	int y = 0;
	while( y < HEIGHT )
	{
		RGBApixel *pCol = (*pDst)( x_src, y );

		int _y;
		for( _y = y + 1;  _y < HEIGHT;  _y ++ )
		{
			RGBApixel *pCol2 = (*pDst)( x_src, _y );

			if( pCol->Red != pCol2->Red  ||  pCol->Green != pCol2->Green  ||  pCol->Blue != pCol2->Blue )
			{
				break;
			}
		}


		if( _y < HEIGHT )
		{
			col_prev = *(*pDst)( x_src, _y );
		}

		for( int i = y;  i < _y;  i ++ )
		{
			pDst->SetPixel( x_dst, i, col_prev );
		}

		y = _y;
	}
}
*/


bool IsPngFilename( const char *szFilename )
{
	int len = strlen( szFilename );

	if( len < 5 )
		return false;

	if(	    szFilename[ len - 4 ] == '.'
		&&  tolower( szFilename[ len - 3 ] ) == 'p'
		&&  tolower( szFilename[ len - 2 ] ) == 'n'
		&&  tolower( szFilename[ len - 1 ] ) == 'g' )
		return true;

	return false;
}

///////////////////////////////////////////////////////////////////////////////

void main( int argc, char *argv[] )
{
	if( argc != 3 )
	{
		printf( "MakeColorTex.exe DstFileName ScriptFileName\n" );
		return;
	}

	char *szDstFileName = argv[ 1 ];
	char *szSrcFileName = argv[ 2 ];


	CTecoDll tecoDll;

	if( ! tecoDll.Init() )
	{
		printf( "tecoDll.Init()\n" );
		return;
	}


	BYTE *pData = NULL;
	int dataSize = 0;

	tecoDll.Convert( szSrcFileName, pData, dataSize );

	if( tecoDll.GetError() != CTecoDll::RET_SUCCESS )
	{
		printf( "%s", tecoDll.GetErrorMessage() );
		if( pData )
			delete [] pData;
		return;
	}

	if( dataSize == 0 )
	{
		printf( "empty" );
		if( pData )
			delete [] pData;
		return;
	}



	BMP dst;

	dst.SetSize( WIDTH, HEIGHT );
	dst.SetBitDepth( 24 );

	BMP dstDebug;

	dstDebug.SetSize( WIDTH, HEIGHT );
	dstDebug.SetBitDepth( 24 );

	{
		int *p = (int *)pData;
		int x = 0;


		int dark_defMode = MODE_DARK_NORMAL;
		int dark_defCol_r = 0;
		int dark_defCol_g = 0;
		int dark_defCol_b = 0;


		int edge_def = 1;
		int mateedge_def = 1;
		int gutter_def = 1;

		int edge = 1;
		int mateedge = 1;
		int gutter = 1;


		float AAThreshold_def = 0.5f;
		float AASubtracter_def = 1.0f;

		float AAThreshold = AAThreshold_def;
		float AASubtracter = AASubtracter_def;


		int aAdjacent_no_edge_def[ MAX_PALETTE ];

		int aAdjacent_no_edge[ MAX_PALETTE ];

		memset( aAdjacent_no_edge_def, 0, sizeof(aAdjacent_no_edge_def) );
		memset( aAdjacent_no_edge, 0, sizeof(aAdjacent_no_edge) );


		float adjustRate = 1.0f;


		bool bEnd = false;
		while( ! bEnd )
		{
			switch( *p )
			{
			case COMMAND_END:
				bEnd = true;
				break;


			case COMMAND_MAXPER:
				{
					g_maxPer = p[ 1 ];
					p += 2;
				}
				break;

			case COMMAND_MATERIAL:
				{
					x = p[ 1 ];
					p += 2;
				}
				break;

			case COMMAND_COL:
				{
					DrawColor( &dstDebug, &dst, x, p[ 1 ], p[ 2 ], p[ 3 ], p[ 4 ] );

					// ダーク部の色を描画
					DrawDark( &dst, x, dark_defMode, dark_defCol_r, dark_defCol_g, dark_defCol_b );

					// EMG情報を描画
					{
						edge = edge_def;
						mateedge = mateedge_def;
						gutter = gutter_def;

						DrawEMG( &dst, x, edge, mateedge, gutter );
					}

					// AA情報を描画
					{
						AAThreshold = AAThreshold_def;
						AASubtracter = AASubtracter_def;

						DrawAA( &dst, x, AAThreshold, AASubtracter );
					}

					// 隣接マテリアル情報を描画
					{
//						adjacent_no_edge = adjacent_no_edge_def;

						memcpy( aAdjacent_no_edge, aAdjacent_no_edge_def, sizeof(aAdjacent_no_edge_def) );

						DrawAdjacent( &dst, x, aAdjacent_no_edge );
					}

					x ++;

					p += 5;
				}
				break;

			case COMMAND_COLGRAD:
				{
					DrawColorGradation( &dstDebug, &dst, x, p[ 1 ], p[ 2 ], p[ 3 ], p[ 4 ], p[ 5 ], p[ 6 ], p[ 7 ] );

					// ダーク部の色を描画
					DrawDark( &dst, x, dark_defMode, dark_defCol_r, dark_defCol_g, dark_defCol_b );

					// EMG情報を描画
					{
						edge = edge_def;
						mateedge = mateedge_def;
						gutter = gutter_def;

						DrawEMG( &dst, x, edge, mateedge, gutter );
					}

					// AA情報を描画
					{
						AAThreshold = AAThreshold_def;
						AASubtracter = AASubtracter_def;

						DrawAA( &dst, x, AAThreshold, AASubtracter );
					}

					// 隣接マテリアル情報を描画
					{
//						adjacent_no_edge = adjacent_no_edge_def;

						memcpy( aAdjacent_no_edge, aAdjacent_no_edge_def, sizeof(aAdjacent_no_edge_def) );

						DrawAdjacent( &dst, x, aAdjacent_no_edge );
					}

					x ++;

					p += 8;
				}
				break;

			case COMMAND_COLMIX:
				{
					DrawColorMix( &dstDebug, &dst, x, p[ 1 ] );

					// ダーク部の色を描画
					DrawDark( &dst, x, dark_defMode, dark_defCol_r, dark_defCol_g, dark_defCol_b );

					// EMG情報を描画
					{
						edge = edge_def;
						mateedge = mateedge_def;
						gutter = gutter_def;

						DrawEMG( &dst, x, edge, mateedge, gutter );
					}

					// AA情報を描画
					{
						AAThreshold = AAThreshold_def;
						AASubtracter = AASubtracter_def;

						DrawAA( &dst, x, AAThreshold, AASubtracter );
					}

					// 隣接マテリアル情報を描画
					{
//						adjacent_no_edge = adjacent_no_edge_def;

						memcpy( aAdjacent_no_edge, aAdjacent_no_edge_def, sizeof(aAdjacent_no_edge_def) );

						DrawAdjacent( &dst, x, aAdjacent_no_edge );
					}

					x ++;

					p += 2;
				}
				break;

			case COMMAND_PERCOL:
				{
					DrawPerColorInit();


					int restPer = g_maxPer;

#if 0
					int r, g, b;

					while( *p == COMMAND_PERCOL )
					{
						int per;

						per = p[ 1 ];
						r = p[ 2 ];
						g = p[ 3 ];
						b = p[ 4 ];

						DrawPerColor( &dstDebug, &dst, x, per, r, g, b );

						restPer -= per;

						p += 5;
					}

					if( 0 < restPer )
					{
						DrawPerColor( &dstDebug, &dst, x, g_maxPer, r, g, b );
					}
#else

					int count = 0;
					int per[ 256 ];
					int r[ 256 ], g[ 256 ], b[ 256 ];

					while( *p == COMMAND_PERCOL )
					{
						per[ count ] = p[ 1 ];
						r[ count ] = p[ 2 ];
						g[ count ] = p[ 3 ];
						b[ count ] = p[ 4 ];

						restPer -= per[ count ];

						p += 5;

						if( ++ count == 256 )
							break;
					}

					per[ count - 1 ] += restPer;

					// 調整
					if( adjustRate != 1.0f  &&  2 <= count )
					{
						float adjust = 1 / adjustRate;

						int start, end, rest;

/*
						if( adjust < 1.0f )
						{
							start = 1;
							end = count - 1;
							rest = 0;
						}
						else
						{
							start = 0;
							end = count - 2;
							rest = count - 1;

							// limit
							float maxRate = (float)g_maxPer / ( g_maxPer - per[ rest ] );
							if( maxRate < adjust )
								adjust = maxRate;
						}
*/
						{
							start = 1;
							end = count - 1;
							rest = 0;

							// limit
							float maxRate = (float)g_maxPer / ( g_maxPer - per[ rest ] );
							if( maxRate < adjust )
								adjust = maxRate;
						}

						int totalPer = 0;

						for( int i = start;  i <= end;  i ++ )
						{
							per[ i ] = (int)( per[ i ] * adjust );
							if( per[ i ] < 1 )
								per[ i ] = 1;

							totalPer += per[ i ];
						}

						per[ rest ] = g_maxPer - totalPer;
					}

					for( int i = 0;  i < count;  i ++ )
					{
						DrawPerColor( &dstDebug, &dst, x, per[ i ], r[ i ], g[ i ], b[ i ] );
					}

					// 元に戻す
					adjustRate = 1;

#endif

					// ダーク部の色を描画
					DrawDark( &dst, x, dark_defMode, dark_defCol_r, dark_defCol_g, dark_defCol_b );

					// EMG情報を描画
					{
						edge = edge_def;
						mateedge = mateedge_def;
						gutter = gutter_def;

						DrawEMG( &dst, x, edge, mateedge, gutter );
					}

					// AA情報を描画
					{
						AAThreshold = AAThreshold_def;
						AASubtracter = AASubtracter_def;

						DrawAA( &dst, x, AAThreshold, AASubtracter );
					}

					// 隣接マテリアル情報を描画
					{
//						adjacent_no_edge = adjacent_no_edge_def;

						memcpy( aAdjacent_no_edge, aAdjacent_no_edge_def, sizeof(aAdjacent_no_edge_def) );

						DrawAdjacent( &dst, x, aAdjacent_no_edge );
					}

					x ++;
				}
				break;

			case COMMAND_PERCOLEND:
				{
					p ++;
				}
				break;

			case COMMAND_DARK:
				{
					int mode = p[ 1 ];
					int r = p[ 2 ];
					int g = p[ 3 ];
					int b = p[ 4 ];

					p += 5;

					if( x == 0 )
					{
						// デフォルト値を設定
						dark_defMode = mode;
						dark_defCol_r = r;
						dark_defCol_g = g;
						dark_defCol_b = b;
					}
					else
					{
						DrawDark( &dst, x - 1, mode, r, g, b );
					}
				}
				break;

			case COMMAND_EDGE:
				{
					int flag = ( p[ 1 ] != 0 );

					if( x == 0 )
					{
						// デフォルト値を設定
						edge_def = flag;
					}
					else
					{
						edge = flag;

						// EMG情報を描画しなおす
						DrawEMG( &dst, x - 1, edge, mateedge, gutter );
					}

					p += 2;
				}
				break;

			case COMMAND_MATEEDGE:
				{
					int flag = ( p[ 1 ] != 0 );

					if( x == 0 )
					{
						// デフォルト値を設定
						mateedge_def = flag;
					}
					else
					{
						mateedge = flag;

						// EMG情報を描画しなおす
						DrawEMG( &dst, x - 1, edge, mateedge, gutter );
					}

					p += 2;
				}
				break;

			case COMMAND_GUTTER:
				{
					int flag = ( p[ 1 ] != 0 );

					if( x == 0 )
					{
						// デフォルト値を設定
						gutter_def = flag;
					}
					else
					{
						gutter = flag;

						// EMG情報を描画しなおす
						DrawEMG( &dst, x - 1, edge, mateedge, gutter );
					}

					p += 2;
				}
				break;

			case COMMAND_AATHRESHOLD:
				{
					float n = ( (float *)p )[ 1 ];

					if( x == 0 )
					{
						// デフォルト値を設定
						AAThreshold_def = n;
					}
					else
					{
						AAThreshold = n;

						// AA情報を描画しなおす
						DrawAA( &dst, x - 1, AAThreshold, AASubtracter );
					}

					p += 2;
				}
				break;

			case COMMAND_AASUBTRACTER:
				{
					float n = ( (float *)p )[ 1 ];

					if( x == 0 )
					{
						// デフォルト値を設定
						AASubtracter_def = n;
					}
					else
					{
						AASubtracter = n;

						// AA情報を描画しなおす
						DrawAA( &dst, x - 1, AAThreshold, AASubtracter );
					}

					p += 2;
				}
				break;

			case COMMAND_NOEDGE:
				{
					int targetMaterialNo = p[ 1 ];
					int flag = ( p[ 2 ] != 0 );

					if( 0 <= targetMaterialNo  &&  targetMaterialNo < MAX_PALETTE )
					{
						if( x == 0 )
						{
							// デフォルト値を設定
							aAdjacent_no_edge_def[ targetMaterialNo ] = flag;
						}
						else
						{
							aAdjacent_no_edge[ targetMaterialNo ] = flag;

							// 隣接マテリアル情報を描画しなおす
							DrawAdjacent( &dst, x - 1, aAdjacent_no_edge );
						}
					}

					p += 3;
				}
				break;

			case COMMAND_ADJUST_RATE:
				{
					float n = ( (float *)p )[ 1 ];

					adjustRate = n;

					p += 2;
				}
				break;
			}
		}
	}


	// save
	{
		std::string strName;

		if( IsPngFilename( szDstFileName ) )
		{
			strName = szDstFileName;
		}
		else
		{
			strName = szDstFileName;
			strName += std::string( ".png" );
		}

//		dst.WriteToFile( strName.c_str() );
		CPngRW::SavePng( strName.c_str(), dst );
	}

	// save debug
//	dstDebug.WriteToFile( "palette_debug.bmp" );
	CPngRW::SavePng( "palette_debug.png", dstDebug );


	if( pData )
		delete [] pData;
}
