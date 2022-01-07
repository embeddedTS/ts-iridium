// sbdctl.h
//  A header file for the ts sbdctl utility.
//  Created 8 Oct 2019 by Michael D Peters

// AT commands that are supported by the Iridium 9602
//   For full descriptions, find the document:
//  Iridium-9602-SBD-Transceiver-Product-Developers-Guide.pdf
//  %R 			displays all S registers
//  &Dn 		n=0 DTR off, n=1 DTR on & special 
//  &Fn 		Restore Factory Defaults  (n = ?)
//  &Kn 		Flow Control.  n=0 Disable n=3 RTS/CTS N=4 XON/XOFF n=6 Both
//  &V 			View active and stored configurations.
//  &Wn 		Store Active Configuration in slot n (0 or 1)
//  &Yn 		Designate default reset profile (0 or 1)
//  *F 			Flush to Eeprom (Modem shutdown prep / save volatile buffer data)
//  *Rn 		Radio Enable.  n=0 disable radio n=1 enable radio.
//  ;+CCLK 		Realt-Time Clock.  +CCLK=<time> "yy/MM/dd,hh:mm:ss+/-zz"
//  ;+CCLK?		Query RTC (see +cclk).
//  ;+CGMI		Query Manufacturer Information
//  ;+CGMM 		Model Identification
//  ;+CGMR		Query Revision Information
//  ;+CGSN 		Query IMEI
//  ;+CIER 		Event Reporting +CIER=[<mode>[,<sigind>[,<svcind>[,<antind>[,<sv_beam_coords_ind>]]]]]
//  ;              mode 0=disable 1=enable.  See documentation.
//  ;+CRIS 		Ring Indication Status +CRIS[X]:<tri>,<sri>,[X]<timestamp> sri: 0=no ring, 1=ring
//  ;+CSQ 		Signal Quality  +CSQ: 0-5 signal quality rating.  0=none 5=best
//  ;+CULK=n		Unlock <should NEVER need this on a 9602 from embeddedTS / contact sat provider>
//  ;+CULK?		Query if unlock is needed.  0 = not locked.
//  ;+GEMON 	xxx	See Documentation.  Estimate of charge from +5V displayed in microamp hours.  26 bit number
//  ;             rolls over after ~67 Ah.  Presset with at+gemon=n where n is the desired start value (typ 0).
//  ;			 note +GEMON function removed in firmware version 16005.
//  ;+GMI 		Get Manufacturer Information.  See +CGMI.
//  ;+GMM		Get Model Information.  See +CGMM
//  ;+GMR		Get Revision Information.  See +CGMR
//  ;+GSN 		Get IMEI.  See +CGSN
//  ;+IPR=n 		ISU Data rate (default 19200).  +IPR=<rate> See documentation for table.
//  ;+SBDAREG=n 	Short Burst Data Automatic Registration.  +SBDAREG=<mode>
//  ;			  0=disable 1=automatic 2=ask 3=auto w/event report 4=ask w/event report
//  ;			  See documentation.
//  ;+SBDC 		Clear MOMSN  (Mobile Originated Message Sequence Number).
//  ;+SBDDn		Short Burst Data Clear SBD Message Buffer(s) +SBDD[<type>]
//  ; 			  0=clear MO buffer 1=clear MT buffer 2=clear both buffers
//  ;			  Caveats: See docs.
//  ;+SBDDET 	Detach ISU from Gateway.  See docs for error list.
//  ;+SBDDSC 	Delivery Short Code.  Complicated.  See docs.
//  ;+SBDI 		Initiate SBD session.  Connect ISU to ESS, download oldest message from ESS to MT buffer.
//  ; 			  Transmit/upload MO buffer to ESS.  ISU is modem, ESS is satellite service.
//  ;			  Detach automatically after operation complete.
//  ;			  See docs for more response info.
//  ;+SBDIX 		Extended SBDI.  Required if using SBD Ring Alerts.  See comment further down.
//  ;+SBDMTA 	Mobile-terminated alert  enable/disable ring alerts +sbdmta=0 disable =1 enable see docs.
//  ;+SBDRB 		Read binary data from ISU. outpts MT binary.  Format is high order byte first:
//  ;			  {2-byte length} + {binary message} + {2 byte checksum}
//  ;			  Caveats.  See docs.
//  ;+SBDREG 	Force manual network registration.  Optional include location data.
//  ;			  +sbdreg[=<location>] where <location> is [+|-] DDMM.MMM,[+|-]ddmm.mmm
//  ;			  DD deg lat (00-89)  MM minutes lat (00-59) MMM 1/1000 min lat (000-999)
// 	;			  ddd deg lon (000-179) mm min lon (00-59) mmm 1/1000 min lon (000-999)
//  ;			  Resonse is +SBDREG:<status>,<err> 0=det 1=not reg 2=reg 3=denied, 0=no err
//  ;			  err list in docs.
//  ;+SBDREG? 	Check for current registration.  0=detached 1=not reg'd 2=reg'd 3=reg denied
//  ;+SBDRT 	 	Read text from MT buffer. Format:  +SBDRT:<CR>{MT buffer}
//  ;+SBDS 		Checks state of MT and MO buffers.
//  ;			  Response +SBDS:<MO flag>,<MOMSN>,<MT flag>,<MTMSN>
//  ;			  Flags:  0=no message 1=message in buffer  MTMSN=-1 means nothing in buffer.
//  ;+SBDST 		Set Session Timeout  +sbdst=<timeout> in seconds.  See docs.
//  ;+SBDSX 		SBD Status Extended.  It's +sbds with ring alert status and waiting msg count.
//  ;			  Note waiting msg count updated after every SBD session (sbdi/sbdix/sbdreg/sbddet).
//  ;+SBDTC 		Transfer MO buffer to MT buffer.  Mostly used to test software without using sat network.
//  ;+SBDWB 		Write binary to ISU MO buffer.  +sbdwb=[<sbd msg len in bytes>]  len does not inc checksum.
//  ;			  Modem will respond "READY<CR><LF>"
//  ; 			  Send {binary sbd message} + {2 byte checksum}.
//  ;  			  Checksum calc is least significant 2 byte sum of whole message.
//  ; 			  Send high order byte first.  eg.  "hello" in ascii would be:
//  ; 			  hex 68 65 6c 6c 6f 02 14
//  ;			  Modem will emit 0 on success, 1 on timeout of 60 seconds.
//  ;+SBDWT 		Write text message to ISU MO buffer.  Two usages:
//  ; 			  +sbdwt alone will allow full length 340 bytes 
//  ;			  Modem will emit "READY<CR><LF>" After <LF> send text message terminated by <CR>.
//  ;			  +sbdwt=[<text message>]  <text message> maximum 120 bytes if sent this way.
//  ;			  Modem will emit "OK" on success or "ERROR" if something went wrong.
//  ;-MSGEO		Request Geolocation.  Response -MSGEO: <x>,<y>,<z>,<stime_stamp>
//  ;			  x,y,z is geolocation grid code from earth centered Cartesian coordinate system
//	;			  The axes are aligned suhch that at 0 lat and 0 lon, both y and z are 0 and x is
//  ;			  +6376, representing the nominal earth radisu in KM.  Coords are minimum -6376,
//  ;			  max +6376, with a resolution of 4.
//  ;-MSSTM		Request System Time.  See docs.  It'll give you a 32 bit integer, but the docs expressly
//  ;			  say the value should not be used for user applications due to frequent rollovers.
//  A/			Repeat last AT command.
//  AT 			Command prefix used for all other commands.
//  En 			Echo characters to DTE.  n=0 no echo n=1 echo.
//  In 			Indentification.  Requests ISU to display information about itself.
// 				  n=0 data rate
// 				  n=1 0000
// 				  n=2 OK
//  			  n=3 "XXXXXXXX" Software revision level (TA16005)
//  			  n=4 IRIDIUM 9600 Family
//  			  n=5 "XXXX" Country Code   (8816 = USA?)
//  			  n=6 "XXX" Factory Identity (1OK)
// 				  n=7 "XXXXXXXX" Hardware specification (BOOT07d2/9602NrvA-D/04/RAW0d)
//  Qn 			Quiet Mode:  n=0 ISU responses are sent to the DTE.  n=1 ISU resopnses are NOT sent to the DTE.
//  Vn  		Verbosity:  0=Numeric responses 1=Text responses.
//  Zn 			Soft Reset:  0=restore config 0  1=restore config 1.
//  +SBDGW[N]-? Unknown compatability:  Query what gateway this iridium is configured to use.
//  			  Responses are +SBDGW <gateway_text>  either "EMSS" or "non-EMSS".
//    			  if 'N' is used:  +SBDGWN: <1 or 2>.  1=default commercial gw.  2=some other gw.
//  +SBDLOE --? Unknown compatability:  Traffic Management Status.
//  			  Returns +SBDLOE:<status>,<time>  <status>=0 time is valid. 1 time could not be determined.
//  			  <time> is the time in seconds to the end of the current SBD traffic mgmt period.
//  			  If traffic management is NOT in effect, <time> will be 0.  Otherwise any SBD session
//  			  will result in error code 38.  Power cycling the ISU has no effect on traffic management.

