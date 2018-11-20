# HttpProxyServer
The server can be compiled with the make file.
>   make

You can start the server with the command
>   webproxy {portNo} {timeout}

HTTP is a client-server protocol; the client usually connects directly with the server. But
often times it is useful to introduce an intermediate entity called proxy. The proxy sits
between HTTP clients and HTTP servers. With the proxy, the HTTP client sends a HTTP
request to the proxy. The proxy then forwards this request to the HTTP server, and then
receives the reply from the HTTP server. The proxy finally forwards this reply to the
HTTP client.

This HTTP server has the following features 
* **Multi-process Proxy** :(Every new request is forked to be handled in async manner)
* **Caching with timeout** :
    Timeout setting: alter your proxy so that you can specify a timeout value on the
    command line as runtime options. For example, “./proxy 10001 60” will run proxy
    with timeout value 60 seconds.
* **Page caching to serve already relayed pages, to reduce network traffic.**
* **Hostname’ IP address cache**: The proxy has a cache of IP addresses it
resolved for any hostname and store (hostname,IP) pair in local cache file (a file on
a disk or within program memory, any approach should be okay). Thus, if same
hostname is requested again, the proxy skips the DNS query to reduce DNS
query time.
* **Blacklist**: Make a file which has a list of websites and IP addresses that must be
blocked. Thus, for every incoming request, proxy must look up this data and deny
requests for these websites or IPs. When client request one of them, you return
“ERROR 403 Forbidden” immediately from the proxy itself. This file can be just one
line after line and can have both hostname or the IP address.
