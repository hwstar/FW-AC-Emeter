/*

  main.c 
  
  
*/


#if defined(__AVR__)
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>
#endif

#include <stdio.h>

#include "u8g.h"
#include "em.h"
#include "uart.h"
#include "uartstream.h"

#define IBASIC 1								// Basic current (A)
#define VREF 240								// Reference voltage (V)
#define IGAIN 24								// Current Gain
#define MVISAMPLE 1								// Millivolts across shunt resistor at basic current
#define MVVSAMPLE 248							// Millivolts at bottom tap of voltage divider at vref
#define MC 3200									// Metering pulse constant (impulses/kWh)
 
#define PLC ((838860800ULL*IGAIN*MVISAMPLE*MVVSAMPLE)/(1ULL*MC*VREF*IBASIC))	// Power Line Constant

#define MODE_WORD	0x3422						// Gain of 8 for current, rest are defaults

	
static uint16_t cal[11] = {
	((uint16_t) (PLC >> 16)), 	// 0x21 High word of power line constant
	((uint16_t) PLC), 	// 0x22 Low word of power line constant
	0x0000,		// 0x23 L line calibration gain
	0x0000,		// 0x24 L line calibration angle
	0x0000,		// 0x25 N line calibration gain 
	0x0000,		// 0x26 N line calibration angle
	0x08BD,		// 0x27 Active power startup threshold
	0x0000,		// 0x28 Active no load power threshold
	0x0AEC,		// 0x29 Reactive start up power threshold
	0x0000,		// 0x2A Reactive no load power threshold
	MODE_WORD	// 0x2B Metering mode configuration
};

u8g_t u8g;

void init(void)
{
#if defined(__AVR__)
  // select minimal prescaler (max system speed)
  CLKPR = 0x80;
  CLKPR = 0x00;
  
 
  
  // Initialize the serial port
  stdout = stdin = uartstream_init(9600);
  
  // Initialize the display
  u8g_InitHWSPI(&u8g, &u8g_dev_st7920_128x64_hw_spi, PN(1, 1), U8G_PIN_NONE, U8G_PIN_NONE);
  
   // Initialize EM chip software SPI
  em_init(); 
  
  // Enable global interrupts
  sei(); 
#endif
}


/*
 * Convert unsigned 16 bit integer to fixed point number
 */
 
char *to_fixed_decimal_uint16(char *dest, uint8_t len, uint8_t places, uint16_t val){
	uint16_t rem;
	uint16_t quot;
	char *format;
	
	if(places == 2){
		rem = val % 100;
		quot = val / 100;
		format = "%d.%02d";
	}
	else{
		rem = val % 1000;
		quot = val / 1000;
		format = "%d.%03d";
	}
	
	/* Generate string */
	
	snprintf(dest, len, format, quot, rem);
	return dest;
}

/*
 * Convert signed 16 bit integer to fixed point number
 */

char *to_fixed_decimal_int16(char *dest, uint8_t len, uint8_t places, int16_t val){
	int16_t rem;
	int16_t quot;
	char *format = NULL;
	
	if(places == 1){
		rem = val%10;
		quot = val/10;
		format = "%d.%d";
	}
	else if(places == 2){
		rem = val%100;
		quot = val/100;
		format = "%d.%02d";

	}
	else{
		rem = val%1000;
		quot = val/1000;
		format = "%d.%03d";
	}
	
	/* Decimal part is always positive */
	
	if(rem < 0)
		rem *= -1;
		
	/* Generate string */
	snprintf(dest, len, format, quot, rem);
	
	return dest;
}



/*
 * Draw meter data on graphic display
 */

void draw_meter_data(char *volts, char *amps, char *kw, 
	char *kva, char *hz, char *pf, char *kvar, char *pa)
{
	
  u8g_SetFont(&u8g, u8g_font_helvR24n);
  u8g_DrawStr(&u8g, 0, 24, kw);
  u8g_SetFont(&u8g, u8g_font_5x7);
  u8g_DrawStr(&u8g, 80, 24, "kW");
  u8g_DrawStr(&u8g, 0, 32, volts); 
  u8g_DrawStr(&u8g, 35, 32, "Vrms"); 
  u8g_DrawStr(&u8g, 60, 32, amps); 
  u8g_DrawStr(&u8g, 95, 32, "Arms"); 
  u8g_DrawStr(&u8g, 0, 40, kva); 
  u8g_DrawStr(&u8g, 35, 40, "kVA"); 
  u8g_DrawStr(&u8g, 60, 40, hz); 
  u8g_DrawStr(&u8g, 95, 40, "Hz"); 
  u8g_DrawStr(&u8g, 0, 48, pf); 
  u8g_DrawStr(&u8g, 35, 48, "PF"); 
  u8g_DrawStr(&u8g, 60, 48, kvar); 
  u8g_DrawStr(&u8g, 95, 48, "kVAR"); 
  u8g_DrawStr(&u8g, 0, 56, pa); 
  u8g_DrawStr(&u8g, 35, 56, "PH<"); 
  
  u8g_DrawStr(&u8g, 0, 64, "Next");
  
}

/*
 * Top function
 */	


int main(void)
{
	static char volts[8], amps[8], kw[8], kva[8], hz[8], pf[8], kvar[8]; 
	static char pa[8];
	uint16_t cs;

	
    init();
  
  
	em_write_transaction(0x00,0x789A); // Soft reset
	_delay_ms(2000);
  
    //Enter meter calibration
	em_write_transaction(0x20, 0x5678);
    // Write out the meter cal values
    cs = em_write_block(EM_PLCONSTH, EM_MMODE, cal);
    // Write CS1 checksum
    em_write_transaction(EM_CS1, cs);
    // Exit meter calibration
    em_write_transaction(0x20, 0x8765); 
	
	printf ("Powerline Constant: %04X%04X\n", 
	em_read_transaction(EM_PLCONSTH),
	em_read_transaction(EM_PLCONSTL));
	
	printf ("Metering Mode: %04X\n\n", em_read_transaction(EM_MMODE));
	
	em_write_transaction(EM_ADJSTART, 0x5678);
	em_write_transaction(EM_IGAINL, 0x4AE4); // Calibrate current
	
  for(;;)
  { 


	/* Get data */

	// kW
	to_fixed_decimal_int16(kw,8,3, 
		(int16_t) em_read_transaction(EM_PMEAN));
	
	// Vrms
	to_fixed_decimal_uint16(volts, 8, 2, em_read_transaction(EM_URMS));
	
	// Irms
	to_fixed_decimal_uint16(amps, 8, 3, em_read_transaction(EM_IRMS));
	
	// Apparent power (kVA)
	to_fixed_decimal_int16(kva,8,3, 
		(int16_t) em_read_transaction(EM_SMEAN));
	
	// Line frequency
	to_fixed_decimal_uint16(hz,8,2,em_read_transaction(EM_FREQ));
	
	// Power Factor
	to_fixed_decimal_int16(pf, 8, 3, em_read_transaction(EM_POWERF));
	
	// Reactive power (kVA)
	
	to_fixed_decimal_int16(kvar, 8, 3, em_read_transaction(EM_QMEAN));
	
	// Phase angle
	to_fixed_decimal_int16(pa, 8, 1, em_read_transaction(EM_PANGLE));

	/* Update display */
		 
    u8g_FirstPage(&u8g);
    do
    {

      draw_meter_data(volts, amps, kw, kva, hz, pf, kvar, pa);
    } while ( u8g_NextPage(&u8g) );

    u8g_Delay(250);
  }
  
}
