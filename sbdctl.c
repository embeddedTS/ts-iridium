//sbdctl.c
// c. 2020, embeddedTS
//  For example use only.
//  Written by Michael D Peters
//
// This program provides an example of how to use the TS-IRIDIUM peripheral in conjunction with
//  the TS-7800-V2 single board computer.
//
// The program is designed such that it should be relatively trivial to modify for use
//  with any SBC + IRIDIUM combination simply by providing it with a valid serial port
//  using the -p <port> option.
//
// This program is licensed for free use to any embeddedTS customer, and is
//  provided with no warranties at all.
//
// Definitions:
//  SBC is Single Board Computer (aka TS-7800-V2)
//  IMU is the Iridium Modem (IRIDIUM 9602 at time of writing)
//

#include <stdio.h>
#include <termios.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include "sbdctl.h"

// setup functions

int serial_init(int fd)
{
	int err=0;
	struct termios options;

	if(tcgetattr(fd, &options)){
		fprintf(stderr, "There was an error setting up the serial port.  %s\n", strerror(errno));
		abort();
	}

	cfsetispeed(&options,BAUD);
	cfsetospeed(&options,BAUD);
	options.c_cflag |= (CLOCAL | CREAD);
  	options.c_cflag &= ~PARENB; // No Parity
  	options.c_cflag &= ~CSTOPB; // 1 stop bit.
  	options.c_cflag &= ~CSIZE; // see CS8 below.
  	options.c_iflag = IGNPAR;  // Ignore parity.
  	options.c_iflag |= IGNBRK; // Ignore breaks.
  	options.c_iflag &= ~(IXON | IXOFF | IXANY); // no flow control.
  	options.c_cflag |= CS8; // 8 bit data width.
  	options.c_oflag = 0;
  	options.c_lflag = 0;
  	//  VMIN > 0, VTIME > 0 --> Blocking Read of up to VMIN chars with intra-char timeout
  	//   Timer doesn't start till first char received.
  	//   Downside:  read quits as soon as VMIN is hit.  Max VMIN is 255.
  	options.c_cc[VMIN] = 255;   // 255 is maximum vmin.
  	options.c_cc[VTIME] = 1; // read timeout in 1/10 seconds of no (more) data.
 
 	err=tcsetattr(fd, TCSANOW, &options);

	return err;
}

// TEXT_MODE = 0
// BIN_MODE = 1
int set_serial_mode(int fd, int option){
	struct termios options;
	int err;
	if(tcgetattr(fd, &options)){
		fprintf(stderr, "Error, could not set port options.  %s\n", strerror(errno));
		abort();
	}
	if(option==TEXT_MODE)
		options.c_cflag &= ~ICANON;
	else
		options.c_cflag |= ICANON;
	err = tcsetattr(fd, TCSANOW, &options);
	return err;
}

// string handling functions

// use this only when you expect single-line data.
//  -- NOTE:  Incompatible with +CIER data (+CIER provides unprovoked data).
//			  Incompatible with binary data, too.
// strip cr/lf and 'OK' from buf, null terminate, and return new size.
int strip(char* buf, int size){
	char buf2[MAX_BUFF];
	char temp = ' ';
	int i,j;
	j = 0;
	for(i=0; i<size; i++){
		if((buf[i] == '\n') || (buf[i] == '\r'))
			fprintf(stderr, "");
		else if((buf[i] == 'O') && (buf[i+1] == 'K'))
			i++;
		else {
			buf2[j++] = buf[i];
		}
	}
	strncpy(buf, buf2, j);
	// cap the new buf for proper string handling.
	buf[j] = '\0';
	return j-1;  // j-1 because the null char (\0) does not count toward length.
}

