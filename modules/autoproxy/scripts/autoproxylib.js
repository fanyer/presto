/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2001 - 2004.
 *
 * JavaScript library -- to be included in the autoproxy module.
 * Lars T Hansen
 *
 *
 * Most of the utility functions are readily written in Javascript
 * (and some are a pain to write in C++).  Performance will suffer
 * a little, especially as the engine is created, but compared to
 * invoking the JS engine in the first place (and doing a name lookup)
 * it won't matter in most cases.  Revisit this decision later.
 *
 * OPERA$dnsResolve(), OPERA$myHostname(), and shExpMatch() are native.
 *
 *
 * This library is included in Opera at build time in source form or
 * compiled form.  Compiled form is preferred.  An #ifdef nest in
 * modules/autoproxy/autoproxy_lib.h selects the appropriate
 * version to use.
 *
 * When the script is included in source form, it is first compressed
 * by the tool in modules/ecmascript/tools/encode.html: copy the entire
 * contents of this file into the text area in that page, push the
 * button, and copy the result into modules/autoproxy/autoproxy_source.h.
 *
 * When the script is included in compiled form, there will generally
 * be one compiled representation per ECMAScript engine edition, and
 * the code to be used is selected at build time.  For example, for
 * the linear_b_2 edition, the compiled code resides in
 * modules/autoproxy/autoproxy_linear_b_2.cpp.  To generate the contents
 * of that file, load modules/autoproxy/scripts/compile_autoproxylib.html
 * into a debug build of an Opera version using linear_b_2, with
 * ECMASCRIPT_DISASSEMBLER enabled.  The compiled form of the script will
 * be presented as text in the browser window.  Copy it into the .cpp
 * file and edit it by hand as documented at the head of the .cpp file.
 */

//$INTEGRATE-USUAL-PROCEDURES

function OPERA$init()
{
	OPERA$daynumber = { SUN:0, MON:1, TUE:2, WED:3, THU:4, FRI:5, SAT:6 };
	OPERA$monthnumber = { JAN:0, FEB:1, MAR:2, APR:3, MAY:4, JUN:5, JUL:6, AUG:7, SEP:8, OCT:9, NOV:10, DEC:11 };
	ProxyConfig = new Object();
	ProxyConfig.bindings = new Array();
	OPERA$init = undefined;
}

function OPERA$getCurrentTime( use_UTC )
{
    var now = new Date();
    now.___value___ = $DATE_NOW();
    
    if (use_UTC)
        return { day:now.getUTCDay(), date:now.getUTCDate(), month:now.getUTCMonth(), 
                 year:now.getUTCFullYear(), hour:now.getUTCHours(), min:now.getUTCMinutes(), sec:now.getUTCSeconds() };
    else
        return { day:now.getDay(), date:now.getDate(), month:now.getMonth(), 
                 year:now.getFullYear(), hour:now.getHours(), min:now.getMinutes(), sec:now.getSeconds() };
}

function isPlainHostName( host )
{
    return host.indexOf('.') == -1;
}

function dnsDomainIs( host, domain )
{
    var lh = host.length;
    var ld = domain.length;
    return lh >= ld && host.substring( lh-ld ) == domain;
}

function localHostOrDomainIs( host, hostdom )
{
    if (host == hostdom)
        return true;
    if (host.indexOf( '.' ) != -1)
        return false;
    lh = host.length;
    lhd = hostdom.length;
    return lh <= lhd && host == hostdom.substring( 0, lh );
}

function isResolvable( host )
{
    if (dnsResolve( host ))
        return true;
    else
        return false;
}

function isInNet( host, pattern, mask )
{
    if (!host.match(/^\\d+\\.\\d+\\.\\d+\\.\\d+$/))
        host = dnsResolve( host );
    if (!host)
        return false;
    h = host.split('.');
    p = pattern.split('.');
    m = mask.split('.');
    for ( i=0; i < 4 ; i++ )
        if ((h[i] & m[i]) != (p[i] & m[i]))
            return false;
    return true;
}

/* The trick here is that OPERA$dnsResolve sets a flag that suspends execution
   following the statement it is called from, and saved state in the execution
   context allows the proxy configuration code to set up a callback that
   will continue execution after the name has been resolved asynchronously.

   OPERA$dnsResolve returns undefined if it suspends, false if it fails,
   and a string if it succeeds.

   Change this function only very, very carefully.
   */
