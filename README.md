httpdate
========

_httpdate_ is a __ntp__ replacement utilizing the `Date:` string
delivered in HTTP responses.

Its accuracy, of course, is worse than NTP but as good as of 1s.
_httpdate_ takes into account round trip times and uses
constant delays with non blocking sockets to achieve maximum
accuracy modulo what HTTP could offer.

Timestamps on UNIX filesystems are in seconds, so NTP
with accuracy of 10ms wont be of much benefit if it all broken
down to seconds anyway.

_httpdate_ works over IPv4 and IPv6. It also applies some kind
of voting mechanism to drop out fooling HTTP servers.
It can drop it privileges to user (`-u` or nobody) and runs
in a chroot, only keeping `CAP_SYS_TIME` capability on Linux.

The HTTP time server to stay in sync with may be given by the
`-T` switch which is the only required argument, unless you want to change
the default setting of the chroot, user, delay slot etc. _httpdate_
requests time servers each `-S` seconds (default 6h).

If the argument of `-T` is a filename rather than a server,
the filename is read and lines are interpreted in the form

```
# comment
host~port
```

The prefered setup for pools of PCs runs with one master _httpdated_
requesting time from the internet installed on a web server
and serving internal clients via _lophttpd_ or a different httpd.

If you require time accuracy of milli seconds or better because you are
deploying radar defense or nuclear rockets you should clearly
not use _httpdate_.