// Binary checker
//  If the modem sends me a binary, it will be:
//  <2 bytes len> + <len bytes message> + <2 bytes checksum>
//  So:
//  parse len, do checksum, confirm len + 4 = rx buffer length.
//  Return calculated buffer length with +4 to confirm binary.
//  Returns -1 if checksum mismatch.
//  There are other ways to do this.
int check_binary(char* buf, int buf_len){
	uint8_t len_hb, len_lb, checksum_hb, checksum_lb;
	uint16_t calc_len, calc_checksum, count_checksum, i;

	len_hb = buf[0];
	len_lb = buf[1];

	calc_len = len_hb << 8;
	calc_len += len_lb;

	//  len-1 is low byte of checksum.  len-2 is high byte of checksum.
	checksum_hb = buf[buf_len - 2];
	checksum_lb = buf[buf_len - 1];

	calc_checksum = checksum_hb << 8;
	calc_checksum += checksum_lb;

	for(i = 2; i < (buf_len - 2); i++)
		count_checksum += buf[i];

	if(calc_checksum != count_checksum) 
		return -1;
	else
		return calc_len + 4; // full length <len>+<data>+<checksum>.
}

// Basic SBC to IMU communications

// Writes buf to imu.  
//  Fancy:  tcdrain() makes sure the function doesn't return
//		until the driver is done writting to hardware.
int write_to_imu(const char* buf, int size, int fd){
	// serial write 
	int bitcount=write(fd, buf, size);
	tcdrain(fd); // wait for serial port to finish transmitting.

	return bitcount;
}

int read_binary_from_imu(unsigned char* buf, int fd){
	int i, i2;
	uint8_t size_hb, size_lb;
	uint16_t size = 0;

	write_to_imu("at+sbdrb\r\n", strlen("at+sbdrb\r\n"), fd);

	set_serial_mode(fd, BIN_MODE);
	i = read(fd, buf, 2);
	size_hb = buf[0];
	size_lb = buf[1];
	size = buf[0] << 8;
	size += buf[1];

	// now we know how many bytes to read, add 2 bytes for checksum.
	i = read(fd, buf, size+2);

	// Check if there is more in the buffer and read that in where we left off.
	if(i < size+2){
		i2 = read(fd, &buf[i], (size-i+2));
	}

	// turn off bin mode when done.
	set_serial_mode(fd, TEXT_MODE);

	// return number of bytes in buf, including checksum.
	return size+2;
}

// read_from_imu
//  wants:  pointer to char array buf[MAX_BUFF].
//  wants:  integer file descriptor to serial port.
//  returns:  number of bytes placed in buf[MAX_BUFF].
//  side effect:  Replaces buf[MAX_BUFF] with new data.
//
// Note:  This is set up to read binary but it does a
//   poor job of it.  Better to use read_binary_from_imu()
//   because it's a deliberate read expecting binary data.
//   This function could use some cleanup as a result.
int read_from_imu(unsigned char* buf, int fd){
	char temp_buff[MAX_BUFF];
	int i, bytesleft;
	int count = 0;
	int count2 = 0;
	int failover = 0;
	uint8_t size_hb, size_lb;
	uint16_t size = 0;

	// first, some housekeeping.  Erase that buffer so strncat will work.
	for(i = 0; i < MAX_BUFF; i++) buf[i] = '\0';
	bzero(temp_buff, sizeof(char[MAX_BUFF]));
	
	count = read(fd, buf, MAX_BUFF); // lol I should be so lucky if only one read.

	size_hb = buf[0];
	size_lb = buf[1];
	size = (uint16_t)(size_hb << 8) + size_lb;

	if(count < 0) {
		// an error happened.
		return count;
	}

	else if( (count < 255) && (count != size) ){
		count = strip(buf, count);
		return count;
	}

	else if(size == count){
		count2 = check_binary(buf, size);
		if(count2 == count)
			return count;  // Binary check good, return out.
		else
			return count2; // Binary check bad, return error.
	}

	// Read again.  There's probably more data coming.
	while(count != (size+4)) {
		count2 = 0;
		count2=read(fd, temp_buff, (MAX_BUFF - count)); // only read what we have room for.
		strncat(buf, temp_buff, count2); 
		count += count2;
	}

	count2 = check_binary(buf, count);

	return count;
}

// Combines imu write & read, takes command, modifies buffer, returns buffer size.
//  Dies violently on error.
// Why?
//  Typically writing to the SBD will generate some sort of immediate response.
//  That response will need to be parsed.  Therefore it makes sense to immediately
//  follow an SBD write with an SBD read.
int imu_rw(const char* command, char* buf, int fd){
	int size;
	size=write_to_imu(command, strlen(command), fd);
	if(size<0) {
		fprintf(stderr, "Failed to write to IMU. %s\n", strerror(errno));
		abort();
	}
	size= read_from_imu(buf, fd);

	return size;
}

