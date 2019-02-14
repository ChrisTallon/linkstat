# linkstat
Colour a terminal screen red or green depending on ping result

A quick and dirty C / ncurses program to repeatedly ping an IP and colour the whole terminal background in red or green, depending on whether there was a ping reply.

Useful when you want a simple, big, easy to spot indicator for when an IP starts or stops pinging. *Hypothetical* example, when you are waiting for your broadband to start working again...

Features:

* Small / lightweight
* Supports IPv4 and IPv6
* Selectable delay between pings
* Displays last ping attempt time
* Displays "down since" time, if known
* Possible to set a default IP in source file

Requirements:

* C compiler
* ncurses development library
* /bin/ping which supports IPv6 addresses
* A target remote IP to monitor
* A terminal to run it on

Compilation:

`cc -o linkstat linkstat.c -lncurses`

Usage:

If you set a default ping IP in the source before compiling it, you can start it with no arguments. The ping interval will be 5 seconds.

Otherwise:

`./linkstat [-6] [-n interval] <IPv4, IPv6 or hostname>`

* -n interval - in seconds
* -6 - force IPv6 if using a hostname with both IPv4 and IPv6 addresses

Note: if the remote host is down the actual interval will be interval + 2 seconds due to the 2s wait in the ping code.

An email saying "Hi, I found and use linkstat and I think it's the best thing ever!"* would be very much appreciated :)

chris@loggytronic.com


\* Or, you know, your own message...
