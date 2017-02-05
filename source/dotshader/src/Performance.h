
#include <map>

#include "LogFile.h"


// このファイル内でのみ有効なログマクロ
#define _LOG( ... )	CTimeCount::m_pLogFile->PrintLine( 0x00a000, __VA_ARGS__ )


typedef unsigned long long		Uint64;

class CTimeCount
{
public:

	CTimeCount( TCHAR *str = NULL, bool bDisablePrint = false )
	{
		m_str = str;

		m_bDisablePrint = bDisablePrint;

		Start();
	}

	~CTimeCount()
	{
		float time = GetTime();

		if( ! m_bDisablePrint )
		{
			if( m_str )
				_LOG( L"%s : %.3f\n", m_str, time );
			else
				_LOG( L"CTimeCount : %.3f\n", time );
		}
	}


	void Start( void )
	{
		::QueryPerformanceCounter( (LARGE_INTEGER *)( &m_startTime ) );
	}

	float GetTime( void )
	{
		Uint64 endTime;

		::QueryPerformanceCounter( (LARGE_INTEGER *)( &endTime ) );

		return (float)( (double)( endTime - m_startTime ) / TimeCount_oneSec );
	}


	static void Init( LogFile *pLogFile )
	{
		m_pLogFile = pLogFile;

		::QueryPerformanceFrequency( (LARGE_INTEGER *)( &TimeCount_oneSec ) );
	}


	TCHAR *m_str;
	Uint64 m_startTime;

	bool m_bDisablePrint;

	static LogFile *m_pLogFile;

	static Uint64 TimeCount_oneSec;
};


class CAveragePerformanceMan
{
private:

	static CAveragePerformanceMan *m_pAveragePerformanceMan;

	CAveragePerformanceMan(){}
	CAveragePerformanceMan( const CAveragePerformanceMan &instance ){}

	virtual ~CAveragePerformanceMan()
	{
		for( TMap::iterator it = m_map.begin();  it != m_map.end();  ++ it )
		{
			delete it->second;
		}

		m_map.clear();
	}


public:

	static CAveragePerformanceMan *GetInstance()
	{
		if( m_pAveragePerformanceMan == NULL )
		{
			m_pAveragePerformanceMan = new CAveragePerformanceMan();
		}

		return m_pAveragePerformanceMan;
	}

	static void DeleteInstance()
	{
		if( m_pAveragePerformanceMan )
		{
			delete m_pAveragePerformanceMan;
		}

		m_pAveragePerformanceMan = NULL;
	}



	static const int m_startFrame = 10;
	static const int m_countFrameNum = 10;


	typedef struct _SData
	{
		_SData()	: count( -1 )
					, timeSum( 0 )
					, tc( NULL, true )
		{
		}


		int count;
		float timeSum;

		CTimeCount tc;

	} SData;

	typedef std::pair< TCHAR *, SData * > TPairData;

	typedef std::map< TCHAR *, SData * > TMap;

	TMap m_map;



	void Start( TCHAR *name )
	{
		TMap::iterator it = m_map.find( name );

		if( it == m_map.end() )
		{
			SData *p = new SData;

			it = m_map.insert( TPairData( name, p ) ).first;
		}

		SData *p = it->second;


		p->count ++;

		if( m_startFrame <= p->count  &&  p->count < m_startFrame + m_countFrameNum )
		{
			p->tc.Start();
		}
	}

	void End( TCHAR *name )
	{
		TMap::iterator it = m_map.find( name );

		if( it != m_map.end() )
		{
			SData *p = it->second;

			if( m_startFrame <= p->count  &&  p->count < m_startFrame + m_countFrameNum )
			{
				p->timeSum += p->tc.GetTime();

				// 最後のフレームで平均値出力
				if( p->count == m_startFrame + m_countFrameNum - 1 )
				{
					float time = p->timeSum / m_countFrameNum;

					_LOG( L":::average of  %s : %f\n", it->first, time );
				}
			}
		}
	}
};


class CAveragePerformance
{
public:

	CAveragePerformance( TCHAR *str )
	{
		m_str = str;

		CAveragePerformanceMan::GetInstance()->Start( m_str );
	}

	virtual ~CAveragePerformance()
	{
		CAveragePerformanceMan::GetInstance()->End( m_str );
	}


	TCHAR *m_str;
};


#undef _LOG