// Request RSSI from modem.
// at+csq
// Return int 1-5, or return < 0 if err.
int get_rssi(int fd){
	int length;
	unsigned char buf[MAX_BUFF] = {'\0'};
	int i = 0;
	length=imu_rw("at+csq\r\n", buf, fd);
	
	// use sscanf to grab the number after +CSQ:.
	sscanf(buf, "+CSQ:%d", &i);

	return i;
}

// Front-end functions
// info() spits out a bunch of modem-related information.
//  It also attempts to connect to the SBD network.
static void info(int fd){
	int error, moflag,momsn,mtflag,mtmsn,raflag,waitcount;
	int x,y,z,geotimestamp;
	unsigned char buf[MAX_BUFF] = {'\0'};
	// MODEM_FIRMWARE=   ... ati3
	imu_rw("ati3\r\n", buf, fd);
	fprintf(stderr, "MODEM_FIRMWARE=%s\n", buf); 
	// MODEM_HARDWARE="..."  ati4
	imu_rw("ati4\r\n", buf, fd);
	fprintf(stderr, "MODEM_HARDWARE=%s\n", buf);
	// MODEM_HW_INFO="..."  ati7
	imu_rw("ati7\r\n", buf, fd);
	fprintf(stderr, "MODEM_HW_INFO=%s\n", buf);
	// IMEI=... 	at+gsn
	imu_rw("at+gsn\r\n", buf, fd);
	fprintf(stderr, "IMEI=%s\n", buf);
	// RSSI=...     at+csq
	fprintf(stderr, "RSSI=%d\n", get_rssi(fd));
	// GW_TYPE=... EMSS or NON-EMSS. 	at+sbdgw
	imu_rw("at+sbdgw\r\n", buf, fd);
	sscanf(buf, "+SBDGW: %s", buf);
	fprintf(stderr, "GW_TYPE=%s\n", buf);
	// MSGEO=... Geolocation String x,y,z,timestamp. 	at-msgeo
	imu_rw("at-msgeo\r\n", buf, fd);
	sscanf(buf, "-MSGEO:%d,%d,%d,%x", &x, &y, &z, &geotimestamp);
	fprintf(stderr, "RAW_MSGEO=%d,%d,%d,0x%x\n", x,y,z,geotimestamp);
	// MSSTM=... <iridium system time in secs from iridium epoch>	at-msstm
	imu_rw("at-msstm\r\n", buf, fd);
	sscanf(buf, "-MSSTM: %x", buf); // msstm should really be expressed as an integer.
	fprintf(stderr, "MSSTM=0x%x\n", buf);
	// SBDS -- Will need to parse this output.  This is how we tell if there's a message
	//  ready to receive.    at+sbds
	imu_rw("at+sbdsx\r\n", buf, fd);
	sscanf(buf, "+SBDSX:%d, %d, %d, %d, %d, %d", &moflag, &momsn, &mtflag, &mtmsn, &raflag, &waitcount);
	fprintf(stderr, "RAW_SBDSX=%d,%d,%d,%d,%d,%d\n", moflag, momsn, mtflag, mtmsn, raflag, waitcount);
	fprintf(stderr, "INBOX_STATUS=%d\n", mtflag);
	fprintf(stderr, "OUTBOX_PENDING=%d\n", moflag);
	fprintf(stderr, "SERVER_MSG_PENDING=%d\n", waitcount);
}

//  send at+sbdwt, wait for READY, dump message text into modem.
// returns 0 or err status.
int send_text_message(char* themessage, int length, int fd){
	unsigned char temp_buff[MAX_BUFF] = {'\0'};
	unsigned char sendCmd[] = "at+sbdwt\r\n";
	unsigned int len = -1;

	if(length > 340){
		fprintf(stderr, "Specified length larger than outbound buffer.  Aborting.\n");
		abort();
	}
	
	write_to_imu(sendCmd, strlen(sendCmd), fd);
	read_from_imu(temp_buff, fd);

	if(temp_buff[0] == 'E') ; // fail case. XXX handle error.

	len = write_to_imu(themessage, length, fd);
	len += write_to_imu("\r", 1, fd);  // text message must be terminated by a carriage return.
	return len;
}

