/* This script tests what happens when the proxy script has a
   run-time error or throws an exception.  The exception should
   show up in the JS console, and the lookup should default to
   DIRECT.  (Should it default for this lookup only or for all
   future lookups?)
   */
function FindProxyForURL( url, host )
{
    throw "The proxy script threw an exception!";
}