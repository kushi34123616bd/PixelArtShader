#include <windows.h>
#include <stdio.h>
#include <ctype.h>
//#include <math.h>

#include "EasyBMP\EasyBMP.h"
#include "libpng\png.h"
#include "libpng\pngstruct.h"

#include "CPngRW.h"


#define PNG_BYTES_TO_CHECK	4



static bool check_if_png( const char *file_name, FILE **fp )
{
	char sig[ PNG_BYTES_TO_CHECK ];

	errno_t err = fopen_s( fp, file_name, "rb" );
	if( err != 0 )
		return false;

	if( fread( sig, 1, PNG_BYTES_TO_CHECK, *fp ) != PNG_BYTES_TO_CHECK )
	{
		fclose( *fp );

		return false;
	}

	if( ! png_check_sig( (png_const_bytep)sig, PNG_BYTES_TO_CHECK ) )
	{
		printf( "%s is not .png", file_name );

		fclose( *fp );

		return false;
	}

	return true;
}


static bool read_png_info( FILE *fp, png_structp *png_ptr, png_infop *info_ptr )
{
	*png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
	if( *png_ptr == NULL )
	{
		fclose( fp );

		return false;
	}

	*info_ptr = png_create_info_struct( *png_ptr );
	if( *info_ptr == NULL )
	{
		png_destroy_read_struct( png_ptr, (png_infopp)NULL, (png_infopp)NULL );
		fclose( fp );

		return false;
	}

	if( setjmp( (*png_ptr)->jmp_buf_local ) )
	{
		png_destroy_read_struct( png_ptr, info_ptr, (png_infopp)NULL );
		fclose( fp );

		return false;
	}

	png_init_io( *png_ptr, fp );
	png_set_sig_bytes( *png_ptr, PNG_BYTES_TO_CHECK );
	png_read_info( *png_ptr, *info_ptr );

	return true;
}


static bool read_png_image( FILE *fp, png_structp png_ptr, png_infop info_ptr, png_bytepp *image, png_uint_32 *width, png_uint_32 *height )
{
	*width = png_get_image_width( png_ptr, info_ptr );
	*height = png_get_image_height( png_ptr, info_ptr );

	*image = (png_bytepp)malloc( *height * sizeof(png_bytep) );
	if( *image == NULL )
	{
		fclose( fp );

		return false;
	}

	for( png_uint_32 i = 0;  i < *height;  i ++ )
	{
		(*image)[ i ] = (png_bytep)malloc( png_get_rowbytes( png_ptr, info_ptr ) );
		if( (*image)[i] == NULL )
		{
			for( png_uint_32 j = 0;  j < i;  j ++ )
				free( (*image)[ j ] );
			free( *image );
			fclose( fp );

			return false;
		}
	}

	png_read_image( png_ptr, *image );

	return true;
}


