//--------------------------------------------------------------------------------------
// File: SkinnedMesh.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTsettingsdlg.h"
#include "SDKmisc.h"
#include "resource.h"

#include <imagehlp.h>

//#include <tchar.h>

#include "printf.h"
#include "misc.h"
#include "CConfig.h"
#include "Performance.h"
#include "LogFile.h"


///////////////////////////////////////////////////////////////////////////////

#pragma warning( disable : 4244 )	//  'int' から 'FLOAT' への変換です。データが失われる可能性があります。

///////////////////////////////////////////////////////////////////////////////


//#define DEBUG_VS	 // Uncomment this line to debug vertex shaders
//#define DEBUG_PS	 // Uncomment this line to debug pixel shaders

#define SHADER_USE_PRECOMPILED	// コンパイル済みシェーダーを使う


#ifndef SHADER_USE_PRECOMPILED
	#define SHADERFILE	L"SkinnedMesh.fx"
#else
	#define SHADERFILE	L"SkinnedMesh.fxb"
#endif


#define PALETTEFILENAME	L"palette.png"


#define CONFIG_NAME	"config.teco"

/*
WCHAR g_wszShaderSource[4][30] =
{
	L"skinmesh1.vsh",
	L"skinmesh2.vsh",
	L"skinmesh3.vsh",
	L"skinmesh4.vsh"
};
*/

//#define CAMERA_FRAME_NAME	"UniqueName0"
#define CAMERA_FRAME_NAME	"camera"

#define DIR_LIGHT_FRAME_NAME	"dirlight%d"


// enum for various skinning modes possible
enum METHOD
{
	D3DNONINDEXED,
	D3DINDEXED,
	SOFTWARE,
	D3DINDEXEDVS,
	D3DINDEXEDHLSLVS,
	NONE
};


struct TLVertex
{
    D3DXVECTOR4 p;
    D3DXVECTOR2 t;
};

static const DWORD          FVF_TLVERTEX = D3DFVF_XYZRHW | D3DFVF_TEX1;


//--------------------------------------------------------------------------------------
// Name: struct D3DXFRAME_DERIVED
// Desc: Structure derived from D3DXFRAME so we can add some app-specific
//		 info that will be stored with each frame
//--------------------------------------------------------------------------------------
struct D3DXFRAME_DERIVED : public D3DXFRAME
{
	D3DXMATRIXA16 CombinedTransformationMatrix;
};


//--------------------------------------------------------------------------------------
// Name: struct D3DXMESHCONTAINER_DERIVED
// Desc: Structure derived from D3DXMESHCONTAINER so we can add some app-specific
//		 info that will be stored with each mesh
//--------------------------------------------------------------------------------------
struct D3DXMESHCONTAINER_DERIVED : public D3DXMESHCONTAINER
{
	LPDIRECT3DTEXTURE9* ppTextures; 	  // array of textures, entries are NULL if no texture specified

	// SkinMesh info
	LPD3DXMESH pOrigMesh;
	LPD3DXATTRIBUTERANGE pAttributeTable;
	DWORD NumAttributeGroups;
	DWORD NumInfl;
	LPD3DXBUFFER pBoneCombinationBuf;
	D3DXMATRIX** ppBoneMatrixPtrs;
	D3DXMATRIX* pBoneOffsetMatrices;
	DWORD NumPaletteEntries;
	bool UseSoftwareVP;
	DWORD iAttributeSW; 	// used to denote the split between SW and HW if necessary for non-indexed skinning
};


//--------------------------------------------------------------------------------------
// Name: class CAllocateHierarchy
// Desc: Custom version of ID3DXAllocateHierarchy with custom methods to create
//		 frames and meshcontainers.
//--------------------------------------------------------------------------------------
class CAllocateHierarchy : public ID3DXAllocateHierarchy
{
public:
	STDMETHOD( CreateFrame )( THIS_ LPCSTR Name, LPD3DXFRAME *ppNewFrame );
	STDMETHOD( CreateMeshContainer )( THIS_
		LPCSTR Name,
		CONST D3DXMESHDATA *pMeshData,
		CONST D3DXMATERIAL *pMaterials,
		CONST D3DXEFFECTINSTANCE *pEffectInstances,
		DWORD NumMaterials,
		CONST DWORD *pAdjacency,
		LPD3DXSKININFO pSkinInfo,
		LPD3DXMESHCONTAINER *ppNewMeshContainer );
	STDMETHOD( DestroyFrame )( THIS_ LPD3DXFRAME pFrameToFree );
	STDMETHOD( DestroyMeshContainer )( THIS_ LPD3DXMESHCONTAINER pMeshContainerBase );

	CAllocateHierarchy()
	{
	}
};


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
ID3DXFont*					g_pFont = NULL; 		// Font for drawing text
ID3DXSprite*				g_pTextSprite = NULL;	// Sprite for batching draw text calls
ID3DXEffect*				g_pEffect = NULL;		// D3DX effect interface
CD3DArcBall 				g_ArcBall;				// Arcball for model control
bool						g_bShowHelp = true; 	// If true, it renders the UI control text
CDXUTDialogResourceManager	g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg 			g_SettingsDlg;			// Device settings dialog
CDXUTDialog 				g_HUD;					// dialog for standard controls
CDXUTDialog 				g_SampleUI; 			// dialog for sample specific controls
LPD3DXFRAME 				g_pFrameRoot = NULL;
ID3DXAnimationController*	g_pAnimController = NULL;
D3DXVECTOR3 				g_vObjectCenter;		// Center of bounding sphere of object
FLOAT						g_fObjectRadius;		// Radius of bounding sphere of object
//METHOD					g_SkinningMethod = D3DNONINDEXED; // Current skinning method
METHOD						g_SkinningMethod = D3DINDEXEDHLSLVS; // Current skinning method
D3DXMATRIXA16*				g_pBoneMatrices = NULL;
UINT						g_NumBoneMatricesMax = 0;
//IDirect3DVertexShader9* 	g_pIndexedVertexShader[4];
D3DXMATRIXA16				g_matView;				// View matrix
D3DXMATRIXA16				g_matProj;				// Projection matrix
D3DXMATRIXA16				g_matProjT; 			// Transpose of projection matrix (for asm shader)
DWORD						g_dwBehaviorFlags;		// Behavior flags of the 3D device
bool						g_bUseSoftwareVP;		// Flag to indicate whether software vp is
// required due to lack of hardware


LPD3DXFRAME 				g_pFrameCamera = NULL;	// カメラフレーム（存在しない場合もある）


#define DIR_LIGHT_MAX	3

LPD3DXFRAME 				g_pFrameDirLight[ DIR_LIGHT_MAX ];	// ディレクショナルライトのフレーム（存在しない場合もある）



LogFile *g_pLogFile = NULL;	// ログファイル
#define LOG( color, ... )	g_pLogFile->PrintLine( color, __VA_ARGS__ )
#define LOG_ERR( ... )		LOG( 0xff0000, __VA_ARGS__ )


int g_windowWidth = 640;
int g_windowHeight = 480;

int g_resultDispScale = 1;


WCHAR g_strExePath[ MAX_PATH ];	// exeファイルのパス（最後が \\ )

WCHAR g_strModelPath[ MAX_PATH ];	// モデルファイルのパス（最後が \\ )
WCHAR g_strModelName[ MAX_PATH ];	// モデルファイル名（パスを含む）

WCHAR g_strCapturePath[ MAX_PATH ];	// 画像の出力パス（最後が \\ )


LPDIRECT3DTEXTURE9			g_pPaletteTexture = NULL;		// A8R8G8B8	パレットテクスチャ(read only)

// ここから作業用サーフェイス（タイル分割時に再利用される）
LPDIRECT3DTEXTURE9			g_pColorTexture = NULL;			// A8R8G8B8	照明係数, paletteU, 0, 0
LPDIRECT3DTEXTURE9			g_pNormalMateTexture = NULL;	// A8R8G8B8	法線xyz, 1
LPDIRECT3DTEXTURE9			g_pDepthTexture = NULL;			// R32F		座標xyz, 深度	// 深度
LPDIRECT3DTEXTURE9			g_pWorkDSTexture = NULL;		// DepthStencil

LPDIRECT3DTEXTURE9			g_pColorTexture_SysMem = NULL;	// g_pColorTexture をCPUで読むためのコピー先
LPDIRECT3DTEXTURE9			g_pDepthTexture_SysMem = NULL;	// g_pDepthTexture をCPUで読むためのコピー先
LPDIRECT3DTEXTURE9			g_pAnalyzedTexture = NULL;		// 解析結果

LPDIRECT3DTEXTURE9			g_pColorTexture2 = NULL;		// A8R8G8B8	照明係数, paletteU, 最大角度差, 最大Z差（結果と同じサイズ）
LPDIRECT3DTEXTURE9			g_pMeanPosTexture = NULL;		// RGBA32F	平均座標（結果と同じサイズ）
LPDIRECT3DTEXTURE9			g_pAnalyzedTexture2 = NULL;		// 解析結果2

LPDIRECT3DTEXTURE9			g_pSplitTexture = NULL;			// A8R8G8B8	結果（分割部分）
// ここまで作業用サーフェイス



LPDIRECT3DTEXTURE9			g_pCombinedTexture = NULL;		// A8R8G8B8	結合結果
LPDIRECT3DTEXTURE9			g_pFinalTexture = NULL;			// A8R8G8B8	最終結果
LPDIRECT3DTEXTURE9			g_pFinalTexture_SysMem = NULL;	// g_pFinalTexture をCPUで読むためのコピー先


LPDIRECT3DQUERY9			g_pD3dQuery = NULL;				// 描画完了のタイミングを調べるためのクエリ


int paletteTextureWidth;

#define FINAL_SURFACE_WIDTH		256
#define FINAL_SURFACE_HEIGHT	256

#define SCALE_RATE				5

#define SPLIT_MARGIN			2	// 分割レンダリング時の余白
#define SPLIT_MARGIN_TOTAL		( SPLIT_MARGIN * 2 )

#define WORK_SURFACE_WIDTH		( FINAL_SURFACE_WIDTH * SCALE_RATE )
#define WORK_SURFACE_HEIGHT		( FINAL_SURFACE_HEIGHT * SCALE_RATE )

//--------------------------------------------------------------------------------------

// シェーダーパラメータ
int		g_dispContour = 0;
//float	g_ZThreshold = 0.088f;	// 0.05f
//float	g_AngleThreshold = 138.0f;	// 60.0f

//float	g_ZThreshold = 0.075f;	// 0.05f
float	g_ZThreshold = 0.1f;
float	g_ZThreshold_min = 0.001f;
float	g_ZThreshold_max = 0.5f;
#define SCALE_ZThreshold	1000

float	g_AngleThreshold = 29.0f;
float	g_AngleThreshold_min = 0.1f;
float	g_AngleThreshold_max = 45.0f;
#define SCALE_AngleThreshold	10

float	g_GutterThreshold = 0.2f;
float	g_GutterThreshold_min = 0.0f;
float	g_GutterThreshold_max = 4.0f;
#define SCALE_GutterThreshold	10

float	g_IgnoreCountThreshold = 0.3f;	// 1.5f / 5.0f
float	g_IgnoreCountThreshold_min = 0.0f;
float	g_IgnoreCountThreshold_max = 1.0f;
#define SCALE_IgnoreCountThreshold	10


typedef struct
{
//	BYTE a, r, g, b;
//	BYTE r, g, b, a;
	BYTE b, g, r, a;

} sARGB8;

typedef struct
{
//	float b, g, r, a;
	float r, g, b, a;

} sARGB32f;


//--------------------------------------------------------------------------------------

int g_workSurfaceSizeMax[ 2 ] = { 1024*2, 1024*2 };	// 拡大サーフェイスの最大サイズ

int g_workSurfaceSize[ 2 ];		// 拡大サーフェイスのサイズ

int g_splitSurfaceSize[ 2 ];	// 分割されたサーフェイスのサイズ

int g_splitNum[ 2 ];	// 分割数


int g_finalSurfaceSize[ 2 ] = { 256, 256 };	// 最終的なサーフェイスのサイズ
//int g_finalSurfaceSize[ 2 ] = { 640, 480 };	// 最終的なサーフェイスのサイズ
//int g_finalSurfaceSize[ 2 ] = { 1280, 1280 };	// 最終的なサーフェイスのサイズ


D3DXMATRIXA16	g_matProjSplit;	// Projection matrix に更に分割レンダリング用のスケール、平行移動を加えたもの



//--------------------------------------------------------------------------------------

bool g_bDebugCapture = false;

bool g_bCapture = true;
//bool g_bCapture = false;

double g_fps = 0;
double g_period = 0;

int g_currentFrame = 0;
int g_frameMax = 0;

int g_frameNum = 0;	// 起動してからのフレーム数

//--------------------------------------------------------------------------------------

CConfig g_config;


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN	1
#define IDC_TOGGLEREF			3
#define IDC_CHANGEDEVICE		4
#define IDC_METHOD				5

enum
{
	IDC_DISPCONTOUR	= 10,

	IDC_Z_THRESHOLD_TEXT,
	IDC_Z_THRESHOLD,

	IDC_ANGLE_THRESHOLD_TEXT,
	IDC_ANGLE_THRESHOLD,

	IDC_GUTTER_THRESHOLD_TEXT,
	IDC_GUTTER_THRESHOLD,

	IDC_IGNORE_COUNT_THRESHOLD_TEXT,
	IDC_IGNORE_COUNT_THRESHOLD,
};



//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
bool CALLBACK IsDeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, bool bWindowed,
								  void* pUserContext );
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
HRESULT CALLBACK OnCreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
								 void* pUserContext );
HRESULT CALLBACK OnResetDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
								void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
void CALLBACK OnFrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
						  void* pUserContext );
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
void CALLBACK OnLostDevice( void* pUserContext );
void CALLBACK OnDestroyDevice( void* pUserContext );

bool InitApp();
HRESULT LoadMesh( IDirect3DDevice9* pd3dDevice, WCHAR* strFileName, ID3DXMesh** ppMesh );
void RenderText();
void DrawMeshContainer( IDirect3DDevice9* pd3dDevice, LPD3DXMESHCONTAINER pMeshContainerBase, LPD3DXFRAME pFrameBase );
void DrawFrame( IDirect3DDevice9* pd3dDevice, LPD3DXFRAME pFrame );
HRESULT SetupBoneMatrixPointersOnMesh( LPD3DXMESHCONTAINER pMeshContainer );
HRESULT SetupBoneMatrixPointers( LPD3DXFRAME pFrame );
void UpdateFrameMatrices( LPD3DXFRAME pFrameBase, LPD3DXMATRIX pParentMatrix );
void UpdateSkinningMethod( LPD3DXFRAME pFrameBase );
HRESULT GenerateSkinnedMesh( IDirect3DDevice9* pd3dDevice, D3DXMESHCONTAINER_DERIVED* pMeshContainer );
void ReleaseAttributeTable( LPD3DXFRAME pFrameBase );

LPD3DXFRAME SearchFrame( LPD3DXFRAME pFrame, char *szName );
void Debug_DispFrame( LPD3DXFRAME pFrame, int indent = 0 );

void GetFrameMatrix( LPD3DXFRAME pFrame, D3DXMATRIX *pMat );

void PrintMatrix( D3DMATRIX *p );

