/* -*- mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * This code is loaded by autoproxy_async.ot during selftest runs and is used
 * to test parts of the autoproxy library.
 *
 * Tests
 *   localHostOrDomainIs
 *   isResolvable
 *   dnsResolve
 *
 * Does not test
 *   myIpAddress
 *
 */

function FindProxyForURL(url, host)
{
	switch (url)
	{
	case "isResolvable":        return testIsResolvable(host);
	case "localHostOrDomainIs": return testLocalHostOrDomainIs(host);
	case "dnsResolve":          return testDNSResolve(host);
	default:                    return "ERROR: unexpected url/testcase " + url;
	}
}

function testIsResolvable(key)
{
	switch(key)
	{
	case "1": return isResolvable("www.opera.com") ? "OK" : "ERROR" ;
	case "2": return isResolvable("bogus.finsikke.foo") ? "ERROR" : "OK";
	default:  return "ERROR: unexpected isResolvable key: " + key;
	}
}


function testLocalHostOrDomainIs(key)
{
	switch(key)
	{
	case "1": return localHostOrDomainIs("www.opera.com","www.opera.com") ? "OK" : "ERROR";
	case "2": return localHostOrDomainIs("www","www.opera.com") ? "OK" : "ERROR";
	case "3": return localHostOrDomainIs("www.microsoft.com","www.opera.com") ? "ERROR" : "OK";
	case "4": return localHostOrDomainIs("home.opera.com","www.opera.com") ? "ERROR" : "OK";
	default:  return "ERROR: unexpected localHostOrDomainIs key: " + key;
	}
}

function testDNSResolve(key)
{
	switch(key)
	{
		// Check that the t server has a 10.20.x.y IP number
	case "1": return dnsResolve("t").match(/^10\.20\./) == "10.20." ? "OK" : "ERROR";
	default:  return "ERROR: unexpected dnsResolve key: " + key;
	}
}
