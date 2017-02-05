#include "DXUT.h"

#include "printf.h"
#include "misc.h"
#include "CConfig.h"


///////////////////////////////////////////////////////////////////////////////

// 命令の種類
enum
{
	COMMAND_END,		// スクリプトの終わり

	COMMAND_DEBUG,


	COMMAND_WINDOW_SIZE,
	COMMAND_MODEL_NAME,
	COMMAND_CAPTURE,
	COMMAND_CAPTURE_PATH,
	COMMAND_RESOLUTION,

	COMMAND_RESOLUTION_INNER_MAX,

	COMMAND_CAMERA_POS,
	COMMAND_CAMERA_LOOKAT,
	COMMAND_CAMERA_FOV_Y,
	COMMAND_CAMERA_ORTHO_HEIGHT,
	COMMAND_CAMERA_Z_NEAR,
	COMMAND_CAMERA_Z_FAR,
	COMMAND_CAMERA_IGNORE_DATA,

	COMMAND_ANIME_SET_FRAME,

	COMMAND_RESULT_DISP_SCALE,


	COMMAND_SHADER_Z_THRESHOLD,
	COMMAND_SHADER_Z_THRESHOLD_MIN,
	COMMAND_SHADER_Z_THRESHOLD_MAX,

	COMMAND_SHADER_ANGLE_THRESHOLD,
	COMMAND_SHADER_ANGLE_THRESHOLD_MIN,
	COMMAND_SHADER_ANGLE_THRESHOLD_MAX,

	COMMAND_SHADER_GUTTER_THRESHOLD,
	COMMAND_SHADER_GUTTER_THRESHOLD_MIN,
	COMMAND_SHADER_GUTTER_THRESHOLD_MAX,

	COMMAND_SHADER_IGNORE_COUNT_THRESHOLD,
	COMMAND_SHADER_IGNORE_COUNT_THRESHOLD_MIN,
	COMMAND_SHADER_IGNORE_COUNT_THRESHOLD_MAX,
};

///////////////////////////////////////////////////////////////////////////////

bool CConfig::Init( void )
{
	// 設定初期化
	{
		m_config.bDebug = false;

		m_config.windowWidth = 640;
		m_config.windowHeight = 480;

		m_config.szModelName[ 0 ] = '\0';

		m_config.bCapture = false;

		m_config.szCapturePath[ 0 ] = '\0';

		m_config.resolution[ 0 ] = 256;
		m_config.resolution[ 1 ] = 256;

		m_config.resolution_inner_max[ 0 ] = 1024 * 2;
		m_config.resolution_inner_max[ 1 ] = 1024 * 2;

		m_config.camera_pos[ 0 ] = 0;
		m_config.camera_pos[ 1 ] = 0;
		m_config.camera_pos[ 2 ] = -10;

		m_config.camera_lookat[ 0 ] = 0;
		m_config.camera_lookat[ 1 ] = 0;
		m_config.camera_lookat[ 2 ] = 0;

		m_config.camera_fov_y = 45.0f;
		m_config.camera_ortho_height = 0;

		m_config.camera_z_near = 0;
		m_config.camera_z_far = 0;

		m_config.camera_bIgnoreData = false;

		m_config.anime_set_frame = -1;

		m_config.result_disp_scale = 1;


		// シェーダーパラメータ
		m_config.shader_ZThreshold = 0;
		m_config.shader_ZThreshold_min = 0;
		m_config.shader_ZThreshold_max = 0;
		m_config.shader_AngleThreshold = 0;
		m_config.shader_AngleThreshold_min = 0;
		m_config.shader_AngleThreshold_max = 0;
		m_config.shader_GutterThreshold = 0;
		m_config.shader_GutterThreshold_min = 0;
		m_config.shader_GutterThreshold_max = 0;
		m_config.shader_IgnoreCountThreshold = 0;
		m_config.shader_IgnoreCountThreshold_min = 0;
		m_config.shader_IgnoreCountThreshold_max = 0;
	}


	if( ! m_tecoDll.Init() )
	{
		printf( L"tecoDll.Init()\n" );
		return false;
	}

	return true;
}


