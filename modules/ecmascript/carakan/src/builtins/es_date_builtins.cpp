/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/builtins/es_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_date_builtins.h"
#include "modules/ecmascript/carakan/src/object/es_date_object.h"
#include "modules/stdlib/util/opdate.h"

/* The Date builtin methods consult OpDate and PI layer methods
 * to sample and compute date/times. In the general case, these
 * should be considered as not callable while on a thread stack.
 *
 * Switching stacks unnecessarily isn't desirable, so
 * for OpDate, the following methods are considered as unsafe
 * for thread stack usage:
 *
 *  OpDate::UTC()
 *  OpDate::LocalTime()
 *  OpDate::GetCurrentUTCTime()
 *  OpDate::ParseDate()
 *
 * NOTE: should any of the other OpDate methods change to also
 * require calling out to the platform, this needs to be
 * reflected for ES_DateBuiltins. We do assume a higher level
 * of stability for these methods in that regard, however.
 */

class ES_SuspendedParseDate
    : public ES_SuspendedCall
{
public:
    ES_SuspendedParseDate(ES_Execution_Context *context, const uni_char *date_string, BOOL with_localtime)
        : utc(-1)
        , local(-1)
        , date_string(date_string)
        , with_localtime(with_localtime)
    {
        context->SuspendedCall(this);
    }

    double utc;
    double local;

private:
    virtual void DoCall(ES_Execution_Context *context)
    {
        utc = OpDate::ParseDate(date_string, FALSE);
        if (with_localtime)
            local = OpDate::LocalTime(utc);
    }

    const uni_char *date_string;
    BOOL with_localtime;
};

class ES_SuspendedUTC
    : public ES_SuspendedCall
{
public:
    ES_SuspendedUTC(ES_Execution_Context *context, double local)
        : result(-1)
        , local(local)
    {
        context->SuspendedCall(this);
    }

    double result;

private:
    virtual void DoCall(ES_Execution_Context *context)
    {
        result = OpDate::UTC(local);
    }

    double local;
};

class ES_SuspendedGetCurrentTimeUTC
    : public ES_SuspendedCall
{
public:
    ES_SuspendedGetCurrentTimeUTC(ES_Execution_Context *context)
        : result(-1)
    {
        context->SuspendedCall(this);
    }

    double result;
private:
    virtual void DoCall(ES_Execution_Context *context)
    {
        result = OpDate::GetCurrentUTCTime();
    }
};

static const char invalid_date[] = "Invalid Date";

static const int year_days[13] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};
static const int leap_year_days[13] = {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366};

ST_HEADER_UNI(months)
    ST_ENTRY(0,UNI_L("January"))
    ST_ENTRY(1,UNI_L("February"))
    ST_ENTRY(2,UNI_L("March"))
    ST_ENTRY(3,UNI_L("April"))
    ST_ENTRY(4,UNI_L("May"))
    ST_ENTRY(5,UNI_L("June"))
    ST_ENTRY(6,UNI_L("July"))
    ST_ENTRY(7,UNI_L("August"))
    ST_ENTRY(8,UNI_L("September"))
    ST_ENTRY(9,UNI_L("October"))
    ST_ENTRY(10,UNI_L("November"))
    ST_ENTRY(11,UNI_L("December"))
ST_FOOTER_UNI

#define MONTHS(x) ST_REF(months,x)

ST_HEADER_UNI(days)
    ST_ENTRY(0,UNI_L("Sunday"))
    ST_ENTRY(1,UNI_L("Monday"))
    ST_ENTRY(2,UNI_L("Tuesday"))
    ST_ENTRY(3,UNI_L("Wednesday"))
    ST_ENTRY(4,UNI_L("Thursday"))
    ST_ENTRY(5,UNI_L("Friday"))
    ST_ENTRY(6,UNI_L("Saturday"))
ST_FOOTER_UNI

#define DAYS(x) ST_REF(days,x)

/** TTS == TimeTo<some>String */
class ES_SuspendedTTS
    : public ES_SuspendedCall
{
public:
    typedef JString* (*TimeToXyzString)(ES_Context *context, double time);

    ES_SuspendedTTS(TimeToXyzString fn, ES_Execution_Context *context, double time)
        : result(NULL),
          fn(fn),
          time(time),
          with_time(TRUE),
          status(OpStatus::OK)
    {
        context->SuspendedCall(this);
        if (OpStatus::IsMemoryError(status))
            context->AbortOutOfMemory();
    }

    ES_SuspendedTTS(TimeToXyzString fn, ES_Execution_Context *context)
        : result(NULL),
          fn(fn),
          time(0),
          with_time(FALSE),
          status(OpStatus::OK)
    {
        context->SuspendedCall(this);
        if (OpStatus::IsMemoryError(status))
            context->AbortOutOfMemory();
    }

    JString *result;

private:
    virtual void DoCall(ES_Execution_Context *context)
    {
        TRAP(status, result = (*fn)(context, with_time ? time : OpDate::GetCurrentUTCTime()));
    }

    TimeToXyzString fn;
    double time;
    BOOL with_time;
    OP_STATUS status;
};

/* static */ JString*
ES_DateBuiltins::TimeToString(ES_Context *context, double time)
{
    if (op_isnan(time))
        return JString::Make(context, invalid_date);

    uni_char string[128]; // ARRAY OK 2008-06-09 stanislavj
    double localtime = OpDate::LocalTime(time);
    int zoneoffset = DOUBLE2INT32((localtime - time) / msPerMinute);

    zoneoffset = zoneoffset % 60 + (zoneoffset / 60 * 100);

    if (uni_snprintf(string, ARRAY_SIZE(string) - 1,
                     // The .3 is highly desirable and some sites depend on it, see bug #54175
#ifdef ZONENAME_IN_DATESTRING   // It breaks www.islam.org and is not compatible with MSIE, but with Mozilla
                     UNI_L("%.3s %.3s %02d %04d %02d:%02d:%02d GMT%+05d (%s)"),
#else
                     UNI_L("%.3s %.3s %02d %04d %02d:%02d:%02d GMT%+05d"),
#endif
                     DAYS(OpDate::WeekDay(localtime)),
                     MONTHS(OpDate::MonthFromTime(localtime)),
                     OpDate::DateFromTime(localtime),
                     OpDate::YearFromTime(localtime),
                     OpDate::HourFromTime(localtime),
                     OpDate::MinFromTime(localtime),
                     OpDate::SecFromTime(localtime),
                     zoneoffset
#ifdef ZONENAME_IN_DATESTRING
                     , uni_tzname(0)
#endif
                    ) < 0)
        LEAVE(OpStatus::ERR_NO_MEMORY);

    string[ARRAY_SIZE(string) - 1] = 0;
    return JString::Make(context, string);
}

/* static */ JString*
ES_DateBuiltins::TimeToDateString(ES_Context *context, double time)
{
    if (op_isnan(time))
        return JString::Make(context, invalid_date);

    uni_char string[128]; // ARRAY OK 2008-06-09 stanislavj
    double localtime = OpDate::LocalTime(time);

    // The .3 is highly desirable and some sites depend on it, see bug #54175
    if (uni_snprintf(string, ARRAY_SIZE(string) - 1, UNI_L("%.3s, %02d %.3s %04d"),
                     DAYS(OpDate::WeekDay(localtime)),
                     OpDate::DateFromTime(localtime),
                     MONTHS(OpDate::MonthFromTime(localtime)),
                     OpDate::YearFromTime(localtime)) < 0)
        LEAVE(OpStatus::ERR_NO_MEMORY);

    string[ARRAY_SIZE(string) - 1] = 0;
    return JString::Make(context, string);
}