// takes buf, sets len and checksum, puts into MO buf on modem.
// returns total number of message bytes written.
//  sbdwb format:  at+sbdwb=<binary len>. modem responds READY
//    Then send <binary> + <2 byte checksum>.
int send_binary_data(char* buf, int fd, int len){
	unsigned int i, calc_checksum, len2;
	uint16_t trunc_checksum;
	unsigned char checksum[2];
	unsigned char sendCmd[MAX_BUFF] = { '\0' };
	char temp_buff[MAX_BUFF] = { '\0' };

	// Construct send command.
	sprintf(sendCmd, "at+sbdwb=%d\r\n", len); 
	   
	// Check for len sanity.
	if(MAX_BUFF <= len + 2){
		// fail loudly here.  len should be at most max_buff minus 2.
		fprintf(stderr, "ERROR=len %d > %d\n", len, (MAX_BUFF-2));
		abort();
	}
	// Insert len and checksum.
	for(i = 0; i < len ; i++) calc_checksum += buf[i];
	trunc_checksum = calc_checksum;
	checksum[0] = trunc_checksum >> 8;
	checksum[1] = trunc_checksum;

	len2 = write_to_imu(sendCmd, strlen(sendCmd), fd);
	i = read_from_imu(temp_buff, fd);
	if (temp_buff[0] == 'E') abort(); // XXX Handle error?
	else if (temp_buff[0] != 'R')  fprintf(stderr, "WRITE_ERROR=\"%s\"\n", temp_buff); // XXX not R not E should not happen.

	len2 = write_to_imu(buf, len, fd); // binary message.
	len2 += write_to_imu(checksum, 2, fd);   // checksum.

	return len2;
}

//  read text data from MT buffer on modem.
//  wants char* buf and fd to serial port.
//  returns # of bytes read & puts data in char* buf.
//  Cleans "+sbdrt:\r\n" so buf contains just the MT message.
int get_text_data(char* buf, int fd){
	int i;
	int len = imu_rw("at+sbdrt\r\n", buf, fd);
	// strip +sbdrt:\r from buf.
	for(i=0; i<(len-6); i++)
		buf[i]=buf[i+7];
	buf[len-6] = '\0';
	len = len - 7;
	return len;
}

// Basic prints text data from sbd buffer.  Needs refactor to support output to
//  filename via -f function.
int print_text_data(int fd){
	unsigned char buf[MAX_BUFF] = { '\0' };
	int len = get_text_data(buf, fd);
	fprintf(stderr, "TEXT_MESSAGE=\"%s\"\n", buf);
}

//  Sends raw binary data from char* buf to stdout.
//  Intended for use with a binary read utility such as
//  hexdump.
void print_binary_data(char* buf, int len){
	// XXX
	write(STDOUT_FILENO, buf, len);
}

void dread(int fd){
	unsigned char buf[MAX_BUFF];
	bzero(buf, sizeof(char)*MAX_BUFF);
	print_binary_data(buf, read_binary_from_imu(buf, fd));
}

// int clearbufs(instruction, *fd)	
// at+sbdd<0,1,2>
// 0=clear MO
// 1=clear MT
// 2=clear both
// returns 0: clear successful
// returns 1: error while clearing buffer.
// modem return format:
// 0<cr><lf><cr><lf><OK><cr><lf>
int clearbufs(int instruction, int fd){
	int bytes = 0;
	unsigned char buf[MAX_BUFF]; 
	switch(instruction){
		case 0:
			imu_rw("at+sbdd0\r\n", buf, fd);
			break;
		case 1:
			imu_rw("at+sbdd1\r\n", buf, fd);
			break;
		case 2:
			imu_rw("at+sbdd2\r\n", buf, fd);
	}
	sscanf(buf, "%d", &bytes);
	return bytes;
}

