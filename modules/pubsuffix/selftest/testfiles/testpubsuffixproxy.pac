function FindProxyForURL( url, host )
{
    if(host == "cvs.oslo.osa")
    {
    	return "PROXY proxy.lab.osa:3128";
    }
   	return "DIRECT";
}