/* static */ JString*
ES_DateBuiltins::TimeToTimeString(ES_Context *context, double time)
{
    if (op_isnan(time))
        return JString::Make(context, invalid_date);

    uni_char string[128]; // ARRAY OK 2008-06-09 stanislavj
    double localtime = OpDate::LocalTime(time);
    int zoneoffset = DOUBLE2INT32((localtime - time) / msPerMinute);

    zoneoffset = zoneoffset % 60 + (zoneoffset / 60 * 100);

   if (uni_snprintf(string, ARRAY_SIZE(string) - 1,
#ifdef ZONENAME_IN_DATESTRING
                    UNI_L("%02d:%02d:%02d GMT%+05d (%s)"),
#else
                    UNI_L("%02d:%02d:%02d GMT%+05d"),
#endif
                    OpDate::HourFromTime(localtime),
                    OpDate::MinFromTime(localtime),
                    OpDate::SecFromTime(localtime),
                    zoneoffset
#ifdef ZONENAME_IN_DATESTRING
                    , uni_tzname(0)
#endif
                   ) < 0)
        LEAVE(OpStatus::ERR_NO_MEMORY);

    string[ARRAY_SIZE(string) - 1] = 0;
    return JString::Make(context, string);
}

static double
BreakdownLocalTime(ES_ImportedAPI::TimeElements* telts, double time)
{
    double localtime = OpDate::LocalTime(time);

    telts->year = OpDate::YearFromTime(localtime);
    telts->month = OpDate::MonthFromTime(localtime);
    telts->day_of_week = OpDate::WeekDay(localtime);
    telts->day_of_month = OpDate::DateFromTime(localtime);
    telts->hour = OpDate::HourFromTime(localtime);
    telts->minute = OpDate::MinFromTime(localtime);
    telts->second = OpDate::SecFromTime(localtime);
    telts->millisecond = (int)OpDate::msFromTime(localtime);
    return localtime;
}

/* static */ JString*
ES_DateBuiltins::TimeToLocaleString(ES_Context *context, double time)
{
    if (op_isnan(time))
        return JString::Make(context, invalid_date);

    uni_char string[128]; // ARRAY OK 2011-05-24 sof
    ES_ImportedAPI::TimeElements telts;
    double localtime = BreakdownLocalTime(&telts, time);

    OP_STATUS r = ES_ImportedAPI::FormatLocalTime(ES_ImportedAPI::GET_DATE_AND_TIME, string, ARRAY_SIZE(string), &telts);
    if (OpStatus::IsMemoryError(r))
        LEAVE(r);
    else if (OpStatus::IsError(r))
    {
        int zoneoffset = DOUBLE2INT32((localtime - time) / msPerMinute);

        zoneoffset = zoneoffset % 60 + (zoneoffset / 60 * 100);

        if (uni_snprintf(string, ARRAY_SIZE(string),
#ifdef ZONENAME_IN_DATESTRING
                         UNI_L("%s %s %02d, %02d:%02d:%02d GMT%+05d (%s) %04d"),
#else
                         UNI_L("%s %s %02d, %02d:%02d:%02d GMT%+05d %04d"),
#endif
                         DAYS(OpDate::WeekDay(localtime)),
                         MONTHS(OpDate::MonthFromTime(localtime)),
                         OpDate::DateFromTime(localtime),
                         OpDate::HourFromTime(localtime),
                         OpDate::MinFromTime(localtime),
                         OpDate::SecFromTime(localtime),
                         zoneoffset,
#ifdef ZONENAME_IN_DATESTRING
                         uni_tzname(0),
#endif
                         OpDate::YearFromTime(localtime)) < 0)
            LEAVE(OpStatus::ERR_NO_MEMORY);
    }

    return JString::Make(context, string);
}

/* static */ JString*
ES_DateBuiltins::TimeToLocaleDateString(ES_Context *context, double time)
{
    if (op_isnan(time))
        return JString::Make(context, invalid_date);

    uni_char string[128]; // ARRAY OK 2011-05-24 sof
    ES_ImportedAPI::TimeElements telts;
    double localtime = BreakdownLocalTime(&telts, time);

    OP_STATUS r = ES_ImportedAPI::FormatLocalTime(ES_ImportedAPI::GET_DATE, string, ARRAY_SIZE(string), &telts);
    if (OpStatus::IsMemoryError(r))
        LEAVE(r);
    else if (OpStatus::IsError(r))
    {
        if (uni_snprintf(string, ARRAY_SIZE(string), UNI_L("%3s %3s %02d, %04d"),
                         DAYS(OpDate::WeekDay(localtime)),
                         MONTHS(OpDate::MonthFromTime(localtime)),
                         OpDate::DateFromTime(localtime),
                         OpDate::YearFromTime(localtime)) < 0)
            LEAVE(OpStatus::ERR_NO_MEMORY);
    }

    return JString::Make(context, string);
}
/* static */ JString*
ES_DateBuiltins::TimeToLocaleTimeString(ES_Context *context, double time)
{
    if (op_isnan(time))
        return JString::Make(context, invalid_date);

    uni_char string[128]; // ARRAY OK 2011-05-24 sof
    ES_ImportedAPI::TimeElements telts;
    double localtime = BreakdownLocalTime(&telts, time);

    OP_STATUS r = ES_ImportedAPI::FormatLocalTime(ES_ImportedAPI::GET_TIME, string, ARRAY_SIZE(string), &telts);
    if (OpStatus::IsMemoryError(r))
        LEAVE(r);
    else if (OpStatus::IsError(r))
    {
        int zoneoffset = DOUBLE2INT32((localtime - time) / msPerMinute);

        zoneoffset = zoneoffset % 60 + (zoneoffset / 60 * 100);

        if (uni_snprintf(string, ARRAY_SIZE(string),
                         UNI_L("%02d:%02d:%02d"),
                         OpDate::HourFromTime(localtime),
                         OpDate::MinFromTime(localtime),
                         OpDate::SecFromTime(localtime)) < 0)
            LEAVE(OpStatus::ERR_NO_MEMORY);
    }

    return JString::Make(context, string);
}

/* static */ JString*
ES_DateBuiltins::TimeToUTCString(ES_Context *context, double time)
{
    if (op_isnan(time))
        return JString::Make(context, invalid_date);

    uni_char string[128]; // ARRAY OK 2008-06-09 stanislavj

    // RFC 1123 format
    // The .3 is highly desirable and some sites depend on it, see bug #54175
    if (uni_snprintf(string, ARRAY_SIZE(string), UNI_L("%.3s, %02d %.3s %04d %02d:%02d:%02d GMT"),
                     DAYS(OpDate::WeekDay(time)),
                     OpDate::DateFromTime(time),
                     MONTHS(OpDate::MonthFromTime(time)),
                     OpDate::YearFromTime(time),
                     OpDate::HourFromTime(time),
                     OpDate::MinFromTime(time),
                     OpDate::SecFromTime(time)) < 0)
        LEAVE(OpStatus::ERR_NO_MEMORY);

    return JString::Make(context, string);
}