void getsbdstatus(int fd){
	// at+sbdsx
	// modem returns "+SBDSX: mflag, momsn, mtflag, mtmsn, raflag, msg_wait"
	//   mtmsn will be -1 if no message pending send.
	//   flags will be 1 if message pending send/receive
	//    or 0 if no message pending.
	//   m*msn is msn# of pending message (out or in)
	//   momsn is outbound, mtmsn is inbound.
	//   raflag is ring alert (not really needed on 9602).
	//   msg_wait is how many inbound messages are queued in the cloud.
	int mflag, momsn, mtflag, mtmsn, raflag, msg_wait;
	unsigned char buf[MAX_BUFF] = { '\0' };
	mflag = momsn = mtflag = mtmsn = raflag = msg_wait = 0;
	imu_rw("at+sbdsx\r\n", buf, fd);
	sscanf(buf, "+SBDSX: %d, %d, %d, %d, %d, %d\n", &mflag, &momsn, &mtflag, &mtmsn,
		&raflag, &msg_wait);
	fprintf(stderr, "MSG_OUT_WAIT=%d\n", mflag);
	fprintf(stderr, "MSG_OUT_SEQ_NUM=%d\n", momsn);
	fprintf(stderr, "MSG_IN_WAIT=%d\n", mtflag);
	fprintf(stderr, "MSG_IN_SEQ_NUM=%d\n", mtmsn);
	fprintf(stderr, "RING_ALERT=%d\n", raflag);
	fprintf(stderr, "MESSAGES_ON_SERVER=%d\n", msg_wait);
}

void getsbdrssi(int fd){
	unsigned char buf[MAX_BUFF];
	int rssi = 0;
	imu_rw(RSSI_QUERY, buf, fd);
	sscanf(buf, "%d", &rssi);
	fprintf(stderr, "RSSI=%d\n", rssi);
}


// The first time this is run, it will usually generate some echo data in the
//  serial buffer.  This is undesirable.  tcflush() will clear the buffer and
//  make sure it's sane for the rest of the program to function properly.
int setup_modem(int fd){
	int error = 0;
	error = write_to_imu(INIT_STRING, strlen(INIT_STRING), fd);
	if(error == -1) fprintf(stderr, "Setup errno=%d, %s\n", errno, strerror(errno));
	tcflush(fd, TCIFLUSH);
	return error;
}

// int sbdopensession(int fd)
//  Takes file descriptor of modem serial port.
//  Prints modem return data to stderr.
//  Returns MO Session Status value.
int sbdopensession(int fd){
	// at+sbdix
	unsigned char buf[MAX_BUFF];
	int len = -1;
	int mostat,momsn,mtstat,mtmsn,mtlen,mtqueued;
	bzero(buf, MAX_BUFF);
	len = imu_rw("at+sbdix\r\n", buf, fd); // XXX Could do with a local error check here.
	sscanf(buf, "+SBDIX:%d,%d,%d,%d,%d,%d", &mostat, &momsn, &mtstat, &mtmsn, &mtlen, 
		&mtqueued);
	fprintf(stderr, 
		"MO_STATUS=%d\n"
		"MO_SEQ_NUM=%d\n"
		"MT_STATUS=%d\n"
		"MT_SEQ_NUM=%d\n"
		"MT_LENGTH=%d\n"
		"MT_QUEUED=%d\n", mostat, momsn, mtstat, mtmsn, mtlen, mtqueued);
	return mostat;
}

// drop a text string into the MO buffer.
int sending_text(int fd, int len){
	int result = 0;
	unsigned char buf[MAX_BUFF] = { '\0' };
	result = read(STDIN_FILENO, buf, len);
	if (result != len) ; // XXX maybe not done reading?
	result = send_text_message(buf, len, fd);
}

// drop a binary blob into the MO buffer.
int sending_binary(int fd, int len){
	int result = 0;
	unsigned char checksum[2] = {0};
	unsigned char buf[MAX_BUFF] = {'\0'};
	result = read(STDIN_FILENO, buf, len);
	// in case read returns before len.
	if(result < len)
		result += read(STDIN_FILENO, &buf[result], (result - len));
	result = send_binary_data(buf, fd, result);
	return result;
}

