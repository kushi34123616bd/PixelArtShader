
#ifdef OutputDebugString
#undef OutputDebugString
#endif

#define printf	OutputDebugString

#ifdef UNICODE
#define OutputDebugString	OutputDebugStringFW
#define OutputDebugStringV	OutputDebugStringFVW
#else
#define OutputDebugString	OutputDebugStringFA
#define OutputDebugStringV	OutputDebugStringFVA
#endif // !UNICODE

void OutputDebugStringFA( const char *format, ... );
void OutputDebugStringFW( const wchar_t *format, ... );
void OutputDebugStringFVA( const char *format, va_list args );
void OutputDebugStringFVW( const wchar_t *format, va_list args );
