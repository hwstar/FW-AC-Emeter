#include "includes.h"

//
//		timer0.c
//
//		Copyright 2015 Stephen Rodgers
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; either version 3 of the License, or
//      (at your option) any later version.
//      
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//      
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
//      MA 02110-1301, USA.
//      
//

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
	if ('\n' == c) 
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
			if('\r' == res)
				res = '\n';
			break;
		}
	}
	return (int) (res & 0xFF);
}


