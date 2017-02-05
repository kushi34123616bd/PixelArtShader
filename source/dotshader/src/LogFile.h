/**
	@file
	@brief ログ用HTMLファイル
	@author 葉迩倭
*/

#pragma once

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
//#include <Common.h>

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
	/**
		@brief ログ用HTMLファイル操作
		@author 葉迩倭
	*/
	class LogFile
	{
	private:
		HANDLE	m_hFile;

	private:
		int Write( const void* pData, int Size );
		int GetFileSize();
		int GetFilePosition();
		int SeekStart( int Offset );
		int SeekEnd( int Offset );
		int Seek( int Offset );

	public:
		/**
			@brief コンストラクタ
			@author 葉迩倭
			@param pFileName	[in] ファイル名
			@param pTitle		[in] タイトル
			@note
			指定したファイル名のhtmlファイルを生成します。
		*/
		LogFile( const wchar_t* pFileName, const wchar_t* pTitle );
		/**
			@brief デストラクタ
			@author 葉迩倭
			@note
			htmlタグを閉じてファイルをcloseします。
		*/
		~LogFile();
		/**
			@brief 描画
			@author 葉迩倭
			@param Color	[in] 描画色
			@param pStr		[in] 描画文字列（printfと同じ書式）
			@note
			文字列の描画をします。
		*/
		void Print( int Color, const wchar_t* pStr,... );
		/**
			@brief 太字描画
			@author 葉迩倭
			@param Color	[in] 描画色
			@param pStr		[in] 描画文字列（printfと同じ書式）
			@note
			太字で文字列の描画をします。
		*/
		void PrintStrong( int Color, const wchar_t* pStr,... );
		/**
			@brief 改行付き描画
			@author 葉迩倭
			@param Color	[in] 描画色
			@param pStr		[in] 描画文字列（printfと同じ書式）
			@note
			改行付きの文字列の描画をします。
		*/
		void PrintLine( int Color, const wchar_t* pStr,... );
		/**
			@brief 改行付き太字描画
			@author 葉迩倭
			@param Color	[in] 描画色
			@param pStr		[in] 描画文字列（printfと同じ書式）
			@note
			改行付きの太字で文字列の描画をします。
		*/
		void PrintStrongLine( int Color, const wchar_t* pStr,... );
		/**
			@brief テーブル描画
			@author 葉迩倭
			@param Width	[in] タイトル幅
			@param pTitle	[in] タイトル
			@param pStr		[in] 描画文字列（printfと同じ書式）
			@note
			１行完結のテーブルを描画します
		*/
		void PrintTable( int Width, const wchar_t* pTitle, const wchar_t* pStr,... );
		/**
			@brief テーブル描画
			@author 葉迩倭
			@param ColorTitle	[in] タイトル色
			@param Color		[in] 文字色
			@param pTitle		[in] タイトル
			@param pKind		[in] 種類
			@param pStr			[in] 描画文字列（printfと同じ書式）
			@note
			１行完結のテーブルを描画します
		*/
		void PrintTable( int ColorTitle, int Color, const wchar_t* pTitle, const wchar_t* pKind, const wchar_t* pStr,... );
		/**
			@brief セルタイトル描画
			@author 葉迩倭
			@param Color		[in] 文字色
			@param pTitle		[in] タイトル
			@note
			セルのタイトルを描画します。
		*/
		void PrintCellTitle( int Color, const wchar_t* pTitle );
		/**
			@brief セル種類描画
			@author 葉迩倭
			@param pKind	[in] 種類（printfと同じ書式）
			@note
			セルの種類を描画します。
		*/
		void PrintCellKind( const wchar_t* pKind,... );
		/**
			@brief テーブル開始
			@author 葉迩倭
			@note
			テーブルの開始をします。
		*/
		void TableBegin();
		/**
			@brief テーブル終了
			@author 葉迩倭
			@note
			テーブルの終了をします。
		*/
		void TableEnd();
		/**
			@brief １行テーブル
			@author 葉迩倭
			@param Bold	[in] 高さ
			@note
			１行だけのテーブルを出力します。
		*/
		void TableLine( int Bold );
		/**
			@brief セル開始
			@author 葉迩倭
			@param Width	[in] セルの幅
			@note
			セルの開始をします。
		*/
		void CellBegin( int Width );
		/**
			@brief セル終了
			@author 葉迩倭
			@note
			セルの終了をします。
		*/
		void CellEnd();
	};