/* static */ JString*
ES_DateBuiltins::TimeToISOString(ES_Context *context, double time)
{
    if (op_isnan(time))
        return JString::Make(context, invalid_date);

    uni_char string[128]; // ARRAY OK 2008-06-09 stanislavj

    int milliseconds = int(op_truncate(op_fmod(time, 1000)));

    if (milliseconds < 0)
        milliseconds = 1000 + milliseconds;

    int year = OpDate::YearFromTime(time);

    const uni_char *sign_prefix = UNI_L("");
    BOOL extended_years;
    if (year > 9999)
    {
        sign_prefix = UNI_L("+");
        extended_years = TRUE;
    }
    else
    {
        extended_years = year < 0;
        if (extended_years)
        {
            sign_prefix = UNI_L("-");
            year = -year;
        }
    }

    if (uni_snprintf(string, ARRAY_SIZE(string),
                     extended_years ? UNI_L("%s%06d-%02d-%02dT%02d:%02d:%02d.%03dZ") : UNI_L("%s%04d-%02d-%02dT%02d:%02d:%02d.%03dZ"),
                     sign_prefix,
                     year,
                     OpDate::MonthFromTime(time)+1,
                     OpDate::DateFromTime(time),
                     OpDate::HourFromTime(time),
                     OpDate::MinFromTime(time),
                     OpDate::SecFromTime(time),
                     milliseconds) < 0)
        LEAVE(OpStatus::ERR_NO_MEMORY);

    return JString::Make(context, string);
}

static BOOL
StrictProcessThis(double &this_number, const ES_Value_Internal &this_value)
{
    if (this_value.IsObject() && this_value.GetObject()->IsDateObject())
    {
        ES_Date_Object *d = static_cast<ES_Date_Object *>(this_value.GetObject());
        this_number = d->GetValue();
    }
    else
        return FALSE;

    return TRUE;
}

static BOOL
StrictProcessThis(ES_Execution_Context *context, double &this_number, const ES_Value_Internal &this_value, BOOL &is_nan, BOOL ltime = FALSE)
{
    if (this_value.IsObject() && this_value.GetObject()->IsDateObject())
    {
        ES_Date_Object *d = static_cast<ES_Date_Object *>(this_value.GetObject());
        is_nan = d->IsInvalid();
        this_number = ltime ? d->GetLocalTime(context) : d->GetValue();
    }
    else
        return FALSE;

    return TRUE;
}

static BOOL
StrictCheckThis(const ES_Value_Internal &this_value)
{
    return this_value.IsObject() && this_value.GetObject()->IsDateObject();
}

static void
SetThis(double v, const ES_Value_Internal &this_value)
{
    static_cast<ES_Date_Object *>(this_value.GetObject())->SetValue(v);
}

static void
SetThisInvalid(const ES_Value_Internal &this_value, ES_Value_Internal *return_value)
{
    static_cast<ES_Date_Object *>(this_value.GetObject())->SetInvalid();

    return_value->SetNan();
}

