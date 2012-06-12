//
// WWWOFFLE Version 2.7 - Proxy autoconfiguration script
//
// See: http://www.netscape.com/eng/mozilla/2.0/relnotes/demo/proxy-live.html
//
// You should replace 'LOCALHOST' with the proxy hostname if required.
//

function FindProxyForURL(url, host)
{
 if(isPlainHostName(host))
    return "DIRECT";
 else
    if((url.substring(0, 5) == "http:") || (url.substring(0, 4) == "ftp:"))
       return "PROXY LOCALHOST; DIRECT";
    else
       return "DIRECT";
}