#define PRINT_MATRIX( p )						\
	{											\
		g_pLogFile->PrintLine( 0, L"%S:", #p );	\
		PrintMatrix( p );						\
	}

void LOG_HRESULT( HRESULT hr );

//--------------------------------------------------------------------------------------
// Name: AllocateName()
// Desc: Allocates memory for a string to hold the name of a frame or mesh
//--------------------------------------------------------------------------------------
HRESULT AllocateName( LPCSTR Name, LPSTR* pNewName )
{
	UINT cbLength;

	if( Name != NULL )
	{
		cbLength = ( UINT )strlen( Name ) + 1;
		*pNewName = new CHAR[cbLength];
		if( *pNewName == NULL )
			return E_OUTOFMEMORY;
		memcpy( *pNewName, Name, cbLength * sizeof( CHAR ) );
	}
	else
	{
		*pNewName = NULL;
	}

	return S_OK;
}




//--------------------------------------------------------------------------------------
// Name: CAllocateHierarchy::CreateFrame()
// Desc:
//--------------------------------------------------------------------------------------
HRESULT CAllocateHierarchy::CreateFrame( LPCSTR Name, LPD3DXFRAME* ppNewFrame )
{
	HRESULT hr = S_OK;
	D3DXFRAME_DERIVED* pFrame;

	*ppNewFrame = NULL;

	pFrame = new D3DXFRAME_DERIVED;
	if( pFrame == NULL )
	{
		hr = E_OUTOFMEMORY;
		goto e_Exit;
	}

	hr = AllocateName( Name, &pFrame->Name );
	if( FAILED( hr ) )
		goto e_Exit;

	// initialize other data members of the frame
	D3DXMatrixIdentity( &pFrame->TransformationMatrix );
	D3DXMatrixIdentity( &pFrame->CombinedTransformationMatrix );

	pFrame->pMeshContainer = NULL;
	pFrame->pFrameSibling = NULL;
	pFrame->pFrameFirstChild = NULL;

	*ppNewFrame = pFrame;
	pFrame = NULL;

e_Exit:
	delete pFrame;
	return hr;
}




//--------------------------------------------------------------------------------------
// Name: CAllocateHierarchy::CreateMeshContainer()
// Desc:
//--------------------------------------------------------------------------------------
HRESULT CAllocateHierarchy::CreateMeshContainer(
	LPCSTR Name,
	CONST D3DXMESHDATA *pMeshData,
	CONST D3DXMATERIAL *pMaterials,
	CONST D3DXEFFECTINSTANCE *pEffectInstances,
	DWORD NumMaterials,
	CONST DWORD *pAdjacency,
	LPD3DXSKININFO pSkinInfo,
	LPD3DXMESHCONTAINER *ppNewMeshContainer )
{
	HRESULT hr;
	D3DXMESHCONTAINER_DERIVED *pMeshContainer = NULL;
	UINT NumFaces;
	UINT iMaterial;
	UINT iBone, cBones;
	LPDIRECT3DDEVICE9 pd3dDevice = NULL;

	LPD3DXMESH pMesh = NULL;

	*ppNewMeshContainer = NULL;

	// this sample does not handle patch meshes, so fail when one is found
	if( pMeshData->Type != D3DXMESHTYPE_MESH )
	{
		hr = E_FAIL;
		goto e_Exit;
	}

	// get the pMesh interface pointer out of the mesh data structure
	pMesh = pMeshData->pMesh;

	// this sample does not FVF compatible meshes, so fail when one is found
	if( pMesh->GetFVF() == 0 )
	{
		hr = E_FAIL;
		goto e_Exit;
	}

	// allocate the overloaded structure to return as a D3DXMESHCONTAINER
	pMeshContainer = new D3DXMESHCONTAINER_DERIVED;
	if( pMeshContainer == NULL )
	{
		hr = E_OUTOFMEMORY;
		goto e_Exit;
	}
	memset( pMeshContainer, 0, sizeof( D3DXMESHCONTAINER_DERIVED ) );

	// make sure and copy the name.  All memory as input belongs to caller, interfaces can be addref'd though
	hr = AllocateName( Name, &pMeshContainer->Name );
	if( FAILED( hr ) )
		goto e_Exit;

	pMesh->GetDevice( &pd3dDevice );
	NumFaces = pMesh->GetNumFaces();

	// if no normals are in the mesh, add them
	if( !( pMesh->GetFVF() & D3DFVF_NORMAL ) )
	{
		pMeshContainer->MeshData.Type = D3DXMESHTYPE_MESH;

		// clone the mesh to make room for the normals
		hr = pMesh->CloneMeshFVF( pMesh->GetOptions(),
									pMesh->GetFVF() | D3DFVF_NORMAL,
									pd3dDevice, &pMeshContainer->MeshData.pMesh );
		if( FAILED( hr ) )
			goto e_Exit;

		// get the new pMesh pointer back out of the mesh container to use
		// NOTE: we do not release pMesh because we do not have a reference to it yet
		pMesh = pMeshContainer->MeshData.pMesh;

		// now generate the normals for the pmesh
		D3DXComputeNormals( pMesh, NULL );
	}
	else  // if no normals, just add a reference to the mesh for the mesh container
	{
		pMeshContainer->MeshData.pMesh = pMesh;
		pMeshContainer->MeshData.Type = D3DXMESHTYPE_MESH;

		pMesh->AddRef();
	}

	// allocate memory to contain the material information.  This sample uses
	//	 the D3D9 materials and texture names instead of the EffectInstance style materials
	pMeshContainer->NumMaterials = max( 1, NumMaterials );
	pMeshContainer->pMaterials = new D3DXMATERIAL[pMeshContainer->NumMaterials];
	pMeshContainer->ppTextures = new LPDIRECT3DTEXTURE9[pMeshContainer->NumMaterials];
	pMeshContainer->pAdjacency = new DWORD[NumFaces*3];
	if( ( pMeshContainer->pAdjacency == NULL ) || ( pMeshContainer->pMaterials == NULL ) )
	{
		hr = E_OUTOFMEMORY;
		goto e_Exit;
	}

	memcpy( pMeshContainer->pAdjacency, pAdjacency, sizeof( DWORD ) * NumFaces*3 );
	memset( pMeshContainer->ppTextures, 0, sizeof( LPDIRECT3DTEXTURE9 ) * pMeshContainer->NumMaterials );

	// if materials provided, copy them
	if( NumMaterials > 0 )
	{
		memcpy( pMeshContainer->pMaterials, pMaterials, sizeof( D3DXMATERIAL ) * NumMaterials );

		for( iMaterial = 0; iMaterial < NumMaterials; iMaterial++ )
		{
			if( pMeshContainer->pMaterials[iMaterial].pTextureFilename != NULL )
			{
				WCHAR strTexturePath[MAX_PATH];
				WCHAR wszBuf[MAX_PATH];
				MultiByteToWideChar( CP_ACP, 0, pMeshContainer->pMaterials[iMaterial].pTextureFilename, -1, wszBuf, MAX_PATH );
				wszBuf[MAX_PATH - 1] = L'\0';
				DXUTFindDXSDKMediaFileCch( strTexturePath, MAX_PATH, wszBuf );
				if( FAILED( D3DXCreateTextureFromFile( pd3dDevice, strTexturePath,
														&pMeshContainer->ppTextures[iMaterial] ) ) )
					pMeshContainer->ppTextures[iMaterial] = NULL;

				// don't remember a pointer into the dynamic memory, just forget the name after loading
				pMeshContainer->pMaterials[iMaterial].pTextureFilename = NULL;
			}
		}
	}
	else // if no materials provided, use a default one
	{
		pMeshContainer->pMaterials[0].pTextureFilename = NULL;
		memset( &pMeshContainer->pMaterials[0].MatD3D, 0, sizeof( D3DMATERIAL9 ) );
		pMeshContainer->pMaterials[0].MatD3D.Diffuse.r = 0.5f;
		pMeshContainer->pMaterials[0].MatD3D.Diffuse.g = 0.5f;
		pMeshContainer->pMaterials[0].MatD3D.Diffuse.b = 0.5f;
		pMeshContainer->pMaterials[0].MatD3D.Specular = pMeshContainer->pMaterials[0].MatD3D.Diffuse;
	}

	// if there is skinning information, save off the required data and then setup for HW skinning
	if( pSkinInfo != NULL )
	{
		// first save off the SkinInfo and original mesh data
		pMeshContainer->pSkinInfo = pSkinInfo;
		pSkinInfo->AddRef();

		pMeshContainer->pOrigMesh = pMesh;
		pMesh->AddRef();

		// Will need an array of offset matrices to move the vertices from the figure space to the bone's space
		cBones = pSkinInfo->GetNumBones();
		pMeshContainer->pBoneOffsetMatrices = new D3DXMATRIX[cBones];
		if( pMeshContainer->pBoneOffsetMatrices == NULL )
		{
			hr = E_OUTOFMEMORY;
			goto e_Exit;
		}

		// get each of the bone offset matrices so that we don't need to get them later
		for( iBone = 0; iBone < cBones; iBone++ )
		{
			pMeshContainer->pBoneOffsetMatrices[iBone] = *( pMeshContainer->pSkinInfo->GetBoneOffsetMatrix( iBone ) );
		}

		// GenerateSkinnedMesh will take the general skinning information and transform it to a HW friendly version
		hr = GenerateSkinnedMesh( pd3dDevice, pMeshContainer );
		if( FAILED( hr ) )
			goto e_Exit;
	}

	*ppNewMeshContainer = pMeshContainer;
	pMeshContainer = NULL;

e_Exit:
	SAFE_RELEASE( pd3dDevice );

	// call Destroy function to properly clean up the memory allocated
	if( pMeshContainer != NULL )
	{
		DestroyMeshContainer( pMeshContainer );
	}

	return hr;
}




//--------------------------------------------------------------------------------------
// Name: CAllocateHierarchy::DestroyFrame()
// Desc:
//--------------------------------------------------------------------------------------
HRESULT CAllocateHierarchy::DestroyFrame( LPD3DXFRAME pFrameToFree )
{
	SAFE_DELETE_ARRAY( pFrameToFree->Name );
	SAFE_DELETE( pFrameToFree );
	return S_OK;
}




//--------------------------------------------------------------------------------------
// Name: CAllocateHierarchy::DestroyMeshContainer()
// Desc:
//--------------------------------------------------------------------------------------
HRESULT CAllocateHierarchy::DestroyMeshContainer( LPD3DXMESHCONTAINER pMeshContainerBase )
{
	UINT iMaterial;
	D3DXMESHCONTAINER_DERIVED* pMeshContainer = ( D3DXMESHCONTAINER_DERIVED* )pMeshContainerBase;

	SAFE_DELETE_ARRAY( pMeshContainer->Name );
	SAFE_DELETE_ARRAY( pMeshContainer->pAdjacency );
	SAFE_DELETE_ARRAY( pMeshContainer->pMaterials );
	SAFE_DELETE_ARRAY( pMeshContainer->pBoneOffsetMatrices );

	// release all the allocated textures
	if( pMeshContainer->ppTextures != NULL )
	{
		for( iMaterial = 0; iMaterial < pMeshContainer->NumMaterials; iMaterial++ )
		{
			SAFE_RELEASE( pMeshContainer->ppTextures[iMaterial] );
		}
	}

	SAFE_DELETE_ARRAY( pMeshContainer->ppTextures );
	SAFE_DELETE_ARRAY( pMeshContainer->ppBoneMatrixPtrs );
	SAFE_RELEASE( pMeshContainer->pBoneCombinationBuf );
	SAFE_RELEASE( pMeshContainer->MeshData.pMesh );
	SAFE_RELEASE( pMeshContainer->pSkinInfo );
	SAFE_RELEASE( pMeshContainer->pOrigMesh );
	SAFE_DELETE( pMeshContainer );
	return S_OK;
}


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
INT WINAPI wWinMain( HINSTANCE, HINSTANCE, LPWSTR, int )
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

	g_pLogFile = new LogFile( L"log.html", L"Log of Pixel Art Shader" );

	CTimeCount::Init( g_pLogFile );

	// Set the callback functions. These functions allow DXUT to notify
	// the application about device changes, user input, and windows messages.	The
	// callbacks are optional so you need only set callbacks for events you're interested
	// in. However, if you don't handle the device reset/lost callbacks then the sample
	// framework won't be able to reset your device since the application must first
	// release all device resources before resetting.  Likewise, if you don't handle the
	// device created/destroyed callbacks then DXUT won't be able to
	// recreate your device resources.
	DXUTSetCallbackD3D9DeviceCreated( OnCreateDevice );
	DXUTSetCallbackD3D9DeviceReset( OnResetDevice );
	DXUTSetCallbackD3D9FrameRender( OnFrameRender );
	DXUTSetCallbackD3D9DeviceLost( OnLostDevice );
	DXUTSetCallbackD3D9DeviceDestroyed( OnDestroyDevice );
	DXUTSetCallbackMsgProc( MsgProc );
	DXUTSetCallbackKeyboard( KeyboardProc );
	DXUTSetCallbackFrameMove( OnFrameMove );
	DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
	DXUTCreateWindow( L"Pixel Art Shader" );

	// Show the cursor and clip it when in full screen
	DXUTSetCursorSettings( true, true );

	if( ! InitApp() )
	{
		return 1;
	}

	// Initialize DXUT and create the desired Win32 window and Direct3D
	// device for the application. Calling each of these functions is optional, but they
	// allow you to set several options which control the behavior of the framework.
	DXUTInit( true, true ); // Parse the command line and show msgboxes
	DXUTSetHotkeyHandling( true, true, true );	// handle the defaul hotkeys

	// Supports all types of vertex processing, including mixed.
	DXUTGetD3D9Enumeration()->SetPossibleVertexProcessingList( true, true, true, true );

	DXUTCreateDevice( true, g_windowWidth, g_windowHeight );


	WCHAR strCWD[ MAX_PATH ];
	GetCurrentDirectory( MAX_PATH, strCWD );

	if( ! SetCurrentDirectory( g_strCapturePath ) )
	{
		return 1;
	}

LOG( 0x000080, L"main loop start" );


	// Pass control to DXUT for handling the message pump and
	// dispatching render calls. DXUT will call your FrameMove
	// and FrameRender callback when there is idle time between handling window messages.
	DXUTMainLoop();

LOG( 0x000080, L"main loop finished" );


	SetCurrentDirectory( strCWD );


	// Perform any application-level cleanup here. Direct3D device resources are released within the
	// appropriate callback functions and therefore don't require any cleanup code here.
	ReleaseAttributeTable( g_pFrameRoot );
	delete[] g_pBoneMatrices;


	delete g_pLogFile;


	return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app
//--------------------------------------------------------------------------------------
bool InitApp()
{
	// exeファイルのパスを得る
	{
		GetModuleFileName( NULL, g_strExePath, MAX_PATH );
		g_strExePath[ MAX_PATH - 1 ] = 0;

		WCHAR *strLastSlash = wcsrchr( g_strExePath, TEXT( '\\' ) );
		if( strLastSlash )
		{
			strLastSlash[ 1 ] = 0;
		}
	}


	// 設定読み込み
	{
		if( ! g_config.Init() )
		{
			return false;
		}

		SConfig *pConfig = g_config.GetConfig();


		// シェーダーパラメータの初期値
		{
			pConfig->shader_ZThreshold = g_ZThreshold;
			pConfig->shader_ZThreshold_min = g_ZThreshold_min;
			pConfig->shader_ZThreshold_max = g_ZThreshold_max;

			pConfig->shader_AngleThreshold = g_AngleThreshold;
			pConfig->shader_AngleThreshold_min = g_AngleThreshold_min;
			pConfig->shader_AngleThreshold_max = g_AngleThreshold_max;

			pConfig->shader_GutterThreshold = g_GutterThreshold;
			pConfig->shader_GutterThreshold_min = g_GutterThreshold_min;
			pConfig->shader_GutterThreshold_max = g_GutterThreshold_max;

			pConfig->shader_IgnoreCountThreshold = g_IgnoreCountThreshold;
			pConfig->shader_IgnoreCountThreshold_min = g_IgnoreCountThreshold_min;
			pConfig->shader_IgnoreCountThreshold_max = g_IgnoreCountThreshold_max;
		}


		g_config.Load( CONFIG_NAME );

		// モデルファイル名
		if( pConfig->szModelName[ 0 ] != '\0' )
		{
			if( pConfig->szModelName[ 1 ] == ':' )
			{
				// フルパス指定だった
				ConvWideChar( g_strModelName, MAX_PATH, pConfig->szModelName );
			}
			else
			{
				// 相対指定だったので、exeのパスを付加する
				wcscpy_s( g_strModelName, MAX_PATH, g_strExePath );

				wcscat_s( g_strModelName, MAX_PATH, GetWideChar( pConfig->szModelName ) );
			}
		}
		else
		{
			// モデルファイル名の指定がなかった
			MessageBox( NULL, L"model not specified", L"error", MB_OK | MB_ICONERROR );
			return false;
		}

		// モデルファイルの存在確認
		if( GetFileAttributes( g_strModelName ) == 0xFFFFFFFF )
		{
			MessageBox( NULL, L"model not found", L"error", MB_OK | MB_ICONERROR );
			return false;
		}


		// モデルファイルのパスを生成
		{
			wcscpy_s( g_strModelPath, MAX_PATH, g_strModelName );

			WCHAR *pLastSlash = wcsrchr( g_strModelPath, L'\\' );
			if( pLastSlash )
			{
				++ pLastSlash;
				*pLastSlash = 0;
			}
		}


		// モデルディレクトリに設定ファイルがあれば、更に読み込む
		{
			char temp[ MAX_PATH ];

			WideCharToMultiByte( CP_ACP, 0, g_strModelPath, -1, temp, MAX_PATH, NULL, NULL );

			strcat_s( temp, MAX_PATH, CONFIG_NAME );

			g_config.Load( temp );
		}


		// 画像の出力パス
		if( pConfig->szCapturePath[ 0 ] != '\0' )
		{
			if( pConfig->szCapturePath[ 1 ] == ':' )
			{
				// フルパス指定だった
				ConvWideChar( g_strCapturePath, MAX_PATH, pConfig->szCapturePath );
			}
			else
			{
				// 相対指定だったので、exeのパスを付加する
				wcscpy_s( g_strCapturePath, MAX_PATH, g_strExePath );

				wcscat_s( g_strCapturePath, MAX_PATH, GetWideChar( pConfig->szCapturePath ) );
			}

			// 末尾に \\ を付ける
			int len = strnlen( pConfig->szCapturePath, sizeof( pConfig->szCapturePath ) );
			if( pConfig->szCapturePath[ len - 1 ] != '\\' )
			{
				wcscat_s( g_strCapturePath, MAX_PATH, L"\\" );
			}


			// 出力パスが存在しなければ作成する
			{
				char *szPath = GetMultiByte( g_strCapturePath );

				if( ! MakeSureDirectoryPathExists( szPath ) )
				{
					LOG_ERR( L"can't open '%s'", szPath );

					return false;
				}
			}
		}

		g_windowWidth = pConfig->windowWidth;
		g_windowHeight = pConfig->windowHeight;
		if( g_windowWidth < 640 )	g_windowWidth = 640;
		if( g_windowHeight < 480 )	g_windowHeight = 480;

		if( 0 < pConfig->result_disp_scale )
			g_resultDispScale = pConfig->result_disp_scale;

		g_bDebugCapture = pConfig->bDebug;

		g_bCapture = pConfig->bCapture;

		g_finalSurfaceSize[ 0 ] = pConfig->resolution[ 0 ];
		g_finalSurfaceSize[ 1 ] = pConfig->resolution[ 1 ];

		g_workSurfaceSizeMax[ 0 ] = pConfig->resolution_inner_max[ 0 ];
		g_workSurfaceSizeMax[ 1 ] = pConfig->resolution_inner_max[ 1 ];

		// カメラ初期位置設定
		{
			D3DXMATRIX tempMat;

			D3DXVECTOR3 vEye( pConfig->camera_pos[ 0 ], pConfig->camera_pos[ 1 ], pConfig->camera_pos[ 2 ] );
			D3DXVECTOR3 vLookAt( pConfig->camera_lookat[ 0 ], pConfig->camera_lookat[ 1 ], pConfig->camera_lookat[ 2 ] );
			D3DXVECTOR3 vUp( 0, 1, 0 );

			D3DXMatrixLookAtLH( &tempMat, &vEye, &vLookAt, &vUp );

			D3DXQUATERNION tempQ;
			D3DXQuaternionRotationMatrix( &tempQ, &tempMat );

			g_ArcBall.SetQuatNow( tempQ );



			D3DXMATRIX *pMat = (D3DXMATRIX *)( g_ArcBall.GetTranslationMatrix() );

#if 0
			pMat->m[ 3 ][0] = - pConfig->camera_pos[ 0 ];
			pMat->m[ 3 ][1] = - pConfig->camera_pos[ 1 ];
			pMat->m[ 3 ][2] = - pConfig->camera_pos[ 2 ];
#elif 0
			pMat->m[ 3 ][0] = + pConfig->camera_pos[ 0 ];
			pMat->m[ 3 ][1] = + pConfig->camera_pos[ 1 ];
			pMat->m[ 3 ][2] = + pConfig->camera_pos[ 2 ];
#else
			pMat->m[ 3 ][0] = + pConfig->camera_lookat[ 0 ];
			pMat->m[ 3 ][1] = + pConfig->camera_lookat[ 1 ];
			pMat->m[ 3 ][2] = + pConfig->camera_lookat[ 2 ];
#endif
		}

		// シェーダーパラメータ設定
		{
			g_ZThreshold = pConfig->shader_ZThreshold;
			g_ZThreshold_min = pConfig->shader_ZThreshold_min;
			g_ZThreshold_max = pConfig->shader_ZThreshold_max;

			g_AngleThreshold = pConfig->shader_AngleThreshold;
			g_AngleThreshold_min = pConfig->shader_AngleThreshold_min;
			g_AngleThreshold_max = pConfig->shader_AngleThreshold_max;

			g_GutterThreshold = pConfig->shader_GutterThreshold;
			g_GutterThreshold_min = pConfig->shader_GutterThreshold_min;
			g_GutterThreshold_max = pConfig->shader_GutterThreshold_max;

			g_IgnoreCountThreshold = pConfig->shader_IgnoreCountThreshold;
			g_IgnoreCountThreshold_min = pConfig->shader_IgnoreCountThreshold_min;
			g_IgnoreCountThreshold_max = pConfig->shader_IgnoreCountThreshold_max;
		}
	}


	// サーフェイスのサイズを算出
	{
		for( int i = 0;  i < 2;  i ++ )
		{
			int renderableSize = g_workSurfaceSizeMax[ i ] / SCALE_RATE - SPLIT_MARGIN_TOTAL;	// 一回でレンダリング可能なサイズ

			g_splitNum[ i ] = ( g_finalSurfaceSize[ i ] + renderableSize - 1 ) / renderableSize;	// 分割数（端数繰り上げ）

			if( g_splitNum[ i ] == 1 )
			{
				// 分割しないなら、ちょうどいいサイズのサーフェイスを作成する
				g_splitSurfaceSize[ i ] = g_finalSurfaceSize[ i ] + SPLIT_MARGIN_TOTAL;
				g_workSurfaceSize[ i ] = g_splitSurfaceSize[ i ] * SCALE_RATE;
			}
			else
			{
				// 分割するなら、可能な限り大きなサイズのサーフェイスを作成する
				g_splitSurfaceSize[ i ] = renderableSize + SPLIT_MARGIN_TOTAL;
				g_workSurfaceSize[ i ] = g_splitSurfaceSize[ i ] * SCALE_RATE;
			}
		}

		LOG( 0, L"g_finalSurfaceSize: %4d, %4d", g_finalSurfaceSize[ 0 ], g_finalSurfaceSize[ 1 ] );
		LOG( 0, L"g_splitSurfaceSize: %4d, %4d", g_splitSurfaceSize[ 0 ], g_splitSurfaceSize[ 1 ] );
		LOG( 0, L"g_workSurfaceSize:  %4d, %4d", g_workSurfaceSize[ 0 ], g_workSurfaceSize[ 1 ] );
		LOG( 0, L"g_splitNum:         %4d, %4d", g_splitNum[ 0 ], g_splitNum[ 1 ] );
	}


	// GUI初期化
	{
		int iY;

		// Initialize dialogs
		g_SettingsDlg.Init( &g_DialogResourceManager );
//		g_HUD.Init( &g_DialogResourceManager );
		g_SampleUI.Init( &g_DialogResourceManager );

/*
		iY = 10;
		g_HUD.SetCallback( OnGUIEvent );
		g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );
		g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22 );
		g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );
*/

		// Add mixed vp to the available vp choices in device settings dialog.
		DXUTGetD3D9Enumeration()->SetPossibleVertexProcessingList( true, false, false, true );

		g_SampleUI.SetCallback( OnGUIEvent );

/*
		iY = 10;
		iY += 45;
		g_SampleUI.AddComboBox( IDC_METHOD, 0, iY, 230, 24, L'S' );
		g_SampleUI.GetComboBox( IDC_METHOD )->AddItem( L"Fixed function non-indexed (s)kinning", ( void* )D3DNONINDEXED );
		g_SampleUI.GetComboBox( IDC_METHOD )->AddItem( L"Fixed function indexed (s)kinning", ( void* )D3DINDEXED );
		g_SampleUI.GetComboBox( IDC_METHOD )->AddItem( L"Software (s)kinning", ( void* )SOFTWARE );
		g_SampleUI.GetComboBox( IDC_METHOD )->AddItem( L"ASM shader indexed (s)kinning", ( void* )D3DINDEXEDVS );
		g_SampleUI.GetComboBox( IDC_METHOD )->AddItem( L"HLSL shader indexed (s)kinning", ( void* )D3DINDEXEDHLSLVS );
*/

		g_SampleUI.EnableNonUserEvents( true );

		// ダイアログを登録（画面の下から）

		// テキストを左揃えにする
		CDXUTElement *pElem = g_SampleUI.GetDefaultElement( DXUT_CONTROL_STATIC, 0 );
		pElem->SetFont( 0, D3DCOLOR_ARGB( 255, 255, 255, 255 ), DT_LEFT | DT_VCENTER );
		pElem->Refresh();


		iY = g_windowHeight;

		// g_IgnoreCountThreshold
		{
			float temp = g_IgnoreCountThreshold;

			iY -= 30;
			g_SampleUI.AddStatic( IDC_IGNORE_COUNT_THRESHOLD_TEXT, L"S", 10 + 200 + 15, iY, 200, 30 );
			g_SampleUI.AddSlider( IDC_IGNORE_COUNT_THRESHOLD, 10, iY, 200, 24,
																				(int)( g_IgnoreCountThreshold_min * SCALE_IgnoreCountThreshold ),
																				(int)( g_IgnoreCountThreshold_max * SCALE_IgnoreCountThreshold ),	// 0.0 ～ 1.0
																				0, false );

			g_IgnoreCountThreshold = temp;
			g_SampleUI.GetSlider( IDC_IGNORE_COUNT_THRESHOLD )->SetValue( (int)( g_IgnoreCountThreshold * SCALE_IgnoreCountThreshold ) );	// 初期値
		}

		// g_GutterThreshold
		{
			float temp = g_GutterThreshold;

			iY -= 30;
			g_SampleUI.AddStatic( IDC_GUTTER_THRESHOLD_TEXT, L"S", 10 + 200 + 15, iY, 200, 30 );
			g_SampleUI.AddSlider( IDC_GUTTER_THRESHOLD, 10, iY, 200, 24,
																		(int)( g_GutterThreshold_min * SCALE_GutterThreshold ),
																		(int)( g_GutterThreshold_max * SCALE_GutterThreshold ),	// 0.0 ～ 4.0
																		0, false );

			g_GutterThreshold = temp;
			g_SampleUI.GetSlider( IDC_GUTTER_THRESHOLD )->SetValue( (int)( g_GutterThreshold * SCALE_GutterThreshold ) );	// 初期値
		}

		// g_AngleThreshold
		{
			float temp = g_AngleThreshold;

			iY -= 30;
			g_SampleUI.AddStatic( IDC_ANGLE_THRESHOLD_TEXT, L"S", 10 + 200 + 15, iY, 200, 30 );
			g_SampleUI.AddSlider( IDC_ANGLE_THRESHOLD, 10, iY, 200, 24,
																		(int)( g_AngleThreshold_min * SCALE_AngleThreshold ),
																		(int)( g_AngleThreshold_max * SCALE_AngleThreshold ),	// 0.1 ～ 45.0
																		0, false );

			g_AngleThreshold = temp;
			g_SampleUI.GetSlider( IDC_ANGLE_THRESHOLD )->SetValue( (int)( g_AngleThreshold * SCALE_AngleThreshold ) );	// 初期値
		}

		// g_ZThreshold
		{
			float temp = g_ZThreshold;

			iY -= 30;
			g_SampleUI.AddStatic( IDC_Z_THRESHOLD_TEXT, L"S", 10 + 200 + 15, iY, 200, 30 );
			g_SampleUI.AddSlider( IDC_Z_THRESHOLD, 10, iY, 200, 24,
																	(int)( g_ZThreshold_min * SCALE_ZThreshold ),
																	(int)( g_ZThreshold_max * SCALE_ZThreshold ),	// 0.001 ～ 0.5
																	0, false );

			g_ZThreshold = temp;
			g_SampleUI.GetSlider( IDC_Z_THRESHOLD )->SetValue( (int)( g_ZThreshold * SCALE_ZThreshold ) );	// 初期値
		}

		// 輪郭表示
		{
			iY -= 30;
			g_SampleUI.AddComboBox( IDC_DISPCONTOUR, 10, iY, 230, 24, L'S' );
			g_SampleUI.GetComboBox( IDC_DISPCONTOUR )->AddItem( L"None", ( void* )0 );
			g_SampleUI.GetComboBox( IDC_DISPCONTOUR )->AddItem( L"Disp contour", ( void* )1 );
			g_SampleUI.GetComboBox( IDC_DISPCONTOUR )->AddItem( L"Disp contour 2", ( void* )2 );

			g_SampleUI.GetComboBox( IDC_DISPCONTOUR )->SetSelectedByIndex( g_dispContour );	// 初期値
		}
	}


	return true;
}


//--------------------------------------------------------------------------------------
// Called during device initialization, this code checks the device for some
// minimum set of capabilities, and rejects those that don't pass by returning false.
//--------------------------------------------------------------------------------------
bool CALLBACK IsDeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat,
								  D3DFORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
	// Skip backbuffer formats that don't support alpha blending
	IDirect3D9* pD3D = DXUTGetD3D9Object();
	if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
										 AdapterFormat, D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
										 D3DRTYPE_TEXTURE, BackBufferFormat ) ) )
		return false;

	// No fallback defined by this app, so reject any device that
	// doesn't support at least ps2.0
	if( pCaps->PixelShaderVersion < D3DPS_VERSION( 2, 0 ) )
		return false;

	return true;
}


