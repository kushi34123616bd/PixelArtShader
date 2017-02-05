//
// Skinned Mesh Effect file
// Copyright (c) 2000-2002 Microsoft Corporation. All rights reserved.
//

//float4 lightDir = {0.0f, 0.0f, -1.0f, 1.0f};	  //light Direction
//float4 lightDiffuse = {0.6f, 0.6f, 0.6f, 1.0f}; // Light Diffuse
//float4 MaterialAmbient : MATERIALAMBIENT = {0.1f, 0.1f, 0.1f, 1.0f};
//float4 MaterialDiffuse : MATERIALDIFFUSE = {0.8f, 0.8f, 0.8f, 1.0f};

// 動的アンビエントライト
float DynamicAmbientLight = 0;

// ディレクショナルライト
float4 DirLight0_Dir = { 0.0f, 1.0f, 0.0f, 0.0f };
float DirLight0_Coeff = 0;
float4 DirLight1_Dir = { 0.0f, 1.0f, 0.0f, 0.0f };
float DirLight1_Coeff = 0;
float4 DirLight2_Dir = { 0.0f, 1.0f, 0.0f, 0.0f };
float DirLight2_Coeff = 0;


float SpecularCoefficient = 0;	// スペキュラ係数
float SpecularExponent = 1;		// スペキュラ指数

//float4 worldCameraPos;

float  PaletteU = 0.0f;


//float4 lht2Dir = {0.0f, 0.0f, -1.0f, 1.0f};

// Matrix Pallette
static const int MAX_MATRICES = 26;
float4x3	mWorldViewMatrixArray[MAX_MATRICES] : WORLDMATRIXARRAY;	// 実際は WorldView 行列
//float4x4	mViewProj : VIEWPROJECTION;
float4x4	mViewProj : PROJECTION;	// TODO: mViewProj -> mProj


//float4		invTexSize = { 0, 0, 0, 0 };	// x,y : inverse size of temp tex    z : inverse width of palette tex   w : unused
float4		invSrcTexSize = { 0, 0, 0, 0 };		// x,y : inverse size of temp tex    z : inverse width of palette tex   w : unused
float4		invSplitTexSize = { 0, 0, 0, 0 };	// x,y : inverse size of split tex   z,w : unused
float4		invFinalTexSize = { 0, 0, 0, 0 };	// x,y : inverse size of final tex   z,w : unused


texture g_Texture;				// Color texture for mesh
texture g_Texture2;
texture g_Texture3;
texture g_Texture4;	// palette
texture g_Texture5;
texture g_Texture6;
texture g_Texture7;


//#define BG_Z	9999999	// 背景のZ値
//#define BGCOLOR	float4( 0, 0.3f, 0, 1 )
//#define BGCOLOR	float4( 1, 1, 1, 1 )


#define PI		3.141592f


#define MAX_PALETTE	24	// シェーダーで扱えるマテリアル数の限界（もう1か2は増やせるかも）


#define LIGHT_RANGE	3.0f

#define PALETTE_TEX_WIDTH	256
#define PALETTE_TEX_HEIGHT	256

#define PALETTE_TEX_COLOR_START_Y	( 2.5f / PALETTE_TEX_HEIGHT )	// パレット画像内の、色情報の開始y座標（ここより上にはマテリアルの各種情報が入っている）
#define PALETTE_TEX_DARK_Y			( 0.5f / PALETTE_TEX_HEIGHT )	// パレット画像内の、暗部情報の開始y座標
#define PALETTE_TEX_AA_Y			( 1.5f / PALETTE_TEX_HEIGHT )	// パレット画像内の、AA情報の開始y座標

#define PALETTE_TEX_ADJACENT_X		( ( PALETTE_TEX_WIDTH - MAX_PALETTE + 0.5f ) / PALETTE_TEX_WIDTH )	// パレット画像内の、隣接マテリアル情報の開始座標
#define PALETTE_TEX_ADJACENT_Y		( ( PALETTE_TEX_HEIGHT - MAX_PALETTE + 0.5f ) / PALETTE_TEX_HEIGHT )



#define BLOCK_WIDTH		5
#define BLOCK_HEIGHT	5
#define BLOCK_NUM		( BLOCK_WIDTH * BLOCK_HEIGHT )
#define BLOCK_WHf		float2( BLOCK_WIDTH, BLOCK_HEIGHT )


float	g_ZThreshold = 0.05f;
float	g_AngleThreshold = 50.0f;
float	g_GutterThreshold = 0.2f;
float	g_IgnoreCountThreshold = 0.3f;	// 1.5f / 5.0f


///////////////////////////////////////////////////////

struct VS_INPUT
{
	float4	Pos 			: POSITION;
	float4	BlendWeights	: BLENDWEIGHT;
	float4	BlendIndices	: BLENDINDICES;
	float3	Normal			: NORMAL;
	float3	Tex0			: TEXCOORD0;
};

struct VS_OUTPUT
{
	float4	Pos 	: POSITION;
//	float4	Diffuse : COLOR;

	float4	ViewPos	: TEXCOORD3;

//	float3	Normal	: COLOR;
//	float3	Normal	: POSITION;
	float3	Normal	: TEXCOORD2;

	float2	Tex0	: TEXCOORD0;
	float2	PosZW	: TEXCOORD1;
};


#if 0
float BiasDiffuse( float3 Normal, float3 lightDir )
{
	const float bias = 0.2f;

	float CosTheta;

	// N.L Clamped
	CosTheta = max( 0.0f, ( dot( Normal, lightDir ) + bias ) / ( 1 + bias ) );

	return CosTheta;
}
#endif


float Diffuse( float3 Normal, float3 lightDir )
{
	float CosTheta;

	// N.L Clamped
	CosTheta = max( 0.0f, dot( Normal, lightDir ) );

	return CosTheta;
}


// リニアなディフューズ値の計算
//   法線が光源の方向を向いている     : 2.0
//   法線が光源と逆の方向を向いている : 0.0
float LinearDiffuse( float3 Normal, float3 lightDir )
{
	// 1-(arccos(cos(x))*2/π)

	float diffuse = 2 - ( acos( dot( Normal, lightDir ) ) * 2 / PI );

	return diffuse;
}


VS_OUTPUT VShade(VS_INPUT i, uniform int NumBones)
{
	VS_OUTPUT	o;
	float3		Pos = 0.0f;
	float3		Normal = 0.0f;
	float		LastWeight = 0.0f;

	// Compensate for lack of UBYTE4 on Geforce3
//	int4 IndexVector = D3DCOLORtoUBYTE4(i.BlendIndices);

	// cast the vectors to arrays for use in the for loop below
	float BlendWeightsArray[4] = (float[4])i.BlendWeights;
//	int   IndexArray[4] 	   = (int[4])IndexVector;
	int   IndexArray[4] 	   = (int[4])i.BlendIndices;

	// calculate the pos/normal using the "normal" weights
	//		  and accumulate the weights to calculate the last weight
	for (int iBone = 0; iBone < NumBones-1; iBone++)
	{
		LastWeight = LastWeight + BlendWeightsArray[iBone];

		Pos += mul(i.Pos, mWorldViewMatrixArray[IndexArray[iBone]]) * BlendWeightsArray[iBone];
		Normal += mul(float4(i.Normal,0), mWorldViewMatrixArray[IndexArray[iBone]]) * BlendWeightsArray[iBone];
	}
	LastWeight = 1.0f - LastWeight;

	// Now that we have the calculated weight, add in the final influence
	Pos += (mul(i.Pos, mWorldViewMatrixArray[IndexArray[NumBones-1]]) * LastWeight);
	Normal += (mul(float4(i.Normal,0), mWorldViewMatrixArray[IndexArray[NumBones-1]]) * LastWeight);

	// transform position from world space into view and then projection space
	o.Pos = mul(float4(Pos.xyz, 1.0f), mViewProj);

	o.ViewPos = float4( Pos.xyz, 1.0f );

	// normalize normals
	Normal = normalize(Normal);

	// Shade (Ambient + etc.)
//	o.Diffuse.xyz = MaterialAmbient.xyz + Diffuse(Normal) * MaterialDiffuse.xyz;
//	o.Diffuse.w = 1.0f;
	o.Normal = Normal;

	// copy the input texture coordinate through
	o.Tex0	= i.Tex0.xy;

///	o.PosZW.xy = o.Pos.zw;
//	o.PosZW.xy = float2( 0, o.Pos.z / o.Pos.w );
//	o.PosZW.xy = float2( 0, 1 / o.Pos.w );
	o.PosZW.xy = float2( 0, o.ViewPos.z );
//	o.PosZW.xy = float2( 0, length( Pos.xyz ) );

	return o;
}