/* static */ BOOL
ES_DateBuiltins::constructor_call(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    return_value->SetString(ES_SuspendedTTS(TimeToString, context).result);
    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::constructor_construct(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double utc;

    if (argc == 0)
        utc = ES_SuspendedGetCurrentTimeUTC(context).result;
    else if (argc == 1)
    {
        if (argv[0].IsString())
        {
            double local;
            if (!context->GetGlobalObject()->GetCachedParsedDate(argv[0].GetString(), utc, local))
            {
                ES_SuspendedParseDate parsed_date(context, StorageZ(context, argv[0].GetString()), TRUE);
                utc = parsed_date.utc;
                local = parsed_date.local;
                context->GetGlobalObject()->SetCachedParsedDate(argv[0].GetString(), utc, local);
            }

            return_value->SetObject(ES_Date_Object::Make(context, ES_GET_GLOBAL_OBJECT(), utc, local));
            return TRUE;
        }
        else
        {
            if (!argv[0].ToNumber(context))
                return FALSE;

            utc = OpDate::TimeClip(argv[0].GetNumAsDouble());
        }
    }
    else
    {
        if (!argv[0].ToNumber(context) || !argv[1].ToNumber(context))
            return FALSE;
        double date = 1;
        double hours = 0;
        double minutes = 0;
        double seconds = 0;
        double ms = 0;
        if (argc >= 3)
        {
            if (!argv[2].ToNumber(context))
                return FALSE;
            date = argv[2].GetNumAsDouble();
            if (argc >= 4)
            {
                if (!argv[3].ToNumber(context))
                    return FALSE;
                hours = argv[3].GetNumAsDouble();
                if (argc >= 5)
                {
                    if (!argv[4].ToNumber(context))
                        return FALSE;
                    minutes = argv[4].GetNumAsDouble();
                    if (argc >= 6)
                    {
                        if (!argv[5].ToNumber(context))
                            return FALSE;
                        seconds = argv[5].GetNumAsDouble();
                        if (argc >= 7)
                        {
                            if (!argv[6].ToNumber(context))
                                return FALSE;
                            ms = argv[6].GetNumAsDouble();
                        }
                    }
                }
            }
        }
        double year = argv[0].GetNumAsDouble();
        if (!op_isnan(year))
        {
            year = argv[0].GetNumAsInteger();
            if (year >= 0 && year <= 99)
                year += 1900;
        }
        double month = argv[1].GetNumAsDouble();
        double day = OpDate::MakeDay(year, month, date);
        double t = OpDate::MakeTime(hours, minutes, seconds, ms);
        utc = OpDate::TimeClip(ES_SuspendedUTC(context, OpDate::MakeDate(day, t)).result);
    }

    return_value->SetObject(ES_Date_Object::Make(context, ES_GET_GLOBAL_OBJECT(), utc));
    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::valueOf(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;

    if (!StrictProcessThis(t, argv[-2]))
    {
        context->ThrowTypeError("Date.prototype.valueOf: this is not a Date object");
        return FALSE;
    }

    return_value->SetNumber(t);

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::toString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;

    if (!StrictProcessThis(t, argv[-2]))
    {
        context->ThrowTypeError("Date.prototype.toString: this is not a Date object");
        return FALSE;
    }
    ES_CollectorLock gclock(context);

    return_value->SetString(ES_SuspendedTTS(TimeToString, context, t).result);

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::toDateString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;

    if (!StrictProcessThis(t, argv[-2]))
    {
        context->ThrowTypeError("Date.prototype.toDateString: this is not a Date object");
        return FALSE;
    }

    ES_CollectorLock gclock(context);

    return_value->SetString(ES_SuspendedTTS(TimeToDateString, context, t).result);

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::toTimeString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;

    if (!StrictProcessThis(t, argv[-2]))
    {
        context->ThrowTypeError("Date.prototype.toTimeString: this is not a Date object");
        return FALSE;
    }

    ES_CollectorLock gclock(context);

    return_value->SetString(ES_SuspendedTTS(TimeToTimeString, context, t).result);

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::toLocaleString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;

    if (!StrictProcessThis(t, argv[-2]))
    {
        context->ThrowTypeError("Date.prototype.toLocaleString: this is not a Date object");
        return FALSE;
    }

    ES_CollectorLock gclock(context);

    return_value->SetString(ES_SuspendedTTS(TimeToLocaleString, context, t).result);

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::toLocaleDateString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;

    if (!StrictProcessThis(t, argv[-2]))
    {
        context->ThrowTypeError("Date.prototype.toLocaleDateString: this is not a Date object");
        return FALSE;
    }

    ES_CollectorLock gclock(context);

    return_value->SetString(ES_SuspendedTTS(TimeToLocaleDateString, context, t).result);

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::toLocaleTimeString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;

    if (!StrictProcessThis(t, argv[-2]))
    {
        context->ThrowTypeError("Date.prototype.toLocaleTimeString: this is not a Date object");
        return FALSE;
    }

    ES_CollectorLock gclock(context);

    return_value->SetString(ES_SuspendedTTS(TimeToLocaleTimeString, context, t).result);

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::getTime(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;

    if (!StrictProcessThis(t, argv[-2]))
    {
        context->ThrowTypeError("Date.prototype.getTime: this is not a Date object");
        return FALSE;
    }

    return_value->SetNumber(t);
    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::getYear(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, TRUE))
    {
        context->ThrowTypeError("Date.prototype.getYear: this is not a Date object");
        return FALSE;
    }

    if (is_nan)
        return_value->SetNan();
    else
    {
        int year = OpDate::YearFromTime(t) - 1900;
        return_value->SetInt32(year);
    }

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::getFullYear(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, TRUE))
    {
        context->ThrowTypeError("Date.prototype.getFullYear: this is not a Date object");
        return FALSE;
    }

    if (is_nan)
        return_value->SetNan();
    else
    {
        int year = OpDate::YearFromTime(t);
        return_value->SetInt32(year);
    }

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::getUTCFullYear(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, FALSE))
    {
        context->ThrowTypeError("Date.prototype.getUTCFullYear: this is not a Date object");
        return FALSE;
    }

    if (is_nan)
        return_value->SetNan();
    else
    {
        int year = OpDate::YearFromTime(t);
        return_value->SetInt32(year);
    }

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::getMonth(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, TRUE))
    {
        context->ThrowTypeError("Date.prototype.getMonth: this is not a Date object");
        return FALSE;
    }

    if (is_nan)
        return_value->SetNan();
    else
    {
        int month = OpDate::MonthFromTime(t);
        return_value->SetInt32(month);
    }

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::getUTCMonth(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, FALSE))
    {
        context->ThrowTypeError("Date.prototype.getUTCMonth: this is not a Date object");
        return FALSE;
    }

    if (is_nan)
        return_value->SetNan();
    else
    {
        int month = OpDate::MonthFromTime(t);
        return_value->SetInt32(month);
    }

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::getDate(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, TRUE))
    {
        context->ThrowTypeError("Date.prototype.getDate: this is not a Date object");
        return FALSE;
    }

    if (is_nan)
        return_value->SetNan();
    else
    {
        int date = OpDate::DateFromTime(t);
        return_value->SetInt32(date);
    }

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::getUTCDate(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, FALSE))
    {
        context->ThrowTypeError("Date.prototype.getUTCDate: this is not a Date object");
        return FALSE;
    }

    if (is_nan)
        return_value->SetNan();
    else
    {
        int date = OpDate::DateFromTime(t);
        return_value->SetInt32(date);
    }

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::getDay(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, TRUE))
    {
        context->ThrowTypeError("Date.prototype.getDay: this is not a Date object");
        return FALSE;
    }

    if (is_nan)
        return_value->SetNan();
    else
    {
        int day = OpDate::WeekDay(t);
        return_value->SetInt32(day);
    }

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::getUTCDay(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, FALSE))
    {
        context->ThrowTypeError("Date.prototype.getUTCDay: this is not a Date object");
        return FALSE;
    }

    if (is_nan)
        return_value->SetNan();
    else
    {
        int day = OpDate::WeekDay(t);
        return_value->SetInt32(day);
    }

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::getHours(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, TRUE))
    {
        context->ThrowTypeError("Date.prototype.getHours: this is not a Date object");
        return FALSE;
    }

    if (is_nan)
        return_value->SetNan();
    else
    {
        int hours = OpDate::HourFromTime(t);
        return_value->SetInt32(hours);
    }

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::getUTCHours(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, FALSE))
    {
        context->ThrowTypeError("Date.prototype.getUTCHours: this is not a Date object");
        return FALSE;
    }

    if (is_nan)
        return_value->SetNan();
    else
    {
        int hours = OpDate::HourFromTime(t);
        return_value->SetInt32(hours);
    }

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::getMinutes(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, TRUE))
    {
        context->ThrowTypeError("Date.prototype.getMinutes: this is not a Date object");
        return FALSE;
    }

    if (is_nan)
        return_value->SetNan();
    else
    {
        int minutes = OpDate::MinFromTime(t);
        return_value->SetInt32(minutes);
    }

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::getUTCMinutes(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, FALSE))
    {
        context->ThrowTypeError("Date.prototype.getUTCMinutes: this is not a Date object");
        return FALSE;
    }

    if (is_nan)
        return_value->SetNan();
    else
    {
        int minutes = OpDate::MinFromTime(t);
        return_value->SetInt32(minutes);
    }

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::getSeconds(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, TRUE))
    {
        context->ThrowTypeError("Date.prototype.getSeconds: this is not a Date object");
        return FALSE;
    }

    if (is_nan)
        return_value->SetNan();
    else
    {
        int seconds = OpDate::SecFromTime(t);
        return_value->SetInt32(seconds);
    }

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::getUTCSeconds(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, FALSE))
    {
        context->ThrowTypeError("Date.prototype.getUTCSeconds: this is not a Date object");
        return FALSE;
    }

    if (is_nan)
        return_value->SetNan();
    else
    {
        int seconds = OpDate::SecFromTime(t);
        return_value->SetInt32(seconds);
    }

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::getMilliseconds(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, TRUE))
    {
        context->ThrowTypeError("Date.prototype.getMilliseconds: this is not a Date object");
        return FALSE;
    }

    if (is_nan)
        return_value->SetNan();
    else
    {
        int ms = static_cast<int>(OpDate::msFromTime(t));
        return_value->SetInt32(ms);
    }

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::getUTCMilliseconds(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, FALSE))
    {
        context->ThrowTypeError("Date.prototype.getUTCMilliseconds: this is not a Date object");
        return FALSE;
    }

    if (is_nan)
        return_value->SetNan();
    else
    {
        int ms = static_cast<int>(OpDate::msFromTime(t));
        return_value->SetInt32(ms);
    }

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::getTimezoneOffset(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, FALSE))
    {
        context->ThrowTypeError("Date.prototype.getTimezoneOffset: this is not a Date object");
        return FALSE;
    }

    if (is_nan)
        return_value->SetNan();
    else
    {
        int zoneoffset = static_cast<int>((t - static_cast<ES_Date_Object *>(argv[-2].GetObject())->GetLocalTime(context)) / msPerMinute);
        return_value->SetInt32(zoneoffset);
    }
    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::setTime(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (!StrictCheckThis(argv[-2]))
    {
        context->ThrowTypeError("Date.prototype.setTime: this is not a Date object");
        return FALSE;
    }

    if (argc > 0 && !argv[0].ToNumber(context))
        return FALSE;

    if (argc >= 1)
    {
        double t = argv[0].GetNumAsDouble();
        t = OpDate::TimeClip(t);
        SetThis(t, argv[-2]);
        return_value->SetNumber(t);
    }
    else
        SetThisInvalid(argv[-2], return_value);

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::setMilliseconds(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, TRUE))
    {
        context->ThrowTypeError("Date.prototype.setMilliseconds: this is not a Date object");
        return FALSE;
    }

    if (argc > 0 && !argv[0].ToNumber(context))
        return FALSE;

    if (argc >= 1 && !is_nan)
    {
        double t2 = OpDate::MakeTime(OpDate::HourFromTime(t), OpDate::MinFromTime(t),
                                     OpDate::SecFromTime(t), argv[0].GetNumAsDouble());
        t = ES_SuspendedUTC(context, OpDate::MakeDate(OpDate::Day(t), t2)).result;
        t = OpDate::TimeClip(t);
        SetThis(t, argv[-2]);
        return_value->SetNumber(t);
    }
    else
        SetThisInvalid(argv[-2], return_value);

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::setUTCMilliseconds(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, FALSE))
    {
        context->ThrowTypeError("Date.prototype.setUTCMilliseconds: this is not a Date object");
        return FALSE;
    }

    if (argc > 0 && !argv[0].ToNumber(context))
        return FALSE;

    if (argc >= 1 && !is_nan)
    {
        double t2 = OpDate::MakeTime(OpDate::HourFromTime(t), OpDate::MinFromTime(t),
                                     OpDate::SecFromTime(t), argv[0].GetNumAsDouble());
        t = OpDate::MakeDate(OpDate::Day(t), t2);
        t = OpDate::TimeClip(t);
        SetThis(t, argv[-2]);
        return_value->SetNumber(t);
    }
    else
        SetThisInvalid(argv[-2], return_value);

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::setSeconds(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, TRUE))
    {
        context->ThrowTypeError("Date.prototype.setSeconds: this is not a Date object");
        return FALSE;
    }

    for (unsigned i = 0; i < MIN(2, argc); i++)
        if (!argv[i].ToNumber(context))
            return FALSE;

    if (argc >= 1 && !is_nan)
    {
        double ms = 0;
        if (argc >= 2)
            ms = argv[1].GetNumAsDouble();
        else
            ms = OpDate::msFromTime(t);

        double t2 = OpDate::MakeTime(OpDate::HourFromTime(t), OpDate::MinFromTime(t),
                                     argv[0].GetNumAsDouble(), ms);
        t = ES_SuspendedUTC(context, OpDate::MakeDate(OpDate::Day(t), t2)).result;
        t = OpDate::TimeClip(t);
        SetThis(t, argv[-2]);
        return_value->SetNumber(t);
    }
    else
        SetThisInvalid(argv[-2], return_value);

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::setUTCSeconds(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, FALSE))
    {
        context->ThrowTypeError("Date.prototype.setUTCSeconds: this is not a Date object");
        return FALSE;
    }

    for (unsigned i = 0; i < MIN(2, argc); i++)
        if (!argv[i].ToNumber(context))
            return FALSE;

    if (argc >= 1 && !is_nan)
    {
        double ms = 0;
        if (argc >= 2)
            ms = argv[1].GetNumAsDouble();
        else
            ms = OpDate::msFromTime(t);

        double t2 = OpDate::MakeTime(OpDate::HourFromTime(t), OpDate::MinFromTime(t),
                                     argv[0].GetNumAsDouble(), ms);
        t = OpDate::MakeDate(OpDate::Day(t), t2);
        t = OpDate::TimeClip(t);
        SetThis(t, argv[-2]);
        return_value->SetNumber(t);
    }
    else
        SetThisInvalid(argv[-2], return_value);

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::setMinutes(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, TRUE))
    {
        context->ThrowTypeError("Date.prototype.setMinutes: this is not a Date object");
        return FALSE;
    }

    for (unsigned i = 0; i < MIN(3, argc); i++)
        if (!argv[i].ToNumber(context))
            return FALSE;

    if (argc >= 1 && !is_nan)
    {
        double s = 0;
        double ms = 0;
        if (argc >= 2)
        {
            s = argv[1].GetNumAsDouble();
            if (argc >= 3)
                ms = argv[2].GetNumAsDouble();
            else
                ms = OpDate::msFromTime(t);
        }
        else
        {
            s = OpDate::SecFromTime(t);
            ms = OpDate::msFromTime(t);
        }

        double t2 = OpDate::MakeTime(OpDate::HourFromTime(t), argv[0].GetNumAsDouble(), s, ms);
        t = ES_SuspendedUTC(context, OpDate::MakeDate(OpDate::Day(t), t2)).result;
        t = OpDate::TimeClip(t);
        SetThis(t, argv[-2]);
        return_value->SetNumber(t);
    }
    else
        SetThisInvalid(argv[-2], return_value);

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::setUTCMinutes(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, FALSE))
    {
        context->ThrowTypeError("Date.prototype.setUTCMinutes: this is not a Date object");
        return FALSE;
    }

    for (unsigned i = 0; i < MIN(3, argc); i++)
        if (!argv[i].ToNumber(context))
            return FALSE;

    if (argc >= 1 && !op_isnan(t))
    {
        double s = 0;
        double ms = 0;
        if (argc >= 2)
        {
            s = argv[1].GetNumAsDouble();
            if (argc >= 3)
                ms = argv[2].GetNumAsDouble();
            else
                ms = OpDate::msFromTime(t);
        }
        else
        {
            s = OpDate::SecFromTime(t);
            ms = OpDate::msFromTime(t);
        }

        double t2 = OpDate::MakeTime(OpDate::HourFromTime(t), argv[0].GetNumAsDouble(),
                                     s, ms);
        t = OpDate::MakeDate(OpDate::Day(t), t2);
        t = OpDate::TimeClip(t);
        SetThis(t, argv[-2]);
        return_value->SetNumber(t);
    }
    else
        SetThisInvalid(argv[-2], return_value);

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::setHours(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, TRUE))
    {
        context->ThrowTypeError("Date.prototype.setHours: this is not a Date object");
        return FALSE;
    }

    for (unsigned i = 0; i < MIN(4, argc); i++)
        if (!argv[i].ToNumber(context))
            return FALSE;

    if (argc >= 1 && !is_nan)
    {
        double m = 0;
        double s = 0;
        double ms = 0;
        if (argc >= 2)
        {
            m = argv[1].GetNumAsDouble();
            if (argc >= 3)
            {
                s = argv[2].GetNumAsDouble();
                if (argc >= 4)
                    ms = argv[3].GetNumAsDouble();
                else
                    ms = OpDate::msFromTime(t);
            }
            else
            {
                s = OpDate::SecFromTime(t);
                ms = OpDate::msFromTime(t);
            }
        }
        else
        {
            m = OpDate::MinFromTime(t);
            s = OpDate::SecFromTime(t);
            ms = OpDate::msFromTime(t);
        }

        double t2 = OpDate::MakeTime(argv[0].GetNumAsDouble(), m, s, ms);
        t = ES_SuspendedUTC(context, OpDate::MakeDate(OpDate::Day(t), t2)).result;
        t = OpDate::TimeClip(t);
        SetThis(t, argv[-2]);
        return_value->SetNumber(t);
    }
    else
        SetThisInvalid(argv[-2], return_value);

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::setUTCHours(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, FALSE))
    {
        context->ThrowTypeError("Date.prototype.setUTCHours: this is not a Date object");
        return FALSE;
    }

    for (unsigned i = 0; i < MIN(4, argc); i++)
        if (!argv[i].ToNumber(context))
            return FALSE;

    if (argc >= 1 && !is_nan)
    {
        double m = 0;
        double s = 0;
        double ms = 0;
        if (argc >= 2)
        {
            m = argv[1].GetNumAsDouble();
            if (argc >= 3)
            {
                s = argv[2].GetNumAsDouble();
                if (argc >= 4)
                    ms = argv[3].GetNumAsDouble();
                else
                    ms = OpDate::msFromTime(t);
            }
            else
            {
                s = OpDate::SecFromTime(t);
                ms = OpDate::msFromTime(t);
            }
        }
        else
        {
            m = OpDate::MinFromTime(t);
            s = OpDate::SecFromTime(t);
            ms = OpDate::msFromTime(t);
        }

        double t2 = OpDate::MakeTime(argv[0].GetNumAsDouble(), m, s, ms);
        t = OpDate::MakeDate(OpDate::Day(t), t2);
        t = OpDate::TimeClip(t);
        SetThis(t, argv[-2]);
        return_value->SetNumber(t);
    }
    else
        SetThisInvalid(argv[-2], return_value);

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::setDate(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, TRUE))
    {
        context->ThrowTypeError("Date.prototype.setDate: this is not a Date object");
        return FALSE;
    }

    if (argc > 0 && !argv[0].ToNumber(context))
        return FALSE;

    if (argc >= 1 && !is_nan)
    {
        double d = OpDate::MakeDay(OpDate::YearFromTime(t), OpDate::MonthFromTime(t), argv[0].GetNumAsDouble());
        t = ES_SuspendedUTC(context, OpDate::MakeDate(d, OpDate::TimeWithinDay(t))).result;
        t = OpDate::TimeClip(t);
        SetThis(t, argv[-2]);
        return_value->SetNumber(t);
    }
    else
        SetThisInvalid(argv[-2], return_value);

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::setUTCDate(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, FALSE))
    {
        context->ThrowTypeError("Date.prototype.setUTCDate: this is not a Date object");
        return FALSE;
    }

    if (argc > 0 && !argv[0].ToNumber(context))
        return FALSE;

    if (argc >= 1 && !is_nan)
    {
        double d = OpDate::MakeDay(OpDate::YearFromTime(t), OpDate::MonthFromTime(t),
                                   argv[0].GetNumAsDouble());
        t = OpDate::MakeDate(d, OpDate::TimeWithinDay(t));
        t = OpDate::TimeClip(t);
        SetThis(t, argv[-2]);
        return_value->SetNumber(t);
    }
    else
        SetThisInvalid(argv[-2], return_value);

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::setMonth(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, TRUE))
    {
        context->ThrowTypeError("Date.prototype.setMonth: this is not a Date object");
        return FALSE;
    }

    for (unsigned i = 0; i < MIN(2, argc); i++)
        if (!argv[i].ToNumber(context))
            return FALSE;

    if (argc >= 1 && !is_nan)
    {
        double date = 1;
        if (argc >= 2)
            date = argv[1].GetNumAsDouble();
        else
            date = OpDate::DateFromTime(t);

        double d = OpDate::MakeDay(OpDate::YearFromTime(t), argv[0].GetNumAsDouble(), date);
        t = ES_SuspendedUTC(context, OpDate::MakeDate(d, OpDate::TimeWithinDay(t))).result;
        t = OpDate::TimeClip(t);
        SetThis(t, argv[-2]);
        return_value->SetNumber(t);
    }
    else
        SetThisInvalid(argv[-2], return_value);

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::setUTCMonth(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, FALSE))
    {
        context->ThrowTypeError("Date.prototype.setUTCMonth: this is not a Date object");
        return FALSE;
    }

    for (unsigned i = 0; i < MIN(2, argc); i++)
        if (!argv[i].ToNumber(context))
            return FALSE;

    if (argc >= 1 && !is_nan)
    {
        double date = 1;
        if (argc >= 2)
            date = argv[1].GetNumAsDouble();
        else
            date = OpDate::DateFromTime(t);

        double d = OpDate::MakeDay(OpDate::YearFromTime(t), argv[0].GetNumAsDouble(),
                                   date);
        t = OpDate::MakeDate(d, OpDate::TimeWithinDay(t));
        t = OpDate::TimeClip(t);
        SetThis(t, argv[-2]);
        return_value->SetNumber(t);
    }
    else
        SetThisInvalid(argv[-2], return_value);

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::setYear(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, TRUE))
    {
        context->ThrowTypeError("Date.prototype.setYear: this is not a Date object");
        return FALSE;
    }

    if (argc > 0 && !argv[0].ToNumber(context))
        return FALSE;

    if (argc >= 1)
    {
        double month = 0;
        double date = 1;
        if (!is_nan)
        {
            month = OpDate::MonthFromTime(t);
            date  = OpDate::DateFromTime(t);
        }
        double year = argv[0].GetNumAsInteger();
        if (0 <= year && year <= 99)
            year += 1900;
        else
            year = argv[0].GetNumAsDouble();
        double d = OpDate::MakeDay(year, month, date);
        double time_within_day = is_nan ? 0 : OpDate::TimeWithinDay(t);
        t = ES_SuspendedUTC(context, OpDate::MakeDate(d, time_within_day)).result;
        t = OpDate::TimeClip(t);
        SetThis(t, argv[-2]);
        return_value->SetNumber(t);
    }
    else
        SetThisInvalid(argv[-2], return_value);

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::setFullYear(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, TRUE))
    {
        context->ThrowTypeError("Date.prototype.setFullYear: this is not a Date object");
        return FALSE;
    }

    for (unsigned i = 0; i < MIN(3, argc); i++)
        if (!argv[i].ToNumber(context))
            return FALSE;

    if (argc >= 1)
    {
        double month = 0;
        double date = 1;
        if (argc >= 2)
        {
            month = argv[1].GetNumAsDouble();
            if (argc >= 3)
                date = argv[2].GetNumAsDouble();
            else if (!is_nan)
                date = OpDate::DateFromTime(t);
        }
        else
        {
            if (!is_nan)
            {
                date = OpDate::DateFromTime(t);
                month = OpDate::MonthFromTime(t);
            }
        }
        double d = OpDate::MakeDay(argv[0].GetNumAsDouble(), month, date);
        double time_within_day = is_nan ? 0 : OpDate::TimeWithinDay(t);
        t = ES_SuspendedUTC(context, OpDate::MakeDate(d, time_within_day)).result;
        t = OpDate::TimeClip(t);
        SetThis(t, argv[-2]);
        return_value->SetNumber(t);
    }
    else
        SetThisInvalid(argv[-2], return_value);

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::setUTCFullYear(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;
    BOOL is_nan;

    if (!StrictProcessThis(context, t, argv[-2], is_nan, FALSE))
    {
        context->ThrowTypeError("Date.prototype.setUTCFullYear: this is not a Date object");
        return FALSE;
    }

    for (unsigned i = 0; i < MIN(3, argc); i++)
        if (!argv[i].ToNumber(context))
            return FALSE;

    if (argc >= 1)
    {
        double month = 0;
        double date = 1;
        if (argc >= 2)
        {
            month = argv[1].GetNumAsDouble();
            if (argc >= 3)
                date = argv[2].GetNumAsDouble();
            else if (!is_nan)
                date = OpDate::DateFromTime(t);
        }
        else
        {
            if (!is_nan)
            {
                date = OpDate::DateFromTime(t);
                month = OpDate::MonthFromTime(t);
            }
        }
        double d = OpDate::MakeDay(argv[0].GetNumAsDouble(), month, date);
        double time_within_day = is_nan ? 0 : OpDate::TimeWithinDay(t);
        t = OpDate::MakeDate(d, time_within_day);
        t = OpDate::TimeClip(t);
        SetThis(t, argv[-2]);
        return_value->SetNumber(t);
    }
    else
        SetThisInvalid(argv[-2], return_value);

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::toUTCString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;

    if (!StrictProcessThis(t, argv[-2]))
    {
        context->ThrowTypeError("Date.prototype.toUTCString: this is not a Date object");
        return FALSE;
    }
    ES_CollectorLock gclock(context);

    return_value->SetString(ES_SuspendedTTS(TimeToUTCString, context, t).result);

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::toISOString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double t;

    if (!StrictProcessThis(t, argv[-2]))
    {
        context->ThrowTypeError("Date.prototype.toISOString: this is not a Date object");
        return FALSE;
    }
    ES_CollectorLock gclock(context);

    if (!op_isfinite(t))
    {
        context->ThrowRangeError("Date.prototype.toISOString: invalid time value");
        return FALSE;
    }

    return_value->SetString(ES_SuspendedTTS(TimeToISOString, context, t).result);

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::toJSON(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (!argv[-2].ToObject(context, FALSE))
    {
        context->ThrowTypeError("Date.prototype.toJSON: this is not an object");
        return FALSE;
    }

    ES_Value_Internal *value_registers = context->AllocateRegisters(1);
    ES_Value_Internal &o_value = value_registers[0]; o_value = argv[-2];

    if (!o_value.ToPrimitive(context, ES_Value_Internal::HintNumber))
    {
        context->FreeRegisters(1);
        return FALSE;
    }

    BOOL is_nonfinite = o_value.IsNumber() && !op_isfinite(o_value.GetNumAsDouble());

    context->FreeRegisters(1);

    if (is_nonfinite)
    {
        return_value->SetNull();
        return TRUE;
    }

    ES_Value_Internal toISOString_value;
    GetResult result = argv[-2].GetObject(context)->GetL(context, context->rt_data->idents[ESID_toISOString], toISOString_value);
    if (result == PROP_GET_FAILED)
        return FALSE;
    if (!GET_OK(result) || !toISOString_value.IsCallable(context))
    {
        context->ThrowTypeError("Date.prototype.toJSON: toISOString not callable");
        return FALSE;
    }
    ES_Value_Internal *registers = context->SetupFunctionCall(toISOString_value.GetObject(context), 0);

    registers[0] = argv[-2];
    registers[1] = toISOString_value;
    return context->CallFunction(registers, 0, return_value);
}

/* static */ BOOL
ES_DateBuiltins::parse(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc >= 1)
    {
        if (!argv[0].ToString(context))
            return FALSE;
        return_value->SetNumber(ES_SuspendedParseDate(context, StorageZ(context, argv[0].GetString()), FALSE).utc);
    }
    else
        return_value->SetNull();

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::UTC(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{

    for (unsigned i = 0; i < MIN(7, argc); i++)
        if (!argv[i].ToNumber(context))
            return FALSE;

    if (argc >= 2)
    {
        double date = 1;
        double hours = 0;
        double minutes = 0;
        double seconds = 0;
        double ms = 0;
        if (argc >= 3)
        {
            date = argv[2].GetNumAsDouble();
            if (argc >= 4)
            {
                hours = argv[3].GetNumAsDouble();
                if (argc >= 5)
                {
                    minutes = argv[4].GetNumAsDouble();
                    if (argc >= 6)
                    {
                        seconds = argv[5].GetNumAsDouble();
                        if (argc >= 7)
                            ms = argv[6].GetNumAsDouble();
                    }
                }
            }
        }
        double year = argv[0].GetNumAsDouble();
        if (!op_isnan(year))
        {
            year = argv[0].GetNumAsInteger();
            if (year >= 0 && year <= 99)
                year += 1900;
        }
        double month = argv[1].GetNumAsDouble();
        double day = OpDate::MakeDay(year, month, date);
        double t = OpDate::MakeTime(hours, minutes, seconds, ms);
        t = OpDate::TimeClip(OpDate::MakeDate(day, t));
        return_value->SetNumber(t);
    }
    else
        return_value->SetNan();

    return TRUE;
}

/* static */ BOOL
ES_DateBuiltins::now(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    return_value->SetNumber(ES_SuspendedGetCurrentTimeUTC(context).result);
    return TRUE;
}

/* static */ void
ES_DateBuiltins::PopulatePrototype(ES_Context *context, ES_Global_Object *global_object, ES_Object *prototype)
{
    ES_Value_Internal undefined;

    ASSERT_CLASS_SIZE(ES_DateBuiltinsCount);

    APPEND_PROPERTY(ES_DateBuiltins, constructor,           undefined);
    APPEND_PROPERTY(ES_DateBuiltins, valueOf,               MAKE_BUILTIN(0, valueOf));
    APPEND_PROPERTY(ES_DateBuiltins, toString,              MAKE_BUILTIN(0, toString));
    APPEND_PROPERTY(ES_DateBuiltins, toDateString,          MAKE_BUILTIN(0, toDateString));
    APPEND_PROPERTY(ES_DateBuiltins, toTimeString,          MAKE_BUILTIN(0, toTimeString));
    APPEND_PROPERTY(ES_DateBuiltins, toLocaleString,        MAKE_BUILTIN(0, toLocaleString));
    APPEND_PROPERTY(ES_DateBuiltins, toLocaleDateString,    MAKE_BUILTIN(0, toLocaleDateString));
    APPEND_PROPERTY(ES_DateBuiltins, toLocaleTimeString,    MAKE_BUILTIN(0, toLocaleTimeString));
    APPEND_PROPERTY(ES_DateBuiltins, getTime,               MAKE_BUILTIN(0, getTime));
    APPEND_PROPERTY(ES_DateBuiltins, getYear,               MAKE_BUILTIN(0, getYear));
    APPEND_PROPERTY(ES_DateBuiltins, getFullYear,           MAKE_BUILTIN(0, getFullYear));
    APPEND_PROPERTY(ES_DateBuiltins, getUTCFullYear,        MAKE_BUILTIN(0, getUTCFullYear));
    APPEND_PROPERTY(ES_DateBuiltins, getMonth,              MAKE_BUILTIN(0, getMonth));
    APPEND_PROPERTY(ES_DateBuiltins, getUTCMonth,           MAKE_BUILTIN(0, getUTCMonth));
    APPEND_PROPERTY(ES_DateBuiltins, getDate,               MAKE_BUILTIN(0, getDate));
    APPEND_PROPERTY(ES_DateBuiltins, getUTCDate,            MAKE_BUILTIN(0, getUTCDate));
    APPEND_PROPERTY(ES_DateBuiltins, getDay,                MAKE_BUILTIN(0, getDay));
    APPEND_PROPERTY(ES_DateBuiltins, getUTCDay,             MAKE_BUILTIN(0, getUTCDay));
    APPEND_PROPERTY(ES_DateBuiltins, getHours,              MAKE_BUILTIN(0, getHours));
    APPEND_PROPERTY(ES_DateBuiltins, getUTCHours,           MAKE_BUILTIN(0, getUTCHours));
    APPEND_PROPERTY(ES_DateBuiltins, getMinutes,            MAKE_BUILTIN(0, getMinutes));
    APPEND_PROPERTY(ES_DateBuiltins, getUTCMinutes,         MAKE_BUILTIN(0, getUTCMinutes));
    APPEND_PROPERTY(ES_DateBuiltins, getSeconds,            MAKE_BUILTIN(0, getSeconds));
    APPEND_PROPERTY(ES_DateBuiltins, getUTCSeconds,         MAKE_BUILTIN(0, getUTCSeconds));
    APPEND_PROPERTY(ES_DateBuiltins, getMilliseconds,       MAKE_BUILTIN(0, getMilliseconds));
    APPEND_PROPERTY(ES_DateBuiltins, getUTCMilliseconds,    MAKE_BUILTIN(0, getUTCMilliseconds));
    APPEND_PROPERTY(ES_DateBuiltins, getTimezoneOffset,     MAKE_BUILTIN(0, getTimezoneOffset));
    APPEND_PROPERTY(ES_DateBuiltins, setTime,               MAKE_BUILTIN(1, setTime));
    APPEND_PROPERTY(ES_DateBuiltins, setMilliseconds,       MAKE_BUILTIN(1, setMilliseconds));
    APPEND_PROPERTY(ES_DateBuiltins, setUTCMilliseconds,    MAKE_BUILTIN(1, setUTCMilliseconds));
    APPEND_PROPERTY(ES_DateBuiltins, setSeconds,            MAKE_BUILTIN(2, setSeconds));
    APPEND_PROPERTY(ES_DateBuiltins, setUTCSeconds,         MAKE_BUILTIN(2, setUTCSeconds));
    APPEND_PROPERTY(ES_DateBuiltins, setMinutes,            MAKE_BUILTIN(3, setMinutes));
    APPEND_PROPERTY(ES_DateBuiltins, setUTCMinutes,         MAKE_BUILTIN(3, setUTCMinutes));
    APPEND_PROPERTY(ES_DateBuiltins, setHours,              MAKE_BUILTIN(4, setHours));
    APPEND_PROPERTY(ES_DateBuiltins, setUTCHours,           MAKE_BUILTIN(4, setUTCHours));
    APPEND_PROPERTY(ES_DateBuiltins, setDate,               MAKE_BUILTIN(1, setDate));
    APPEND_PROPERTY(ES_DateBuiltins, setUTCDate,            MAKE_BUILTIN(1, setUTCDate));
    APPEND_PROPERTY(ES_DateBuiltins, setMonth,              MAKE_BUILTIN(2, setMonth));
    APPEND_PROPERTY(ES_DateBuiltins, setUTCMonth,           MAKE_BUILTIN(2, setUTCMonth));
    APPEND_PROPERTY(ES_DateBuiltins, setYear,               MAKE_BUILTIN(1, setYear));
    APPEND_PROPERTY(ES_DateBuiltins, setFullYear,           MAKE_BUILTIN(3, setFullYear));
    APPEND_PROPERTY(ES_DateBuiltins, setUTCFullYear,        MAKE_BUILTIN(3, setUTCFullYear));
    APPEND_PROPERTY(ES_DateBuiltins, toUTCString,           MAKE_BUILTIN(0, toUTCString));
    APPEND_PROPERTY(ES_DateBuiltins, toGMTString,           MAKE_BUILTIN_WITH_NAME(0, toUTCString, toGMTString)); // Alias for toUTCString
    APPEND_PROPERTY(ES_DateBuiltins, toISOString,           MAKE_BUILTIN(0, toISOString));
    APPEND_PROPERTY(ES_DateBuiltins, toJSON,                MAKE_BUILTIN(1, toJSON));

    ASSERT_OBJECT_COUNT(ES_DateBuiltins);
}

/* static */ void
ES_DateBuiltins::PopulatePrototypeClass(ES_Context *context, ES_Class_Singleton *prototype_class)
{
    OP_ASSERT(prototype_class->GetPropertyTable()->Capacity() >= ES_DateBuiltinsCount);

    JString **idents = context->rt_data->idents;
    ES_Layout_Info layout;

    DECLARE_PROPERTY(ES_DateBuiltins, constructor,          DE, ES_STORAGE_WHATEVER);
    DECLARE_PROPERTY(ES_DateBuiltins, valueOf,              DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, toString,             DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, toDateString,         DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, toTimeString,         DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, toLocaleString,       DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, toLocaleDateString,   DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, toLocaleTimeString,   DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, getTime,              DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, getYear,              DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, getFullYear,          DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, getUTCFullYear,       DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, getMonth,             DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, getUTCMonth,          DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, getDate,              DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, getUTCDate,           DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, getDay,               DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, getUTCDay,            DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, getHours,             DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, getUTCHours,          DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, getMinutes,           DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, getUTCMinutes,        DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, getSeconds,           DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, getUTCSeconds,        DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, getMilliseconds,      DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, getUTCMilliseconds,   DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, getTimezoneOffset,    DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, setTime,              DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, setMilliseconds,      DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, setUTCMilliseconds,   DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, setSeconds,           DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, setUTCSeconds,        DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, setMinutes,           DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, setUTCMinutes,        DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, setHours,             DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, setUTCHours,          DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, setDate,              DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, setUTCDate,           DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, setMonth,             DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, setUTCMonth,          DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, setYear,              DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, setFullYear,          DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, setUTCFullYear,       DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, toUTCString,          DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, toGMTString,          DE, ES_STORAGE_OBJECT); // Alias for toUTCString
    DECLARE_PROPERTY(ES_DateBuiltins, toISOString,          DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DateBuiltins, toJSON,               DE, ES_STORAGE_OBJECT); // Alias for toUTCString
}

/* static */ void
ES_DateBuiltins::PopulateConstructor(ES_Context *context, ES_Global_Object *global_object, ES_Object *constructor)
{
    JString **idents = context->rt_data->idents;
    constructor->InitPropertyL(context, idents[ESID_parse], MAKE_BUILTIN(1, parse), DE);
    constructor->InitPropertyL(context, idents[ESID_UTC], MAKE_BUILTIN(7, UTC), DE);
    constructor->InitPropertyL(context, idents[ESID_now], MAKE_BUILTIN(0, now), DE);
}