function dnsResolve( host )
{
    var result = undefined;
    while (result === undefined)
        result = OPERA$dnsResolve( host );
    return result;
}

function myIpAddress()
{
    var attempt = dnsResolve( OPERA$myHostname() );
    return attempt ? attempt : '127.0.0.1';
}

function dnsDomainLevels( host )
{
    var n=0;
    var idx=0;
    while ( (idx = host.indexOf( '.', idx )) != -1 ) {
        n++;
        idx++;
    }
    return n;
}

function weekdayRange(/* varargs */)
{
    var wd1 = false, wd2 = false, GMT = false;

    switch (arguments.length) 
	{
    case 1:
        wd1 = arguments[0];
        break;
    case 2:
        wd1 = arguments[0];
        if (arguments[1] == 'GMT') GMT = true; else wd2 = arguments[1];
        break;
    case 3:
        if (arguments[2] != 'GMT') return false;
        GMT=true; wd1=arguments[0]; wd2=arguments[1];
        break;
    }

    var now = OPERA$getCurrentTime( GMT );

    if (!wd1 || !wd1 in OPERA$daynumber)
        return false;
    wd1 = OPERA$daynumber[wd1];
    if (!wd2 || !wd2 in OPERA$daynumber)
        return now.day == wd1;
    wd2 = OPERA$daynumber[wd2];
    if (wd1 <= wd2)
        return now.day >= wd1 && now.day <= wd2;
    else
        return now.day >= wd1 || now.day <= wd2;
}

function dateRange(/* varargs */)
{
    var timezone=false;

    function isDay( x ) { 
		return typeof(x) == 'number' && 0 < x && x <= 31 && isFinite(x) && Math.floor(x) == x; 
	}

    function isMonth( x ) { 
		return x in OPERA$monthnumber; 
	}

    function isYear( x ) { 
		return typeof(x) == 'number' && x > 31 && isFinite(x) && Math.floor(x) == x; 
	}

    function onearg( a ) {
        var now = OPERA$getCurrentTime( timezone );
        if (isDay( a ))
            return now.date == a;
        else if (isMonth( a ))
            return now.month == OPERA$monthnumber[a];
        else if (isYear( a ))
            return now.year == a;
        else
            return false;
    }

    function twoarg( a, b ) {
        var now = OPERA$getCurrentTime( timezone );
        if (isDay( a ) && isDay( b ))
            return a <= b ? (a <= now.date && now.date <= b) : (a <= now.date || now.date <= b);
        else if (isMonth( a ) && isMonth( b )) {
            var m1=OPERA$monthnumber[a], m2=OPERA$monthnumber[b];
            return m1 <= m2 ? (m1 <= now.month && now.month <= m2) : (m1 <= now.month || now.month <= m2);
        }
        else if (isYear( a ) && isYear( b ))
            return a <= now.year && now.year <= b;
        else
            return false;
    }

    function fourarg( a, b, c, d ) {
        var now = OPERA$getCurrentTime( timezone );
        if (isDay( a ) && isMonth( b ) && isDay( c ) && isMonth( d )) {
            var d1=a, m1=OPERA$monthnumber[b], d2=c, m2=OPERA$monthnumber[d];
            if (m1 < m2 || m1 == m2 && d1 <= d2)
                return (now.month > m1 || now.month == m1 && now.date >= d1) && (now.month < m2 || now.month == m2 && now.date <= d2);
            else
                return (now.month > m1 || now.month == m1 && now.date >= d1) || (now.month < m2 || now.month == m2 && now.date <= d2);
        }
        else if (isMonth( a ) && isYear( b ) && isMonth( c ) && isYear( d )) {
            var m1=OPERA$monthnumber[a], y1=b, m2=OPERA$monthnumber[c], y2=d;
            return (now.year > y1 || now.year == y1 && now.month >= m1) && (now.year < y2 || now.year == y2 && now.month <= m2);
        }
        else
            return false;
    }

    function sixarg( a, b, c, d, e, f ) {
        var now = OPERA$getCurrentTime( timezone );
        if (isDay( a ) && isMonth( b ) && isYear( c ) && isDay( d ) && isMonth( e ) && isYear( f )) {
            var m1=OPERA$monthnumber[b], m2=OPERA$monthnumber[e];
			var d1=a, d2=d;
			var y1=d, y2=f;

			return (now.year > y1 || now.year == y1 && (now.month > m1 || now.month == m1 && now.date >= d1)) && // now >= date1
				   (now.year < y1 || now.year == y2 && (now.month < m2 || now.month == m2 && now.date <= d2));   // now <= date2
        }
        else
            return false;
    }

    switch (arguments.length) 
	{
    case 2 :
        if (arguments[1] != 'GMT')
            return twoarg( arguments[0], arguments[1] );
        timezone=true;
    case 1 :
        return onearg( arguments[0] );
    case 3:
        if (arguments[2] != 'GMT')
            return false;
        timezone=true;
        return twoarg( arguments[0], arguments[1] );
    case 5:
        if (arguments[4] != 'GMT')
            return false;
        timezone=true;
    case 4:
        return fourarg( arguments[0], arguments[1], arguments[2], arguments[3] );
    case 7:
        if (arguments[6] != 'GMT')
            return false;
        timezone=true;
    case 6:
        return sixarg( arguments[0], arguments[1], arguments[2], arguments[3], arguments[4], arguments[5] );
    default:
        return false;
    }
}

