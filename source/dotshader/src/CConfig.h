
#include "CTecoDll.h"

typedef struct
{
	bool bDebug;

	int windowWidth;
	int windowHeight;

	char szModelName[ MAX_PATH ];

	bool bCapture;

	char szCapturePath[ MAX_PATH ];
	int resolution[ 2 ];

	int resolution_inner_max[ 2 ];

	float camera_pos[ 3 ];
	float camera_lookat[ 3 ];
	float camera_fov_y;
	float camera_ortho_height;
	float camera_z_near;
	float camera_z_far;
	bool camera_bIgnoreData;

	int anime_set_frame;

	int result_disp_scale;


	// シェーダーパラメータ

	float shader_ZThreshold;
	float shader_ZThreshold_min;
	float shader_ZThreshold_max;

	float shader_AngleThreshold;
	float shader_AngleThreshold_min;
	float shader_AngleThreshold_max;

	float shader_GutterThreshold;
	float shader_GutterThreshold_min;
	float shader_GutterThreshold_max;

	float shader_IgnoreCountThreshold;
	float shader_IgnoreCountThreshold_min;
	float shader_IgnoreCountThreshold_max;


} SConfig;


class CConfig
{
public:

	CConfig(){}
	~CConfig(){}


	bool Init( void );

	void Load( char *szName );

	SConfig *GetConfig( void )
	{
		return &m_config;
	}



	CTecoDll m_tecoDll;

	SConfig m_config;
};