//--------------------------------------------------------------------------------------
// This callback function is called immediately before a device is created to allow the
// application to modify the device settings. The supplied pDeviceSettings parameter
// contains the settings that the framework has selected for the new device, and the
// application can make any desired changes directly to this structure.  Note however that
// DXUT will not correct invalid device settings so care must be taken
// to return valid device settings, otherwise IDirect3D9::CreateDevice() will fail.
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
	assert( DXUT_D3D9_DEVICE == pDeviceSettings->ver );

	HRESULT hr;
	IDirect3D9* pD3D = DXUTGetD3D9Object();
	D3DCAPS9 caps;

	V( pD3D->GetDeviceCaps( pDeviceSettings->d3d9.AdapterOrdinal,
							pDeviceSettings->d3d9.DeviceType,
							&caps ) );

	// Turn vsync off
	pDeviceSettings->d3d9.pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
	g_SettingsDlg.GetDialogControl()->GetComboBox( DXUTSETTINGSDLG_PRESENT_INTERVAL )->SetEnabled( false );

	// If device doesn't support HW T&L or doesn't support 1.1 vertex shaders in HW
	// then switch to SWVP.
	if( ( caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ) == 0 ||
		caps.VertexShaderVersion < D3DVS_VERSION( 1, 1 ) )
	{
		pDeviceSettings->d3d9.BehaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
	}

	// If the hardware cannot do vertex blending, use software vertex processing.
	if( caps.MaxVertexBlendMatrices < 2 )
		pDeviceSettings->d3d9.BehaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;

	// If using hardware vertex processing, change to mixed vertex processing
	// so there is a fallback.
	if( pDeviceSettings->d3d9.BehaviorFlags & D3DCREATE_HARDWARE_VERTEXPROCESSING )
		pDeviceSettings->d3d9.BehaviorFlags = D3DCREATE_MIXED_VERTEXPROCESSING;

	// Debugging vertex shaders requires either REF or software vertex processing
	// and debugging pixel shaders requires REF.
#ifdef DEBUG_VS
	if( pDeviceSettings->d3d9.DeviceType != D3DDEVTYPE_REF )
	{
		pDeviceSettings->d3d9.BehaviorFlags &= ~D3DCREATE_HARDWARE_VERTEXPROCESSING;
		pDeviceSettings->d3d9.BehaviorFlags &= ~D3DCREATE_PUREDEVICE;
		pDeviceSettings->d3d9.BehaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
	}
#endif
#ifdef DEBUG_PS
	pDeviceSettings->d3d9.DeviceType = D3DDEVTYPE_REF;
#endif
	// For the first device created if its a REF device, optionally display a warning dialog box
	static bool s_bFirstTime = true;
	if( s_bFirstTime )
	{
		s_bFirstTime = false;
		if( pDeviceSettings->d3d9.DeviceType == D3DDEVTYPE_REF )
			DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
	}

	return true;
}


//--------------------------------------------------------------------------------------
// Called either by CreateMeshContainer when loading a skin mesh, or when
// changing methods.  This function uses the pSkinInfo of the mesh
// container to generate the desired drawable mesh and bone combination
// table.
//--------------------------------------------------------------------------------------
HRESULT GenerateSkinnedMesh( IDirect3DDevice9* pd3dDevice, D3DXMESHCONTAINER_DERIVED* pMeshContainer )
{
	HRESULT hr = S_OK;
	D3DCAPS9 d3dCaps;
	pd3dDevice->GetDeviceCaps( &d3dCaps );

	if( pMeshContainer->pSkinInfo == NULL )
		return hr;

	g_bUseSoftwareVP = false;

	SAFE_RELEASE( pMeshContainer->MeshData.pMesh );
	SAFE_RELEASE( pMeshContainer->pBoneCombinationBuf );

	// if non-indexed skinning mode selected, use ConvertToBlendedMesh to generate drawable mesh
	if( g_SkinningMethod == D3DNONINDEXED )
	{

		hr = pMeshContainer->pSkinInfo->ConvertToBlendedMesh
			(
			pMeshContainer->pOrigMesh,
			D3DXMESH_MANAGED | D3DXMESHOPT_VERTEXCACHE,
			pMeshContainer->pAdjacency,
			NULL, NULL, NULL,
			&pMeshContainer->NumInfl,
			&pMeshContainer->NumAttributeGroups,
			&pMeshContainer->pBoneCombinationBuf,
			&pMeshContainer->MeshData.pMesh
			);
		if( FAILED( hr ) )
			goto e_Exit;


		// If the device can only do 2 matrix blends, ConvertToBlendedMesh cannot approximate all meshes to it
		// Thus we split the mesh in two parts: The part that uses at most 2 matrices and the rest. The first is
		// drawn using the device's HW vertex processing and the rest is drawn using SW vertex processing.
		LPD3DXBONECOMBINATION rgBoneCombinations = reinterpret_cast<LPD3DXBONECOMBINATION>(
			pMeshContainer->pBoneCombinationBuf->GetBufferPointer() );

		// look for any set of bone combinations that do not fit the caps
		for( pMeshContainer->iAttributeSW = 0; pMeshContainer->iAttributeSW < pMeshContainer->NumAttributeGroups;
			 pMeshContainer->iAttributeSW++ )
		{
			DWORD cInfl = 0;

			for( DWORD iInfl = 0; iInfl < pMeshContainer->NumInfl; iInfl++ )
			{
				if( rgBoneCombinations[pMeshContainer->iAttributeSW].BoneId[iInfl] != UINT_MAX )
				{
					++cInfl;
				}
			}

			if( cInfl > d3dCaps.MaxVertexBlendMatrices )
			{
				break;
			}
		}

		// if there is both HW and SW, add the Software Processing flag
		if( pMeshContainer->iAttributeSW < pMeshContainer->NumAttributeGroups )
		{
			LPD3DXMESH pMeshTmp;

			hr = pMeshContainer->MeshData.pMesh->CloneMeshFVF( D3DXMESH_SOFTWAREPROCESSING |
															   pMeshContainer->MeshData.pMesh->GetOptions(),
															   pMeshContainer->MeshData.pMesh->GetFVF(),
															   pd3dDevice, &pMeshTmp );
			if( FAILED( hr ) )
			{
				goto e_Exit;
			}

			pMeshContainer->MeshData.pMesh->Release();
			pMeshContainer->MeshData.pMesh = pMeshTmp;
			pMeshTmp = NULL;
		}
	}
		// if indexed skinning mode selected, use ConvertToIndexedsBlendedMesh to generate drawable mesh
	else if( g_SkinningMethod == D3DINDEXED )
	{
		DWORD NumMaxFaceInfl;
		DWORD Flags = D3DXMESHOPT_VERTEXCACHE;

		LPDIRECT3DINDEXBUFFER9 pIB;
		hr = pMeshContainer->pOrigMesh->GetIndexBuffer( &pIB );
		if( FAILED( hr ) )
			goto e_Exit;

		hr = pMeshContainer->pSkinInfo->GetMaxFaceInfluences( pIB,
															  pMeshContainer->pOrigMesh->GetNumFaces(),
															  &NumMaxFaceInfl );
		pIB->Release();
		if( FAILED( hr ) )
			goto e_Exit;

		// 12 entry palette guarantees that any triangle (4 independent influences per vertex of a tri)
		// can be handled
		NumMaxFaceInfl = min( NumMaxFaceInfl, 12 );

		if( d3dCaps.MaxVertexBlendMatrixIndex + 1 < NumMaxFaceInfl )
		{
			// HW does not support indexed vertex blending. Use SW instead
			pMeshContainer->NumPaletteEntries = min( 256, pMeshContainer->pSkinInfo->GetNumBones() );
			pMeshContainer->UseSoftwareVP = true;
			g_bUseSoftwareVP = true;
			Flags |= D3DXMESH_SYSTEMMEM;
		}
		else
		{
			// using hardware - determine palette size from caps and number of bones
			// If normals are present in the vertex data that needs to be blended for lighting, then
			// the number of matrices is half the number specified by MaxVertexBlendMatrixIndex.
			pMeshContainer->NumPaletteEntries = min( ( d3dCaps.MaxVertexBlendMatrixIndex + 1 ) / 2,
													 pMeshContainer->pSkinInfo->GetNumBones() );
			pMeshContainer->UseSoftwareVP = false;
			Flags |= D3DXMESH_MANAGED;
		}

		hr = pMeshContainer->pSkinInfo->ConvertToIndexedBlendedMesh
			(
			pMeshContainer->pOrigMesh,
			Flags,
			pMeshContainer->NumPaletteEntries,
			pMeshContainer->pAdjacency,
			NULL, NULL, NULL,
			&pMeshContainer->NumInfl,
			&pMeshContainer->NumAttributeGroups,
			&pMeshContainer->pBoneCombinationBuf,
			&pMeshContainer->MeshData.pMesh );
		if( FAILED( hr ) )
			goto e_Exit;
	}
		// if vertex shader indexed skinning mode selected, use ConvertToIndexedsBlendedMesh to generate drawable mesh
	else if( ( g_SkinningMethod == D3DINDEXEDVS ) || ( g_SkinningMethod == D3DINDEXEDHLSLVS ) )
	{
		// Get palette size
		// First 9 constants are used for other data.  Each 4x3 matrix takes up 3 constants.
		// (96 - 9) /3 i.e. Maximum constant count - used constants
		UINT MaxMatrices = 26;
		pMeshContainer->NumPaletteEntries = min( MaxMatrices, pMeshContainer->pSkinInfo->GetNumBones() );

		DWORD Flags = D3DXMESHOPT_VERTEXCACHE;
		if( d3dCaps.VertexShaderVersion >= D3DVS_VERSION( 1, 1 ) )
		{
			pMeshContainer->UseSoftwareVP = false;
			Flags |= D3DXMESH_MANAGED;
		}
		else
		{
			pMeshContainer->UseSoftwareVP = true;
			g_bUseSoftwareVP = true;
			Flags |= D3DXMESH_SYSTEMMEM;
		}

		SAFE_RELEASE( pMeshContainer->MeshData.pMesh );

		hr = pMeshContainer->pSkinInfo->ConvertToIndexedBlendedMesh
			(
			pMeshContainer->pOrigMesh,
			Flags,
			pMeshContainer->NumPaletteEntries,
			pMeshContainer->pAdjacency,
			NULL, NULL, NULL,
			&pMeshContainer->NumInfl,
			&pMeshContainer->NumAttributeGroups,
			&pMeshContainer->pBoneCombinationBuf,
			&pMeshContainer->MeshData.pMesh );
		if( FAILED( hr ) )
			goto e_Exit;


		// FVF has to match our declarator. Vertex shaders are not as forgiving as FF pipeline
		DWORD OldFVF = pMeshContainer->MeshData.pMesh->GetFVF();
		DWORD NewFVF = ( OldFVF & D3DFVF_POSITION_MASK ) | D3DFVF_NORMAL | D3DFVF_TEX1 | D3DFVF_LASTBETA_UBYTE4;
		if( NewFVF != OldFVF )
		{
			LPD3DXMESH pMesh;
			hr = pMeshContainer->MeshData.pMesh->CloneMeshFVF( pMeshContainer->MeshData.pMesh->GetOptions(), NewFVF,
															   pd3dDevice, &pMesh );
			if( !FAILED( hr ) )
			{
				pMeshContainer->MeshData.pMesh->Release();
				pMeshContainer->MeshData.pMesh = pMesh;
				pMesh = NULL;
			}
		}

		D3DVERTEXELEMENT9 pDecl[MAX_FVF_DECL_SIZE];
//		LPD3DVERTEXELEMENT9 pDeclCur;
		hr = pMeshContainer->MeshData.pMesh->GetDeclaration( pDecl );
		if( FAILED( hr ) )
			goto e_Exit;


#if 0
		printf( L"--- Vertex Declaration ---\n" );
		DXUTTraceDecl( pDecl );
#endif

#if 0
		// GeForce3対策？

		// the vertex shader is expecting to interpret the UBYTE4 as a D3DCOLOR, so update the type
		//	 NOTE: this cannot be done with CloneMesh, that would convert the UBYTE4 data to float and then to D3DCOLOR
		//			this is more of a "cast" operation
		pDeclCur = pDecl;
		while( pDeclCur->Stream != 0xff )
		{
			if( ( pDeclCur->Usage == D3DDECLUSAGE_BLENDINDICES ) && ( pDeclCur->UsageIndex == 0 ) )
				pDeclCur->Type = D3DDECLTYPE_D3DCOLOR;
			pDeclCur++;
		}
#endif

		hr = pMeshContainer->MeshData.pMesh->UpdateSemantics( pDecl );
		if( FAILED( hr ) )
			goto e_Exit;

		// allocate a buffer for bone matrices, but only if another mesh has not allocated one of the same size or larger
		if( g_NumBoneMatricesMax < pMeshContainer->pSkinInfo->GetNumBones() )
		{
			g_NumBoneMatricesMax = pMeshContainer->pSkinInfo->GetNumBones();

			// Allocate space for blend matrices
			delete[] g_pBoneMatrices;
			g_pBoneMatrices = new D3DXMATRIXA16[g_NumBoneMatricesMax];
			if( g_pBoneMatrices == NULL )
			{
				hr = E_OUTOFMEMORY;
				goto e_Exit;
			}
		}

	}
		// if software skinning selected, use GenerateSkinnedMesh to create a mesh that can be used with UpdateSkinnedMesh
	else if( g_SkinningMethod == SOFTWARE )
	{
		hr = pMeshContainer->pOrigMesh->CloneMeshFVF( D3DXMESH_MANAGED, pMeshContainer->pOrigMesh->GetFVF(),
													  pd3dDevice, &pMeshContainer->MeshData.pMesh );
		if( FAILED( hr ) )
			goto e_Exit;

		hr = pMeshContainer->MeshData.pMesh->GetAttributeTable( NULL, &pMeshContainer->NumAttributeGroups );
		if( FAILED( hr ) )
			goto e_Exit;

		delete[] pMeshContainer->pAttributeTable;
		pMeshContainer->pAttributeTable = new D3DXATTRIBUTERANGE[pMeshContainer->NumAttributeGroups];
		if( pMeshContainer->pAttributeTable == NULL )
		{
			hr = E_OUTOFMEMORY;
			goto e_Exit;
		}

		hr = pMeshContainer->MeshData.pMesh->GetAttributeTable( pMeshContainer->pAttributeTable, NULL );
		if( FAILED( hr ) )
			goto e_Exit;

		// allocate a buffer for bone matrices, but only if another mesh has not allocated one of the same size or larger
		if( g_NumBoneMatricesMax < pMeshContainer->pSkinInfo->GetNumBones() )
		{
			g_NumBoneMatricesMax = pMeshContainer->pSkinInfo->GetNumBones();

			// Allocate space for blend matrices
			delete[] g_pBoneMatrices;
			g_pBoneMatrices = new D3DXMATRIXA16[g_NumBoneMatricesMax];
			if( g_pBoneMatrices == NULL )
			{
				hr = E_OUTOFMEMORY;
				goto e_Exit;
			}
		}
	}
	else  // invalid g_SkinningMethod value
	{
		// return failure due to invalid skinning method value
		hr = E_INVALIDARG;
		goto e_Exit;
	}

e_Exit:
	return hr;
}


//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has been
// created, which will happen during application initialization and windowed/full screen
// toggles. This is the best location to create D3DPOOL_MANAGED resources since these
// resources need to be reloaded whenever the device is destroyed. Resources created
// here should be released in the OnDestroyDevice callback.
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnCreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
								 void* pUserContext )
{
	HRESULT hr;

	V_RETURN( g_DialogResourceManager.OnD3D9CreateDevice( pd3dDevice ) );
	V_RETURN( g_SettingsDlg.OnD3D9CreateDevice( pd3dDevice ) );

	// Initialize the font
	V_RETURN( D3DXCreateFont( pd3dDevice, 15, 0, FW_BOLD, 0, FALSE, DEFAULT_CHARSET,
							  OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
							  L"Arial", &g_pFont ) );

	// Define DEBUG_VS and/or DEBUG_PS to debug vertex and/or pixel shaders with the
	// shader debugger. Debugging vertex shaders requires either REF or software vertex
	// processing, and debugging pixel shaders requires REF.  The
	// D3DXSHADER_FORCE_*_SOFTWARE_NOOPT flag improves the debug experience in the
	// shader debugger.  It enables source level debugging, prevents instruction
	// reordering, prevents dead code elimination, and forces the compiler to compile
	// against the next higher available software target, which ensures that the
	// unoptimized shaders do not exceed the shader model limitations.	Setting these
	// flags will cause slower rendering since the shaders will be unoptimized and
	// forced into software.  See the DirectX documentation for more information about
	// using the shader debugger.
	DWORD dwShaderFlags = D3DXFX_NOT_CLONEABLE;

#if defined( DEBUG ) || defined( _DEBUG )
	// Set the D3DXSHADER_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows
	// the shaders to be optimized and to run exactly the way they will run in
	// the release configuration of this program.
	dwShaderFlags |= D3DXSHADER_DEBUG;
	#endif

#ifdef DEBUG_VS
		dwShaderFlags |= D3DXSHADER_FORCE_VS_SOFTWARE_NOOPT;
	#endif
#ifdef DEBUG_PS
		dwShaderFlags |= D3DXSHADER_FORCE_PS_SOFTWARE_NOOPT;
	#endif


//	dwShaderFlags |= D3DXSHADER_IEEE_STRICTNESS;

#ifdef SHADER_USE_PRECOMPILED
	dwShaderFlags = D3DXFX_NOT_CLONEABLE | D3DXSHADER_SKIPVALIDATION;
#endif


	// Read the D3DX effect file
	WCHAR str[MAX_PATH];
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, SHADERFILE ) );


	// If this fails, there should be debug output as to
	// they the .fx file failed to compile
