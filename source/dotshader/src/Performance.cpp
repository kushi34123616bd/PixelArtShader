#include "DXUT.h"

#include "printf.h"
#include "Performance.h"


///////////////////////////////////////////////////////////////////////////////

LogFile *CTimeCount::m_pLogFile = NULL;

Uint64 CTimeCount::TimeCount_oneSec = 0;


CAveragePerformanceMan *CAveragePerformanceMan::m_pAveragePerformanceMan = NULL;
