
kojines is an application that accepts connections forwarded to it by
the iptables REDIRECT target, and forwards them to a remote SOCKS5
proxy, using the original destination address of the REDIRECTed
connection as the destination address in the SOCKS5 proxy request.

Example setup:

1. Set up a SOCKS5 proxy.  For example, using ssh, run ssh with the
   "-D 1080" argument (or add 'DynamicForward 0.0.0.0:1080' in the
   config file for the host you are ssh'ing to).

2. Start kojines.

3. Forward connections to kojines by adding an iptables rule to
   REDIRECT those connections to localhost port 63636 (which is the
   default address:port combination that kojines listens on).

   For example, to forward all connections to 1.2.3.4 port 80 (TCP)
   over the SOCKS5 proxy, do:

	iptables -t nat -A OUTPUT -d 1.2.3.4 -p tcp --dport 80 -j REDIRECT --to-ports 63636

   Now if a connection is made to 1.2.3.4:80, it will be redirected to
   localhost:63636, where kojines will pick it up, issue a SO_ORIGINAL_DST
   ioctl on it to find the original destination address, get 1.2.3.4:80
   back from that ioctl, connect to 127.0.0.1:1080 (the ssh process you
   started earlier), send a SOCKS5 open request for the remote address
   1.2.3.4:80 to 127.0.0.1:1080, and once the SOCKS5 connection establishes,
   splice the two TCP connections together.

There is currently no run-time (config file or command line option)
method to change the listening address and SOCKS5 proxy address -- you'll
have to edit main.c for now.