//	V_RETURN( D3DXCreateEffectFromFile( pd3dDevice, str, NULL, NULL, dwShaderFlags,
//										NULL, &g_pEffect, &errBuffer ) );
	{
		CTimeCount tc( L"D3DXCreateEffectFromFile" );

		LPD3DXBUFFER errBuffer;

		hr = D3DXCreateEffectFromFile( pd3dDevice, str, NULL, NULL, dwShaderFlags, NULL, &g_pEffect, &errBuffer );
		if( FAILED(hr) )
		{
			LPVOID pBuf = errBuffer->GetBufferPointer();
			DWORD size = errBuffer->GetBufferSize();

			char *pText = (char *)pBuf;

			return DXUTTrace( __FILE__, (DWORD)__LINE__, hr, L"D3DXCreateEffectFromFile", true );
		}
	}


	// Load the mesh
	{
		WCHAR *pLastSlash = wcsrchr( g_strModelName, L'\\' );
		if( pLastSlash )
		{
			++ pLastSlash;
		}
		else
		{
			pLastSlash = g_strModelName;
		}

		WCHAR strCWD[MAX_PATH];

		GetCurrentDirectory( MAX_PATH, strCWD );
		SetCurrentDirectory( g_strModelPath );

		{
			CTimeCount tc( L"D3DXLoadMeshHierarchyFromX" );

			CAllocateHierarchy Alloc;
			hr = D3DXLoadMeshHierarchyFromX( pLastSlash, D3DXMESH_MANAGED, pd3dDevice, &Alloc, NULL, &g_pFrameRoot, &g_pAnimController );
			if( FAILED( hr ) )
			{
				if( hr == D3DXFERR_FILENOTFOUND )
				{
					LOG_ERR( L"can't open '%S'", GetMultiByte( pLastSlash ) );
					LOG_ERR( L"        in '%S'", GetMultiByte( g_strModelPath ) );
				}
				else
				{
					LOG_ERR( L"error in D3DXLoadMeshHierarchyFromX" );
					LOG_HRESULT( hr );
				}

				return hr;
			}
		}
		{
			CTimeCount tc( L"SetupBoneMatrixPointers" );
			V_RETURN( SetupBoneMatrixPointers( g_pFrameRoot ) );
		}
		{
			CTimeCount tc( L"D3DXFrameCalculateBoundingSphere" );
			V_RETURN( D3DXFrameCalculateBoundingSphere( g_pFrameRoot, &g_vObjectCenter, &g_fObjectRadius ) );
		}

		SetCurrentDirectory( strCWD );

		// 階層構造を表示（デバッグ用）
//		Debug_DispFrame( g_pFrameRoot );

		// カメラフレームを取得
		if( g_pFrameCamera = SearchFrame( g_pFrameRoot, CAMERA_FRAME_NAME ) )
			LOG( 0, L"camera frame found" );
		else
			LOG( 0, L"camera frame NOT found" );


		// ディレクショナルライトのフレームを取得
		{
			char str[ 64 ];

			for( int i = 0;  i < DIR_LIGHT_MAX;  i ++ )
			{
				sprintf_s( str, 64, DIR_LIGHT_FRAME_NAME, i );

				if( g_pFrameDirLight[ i ] = SearchFrame( g_pFrameRoot, str ) )
					LOG( 0, L"dir light %d frame found", i );
				else
					LOG( 0, L"dir light %d frame NOT found", i );
			}
		}
	}

	// Obtain the behavior flags
	D3DDEVICE_CREATION_PARAMETERS cp;
	pd3dDevice->GetCreationParameters( &cp );
	g_dwBehaviorFlags = cp.BehaviorFlags;

	return S_OK;
}


LPD3DXFRAME SearchFrame( LPD3DXFRAME pFrame, char *szName )
{
	while( pFrame )
	{
		if( strcmp( pFrame->Name, szName ) == 0 )
//		if( _tcscmp( pFrame->Name, szName ) == 0 )
			return pFrame;

		if( pFrame->pFrameFirstChild )
		{
			LPD3DXFRAME ret = SearchFrame( pFrame->pFrameFirstChild, szName );
			if( ret )
				return ret;
		}

		pFrame = pFrame->pFrameSibling;
	}

	return NULL;
}


void Debug_DispFrame( LPD3DXFRAME pFrame, int indent )
{
	while( pFrame )
	{
		for( int i = 0;  i < indent;  i ++ )
			g_pLogFile->Print( 0, L"  " );

		LOG( 0, L"%S", pFrame->Name );

		if( pFrame->pFrameFirstChild )
		{
			Debug_DispFrame( pFrame->pFrameFirstChild, indent + 1 );
		}

		pFrame = pFrame->pFrameSibling;
	}
}


//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has been
// reset, which will happen after a lost device scenario. This is the best location to
// create D3DPOOL_DEFAULT resources since these resources need to be reloaded whenever
// the device is lost. Resources created here should be released in the OnLostDevice
// callback.
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnResetDevice( IDirect3DDevice9* pd3dDevice,
								const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
	LOG( 0x404040, L"entered OnResetDevice()" );

	CTimeCount tc( L"OnResetDevice" );

	HRESULT hr;

	V_RETURN( g_DialogResourceManager.OnD3D9ResetDevice() );
	V_RETURN( g_SettingsDlg.OnD3D9ResetDevice() );

	if( g_pFont )
		V_RETURN( g_pFont->OnResetDevice() );
	if( g_pEffect )
		V_RETURN( g_pEffect->OnResetDevice() );

	// Create a sprite to help batch calls when drawing many lines of text
	V_RETURN( D3DXCreateSprite( pd3dDevice, &g_pTextSprite ) );


	// パレットテクスチャ読み込み
	{
		WCHAR strCWD[MAX_PATH];
		GetCurrentDirectory( MAX_PATH, strCWD );
		SetCurrentDirectory( g_strModelPath );

		V( D3DXCreateTextureFromFile( pd3dDevice, PALETTEFILENAME, &g_pPaletteTexture ) );

		SetCurrentDirectory( strCWD );

		if( FAILED( hr ) )
		{
			OutputDebugString( L"Error: g_pPaletteTexture.\n" );
			return hr;
		}

		// テクスチャの横幅を保存
		D3DSURFACE_DESC desc;
		g_pPaletteTexture->GetLevelDesc( 0, &desc );

		paletteTextureWidth = desc.Width;
	}



	typedef struct
	{
		UINT width, height;
		DWORD usage;
		D3DFORMAT format;
		D3DPOOL pool;
		LPDIRECT3DTEXTURE9 *ppTex;
		WCHAR *szName;

	} STexInfo;

	STexInfo texInfo[] =
	{
		{ g_workSurfaceSize[ 0 ], g_workSurfaceSize[ 1 ],	D3DUSAGE_DEPTHSTENCIL,	D3DFMT_D24X8,			D3DPOOL_DEFAULT,	&g_pWorkDSTexture,			L"g_pWorkDSTexture" },
		{ g_workSurfaceSize[ 0 ], g_workSurfaceSize[ 1 ],	D3DUSAGE_RENDERTARGET,	D3DFMT_A8R8G8B8,		D3DPOOL_DEFAULT,	&g_pColorTexture,			L"g_pColorTexture" },
		{ g_workSurfaceSize[ 0 ], g_workSurfaceSize[ 1 ],	D3DUSAGE_RENDERTARGET,	D3DFMT_A8R8G8B8,		D3DPOOL_DEFAULT,	&g_pNormalMateTexture,		L"g_pNormalMateTexture" },
		{ g_workSurfaceSize[ 0 ], g_workSurfaceSize[ 1 ],	D3DUSAGE_RENDERTARGET,	D3DFMT_A32B32G32R32F,	D3DPOOL_DEFAULT,	&g_pDepthTexture,			L"g_pDepthTexture" },
//		{ g_workSurfaceSize[ 0 ], g_workSurfaceSize[ 1 ],	0,						D3DFMT_A8R8G8B8,		D3DPOOL_SYSTEMMEM,	&g_pColorTexture_SysMem,	L"g_pColorTexture_SysMem" },
//		{ g_workSurfaceSize[ 0 ], g_workSurfaceSize[ 1 ],	0,						D3DFMT_A32B32G32R32F,	D3DPOOL_SYSTEMMEM,	&g_pDepthTexture_SysMem,	L"g_pDepthTexture_SysMem" },



//		{ g_splitSurfaceSize[ 0 ], g_splitSurfaceSize[ 1 ],	D3DUSAGE_DYNAMIC,		D3DFMT_A8R8G8B8,		D3DPOOL_DEFAULT,	&g_pAnalyzedTexture,		L"g_pAnalyzedTexture" },
		{ g_splitSurfaceSize[ 0 ], g_splitSurfaceSize[ 1 ],	D3DUSAGE_RENDERTARGET,	D3DFMT_A8R8G8B8,		D3DPOOL_DEFAULT,	&g_pAnalyzedTexture,		L"g_pAnalyzedTexture" },

		{ g_splitSurfaceSize[ 0 ], g_splitSurfaceSize[ 1 ],	D3DUSAGE_RENDERTARGET,	D3DFMT_A8R8G8B8,		D3DPOOL_DEFAULT,	&g_pColorTexture2,			L"g_pColorTexture2" },

//		{ g_splitSurfaceSize[ 0 ], g_splitSurfaceSize[ 1 ],	D3DUSAGE_DYNAMIC,		D3DFMT_A32B32G32R32F,	D3DPOOL_DEFAULT,	&g_pMeanPosTexture,			L"g_pMeanPosTexture" },
		{ g_splitSurfaceSize[ 0 ], g_splitSurfaceSize[ 1 ],	D3DUSAGE_RENDERTARGET,	D3DFMT_A32B32G32R32F,	D3DPOOL_DEFAULT,	&g_pMeanPosTexture,			L"g_pMeanPosTexture" },

		{ g_splitSurfaceSize[ 0 ], g_splitSurfaceSize[ 1 ],	D3DUSAGE_RENDERTARGET,	D3DFMT_A8R8G8B8,		D3DPOOL_DEFAULT,	&g_pAnalyzedTexture2,		L"g_pAnalyzedTexture2" },
		{ g_splitSurfaceSize[ 0 ], g_splitSurfaceSize[ 1 ],	D3DUSAGE_RENDERTARGET,	D3DFMT_A8R8G8B8,		D3DPOOL_DEFAULT,	&g_pSplitTexture,			L"g_pSplitTexture" },



		{ g_finalSurfaceSize[ 0 ], g_finalSurfaceSize[ 1 ],	D3DUSAGE_RENDERTARGET,	D3DFMT_A8R8G8B8,		D3DPOOL_DEFAULT,	&g_pCombinedTexture,		L"g_pCombinedTexture" },
		{ g_finalSurfaceSize[ 0 ], g_finalSurfaceSize[ 1 ],	D3DUSAGE_RENDERTARGET,	D3DFMT_A8R8G8B8,		D3DPOOL_DEFAULT,	&g_pFinalTexture,			L"g_pFinalTexture" },

		{ g_finalSurfaceSize[ 0 ], g_finalSurfaceSize[ 1 ],	0,						D3DFMT_A8R8G8B8,		D3DPOOL_SYSTEMMEM,	&g_pFinalTexture_SysMem,	L"g_pFinalTexture_SysMem" },
	};

	for( int i = 0;  i < sizeof( texInfo ) / sizeof( STexInfo );  i ++ )
	{
		STexInfo *p = &texInfo[ i ];

		V( pd3dDevice->CreateTexture( p->width, p->height, 1, p->usage, p->format, p->pool, p->ppTex, NULL ) );

		if( FAILED( hr ) )
		{
			LOG_ERR( L"Error: CreateTexture %s", p->szName );
			return hr;
		}
	}


	// 描画完了のタイミングを調べるためのクエリを作成
	pd3dDevice->CreateQuery( D3DQUERYTYPE_EVENT, &g_pD3dQuery );



	// Setup render state
	pd3dDevice->SetRenderState( D3DRS_LIGHTING, TRUE );
	pd3dDevice->SetRenderState( D3DRS_DITHERENABLE, FALSE );
	pd3dDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_TRUE );
//	pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESS );
	pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
	pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
	pd3dDevice->SetRenderState( D3DRS_AMBIENT, 0x33333333 );
	pd3dDevice->SetRenderState( D3DRS_NORMALIZENORMALS, TRUE );
	pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
	pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );


/*
	// load the indexed vertex shaders
	DWORD dwShaderFlags = 0;

#if defined( DEBUG ) || defined( _DEBUG )
	// Set the D3DXSHADER_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows
	// the shaders to be optimized and to run exactly the way they will run in
	// the release configuration of this program.
	dwShaderFlags |= D3DXSHADER_DEBUG;
	#endif

#if defined(DEBUG_VS) || defined(DEBUG_PS)
		dwShaderFlags |= D3DXSHADER_DEBUG|D3DXSHADER_SKIPVALIDATION;
	#endif
	for( DWORD iInfl = 0; iInfl < 4; ++iInfl )
	{
		LPD3DXBUFFER pCode;

		// Assemble the vertex shader file
		WCHAR str[MAX_PATH];
		DXUTFindDXSDKMediaFileCch( str, MAX_PATH, g_wszShaderSource[iInfl] );
		if( FAILED( hr = D3DXAssembleShaderFromFile( str, NULL, NULL, dwShaderFlags, &pCode, NULL ) ) )
			return hr;

		// Create the vertex shader
		if( FAILED( hr = pd3dDevice->CreateVertexShader( ( DWORD* )pCode->GetBufferPointer(),
														 &g_pIndexedVertexShader[iInfl] ) ) )
		{
			return hr;
		}

		pCode->Release();
	}
*/

	// Setup the projection matrix
	{
		float fAspect = ( float )g_finalSurfaceSize[ 0 ] / g_finalSurfaceSize[ 1 ];	// RT のアスペクト

		float nearZ = g_fObjectRadius / 64.0f;
//		float farZ = g_fObjectRadius * 200.0f
		float farZ = g_fObjectRadius * 10.0f;


		if( 0 < g_config.GetConfig()->camera_z_near )
			nearZ = g_config.GetConfig()->camera_z_near;

		if( 0 < g_config.GetConfig()->camera_z_far )
			farZ = g_config.GetConfig()->camera_z_far;


		if( 0 < g_config.GetConfig()->camera_fov_y )
		{
			// Perspective

			float fovY_WithoutMargin = D3DX_PI * 2.0f * g_config.GetConfig()->camera_fov_y / 360.0f;	// マージン抜きでの視野角

			float focalLength = ( g_finalSurfaceSize[ 1 ] / 2.0f ) / tanf( fovY_WithoutMargin / 2 );

			float fovY = atanf( ( ( g_finalSurfaceSize[ 1 ] / 2.0f ) + SPLIT_MARGIN ) / focalLength ) * 2;	// マージン込みでの視野角

			D3DXMatrixPerspectiveFovLH( &g_matProj, fovY, fAspect, nearZ, farZ );
		}
		else
		{
			// Ortho

			float h = g_config.GetConfig()->camera_ortho_height;
			float w = h * fAspect;

			float margined_w = w * ( g_finalSurfaceSize[ 0 ] + SPLIT_MARGIN_TOTAL ) / g_finalSurfaceSize[ 0 ];
			float margined_h = h * ( g_finalSurfaceSize[ 1 ] + SPLIT_MARGIN_TOTAL ) / g_finalSurfaceSize[ 1 ];

			D3DXMatrixOrthoLH( &g_matProj, margined_w, margined_h, nearZ, farZ );
		}

		pd3dDevice->SetTransform( D3DTS_PROJECTION, &g_matProj );
//		D3DXMatrixTranspose( &g_matProjT, &g_matProj );
	}

	// Setup the arcball parameters
	g_ArcBall.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height, 0.85f );
	g_ArcBall.SetTranslationRadius( g_fObjectRadius );

/*
	// 初期位置設定
	{
		D3DXMATRIX *pMat = (D3DXMATRIX *)( g_ArcBall.GetTranslationMatrix() );

		pMat->m[ 3 ][0] = 0.0084524062f;
		pMat->m[ 3 ][1] = 0.58603340f;
		pMat->m[ 3 ][2] = 7.6071658f;
	}
*/


//	g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
//	g_HUD.SetSize( 170, 170 );
//	g_SampleUI.SetLocation( 3, 45 );
//	g_SampleUI.SetSize( 240, 70 );
	g_SampleUI.SetLocation( 0, 0 );
	g_SampleUI.SetSize( 640, 480 );


//	g_SampleUI.GetControl( IDC_ENABLEIME )->SetLocation( 130, pBackBufferSurfaceDesc->Height - 80 );


	return S_OK;
}


//--------------------------------------------------------------------------------------
// This callback function will be called once at the beginning of every frame. This is the
// best location for your application to handle updates to the scene, but is not
// intended to contain actual rendering calls, which should instead be placed in the
// OnFrameRender callback.
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
	g_frameNum ++;


	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();

	// Setup world matrix
	// model行列（world座標系）
	D3DXMATRIXA16 matWorld;
#if 0
	D3DXMatrixTranslation( &matWorld, -g_vObjectCenter.x,
						   -g_vObjectCenter.y,
						   -g_vObjectCenter.z );
	D3DXMatrixMultiply( &matWorld, &matWorld, g_ArcBall.GetRotationMatrix() );
	D3DXMatrixMultiply( &matWorld, &matWorld, g_ArcBall.GetTranslationMatrix() );
	pd3dDevice->SetTransform( D3DTS_WORLD, &matWorld );
#else

/*
	static bool b=true;
	if(b)
	{
		b=false;
		LOG( 0, L"center x:%f  y:%f  z:%f", g_vObjectCenter.x, g_vObjectCenter.y, g_vObjectCenter.z );
	}
*/


	D3DXMatrixIdentity( &matWorld );
	pd3dDevice->SetTransform( D3DTS_WORLD, &matWorld );
#endif



/*
	// debug
	{
		D3DXMATRIX *pMat = (D3DXMATRIX *)( g_ArcBall.GetTranslationMatrix() );
		D3DXQUATERNION quat = g_ArcBall.GetQuatNow();

		g_ArcBall.SetQuatNow( quat );
	}
*/

/*
	D3DXVECTOR3 vEye( 0, 0, -2 * g_fObjectRadius );
	D3DXVECTOR3 vAt( 0, 0, 0 );
	D3DXVECTOR3 vUp( 0, 1, 0 );
	D3DXMatrixLookAtLH( &g_matView, &vEye, &vAt, &vUp );
*/
#if 0
	D3DXMatrixIdentity( &g_matView );
	pd3dDevice->SetTransform( D3DTS_VIEW, &g_matView );
#elif 0

	D3DXMatrixIdentity( &g_matView );
//	D3DXMatrixMultiply( &g_matView, &g_matView, g_ArcBall.GetTranslationMatrix() );
//	D3DXMatrixMultiply( &g_matView, &g_matView, g_ArcBall.GetRotationMatrix() );

	// trans
	{
		const D3DXMATRIX *pTrans = g_ArcBall.GetTranslationMatrix();

		D3DXMATRIX invTrans;
		D3DXMatrixTranslation( &invTrans, - pTrans->m[ 3 ][ 0 ], - pTrans->m[ 3 ][ 1 ], - pTrans->m[ 3 ][ 2 ] );

		D3DXMatrixMultiply( &g_matView, &g_matView, &invTrans );
	}

	// rot
	{
		const D3DXMATRIX *pRot = g_ArcBall.GetRotationMatrix();

		D3DXMATRIX invRot;
		D3DXMatrixTranspose( &invRot, pRot );

		D3DXMatrixMultiply( &g_matView, &g_matView, &invRot );
	}
#elif 1

	SConfig *pConfig = g_config.GetConfig();

	D3DXVECTOR3 vec( pConfig->camera_pos[ 0 ] - pConfig->camera_lookat[ 0 ], pConfig->camera_pos[ 1 ] - pConfig->camera_lookat[ 1 ], pConfig->camera_pos[ 2 ] - pConfig->camera_lookat[ 2 ] );

	float targetToCameraLen = D3DXVec3Length( &vec );

	D3DXMATRIX mat;

	D3DXMatrixIdentity( &mat );
	mat.m[ 3 ][ 2 ] = - targetToCameraLen;

	D3DXMatrixMultiply( &mat, &mat, g_ArcBall.GetRotationMatrix() );
	D3DXMatrixMultiply( &mat, &mat, g_ArcBall.GetTranslationMatrix() );

	D3DXMatrixInverse( &g_matView, NULL, &mat );