int CurNumBones = 2;
VertexShader vsArray[4] = { compile vs_2_0 VShade(1),
							compile vs_2_0 VShade(2),
							compile vs_2_0 VShade(3),
							compile vs_2_0 VShade(4)
						  };


//--------------------------------------------------------------------------------------
// Texture samplers
//--------------------------------------------------------------------------------------
sampler PS2TextureSampler1 =
sampler_state
{
	Texture = <g_Texture>;

	AddressU = CLAMP;
	AddressV = CLAMP;

	MipFilter = NONE;
	MinFilter = POINT;
	MagFilter = POINT;
//	MinFilter = LINEAR;
//	MagFilter = LINEAR;
};

sampler PS2TextureSampler2 =
sampler_state
{
	Texture = <g_Texture2>;

	AddressU = CLAMP;
	AddressV = CLAMP;

	MipFilter = NONE;
	MinFilter = POINT;
	MagFilter = POINT;
//	MinFilter = LINEAR;
//	MagFilter = LINEAR;
};

sampler PS2TextureSampler3 =
sampler_state
{
	Texture = <g_Texture3>;

	AddressU = CLAMP;
	AddressV = CLAMP;

	MipFilter = NONE;
	MinFilter = POINT;
	MagFilter = POINT;
};

sampler PS2TextureSampler4 =
sampler_state
{
	Texture = <g_Texture4>;

	AddressU = CLAMP;
	AddressV = CLAMP;

	MipFilter = NONE;
	MinFilter = POINT;
	MagFilter = POINT;
};

sampler PS2TextureSampler5 =
sampler_state
{
	Texture = <g_Texture5>;

	AddressU = CLAMP;
	AddressV = CLAMP;

	MipFilter = NONE;
	MinFilter = POINT;
	MagFilter = POINT;
};

sampler PS2TextureSampler6 =
sampler_state
{
	Texture = <g_Texture6>;

	AddressU = CLAMP;
	AddressV = CLAMP;

	MipFilter = NONE;
	MinFilter = POINT;
	MagFilter = POINT;
};

sampler PS2TextureSampler7 =
sampler_state
{
	Texture = <g_Texture7>;

	AddressU = CLAMP;
	AddressV = CLAMP;

	MipFilter = NONE;
	MinFilter = POINT;
	MagFilter = POINT;
};


//--------------------------------------------------------------------------------------
// Pixel shader output structure
//--------------------------------------------------------------------------------------
struct PS_OUTPUT
{
	float4 RGBColor		: COLOR0;  // Pixel color
	float4 RGBColor2	: COLOR1;  // Pixel color
	float4 Depth		: COLOR2;
};


//--------------------------------------------------------------------------------------
// This shader outputs the pixel's color by modulating the texture's
//		 color with diffuse material color
//--------------------------------------------------------------------------------------
PS_OUTPUT RenderScenePS(	VS_OUTPUT In,
							uniform bool bMaterialMap,
							uniform bool bAOMap,
							uniform bool bLightMap
						)
{
	PS_OUTPUT Output;

	float3		Normal = normalize( In.Normal );


	// ディレクショナルライト
//	float diffuse = Diffuse( Normal, DirLight0_Dir );
	float diffuse = LinearDiffuse( Normal, DirLight0_Dir.xyz ) * DirLight0_Coeff;	// [0,2 * DirLight0_Coeff]
	diffuse += Diffuse( Normal, DirLight1_Dir.xyz ) * DirLight1_Coeff;
	diffuse += Diffuse( Normal, DirLight2_Dir.xyz ) * DirLight2_Coeff;


	// スペキュラ
	float specular = 0;	// [0,SpecularCoefficient]
#if 1
	{
//		float3 vHalf = normalize( lightDir.xyz + normalize( worldCameraPos.xyz - In.ViewPos.xyz ) );
		float3 vHalf = normalize( DirLight0_Dir.xyz + normalize( - In.ViewPos.xyz ) );

//		float powNum = 20;

//		specular = pow( max( dot( vHalf, Normal ), 0 ), powNum );

		specular = pow( max( dot( vHalf, Normal ), 0 ), SpecularExponent );	// [0,1]
		specular *= SpecularCoefficient;									// [0,SpecularCoefficient]
	}
#endif



	float lightCoefficient;	// 最終的な照明係数

	lightCoefficient = diffuse + specular;	// [0, 2 * DirLight0_Coeff + DirLight1_Coeff + DirLight2_Coeff + SpecularCoefficient ]

	// 動的アンビエントライト
	lightCoefficient += DynamicAmbientLight;

	// ライトマップ（アンビエントライトの効果）
	if( bLightMap )
	{
		lightCoefficient += tex2D( PS2TextureSampler1, In.Tex0 ).b * LIGHT_RANGE;
	}

	// AOマップ
	if( bAOMap )
	{
		lightCoefficient *= tex2D( PS2TextureSampler1, In.Tex0 ).g;
	}

	lightCoefficient /= LIGHT_RANGE;	// レンジ圧縮  ※LIGHT_RANGEより大きい値はフレームバッファへの書き込み時にクランプされる



#if 0
	// 確認用
	{
		float3 col = tex2D( PS2TextureSampler4, float2( PaletteU, 0.5f ) ).xyz;

		col *= lightCoefficient * LIGHT_RANGE / 2;

		Output.RGBColor = float4( col, 1 );
	}
#else
//	Output.RGBColor = float4( lightCoefficient, PaletteU, 0, 0 );



	float palette = PaletteU;

	// マテリアルマップ
	if( bMaterialMap )
	{
		palette = tex2D( PS2TextureSampler1, In.Tex0 ).r;
	}

	Output.RGBColor = float4( lightCoefficient, palette, 0, 0 );






#endif








	float3 temp = ( Normal + 1.0f ) / 2.0f;
	Output.RGBColor2 = float4( temp, 1 );

///	Output.Depth = In.PosZW.x / In.PosZW.y;
//	Output.Depth = 1 / In.PosZW.y;
	Output.Depth = float4( In.ViewPos.xyz, In.PosZW.y );

	return Output;
}

int PSMode = 0;
PixelShader psArray[ 4 ] = {	compile ps_3_0 RenderScenePS( false, false, false ),	// 0:  マテリアルマップなし  AO・ライトマップなし
								compile ps_3_0 RenderScenePS( true,  false, false ),	// 1:  マテリアルマップあり  AO・ライトマップなし
								compile ps_3_0 RenderScenePS( false, true,  true ),		// 2:  マテリアルマップなし  AO・ライトマップあり
								compile ps_3_0 RenderScenePS( true,  true,  true )		// 3:  マテリアルマップあり  AO・ライトマップあり
							};