//  This funciton is an example that runs through some
//  modem functions.  It's not meant for production.
int test_function(int fd){
	#define TESTSIZE 320
	#define wbstring "at+sbdwb=320\r\n"
	unsigned char buf[MAX_BUFF] = {'\0'};
	unsigned char buf2[MAX_BUFF] = { 0xFF };
	unsigned char checksum[2] = { 0 };

	unsigned int i, len, len2, calc_checksum;
	uint16_t trunc_checksum;

	fprintf(stderr, "This is a test function.\n");
	imu_rw("ati7\r\n", buf, fd);
	fprintf(stderr, "%s\n", buf);


	// put some test data in the MO buffer.  Two step process.
	// First tell the modem we want to send data.
	// at+sbdwb=<msg len>

	//  While the MO buffer can be up to 340 bytes, the MT buffer
	//  is supposedly smaller at 270 according to some documentation.
	
	// Even in quiet mode (q1), the modem will respond READY.
    write_to_imu(wbstring, strlen(wbstring), fd);
    i = read_from_imu(buf, fd);
    if (i < 5) 
    	fprintf(stderr, "Command sent.  Modem responded %s.\n", buf);
    else if (buf[0] = 'R')
	    fprintf(stderr, "MO Start confirmed.  Modem is %s\n", buf);

    calc_checksum = 0;
    for(i=0; i<TESTSIZE; i++) buf2[i] = 0xff;
	for(i = 0; i < TESTSIZE; i++)
		calc_checksum += buf2[i];
	fprintf(stderr, "\n");
	fprintf(stderr, "Checksum calculated to %d = 0x%x\n", calc_checksum, calc_checksum);
	trunc_checksum = calc_checksum;  //this should truncate it to 2 bytes
	fprintf(stderr, "Checksum truncated to %d = 0x%04x\n", trunc_checksum, trunc_checksum);

	// now I have to change trunc_checksum into a char...
	checksum[0] = trunc_checksum >> 8;
	checksum[1] = trunc_checksum;

	// write the message body and checksum into the waiting modem buffer.
	fprintf(stderr, "writing checksum 0x%02x 0x%02x\n", checksum[0], checksum[1]);
	write_to_imu(buf2, TESTSIZE, fd);
	write_to_imu(checksum, 2, fd);
	len = read_from_imu(buf, fd);
	// I should get one of three answers:
	//  0 means it worked.  1 means write timeout (maybe I didn't send enough?)
	//  2 means my checksum disagrees with the modem's checksum.
	//  3 means "message size is not correct", why not say message too long?
	//   Max send is 340 bytes, min send is 1 byte.  Does 3 mean message > 340
	//   or does it mean message > 270?  I don't see how it could be < 1 without
	//   triggering a result 1 timeout first.
	fprintf(stderr, "test data written to MO buffer, modem responded result=%s\n", buf);

	// move test data from MO buffer to MT buffer.
	// at+sbdtc
	//  Modem will return something that looks like this:
	//  SBDTC: Outbound SBD Copied to Inbound SBD: size = 0
	len2 = write_to_imu("at+sbdtc\r\n", strlen("at+sbdtc\r\n"), fd);
	fprintf(stderr, "Wrote at+sbdtc command to modem. %d bytes.\n", len2);

	len2 = read_from_imu(buf, fd);
	sscanf(buf, "SBDTC: Outbound SBD Copied to Inbound SBD: size =%d\n", &len2);
	fprintf(stderr, "%s\n", buf);

	fprintf(stderr, "------------------------------------------------------------------------\n");

	// erase buf before read.
	for(i=0; i==MAX_BUFF; i++)
		buf[i] = '\0';

	fprintf(stderr, "Reading binary data from modem.\n");
	len=read_binary_from_imu(buf, fd);

	fprintf(stderr, "Read %d bytes.\n", len);

	print_binary_data(buf, len);

	return 0;
}

