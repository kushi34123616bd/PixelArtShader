#pragma once

///////////////////////////////////////////////////////////////////////////////
//
// Teco.dll ‚ð—˜—p‚·‚é‚½‚ß‚ÌƒNƒ‰ƒX
//
///////////////////////////////////////////////////////////////////////////////


class CTecoDll
{
public:
	CTecoDll()
		: m_hDLL( NULL )
		, m_retValue( RET_SUCCESS )
	{
	}

	~CTecoDll()
	{
		Finish();
	}


	typedef int (CALLBACK *LP_GPSCONV_STARTCONVERT)( char * );
	typedef int (CALLBACK *LP_GPSCONV_GETOUTDATASIZE)( void );
	typedef BYTE *(CALLBACK *LP_GPSCONV_GETOUTDATA)( void );
	typedef void (CALLBACK *LP_GPSCONV_FREEDATA)( void );
	typedef char *(CALLBACK *LP_GPSCONV_GETERRORMESSAGE)( void );


	void Finish( void )
	{
		if( m_hDLL )
		{
			FreeLibrary( m_hDLL );
		}

		m_hDLL = NULL;
		m_StartConvert = NULL;
		m_GetOutDataSize = NULL;
		m_GetOutData = NULL;
		m_FreeData = NULL;
		m_GetErrorMessage = NULL;

		m_retValue = RET_SUCCESS;
	}


	bool Init( void )
	{
		Finish();

		m_hDLL = LoadLibrary( TEXT( "Teco" ) );
		if( m_hDLL )
		{
			m_StartConvert		= (LP_GPSCONV_STARTCONVERT)		GetProcAddress( m_hDLL, "StartConvert" );
			m_GetOutDataSize	= (LP_GPSCONV_GETOUTDATASIZE)	GetProcAddress( m_hDLL, "GetOutDataSize" );
			m_GetOutData		= (LP_GPSCONV_GETOUTDATA)		GetProcAddress( m_hDLL, "GetOutData" );
			m_FreeData			= (LP_GPSCONV_FREEDATA)			GetProcAddress( m_hDLL, "FreeData" );
			m_GetErrorMessage	= (LP_GPSCONV_GETERRORMESSAGE)	GetProcAddress( m_hDLL, "GetErrorMessage" );
		}

		if(	    ! m_hDLL
			||  ! m_StartConvert
			||  ! m_GetOutDataSize
			||  ! m_GetOutData
			||  ! m_FreeData
			||  ! m_GetErrorMessage
		)
		{
			Finish();
			return false;
		}

		return true;
	}


	bool Convert( char *szSrcFileName, BYTE *&pOutData, int &outDataSize )
	{
		pOutData = NULL;
		outDataSize = 0;

		if( ! m_hDLL )
		{
			return false;
		}

		m_retValue = m_StartConvert( szSrcFileName );

		if( m_retValue <= RET_WARNING )
		{
			outDataSize = m_GetOutDataSize();

			pOutData = new BYTE[ outDataSize ];

			memcpy_s( pOutData, outDataSize, m_GetOutData(), outDataSize );

			m_FreeData();

			return true;
		}

		return false;
	}


	char *GetErrorMessage( void )
	{
		return m_GetErrorMessage();
	}


	int GetError( void )
	{
		return m_retValue;
	}



	HINSTANCE	m_hDLL;

	LP_GPSCONV_STARTCONVERT		m_StartConvert;
	LP_GPSCONV_GETOUTDATASIZE	m_GetOutDataSize;
	LP_GPSCONV_GETOUTDATA		m_GetOutData;
	LP_GPSCONV_FREEDATA			m_FreeData;
	LP_GPSCONV_GETERRORMESSAGE	m_GetErrorMessage;

	enum
	{
		RET_SUCCESS		= 0,
		RET_WARNING		= 1,
		RET_ERROR		= 2,
		RET_FATAL_ERROR	= 3,
	};

	int m_retValue;
};
