#include "DXUT.h"

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
#include "LogFile.h"


#include <Strsafe.h>


//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------

typedef unsigned char Uint8;
typedef unsigned int Uint32;


//-----------------------------------------------------------------------------------
// VARIABLE
//-----------------------------------------------------------------------------------
	static const wchar_t g_HtmlHeadStart[] = {
		L"<HTML>\r\n"
		L"<HEAD>\r\n"
		L"<TITLE>%s</TITLE>\r\n"
		L"<META http-equiv=\"Content-Type\" content=\"text/html\">\r\n"
		L"<STYLE type=\"text/css\">\r\n"
		L"<!--\r\n"
		L"BODY{\r\n"
		L" font-family: Consolas, 'Courier New', Courier, Monaco, monospace;\r\n"
		L" font-size : 16px;\r\n"
		L" font-weight : normal;\r\n"
		L"}\r\n"
		L"TABLE{\r\n"
		L" font-family: Consolas, 'Courier New', Courier, Monaco, monospace;\r\n"
		L" font-size : 16px;\r\n"
		L" font-weight : normal;\r\n"
		L"}\r\n"
		L"-->\r\n"
		L"</STYLE>\r\n"
		L"</HEAD>\r\n"
		L"<BODY text=\"#000000\" bgcolor=\"#FFFFFF\">\r\n"
	};

	static const wchar_t g_HtmlHeadEnd[] = {
		L"</BODY>\r\n"
		L"</HTML>\r\n"
	};

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
LogFile::LogFile( const wchar_t* pFileName, const wchar_t* pTitle )
	: m_hFile	( INVALID_HANDLE_VALUE )
{
	if ( pFileName != NULL )
	{
		m_hFile = ::CreateFile( pFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
		if ( m_hFile == INVALID_HANDLE_VALUE ) return;

		Uint8 Bom[] = { 0xFF, 0xFE };
		Write( (const void*)Bom, (int)sizeof(Bom) );

		wchar_t Buff[1024] = L"";
		StringCbPrintf( Buff, sizeof(Buff), g_HtmlHeadStart, pTitle );
		Write( (const void*)Buff, (int)wcslen( Buff ) * 2 );
	}
}

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
LogFile::~LogFile()
{
	Write( (const void*)g_HtmlHeadEnd, sizeof(g_HtmlHeadEnd) );
}

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
int LogFile::Write( const void* pData, int Size )
{
	if ( m_hFile == INVALID_HANDLE_VALUE ) return 0;

	Uint32 WriteBytes;
	::WriteFile( m_hFile, pData, Size, (LPDWORD)&WriteBytes, NULL);
	return WriteBytes;
}

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
int LogFile::GetFileSize()
{
	if ( m_hFile == INVALID_HANDLE_VALUE ) return 0;

	return ::GetFileSize( m_hFile, NULL );
}

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
int LogFile::SeekStart( int Offset )
{
	if ( m_hFile == INVALID_HANDLE_VALUE ) return 0;

	return ::SetFilePointer( m_hFile, Offset, NULL, FILE_BEGIN );
}

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
int LogFile::SeekEnd( int Offset )
{
	if ( m_hFile == INVALID_HANDLE_VALUE ) return 0;

	return ::SetFilePointer( m_hFile, Offset, NULL, FILE_END );
}

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
int LogFile::Seek( int Offset )
{
	if ( m_hFile == INVALID_HANDLE_VALUE ) return 0;

	return ::SetFilePointer( m_hFile, Offset, NULL, FILE_CURRENT );
}

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
int LogFile::GetFilePosition()
{
	return LogFile::Seek( 0 );
}	

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
void LogFile::Print( int Color, const wchar_t* pStr,... )
{
	wchar_t Buff[1024] = L"";
	wchar_t InBuff[1024] = L"";

	StringCbVPrintf( Buff, sizeof(Buff), pStr, reinterpret_cast<va_list>(&pStr + 1) );

	static wchar_t In[] = L"<FONT COLOR=\"#%06X\">";
	static wchar_t Out[] = L"</FONT>";

	StringCbPrintf( InBuff, sizeof(InBuff), In, Color );

	if ( Color != 0x000000 )
	{
		Write( (const void*)InBuff, (int)wcslen(InBuff) * 2 );
		Write( (const void*)Buff, (int)wcslen(Buff) * 2 );
		Write( (const void*)Out, (int)wcslen(Out) * 2 );
	}
	else
	{
		Write( (const void*)Buff, (int)wcslen(Buff) * 2 );
	}
}

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
void LogFile::PrintStrong( int Color, const wchar_t* pStr,... )
{
	wchar_t Buff[1024] = L"";
	wchar_t Str[1024] = L"";
	wchar_t InBuff[1024] = L"";

	StringCbPrintf( Str, sizeof(Str), L"<B>%s</B>", pStr );
	StringCbVPrintf( Buff, sizeof(Buff), Str, reinterpret_cast<va_list>(&pStr + 1) );

	static wchar_t In[] = L"<FONT COLOR=\"#%06X\">";
	static wchar_t Out[] = L"</FONT>\r\n";

	StringCbPrintf( InBuff, sizeof(InBuff), In, Color );

	if ( Color != 0x000000 )
	{
		Write( (const void*)InBuff, (int)wcslen(InBuff) * 2 );
		Write( (const void*)Buff, (int)wcslen(Buff) * 2 );
		Write( (const void*)Out, (int)wcslen(Out) * 2 );
	}
	else
	{
		Write( (const void*)Buff, (int)wcslen(Buff) * 2 );
	}
}

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
void LogFile::PrintLine( int Color, const wchar_t* pStr,... )
{
	wchar_t Buff[1024] = L"";

	StringCbVPrintf( Buff, sizeof(Buff), pStr, reinterpret_cast<va_list>(&pStr + 1) );

	Print( Color, Buff );
	Print( 0x000000, L"<BR>\r\n" );
}

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
void LogFile::PrintStrongLine( int Color, const wchar_t* pStr,... )
{
	wchar_t Buff[1024] = L"";

	StringCbVPrintf( Buff, sizeof(Buff), pStr, reinterpret_cast<va_list>(&pStr + 1) );

	PrintStrong( Color, Buff );
	Print( 0x000000, L"<BR>\r\n" );
}

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
void LogFile::PrintTable( int Width, const wchar_t* pTitle, const wchar_t* pStr,... )
{
	wchar_t Buff[1024] = L"";
	StringCbVPrintf( Buff, sizeof(Buff), pStr, reinterpret_cast<va_list>(&pStr + 1) );

	TableBegin();
	CellBegin( Width );
	PrintLine( 0x000000, pTitle );
	CellEnd();
	CellBegin( 0 );
	PrintLine( 0x000000, Buff );
	CellEnd();
	TableEnd();
}

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
void LogFile::PrintTable( int ColorTitle, int Color, const wchar_t* pTitle, const wchar_t* pKind, const wchar_t* pStr,... )
{
	wchar_t Buff[1024] = L"";
	StringCbVPrintf( Buff, sizeof(Buff), pStr, reinterpret_cast<va_list>(&pStr + 1) );

	TableBegin();
	PrintCellTitle( ColorTitle, pTitle );
	PrintCellKind( pKind );
	CellBegin( 0 );
	Print( Color, Buff );
	CellEnd();
	TableEnd();
}

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
void LogFile::PrintCellTitle( int Color, const wchar_t* pTitle )
{
	CellBegin( 64 );
	PrintStrong( Color, pTitle );
	CellEnd();
}

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
void LogFile::PrintCellKind( const wchar_t* pKind,... )
{
	wchar_t Buff[1024] = L"";
	StringCbVPrintf( Buff, sizeof(Buff), pKind, reinterpret_cast<va_list>(&pKind + 1) );

	CellBegin( 192 );
	Print( 0x000000, Buff );
	CellEnd();
}

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
void LogFile::TableBegin()
{
	static const wchar_t Table[] = {
		L"<TABLE width=\"100%\">\r\n"
		L"  <TBODY>\r\n"
		L"    <TR>\r\n"
	};

	Write( (const void*)Table, (int)wcslen(Table) * 2 );
}

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
void LogFile::TableEnd()
{
	static const wchar_t Table[] = {
		L"    </TR>\r\n"
		L"  </TBODY>\r\n"
		L"</TABLE>\r\n"
	};

	Write( (const void*)Table, (int)wcslen(Table) * 2 );
}

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
void LogFile::CellBegin( int Width )
{
	static const wchar_t Table1[] = {
		L"<TD width=\"%d\" valign=\"top\">\r\n"
	};

	static const wchar_t Table2[] = {
		L"<TD width=\"%d\" valign=\"top\">\r\n"
	};

	wchar_t Buff[1024] = L"";
	StringCbPrintf( Buff, sizeof(Buff), (Width == 0) ? Table2 : Table1, Width );

	Write( (const void*)Buff, (int)wcslen(Buff) * 2 );
}

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
void LogFile::CellEnd()
{
	static const wchar_t Table[] = {
		L"</TD>\r\n"
	};

	Write( (const void*)Table, (int)wcslen(Table) * 2 );
}

//-----------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------
void LogFile::TableLine( int Bold )
{
	static const wchar_t Table[] = {
		L"<TABLE width=\"100%%\">\r\n"
		L"  <TR>\r\n"
		L"    <TD height=\"%d\" bgcolor=\"#000000\"></TD>\r\n"
		L"  </TR>\r\n"
		L"</TABLE>\r\n"
	};

	wchar_t Buff[1024] = L"";
	StringCbPrintf( Buff, sizeof(Buff), Table, Bold );

	size_t Len = wcslen( Buff );
	Write( (const void*)Buff, (int)Len * 2 );
}