static void usage(char* myname){
	fprintf(stderr, 
		"Usage: %s [options] ... \n"
		"embeddedTS SBD Control Utility\n"
		"Status output goes to stderr.  All other IO uses stdin/stdout.\n"
		"Options are executed in the order given on the command line.\n"
		"For example:\n"
		"sbdctl -D -c < myfile.bin\n"
		"This would read myfile.bin into the MO buffer, then initiate an SBD session\n"
		"(if possible) to transmit the data to the network.\n"
		"NOTE:  Maximum input is 340 bytes for either text or binary transmissions.\n"
		"NOTE2:  The MO and MT buffers can only contain one message each.\n"
		"\n"
		" -p, --port </dev/ttyEX1>  Define which serial port to use.\n"
		" -c, --connect             Connect to satellite and initiate SBD session.\n"
		" -t, --tread               Read text from SBD modem's receive buffer.\n"
		" -d, --dread               Read binary data from SBD modem's receive buffer.\n"
		" -T, --twrite <len>        Write <len> bytes text data from stdin to SBD modem's transmit buffer.\n"
		" -D, --dwrite <len>        Write <len> bytes binary data from stdin to SBD modem's tx buffer.\n"
		" -r, --rssi                Request new RSSI reading from SBD modem.\n"
		" -s, --status              Get local MO and MT message queue status.\n"
		" -e, --events              Enable unsolicited event reporting from modem.\n"
		" -i, --info                Dump modem-related info in BASH-compatible variables.\n"
		" -x, --indexes             Report message index number for MO and MT.\n"
		" -y, --clearindex          Clear Mobile Originated Message Sequence Number.\n"
		" -k, --clearbuffers        Clear both MO and MT buffers.\n"
		" -l, --clearmobuf          Clear Mobile Originated (MO) buffer.\n"
		" -m, --clearmtbuf          Clear Mobile Terminated (MT) buffer.\n"
		" -a, --cpymomtbuf          Copy mo to mt buffer on modem.\n"
		" -z, --test                Programmer's test point: Not for release version.\n"
		, myname);
}

int open_sbd_port(char* thePort){
		// open the serial port
	int temp;
	int fd=open(thePort, O_RDWR | O_NOCTTY );
	if(fd==-1){
		// process error.
		fprintf(stderr, "PORT_ERROR=%d\nERROR_STR=\"%s.\"\n", errno, strerror(errno));
		abort();
	}

	temp = serial_init(fd);
	if(temp ==-1){
		// handle error.
		fprintf(stderr, "INIT_ERROR=%d\nERROR_STR=\"%s.\"\n", errno, strerror(errno));
		abort();
	}

	temp = setup_modem(fd);
	if(temp == -1) {
		// handle error.
		fprintf(stderr, "SETUP_ERROR=%d\nERROR_STR=\"%s.\"\n", errno, strerror(errno));
		abort();
	}

	return fd;
}

// copies mobuf to mtbuf inside the modem.
// returns number of bytes copied from MO to MT.
int cpymomtbuf(int fd){
	unsigned char buf[MAX_BUFF];
	int len;
	write_to_imu("at+sbdtc\r\n", strlen("at+sbdtc\r\n"), fd);
	read_from_imu(buf, fd);
	sscanf(buf, "SBDTC: Outbound SBD Copied to Inbound SBD: size = %d", &len);
	fprintf(stderr, "MOMTCP_BYTES=%d\n", len);
	return len;
}

