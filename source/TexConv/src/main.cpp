#include <windows.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include "EasyBMP\EasyBMP.h"
//#include "libpng\png.h"
#include "CTecoDll.h"
#include "CPngRW.h"


///////////////////////////////////////////////////////////////////////////////

// 命令の種類
enum
{
	COMMAND_END,		// スクリプトの終わり

	COMMAND_DEST_NAME,

	COMMAND_TEX_MATERIAL,
	COMMAND_TEX_AO,
	COMMAND_TEX_LIGHT,
	COMMAND_SET,

	COMMAND_AO_RATE,
	COMMAND_LIGHT_RATE,

	COMMAND_LIGHT_VALUE,
};


///////////////////////////////////////////////////////////////////////////////

typedef struct
{
	BYTE palette;

//	BYTE r, g, b;
	RGBApixel col;

} SConvInfo;


SConvInfo convInfo[ 256 ];
int convInfoNum = 0;

///////////////////////////////////////////////////////////////////////////////

template <typename T>
inline T Abs( T a )
{
	return 0 <= a ? a : ( -a );
}

template <typename T>
inline T Clamp( T n, T min, T max )
{
	if( n < min )
		return min;

	if( max < n )
		return max;

	return n;
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


void main( int argc, char *argv[] )
{
	if( argc != 2 )
	{
		printf( "TexConv.exe ScriptFileName\n" );
		return;
	}

	char *szScriptFileName = argv[ 1 ];

	char *szDstFileName = NULL;
	char *szTexFileName_Material = NULL;
	char *szTexFileName_AO = NULL;
	char *szTexFileName_Light = NULL;

	float ao_rate = 1;
	float light_rate = 1;

	ebmpBYTE light_value = 0;
	bool b_light_value = false;	// 値の指定をしたか


	CTecoDll tecoDll;

	if( ! tecoDll.Init() )
	{
		printf( "tecoDll.Init()\n" );
		return;
	}


	BYTE *pData = NULL;
	int dataSize = 0;

	tecoDll.Convert( szScriptFileName, pData, dataSize );

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





	{
		int *p = (int *)pData;
		bool bEnd = false;

		while( ! bEnd )
		{
			switch( *p )
			{
			case COMMAND_END:
				bEnd = true;
				break;


			case COMMAND_DEST_NAME:
				{
					szDstFileName = (char *)( &( (BYTE *)pData )[ p[ 1 ] ] );

					p += 2;
				}
				break;

			case COMMAND_TEX_MATERIAL:
				{
					szTexFileName_Material = (char *)( &( (BYTE *)pData )[ p[ 1 ] ] );

					p += 2;
				}
				break;

			case COMMAND_TEX_AO:
				{
					szTexFileName_AO = (char *)( &( (BYTE *)pData )[ p[ 1 ] ] );

					p += 2;
				}
				break;

			case COMMAND_TEX_LIGHT:
				{
					szTexFileName_Light = (char *)( &( (BYTE *)pData )[ p[ 1 ] ] );

					p += 2;
				}
				break;

			case COMMAND_SET:
				{
					convInfo[ convInfoNum ].palette = p[ 1 ];
					convInfo[ convInfoNum ].col.Red = p[ 2 ];
					convInfo[ convInfoNum ].col.Green = p[ 3 ];
					convInfo[ convInfoNum ].col.Blue = p[ 4 ];

					convInfoNum ++;

					p += 5;
				}
				break;

			case COMMAND_AO_RATE:
				{
					float *pF = (float *)p;

					ao_rate = pF[ 1 ];

					Clamp01( ao_rate );

					p += 2;
				}
				break;

			case COMMAND_LIGHT_RATE:
				{
					float *pF = (float *)p;

					light_rate = pF[ 1 ];

					p += 2;
				}
				break;

			case COMMAND_LIGHT_VALUE:
				{
					float *pF = (float *)p;

					float value = pF[ 1 ];

					Clamp01( value );

					light_value = (ebmpBYTE)( value * 255.0f );

					b_light_value = true;

					szTexFileName_Light = NULL;	// ライトマップの指定が無効になる

					p += 2;
				}
				break;
			}
		}
	}



	BMP src_material;
	BMP src_ao;
	BMP src_light;
	BMP dst;

	int width = 0, height = 0;


	if( szDstFileName == NULL  ||  ( szTexFileName_Material == NULL  &&  szTexFileName_AO == NULL  &&  ( szTexFileName_Light == NULL  &&  b_light_value == false ) ) )
	{
		printf( "specify file names or light_value\n" );
		goto end;
	}



	if( szTexFileName_Material )
	{
		bool ret;

		if( IsPngFilename( szTexFileName_Material ) )
			ret = CPngRW::LoadPng( szTexFileName_Material, src_material );
		else
			ret = src_material.ReadFromFile( szTexFileName_Material );

		if( ! ret )
		{
			printf( "file can't open: %s\n", szTexFileName_Material );
			goto end;
		}

		width = src_material.TellWidth();
		height = src_material.TellHeight();
	}

	if( szTexFileName_AO )
	{
		bool ret;

		if( IsPngFilename( szTexFileName_AO ) )
			ret = CPngRW::LoadPng( szTexFileName_AO, src_ao );
		else
			ret = src_ao.ReadFromFile( szTexFileName_AO );

		if( ! ret )
		{
			printf( "file can't open: %s\n", szTexFileName_AO );
			goto end;
		}

		// テクスチャのサイズは同じ必要がある
		if( width != 0 )
		{
			if( width != src_ao.TellWidth()  ||  height != src_ao.TellHeight() )
			{
				printf( "textures are not same size\n" );
				goto end;
			}
		}
		else
		{
			width = src_ao.TellWidth();
			height = src_ao.TellHeight();
		}
	}

	if( szTexFileName_Light )
	{
		bool ret;

		if( IsPngFilename( szTexFileName_Light ) )
			ret = CPngRW::LoadPng( szTexFileName_Light, src_light );
		else
			ret = src_light.ReadFromFile( szTexFileName_Light );

		if( ! ret )
		{
			printf( "file can't open: %s\n", szTexFileName_Light );
			goto end;
		}

		// テクスチャのサイズは同じ必要がある
		if( width != 0 )
		{
			if( width != src_light.TellWidth()  ||  height != src_light.TellHeight() )
			{
				printf( "textures are not same size\n" );
				goto end;
			}
		}
		else
		{
			width = src_light.TellWidth();
			height = src_light.TellHeight();
		}
	}


	dst.SetSize( width, height );
	dst.SetBitDepth( 24 );


	// 初期化
	for( int y = 0;  y < height;  y ++ )
	{
		for( int x = 0;  x < width;  x ++ )
		{
			dst( x, y )->Red = 0;
			dst( x, y )->Green = 255;
			dst( x, y )->Blue = light_value;
		}
	}


	if( szTexFileName_Material )
	{
		int i;

		for( int y = 0;  y < height;  y ++ )
		{
			for( int x = 0;  x < width;  x ++ )
			{
				RGBApixel *pRGBA = src_material( x, y );

				// 登録されている中で最も似た色を探す
				BYTE index = 0;
				int minError = 9999;

				for( i = 0;  i < convInfoNum;  i ++ )
				{
					int error =	  Abs( pRGBA->Red - convInfo[ i ].col.Red )
								+ Abs( pRGBA->Green - convInfo[ i ].col.Green )
								+ Abs( pRGBA->Blue - convInfo[ i ].col.Blue );

					if( error < minError )
					{
						minError = error;
						index = i;
					}
				}

				dst( x, y )->Red = convInfo[ index ].palette;
			}
		}
	}

	if( szTexFileName_AO )
	{
		for( int y = 0;  y < height;  y ++ )
		{
			for( int x = 0;  x < width;  x ++ )
			{
				RGBApixel *pRGBA = src_ao( x, y );

				dst( x, y )->Green = (ebmpBYTE)Lerp( 255, (int)pRGBA->Red, ao_rate );
			}
		}
	}

	if( szTexFileName_Light )
	{
		for( int y = 0;  y < height;  y ++ )
		{
			for( int x = 0;  x < width;  x ++ )
			{
				RGBApixel *pRGBA = src_light( x, y );

				dst( x, y )->Blue = (ebmpBYTE)Clamp( (int)( pRGBA->Red * light_rate ), 0, 255 );
			}
		}
	}




	if( IsPngFilename( szDstFileName ) )
		CPngRW::SavePng( szDstFileName, dst );
	else
		dst.WriteToFile( szDstFileName );





end:

	if( pData )
		delete [] pData;
}
