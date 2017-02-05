#include "DXUT.h"

#include "misc.h"


///////////////////////////////////////////////////////////////////////////////

typedef struct
{
	union
	{
		struct
		{
			BYTE b, g, r, a;
		};

		BYTE e[ 4 ];
	};

} sARGB8;


///////////////////////////////////////////////////////////////////////////////

wchar_t g_wchar_t_temp[ GLB_LENGTH_g_wchar_t_temp ];
char g_char_temp[ GLB_LENGTH_g_wchar_t_temp ];

wchar_t *GetWideChar( const char *str )
{
	MultiByteToWideChar( CP_ACP, 0, str, -1, g_wchar_t_temp, GLB_LENGTH_g_wchar_t_temp );

	return g_wchar_t_temp;
}

void ConvWideChar( wchar_t *pOut, int buffsize, const char *str )
{
	MultiByteToWideChar( CP_ACP, 0, str, -1, pOut, buffsize );
}


char *GetMultiByte( const wchar_t *str )
{
	WideCharToMultiByte( CP_ACP, 0, str, -1, g_char_temp, GLB_LENGTH_g_wchar_t_temp, NULL, NULL );

	return g_char_temp;
}

void ConvMultiByte( char *pOut, int buffsize, const wchar_t *str )
{
	WideCharToMultiByte( CP_ACP, 0, str, -1, pOut, buffsize, NULL, NULL );

}


///////////////////////////////////////////////////////////////////////////////

void SaveTexture( LPDIRECT3DTEXTURE9 pTex, WCHAR *szPath, const wchar_t *format, ... )
{
	LPDIRECT3DSURFACE9 pSurf = NULL;
	pTex->GetSurfaceLevel( 0, &pSurf );


//	WCHAR strCWD[ MAX_PATH ];
//	GetCurrentDirectory( MAX_PATH, strCWD );

//	if( SetCurrentDirectory( szPath ) )
	{
		WCHAR strFileName[ MAX_PATH ];
		va_list args;

		va_start( args, format );
		vswprintf_s( strFileName, MAX_PATH, format, args );
		va_end( args );

		D3DXSaveSurfaceToFile(	strFileName,
								D3DXIFF_BMP,
								pSurf,
								NULL, NULL );
	}

//	SetCurrentDirectory( strCWD );


	SAFE_RELEASE( pSurf );
}


void SaveTexture_Channel( IDirect3DDevice9* pd3dDevice, LPDIRECT3DTEXTURE9 pTex, int channel, WCHAR *szPath, const wchar_t *format, ... )
{
	HRESULT hr;


	D3DSURFACE_DESC desc;
	pTex->GetLevelDesc( 0, &desc );


	LPDIRECT3DTEXTURE9 pTexWork = NULL;

	hr = pd3dDevice->CreateTexture( desc.Width, desc.Height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &pTexWork, NULL );
	if( FAILED( hr ) )
	{
		return;
	}



	{
		LPDIRECT3DSURFACE9 pSurf = NULL;
		pTex->GetSurfaceLevel( 0, &pSurf );

		LPDIRECT3DSURFACE9 pSurfWork = NULL;
		pTexWork->GetSurfaceLevel( 0, &pSurfWork );

		pd3dDevice->GetRenderTargetData( pSurf, pSurfWork );

		SAFE_RELEASE( pSurfWork );
		SAFE_RELEASE( pSurf );
	}


	{
/*
typedef struct D3DLOCKED_RECT {
    INT Pitch;
    void * pBits;
} D3DLOCKED_RECT, *LPD3DLOCKED_RECT;
*/

		D3DLOCKED_RECT rect;

		V( pTexWork->LockRect( 0, &rect, NULL, 0 ) );

		for( unsigned int y = 0;  y < desc.Height;  y ++ )
		{
			sARGB8 *pCol = (sARGB8 *)( (BYTE *)rect.pBits + rect.Pitch * y );

			for( unsigned int x = 0;  x < desc.Width;  x ++ )
			{
				BYTE value = pCol[ x ].e[ channel ];

				pCol[ x ].r = value;
				pCol[ x ].g = 0;
				pCol[ x ].b = 0;
				pCol[ x ].a = 0;
			}
		}

		pTexWork->UnlockRect( 0 );
	}

	{
		WCHAR strFileName[ MAX_PATH ];
		va_list args;

		va_start( args, format );
		vswprintf_s( strFileName, MAX_PATH, format, args );
		va_end( args );

		SaveTexture( pTexWork, szPath, strFileName );
	}

	SAFE_RELEASE( pTexWork );
}


///////////////////////////////////////////////////////////////////////////////

int GetProcessorNum( void )
{
	static int num = 0;

	if( num == 0 )
	{
		SYSTEM_INFO info;

		::GetSystemInfo( &info );

		num = info.dwNumberOfProcessors;

		if( num == 0 )
		{
			num = 1;
		}
	}

	return num;
}
