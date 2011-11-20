
This directory holds ivykis example programs and contributed ivykis
modules.


iv_getaddrinfo/
	A nonblocking DNS stub resolver module.


iv_openssl/
	Example module that wraps openssl's I/O functions, so that one
	can write ivykis SSL-enabled client and server apps.


kojines/
	Application that accepts connections forwarded to it by the
	iptables REDIRECT target, and forwards them to a remote SOCKS5
	proxy, using the original destination address of the REDIRECTed
	connection as the destination address in the SOCKS5 proxy
	request.