bool CPngRW::LoadPng( const char *szFilename, BMP &bmp )
{
	FILE *fp;
	png_structp png_ptr;
	png_infop info_ptr;
	unsigned char **image;
	unsigned long width, height;

	if( ! check_if_png( szFilename, &fp ) )
		return false;

	if( ! read_png_info( fp, &png_ptr, &info_ptr ) )
		return false;

	if( ! read_png_image( fp, png_ptr, info_ptr, &image, (png_uint_32 *)&width, (png_uint_32 *)&height ) )
		return false;


	int channel = png_get_channels( png_ptr, info_ptr );

/*
	printf( "%s : %dx%d (channel:%d)\n", szFilename, width, height, channel );

	// gAMAチャンクを取得し表示します
	{
		double file_gamma;

		if( png_get_gAMA( png_ptr, info_ptr, &file_gamma ) )
			printf( "gamma = %lf\n", file_gamma );
	}

	// tEXtチャンクを取得し表示します
	{
		png_textp text_ptr;
		int num_text;

		if( png_get_text( png_ptr, info_ptr, &text_ptr, &num_text ) )
			for( int i = 0;  i < num_text;  i ++ )
				printf( "%s = %s\n", text_ptr[ i ].key, text_ptr[ i ].text );
	}
*/


	{
		int w = (int)width;
		int h = (int)height;

		bmp.SetSize( w, h );
		bmp.SetBitDepth( 32 );

		RGBApixel col;

		for( int y = 0;  y < h;  y ++ )
		{
			for( int x = 0;  x < w;  x ++ )
			{
				unsigned char *pData = &image[ y ][ x * channel ];

				col.Red =	(ebmpBYTE)pData[ 0 ];
				col.Green =	(ebmpBYTE)pData[ 1 ];
				col.Blue =	(ebmpBYTE)pData[ 2 ];
				col.Alpha =	channel == 4 ? (ebmpBYTE)pData[ 3 ] : 0xff;

				bmp.SetPixel( x, y, col );
			}
		}

	}


	for( unsigned long i = 0;  i < height;  i ++ )
		free( image[ i ] );
	free( image );

	png_destroy_read_struct( &png_ptr, &info_ptr, (png_infopp)NULL );
	fclose( fp );


	return true;
}


bool CPngRW::SavePng( const char *szFilename, BMP &bmp )
{
	FILE *fp = NULL;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	unsigned char **image;


	int bitDepth = bmp.TellBitDepth();
	if( bitDepth != 24  &&  bitDepth != 32 )
	{
		printf( "bmp must be 24 or 32 bit depth\n" );
		return false;
	}

	int channel = bitDepth / 8;
	int width = bmp.TellWidth();
	int height = bmp.TellHeight();


	errno_t err = fopen_s( &fp, szFilename, "wb" );
	if( err != 0 )
		goto error;


	png_ptr = png_create_write_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
	if( png_ptr == NULL )
		goto error;

	info_ptr = png_create_info_struct( png_ptr );
	if( info_ptr == NULL )
		goto error;

	if( setjmp( png_ptr->jmp_buf_local ) )
		goto error;

	// 画像をコピー
	{
		image = (png_bytepp)malloc( sizeof(png_bytep) * height );
		for( int y = 0;  y < height;  y ++ )
			image[ y ] = (png_bytep)malloc( sizeof(png_byte) * channel * width );

		for( int y = 0;  y < height;  y ++ )
		{
			for( int x = 0;  x < width;  x ++ )
			{
				unsigned char *pData = &image[ y ][ x * channel ];

				RGBApixel *pCol = bmp( x, y );

				pData[ 0 ] = pCol->Red;
				pData[ 1 ] = pCol->Green;
				pData[ 2 ] = pCol->Blue;
				if( channel == 4 )
					pData[ 3 ] = pCol->Alpha;
			}
		}
	}

	png_init_io( png_ptr, fp );
//	png_set_write_status_fn( png_ptr, write_row_callback );
//	png_set_filter( png_ptr, 0, PNG_ALL_FILTERS );
	png_set_compression_level( png_ptr, Z_BEST_COMPRESSION );
	png_set_IHDR(	png_ptr, info_ptr, width, height, 8, channel == 3 ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_RGB_ALPHA,
					PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT );
//	png_set_gAMA( png_ptr, info_ptr, 1.0 );

	png_write_info( png_ptr, info_ptr );
	png_write_image( png_ptr, image );
	png_write_end( png_ptr, info_ptr );

	for( int y = 0;  y < height;  y ++ )
		free( image[ y ] );
	free( image );

	png_destroy_write_struct( &png_ptr, &info_ptr );
	fclose( fp );


	return true;


error:

	if( png_ptr )
	{
		if( info_ptr )
			png_destroy_write_struct( &png_ptr, &info_ptr );
		else
			png_destroy_write_struct( &png_ptr, (png_infopp)NULL );
	}

	if( fp )
		fclose( fp );

	return false;
}