#else

	D3DXMatrixIdentity( &g_matView );

	g_matView.m[ 3 ][ 1 ] = -0.58f;
	g_matView.m[ 3 ][ 2 ] = 7.6f;

#endif







	if( g_pAnimController != NULL )
	{
		// アニメーションあり

/*
		{
			ID3DXAnimationSet *pAnimSet;

			g_pAnimController->GetAnimationSet( 0, &pAnimSet );

			ID3DXKeyframedAnimationSet *pKeyAnimSet = (ID3DXKeyframedAnimationSet *)pAnimSet;

			float period = (float)pAnimSet->GetPeriod();	// 8.3

			UINT numAnimations = pAnimSet->GetNumAnimations();	// 7


			double TPS = pKeyAnimSet->GetSourceTicksPerSecond();	// 30.0
		}
*/

		if( g_frameNum == 1 )
		{
			// 最初のフレームで情報取得

			ID3DXKeyframedAnimationSet *pKeyAnimSet;

			g_pAnimController->GetAnimationSet( 0, (ID3DXAnimationSet **)( &pKeyAnimSet ) );

			g_period = pKeyAnimSet->GetPeriod();	// 8.3
			g_fps = pKeyAnimSet->GetSourceTicksPerSecond();	// 30.0

			LOG( 0, L"period : %f", g_period );
			LOG( 0, L"fps : %f", g_fps );
			LOG( 0, L"NumAnimations : %d", pKeyAnimSet->GetNumAnimations() );

			g_frameMax = (int)( g_period * g_fps + 0.5 );

			g_currentFrame = 0;
		}
		else
		{
			g_currentFrame ++;

			// 最終フレームに到達した or 指定フレームのみ再生する状態
			if( g_frameMax < g_currentFrame  ||  0 <= g_config.GetConfig()->anime_set_frame )
			{
				g_bDebugCapture = false;

				if( g_bCapture )
				{
					g_currentFrame --;

					g_bCapture = false;
//					DXUTShutdown();
					SendMessage( DXUTGetHWND(), WM_CLOSE, 0, 0 );
				}
				else
				{
					g_currentFrame = 0;	// ループ
				}
			}
		}


		// フレーム数の指定がある場合
		if( 0 <= g_config.GetConfig()->anime_set_frame )
		{
			g_currentFrame = g_config.GetConfig()->anime_set_frame;
		}


//		g_pAnimController->AdvanceTime( fElapsedTime, NULL );

		g_pAnimController->SetTrackPosition( 0, g_currentFrame / g_fps );
		g_pAnimController->AdvanceTime( 0, NULL );

/*
		if( g_frameMax < g_currentFrame )
//		if( g_frameMax < g_currentFrame || 1)
		{
			// end

			g_bDebugCapture = false;

			if( g_bCapture )
			{
				g_bCapture = false;
//				DXUTShutdown();
				SendMessage( DXUTGetHWND(), WM_CLOSE, 0, 0 );
				return;
			}
		}
		else
		{
			g_currentFrame ++;
		}
*/
	}
	else
	{
		// アニメーションなし

		if( g_frameNum == 1 )
		{
			// 最初のフレーム

			g_currentFrame = -1;
		}

		g_currentFrame ++;
		if( 1 <= g_currentFrame )
		{
			// end

			g_bDebugCapture = false;

//if(0)
			if( g_bCapture )
			{
				g_bCapture = false;

#if 1
				SendMessage( DXUTGetHWND(), WM_CLOSE, 0, 0 );
				return;
#endif
			}
		}
	}








#if 1
	UpdateFrameMatrices( g_pFrameRoot, &matWorld );
#else
	D3DXMATRIX matWorldView;

	D3DXMatrixMultiply( &matWorldView, &matWorld, &g_matView );

	UpdateFrameMatrices( g_pFrameRoot, &matWorldView );
#endif



	// カメラフレームが存在するなら、座標を取得する
	if( g_pFrameCamera  &&  ! g_config.GetConfig()->camera_bIgnoreData )
	{
		D3DXFRAME_DERIVED *pFrame = ( D3DXFRAME_DERIVED* )g_pFrameCamera;

//		PRINT_MATRIX( &pFrame->TransformationMatrix );
//		PRINT_MATRIX( &pFrame->CombinedTransformationMatrix );

/*
		D3DXMATRIX mat = pFrame->CombinedTransformationMatrix;

		// swap y & z axis（右手座標系から左手座標系へ変換）
		for( int i = 0;  i < 4;  i ++ )
		{
			float f = mat.m[ 1 ][ i ];
			mat.m[ 1 ][ i ] = mat.m[ 2 ][ i ];
			mat.m[ 2 ][ i ] = f;
		}
*/

		D3DXMATRIX mat;
		GetFrameMatrix( g_pFrameCamera, &mat );


//		D3DXMatrixInverse( &g_matView, NULL, &pFrame->CombinedTransformationMatrix );
		D3DXMatrixInverse( &g_matView, NULL, &mat );

//		PRINT_MATRIX( &g_matView );
	}
}


class CPushRT
{
public:

	CPushRT()
		: m_pD3dDevice( NULL )
		, m_pSurf( NULL )
		, m_pSurfPrev( NULL )
	{
	}

	CPushRT( IDirect3DDevice9 *pd3dDevice, int index, LPDIRECT3DTEXTURE9 pTex )
	{
		Set( pd3dDevice, index, pTex );
	}


	void Set( IDirect3DDevice9 *pd3dDevice, int index, LPDIRECT3DTEXTURE9 pTex )
	{
		HRESULT hr;

		m_pD3dDevice = pd3dDevice;
		m_index = index;

		hr = m_pD3dDevice->GetRenderTarget( m_index, &m_pSurfPrev );

		// レンダーターゲットがなかった
		if( FAILED( hr ) )
		{
			m_pSurfPrev = NULL;
		}

		V( pTex->GetSurfaceLevel( 0, &m_pSurf ) );

		V( m_pD3dDevice->SetRenderTarget( m_index, m_pSurf ) );
	}

	~CPushRT()
	{
		if( m_pD3dDevice )
		{
			HRESULT hr;

			V( m_pD3dDevice->SetRenderTarget( m_index, m_pSurfPrev ) );
			SAFE_RELEASE( m_pSurf );
			SAFE_RELEASE( m_pSurfPrev );
		}
	}


	IDirect3DDevice9 *m_pD3dDevice;

	int m_index;
	LPDIRECT3DSURFACE9 m_pSurf;
	LPDIRECT3DSURFACE9 m_pSurfPrev;
};


void DrawRect(	IDirect3DDevice9 *pd3dDevice,
				D3DXVECTOR4 uvwh, D3DXVECTOR4 xywh )
{
	TLVertex v[ 4 ];

	v[ 0 ].t = D3DXVECTOR2( uvwh.x,				uvwh.y );
	v[ 1 ].t = D3DXVECTOR2( uvwh.x + uvwh.z,	uvwh.y );
	v[ 2 ].t = D3DXVECTOR2( uvwh.x,				uvwh.y + uvwh.w );
	v[ 3 ].t = D3DXVECTOR2( uvwh.x + uvwh.z,	uvwh.y + uvwh.w );

	v[ 0 ].p = D3DXVECTOR4( xywh.x,				xywh.y,				0.0f, 1.0f );
	v[ 1 ].p = D3DXVECTOR4( xywh.x + xywh.z,	xywh.y,				0.0f, 1.0f );
	v[ 2 ].p = D3DXVECTOR4( xywh.x,				xywh.y + xywh.w,	0.0f, 1.0f );
	v[ 3 ].p = D3DXVECTOR4( xywh.x + xywh.z,	xywh.y + xywh.w,	0.0f, 1.0f );

	for( int i = 0;  i < 4;  i ++ )
	{
		v[ i ].p.x -= 0.5f;
		v[ i ].p.y -= 0.5f;
	}


	pd3dDevice->SetFVF( FVF_TLVERTEX );

	pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, v, sizeof( TLVertex ) );
}


void DrawRect(	IDirect3DDevice9 *pd3dDevice,
				LPDIRECT3DTEXTURE9 pTex0, LPDIRECT3DTEXTURE9 pTex1, LPDIRECT3DTEXTURE9 pTex2, LPDIRECT3DTEXTURE9 pTex3,
				D3DXVECTOR4 uvwh, D3DXVECTOR4 xywh, bool bClear = false, D3DCOLOR col = D3DCOLOR_ARGB( 0, 0, 0, 0 ) )
{
	CPushRT pushRT[ 4 ];

	if( pTex0 )	pushRT[ 0 ].Set( pd3dDevice, 0, pTex0 );
	if( pTex1 )	pushRT[ 1 ].Set( pd3dDevice, 1, pTex1 );
	if( pTex2 )	pushRT[ 2 ].Set( pd3dDevice, 2, pTex2 );
	if( pTex3 )	pushRT[ 3 ].Set( pd3dDevice, 3, pTex3 );

	if( bClear )
	{
		// Clear the backbuffer
		pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET, col, 1.0f, 0L );
	}


	{
		HRESULT hr;
		TLVertex v[ 4 ];

		v[ 0 ].t = D3DXVECTOR2( uvwh.x,				uvwh.y );
		v[ 1 ].t = D3DXVECTOR2( uvwh.x + uvwh.z,	uvwh.y );
		v[ 2 ].t = D3DXVECTOR2( uvwh.x,				uvwh.y + uvwh.w );
		v[ 3 ].t = D3DXVECTOR2( uvwh.x + uvwh.z,	uvwh.y + uvwh.w );

		v[ 0 ].p = D3DXVECTOR4( xywh.x,				xywh.y,				0.0f, 1.0f );
		v[ 1 ].p = D3DXVECTOR4( xywh.x + xywh.z,	xywh.y,				0.0f, 1.0f );
		v[ 2 ].p = D3DXVECTOR4( xywh.x,				xywh.y + xywh.w,	0.0f, 1.0f );
		v[ 3 ].p = D3DXVECTOR4( xywh.x + xywh.z,	xywh.y + xywh.w,	0.0f, 1.0f );

		for( int i = 0;  i < 4;  i ++ )
		{
			v[ i ].p.x -= 0.5f;
			v[ i ].p.y -= 0.5f;
		}


		pd3dDevice->SetFVF( FVF_TLVERTEX );

		UINT numPasses;
		V( g_pEffect->Begin( &numPasses, D3DXFX_DONOTSAVESTATE ) );
		for( UINT iPass = 0; iPass < numPasses; iPass++ )
		{
			V( g_pEffect->BeginPass( iPass ) );

			pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, v, sizeof( TLVertex ) );

			V( g_pEffect->EndPass() );
		}

		V( g_pEffect->End() );
	}
}


