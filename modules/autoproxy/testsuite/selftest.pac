/* -*- mode: c++; tab-width: 4 -*-
 *
 * Autoproxy script that tests network functionality of autoproxy code.
 *
 * Tord Akerbæk, Lars T Hansen
 *
 * Tests:
 *   localHostOrDomainIs
 *   isResolvable
 *   dnsResolve
 *
 * Does not test:
 *   myIpAddress
 *
 * To use this code:
 *   Make a SELFTEST build of Opera, and start it
 *   Set up this file as the autoproxy script URL
 *   Clear the cache
 *   Load one page, and quit
 *   test_apc.txt will be created in the current directory for builds where YNP_WORK is defined.
 *   The output is in test_apc.txt in the working directory
 *
 * Note it is not necessary to start Opera with -test, in fact it is
 * probably better not to.
 *
 * In order to test that the script caching logic does not force
 * constant reloading, you must do this once for a script from a file:
 * URL, and one for a script from an http: URL.
 *
 * If the output contains a line that says eg
 *     Number of loads of proxy script ERROR (should be 1, was 37)
 * and a lot of other output then bug #168185 is still not fixed.
 */

var tested=false;

function FindProxyForURL(url, host)
{
	if (tested) return "DIRECT";

	tested=true;
	debugAPC( "-----------------------------" );
	debugAPC( "test run: " + (new Date()).toString() );
	debugAPC( "localHostOrDomainIs #1 " + (localHostOrDomainIs("www.opera.com","www.opera.com") ? "OK" : "ERROR") );
    debugAPC( "localHostOrDomainIs #2 " + (localHostOrDomainIs("www","www.opera.com") ? "OK" : "ERROR" ) );
    debugAPC( "localHostOrDomainIs #3 " + (localHostOrDomainIs("www.microsoft.com","www.opera.com") ? "ERROR" : "OK" ) );
    debugAPC( "localHostOrDomainIs #4 " + (localHostOrDomainIs("home.opera.com","www.opera.com") ? "ERROR" : "OK" ) );

    debugAPC( "isResolvable #1 " + (isResolvable("www.opera.com") ? "OK" : "ERROR" ) );
    debugAPC( "isResolvable #2 " + (isResolvable("bogus.finsikke.foo") ? "ERROR" : "OK" ) );

	// Here we want to use a host with a constant IP address.  www.opera.com does not have one.
	// Be prepared to adjust this test case from time to time.

    debugAPC( "dnsResolve #1 " + (dnsResolve("cvs.oslo.opera.com") == "192.168.1.129" ? "OK" : "ERROR") );
    debugAPC( "dnsResolve #2 " + (dnsResolve("cvs.oslo.opera.com") != "192.168.1.129" ? "ERROR" : "OK" ) );

	// FIXME: myIpAddress

	return "DIRECT";
}

