///////////////////////////////////////////////////////////////////////////////
//
//	共通ヘッダ
//
///////////////////////////////////////////////////////////////////////////////

// コンバータの設定
// 0:false 1:true
#setting "NeedVariableDeclaration"		1		// 変数宣言が必要か(default:false)
#setting "DefaultVariableType"			0		// デフォルト型 0:int 1:uint 2:float 3:str 4:label (default:int)

#setting "NoHeader"						1		// ヘッダを出力しない(default:false)
#setting "ResumableSegment"				0		// セグメントの再開が可能か(default:true)
#setting "MixableCodeData"				0		// セグメントにコードとデータが混在するのを許可(default:false)
#setting "DeleteEmptySegment"			1		// 空のセグメントを削除するか(default:true)
#setting "MacroLoopMax"					2000	// マクロループの繰り返し回数の上限(default:2000)

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


#define true	1
#define false	0



// フッタ（スクリプトの最後に自動的に呼ばれる）
macro footer
{
	Raw COMMAND_END
}


macro debug int b
{
	Raw COMMAND_DEBUG, b
}


macro window_size int w, int h
{
	Raw COMMAND_WINDOW_SIZE, w, h
}


macro model_name str name
{
	Raw COMMAND_MODEL_NAME, name
}


macro capture int b
{
	Raw COMMAND_CAPTURE, b
}


macro capture_path str path
{
	Raw COMMAND_CAPTURE_PATH, path
}


macro resolution int w, int h
{
	Raw COMMAND_RESOLUTION, w, h
}


macro resolution_inner_max int w, int h
{
	Raw COMMAND_RESOLUTION_INNER_MAX, w, h
}


macro camera_pos float x, float y, float z
{
	Raw COMMAND_CAMERA_POS, x, y, z
}


macro camera_lookat float x, float y, float z
{
	Raw COMMAND_CAMERA_LOOKAT, x, y, z
}


macro camera_fov_y float fovy
{
	Raw COMMAND_CAMERA_FOV_Y, fovy
}


macro camera_ortho_height float height
{
	Raw COMMAND_CAMERA_ORTHO_HEIGHT, height
}


macro camera_z_near float z
{
	Raw COMMAND_CAMERA_Z_NEAR, z
}


macro camera_z_far float z
{
	Raw COMMAND_CAMERA_Z_FAR, z
}


// モデルデータ内にカメラ情報があっても無視する(デフォルト:false)
// b : true(無視する) or false(無視しない)
macro camera_ignore_data int b
{
	Raw COMMAND_CAMERA_IGNORE_DATA, b
}



// 表示するフレーム（時間）を指定
// ※アニメーションデータが存在する場合のみ有効
// ※指定しなければ、アニメーションの全てのフレームが表示される
// ※0が最初のフレーム、次が1
macro anime_set_frame int frame
{
	Raw COMMAND_ANIME_SET_FRAME, frame
}



macro result_disp_scale int n
{
	Raw COMMAND_RESULT_DISP_SCALE, n
}



macro shader_z_threshold float n
{
	Raw COMMAND_SHADER_Z_THRESHOLD, n
}
macro shader_z_threshold_min float n
{
	Raw COMMAND_SHADER_Z_THRESHOLD_MIN, n
}
macro shader_z_threshold_max float n
{
	Raw COMMAND_SHADER_Z_THRESHOLD_MAX, n
}


macro shader_angle_threshold float n
{
	Raw COMMAND_SHADER_ANGLE_THRESHOLD, n
}
macro shader_angle_threshold_min float n
{
	Raw COMMAND_SHADER_ANGLE_THRESHOLD_MIN, n
}
macro shader_angle_threshold_max float n
{
	Raw COMMAND_SHADER_ANGLE_THRESHOLD_MAX, n
}


macro shader_gutter_threshold float n
{
	Raw COMMAND_SHADER_GUTTER_THRESHOLD, n
}
macro shader_gutter_threshold_min float n
{
	Raw COMMAND_SHADER_GUTTER_THRESHOLD_MIN, n
}
macro shader_gutter_threshold_max float n
{
	Raw COMMAND_SHADER_GUTTER_THRESHOLD_MAX, n
}


macro shader_ignore_count_threshold float n
{
	Raw COMMAND_SHADER_IGNORE_COUNT_THRESHOLD, n
}
macro shader_ignore_count_threshold_min float n
{
	Raw COMMAND_SHADER_IGNORE_COUNT_THRESHOLD_MIN, n
}
macro shader_ignore_count_threshold_max float n
{
	Raw COMMAND_SHADER_IGNORE_COUNT_THRESHOLD_MAX, n
}