//
// Useful info:
// AT+SBDIX Status Codes:
//  <these come from the gateway>
//  0:  MO message successful send.
//  1:  MO message sent successful, but the MT message queue was too big to receive.
//  2:  MO message sent, but requested location update was not accepted.
//  3-4:Reserved, but indicates MO session success.
//  5-8:Reserved, but indicates MO session failure.
//  10: Gateway reported timeout.
//  11: MO message queue full at gateway.
//  12: MO message has too many segments.
//  13: Gateway reported session did not complete.
//  14: Invalid segment size.
//  15: Access Denied!
//  <these come from the transceiver>
//  16: Transceiver is locked and may not make SBD calls (see +CULK).  AKA call your service provider.
//  17: Gateway not responding (local session timeout).
//  18: Connection lost (RF drop).
//  19-31:  Reserved, but indicate MO session failed.
//  32: No network service, unable to initiate call.
//  33: Antenna fault, unable to initiate call.
//  34: Radio is disabled, unable to initiate (psst, turn on the radio with at*r1).
//  35: Transceiver is busy, try again (transceiver is probably doing auto-negotiation).
//  36: Reserved, but indicates failure.

#define BAUD B19200  	// Default Iridium 9602 baud rate.
#define MAXBYTES 320	// Maximum transmit message size.
#define MAX_BUFF 350    // Maximum transmit size with CRC.
#define RSSI_BAD 0   	// No signal.
#define RSSI_FULL 5		// Full signal.
#define NO_NETWORK 0	
#define INIT_STRING "ate0v1&k0q1\r\n"	// echo = off, verbose = on, handshake = off, quiet = on.
#define INIT_STRING_LEN 13
#define EVENTS_ENABLE "at+cier=1\r\n" // turns on auto event reporting.
#define EVENTS_DISABLE "at+cier=0\r\n" // turns off auto event reporting.
#define RSSI_QUERY "at+csq\r\n"		// returns current rssi
#define IMEI_QUERY "at+gsn\r\n"		// returns imei
#define SBD_WRITE_BINARY "at+sbdwb=%d\r\n"   // %d is message length not including 2-byte checksum.
#define SBD_READ_BINARY "at+sbdrb\r\n"
#define SBD_WRITE_TEXT_INLINE "at+sbdwt=%s\r\n" // %s is string of text less than 120 chars.
#define SBD_WRITE_TEXT "at+sbdwt\r\n"     // responds with READY, then send up to 340 chars. + CR (?)
#define SBD_READ_TEXT "at+sbdrt\r\n"		// read text from receive buffer.
#define SBDD_CLEAR_MO_BUFF 0
#define SBDD_CLEAR_MT_BUFF 1
#define SBDD_CLEAR_ALL_BUFF 2
#define TEXT_MODE 0
#define BIN_MODE 1
