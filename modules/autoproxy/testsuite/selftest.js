/* -*- mode: c++; tab-width: 4 -*-
 *
 * This code is loaded by autoproxy.ot during selftest runs and is used
 * to test parts of the autoproxy library.
 *
 * Tord Akerbæk, Lars T Hansen
 *
 * Does not test
 *   localHostOrDomainIs
 *   isResolvable
 *   dnsResolve
 *   myIpAddress
 *
 * Some tests in this file need to be expanded, see comments COMPLETEME below.
 *
 * Time-dependent tests should probably be rewritten to time-independent ones.
 * We don't want tests that fail one day and not the next, plus that it is
 * difficult to check border cases without this.  -jhoff
 *
 */

var OK = "OK";
var ERROR = "ERROR";

function FindProxyForURL(url, host)
{
    switch (url)
    {
    case "isPlainHostName": return isPlainHostNameTest(host);
    case "dnsDomainIs":     return dnsDomainIsTest(host);
    case "dnsDomainLevels": return dnsDomainLevelsTest(host);
    case "shExpMatch":      return shExpMatchTest(host);
    case "weekdayRange":    return weekdayRangeTest(host);
    case "dateRange":       return dateRangeTest(host);
    case "timeRange":       return timeRangeTest(host);
    default:                return "ERROR: unexpected key " + url;
    }
}

function isPlainHostNameTest(key)
{
    switch(key)
    {
    case "1": return isPlainHostName("www") ? OK : ERROR;
    case "2": return isPlainHostName("www.netscape.com") ? ERROR : OK;
    default:  return "ERROR: unexpected key: " + key;
    }
}

function dnsDomainIsTest(key)
{
    switch(key)
    {
    case "1": return dnsDomainIs("www.opera.com",".opera.com") ? OK : ERROR;
    case "2": return dnsDomainIs("www",".opera.com") ? ERROR : OK;
    case "3": return dnsDomainIs("www.microsoft.com",".opera.com") ? ERROR : OK;
    default:  return "ERROR: unexpected key: " + key;
    }
}

function dnsDomainLevelsTest(key)
{
    switch(key)
    {
    case "1": return dnsDomainLevels("www") == 0 ? OK : ERROR;
    case "2": return dnsDomainLevels("www.opera.com") == 2 ? OK: ERROR;
    default:  return "ERROR: unexpected key: " + key;
    }
}

function shExpMatchTest(key)
{
    switch(key)
    {
    case "1": return shExpMatch("http://web.opera.com/people/lars/index.html","*/lars/*") ? OK : ERROR;
    case "2": return shExpMatch("index.html","*.*") ? OK : ERROR;
    case "3": return shExpMatch("shttp://web.opera.com/people/johan/index.html","shttp*.*") ? OK : ERROR;
    case "4": return shExpMatch("http://web.opera.com/people/johan/index.html","*people/johan*.*") ? OK : ERROR;
    case "5": return shExpMatch("http://web.opera.com/","http://w??.opera.com/") ? OK : ERROR;
    case "6": return shExpMatch("arg=2kilo","arg=[0123]kilo") ? OK : ERROR;
    case "7": return shExpMatch("arg=3kilo","arg=[0-9]kilo") ? OK : ERROR;
    case "8": return shExpMatch("arg=B;","arg=[!A];") ? OK : ERROR;
    case "9": return shExpMatch("http://web.opera.com/people/johan/index.html","*/lars/*") ? ERROR : OK;
    case "A": return shExpMatch("http://web.opera.com/people/johan/index","*/johan/*.*") ? ERROR : OK;
    case "B": return shExpMatch("http://webb.opera.com/","http://w??.opera.com/") ? ERROR : OK;
    case "C": return shExpMatch("arg=Akilo","arg=[0123]kilo") ? ERROR : OK;
    case "D": return shExpMatch("arg=fkilo","arg=[0-9]kilo") ? ERROR : OK;
    case "E": return shExpMatch("arg=A;","arg=[!A];") ? ERROR : OK;
    default:  return "ERROR: unexpected key: " + key;
    }
}

