#ifndef MACOPLOCALE_H
#define MACOPLOCALE_H

#ifndef MACGOGI
#include "platforms/mac/util/MachOCompatibility.h"
#endif

#include "modules/pi/OpLocale.h"

class MacOpLocale : public OpLocale
{
private:
	CFDateFormatterRef m_date_time_formatter;
	CFDateFormatterRef m_date_formatter;
	CFDateFormatterRef m_time_formatter;
	CFDateFormatterRef m_weekday_formatter;
	CFDateFormatterRef m_weekdayfull_formatter;
	CFDateFormatterRef m_month_formatter;
	CFDateFormatterRef m_monthfull_formatter;

public:
	MacOpLocale();
	virtual ~MacOpLocale();

	virtual int CompareStringsL(const uni_char *str1, const uni_char *str2, long len
#ifdef PI_CAP_COMPARESTRINGSL_CASE
								, BOOL ignore_case
#endif // PI_CAP_COMPARESTRINGSL_CASE
		);

	virtual size_t op_strftime(uni_char *dest, size_t max, const uni_char *fmt, const struct tm *tm);
	BOOL Use24HourClock();
	virtual OP_STATUS GetFirstDayOfWeek(int& day);

	virtual int NumberFormat(uni_char * buffer, size_t max, int number, BOOL with_thousands_sep);
	virtual int NumberFormat(uni_char * buffer, size_t max, double number, int precision, BOOL with_thousands_sep);
	virtual int NumberFormat(uni_char * buffer, size_t max, OpFileLength number, BOOL with_thousands_sep);

   virtual OP_STATUS ConvertFromLocaleEncoding(OpString* utf16_str, const char* locale_str);
   virtual OP_STATUS ConvertToLocaleEncoding(OpString8* locale_str, const uni_char* utf16_str);

};

#endif
