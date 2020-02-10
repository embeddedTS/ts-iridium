# ts-iridium
Sample code for use with the TS-IRIDIUM satellite communications peripheral.


---------------------
License:  This software is offered under the Technologic Systems Standard License:

We don't mind what you do with it so long as you're one of our customers.

The software is provided without warranty of any kind.
---------------------

Compiling:  
For debug:
gcc sbdctl.c -o sbdctl -O0 -g

Example Output & usage:
#check modem input and output buffers:
./sbdctl -D 12 < hello.txt
./sbdctl -a
MOMTCP_BYTES=12
./sbdctl -d | hexdump -c
0000000   H   e   l   l   o       W   o   r   l   d  \n 004   &
000000e

Check software & modem basic functionality:
./sbdctl -z | hexdump
This is a test function.
BOOT07d2/9602NrvA-D/04/RAW0d
Command sent.  Modem responded READY.

Checksum calculated to 81600 = 0x13ec0
Checksum truncated to 16064 = 0x3ec0
writing checksum 0x3e 0xc0
test data written to MO buffer, modem responded result=0
Wrote at+sbdtc command to modem. 10 bytes.
SBDTC: Outbound SBD Copied to Inbound SBD: size = 320
------------------------------------------------------------------------
Sending read-binary request to modem.
Reading binary data from modem.
Read 322 bytes.
0000000 ffff ffff ffff ffff ffff ffff ffff ffff
*
0000140 c03e
0000142