function weekdayRangeTest(key) // COMPLETEME
{
    switch(key)
    {
    case "1": return weekdayRange("MON","FRI") != weekdayRange("SAT","SUN") ? OK : ERROR;
    case "2": return weekdayRange("MON","FRI","GMT") != weekdayRange("SAT","SUN","GMT") ? OK : ERROR;
    case "3": return weekdayRange("SAT") && weekdayRange("WED") ? ERROR : OK;
    case "4": return weekdayRange("SAT","GMT") && weekdayRange("THU","GMT") ? ERROR : OK;
    case "5": return weekdayRange("FRI","MON") == weekdayRange("TUE","THU") ? ERROR : OK;
    case "6": return weekdayRange("SAT","MON") == weekdayRange("TUE","FRI") ? ERROR : OK;
    case "7": return weekdayRange("THU","MON") == weekdayRange("TUE","WED") ? ERROR : OK;
    case "8": return weekdayRange("MON","WED") == weekdayRange("THU","SUN") ? ERROR : OK;
    default:  return "ERROR: unexpected key: " + key;
    }
}

function dateRangeTest(key)  // COMPLETEME
{
    var now = OPERA$getCurrentTime( false );
    var old_time_fn = OPERA$getCurrentTime;
    var res;

    switch (key)
    {
    case "1": return dateRange(1) != dateRange(2,31) ? OK : ERROR;
    case "2": return dateRange(1,"GMT") != dateRange(2,31,"GMT") ? OK : ERROR;
    case "3": return dateRange(1,15) == dateRange(16,31) ? ERROR : OK;
    case "4": return dateRange(1,"DEC") && dateRange("JAN","NOV") ? ERROR : OK;
    case "5": return dateRange(17,"MAY") ? "Arbeider du i dag?" : OK;
    case "6": return dateRange(18,"JAN") && dateRange("FEB","DEC") ? ERROR : OK;
    case "7": return dateRange("JAN","MAR") == dateRange("APR","DEC") ? ERROR : OK;
    case "8": return dateRange(1,"JAN",15,"AUG") == dateRange(16,"AUG",31,"DEC") ? ERROR : OK;
    case "9": return dateRange("OCT",2000,"MAY",2020) ? OK : ERROR;
    case "A": return dateRange(1995) ? ERROR : OK;
    case "B": return dateRange(1990,2000) ? ERROR : OK;
    case "C": return dateRange(2, 3) == dateRange(4, 1) ? ERROR : OK;
    case "D": return dateRange(2, 27) == dateRange(28, 1)  ? ERROR : OK;
    case "E": return dateRange("MAR", "MAY") == dateRange("JUN", "FEB") ? ERROR : OK;
    case "F": return dateRange("JUL", "SEP") == dateRange("OCT", "JUN") ? ERROR : OK;
    case "G": return dateRange(now.date) ? OK : ERROR;
    case "H": return dateRange("JUN", 2000, "MAY", 2005) ? ERROR : OK;
    case "I":
        // From bug 189397
        OPERA$getCurrentTime = function(is_GMT) { return { day:0, date:16, month:7-1, year:2005, hour:12, min:0, sec:0 }; };
        res = dateRange("JUL", 2004, "APR", 2005) ? ERROR : OK;
        OPERA$getCurrentTime = old_time_fn;
        return res;
    case "J":
        // From bug 188416
        OPERA$getCurrentTime = function(is_GMT) { return { day:0, date:5, month:9-1, year:2005, hour:12, min:0, sec:0 }; };
        res = dateRange(15, 1) ? ERROR : OK;
        OPERA$getCurrentTime = old_time_fn;
        return res;
    default:
		return "ERROR: unexpected key: " + key;
    }
}

function timeRangeTest(key) // COMPLETEME
{
    var old_time_fn = OPERA$getCurrentTime;
    var res;

    switch (key)
    {
    case "1": return timeRange(0,12) != timeRange(13,0) ? OK : ERROR;
    case "2": return timeRange(0) != timeRange(1,23) ? OK : ERROR;
    case "3": return timeRange(12) == timeRange(13,11) ? ERROR : OK;
    case "4": return timeRange(4,"GMT") == timeRange(5,3,"GMT") ? ERROR : OK;
    case "5": return timeRange(0,30,16,00) == timeRange(16,01,0,29) ? ERROR : OK;
    case "6": return timeRange(0,0,0,0,0,30) == timeRange(0,0,31,23,59,59) ? ERROR : OK;
    case "7":
        // From bug 189401
        OPERA$getCurrentTime = function(is_GMT) { return { day:0, date:1, month:1, year:2005, hour:13, min:10, sec:0 }; };
        res = timeRange(14, 10, 12, 20) ? ERROR : OK;
        OPERA$getCurrentTime = old_time_fn;
        return res;
    default : return "ERROR: unexpected key: " + key;
    }
}