///////////////////////////////////////////////////////////////////////////////

struct PS_OUTPUT_ANALYZE_COUNTPALETTE_MEANPOS
{
	float4 dst		: COLOR0;  // パレット番号, 面積（選ばれたパレットのみ）, 面積（z値で無視された領域も含む）, 0
	float4 dst2		: COLOR1;  // 平均座標xyz, 0
};

PS_OUTPUT_ANALYZE_COUNTPALETTE_MEANPOS RenderScenePSAnalyze_CountPalette_MeanPos( in float2 texcoord : TEXCOORD0 )
{
	PS_OUTPUT_ANALYZE_COUNTPALETTE_MEANPOS output;


#if 1

	float aPaletteCount[ MAX_PALETTE ];
	float aMeanPos_Z[ MAX_PALETTE ];


	{
		for( int i = 0;  i < MAX_PALETTE;  i ++ )
		{
			aPaletteCount[ i ] = 0;
			aMeanPos_Z[ i ] = 0;
		}
	}


	{
		float2 origin = texcoord - invSrcTexSize.xy * ( BLOCK_WHf / 2.0f - 0.5f );

//		[unroll(BLOCK_HEIGHT)] // attribute
		for( int y = 0;  y < BLOCK_HEIGHT;  y ++ )
		{
//			[unroll(BLOCK_WIDTH)] // attribute
			for( int x = 0;  x < BLOCK_WIDTH;  x ++ )
			{
				float2 srcTexCoord = origin + invSrcTexSize.xy * float2( x, y );

				float paletteU = tex2D( PS2TextureSampler1, srcTexCoord ).g;

				int paletteIndex = round( paletteU * 255 );

				float posZ = tex2D( PS2TextureSampler3, srcTexCoord ).z;


//				[unroll(MAX_PALETTE)]
				for( int i = 0;  i < MAX_PALETTE;  i ++ )
				{
					if( i == paletteIndex )
					{
						aPaletteCount[ i ] ++;
						aMeanPos_Z[ i ] += posZ;
					}
				}
			}
		}
	}


	{
		for( int i = 1;  i < MAX_PALETTE;  i ++ )
		{
			if( 0 < aPaletteCount[ i ] )
			{
				aMeanPos_Z[ i ] /= aPaletteCount[ i ];
			}
		}
	}

	//////////////////////////////////////////////////////////////////////

	#define IGNORE_COUNT_THRESHOULD	( BLOCK_NUM * g_IgnoreCountThreshold )

	int n = -1;
	int ignoreCount = 0;

	float meanPos_minZ = 999999;
	float paletteCount_minZ = aPaletteCount[ 0 ];

	{
		if( 0 < aPaletteCount[ 0 ] )
		{
			n = 0;
		}

		// 閾値を越える面積を持つマテリアルの中で、最も手前のものを選ぶ
		for( int i = 1;  i < MAX_PALETTE;  i ++ )
		{
			if( IGNORE_COUNT_THRESHOULD < aPaletteCount[ i ] )
			{
				if( aMeanPos_Z[ i ] < meanPos_minZ )
				{
					n = i;

					meanPos_minZ = aMeanPos_Z[ i ];
					paletteCount_minZ = aPaletteCount[ i ];
				}
			}
		}

		// 背景が存在せず、全てのマテリアルが閾値以下だったので、閾値を抜きにしてもう一度調べる
		if( n == -1 )
		{
			for( int i = 1;  i < MAX_PALETTE;  i ++ )
			{
				if( 0 < aPaletteCount[ i ] )
				{
					if( aMeanPos_Z[ i ] < meanPos_minZ )
					{
						n = i;

						meanPos_minZ = aMeanPos_Z[ i ];
						paletteCount_minZ = aPaletteCount[ i ];
					}
				}
			}
		}

		// 無視面積の計算
		if( n != 0 )
		{
			for( int i = 1;  i < MAX_PALETTE;  i ++ )
			{
				if( aMeanPos_Z[ i ] < meanPos_minZ )
				{
					// 選択されたマテリアルより手前にあるマテリアルの面積をカウント

					ignoreCount += aPaletteCount[ i ];
				}
				else if( i != n )
				{
					// 「隣接マテリアルにエッジをかけないフラグ」が立っているものをカウント

					// 隣接マテリアルにエッジをかけないフラグ  0 or 1
					float adjacent_noEdge = tex2D( PS2TextureSampler4, float2( PALETTE_TEX_ADJACENT_X + (float)n / PALETTE_TEX_WIDTH, PALETTE_TEX_ADJACENT_Y + (float)i / PALETTE_TEX_HEIGHT ) ).r;

					if( 0 < adjacent_noEdge )
						ignoreCount += aPaletteCount[ i ];
				}
			}
		}
	}


	//////////////////////////////////////////////////////////////////////

	float2 meanPosXY = float2( 0, 0 );

	if( n != 0 )
	{
		float2 origin = texcoord - invSrcTexSize.xy * ( BLOCK_WHf / 2.0f - 0.5f );

//		[unroll(BLOCK_HEIGHT)] // attribute
		for( int y = 0;  y < BLOCK_HEIGHT;  y ++ )
		{
//			[unroll(BLOCK_WIDTH)] // attribute
			for( int x = 0;  x < BLOCK_WIDTH;  x ++ )
			{
				float2 srcTexCoord = origin + invSrcTexSize.xy * float2( x, y );

				float paletteU = tex2D( PS2TextureSampler1, srcTexCoord ).g;

				int paletteIndex = round( paletteU * 255 );

				if( n == paletteIndex )
				{
					meanPosXY += tex2D( PS2TextureSampler3, srcTexCoord ).xy;
				}
			}
		}

		for( int i = 1;  i < MAX_PALETTE;  i ++ )
		{
			if( i == n )
			{
				meanPosXY /= aPaletteCount[ i ];
			}
		}
	}






#else

	float aPaletteCount[ MAX_PALETTE ];
	float3 aMeanPos[ MAX_PALETTE ];


	for( int i = 0;  i < MAX_PALETTE;  i ++ )
	{
		aPaletteCount[ i ] = 0;
		aMeanPos[ i ] = float3( 0, 0, 0 );
	}


	{
		float2 origin = texcoord - invSrcTexSize.xy * ( BLOCK_WHf / 2.0f - 0.5f );

//		[unroll(BLOCK_HEIGHT)] // attribute
		for( int y = 0;  y < BLOCK_HEIGHT;  y ++ )
		{
//			[unroll(BLOCK_WIDTH)] // attribute
			for( int x = 0;  x < BLOCK_WIDTH;  x ++ )
			{
				float2 srcTexCoord = origin + invSrcTexSize.xy * float2( x, y );

				float paletteU = tex2D( PS2TextureSampler1, srcTexCoord ).g;

				int paletteIndex = round( paletteU * 255 );

				float3 pos = tex2D( PS2TextureSampler3, srcTexCoord ).xyz;


//				[unroll(MAX_PALETTE)]
				for( int i = 0;  i < MAX_PALETTE;  i ++ )
				{
					if( i == paletteIndex )
					{
						aPaletteCount[ i ] ++;
						aMeanPos[ i ] += pos;
					}
				}
			}
		}
	}


	for( int i = 1;  i < MAX_PALETTE;  i ++ )
	{
		if( 0 < aPaletteCount[ i ] )
		{
			aMeanPos[ i ] /= aPaletteCount[ i ];
		}
	}

	aMeanPos[ 0 ].z = 999999;

	//////////////////////////////////////////////////////////////////////

	#define IGNORE_COUNT_THRESHOULD	( BLOCK_WIDTH * BLOCK_HEIGHT / 5.0f * 1.5f )

	int n = 0;
	int ignoreCount = 0;

	float3 meanPos_minZ = aMeanPos[ 0 ];
	float paletteCount_minZ = aPaletteCount[ 0 ];
	{
#if 1
		for( int i = 1;  i < MAX_PALETTE;  i ++ )
		{
#if 1
			if( IGNORE_COUNT_THRESHOULD < aPaletteCount[ i ] )
			{
				if( aMeanPos[ i ].z < meanPos_minZ.z )
				{
					n = i;

					meanPos_minZ = aMeanPos[ i ];
					paletteCount_minZ = aPaletteCount[ i ];
				}
			}
#else
			float b = step( IGNORE_COUNT_THRESHOULD, aPaletteCount[ i ] ) * step( aMeanPos[ i ].z, meanPos_minZ.z );

			n = lerp( n, i, b );


			meanPos_minZ = lerp( meanPos_minZ, aMeanPos[ i ], b );
			paletteCount_minZ = lerp( paletteCount_minZ, aPaletteCount[ i ], b );
#endif
		}
#endif

#if 1
		if( n != 0 )
		{
			for( int i = 1;  i < MAX_PALETTE;  i ++ )
			{
				if( aMeanPos[ i ].z < meanPos_minZ.z )
				{
					ignoreCount += aPaletteCount[ i ];
				}
			}
		}
#endif
	}
#endif









//////////////////////////////////////////////////////////////////////

#if 1
	output.dst = float4(
							n,
							paletteCount_minZ,
							paletteCount_minZ + ignoreCount,
							0
						) / 255.0f;
//	output.dst2 = float4( meanPos_minZ, 0 );
	output.dst2 = float4( meanPosXY, meanPos_minZ, 0 );
#else
	output.dst = float4( aPaletteCount[ 0 ], 0, 0, 0 );
	output.dst2 = float4( aMeanPos[ 0 ].xyz, 0 );
#endif
	return output;
}


