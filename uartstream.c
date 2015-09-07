#include "uartstream.h"

/*
 * Allow UART0 to be used as a stream for printf, scanf, etc...
 */

// I/O functions

static int uartstream_putchar(char c, FILE *stream);
static int uartstream_getchar(FILE *stream);

// Stream declaration

static FILE uartstream_stdout = FDEV_SETUP_STREAM(uartstream_putchar, uartstream_getchar, _FDEV_SETUP_RW);

/*
 * Initialize the file handle, return the file handle
 */

FILE *uartstream_init(uint32_t baudrate)
{
	uart0_init(UART_BAUD_SELECT(baudrate, F_CPU));
	return &uartstream_stdout;
}	

/*
 * Write a char to the UART
 */

static int uartstream_putchar(char c, FILE *stream)
{
	if (c == '\n') 
		uartstream_putchar('\r', stream);
	uart0_putc((uint8_t) c);
	return 0;
}

/*
 * Read a character from the UART
 */

static int uartstream_getchar(FILE *stream)
{
	uint16_t res;
	while(1){
		res = uart0_getc();
		if(res < 256){
			if(res == '\r')
				res = '\n';
			break;
		}
	}
	return (int) (res & 0xFF);
}


