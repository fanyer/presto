function FindProxyForURL( url, host )
{
    if(shExpMatch(url, "ftp:*"))
    {
    	return "DIRECT";
    }

    if(shExpMatch(url, "https:*"))
    {
    	if(shExpMatch(host,"*.opera.com"))
    	{
    		return "DIRECT";
    	}
    }

    return "PROXY proxy.lab.osa:3128";
}