///////////////////////////////////////////////////////////////////////////////

struct PS_OUTPUT_ANALYZE
{
	float4 dst		: COLOR0;  // 平均法線xyz, 平均照度
	float4 dst2		: COLOR1;  // bEdge( 0 or 1 ), bGutter( 0 or 1 ), 0,0
};

PS_OUTPUT_ANALYZE RenderScenePSAnalyze( in float2 texcoord : TEXCOORD0 )
{
	PS_OUTPUT_ANALYZE output;

/*
	output.dst = float4( 0, 0, 0, 0 );

	return output;
*/


	float paletteU = tex2D( PS2TextureSampler5, texcoord ).r;						// パレット番号
//	float areaRate = tex2D( PS2TextureSampler5, texcoord ).g * 255.0f / BLOCK_NUM;	// 面積比
	float area = round( tex2D( PS2TextureSampler5, texcoord ).g * 255.0f );			// 面積

	float avgLight = 0;
	float3 avgNormal = float3( 0, 0, 0 );



	// エッジチェック
	float bEdge = 0;
	{
		float maxSubZ = 0;

		float2 origin = texcoord - invSrcTexSize.xy * ( BLOCK_WHf / 2.0f - 0.5f );

//		[loop] // attribute
		for( int y = 0;  y < BLOCK_HEIGHT;  y ++ )
		{
//			[loop] // attribute
			for( int x = 0;  x < BLOCK_WIDTH;  x ++ )
			{
				float2 srcTexCoord = origin + invSrcTexSize.xy * float2( x, y );

				float _paletteU = tex2D( PS2TextureSampler1, srcTexCoord ).g;

				if( _paletteU == paletteU )
				{
					avgLight += tex2D( PS2TextureSampler1, srcTexCoord ).r;
//					avgLight = max( avgLight, tex2D( PS2TextureSampler1, srcTexCoord ).r );

					float3 normal = tex2D( PS2TextureSampler2, srcTexCoord ).xyz * 2.0f - 1.0f;

					avgNormal += normal;

					float3 pos = tex2D( PS2TextureSampler3, srcTexCoord ).xyz;

					// 隣接4ピクセルとのZ差を測る
					{
						float D = dot( normal, pos );	// 平面パラメータ -D = N・P0

						//   0
						// 2 C 3
						//   1
						const int2 aNeighborhoodUVTable[] =
						{
							{  0, -1 },
							{  0,  1 },
							{ -1,  0 },
							{  1,  0 },
						};

//						[loop] // attribute
						for( int i = 0;  i < 4;  i ++ )
						{
							float2 NbTexCoord = srcTexCoord + invSrcTexSize.xy * aNeighborhoodUVTable[ i ];

							// 隣接マテリアルが中央と異なり、かつ「隣接マテリアルにエッジをかけないフラグ」が立っているなら、判定をしない
							{
								float NbPaletteU = tex2D( PS2TextureSampler1, NbTexCoord ).g;

								// 隣接マテリアルにエッジをかけないフラグ  0 or 1
//								float adjacent_noEdge = tex2D( PS2TextureSampler4, float2( NbPaletteU + 0.5f, PALETTE_TEX_ADJACENT_Y ) ).r;
								float adjacent_noEdge = tex2D( PS2TextureSampler4, float2( PALETTE_TEX_ADJACENT_X + _paletteU, PALETTE_TEX_ADJACENT_Y + NbPaletteU ) ).r;

								if( _paletteU != NbPaletteU  &&  0 < adjacent_noEdge )
									continue;
							}


							float3 NbPos = tex2D( PS2TextureSampler3, NbTexCoord ).xyz;

//							float t = D / dot( normal, NbPos );	// 0除算の可能性あり
							float NbD = dot( normal, NbPos );

							// t が 1 未満なら、隣接ピクセルの座標は理想より奥にある
//							if( t < 1.0f )

							// 隣接ピクセルの座標は理想より奥にある
							if( NbD < D )
							{
//								float3 IdealPos = NbPos * t;	// (x,y) から求めた理想の座標

								// 隣接ピクセルが理想座標より奥にあるなら、NbD は必ず負の値（0除算は発生しない）
								float3 IdealPos = NbPos * ( D / NbD );	// 理想の座標（視点から隣接座標までの線分と平面の交点）

								float Sub = length( NbPos - IdealPos );	// 理想座標との距離

								maxSubZ = max( maxSubZ, Sub );
							}
						}
					}
				}


//				int paletteU = floor( paletteUf * 255 + 0.5f );	// 0.5f : 念のため
			}
		}

		if( g_ZThreshold <= maxSubZ )
		{
			bEdge = 1;
		}
	}

	if( paletteU == 0 )
	{
		avgNormal = float3( 0, 0, -1 );
		bEdge = 0;
	}





	// 溝チェック
	float bGutter = 0;
	{
		float gutter = 0;


		float2 origin = texcoord - invSrcTexSize.xy * ( BLOCK_WHf / 2.0f - 0.5f );


#if 0

		// 本当はこれでコンパイルが通るはずなのだが、エラーが出るので↓ループ展開をした

		const float2 aToAdjacent[] =	{
											{  1,  0 },	// 左   → 右
											{  0,  1 },	// 上   → 下
											{  1,  1 },	// 左上 → 右下
											{  1, -1 },	// 左下 → 右上
										};

		const float2 aAxis[] =	{
									{  0,  1 },	// 左   → 右
									{  1,  0 },	// 上   → 下
									{  1,  1 },	// 左上 → 右下
									{ -1,  1 },	// 左下 → 右上
								};

		[unroll] // attribute
		for( int i = 0;  i < 4;  i ++ )
		{
//			[unroll] // attribute
			for( int y = 0;  y < BLOCK_HEIGHT;  y ++ )
			{
//				[unroll] // attribute
				for( int x = 0;  x < BLOCK_WIDTH;  x ++ )
				{
					float2 srcTexCoord1 = origin + invSrcTexSize.xy * float2( x, y );
					float2 srcTexCoord2 = origin + invSrcTexSize.xy * ( float2( x, y ) + aToAdjacent[ i ] );

					float3 normal1 = normalize( tex2D( PS2TextureSampler2, srcTexCoord1 ).xyz * 2.0f - 1.0f );
					float3 normal2 = normalize( tex2D( PS2TextureSampler2, srcTexCoord2 ).xyz * 2.0f - 1.0f );

					float3 cp = cross( normal1, normal2 );

					// 法線が向き合っているか
					if( 0 < dot( cp.xy, aAxis[ i ] ) )
					{
						float dp = dot( normal1, normal2 );

						float sinTheta = length( cp );

						// ある程度以上の角度で向き合っているか
						if( dp <= 0  ||  g_AngleThreshold < sinTheta )
						{
							gutter ++;
						}
					}
				}
			}
		}

#else

		// ループ展開版

		{
			for( int y = 0;  y < BLOCK_HEIGHT;  y ++ )
			{
				for( int x = 0;  x < BLOCK_WIDTH;  x ++ )
				{
					float2 srcTexCoord1 = origin + invSrcTexSize.xy * float2( x, y );
//					float2 srcTexCoord2 = origin + invSrcTexSize.xy * ( float2( x, y ) + aToAdjacent[ i ] );
					float2 srcTexCoord2 = origin + invSrcTexSize.xy * float2( x + 1, y );

					float3 normal1 = normalize( tex2D( PS2TextureSampler2, srcTexCoord1 ).xyz * 2.0f - 1.0f );
					float3 normal2 = normalize( tex2D( PS2TextureSampler2, srcTexCoord2 ).xyz * 2.0f - 1.0f );

					float3 cp = cross( normal1, normal2 );

					// 法線が向き合っているか
//					if( 0 < dot( cp.xy, aAxis[ i ] ) )
					if( 0 < cp.y )
					{
						float dp = dot( normal1, normal2 );
						float sinTheta = length( cp );
						float theta = asin( sinTheta );

						if( dp <= 0 )
						{
							theta = PI - theta;
						}

						// ある程度以上の角度で向き合っているか
//						if( dp <= 0  ||  angleThreshould < sinTheta )
						if( radians( g_AngleThreshold ) < theta )
						{
							gutter ++;
						}
					}
				}
			}
		}

		{
			for( int y = 0;  y < BLOCK_HEIGHT;  y ++ )
			{
				for( int x = 0;  x < BLOCK_WIDTH;  x ++ )
				{
					float2 srcTexCoord1 = origin + invSrcTexSize.xy * float2( x, y );
//					float2 srcTexCoord2 = origin + invSrcTexSize.xy * ( float2( x, y ) + aToAdjacent[ i ] );
					float2 srcTexCoord2 = origin + invSrcTexSize.xy * float2( x, y + 1 );

					float3 normal1 = normalize( tex2D( PS2TextureSampler2, srcTexCoord1 ).xyz * 2.0f - 1.0f );
					float3 normal2 = normalize( tex2D( PS2TextureSampler2, srcTexCoord2 ).xyz * 2.0f - 1.0f );

					float3 cp = cross( normal1, normal2 );

					// 法線が向き合っているか
//					if( 0 < dot( cp.xy, aAxis[ i ] ) )
					if( 0 < cp.x )
					{
						float dp = dot( normal1, normal2 );
						float sinTheta = length( cp );
						float theta = asin( sinTheta );

						if( dp <= 0 )
						{
							theta = PI - theta;
						}

						// ある程度以上の角度で向き合っているか
						if( radians( g_AngleThreshold ) < theta )
						{
							gutter ++;
						}
					}
				}
			}
		}

		{
			for( int y = 0;  y < BLOCK_HEIGHT;  y ++ )
			{
				for( int x = 0;  x < BLOCK_WIDTH;  x ++ )
				{
					float2 srcTexCoord1 = origin + invSrcTexSize.xy * float2( x, y );
//					float2 srcTexCoord2 = origin + invSrcTexSize.xy * ( float2( x, y ) + aToAdjacent[ i ] );
					float2 srcTexCoord2 = origin + invSrcTexSize.xy * float2( x + 1, y + 1 );

					float3 normal1 = normalize( tex2D( PS2TextureSampler2, srcTexCoord1 ).xyz * 2.0f - 1.0f );
					float3 normal2 = normalize( tex2D( PS2TextureSampler2, srcTexCoord2 ).xyz * 2.0f - 1.0f );

					float3 cp = cross( normal1, normal2 );

					// 法線が向き合っているか
//					if( 0 < dot( cp.xy, aAxis[ i ] ) )
					if( 0 < cp.x + cp.y )
					{
						float dp = dot( normal1, normal2 );
						float sinTheta = length( cp );
						float theta = asin( sinTheta );

						if( dp <= 0 )
						{
							theta = PI - theta;
						}

						// ある程度以上の角度で向き合っているか
						if( radians( g_AngleThreshold ) < theta )
						{
							gutter ++;
						}
					}
				}
			}
		}

		{
			for( int y = 0;  y < BLOCK_HEIGHT;  y ++ )
			{
				for( int x = 0;  x < BLOCK_WIDTH;  x ++ )
				{
					float2 srcTexCoord1 = origin + invSrcTexSize.xy * float2( x, y );
//					float2 srcTexCoord2 = origin + invSrcTexSize.xy * ( float2( x, y ) + aToAdjacent[ i ] );
					float2 srcTexCoord2 = origin + invSrcTexSize.xy * float2( x + 1, y - 1 );

					float3 normal1 = normalize( tex2D( PS2TextureSampler2, srcTexCoord1 ).xyz * 2.0f - 1.0f );
					float3 normal2 = normalize( tex2D( PS2TextureSampler2, srcTexCoord2 ).xyz * 2.0f - 1.0f );

					float3 cp = cross( normal1, normal2 );

					// 法線が向き合っているか
//					if( 0 < dot( cp.xy, aAxis[ i ] ) )
					if( 0 < cp.y - cp.x )
					{
						float dp = dot( normal1, normal2 );
						float sinTheta = length( cp );
						float theta = asin( sinTheta );

						if( dp <= 0 )
						{
							theta = PI - theta;
						}

						// ある程度以上の角度で向き合っているか
						if( radians( g_AngleThreshold ) < theta )
						{
							gutter ++;
						}
					}
				}
			}
		}
#endif

		if(	    BLOCK_NUM * g_GutterThreshold <= gutter
			&&  0 < paletteU
		)
		{
			bGutter = 1;
		}
	}



	output.dst = float4( ( normalize( avgNormal ) + 1.0f ) / 2, avgLight / area );
//	output.dst = float4( ( normalize( avgNormal ) + 1.0f ) / 2, avgLight );

	output.dst2 = float4( bEdge, bGutter, 0, 0 );

	return output;
}