function timeRange(/* varargs */)
{
    var timezone=false;

    function isHour( x ) { 
		return typeof(x) == 'number' && 0 <= x && x < 24 && isFinite(x) && Math.floor(x) == x;
	}

    function isMinSec( x ) {
		return typeof(x) == 'number' && 0 <= x && x < 60 && isFinite(x) && Math.floor(x) == x;
	}

    function onearg( h ) {
        var now = OPERA$getCurrentTime( timezone );
        return isHour( h ) && h == now.hour;
    }

    function twoarg( h1, h2 ) {
        var now = OPERA$getCurrentTime( timezone );
        if (isHour( h1 ) && isHour( h2 ))
            return h1 <= h2 ? now.hour >= h1 && now.hour <= h2 : now.hour >= h1 || now.hour <= h2;
        else
            return false;
    }

    function fourarg( h1, m1, h2, m2 ) {
        var now = OPERA$getCurrentTime( timezone );
        if (isHour( h1 ) && isMinSec( m1 ) && isHour( h2 ) && isMinSec( m2 )) {
            if (h1 <= h2)
                return (h1 < now.hour || h1 == now.hour && m1 <= now.min) && (now.hour < h2 || now.hour == h2 && now.min <= m2);
            else
                return (h1 < now.hour || h1 == now.hour && m1 <= now.min) || (now.hour < h2 || now.hour == h2 && now.min <= m2);
        }
        else
            return false;
    }

    function sixarg( h1, m1, s1, h2, m2, s2 ) {
        var now = OPERA$getCurrentTime( timezone );
        var after_start, before_end;
        if (isHour( h1 ) && isMinSec( m1 ) && isMinSec( s1 ) && isHour( h2 ) && isMinSec( m2 ) && isMinSec( s2 )) {
            if (h1 <= h2)
                return (h1 < now.hour || h1 == now.hour && (m1 < now.min || m1 == now.min && s1 <= now.sec)) && 
                    (now.hour < h2 || now.hour == h2 && (now.min < m2 || now.min == m2 && now.sec <= s2));
            else
                return (h1 < now.hour || h1 == now.hour && (m1 < now.min || m1 == now.min && s1 <= now.sec)) ||
                    (now.hour < h2 || now.hour == h2 && (now.min < m2 || now.min == m2 && now.sec <= s2));
        }
        else
            return false;
    }

    switch(arguments.length) 
	{
    case 2:
        if (arguments[1] != 'GMT')
            return twoarg( arguments[0], arguments[1] );
        timezone=true;
    case 1:
        return onearg( arguments[0] );
    case 3:
        if (arguments[2] != 'GMT') return false;
        timezone=true;
        return twoarg( arguments[0], arguments[1] );
    case 5:
        if (arguments[4] != 'GMT') return false;
        timezone=true;
    case 4:
        return fourarg( arguments[0], arguments[1], arguments[2], arguments[3] );
    case 7:
        if (arguments[6] != 'GMT') return false;
        timezone=true;
    case 6:
        return sixarg( arguments[0], arguments[1], arguments[2], arguments[3], arguments[4], arguments[5] );
    default:
        return false;
    }
}

/* eof */