void CConfig::Load( char *szName )
{
	BYTE *pData = NULL;
	int dataSize = 0;

	m_tecoDll.Convert( szName, pData, dataSize );

	if( m_tecoDll.GetError() != CTecoDll::RET_SUCCESS )
	{
		printf( L"teco: %s", GetWideChar( m_tecoDll.GetErrorMessage() ) );
		if( pData )
			delete [] pData;
		return;
	}

	if( dataSize == 0 )
	{
		printf( L"empty" );
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


			case COMMAND_DEBUG:
				{
					m_config.bDebug = false;
					if( p[ 1 ] )
						m_config.bDebug = true;

					p += 2;
				}
				break;

			case COMMAND_WINDOW_SIZE:
				{
					m_config.windowWidth = p[ 1 ];
					m_config.windowHeight = p[ 2 ];
					p += 3;
				}
				break;

			case COMMAND_MODEL_NAME:
				{
					strcpy_s( m_config.szModelName, MAX_PATH, (char *)( &pData[ p[ 1 ] ] ) );
					p += 2;
				}
				break;

			case COMMAND_CAPTURE:
				{
					m_config.bCapture = false;
					if( p[ 1 ] )
						m_config.bCapture = true;

					p += 2;
				}
				break;

			case COMMAND_CAPTURE_PATH:
				{
					strcpy_s( m_config.szCapturePath, MAX_PATH, (char *)( &pData[ p[ 1 ] ] ) );
					p += 2;
				}
				break;

			case COMMAND_RESOLUTION:
				{
					m_config.resolution[ 0 ] = p[ 1 ];
					m_config.resolution[ 1 ] = p[ 2 ];
					p += 3;
				}
				break;

			case COMMAND_RESOLUTION_INNER_MAX:
				{
					m_config.resolution_inner_max[ 0 ] = p[ 1 ];
					m_config.resolution_inner_max[ 1 ] = p[ 2 ];
					p += 3;
				}
				break;

			case COMMAND_CAMERA_POS:
				{
					m_config.camera_pos[ 0 ] = ((float *)p)[ 1 ];
					m_config.camera_pos[ 1 ] = ((float *)p)[ 2 ];
					m_config.camera_pos[ 2 ] = ((float *)p)[ 3 ];
					p += 4;
				}
				break;

			case COMMAND_CAMERA_LOOKAT:
				{
					m_config.camera_lookat[ 0 ] = ((float *)p)[ 1 ];
					m_config.camera_lookat[ 1 ] = ((float *)p)[ 2 ];
					m_config.camera_lookat[ 2 ] = ((float *)p)[ 3 ];
					p += 4;
				}
				break;

			case COMMAND_CAMERA_FOV_Y:
				{
					m_config.camera_fov_y = ((float *)p)[ 1 ];
					p += 2;

					// 正射影カメラを無効にする
					m_config.camera_ortho_height = 0;
				}
				break;

			case COMMAND_CAMERA_ORTHO_HEIGHT:
				{
					m_config.camera_ortho_height = ((float *)p)[ 1 ];
					p += 2;

					// パースペクティブカメラを無効にする
					m_config.camera_fov_y = 0;
				}
				break;

			case COMMAND_CAMERA_Z_NEAR:
				{
					m_config.camera_z_near = ((float *)p)[ 1 ];
					p += 2;
				}
				break;

			case COMMAND_CAMERA_Z_FAR:
				{
					m_config.camera_z_far = ((float *)p)[ 1 ];
					p += 2;
				}
				break;

			case COMMAND_CAMERA_IGNORE_DATA:
				{
					m_config.camera_bIgnoreData = ( p[ 1 ] != false );
					p += 2;
				}
				break;

			case COMMAND_ANIME_SET_FRAME:
				{
					m_config.anime_set_frame = p[ 1 ];
					p += 2;
				}
				break;

			case COMMAND_RESULT_DISP_SCALE:
				{
					m_config.result_disp_scale = p[ 1 ];
					p += 2;
				}
				break;


			case COMMAND_SHADER_Z_THRESHOLD:
				{
					m_config.shader_ZThreshold = ((float *)p)[ 1 ];
					p += 2;
				}
				break;
			case COMMAND_SHADER_Z_THRESHOLD_MIN:
				{
					m_config.shader_ZThreshold_min = ((float *)p)[ 1 ];
					p += 2;
				}
				break;
			case COMMAND_SHADER_Z_THRESHOLD_MAX:
				{
					m_config.shader_ZThreshold_max = ((float *)p)[ 1 ];
					p += 2;
				}
				break;

			case COMMAND_SHADER_ANGLE_THRESHOLD:
				{
					m_config.shader_AngleThreshold = ((float *)p)[ 1 ];
					p += 2;
				}
				break;
			case COMMAND_SHADER_ANGLE_THRESHOLD_MIN:
				{
					m_config.shader_AngleThreshold_min = ((float *)p)[ 1 ];
					p += 2;
				}
				break;
			case COMMAND_SHADER_ANGLE_THRESHOLD_MAX:
				{
					m_config.shader_AngleThreshold_max = ((float *)p)[ 1 ];
					p += 2;
				}
				break;

			case COMMAND_SHADER_GUTTER_THRESHOLD:
				{
					m_config.shader_GutterThreshold = ((float *)p)[ 1 ];
					p += 2;
				}
				break;
			case COMMAND_SHADER_GUTTER_THRESHOLD_MIN:
				{
					m_config.shader_GutterThreshold_min = ((float *)p)[ 1 ];
					p += 2;
				}
				break;
			case COMMAND_SHADER_GUTTER_THRESHOLD_MAX:
				{
					m_config.shader_GutterThreshold_max = ((float *)p)[ 1 ];
					p += 2;
				}
				break;

			case COMMAND_SHADER_IGNORE_COUNT_THRESHOLD:
				{
					m_config.shader_IgnoreCountThreshold = ((float *)p)[ 1 ];
					p += 2;
				}
				break;
			case COMMAND_SHADER_IGNORE_COUNT_THRESHOLD_MIN:
				{
					m_config.shader_IgnoreCountThreshold_min = ((float *)p)[ 1 ];
					p += 2;
				}
				break;
			case COMMAND_SHADER_IGNORE_COUNT_THRESHOLD_MAX:
				{
					m_config.shader_IgnoreCountThreshold_max = ((float *)p)[ 1 ];
					p += 2;
				}
				break;
			}
		}
	}
}