int main(int argc, char** argv)
{
	int fd = -1;  		// file handle for serial port.
	FILE * output_fd = NULL;
	int c;   			// return value of getopt_long.
	int longindex = 0; 	// getopt_long wants this.
	int temp = -1;
	char thePort[12] = {"/dev/ttyS12"};
	char* outfile = NULL;
	int bytes, result, len;

	// long options here.  format:
	// "option", argument (no_ or rquired_), flag pointer (or 0?), 'char' short opt.
	static struct option longopts[] = 
	{
		{"port",	required_argument, 	0, 'p'},  // /dev/tty...
		{"connect", no_argument, 		0, 'c'},  // at+sbdix -- initiate radio contact.
		{"tread",	no_argument, 		0, 't'},  // text read from modem MT buffer
		{"dread",	no_argument, 		0, 'd'},  // data read from modem MT buffer
		{"twrite", 	required_argument, 	0, 'T'},  // text write <len> bytes to modem MO buffer.
		{"dwrite",	no_argument, 		0, 'D'},  // data write <len> bytes to modem MO buffer with checksum.
		{"rssi",	no_argument,		0, 'r'},  // request rssi from modem
		{"status",	no_argument,		0, 's'},  // request modem status
		{"events", 	no_argument,		0, 'e'},  // request running events XXX remove this?
		{"info", 	no_argument,		0, 'i'},  // info grab
		{"clearbufs", no_argument, 		0, 'k'},  // Clear all message indexes
		{"clearmobuf", no_argument,		0, 'l'},  // Clear MO index
		{"clearmtbuf", no_argument,		0, 'm'},  // Clear MT index
		{"cpymomtbuf", no_argument,	    0, 'a'},  // XXX TEST cp MO buf to MT buf for testing.
		{"test",	no_argument,		0, 'z'},  // XXX TEST ARG <-------------------------<<<
		{0, 0, 0, 0}
	};

	// do until done
	while (1){
		c=getopt_long(argc, argv, "p:ctdT:D:rseiklmaz", longopts, &longindex);
		if((c == -1) && (argc == 1)){
			usage(argv[0]);
			return 1;  // Bail & fail if no options provided.
		}
		else if (c == -1) break; // detect end of options list and exit.

		switch(c){
			case 'p':
				strncpy(thePort, optarg, (sizeof(char)*11));
				fprintf(stderr, "SBDPORT=%s\n", thePort);
				break;
			case 'c':
				if(fd == -1) fd = open_sbd_port(thePort);
				sbdopensession(fd);
				break;
			case 't':
				if(fd == -1) fd = open_sbd_port(thePort);
				print_text_data(fd);
				break;
			case 'd':
				if(fd == -1) fd = open_sbd_port(thePort);
				dread(fd);
				break;
			case 'T':
				if(fd == -1) fd = open_sbd_port(thePort);
				sscanf(optarg, "%d", &len);
				if(len > MAX_BUFF) {
					fprintf(stderr, "Message length must be less than 340 bytes!");
					abort();
				}
				result = sending_text(fd, len);
				break;
			case 'D':
				if(fd == -1) fd = open_sbd_port(thePort);
				sscanf(optarg, "%d", &len);
				result = sending_binary(fd, len);
				break;
			case 'r':
				if(fd == -1)	fd = open_sbd_port(thePort);
				getsbdrssi(fd);
				break;
			case 's':
				if(fd == -1)	fd = open_sbd_port(thePort);
				getsbdstatus(fd);
				break;
			case 'e': // XXX modem can send unsolicited messages to sbc. (like rssi changes) 
					  // XXX  Not sure how to implement though.  Remove?
				break;
			case 'i':
				if(fd == -1)	fd = open_sbd_port(thePort);
				info(fd);
				break;
			case 'k':  // clear both mo and mt indexes
				if(fd == -1) 	fd = open_sbd_port(thePort);
				result = clearbufs(SBDD_CLEAR_ALL_BUFF, fd);
				if(result == 0)	fprintf(stderr, "SBD Buffers cleared!\n");
				else fprintf(stderr, "SBD Buffer clear failed.\n");
				break;
			case 'l':  // clear mo idx.
				if(fd == -1)	fd = open_sbd_port(thePort);
				result = clearbufs(SBDD_CLEAR_MO_BUFF, fd);
				if(result == 0)	fprintf(stderr, "SBD MO Buffer cleared!\n");
				else fprintf(stderr, "SBD Buffer clear failed.\n");
				break;
			case 'm':  // clear mt idx.
				if(fd == -1)	fd = open_sbd_port(thePort);
				result = clearbufs(SBDD_CLEAR_MT_BUFF, fd);
				if(result == 0)	fprintf(stderr, "SBD MT Buffer cleared!\n");
				else fprintf(stderr, "SBD Buffer clear failed.\n");
				break;
			case 'a':  // copy mo buffer to mt buffer.
				if(fd == -1)	fd = open_sbd_port(thePort);
				bytes = cpymomtbuf(fd);
				break;
			case 'z':
				if(fd == -1)	fd = open_sbd_port(thePort);
				test_function(fd);
				break;
			default:
				usage(argv[0]);
		}
	}
//  Cleanup:  Close the port, release any mmap'd variables, etc. 
	close(fd);
	return 0;
}
