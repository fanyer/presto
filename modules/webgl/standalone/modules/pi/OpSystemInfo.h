#ifndef OP_SYSTEM_INFO_H
#define OP_SYSTEM_INFO_H

class OpSystemInfo
{
public:
	double GetTimeUTC();
	double GetRuntimeMS();
	unsigned int GetRuntimeTickMS();

#if defined _MACINTOSH_ || defined UNIX || defined(MSWIN) && !defined(_MSC_VER)
	OpSystemInfo();

	int DaylightSavingsTimeAdjustmentMS(double t) { return IsDST(t) ? 3600000 : 0; }
	int GetTimezone() { return m_timezone; }

private:
	double m_last_dst; ///< Last change to DST
	double m_next_dst; ///< Next change to DST
	bool m_is_dst; ///< Does DST apply between last and next ?
	long m_timezone; ///< GetTimezone() cache: seconds west of UTC, DST-adjusted.
	long ComputeTimezone(); ///< Update m_*_isdst, return value for m_timezone.
	bool IsDST(double t); ///< Determines whether DST applies to time t.
#else
	int DaylightSavingsTimeAdjustmentMS(double t);
	int GetTimezone();
#endif
};

#endif
