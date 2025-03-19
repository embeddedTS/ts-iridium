// sbdctl.h
// Header file for ts sbdctl utility
// Created 8 Oct 2019 by Michael D Peters
// Updated for clarity and completeness.

#ifndef SBDCTL_H
#define SBDCTL_H

#include <stdint.h>
#include <termios.h>

// Default Configuration Constants
#define BAUD B19200                 // Default baud rate for Iridium 9602
#define MAXBYTES 320                // Maximum message size without checksum
#define MAX_BUFF 350                // Maximum buffer size including checksum
#define RSSI_BAD 0                  // No signal strength
#define RSSI_FULL 5                 // Full signal strength
#define NO_NETWORK 0

// Serial Modes
#define TEXT_MODE 0
#define BIN_MODE 1

// Initialization and Configuration Commands
#define INIT_STRING "ate0v1&k0q1\r\n"       // Echo off, verbose on, handshake off, quiet on
#define EVENTS_ENABLE "at+cier=1\r\n"        // Enable event reporting
#define EVENTS_DISABLE "at+cier=0\r\n"       // Disable event reporting

// Queries and Data Handling Commands
#define RSSI_QUERY "at+csq\r\n"              // Query RSSI
#define IMEI_QUERY "at+gsn\r\n"              // Query IMEI
#define SBD_WRITE_BINARY "at+sbdwb=%d\r\n"   // Write binary data (length without checksum)
#define SBD_READ_BINARY "at+sbdrb\r\n"       // Read binary data
#define SBD_WRITE_TEXT_INLINE "at+sbdwt=%s\r\n" // Inline text write (<120 chars)
#define SBD_WRITE_TEXT "at+sbdwt\r\n"        // Write text (up to 340 chars, terminated by CR)
#define SBD_READ_TEXT "at+sbdrt\r\n"         // Read text data

// Buffer Clearing Options
#define SBDD_CLEAR_MO_BUFF 0
#define SBDD_CLEAR_MT_BUFF 1
#define SBDD_CLEAR_ALL_BUFF 2

// Function Prototypes

// Setup
int serial_init(int fd);
int set_serial_mode(int fd, int option);
int setup_modem(int fd);

// String Handling
int strip(char* buf, int size);
int check_binary(char* buf, int buf_len);

// Communication
int write_to_imu(const char* buf, int size, int fd);
int read_binary_from_imu(unsigned char* buf, int fd);
int read_from_imu(unsigned char* buf, int fd);
int imu_rw(const char* command, char* buf, int fd);

// RSSI Handling
int get_rssi(int fd);
void getsbdrssi(int fd);

// Modem Information
void info(int fd);

// Message Handling
int send_text_message(char* themessage, int length, int fd);
int send_binary_data(char* buf, int fd, int len);
int get_text_data(char* buf, int fd);
int print_text_data(int fd);
void print_binary_data(char* buf, int len);
void dread(int fd);

// Buffer Management
int clearbufs(int instruction, int fd);
void getsbdstatus(int fd);
int cpymomtbuf(int fd);

// Session Management
int sbdopensession(int fd);

// Sending Data
int sending_text(int fd, int len);
int sending_binary(int fd, int len);

// Utility and Testing
int test_function(int fd);
int open_sbd_port(char* thePort);

#endif // SBDCTL_H