///////////////////////////////////////////////////////////////////////////////

// パラメータ合成パス

struct PS_OUTPUT_MIX
{
//	float4 RGBColor		: COLOR0;  // Pixel color

	float4 Color		: COLOR0;  // paletteU, colorNum, 0, 0
};

PS_OUTPUT_MIX RenderScenePSMix( in float2 texcoord : TEXCOORD0, uniform int mode_DispContour )
{
	PS_OUTPUT_MIX output;


	//   0
	// 2 C 3
	//   1
	const int2 aNeighborhoodUVTable[] =
	{
		{  0, -1 },
		{  0,  1 },
		{ -1,  0 },
		{  1,  0 },
	};


	// 周辺4ブロックとのZ差を調べる
	float bBlockEdge = 0;
#if 1
	{
		float3 pos = tex2D( PS2TextureSampler6, texcoord ).xyz;				// 平均座標
		float3 normal = tex2D( PS2TextureSampler1, texcoord ).xyz * 2 - 1;	// 法線

		float D = dot( normal, pos );	// 平面パラメータ -D = N・P0

		float paletteU = tex2D( PS2TextureSampler5, texcoord ).r;	// パレットU

		for( int i = 0;  i < 4;  i ++ )
		{
			float2 NbTexCoord = texcoord + invSplitTexSize.xy * aNeighborhoodUVTable[ i ];

			// 隣接マテリアルが中央と異なり、かつ「隣接マテリアルにエッジをかけないフラグ」が立っているなら、判定をしない
			{
				float NbPaletteU = tex2D( PS2TextureSampler5, NbTexCoord ).r;

				// 隣接マテリアルにエッジをかけないフラグ  0 or 1
//				float adjacent_noEdge = tex2D( PS2TextureSampler4, float2( NbPaletteU + 0.5f, PALETTE_TEX_ADJACENT_Y ) ).r;
				float adjacent_noEdge = tex2D( PS2TextureSampler4, float2( PALETTE_TEX_ADJACENT_X + paletteU, PALETTE_TEX_ADJACENT_Y + NbPaletteU ) ).r;

				if( paletteU != NbPaletteU  &&  0 < adjacent_noEdge )
					continue;
			}


			float3 NbPos = tex2D( PS2TextureSampler6, NbTexCoord ).xyz;

//			float t = D / dot( normal, NbPos );	// 0除算の可能性あり
			float NbD = dot( normal, NbPos );

			// t が 1 未満なら、隣接ピクセルの座標は理想より奥にある
//			if( t < 1.0f )

			// 隣接ピクセルの座標は理想より奥にある
			if( NbD < D )
			{
//				float3 IdealPos = NbPos * t;	// (x,y) から求めた理想の座標

				// 隣接ピクセルが理想座標より奥にあるなら、NbD は必ず負の値（0除算は発生しない）
				float3 IdealPos = NbPos * ( D / NbD );	// 理想の座標（視点から隣接座標までの線分と平面の交点）

				float Sub = length( NbPos - IdealPos );	// 理想座標との距離

				if( g_ZThreshold <= Sub )
				{
					bBlockEdge = 1;
				}
			}
		}
	}
#endif


	// マテリアルエッジ
	float bMaterialEdge = 0;
	{
		float3 pos = tex2D( PS2TextureSampler6, texcoord ).xyz;		// 平均座標
		float paletteU = tex2D( PS2TextureSampler5, texcoord ).r;	// パレットU

		for( int i = 0;  i < 4;  i ++ )
		{
			float2 NbTexCoord = texcoord + invSplitTexSize.xy * aNeighborhoodUVTable[ i ];
			float NbPaletteU = tex2D( PS2TextureSampler5, NbTexCoord ).r;

			if( paletteU == NbPaletteU )
				continue;


			// 隣接マテリアルが中央と異なり、かつ「隣接マテリアルにエッジをかけないフラグ」が立っているなら、判定をしない
			{
				// 隣接マテリアルにエッジをかけないフラグ  0 or 1
//				float adjacent_noEdge = tex2D( PS2TextureSampler4, float2( NbPaletteU + 0.5f, PALETTE_TEX_ADJACENT_Y ) ).r;
				float adjacent_noEdge = tex2D( PS2TextureSampler4, float2( PALETTE_TEX_ADJACENT_X + paletteU, PALETTE_TEX_ADJACENT_Y + NbPaletteU ) ).r;

				if( 0 < adjacent_noEdge )
					continue;
			}


			float3 NbPos = tex2D( PS2TextureSampler6, NbTexCoord ).xyz;

			if( pos.z < NbPos.z )
			{
				bMaterialEdge = 1;
			}
		}
	}







	float paletteU = tex2D( PS2TextureSampler5, texcoord ).r + 0.5f * invSrcTexSize.z;	// パレットU

#if 0
//	float areaRate = tex2D( PS2TextureSampler5, texcoord ).g;		// 面積比
	float areaRate = tex2D( PS2TextureSampler5, texcoord ).b;		// 面積比（無視面積含む）

	areaRate = ( areaRate * 255.0f ) / BLOCK_NUM;
#else

//	float areaNum = tex2D( PS2TextureSampler5, texcoord ).g;		// 面積 [ g_IgnoreCountThreshold * BLOCK_NUM / 255, BLOCK_NUM / 255 ]
	float areaNum = tex2D( PS2TextureSampler5, texcoord ).b;		// 面積（無視面積含む）


	float areaRate = smoothstep( g_IgnoreCountThreshold, 1.0f, areaNum * 255.0f / BLOCK_NUM );	// 面積比 [ 0, 1 ]
	// areaRate が 0 だとしても、そのマテリアルの面積が 0 という訳ではなく、閾値ギリギリという意味になる
#endif




	float light = tex2D( PS2TextureSampler1, texcoord ).a * LIGHT_RANGE;	// 明るさ

	float edge = tex2D( PS2TextureSampler2, texcoord ).r;	// エッジ  0 or 1
	float gutter = tex2D( PS2TextureSampler2, texcoord ).g;	// 溝  0 or 1

	float darkMode = tex2D( PS2TextureSampler4, float2( paletteU + 0.5f, PALETTE_TEX_DARK_Y ) ).r;	// 0, 1/255, 1 のいずれか


	float emgInfo = tex2D( PS2TextureSampler4, float2( paletteU + 0.5f, PALETTE_TEX_DARK_Y ) ).g;	// 0～2 ビット目がそれぞれ edge, material_edge, gutter のON/OFFフラグ
	int emgInfo_int = (int)round( emgInfo * 255 );

	float emg_b_edge			= emgInfo_int % 2;		// 0ビット目
	float emg_b_materialEdge	= emgInfo_int / 2 % 2;	// 1ビット目
	float emg_b_gutter			= emgInfo_int / 4 % 2;	// 2ビット目


	float AAThreshold	= tex2D( PS2TextureSampler4, float2( paletteU + 0.5f, PALETTE_TEX_AA_Y ) ).r;	// 面積比がこの値に満たない場合にAA処理をする [ 0.0, 1.0 ]
	float AASubtracter	= tex2D( PS2TextureSampler4, float2( paletteU + 0.5f, PALETTE_TEX_AA_Y ) ).g;	// AAの強さ [ 0.0, 1.0 ]


#define SELECT	2

	float colorNum = 0;

#if SELECT == 0		// 改良前

	float dark = min( min( edge + bBlockEdge, 1 ) + gutter + bMaterialEdge, 1.5f );

	float coeff = light - dark * 0.5f - ( 1 - areaRate ) * 0.3f;

#elif SELECT == 1	// 適当に改良

//	float _bEdge = min( edge + bBlockEdge + bMaterialEdge, 1 );
	float _bEdge = min( edge + bBlockEdge, 1 );

	float _coeff = 2.0f;

	areaRate = max( areaRate, 0.5f );	// [ 0.5, 1.0 ]

	if( _bEdge != 0.0f )
	{
		areaRate = 0.5f;
	}

//	float _dark = ( 1 - areaRate ) * _bEdge * _coeff;
	float _dark = ( 1 - areaRate ) * _coeff;

//	float _gutter = gutter * _coeff;
	float _gutter = 0;

	float coeff = light - max( _dark, _gutter );

#elif SELECT == 2	// エッジパレット作成

//	float dark = min( edge + bBlockEdge + gutter + bMaterialEdge, 1 );

	float dark = min(
						  ( edge + bBlockEdge ) * emg_b_edge
						+ bMaterialEdge * emg_b_materialEdge
						+ gutter * emg_b_gutter
						, 1
					);


	if( 0 < dark )
	{
//		colorNum = 1.0f / 255;
		colorNum = darkMode;
	}


//	float areaRateThreshold = 0.5f;	// 面積比がこの値に満たない場合にAA処理をする

	// AAによる「暗さ」への影響度 [ 0.0, 1.0 )
	//   面積比が areaRateThreshold 以上であれば、AARate は 0.0 になる
	//   面積比が areaRateThreshold 未満であれば、面積比が 0.0 に近づくにつれ、AARate も 1.0 に近づく
	float AARate = max( 1 - areaRate - ( 1.0f - AAThreshold ), 0 ) / AAThreshold;


//	float coeff = light;
	float coeff = light - AARate * AASubtracter;

#elif SELECT == 3	// エッジ等非表示

	float coeff = light;

#endif
#undef SELECT


//	output.RGBColor = tex2D( PS2TextureSampler4, float2( paletteU, 1 - coeff / LIGHT_RANGE ) );


	float bDark = 0;

	colorNum += tex2D( PS2TextureSampler4, float2( paletteU, 1 - coeff / LIGHT_RANGE ) ).r;

	// 明るさ情報が最大値を超えないようにする
	{
		float colorNumMax = tex2D( PS2TextureSampler4, float2( paletteU, 1 ) ).r;

		if( darkMode < 1 )
		{
			// 通常
			colorNum = min( colorNum, colorNumMax );
		}
		else
		{
			// ダークカラーを使用

			if( colorNumMax < colorNum )
			{
				bDark = 1;
			}

			colorNum = min( colorNum, colorNumMax + 1.0f / 255 );
		}
	}


	output.Color = float4( paletteU, colorNum, bDark, 0 );


/*
	if( mode_DispContour == 0 )
	{
		// 輪郭表示なし
	}
	else if( mode_DispContour == 1 )
	{
		// 輪郭表示（単色）

//		if( edge != 0 )
		if( gutter != 0 )
		{
			output.RGBColor = float4( 1, 0, 0, 1 );
		}
	}
*/

	return output;
}

