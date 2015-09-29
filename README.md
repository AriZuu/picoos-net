uIP for pico]OS
===============

This is a uIP-based network stack for pico]OS.
It is currently based on version 3.0 of [contiki][3].
Support for both IPv4 and ipV6 is included.

In addition to standard uIP stack library includes two
socket layers ("native" and BSD sockets) to make programming easier. 
BSD socket layer contains only basic functionality but should
enable writing programs that are compatible with other environments
using sockets.

Library includes some device drivers. Enc28j60 is quite
common chip, [driver being used currently][4] is written by
Ivan A. Sergeev.

For additional details see [this article][1] or [manual][2].

[1]: http://stonepile.fi/uip-based-network-layer-for-picoos/
[2]: http://arizuu.github.io/picoos-net
[3]: http://www.contiki-os.org
[4]: http://github.com/vsergeev/embedded-drivers/tree/master/lpc2148-enc28j60

