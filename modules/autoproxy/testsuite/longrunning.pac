/* This script tests that the suspend/resume logic for long-running
   scripts inside the autoproxy scheduler works correctly.
   */
function FindProxyForURL( url, host )
{
    for ( var i=0 ; i < 10000 ; ++i )
      ;
    return "DIRECT";
}