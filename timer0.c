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

#include "includes.h"

// System tick counter
volatile uint64_t timer0_ticks64 = 0;

/*
 * Calculate a future delay time or time out in milliseconds
 */

void timer0_future_ms(uint32_t msec, uint64_t *future)
{
	uint64_t now;
	uint64_t x;
	
	// Critical section start
	cli();
	now = timer0_ticks64;
	sei();
	// Critical section end
	
	if(msec < 42) // For very short times, don't do adjustment calcs
		*future = now + msec;
	else{
		x = (msec * 1000ULL) / 1024ULL;
		*future = x;
	}
}

/*
 * Test a future delay time or time out
 */
 
int timer0_test_future_ms(uint64_t *future)
{
	int res;
	
	// Critical section start
	cli();
	res = (timer0_ticks64 >= *future);
	sei();
	// Critical section end
	
	return res;		
}

/*
 * Delay to a time in the future in milliseconds
 */
 

void timer0_delay_ms(uint32_t value)
{
	uint64_t future;
	
	timer0_future_ms(value, &future);
	while(FALSE == timer0_test_future_ms(&future));
}


/*
 * Make an elapsed time string from ticks
 */

void timer0_elapsed_time(char *elap, uint8_t size)
{
	uint64_t now;

	// Critical Section Start
	cli();
	now = timer0_ticks64;
	sei();
	
	now *= 10000;
	now /= 9765;
	// FIXME: Small printf doesn't seem to support uint64_t
	// elapsed time will overflow after appx. 1149 power on hrs.
	snprintf_P(elap, size, PSTR("%lu"), (uint32_t) now);
}