//--------------------------------------------------------------------------------------
// Called to render a mesh in the hierarchy
//--------------------------------------------------------------------------------------
void DrawMeshContainer( IDirect3DDevice9* pd3dDevice, LPD3DXMESHCONTAINER pMeshContainerBase, LPD3DXFRAME pFrameBase )
{
	HRESULT hr;
	D3DXMESHCONTAINER_DERIVED* pMeshContainer = ( D3DXMESHCONTAINER_DERIVED* )pMeshContainerBase;
	D3DXFRAME_DERIVED* pFrame = ( D3DXFRAME_DERIVED* )pFrameBase;
	UINT iMaterial;
//	UINT NumBlend;
	UINT iAttrib;
//	DWORD AttribIdPrev;
	LPD3DXBONECOMBINATION pBoneComb;

	UINT iMatrixIndex;
	UINT iPaletteEntry;
	D3DXMATRIXA16 matTemp;
	D3DCAPS9 d3dCaps;
	pd3dDevice->GetDeviceCaps( &d3dCaps );





	// first check for skinning
	if( pMeshContainer->pSkinInfo != NULL )
	{
		{
			if( pMeshContainer->UseSoftwareVP )
			{
				// If hw or pure hw vertex processing is forced, we can't render the
				// mesh, so just exit out.	Typical applications should create
				// a device with appropriate vertex processing capability for this
				// skinning method.
				if( g_dwBehaviorFlags & D3DCREATE_HARDWARE_VERTEXPROCESSING )
					return;

				V( pd3dDevice->SetSoftwareVertexProcessing( TRUE ) );
			}

			pBoneComb = reinterpret_cast<LPD3DXBONECOMBINATION>( pMeshContainer->pBoneCombinationBuf->GetBufferPointer() );
			for( iAttrib = 0; iAttrib < pMeshContainer->NumAttributeGroups; iAttrib++ )
			{
				// first calculate all the world matrices
				for( iPaletteEntry = 0; iPaletteEntry < pMeshContainer->NumPaletteEntries; ++iPaletteEntry )
				{
					iMatrixIndex = pBoneComb[iAttrib].BoneId[iPaletteEntry];
					if( iMatrixIndex != UINT_MAX )
					{
#if 1	// 2014/10/03
						D3DXMatrixMultiply( &matTemp, &pMeshContainer->pBoneOffsetMatrices[iMatrixIndex],
											pMeshContainer->ppBoneMatrixPtrs[iMatrixIndex] );
						D3DXMatrixMultiply( &g_pBoneMatrices[iPaletteEntry], &matTemp, &g_matView );
#else
						D3DXMatrixMultiply( &g_pBoneMatrices[iPaletteEntry], &pMeshContainer->pBoneOffsetMatrices[iMatrixIndex],
											pMeshContainer->ppBoneMatrixPtrs[iMatrixIndex] );
#endif
					}
				}
				V( g_pEffect->SetMatrixArray( "mWorldViewMatrixArray", g_pBoneMatrices,
											  pMeshContainer->NumPaletteEntries ) );
/*
				// Sum of all ambient and emissive contribution
				D3DXCOLOR color1( pMeshContainer->pMaterials[pBoneComb[iAttrib].AttribId].MatD3D.Ambient );
				D3DXCOLOR color2( .25, .25, .25, 1.0 );
				D3DXCOLOR ambEmm;
				D3DXColorModulate( &ambEmm, &color1, &color2 );
				ambEmm += D3DXCOLOR( pMeshContainer->pMaterials[pBoneComb[iAttrib].AttribId].MatD3D.Emissive );
*/
//				D3DXCOLOR ambEmm( 0, 0, 0, 1.0 );
//				D3DXCOLOR matDiff( 1, 1, 1, 1.0 );

				// set material color properties
//				V( g_pEffect->SetVector( "MaterialDiffuse", ( D3DXVECTOR4* )&( pMeshContainer->pMaterials[pBoneComb[iAttrib].AttribId].MatD3D.Diffuse ) ) );
//				V( g_pEffect->SetVector( "MaterialDiffuse", ( D3DXVECTOR4* )&( matDiff ) ) );
//				V( g_pEffect->SetVector( "MaterialAmbient", ( D3DXVECTOR4* )&ambEmm ) );

				// スペキュラ係数・指数
				V( g_pEffect->SetFloat( "SpecularCoefficient", pMeshContainer->pMaterials[pBoneComb[iAttrib].AttribId].MatD3D.Specular.r ) );
				V( g_pEffect->SetFloat( "SpecularExponent", pMeshContainer->pMaterials[pBoneComb[iAttrib].AttribId].MatD3D.Specular.g ) );

				// パレット番号
				V( g_pEffect->SetFloat( "PaletteU", pMeshContainer->pMaterials[pBoneComb[iAttrib].AttribId].MatD3D.Power / 255 ) );

				// setup the material of the mesh subset - REMEMBER to use the original pre-skinning attribute id to get the correct material id
//				V( pd3dDevice->SetTexture( 0, pMeshContainer->ppTextures[pBoneComb[iAttrib].AttribId] ) );
				V( g_pEffect->SetTexture( "g_Texture", pMeshContainer->ppTextures[pBoneComb[iAttrib].AttribId] ) );

				// テクスチャの有無でシェーダ切り替え
				{
					int PSMode = 0;

					if( pMeshContainer->ppTextures[pBoneComb[iAttrib].AttribId] != NULL )
					{
						if( pMeshContainer->pMaterials[pBoneComb[iAttrib].AttribId].MatD3D.Specular.b == 0.0f )
						{
							PSMode = 3;
						}
						else
						{
							PSMode = 2;
						}
					}

					V( g_pEffect->SetInt( "PSMode", PSMode ) );
				}

				// Set CurNumBones to select the correct vertex shader for the number of bones
				V( g_pEffect->SetInt( "CurNumBones", pMeshContainer->NumInfl - 1 ) );

				// Start the effect now all parameters have been updated
				UINT numPasses;
				V( g_pEffect->Begin( &numPasses, D3DXFX_DONOTSAVESTATE ) );
				for( UINT iPass = 0; iPass < numPasses; iPass++ )
				{
					V( g_pEffect->BeginPass( iPass ) );

					// draw the subset with the current world matrix palette and material state
					V( pMeshContainer->MeshData.pMesh->DrawSubset( iAttrib ) );

					V( g_pEffect->EndPass() );
				}

				V( g_pEffect->End() );

				V( pd3dDevice->SetVertexShader( NULL ) );
			}

			// remember to reset back to hw vertex processing if software was required
			if( pMeshContainer->UseSoftwareVP )
			{
				V( pd3dDevice->SetSoftwareVertexProcessing( FALSE ) );
			}
		}
	}
	else
	{
#if 0
		V( g_pEffect->SetMatrixArray( "mWorldViewMatrixArray", &pFrame->CombinedTransformationMatrix, 1 ) );
#else
		D3DXMatrixMultiply( &matTemp, &pFrame->CombinedTransformationMatrix, &g_matView );

		V( g_pEffect->SetMatrixArray( "mWorldViewMatrixArray", &matTemp, 1 ) );
#endif

		for( iMaterial = 0; iMaterial < pMeshContainer->NumMaterials; iMaterial++ )
		{
/*
			// Sum of all ambient and emissive contribution
			D3DXCOLOR color1( pMeshContainer->pMaterials[iMaterial].MatD3D.Ambient );
			D3DXCOLOR color2( .25, .25, .25, 1.0 );
			D3DXCOLOR ambEmm;
			D3DXColorModulate( &ambEmm, &color1, &color2 );
			ambEmm += D3DXCOLOR( pMeshContainer->pMaterials[iMaterial].MatD3D.Emissive );
*/
			D3DXCOLOR ambEmm( 0, 0, 0, 1.0 );
			D3DXCOLOR matDiff( 1, 1, 1, 1.0 );

			// set material color properties
//			V( g_pEffect->SetVector( "MaterialDiffuse", ( D3DXVECTOR4* )&( pMeshContainer->pMaterials[iMaterial].MatD3D.Diffuse ) ) );
//			V( g_pEffect->SetVector( "MaterialDiffuse", ( D3DXVECTOR4* )&( matDiff ) ) );
//			V( g_pEffect->SetVector( "MaterialAmbient", ( D3DXVECTOR4* )&ambEmm ) );

			// スペキュラ係数・指数
			V( g_pEffect->SetFloat( "SpecularCoefficient", pMeshContainer->pMaterials[iMaterial].MatD3D.Specular.r ) );
			V( g_pEffect->SetFloat( "SpecularExponent", pMeshContainer->pMaterials[iMaterial].MatD3D.Specular.g ) );

			// パレット番号
			V( g_pEffect->SetFloat( "PaletteU", pMeshContainer->pMaterials[iMaterial].MatD3D.Power / 255 ) );

			// setup the material of the mesh subset - REMEMBER to use the original pre-skinning attribute id to get the correct material id
//			V( pd3dDevice->SetTexture( 0, pMeshContainer->ppTextures[pBoneComb[iAttrib].AttribId] ) );
			V( g_pEffect->SetTexture( "g_Texture", pMeshContainer->ppTextures[iMaterial] ) );

			// テクスチャの有無でシェーダ切り替え
			{
				int PSMode = 0;

				if( pMeshContainer->ppTextures[iMaterial] != NULL )
				{
					if( pMeshContainer->pMaterials[iMaterial].MatD3D.Specular.b == 0.0f )
					{
						PSMode = 3;
					}
					else
					{
						PSMode = 2;
					}
				}

				V( g_pEffect->SetInt( "PSMode", PSMode ) );
			}

			// Set CurNumBones to select the correct vertex shader for the number of bones
			V( g_pEffect->SetInt( "CurNumBones", 0 ) );


			// Start the effect now all parameters have been updated
			UINT numPasses;
			V( g_pEffect->Begin( &numPasses, D3DXFX_DONOTSAVESTATE ) );
			for( UINT iPass = 0; iPass < numPasses; iPass++ )
			{
				V( g_pEffect->BeginPass( iPass ) );

				// draw the subset with the current world matrix palette and material state
				V( pMeshContainer->MeshData.pMesh->DrawSubset( iMaterial ) );

				V( g_pEffect->EndPass() );
			}

			V( g_pEffect->End() );

			V( pd3dDevice->SetVertexShader( NULL ) );
		}


/*
		V( pd3dDevice->SetTransform( D3DTS_WORLD, &pFrame->CombinedTransformationMatrix ) );

		for( iMaterial = 0; iMaterial < pMeshContainer->NumMaterials; iMaterial++ )
		{
			V( pd3dDevice->SetMaterial( &pMeshContainer->pMaterials[iMaterial].MatD3D ) );
			V( pd3dDevice->SetTexture( 0, pMeshContainer->ppTextures[iMaterial] ) );
			V( pMeshContainer->MeshData.pMesh->DrawSubset( iMaterial ) );
		}
*/
	}


	return;

















#if 0
	// first check for skinning
	if( pMeshContainer->pSkinInfo != NULL )
	{
		if( g_SkinningMethod == D3DNONINDEXED )
		{
			AttribIdPrev = UNUSED32;
			pBoneComb = reinterpret_cast<LPD3DXBONECOMBINATION>( pMeshContainer->pBoneCombinationBuf->GetBufferPointer
																 () );

			// Draw using default vtx processing of the device (typically HW)
			for( iAttrib = 0; iAttrib < pMeshContainer->NumAttributeGroups; iAttrib++ )
			{
				NumBlend = 0;
				for( DWORD i = 0; i < pMeshContainer->NumInfl; ++i )
				{
					if( pBoneComb[iAttrib].BoneId[i] != UINT_MAX )
					{
						NumBlend = i;
					}
				}

				if( d3dCaps.MaxVertexBlendMatrices >= NumBlend + 1 )
				{
					// first calculate the world matrices for the current set of blend weights and get the accurate count of the number of blends
					for( DWORD i = 0; i < pMeshContainer->NumInfl; ++i )
					{
						iMatrixIndex = pBoneComb[iAttrib].BoneId[i];
						if( iMatrixIndex != UINT_MAX )
						{
							D3DXMatrixMultiply( &matTemp, &pMeshContainer->pBoneOffsetMatrices[iMatrixIndex],
												pMeshContainer->ppBoneMatrixPtrs[iMatrixIndex] );
							V( pd3dDevice->SetTransform( D3DTS_WORLDMATRIX( i ), &matTemp ) );
						}
					}

					V( pd3dDevice->SetRenderState( D3DRS_VERTEXBLEND, NumBlend ) );

					// lookup the material used for this subset of faces
					if( ( AttribIdPrev != pBoneComb[iAttrib].AttribId ) || ( AttribIdPrev == UNUSED32 ) )
					{
						V( pd3dDevice->SetMaterial( &pMeshContainer->pMaterials[pBoneComb[iAttrib].AttribId].MatD3D )
						   );
						V( pd3dDevice->SetTexture( 0, pMeshContainer->ppTextures[pBoneComb[iAttrib].AttribId] ) );
						AttribIdPrev = pBoneComb[iAttrib].AttribId;
					}

					// draw the subset now that the correct material and matrices are loaded
					V( pMeshContainer->MeshData.pMesh->DrawSubset( iAttrib ) );
				}
			}

			// If necessary, draw parts that HW could not handle using SW
			if( pMeshContainer->iAttributeSW < pMeshContainer->NumAttributeGroups )
			{
				AttribIdPrev = UNUSED32;
				V( pd3dDevice->SetSoftwareVertexProcessing( TRUE ) );
				for( iAttrib = pMeshContainer->iAttributeSW; iAttrib < pMeshContainer->NumAttributeGroups; iAttrib++ )
				{
					NumBlend = 0;
					for( DWORD i = 0; i < pMeshContainer->NumInfl; ++i )
					{
						if( pBoneComb[iAttrib].BoneId[i] != UINT_MAX )
						{
							NumBlend = i;
						}
					}

					if( d3dCaps.MaxVertexBlendMatrices < NumBlend + 1 )
					{
						// first calculate the world matrices for the current set of blend weights and get the accurate count of the number of blends
						for( DWORD i = 0; i < pMeshContainer->NumInfl; ++i )
						{
							iMatrixIndex = pBoneComb[iAttrib].BoneId[i];
							if( iMatrixIndex != UINT_MAX )
							{
								D3DXMatrixMultiply( &matTemp, &pMeshContainer->pBoneOffsetMatrices[iMatrixIndex],
													pMeshContainer->ppBoneMatrixPtrs[iMatrixIndex] );
								V( pd3dDevice->SetTransform( D3DTS_WORLDMATRIX( i ), &matTemp ) );
							}
						}

						V( pd3dDevice->SetRenderState( D3DRS_VERTEXBLEND, NumBlend ) );

						// lookup the material used for this subset of faces
						if( ( AttribIdPrev != pBoneComb[iAttrib].AttribId ) || ( AttribIdPrev == UNUSED32 ) )
						{
							V( pd3dDevice->SetMaterial( &pMeshContainer->pMaterials[pBoneComb[iAttrib].AttribId].MatD3D
														) );
							V( pd3dDevice->SetTexture( 0, pMeshContainer->ppTextures[pBoneComb[iAttrib].AttribId] ) );
							AttribIdPrev = pBoneComb[iAttrib].AttribId;
						}

						// draw the subset now that the correct material and matrices are loaded
						V( pMeshContainer->MeshData.pMesh->DrawSubset( iAttrib ) );
					}
				}
				V( pd3dDevice->SetSoftwareVertexProcessing( FALSE ) );
			}

			V( pd3dDevice->SetRenderState( D3DRS_VERTEXBLEND, 0 ) );
		}
		else if( g_SkinningMethod == D3DINDEXED )
		{
			// if hw doesn't support indexed vertex processing, switch to software vertex processing
			if( pMeshContainer->UseSoftwareVP )
			{
				// If hw or pure hw vertex processing is forced, we can't render the
				// mesh, so just exit out.	Typical applications should create
				// a device with appropriate vertex processing capability for this
				// skinning method.
				if( g_dwBehaviorFlags & D3DCREATE_HARDWARE_VERTEXPROCESSING )
					return;

				V( pd3dDevice->SetSoftwareVertexProcessing( TRUE ) );
			}

			// set the number of vertex blend indices to be blended
			if( pMeshContainer->NumInfl == 1 )
			{
				V( pd3dDevice->SetRenderState( D3DRS_VERTEXBLEND, D3DVBF_0WEIGHTS ) );
			}
			else
			{
				V( pd3dDevice->SetRenderState( D3DRS_VERTEXBLEND, pMeshContainer->NumInfl - 1 ) );
			}

			if( pMeshContainer->NumInfl )
				V( pd3dDevice->SetRenderState( D3DRS_INDEXEDVERTEXBLENDENABLE, TRUE ) );

			// for each attribute group in the mesh, calculate the set of matrices in the palette and then draw the mesh subset
			pBoneComb = reinterpret_cast<LPD3DXBONECOMBINATION>( pMeshContainer->pBoneCombinationBuf->GetBufferPointer
																 () );
			for( iAttrib = 0; iAttrib < pMeshContainer->NumAttributeGroups; iAttrib++ )
			{
				// first calculate all the world matrices
				for( iPaletteEntry = 0; iPaletteEntry < pMeshContainer->NumPaletteEntries; ++iPaletteEntry )
				{
					iMatrixIndex = pBoneComb[iAttrib].BoneId[iPaletteEntry];
					if( iMatrixIndex != UINT_MAX )
					{
						D3DXMatrixMultiply( &matTemp, &pMeshContainer->pBoneOffsetMatrices[iMatrixIndex],
											pMeshContainer->ppBoneMatrixPtrs[iMatrixIndex] );
						V( pd3dDevice->SetTransform( D3DTS_WORLDMATRIX( iPaletteEntry ), &matTemp ) );
					}
				}

				// setup the material of the mesh subset - REMEMBER to use the original pre-skinning attribute id to get the correct material id
				V( pd3dDevice->SetMaterial( &pMeshContainer->pMaterials[pBoneComb[iAttrib].AttribId].MatD3D ) );
				V( pd3dDevice->SetTexture( 0, pMeshContainer->ppTextures[pBoneComb[iAttrib].AttribId] ) );

				// finally draw the subset with the current world matrix palette and material state
				V( pMeshContainer->MeshData.pMesh->DrawSubset( iAttrib ) );
			}

			// reset blending state
			V( pd3dDevice->SetRenderState( D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE ) );
			V( pd3dDevice->SetRenderState( D3DRS_VERTEXBLEND, 0 ) );

			// remember to reset back to hw vertex processing if software was required
			if( pMeshContainer->UseSoftwareVP )
			{
				V( pd3dDevice->SetSoftwareVertexProcessing( FALSE ) );
			}
		}
		else if( g_SkinningMethod == D3DINDEXEDVS )
		{
			// Use COLOR instead of UBYTE4 since Geforce3 does not support it
			// vConst.w should be 3, but due to COLOR/UBYTE4 issue, mul by 255 and add epsilon
			D3DXVECTOR4 vConst( 1.0f, 0.0f, 0.0f, 765.01f );

			if( pMeshContainer->UseSoftwareVP )
			{
				// If hw or pure hw vertex processing is forced, we can't render the
				// mesh, so just exit out.	Typical applications should create
				// a device with appropriate vertex processing capability for this
				// skinning method.
				if( g_dwBehaviorFlags & D3DCREATE_HARDWARE_VERTEXPROCESSING )
					return;

				V( pd3dDevice->SetSoftwareVertexProcessing( TRUE ) );
			}

			V( pd3dDevice->SetVertexShader( g_pIndexedVertexShader[pMeshContainer->NumInfl - 1] ) );

			pBoneComb = reinterpret_cast<LPD3DXBONECOMBINATION>( pMeshContainer->pBoneCombinationBuf->GetBufferPointer
																 () );
			for( iAttrib = 0; iAttrib < pMeshContainer->NumAttributeGroups; iAttrib++ )
			{
				// first calculate all the world matrices
				for( iPaletteEntry = 0; iPaletteEntry < pMeshContainer->NumPaletteEntries; ++iPaletteEntry )
				{
					iMatrixIndex = pBoneComb[iAttrib].BoneId[iPaletteEntry];
					if( iMatrixIndex != UINT_MAX )
					{
						D3DXMatrixMultiply( &matTemp, &pMeshContainer->pBoneOffsetMatrices[iMatrixIndex],
											pMeshContainer->ppBoneMatrixPtrs[iMatrixIndex] );
						D3DXMatrixMultiplyTranspose( &matTemp, &matTemp, &g_matView );
						V( pd3dDevice->SetVertexShaderConstantF( iPaletteEntry * 3 + 9, ( float* )&matTemp, 3 ) );
					}
				}

				// Sum of all ambient and emissive contribution
				D3DXCOLOR color1( pMeshContainer->pMaterials[pBoneComb[iAttrib].AttribId].MatD3D.Ambient );
				D3DXCOLOR color2( .25, .25, .25, 1.0 );
				D3DXCOLOR ambEmm;
				D3DXColorModulate( &ambEmm, &color1, &color2 );
				ambEmm += D3DXCOLOR( pMeshContainer->pMaterials[pBoneComb[iAttrib].AttribId].MatD3D.Emissive );

				// set material color properties
				V( pd3dDevice->SetVertexShaderConstantF( 8,
														 ( float* )&(
														 pMeshContainer->pMaterials[pBoneComb[iAttrib].AttribId].MatD3D.Diffuse ), 1 ) );
				V( pd3dDevice->SetVertexShaderConstantF( 7, ( float* )&ambEmm, 1 ) );
				vConst.y = pMeshContainer->pMaterials[pBoneComb[iAttrib].AttribId].MatD3D.Power;
				V( pd3dDevice->SetVertexShaderConstantF( 0, ( float* )&vConst, 1 ) );

				V( pd3dDevice->SetTexture( 0, pMeshContainer->ppTextures[pBoneComb[iAttrib].AttribId] ) );

				// finally draw the subset with the current world matrix palette and material state
				V( pMeshContainer->MeshData.pMesh->DrawSubset( iAttrib ) );
			}

			// remember to reset back to hw vertex processing if software was required
			if( pMeshContainer->UseSoftwareVP )
			{
				V( pd3dDevice->SetSoftwareVertexProcessing( FALSE ) );
			}
			V( pd3dDevice->SetVertexShader( NULL ) );
		}
		else if( g_SkinningMethod == D3DINDEXEDHLSLVS )
		{
			if( pMeshContainer->UseSoftwareVP )
			{
				// If hw or pure hw vertex processing is forced, we can't render the
				// mesh, so just exit out.	Typical applications should create
				// a device with appropriate vertex processing capability for this
				// skinning method.
				if( g_dwBehaviorFlags & D3DCREATE_HARDWARE_VERTEXPROCESSING )
					return;

				V( pd3dDevice->SetSoftwareVertexProcessing( TRUE ) );
			}

			pBoneComb = reinterpret_cast<LPD3DXBONECOMBINATION>( pMeshContainer->pBoneCombinationBuf->GetBufferPointer() );
			for( iAttrib = 0; iAttrib < pMeshContainer->NumAttributeGroups; iAttrib++ )
			{
				// first calculate all the world matrices
				for( iPaletteEntry = 0; iPaletteEntry < pMeshContainer->NumPaletteEntries; ++iPaletteEntry )
				{
					iMatrixIndex = pBoneComb[iAttrib].BoneId[iPaletteEntry];
					if( iMatrixIndex != UINT_MAX )
					{
						D3DXMatrixMultiply( &matTemp, &pMeshContainer->pBoneOffsetMatrices[iMatrixIndex],
											pMeshContainer->ppBoneMatrixPtrs[iMatrixIndex] );
						D3DXMatrixMultiply( &g_pBoneMatrices[iPaletteEntry], &matTemp, &g_matView );
					}
				}
				V( g_pEffect->SetMatrixArray( "mWorldViewMatrixArray", g_pBoneMatrices,
											  pMeshContainer->NumPaletteEntries ) );

				// Sum of all ambient and emissive contribution
				D3DXCOLOR color1( pMeshContainer->pMaterials[pBoneComb[iAttrib].AttribId].MatD3D.Ambient );
				D3DXCOLOR color2( .25, .25, .25, 1.0 );
				D3DXCOLOR ambEmm;
				D3DXColorModulate( &ambEmm, &color1, &color2 );
				ambEmm += D3DXCOLOR( pMeshContainer->pMaterials[pBoneComb[iAttrib].AttribId].MatD3D.Emissive );

				// set material color properties
				V( g_pEffect->SetVector( "MaterialDiffuse",
										 ( D3DXVECTOR4* )&(
										 pMeshContainer->pMaterials[pBoneComb[iAttrib].AttribId].MatD3D.Diffuse ) ) );
				V( g_pEffect->SetVector( "MaterialAmbient", ( D3DXVECTOR4* )&ambEmm ) );

				// setup the material of the mesh subset - REMEMBER to use the original pre-skinning attribute id to get the correct material id
//				  V( pd3dDevice->SetTexture( 0, pMeshContainer->ppTextures[pBoneComb[iAttrib].AttribId] ) );
				V( g_pEffect->SetTexture( "g_Texture", pMeshContainer->ppTextures[pBoneComb[iAttrib].AttribId] ) );

				// Set CurNumBones to select the correct vertex shader for the number of bones
				V( g_pEffect->SetInt( "CurNumBones", pMeshContainer->NumInfl - 1 ) );

				// Start the effect now all parameters have been updated
				UINT numPasses;
				V( g_pEffect->Begin( &numPasses, D3DXFX_DONOTSAVESTATE ) );
				for( UINT iPass = 0; iPass < numPasses; iPass++ )
				{
					V( g_pEffect->BeginPass( iPass ) );

					// draw the subset with the current world matrix palette and material state
					V( pMeshContainer->MeshData.pMesh->DrawSubset( iAttrib ) );

					V( g_pEffect->EndPass() );
				}

				V( g_pEffect->End() );

				V( pd3dDevice->SetVertexShader( NULL ) );
			}

			// remember to reset back to hw vertex processing if software was required
			if( pMeshContainer->UseSoftwareVP )
			{
				V( pd3dDevice->SetSoftwareVertexProcessing( FALSE ) );
			}
		}
		else if( g_SkinningMethod == SOFTWARE )
		{
			D3DXMATRIX Identity;
			DWORD cBones = pMeshContainer->pSkinInfo->GetNumBones();
			DWORD iBone;
			PBYTE pbVerticesSrc;
			PBYTE pbVerticesDest;

			// set up bone transforms
			for( iBone = 0; iBone < cBones; ++iBone )
			{
				D3DXMatrixMultiply
					(
					&g_pBoneMatrices[iBone],				 // output
					&pMeshContainer->pBoneOffsetMatrices[iBone],
					pMeshContainer->ppBoneMatrixPtrs[iBone]
					);
			}

			// set world transform
			D3DXMatrixIdentity( &Identity );
			V( pd3dDevice->SetTransform( D3DTS_WORLD, &Identity ) );

			V( pMeshContainer->pOrigMesh->LockVertexBuffer( D3DLOCK_READONLY, ( LPVOID* )&pbVerticesSrc ) );
			V( pMeshContainer->MeshData.pMesh->LockVertexBuffer( 0, ( LPVOID* )&pbVerticesDest ) );

			// generate skinned mesh
			pMeshContainer->pSkinInfo->UpdateSkinnedMesh( g_pBoneMatrices, NULL, pbVerticesSrc, pbVerticesDest );

			V( pMeshContainer->pOrigMesh->UnlockVertexBuffer() );
			V( pMeshContainer->MeshData.pMesh->UnlockVertexBuffer() );

			for( iAttrib = 0; iAttrib < pMeshContainer->NumAttributeGroups; iAttrib++ )
			{
				V( pd3dDevice->SetMaterial( &(
											pMeshContainer->pMaterials[pMeshContainer->pAttributeTable[iAttrib].AttribId].MatD3D ) ) );
				V( pd3dDevice->SetTexture( 0,
										   pMeshContainer->ppTextures[pMeshContainer->pAttributeTable[iAttrib].AttribId] ) );
				V( pMeshContainer->MeshData.pMesh->DrawSubset( pMeshContainer->pAttributeTable[iAttrib].AttribId ) );
			}
		}
		else // bug out as unsupported mode
		{
			return;
		}
	}
	else  // standard mesh, just draw it after setting material properties
	{
		V( pd3dDevice->SetTransform( D3DTS_WORLD, &pFrame->CombinedTransformationMatrix ) );

		for( iMaterial = 0; iMaterial < pMeshContainer->NumMaterials; iMaterial++ )
		{
			V( pd3dDevice->SetMaterial( &pMeshContainer->pMaterials[iMaterial].MatD3D ) );
			V( pd3dDevice->SetTexture( 0, pMeshContainer->ppTextures[iMaterial] ) );
			V( pMeshContainer->MeshData.pMesh->DrawSubset( iMaterial ) );
		}
	}
#endif
}




//--------------------------------------------------------------------------------------
// Called to render a frame in the hierarchy
//--------------------------------------------------------------------------------------
void DrawFrame( IDirect3DDevice9* pd3dDevice, LPD3DXFRAME pFrame )
{
	LPD3DXMESHCONTAINER pMeshContainer;

	pMeshContainer = pFrame->pMeshContainer;
	while( pMeshContainer != NULL )
	{
		DrawMeshContainer( pd3dDevice, pMeshContainer, pFrame );

		pMeshContainer = pMeshContainer->pNextMeshContainer;
	}

	if( pFrame->pFrameSibling != NULL )
	{
		DrawFrame( pd3dDevice, pFrame->pFrameSibling );
	}

	if( pFrame->pFrameFirstChild != NULL )
	{
		DrawFrame( pd3dDevice, pFrame->pFrameFirstChild );
	}
}


//--------------------------------------------------------------------------------------
// This callback function will be called at the end of every frame to perform all the
// rendering calls for the scene, and it will also be called if the window needs to be
// repainted. After this function has returned, DXUT will call
// IDirect3DDevice9::Present to display the contents of the next buffer in the swap chain
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
//printf( L"OnFrameRender in\n" );

	CAveragePerformance avgp( L"OnFrameRender()" );


	// If the settings dialog is being shown, then
	// render it instead of rendering the app's scene
	if( g_SettingsDlg.IsActive() )
	{
		g_SettingsDlg.OnRender( fElapsedTime );
		return;
	}

	HRESULT hr;

	LPDIRECT3DSURFACE9 pLDRSurface = NULL;
	LPDIRECT3DSURFACE9 pLDRDSSurface = NULL;

	//Configure the render targets
	V( pd3dDevice->GetRenderTarget( 0, &pLDRSurface ) );	// This is the output surface - a standard 32bit device
	V( pd3dDevice->GetDepthStencilSurface( &pLDRDSSurface ) );	// Depth Stencil


	// Clear the backbuffer
	V( pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB( 0, 66, 75, 121 ), 1.0f, 0L ) );

//printf( L"OnFrameRender Cleared\n" );

/*
	// Setup the light
	D3DLIGHT9 light;
	D3DXVECTOR3 vecLightDirUnnormalized( 0.0f, -1.0f, 1.0f );
	ZeroMemory( &light, sizeof( D3DLIGHT9 ) );
	light.Type = D3DLIGHT_DIRECTIONAL;
	light.Diffuse.r = 1.0f;
	light.Diffuse.g = 0.0f;
	light.Diffuse.b = 0.0f;
	D3DXVec3Normalize( ( D3DXVECTOR3* )&light.Direction, &vecLightDirUnnormalized );
	light.Position.x = 0.0f;
	light.Position.y = -1.0f;
	light.Position.z = 1.0f;
	light.Range = 1000.0f;
	V( pd3dDevice->SetLight( 0, &light ) );
	V( pd3dDevice->LightEnable( 0, TRUE ) );
*/

	// Set the projection matrix for the vertex shader based skinning method
