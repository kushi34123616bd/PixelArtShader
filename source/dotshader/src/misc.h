
///////////////////////////////////////////////////////////////////////////////

#define GLB_LENGTH_g_wchar_t_temp	( MAX_PATH * 40 )

extern wchar_t g_wchar_t_temp[ GLB_LENGTH_g_wchar_t_temp ];
extern char g_char_temp[ GLB_LENGTH_g_wchar_t_temp ];

extern wchar_t *GetWideChar( const char *str );
extern void ConvWideChar( wchar_t *pOut, int buffsize, const char *str );

extern char *GetMultiByte( const wchar_t *str );
extern void ConvMultiByte( char *pOut, int buffsize, const wchar_t *str );

///////////////////////////////////////////////////////////////////////////////

enum
{
	MISC_COLOR_CHANNEL_B = 0,
	MISC_COLOR_CHANNEL_G,
	MISC_COLOR_CHANNEL_R,
	MISC_COLOR_CHANNEL_A,
};

void SaveTexture( LPDIRECT3DTEXTURE9 pTex, WCHAR *szPath, const wchar_t *format, ... );

void SaveTexture_Channel( IDirect3DDevice9* pd3dDevice, LPDIRECT3DTEXTURE9 pTex, int channel, WCHAR *szPath, const wchar_t *format, ... );

///////////////////////////////////////////////////////////////////////////////

int GetProcessorNum( void );
