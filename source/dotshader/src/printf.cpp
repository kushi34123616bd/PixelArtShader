//#include "Pch.h"

#include "DXUT.h"

#include "printf.h"

void OutputDebugStringFA( const char *format, ... )
{
	va_list args;
	va_start( args, format );
	OutputDebugStringFVA( format, args );
	va_end( args );
}
void OutputDebugStringFW( const wchar_t *format, ... )
{
	va_list args;
	va_start( args, format );
	OutputDebugStringFVW( format, args );
	va_end( args );
}
void OutputDebugStringFVA( const char *format, va_list args )
{
	int len = _vscprintf( format, args ) + 1;
	char * buffer = new char[len];
	vsprintf_s( buffer, len, format, args );
	OutputDebugStringA( buffer );
	delete[] buffer;
}
void OutputDebugStringFVW( const wchar_t *format, va_list args )
{
	int len = _vscwprintf( format, args ) + 1;
	wchar_t * buffer = new wchar_t[len];
	vswprintf_s( buffer, len, format, args );
	OutputDebugStringW( buffer );
	delete[] buffer;
}