/*
	if( g_SkinningMethod == D3DINDEXEDVS )
	{
		V( pd3dDevice->SetVertexShaderConstantF( 2, ( float* )&g_matProjT, 4 ) );
	}
	else if( g_SkinningMethod == D3DINDEXEDHLSLVS )
	{
		V( g_pEffect->SetMatrix( "mViewProj", &g_matProj ) );
	}
*/


	// 分割レンダリング用にコメントアウト
//	V( g_pEffect->SetMatrix( "mViewProj", &g_matProj ) );


	// 動的アンビエントライト（現在変更不可）
	V( g_pEffect->SetFloat( "DynamicAmbientLight", 0.0f ) );


#if 0
	// ライトへのベクトル
	{
		D3DXVECTOR4 vLightDir( -0.2f, 0.7f, -0.4f, 0.0f );
		D3DXVec4Normalize( &vLightDir, &vLightDir );

#if 0
		static bool b = true;
		if( b )
		{
			b = false;

			LOG( 0, L"vLightDir:  %f  %f  %f", vLightDir.x, vLightDir.y, vLightDir.z );


			D3DXVECTOR3 vCur( vLightDir.x, vLightDir.y, vLightDir.z );


			D3DXVECTOR3 vTemp;
			D3DXVECTOR3 cross;

			float theta[ 3 ] = { 0, 0, 0 };

			// z
			{
				vTemp = D3DXVECTOR3( vCur.x, vCur.y, 0 );
				D3DXVec3Normalize( &vTemp, &vTemp );

				D3DXVECTOR3 vAxis( 0, 1, 0 );

				float dotZAxis = D3DXVec3Dot( &vTemp, &vAxis );
				theta[ 2 ] = acosf( dotZAxis );

				D3DXVec3Cross( &cross, &vTemp, &vAxis );

				if( cross.z < 0 )
					theta[ 2 ] *= -1;
			}

			{
				D3DXMATRIX matRot;
				D3DXMatrixRotationZ( &matRot, theta[ 2 ] );

				D3DXVec3TransformNormal( &vCur, &vCur, &matRot );

				LOG( 0, L"vCur:  %f  %f  %f", vCur.x, vCur.y, vCur.z );
			}

/*
			// y
			{
				vTemp = D3DXVECTOR3( vCur.x, 0, vCur.z );
				D3DXVec3Normalize( &vTemp, &vTemp );

				D3DXVECTOR3 vAxis( 0, 0, 1 );

				float dotYAxis = D3DXVec3Dot( &vTemp, &vAxis );
				theta[ 1 ] = acosf( dotYAxis );

				D3DXVec3Cross( &cross, &vTemp, &vAxis );

				if( cross.y < 0 )
					theta[ 1 ] *= -1;
			}

			{
				D3DXMATRIX matRot;
				D3DXMatrixRotationY( &matRot, theta[ 1 ] );

				D3DXVec3TransformNormal( &vCur, &vCur, &matRot );

				LOG( 0, L"vCur:  %f  %f  %f", vCur.x, vCur.y, vCur.z );
			}
*/

			// x
			{
				vTemp = D3DXVECTOR3( 0, vCur.y, vCur.z );
				D3DXVec3Normalize( &vTemp, &vTemp );

				D3DXVECTOR3 vAxis( 0, 1, 0 );

				float dotXAxis = D3DXVec3Dot( &vTemp, &vAxis );
				theta[ 0 ] = acosf( dotXAxis );

				D3DXVec3Cross( &cross, &vTemp, &vAxis );

				if( cross.x < 0 )
					theta[ 0 ] *= -1;
			}

			{
				D3DXMATRIX matRot;
				D3DXMatrixRotationX( &matRot, theta[ 0 ] );

				D3DXVec3TransformNormal( &vCur, &vCur, &matRot );

				LOG( 0, L"vCur:  %f  %f  %f", vCur.x, vCur.y, vCur.z );
			}

			LOG( 0, L"theta:  %f  %f  %f", theta[ 0 ], theta[ 1 ], theta[ 2 ] );
		}
#endif

		// ビュー座標系に変換
		D3DXVec4Transform( &vLightDir, &vLightDir, &g_matView );

		V( pd3dDevice->SetVertexShaderConstantF( 1, ( float* )&vLightDir, 1 ) );
		V( g_pEffect->SetVector( "lightDir", &vLightDir ) );
	}
#endif


	// ディレクショナルライトのフレームが存在するなら、情報を取得する
	for( int i = 0;  i < DIR_LIGHT_MAX;  i ++ )
	{
		char szDirLightDir[ 64 ];
		char szDirLightCoeff[ 64 ];

		sprintf_s( szDirLightDir, 64, "DirLight%d_Dir", i );
		sprintf_s( szDirLightCoeff, 64, "DirLight%d_Coeff", i );


		D3DXVECTOR4 vLightDir( 0.0f, 1.0f, 0.0f, 0.0f );
		float lightCoeff = 0;

		if( g_pFrameDirLight[ i ] == NULL )
		{
//			if( i != 0 )
//				lightCoeff = 0;
		}
		else
		{
			D3DXMATRIX mat;
			GetFrameMatrix( g_pFrameDirLight[ i ], &mat );

//			PRINT_MATRIX( &mat );


			// y軸の方向＝ライトへの方向
			vLightDir.x = mat.m[ 1 ][ 0 ];
			vLightDir.y = mat.m[ 1 ][ 1 ];
			vLightDir.z = mat.m[ 1 ][ 2 ];

			D3DXVec4Normalize( &vLightDir, &vLightDir );


//			// 2番目以降のライトはx軸のスケールをライトの強さとする
//			if( i != 0 )

			// x軸のスケールをライトの強さとする
			{
				D3DXVECTOR3 v( mat.m[ 0 ][ 0 ], mat.m[ 0 ][ 1 ], mat.m[ 0 ][ 2 ] );
				lightCoeff = sqrtf( v.x * v.x + v.y * v.y + v.z * v.z );
			}
		}

		// ビュー座標系に変換
		D3DXVec4Transform( &vLightDir, &vLightDir, &g_matView );

		V( g_pEffect->SetVector( szDirLightDir, &vLightDir ) );
		V( g_pEffect->SetFloat( szDirLightCoeff, lightCoeff ) );
	}


/*
	// カメラのワールド座標
	{
//		D3DXVECTOR4 vEye( 0, 0, -2 * g_fObjectRadius, 1 );


		const D3DXMATRIX *pTrans = g_ArcBall.GetTranslationMatrix();

		D3DXVECTOR4 vEye( pTrans->m[ 3 ][ 0 ], pTrans->m[ 3 ][ 1 ], pTrans->m[ 3 ][ 2 ], 1 );

		V( g_pEffect->SetVector( "worldCameraPos", &vEye ) );
	}
*/

	// Begin the scene
	if( SUCCEEDED( pd3dDevice->BeginScene() ) )
	{

		{
//printf( L"OnFrameRender split\n" );

			CAveragePerformance avgp( L"draw all split surfaces and bind" );


			// 分割レンダリングループ
			for( int split_y = 0;  split_y < g_splitNum[ 1 ];  split_y ++ )
			{
				for( int split_x = 0;  split_x < g_splitNum[ 0 ];  split_x ++ )
				{
					LOG( 0x404040, L"split %d, %d", split_x, split_y );

					pd3dDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_TRUE );
					pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );

					// モデル描画
					{
						CAveragePerformance avgp( L"Draw Model" );


						// 分割レンダリング用の射影行例を作成
						{
#if 0
							D3DXVECTOR2 scale( (float)g_finalSurfaceSize[ 0 ] / ( g_splitSurfaceSize[ 0 ] - SPLIT_MARGIN_TOTAL ), (float)g_finalSurfaceSize[ 1 ] / ( g_splitSurfaceSize[ 1 ] - SPLIT_MARGIN_TOTAL ) );

							D3DXVECTOR2 transToOrigin(
														  ( (float)g_finalSurfaceSize[ 0 ] / g_splitSurfaceSize[ 0 ] - 1 ),
														- ( (float)g_finalSurfaceSize[ 1 ] / g_splitSurfaceSize[ 1 ] - 1 )
													);

							D3DXVECTOR2 transToSplit(
														split_x * - ( 2.0f - 2.0f * (float)SPLIT_MARGIN_TOTAL / g_splitSurfaceSize[ 0 ] ),
														split_y *   ( 2.0f - 2.0f * (float)SPLIT_MARGIN_TOTAL / g_splitSurfaceSize[ 1 ] )
													);

							D3DXVECTOR2 trans( transToOrigin + transToSplit );
#else

							D3DXVECTOR2 splitsizeWithoutMargin( g_splitSurfaceSize[ 0 ] - SPLIT_MARGIN_TOTAL, g_splitSurfaceSize[ 1 ] - SPLIT_MARGIN_TOTAL );

							D3DXVECTOR2 scale(
												(float)g_finalSurfaceSize[ 0 ] / splitsizeWithoutMargin.x,
												(float)g_finalSurfaceSize[ 1 ] / splitsizeWithoutMargin.y
											);


							D3DXVECTOR2 pixelNum_in_HC_one(	// HomogenousCoordinate: 同次座標系での 1.0 に相当するピクセル数（2.0 * 2.0 で全画面）
															g_splitSurfaceSize[ 0 ] * SCALE_RATE / 2,
															g_splitSurfaceSize[ 1 ] * SCALE_RATE / 2
														);

							D3DXVECTOR2 pixelNum_full(	// 拡大時の全ピクセル数
														( (float)g_finalSurfaceSize[ 0 ] + SPLIT_MARGIN_TOTAL ) * SCALE_RATE,
														( (float)g_finalSurfaceSize[ 1 ] + SPLIT_MARGIN_TOTAL ) * SCALE_RATE
													);



							D3DXVECTOR2 transToOrigin(
														  ( pixelNum_full.x / pixelNum_in_HC_one.x / 2 - 1 ),
														- ( pixelNum_full.y / pixelNum_in_HC_one.y / 2 - 1 )
													);



							D3DXVECTOR2 transToSplit(
														split_x * - ( splitsizeWithoutMargin.x * SCALE_RATE / pixelNum_in_HC_one.x ),
														split_y *   ( splitsizeWithoutMargin.y * SCALE_RATE / pixelNum_in_HC_one.y )
													);


							D3DXVECTOR2 trans( transToOrigin + transToSplit );
#endif


							D3DXMATRIXA16 matScale, matTrans;

							D3DXMatrixScaling( &matScale, scale.x, scale.y, 1 );
							D3DXMatrixTranslation( &matTrans, trans.x, trans.y, 0 );


#if 1
							D3DXMatrixMultiply( &g_matProjSplit, &g_matProj, &matScale );
#else
							// World to View 変換
							D3DXMatrixMultiply( &g_matProjSplit, &g_matView, &g_matProj );

							D3DXMatrixMultiply( &g_matProjSplit, &g_matProjSplit, &matScale );
#endif

							D3DXMatrixMultiply( &g_matProjSplit, &g_matProjSplit, &matTrans );

							V( g_pEffect->SetMatrix( "mViewProj", &g_matProjSplit ) );
						}

						CPushRT rt0( pd3dDevice, 0, g_pColorTexture );
						CPushRT rt1( pd3dDevice, 1, g_pNormalMateTexture );
						CPushRT rt2( pd3dDevice, 2, g_pDepthTexture );

						// Depth Stencil
						LPDIRECT3DSURFACE9 pSurf_DS = NULL;
						V( g_pWorkDSTexture->GetSurfaceLevel( 0, &pSurf_DS ) );
						V( pd3dDevice->SetDepthStencilSurface( pSurf_DS ) );


						V( g_pEffect->SetTexture( "g_Texture4", g_pPaletteTexture ) );


						// Clear the backbuffer
						V( pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
										   D3DCOLOR_ARGB( 0, 0, 0, 0 ), 1.0f, 0L ) );


						V( g_pEffect->SetTechnique( "Mesh" ) );

						// エクスポータが吐くモデルはカリングが逆なので合わせる
						pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW );

						DrawFrame( pd3dDevice, g_pFrameRoot );

						pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );

//						V( pd3dDevice->SetDepthStencilSurface( pLDRDSSurface ) );
						V( pd3dDevice->SetDepthStencilSurface( NULL ) );


						SAFE_RELEASE( pSurf_DS );
					}


					pd3dDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_FALSE );
					pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );


					// 解析(Analyze_CountPalette_MeanPos)
					{
						CAveragePerformance avgp( L"GPU analyze_CountPalette_MeanPos" );



						V( g_pEffect->SetTechnique( "Analyze_CountPalette_MeanPos" ) );


						V( g_pEffect->SetTexture( "g_Texture", g_pColorTexture ) );
						V( g_pEffect->SetTexture( "g_Texture3", g_pDepthTexture ) );
						V( g_pEffect->SetTexture( "g_Texture4", g_pPaletteTexture ) );


						D3DXVECTOR4 vSrcTexSize( 1.0f / g_workSurfaceSize[ 0 ], 1.0f / g_workSurfaceSize[ 1 ], 1.0f / paletteTextureWidth, 0 );
						V( g_pEffect->SetVector( "invSrcTexSize", &vSrcTexSize ) );
/*
						V( g_pEffect->SetFloat( "g_ZThreshold", g_ZThreshold ) );
						V( g_pEffect->SetFloat( "g_AngleThreshold", g_AngleThreshold ) );
*/
						V( g_pEffect->SetFloat( "g_IgnoreCountThreshold", g_IgnoreCountThreshold ) );

						DrawRect(	pd3dDevice,
									g_pAnalyzedTexture, g_pMeanPosTexture, NULL, NULL,
									D3DXVECTOR4( 0.0f, 0.0f, 1.0f, 1.0f ),
									D3DXVECTOR4( 0.0f, 0.0f, g_splitSurfaceSize[ 0 ], g_splitSurfaceSize[ 1 ] ) );
					}


					// 解析
					// g_pColorTexture2 <- ( UV of temp, paletteU, 0 )
					{
						CAveragePerformance avgp( L"GPU analyze" );



						V( g_pEffect->SetTechnique( "Analyze" ) );


						V( g_pEffect->SetTexture( "g_Texture", g_pColorTexture ) );
						V( g_pEffect->SetTexture( "g_Texture2", g_pNormalMateTexture ) );
						V( g_pEffect->SetTexture( "g_Texture3", g_pDepthTexture ) );

						V( g_pEffect->SetTexture( "g_Texture4", g_pPaletteTexture ) );
						V( g_pEffect->SetTexture( "g_Texture5", g_pAnalyzedTexture ) );
						V( g_pEffect->SetTexture( "g_Texture6", g_pMeanPosTexture ) );

						D3DXVECTOR4 vSrcTexSize( 1.0f / g_workSurfaceSize[ 0 ], 1.0f / g_workSurfaceSize[ 1 ], 1.0f / paletteTextureWidth, 0 );
						V( g_pEffect->SetVector( "invSrcTexSize", &vSrcTexSize ) );

						V( g_pEffect->SetFloat( "g_ZThreshold", g_ZThreshold ) );
						V( g_pEffect->SetFloat( "g_AngleThreshold", g_AngleThreshold ) );
						V( g_pEffect->SetFloat( "g_GutterThreshold", g_GutterThreshold ) );


						DrawRect(	pd3dDevice,
									g_pColorTexture2, g_pAnalyzedTexture2, NULL, NULL,
									D3DXVECTOR4( 0.0f, 0.0f, 1.0f, 1.0f ),
									D3DXVECTOR4( 0.0f, 0.0f, g_splitSurfaceSize[ 0 ], g_splitSurfaceSize[ 1 ] ) );
					}


					// パラメータ合成
					{
						CAveragePerformance avgp( L"mix param" );


						V( g_pEffect->SetTechnique( "Mix" ) );

						V( g_pEffect->SetTexture( "g_Texture", g_pColorTexture2 ) );
						V( g_pEffect->SetTexture( "g_Texture2", g_pAnalyzedTexture2 ) );

						V( g_pEffect->SetTexture( "g_Texture4", g_pPaletteTexture ) );
						V( g_pEffect->SetTexture( "g_Texture5", g_pAnalyzedTexture ) );
						V( g_pEffect->SetTexture( "g_Texture6", g_pMeanPosTexture ) );



						D3DXVECTOR4 vSrcTexSize( 1.0f / g_workSurfaceSize[ 0 ], 1.0f / g_workSurfaceSize[ 1 ], 1.0f / paletteTextureWidth, 0 );
						V( g_pEffect->SetVector( "invSrcTexSize", &vSrcTexSize ) );

						D3DXVECTOR4 vSplitTexSize( 1.0f / g_splitSurfaceSize[ 0 ], 1.0f / g_splitSurfaceSize[ 1 ], 0, 0 );
						V( g_pEffect->SetVector( "invSplitTexSize", &vSplitTexSize ) );


						V( g_pEffect->SetFloat( "g_ZThreshold", g_ZThreshold ) );
						V( g_pEffect->SetFloat( "g_IgnoreCountThreshold", g_IgnoreCountThreshold ) );

//						V( g_pEffect->SetInt( "finalPass_mode_DispContour", g_dispContour ) );
						V( g_pEffect->SetInt( "mixPass_mode_DispContour", g_dispContour ) );


						DrawRect(	pd3dDevice,
									g_pSplitTexture, NULL, NULL, NULL,
									D3DXVECTOR4( 0.0f, 0.0f, 1.0f, 1.0f ),
									D3DXVECTOR4( 0.0f, 0.0f, g_splitSurfaceSize[ 0 ], g_splitSurfaceSize[ 1 ] ) );
					}


					// 結果用サーフェイスにコピー（分割された画像の結合）
					{
						CAveragePerformance avgp( L"bind dst surface" );


						CPushRT rt0( pd3dDevice, 0, g_pCombinedTexture );


						pd3dDevice->SetPixelShader( NULL );
						pd3dDevice->SetVertexShader( NULL );

						pd3dDevice->SetTexture( 0, g_pSplitTexture );


						float margin_w = (float)SPLIT_MARGIN / g_splitSurfaceSize[ 0 ];
						float margin_h = (float)SPLIT_MARGIN / g_splitSurfaceSize[ 1 ];

						float w = g_splitSurfaceSize[ 0 ] - SPLIT_MARGIN_TOTAL;
						float h = g_splitSurfaceSize[ 1 ] - SPLIT_MARGIN_TOTAL;
						float x = w * split_x;
						float y = h * split_y;

						DrawRect(	pd3dDevice,
									D3DXVECTOR4( margin_w, margin_h, 1.0f - margin_w * 2, 1.0f - margin_h * 2 ),
									D3DXVECTOR4( x, y, w, h ) );
					}
				}
			}
		}



		// 最終パス
		{
			LOG( 0x404040, L"OnFrameRender select color" );

			CAveragePerformance avgp( L"select color" );


			V( g_pEffect->SetTechnique( "SelectColor" ) );

			V( g_pEffect->SetTexture( "g_Texture4", g_pPaletteTexture ) );
			V( g_pEffect->SetTexture( "g_Texture7", g_pCombinedTexture ) );

			D3DXVECTOR4 vFinalTexSize( 1.0f / g_finalSurfaceSize[ 0 ], 1.0f / g_finalSurfaceSize[ 1 ], 0, 0 );
			V( g_pEffect->SetVector( "invFinalTexSize", &vFinalTexSize ) );


			DrawRect(	pd3dDevice,
						g_pFinalTexture, NULL, NULL, NULL,
						D3DXVECTOR4( 0.0f, 0.0f, 1.0f, 1.0f ),
						D3DXVECTOR4( 0.0f, 0.0f, g_finalSurfaceSize[ 0 ], g_finalSurfaceSize[ 1 ] ) );
		}




		// コピー g_pFinalTexture -> g_pFinalTexture_SysMem
		if( g_bCapture )
		{
			LOG( 0x404040, L"OnFrameRender copy sysmem" );

			CAveragePerformance avgp( L"Copy  g_pFinalTexture -> g_pFinalTexture_SysMem" );

			LPDIRECT3DSURFACE9 pSurf = NULL;
			g_pFinalTexture->GetSurfaceLevel( 0, &pSurf );

			LPDIRECT3DSURFACE9 pSurf_SysMem = NULL;
			g_pFinalTexture_SysMem->GetSurfaceLevel( 0, &pSurf_SysMem );

			pd3dDevice->GetRenderTargetData( pSurf, pSurf_SysMem );

			SAFE_RELEASE( pSurf_SysMem );
			SAFE_RELEASE( pSurf );
		}


