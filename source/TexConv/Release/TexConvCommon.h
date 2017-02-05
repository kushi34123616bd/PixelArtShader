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

	COMMAND_DEST_NAME,

	COMMAND_TEX_MATERIAL,
	COMMAND_TEX_LIGHT,
	COMMAND_SET,
};



macro dest_name str filename
{
	Raw COMMAND_DEST_NAME, filename
}


macro tex_material str texname
{
	Raw COMMAND_TEX_MATERIAL, texname
}


macro tex_light str texname
{
	Raw COMMAND_TEX_LIGHT, texname
}


macro set int palette, int r, int g, int b
{
	Raw COMMAND_SET, palette, r, g, b
}


// フッタ（スクリプトの最後に自動的に呼ばれる）
macro footer
{
	Raw COMMAND_END
}
