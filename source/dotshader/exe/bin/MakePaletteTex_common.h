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


enum
{
	off,
	on,
};



macro Material int num
{
	Raw COMMAND_MATERIAL, num
}


macro Col int divNum, int r, int g, int b
{
	Raw COMMAND_COL, divNum, r, g, b
}


macro ColGrad int divNum, int r1, int g1, int b1, int r2, int g2, int b2
{
	Raw COMMAND_COLGRAD, divNum, r1, g1, b1, r2, g2, b2
}


macro ColMix int divNum
{
	Raw COMMAND_COLMIX, divNum
}


macro PerCol int per, int r, int g, int b
{
	Raw COMMAND_PERCOL, per, r, g, b
}

macro PerColEnd
{
	Raw COMMAND_PERCOLEND
}


macro MaxPer int per
{
	Raw COMMAND_MAXPER, per
}


macro DarkNormal
{
	Raw COMMAND_DARK, MODE_DARK_NORMAL, 0, 0, 0
}


macro DarkNone
{
	Raw COMMAND_DARK, MODE_DARK_NONE, 0, 0, 0
}


macro DarkCol int r, int g, int b
{
	Raw COMMAND_DARK, MODE_DARK_COL, r, g, b
}


macro Edge int flag
{
	Raw COMMAND_EDGE, flag
}


macro MateEdge int flag
{
	Raw COMMAND_MATEEDGE, flag
}


macro Gutter int flag
{
	Raw COMMAND_GUTTER, flag
}


// 面積比がこの値に満たない場合にAA処理をする
// n : 0.0 ～ 1.0
// n が 0 なら、AAなし
// n が 1 なら、かなりの部分（エッジに限る）にAAがかかる
// default : 0.5
macro AAThreshold float n
{
	Raw COMMAND_AATHRESHOLD, n
}


// AAの強さ
// n : 0.0 ～ 1.0
// default : 1.0
macro AASubtracter float n
{
	Raw COMMAND_AASUBTRACTER, n
}


// 指定のマテリアルに対してのエッジを抑制する
macro NoEdge int targetMaterialNo, int flag
{
	Raw COMMAND_NOEDGE, targetMaterialNo, flag
}


// マテリアル内の色の比率を調整する
// rate > 1 : 明るい部分を増やす
// rate < 1 : 暗い部分を増やす
// rate = 1 : default（調整しない）
// ※直後の PerCol にのみ影響する。PerColより先に書くこと。
macro AdjustRate float rate
{
	Raw COMMAND_ADJUST_RATE, rate
}



// フッタ（スクリプトの最後に自動的に呼ばれる）
macro footer
{
	Raw COMMAND_END
}
