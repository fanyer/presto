/* This script is incorrect (syntax error).  Setting it as the PAC
   script and loading a page should cause a message about PAC being
   disabled.  The syntax error should also appear in the JS console.
   */
function FindProxyForURL( url, host )
{
    for ( int i=0 ; i < 10 ; ++i )  // 'int' is not JS.
      ;
    return "DIRECT";
}