///////////////////////////////////////////////////////////////////////////////

// 色決定パス

struct PS_OUTPUT_SELECTCOLOR
{
	float4 RGBColor		: COLOR0;  // Pixel color
};

PS_OUTPUT_SELECTCOLOR RenderScenePSSelectColor( in float2 texcoord : TEXCOORD0 )
{
	PS_OUTPUT_SELECTCOLOR output;

/*
	//   0
	// 2 C 3
	//   1
	const int2 aNeighborhoodUVTable[] =
	{
		{  0, -1 },
		{  0,  1 },
		{ -1,  0 },
		{  1,  0 },
	};
*/


#define SELECT	2

	float2 paletteUV;

#if SELECT == 0		// 改良前

	paletteUV = tex2D( PS2TextureSampler7, texcoord ).rg;

#elif SELECT == 1	// 適当に改良


	float2 palC = tex2D( PS2TextureSampler7, texcoord ).rg;
	float bDark = tex2D( PS2TextureSampler7, texcoord ).b;

//	float plusMinus = 0;
	float plus = 0;
	float minus = 0;

	bool bExistSamePal = false;


	for( int y = -1;  y <= +1;  y ++ )
	{
		for( int x = -1;  x <= +1;  x ++ )
		{
			if( x == 0  &&  y == 0 )
				continue;

			float2 NbTexCoord = texcoord + invFinalTexSize.xy * float2( x, y );

			float2 pal = tex2D( PS2TextureSampler7, NbTexCoord ).rg;

			if( palC.x == pal.x )
			{
				// 同じマテリアルだった

				if( palC.y == pal.y )
				{
					// 同じ明るさだった
					bExistSamePal = true;
				}
				else
				{
//					plusMinus += pal.y - palC.y;

					plus += max( pal.y - palC.y, 0 );
					minus += max( palC.y - pal.y, 0 );
				}
			}
		}
	}

	if( ! bExistSamePal )
	{
//		palC.y += sign( plusMinus ) * ( 1.0f / 255 );

		if( 0 < plus  &&  0 < minus )
		{
			if( 2 < max( plus, minus ) / min( plus, minus ) )
			{
				palC.y += sign( plus - minus ) * ( 1.0f / 255 );
			}
			else
			{
				// nothing
//				palC.x = 5.0f / 255;	// debug GREEN
			}
		}
		else if( 2.0f / 255 <= plus  ||  2.0f / 255 <= minus )
		{
			palC.y += sign( plus - minus ) * ( 1.0f / 255 );
		}
		else
		{
//			palC.x = 8.0f / 255;	// debug RED
		}
	}

	paletteUV = palC;

#elif SELECT == 2	// ダーク処理に対応

	float2 palC = tex2D( PS2TextureSampler7, texcoord ).rg;
	float bDark = tex2D( PS2TextureSampler7, texcoord ).b;

//	float plusMinus = 0;
	float plus = 0;
	float minus = 0;

	bool bExistSamePal = false;



//	const int checkStep = 5;
	#define CHECK_SAME_MATE_NUM		5
	#define CHECK_SAME_MATE_START	-2
//	#define CHECK_SAME_MATE_STEP	

	float sameMateNum[ CHECK_SAME_MATE_NUM ];

	for( int i = 0;  i < CHECK_SAME_MATE_NUM;  i ++ )
	{
		sameMateNum[ i ] = 0;
	}




	for( int y = -1;  y <= +1;  y ++ )
	{
		for( int x = -1;  x <= +1;  x ++ )
		{
			if( x == 0  &&  y == 0 )
				continue;

			float2 NbTexCoord = texcoord + invFinalTexSize.xy * float2( x, y );

			float2 pal = tex2D( PS2TextureSampler7, NbTexCoord ).rg;

			if( palC.x == pal.x )
			{
				// 同じマテリアルだった

				int sub = (int)round( ( pal.y - palC.y ) * 255 );

				if( sub <= -2 )			sameMateNum[ 0 ] ++;
				else if( sub == -1 )	sameMateNum[ 1 ] ++;
				else if( sub == 0 )		sameMateNum[ 2 ] ++;
				else if( sub == 1 )		sameMateNum[ 3 ] ++;
				else					sameMateNum[ 4 ] ++;

			}
		}
	}


	if( sameMateNum[ 2 ] == 0 )
	{
		// 同じ明るさのマテリアルがなかった

		if( ! bDark )
		{
			// 通常部分

			float plus = sameMateNum[ 3 ] + sameMateNum[ 4 ];
			float minus = sameMateNum[ 0 ] + sameMateNum[ 1 ];

			if( 0 < minus  &&  0 < plus )
			{
				if( 2 < max( plus, minus ) / min( plus, minus ) )
				{
					palC.y += sign( plus - minus ) * ( 1.0f / 255 );
				}
				else
				{
					// nothing
//					palC.x = 5.0f / 255;	// debug GREEN
				}
			}
			else if( 2 <= plus  ||  2 <= minus )
			{
				palC.y += sign( plus - minus ) * ( 1.0f / 255 );
			}
			else
			{
//				palC.x = 8.0f / 255;	// debug RED
			}
		}
		else
		{
			// ダーク部分

			int sameNum[ 9 ];

			for( int i = 0;  i < 9;  i ++ )
			{
				sameNum[ i ] = 0;
			}


			// 周囲に同じマテリアルが存在するなら 1
			int bSameMaterial = min( sameMateNum[ 0 ] + sameMateNum[ 1 ] + sameMateNum[ 3 ] + sameMateNum[ 4 ], 1 );


			// 周囲に同じマテリアルが存在する：
			// 周囲の同じマテリアルの中で、最も面積の多い明るさを選ぶ

			// 周囲に同じマテリアルが存在しない：
			// 周囲のマテリアルの中で、最も面積の多いマテリアルを選ぶ


			for( int i = 0;  i < 9;  i ++ )
			{
				int curX = i % 3 - 1;
				int curY = i / 3 - 1;

				float2 curPal;
				{
					float2 NbTexCoord = texcoord + invFinalTexSize.xy * float2( curX, curY );

					curPal = tex2D( PS2TextureSampler7, NbTexCoord ).rg;
				}

				for( int y = -1;  y <= +1;  y ++ )
				{
					for( int x = -1;  x <= +1;  x ++ )
					{
//						if( x == 0  &&  y == 0 )
//							continue;

						float2 NbTexCoord = texcoord + invFinalTexSize.xy * float2( x, y );

						float2 pal = tex2D( PS2TextureSampler7, NbTexCoord ).rg;

						if( curPal.x == pal.x  &&  curPal.y == pal.y )
						{
							sameNum[ i ] ++;
						}
					}
				}

				// 「周囲に同じマテリアルが存在する」の場合、中央と違うマテリアルは除外する
				if( bSameMaterial  &&  curPal.x != palC.x )
				{
					sameNum[ i ] = 0;
				}
			}









/*
			if( 0 < sameMateNum[ 0 ] + sameMateNum[ 1 ] + sameMateNum[ 3 ] + sameMateNum[ 4 ] )
			{
				// 周囲に同じマテリアルが存在する：
				// 周囲の同じマテリアルの中で、最も面積の多い明るさを選ぶ

				// sameNum[ i ]: 場所 i のマテリアルが中央と同じ場合のみ、場所 i と同じマテリアルで同じ明るさのピクセル数が入る（自分自身もカウントされる）

				for( int i = 0;  i < 9;  i ++ )
				{
					int curX = i % 3 - 1;
					int curY = i / 3 - 1;

					float2 curPal;
					{
						float2 NbTexCoord = texcoord + invFinalTexSize.xy * float2( curX, curY );

						curPal = tex2D( PS2TextureSampler7, NbTexCoord ).rg;
					}

					if( curPal.x == palC.x )
					{
						for( int y = -1;  y <= +1;  y ++ )
						{
							for( int x = -1;  x <= +1;  x ++ )
							{
//								if( x == 0  &&  y == 0 )
//									continue;

								float2 NbTexCoord = texcoord + invFinalTexSize.xy * float2( x, y );

								float2 pal = tex2D( PS2TextureSampler7, NbTexCoord ).rg;

								if( curPal.x == pal.x  &&  curPal.y == pal.y )
								{
									sameNum[ i ] ++;
								}
							}
						}
					}
				}
			}
			else
			{
				// 周囲に同じマテリアルが存在しない：

				// 周囲のマテリアルの中で、最も面積の多いマテリアルを選ぶ
				// 明るさは、最初に調べた（つまり左上の）ピクセルのものを採用

				// sameNum[ i ]: 場所 i のマテリアルが中央と同じ場合、場所 i と同じマテリアルで同じ明るさのピクセル数が入る（自分自身もカウントされる）

				for( int i = 0;  i < 9;  i ++ )
				{
					int curX = i % 3 - 1;
					int curY = i / 3 - 1;

					float2 curPal;
					{
						float2 NbTexCoord = texcoord + invFinalTexSize.xy * float2( curX, curY );

						curPal = tex2D( PS2TextureSampler7, NbTexCoord ).rg;
					}

					for( int y = -1;  y <= +1;  y ++ )
					{
						for( int x = -1;  x <= +1;  x ++ )
						{
//							if( x == 0  &&  y == 0 )
//								continue;

							float2 NbTexCoord = texcoord + invFinalTexSize.xy * float2( x, y );

							float2 pal = tex2D( PS2TextureSampler7, NbTexCoord ).rg;

							if( curPal.x == pal.x  &&  curPal.y == pal.y )
							{
								sameNum[ i ] ++;
							}
						}
					}
				}
			}
*/

			int mostIndex = 0;
			int mostNum = sameNum[ 0 ];

			for( int i = 1;  i < 9;  i ++ )
			{
				if( i == 4 )
				{
					// 中央の値は無視
					continue;
				}

				if( mostNum < sameNum[ i ] )
				{
					mostIndex = i;
					mostNum = sameNum[ i ];
				}
			}

			{
				int x = mostIndex % 3 - 1;
				int y = mostIndex / 3 - 1;

				float2 NbTexCoord = texcoord + invFinalTexSize.xy * float2( x, y );

				palC = tex2D( PS2TextureSampler7, NbTexCoord ).rg;
			}
		}
	}

	paletteUV = palC;

#endif
#undef SELECT


	// 色情報の座標に変換
	paletteUV.x += 0.5f;
	paletteUV.y += PALETTE_TEX_COLOR_START_Y;

	// テクセルの中心に変換
	paletteUV.x += 0.5f * invSrcTexSize.z;

	paletteUV.y = ( round( paletteUV.y * 255.0f ) + 0.5f ) * invSrcTexSize.z;	// 値補正＆テクセルの中心に変換


	output.RGBColor = tex2D( PS2TextureSampler4, paletteUV );

	return output;
}