//printf( L"OnFrameRender make window\n" );

		pd3dDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_TRUE );
		pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );

		// 結果
		pd3dDevice->SetRenderTarget( 0, pLDRSurface );
		pd3dDevice->SetDepthStencilSurface( pLDRDSSurface );

		{
			LOG( 0x404040, L"OnFrameRender draw window" );

			CAveragePerformance avgp( L"draw window" );


			D3DSURFACE_DESC d;
			pLDRSurface->GetDesc( &d );

			pd3dDevice->SetPixelShader( NULL );
			pd3dDevice->SetVertexShader( NULL );

			float fWidth = d.Width / 4.0f;
			float fHeight = d.Height / 4.0f;

			float x = d.Width - fWidth;
			float y = d.Height - fHeight * 3;

			pd3dDevice->SetTexture( 0, g_pColorTexture );
			DrawRect(	pd3dDevice,
						D3DXVECTOR4( 0.0f, 0.0f, 1.0f, 1.0f ),
						D3DXVECTOR4( x, y, fWidth, fHeight ) );

			pd3dDevice->SetTexture( 0, g_pNormalMateTexture );
			DrawRect(	pd3dDevice,
						D3DXVECTOR4( 0.0f, 0.0f, 1.0f, 1.0f ),
						D3DXVECTOR4( x, y + fHeight, fWidth, fHeight ) );

			pd3dDevice->SetTexture( 0, g_pDepthTexture );
			DrawRect(	pd3dDevice,
						D3DXVECTOR4( 0.0f, 0.0f, 1.0f, 1.0f ),
						D3DXVECTOR4( x, y + fHeight * 2, fWidth, fHeight ) );


			x = 20;
			y = 40;

			pd3dDevice->SetTexture( 0, g_pFinalTexture );
//			pd3dDevice->SetTexture( 0, g_pColorTexture2 );
			DrawRect(	pd3dDevice,
						D3DXVECTOR4( 0.0f, 0.0f, 1.0f, 1.0f ),
						D3DXVECTOR4( x, y, g_finalSurfaceSize[ 0 ] * g_resultDispScale, g_finalSurfaceSize[ 1 ] * g_resultDispScale ) );
		}






		LOG( 0x404040, L"OnFrameRender render UI" );

		RenderText();
//		V( g_HUD.OnRender( fElapsedTime ) );
		V( g_SampleUI.OnRender( fElapsedTime ) );


		// End the scene.
		pd3dDevice->EndScene();
	}

//printf( L"OnFrameRender end scene\n" );


	//Remove any memory involved in the render target switching
	SAFE_RELEASE( pLDRDSSurface );
	SAFE_RELEASE( pLDRSurface );



	// キャプチャ
	if( g_bCapture )
	{
		LOG( 0x404040, L"OnFrameRender capture" );

		// 描画完了を待つ
		{
			CAveragePerformance avgp( L"wait draw" );

			g_pD3dQuery->Issue( D3DISSUE_END );

			while( 1 )
			{
				HRESULT hr = g_pD3dQuery->GetData( NULL, 0, D3DGETDATA_FLUSH );

				if( hr == S_OK )
				{
					break;
				}
				else if( hr == S_FALSE )
				{
//					Sleep( 1 );

					continue;
				}
				else
				{
					if( hr == D3DERR_DEVICELOST )
					{
						LOG_ERR( L"ERROR: GetData(): D3DERR_DEVICELOST" );
					}
					else
					{
						LOG_ERR( L"ERROR: GetData(): unknown %x", hr );
					}

//					break;
					return;
				}
			}
		}

		// キャプチャ
		{
			CAveragePerformance avgp( L"capture frame on CPU" );

			SaveTexture( g_pFinalTexture_SysMem, g_strCapturePath, L"frame%04d.bmp", g_currentFrame );
//			SaveTexture( g_pFinalTexture, g_strCapturePath, L"frame%04d.png", g_currentFrame );
		}
	}


	// デバッグキャプチャ
	if( g_bDebugCapture )
	{
		CAveragePerformance avgp( L"capture frame - debug" );


		SaveTexture( g_pColorTexture, g_strCapturePath, L"debug_frame%04d_g_pColorTexture.bmp", g_currentFrame );

		SaveTexture_Channel( pd3dDevice, g_pColorTexture2, MISC_COLOR_CHANNEL_A, g_strCapturePath, L"debug_frame%04d_g_pColorTexture2.bmp", g_currentFrame );
	}


//printf( L"OnFrameRender out\n" );
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text. This function uses the ID3DXFont interface for
// efficient text rendering.
//--------------------------------------------------------------------------------------
void RenderText()
{
	// The helper object simply helps keep track of text position, and color
	// and then it calls pFont->DrawText( m_pSprite, strMsg, -1, &rc, DT_NOCLIP, m_clr );
	// If NULL is passed in as the sprite object, then it will work however the
	// pFont->DrawText() will not be batched together.	Batching calls will improves performance.
	const D3DSURFACE_DESC* pd3dsdBackBuffer = DXUTGetD3D9BackBufferSurfaceDesc();
	CDXUTTextHelper txtHelper( g_pFont, g_pTextSprite, 15 );

	// Output statistics
	txtHelper.Begin();
	txtHelper.SetInsertionPos( 5, 5 );
	txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
	txtHelper.DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) ); // Show FPS
	txtHelper.DrawTextLine( DXUTGetDeviceStats() );


	{
		txtHelper.SetInsertionPos( 640 - 200, 3 );

		D3DXMATRIX *pMat = (D3DXMATRIX *)( g_ArcBall.GetTranslationMatrix() );

		txtHelper.DrawFormattedTextLine(	L"CamPos:%1.4f %1.4f %1.4f",
											- pMat->m[ 3 ][ 0 ],
											- pMat->m[ 3 ][ 1 ],
											- pMat->m[ 3 ][ 2 ] );

	}


/*
	txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
	// Output statistics
	switch( g_SkinningMethod )
	{
		case D3DNONINDEXED:
			txtHelper.DrawTextLine( L"Using fixed-function non-indexed skinning\n" );
			break;
		case D3DINDEXED:
			txtHelper.DrawTextLine( L"Using fixed-function indexed skinning\n" );
			break;
		case SOFTWARE:
			txtHelper.DrawTextLine( L"Using software skinning\n" );
			break;
		case D3DINDEXEDVS:
			txtHelper.DrawTextLine( L"Using assembly vertex shader indexed skinning\n" );
			break;
		case D3DINDEXEDHLSLVS:
			txtHelper.DrawTextLine( L"Using HLSL vertex shader indexed skinning\n" );
			break;
		default:
			txtHelper.DrawTextLine( L"No skinning\n" );
	}

	// Draw help
	if( g_bShowHelp )
	{
		txtHelper.SetInsertionPos( 10, pd3dsdBackBuffer->Height - 15 * 6 );
		txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 0.75f, 0.0f, 1.0f ) );
		txtHelper.DrawTextLine( L"Controls (F1 to hide):" );

		txtHelper.SetInsertionPos( 40, pd3dsdBackBuffer->Height - 15 * 5 );
		txtHelper.DrawTextLine( L"Rotate model: Left click drag\n"
								L"Zoom: Middle click drag\n"
								L"Pane: Right click drag\n"
								L"Quit: ESC" );
	}
	else
	{
		txtHelper.SetInsertionPos( 10, pd3dsdBackBuffer->Height - 15 * 2 );
		txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
		txtHelper.DrawTextLine( L"Press F1 for help" );
	}

	// If software vp is required and we are using a hwvp device,
	// the mesh is not being displayed and we output an error message here.
	if( g_bUseSoftwareVP && ( g_dwBehaviorFlags & D3DCREATE_HARDWARE_VERTEXPROCESSING ) )
	{
		txtHelper.SetInsertionPos( 5, 85 );
		txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 0.0f, 0.0f, 1.0f ) );
		txtHelper.DrawTextLine( L"The HWVP device does not support this skinning method.\n"
								L"Select another skinning method or switch to mixed or software VP." );
	}
*/

	txtHelper.End();
}


//--------------------------------------------------------------------------------------
// Before handling window messages, DXUT passes incoming windows
// messages to the application through this callback function. If the application sets
// *pbNoFurtherProcessing to TRUE, then DXUT will not process this message.
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
						  void* pUserContext )
{
	// Always allow dialog resource manager calls to handle global messages
	// so GUI state is updated correctly
	*pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
	if( *pbNoFurtherProcessing )
		return 0;

	if( g_SettingsDlg.IsActive() )
	{
		g_SettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
		return 0;
	}

	// Give the dialogs a chance to handle the message first
/*
	*pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
	if( *pbNoFurtherProcessing )
		return 0;
*/
	*pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
	if( *pbNoFurtherProcessing )
		return 0;

	// Pass all remaining windows messages to arcball so it can respond to user input
	g_ArcBall.HandleMessages( hWnd, uMsg, wParam, lParam );

	return 0;
}


//--------------------------------------------------------------------------------------
// As a convenience, DXUT inspects the incoming windows messages for
// keystroke messages and decodes the message parameters to pass relevant keyboard
// messages to the application.  The framework does not remove the underlying keystroke
// messages, which are still passed to the application's MsgProc callback.
//--------------------------------------------------------------------------------------
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
	if( bKeyDown )
	{
		switch( nChar )
		{
			case VK_F1:
				g_bShowHelp = !g_bShowHelp; break;
		}
	}
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
	WCHAR wszOutput[1024];

	switch( nControlID )
	{
		case IDC_TOGGLEFULLSCREEN:
			DXUTToggleFullScreen(); break;
		case IDC_TOGGLEREF:
			DXUTToggleREF(); break;
		case IDC_CHANGEDEVICE:
			g_SettingsDlg.SetActive( !g_SettingsDlg.IsActive() ); break;
		case IDC_METHOD:
		{
			METHOD NewSkinningMethod = ( METHOD )( size_t )( ( CDXUTComboBox* )pControl )->GetSelectedData();

			// If the selected skinning method is different than the current one
			if( g_SkinningMethod != NewSkinningMethod )
			{
				g_SkinningMethod = NewSkinningMethod;

				// update the meshes to the new skinning method
				UpdateSkinningMethod( g_pFrameRoot );
			}
			break;
		}


		case IDC_DISPCONTOUR:
			g_dispContour = (int)( ( CDXUTComboBox* )pControl )->GetSelectedData();
			break;

		case IDC_Z_THRESHOLD:
			g_ZThreshold = ( ( CDXUTSlider* )pControl )->GetValue() / (float)SCALE_ZThreshold;

			swprintf_s(	wszOutput, 1024, L"Z threshold  %1.3f", g_ZThreshold );
			g_SampleUI.GetStatic( IDC_Z_THRESHOLD_TEXT )->SetText( wszOutput );
			break;

		case IDC_ANGLE_THRESHOLD:
			g_AngleThreshold = ( ( CDXUTSlider* )pControl )->GetValue() / (float)SCALE_AngleThreshold;

			swprintf_s(	wszOutput, 1024, L"angle threshold  %3.1f", g_AngleThreshold );
			g_SampleUI.GetStatic( IDC_ANGLE_THRESHOLD_TEXT )->SetText( wszOutput );
			break;

		case IDC_GUTTER_THRESHOLD:
			g_GutterThreshold = ( ( CDXUTSlider* )pControl )->GetValue() / (float)SCALE_GutterThreshold;

			swprintf_s(	wszOutput, 1024, L"gutter threshold  %1.1f", g_GutterThreshold );
			g_SampleUI.GetStatic( IDC_GUTTER_THRESHOLD_TEXT )->SetText( wszOutput );
			break;

		case IDC_IGNORE_COUNT_THRESHOLD:
			g_IgnoreCountThreshold = ( ( CDXUTSlider* )pControl )->GetValue() / (float)SCALE_IgnoreCountThreshold;

			swprintf_s(	wszOutput, 1024, L"ignore count threshold  %1.1f", g_IgnoreCountThreshold );
			g_SampleUI.GetStatic( IDC_IGNORE_COUNT_THRESHOLD_TEXT )->SetText( wszOutput );
			break;
	}
}


void ReleaseResources( void )
{
	SAFE_RELEASE( g_pTextSprite );


	SAFE_RELEASE( g_pPaletteTexture );

	SAFE_RELEASE( g_pColorTexture );
	SAFE_RELEASE( g_pDepthTexture );
	SAFE_RELEASE( g_pNormalMateTexture );
	SAFE_RELEASE( g_pWorkDSTexture );

	SAFE_RELEASE( g_pColorTexture_SysMem );
	SAFE_RELEASE( g_pDepthTexture_SysMem );
	SAFE_RELEASE( g_pAnalyzedTexture );

	SAFE_RELEASE( g_pColorTexture2 );
	SAFE_RELEASE( g_pMeanPosTexture );
	SAFE_RELEASE( g_pAnalyzedTexture2 );

	SAFE_RELEASE( g_pSplitTexture );
	SAFE_RELEASE( g_pCombinedTexture );
	SAFE_RELEASE( g_pFinalTexture );

	SAFE_RELEASE( g_pFinalTexture_SysMem );



	SAFE_RELEASE( g_pD3dQuery );

/*
	// Release the vertex shaders
	for( DWORD iInfl = 0; iInfl < 4; ++iInfl )
		SAFE_RELEASE( g_pIndexedVertexShader[iInfl] );
*/
}


//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has
// entered a lost state and before IDirect3DDevice9::Reset is called. Resources created
// in the OnResetDevice callback should be released here, which generally includes all
// D3DPOOL_DEFAULT resources. See the "Lost Devices" section of the documentation for
// information about lost devices.
//--------------------------------------------------------------------------------------
void CALLBACK OnLostDevice( void* pUserContext )
{
	LOG( 0x404040, L"entered OnLostDevice()" );

	g_DialogResourceManager.OnD3D9LostDevice();
	g_SettingsDlg.OnD3D9LostDevice();
	if( g_pFont )
		g_pFont->OnLostDevice();
	if( g_pEffect )
		g_pEffect->OnLostDevice();

	ReleaseResources();
}


//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has
// been destroyed, which generally happens as a result of application termination or
// windowed/full screen toggles. Resources created in the OnCreateDevice callback
// should be released here, which generally includes all D3DPOOL_MANAGED resources.
//--------------------------------------------------------------------------------------
void CALLBACK OnDestroyDevice( void* pUserContext )
{
	LOG( 0x404040, L"entered OnDestroyDevice()" );

	g_DialogResourceManager.OnD3D9DestroyDevice();
	g_SettingsDlg.OnD3D9DestroyDevice();
	SAFE_RELEASE( g_pEffect );
	SAFE_RELEASE( g_pFont );


	ReleaseResources();

	CAllocateHierarchy Alloc;
	D3DXFrameDestroy( g_pFrameRoot, &Alloc );
	SAFE_RELEASE( g_pAnimController );


	CAveragePerformanceMan::DeleteInstance();
}


//--------------------------------------------------------------------------------------
// Called to setup the pointers for a given bone to its transformation matrix
//--------------------------------------------------------------------------------------
HRESULT SetupBoneMatrixPointersOnMesh( LPD3DXMESHCONTAINER pMeshContainerBase )
{
	UINT iBone, cBones;
	D3DXFRAME_DERIVED* pFrame;

	D3DXMESHCONTAINER_DERIVED* pMeshContainer = ( D3DXMESHCONTAINER_DERIVED* )pMeshContainerBase;

	// if there is a skinmesh, then setup the bone matrices
	if( pMeshContainer->pSkinInfo != NULL )
	{
		cBones = pMeshContainer->pSkinInfo->GetNumBones();

		pMeshContainer->ppBoneMatrixPtrs = new D3DXMATRIX*[cBones];
		if( pMeshContainer->ppBoneMatrixPtrs == NULL )
			return E_OUTOFMEMORY;

		for( iBone = 0; iBone < cBones; iBone++ )
		{
			LOG( 0, L"Bone(%3d)  '%s'", iBone, GetWideChar( pMeshContainer->pSkinInfo->GetBoneName( iBone ) ) );

			pFrame = ( D3DXFRAME_DERIVED* )D3DXFrameFind( g_pFrameRoot,
														  pMeshContainer->pSkinInfo->GetBoneName( iBone ) );
			if( pFrame == NULL )
				return E_FAIL;

			pMeshContainer->ppBoneMatrixPtrs[iBone] = &pFrame->CombinedTransformationMatrix;
		}
	}

	return S_OK;
}


//--------------------------------------------------------------------------------------
// Called to setup the pointers for a given bone to its transformation matrix
//--------------------------------------------------------------------------------------
HRESULT SetupBoneMatrixPointers( LPD3DXFRAME pFrame )
{
	HRESULT hr;

	if( pFrame->pMeshContainer != NULL )
	{
		hr = SetupBoneMatrixPointersOnMesh( pFrame->pMeshContainer );
		if( FAILED( hr ) )
			return hr;
	}

	if( pFrame->pFrameSibling != NULL )
	{
		hr = SetupBoneMatrixPointers( pFrame->pFrameSibling );
		if( FAILED( hr ) )
			return hr;
	}

	if( pFrame->pFrameFirstChild != NULL )
	{
		hr = SetupBoneMatrixPointers( pFrame->pFrameFirstChild );
		if( FAILED( hr ) )
			return hr;
	}

	return S_OK;
}




//--------------------------------------------------------------------------------------
// update the frame matrices
//--------------------------------------------------------------------------------------
void UpdateFrameMatrices( LPD3DXFRAME pFrameBase, LPD3DXMATRIX pParentMatrix )
{
	D3DXFRAME_DERIVED* pFrame = ( D3DXFRAME_DERIVED* )pFrameBase;

	if( pParentMatrix != NULL )
		D3DXMatrixMultiply( &pFrame->CombinedTransformationMatrix, &pFrame->TransformationMatrix, pParentMatrix );
	else
		pFrame->CombinedTransformationMatrix = pFrame->TransformationMatrix;

	if( pFrame->pFrameSibling != NULL )
	{
		UpdateFrameMatrices( pFrame->pFrameSibling, pParentMatrix );
	}

	if( pFrame->pFrameFirstChild != NULL )
	{
		UpdateFrameMatrices( pFrame->pFrameFirstChild, &pFrame->CombinedTransformationMatrix );
	}
}


//--------------------------------------------------------------------------------------
// update the skinning method
//--------------------------------------------------------------------------------------
void UpdateSkinningMethod( LPD3DXFRAME pFrameBase )
{
	D3DXFRAME_DERIVED* pFrame = ( D3DXFRAME_DERIVED* )pFrameBase;
	D3DXMESHCONTAINER_DERIVED* pMeshContainer;

	pMeshContainer = ( D3DXMESHCONTAINER_DERIVED* )pFrame->pMeshContainer;

	while( pMeshContainer != NULL )
	{
		GenerateSkinnedMesh( DXUTGetD3D9Device(), pMeshContainer );

		pMeshContainer = ( D3DXMESHCONTAINER_DERIVED* )pMeshContainer->pNextMeshContainer;
	}

	if( pFrame->pFrameSibling != NULL )
	{
		UpdateSkinningMethod( pFrame->pFrameSibling );
	}

	if( pFrame->pFrameFirstChild != NULL )
	{
		UpdateSkinningMethod( pFrame->pFrameFirstChild );
	}
}


void ReleaseAttributeTable( LPD3DXFRAME pFrameBase )
{
	D3DXFRAME_DERIVED* pFrame = ( D3DXFRAME_DERIVED* )pFrameBase;
	D3DXMESHCONTAINER_DERIVED* pMeshContainer;

	pMeshContainer = ( D3DXMESHCONTAINER_DERIVED* )pFrame->pMeshContainer;

	while( pMeshContainer != NULL )
	{
		delete[] pMeshContainer->pAttributeTable;

		pMeshContainer = ( D3DXMESHCONTAINER_DERIVED* )pMeshContainer->pNextMeshContainer;
	}

	if( pFrame->pFrameSibling != NULL )
	{
		ReleaseAttributeTable( pFrame->pFrameSibling );
	}

	if( pFrame->pFrameFirstChild != NULL )
	{
		ReleaseAttributeTable( pFrame->pFrameFirstChild );
	}
}


void GetFrameMatrix( LPD3DXFRAME pFrame, D3DXMATRIX *pMat )
{
	D3DXFRAME_DERIVED *_pFrame = (D3DXFRAME_DERIVED *)pFrame;

	*pMat = _pFrame->CombinedTransformationMatrix;

	// swap y & z axis（右手座標系から左手座標系へ変換）
	for( int i = 0;  i < 4;  i ++ )
	{
		float f = pMat->m[ 1 ][ i ];
		pMat->m[ 1 ][ i ] = pMat->m[ 2 ][ i ];
		pMat->m[ 2 ][ i ] = f;
	}
}


void PrintMatrix( D3DMATRIX *p )
{
	for( int i = 0;  i < 4;  i ++ )
		LOG( 0, L"  %f  %f  %f  %f", p->m[ i ][ 0 ], p->m[ i ][ 1 ], p->m[ i ][ 2 ], p->m[ i ][ 3 ] );
}


void LOG_HRESULT( HRESULT hr )
{
	LOG_ERR( L"HRESULT: %08X (%S)", hr, FAILED( hr ) ? "FAILED" : "SUCCEEDED" );
	LOG_ERR( L"ErrStr: %S", GetMultiByte( DXGetErrorString( hr ) ) );
	LOG_ERR( L"Desc: %S", GetMultiByte( DXGetErrorDescription( hr ) ) );
}