///////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////
// Techniques specs follow
//////////////////////////////////////
technique Mesh
{
	pass p0
	{
		VertexShader = (vsArray[CurNumBones]);

//		PixelShader  = compile ps_3_0 RenderScenePS( true ); // trivial pixel shader (could use FF instead if desired)
		PixelShader  = (psArray[PSMode]);
	}
}


technique Analyze_CountPalette_MeanPos
{
	pass p0
	{
		PixelShader  = compile ps_3_0 RenderScenePSAnalyze_CountPalette_MeanPos();
	}
}


technique Analyze
{
	pass p0
	{
		PixelShader  = compile ps_3_0 RenderScenePSAnalyze();
	}
}


int mixPass_mode_DispContour = 0;

PixelShader mixPass_psArray[3] = {	compile ps_3_0 RenderScenePSMix( 0 ),
									compile ps_3_0 RenderScenePSMix( 1 ),
									compile ps_3_0 RenderScenePSMix( 1 ),
								};

technique Mix
{
	pass p0
	{
//		PixelShader  = compile ps_3_0 RenderScenePSMix();
		PixelShader = mixPass_psArray[ mixPass_mode_DispContour ];
	}
}

technique SelectColor
{
	pass p0
	{
		PixelShader  = compile ps_3_0 RenderScenePSSelectColor();
	}
}
