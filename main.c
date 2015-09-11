/*

  main.c 
  
  
  
*/

#include "includes.h"

/*
 * Constants
 */


#define IBASIC 1								// Basic current (A)
#define VREF 240								// Reference voltage (V)
#define IGAIN 24								// Current Gain
#define MVISAMPLE 1								// Millivolts across shunt resistor at basic current
#define MVVSAMPLE 248							// Millivolts at bottom tap of voltage divider at vref
#define MC 3200									// Metering pulse constant (impulses/kWh)
 
#define PLC ((838860800ULL*IGAIN*MVISAMPLE*MVVSAMPLE)/(1ULL*MC*VREF*IBASIC))	// Power Line Constant

#define MODE_WORD	0x3422						// Gain of 8 for current, rest are defaults

enum {PLCONSTH=0, PLCONSTL, LGAIN, LPHI, NGAIN, NPHI, PSTARTTH, PNOLTH, QSTARTTH, QNOLTH, MMODE};
enum {UGAIN = 0, IGAINL, IGAINN, UOFFSET, IOFFSETL, IOFFSETN, POFFSETL, QOFFSETL, POFFSETN, QOFFSETN};




/*
 * Data structures
 */
 
typedef struct {
	uint16_t sig;
	uint16_t meter_cal[11];						// Meter calibration data
	uint16_t measure_cal[10];					// Measurement calibration data
	uint16_t cal_crc;
} eeprom_cal_data_t;

typedef struct {
	unsigned send_measurement_records : 1;		// Send measurement records when enabled
} switches_t;


/*
 * EEPROM variables
 */
eeprom_cal_data_t EEMEM eecal_eemem;


/*
 * Static variables
 */
 
// Calibration data

static eeprom_cal_data_t eecal;


// Button data 

static button_data_t button1, button2, button3;

// System tick counter
static volatile uint64_t ticks = 0;

// Display mode
typedef enum {DISPMODE_SPLASH=0, DISPMODE_KW, 
	DISPMODE_CALIB} dispmode_t;
static dispmode_t dispmode;
// Calibration mode
typedef enum {CALMODE_OFF=0, CALMODE_SMALL_POWER, CALMODE_MEASUREMENT, 
	CALMODE_ENERGY} calmode_t;
static calmode_t calmode;

// System soft switches
static switches_t switches;

// U8clib data
static u8g_t u8g;


/*
 * Timer0 overflow interrupt
 * 
 * This happens every 1.024 milliseconds
 */

ISR(TIMER0_OVF_vect)
{
	ticks++;
	// Every 16 ticks, service the button list
	if(!(ticks & 0xF))
		button_service();		
}


/*
 * Calculate a future delay time or time out in milliseconds
 */

static void set_future_ms(uint32_t msec, uint64_t *future)
{
	uint64_t now;
	uint64_t x;
	
	// Critical section start
	cli();
	now = ticks;
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
 
static int test_future_ms(uint64_t *future)
{
	int res;
	
	// Critical section start
	cli();
	res = (ticks >= *future);
	sei();
	// Critical section end
	
	return res;		
}

/*
 * Delay to a time in the future in milliseconds
 */
 

void static delay_ms(uint32_t value)
{
	uint64_t future;
	
	set_future_ms(value, &future);
	while(FALSE == test_future_ms(&future));
}


/*
 * Make an elapsed time string from ticks
 */

void make_time(char *elap, uint8_t size)
{
	uint64_t now;

	// Critical Section Start
	cli();
	now = ticks;
	sei();
	
	now *= 10000;
	now /= 9765;
	// FIXME: Small printf doesn't seem to support uint64_t
	// elapsed time will overflow after appx. 1149 power on hrs.
	snprintf_P(elap, size, PSTR("%lu"), (uint32_t) now);
}

/* 
 * Calculate CRC over buffer using polynomial: X^16 + X^12 + X^5 + 1 
 */

static uint16_t calcCRC16(void *buf, int len)
{
	uint8_t i;
	uint16_t crc = 0;
	uint8_t *b = (uint8_t *) buf;
	

	while(len--){
		crc ^= (((uint16_t) *b++) << 8);
		for ( i = 0 ; i < 8 ; ++i ){
			if (crc & 0x8000)
				crc = (crc << 1) ^ 0x1021;
			else
				crc <<= 1;
          	}
	}
	return crc;
}



/*
 * Initialization function
 */
 

static void init(void)
{
#if defined(__AVR__)
	// select minimal prescaler (max system speed)
	CLKPR = 0x80;
	CLKPR = 0x00;
  
	// Initialize the serial port
	stdout = stdin = uartstream_init(9600);
  
	// Initialize the display
	u8g_InitHWSPI(&u8g, &u8g_dev_st7920_128x64_hw_spi, 
	PN(1, 1), U8G_PIN_NONE, U8G_PIN_NONE);
  
	// Initialize EM chip software SPI
	em_init(); 
  
	// Set up timer 0 for 1.024ms interrupts 
	TCCR0B |= (_BV(CS01) | _BV(CS00)); // Prescaler 16000000/64 =  250KHz
	TIMSK0 |= _BV(TOIE0); // Enable timer overflow interrupt
	TCNT0 = 0; // Zero out the timer
  
	// Add the buttons to the button handler
	button_add(&button1, &BUTTON_PINPORT, PIN_BUTTON1 , 1);
	button_add(&button2, &BUTTON_PINPORT, PIN_BUTTON2 , 2);
	button_add(&button3, &BUTTON_PINPORT, PIN_BUTTON3 , 3);
  
	// Enable global interrupts
	sei(); 
#endif
}


/*
 * Convert unsigned 16 bit integer to fixed point number
 */
 
static char *to_fixed_decimal_uint16(char *dest, uint8_t len, 
uint8_t places, uint16_t val){
	uint16_t rem;
	uint16_t quot;
	const char *format;
	
	if(places == 2){
		rem = val % 100;
		quot = val / 100;
		format = PSTR("%d.%02d");
	}
	else{
		rem = val % 1000;
		quot = val / 1000;
		format = PSTR("%d.%03d");
	}
	
	/* Generate string */
	
	snprintf_P(dest, len, format, quot, rem);
	return dest;
}

/*
 * Convert ones complement signed 16 bit integer to fixed point number
 */

static char *ones_compl_to_fixed_decimal_int16(char *dest, uint8_t len, 
uint8_t places, uint16_t val){
	int16_t rem;
	int16_t quot;
	const char *format = NULL;
	uint8_t negative = val & 0x8000 ? TRUE : FALSE;
	
	val = val & 0x7FFF;// Strip sign bit
	
	if(places == 1){
		rem = val%10;
		quot = val/10;
		format = PSTR("%d.%d");
	}
	else if(places == 2){
		rem = val%100;
		quot = val/100;
		format = PSTR("%d.%02d");

	}
	else{
		rem = val%1000;
		quot = val/1000;
		format = PSTR("%d.%03d");
	}
	
	// Set the sign of the integer part if negative
	
	if(negative)
		quot *= -1;
		
	// Generate string 
	snprintf_P(dest, len, format, quot, rem);
	
	return dest;
}


/*
 * Convert twos complement signed 16 bit integer to fixed point number
 */

static char *twos_compl_to_fixed_decimal_int16(char *dest, uint8_t len, 
uint8_t places, int16_t val){
	int16_t rem;
	int16_t quot;
	const char *format = NULL;
	
	if(places == 1){
		rem = val%10;
		quot = val/10;
		format = PSTR("%d.%d");
	}
	else if(places == 2){
		rem = val%100;
		quot = val/100;
		format = PSTR("%d.%02d");

	}
	else{
		rem = val%1000;
		quot = val/1000;
		format = PSTR("%d.%03d");
	}
	
	// Decimal part is always positive.
	
	if(val < 0)
		rem *= -1;
		
	// Generate string 
	snprintf_P(dest, len, format, quot, rem);
	
	return dest;
}


/*
 * Set a string to double dash followed by 4 spaces
 */
 
void set_doubledash(char *str)
{
	strcpy_P(str, PSTR("--    "));
}

/*
 * Clear the screen with an empty picture loop
 */

static void clear_screen(void)
{
	/* Clear display with an empty picture loop */ 
	u8g_FirstPage(&u8g);
				
    do{					
    } while ( u8g_NextPage(&u8g) );
}

/*
 * Draw string from program memory
 */
 
static u8g_uint_t drawstr_P(u8g_t *u8g, u8g_uint_t x, u8g_uint_t y, const char *s)
{
	char temp[32];
	
	return u8g_DrawStr(u8g, x, y, strcpy_P(temp, s));
}

/*
 * Draw splash screen
 */
 
static void draw_splash(void)
{
  u8g_SetFont(&u8g, u8g_font_5x7);
  drawstr_P(&u8g, 0, 24, PSTR("Energy Meter Version 0.0"));
  drawstr_P(&u8g, 24, 32, PSTR("Copyright 2015"));
  drawstr_P(&u8g, 0, 40, PSTR("Stephen Rodgers (HWSTAR)"));
  drawstr_P(&u8g, 10, 48, PSTR("All Rights Reserved"));	
}


/*
 * Draw meter data on graphic display
 */

static void draw_meter_data(char *volts, char *amps, char *kw, 
	char *kva, char *hz, char *pf, char *kvar, char *pa)
{
	
  u8g_SetFont(&u8g, u8g_font_helvR24n);
  u8g_DrawStr(&u8g, 0, 24, kw);
  u8g_SetFont(&u8g, u8g_font_5x7);
  drawstr_P(&u8g, 80, 24, PSTR("kW"));
  u8g_DrawStr(&u8g, 0, 32, volts); 
  drawstr_P(&u8g, 35, 32, PSTR("Vrms")); 
  u8g_DrawStr(&u8g, 60, 32, amps); 
  drawstr_P(&u8g, 95, 32, PSTR("Arms")); 
  u8g_DrawStr(&u8g, 0, 40, kva); 
  drawstr_P(&u8g, 35, 40, PSTR("kVA")); 
  u8g_DrawStr(&u8g, 60, 40, hz); 
  drawstr_P(&u8g, 95, 40, PSTR("Hz")); 
  u8g_DrawStr(&u8g, 0, 48, pf); 
  drawstr_P(&u8g, 35, 48, PSTR("PF")); 
  u8g_DrawStr(&u8g, 60, 48, kvar); 
  drawstr_P(&u8g, 95, 48, PSTR("kVAR")); 
  u8g_DrawStr(&u8g, 0, 56, pa); 
  drawstr_P(&u8g, 35, 56, PSTR("PH<")); 
  
  drawstr_P(&u8g, 0, 64, PSTR("Next"));
  
}

static bool valid_switch(char *line)
{
	if(strlen(line) >= 1){
		if((line[0] == '1')||(line[0] == '0'))
			return TRUE;
	}
	return FALSE;	
}

/*
 * Process a command line
 */
 

static void process_command(char *line)
{
	char *p;
	uint16_t reg,val;
	uint8_t len = strlen(line);
	
	if((len >= 2) && (line[0] == '[')){
		
		switch(line[1]){
			case 'm':
				if(valid_switch(line + 2)){
					if(line[2] == '1')
						switches.send_measurement_records = TRUE;
					else
						switches.send_measurement_records = FALSE;
					printf_P(PSTR("[m%c]\n"),line[2]);
				}
				break;
		
			case 'c':
				if(strlen(line) >= 3){
					// Calibration sub commands
					switch(line[2]){
						// Enter measurement calibration mode
						case 'm':
							em_write_transaction(EM_ADJSTART, 0x5678);						
							calmode = CALMODE_MEASUREMENT;
							printf_P(PSTR("[cm]\n"));
							
							break;
							
						case 'w':
							// Write a calibration register value to the em chip
							if(CALMODE_MEASUREMENT == calmode){
								if(line[3] == ':'){
									p = strchr(line + 4, ',');
									if(p){
										*p++ = 0;
										reg = (uint8_t) strtoul(line + 4, NULL, 16);
										val = (uint16_t) strtoul(p, NULL, 16);
										// Check to see if it is within bounds
										if((reg >= EM_UGAIN) && (reg <= EM_QOFFSETN)){
											printf_P(PSTR("[cw:%02X,%04X]\n"), reg, val);
											// Update value in memory
											eecal.measure_cal[reg - EM_UGAIN] = val;
											// Update value on the em chip
											em_write_transaction(reg, val);
										}
									}
								}
	
							}
							break;
							
						case 'r':
							// Read a calibration register value from the em chip
							if(CALMODE_MEASUREMENT == calmode){
								// Convert input string to register address
								if((line[3] == ':')){
									reg = (uint8_t) strtoul(line + 4, NULL, 16);
									// Check to see if it is within bounds
									//printf("foop1\n");
									if((reg >= EM_UGAIN) && (reg <= EM_QOFFSETN)){
										//printf("foop2\n");
										// Convert to an offset
										uint8_t index = reg - EM_UGAIN;
										printf_P(PSTR("[cr:%02X,%04X]\n"), reg, eecal.measure_cal[index]);
									}
								}
									
							}	
							break;
							
							
						case 's': 
						    // Write cal data to eeprom and exit
							if(calmode == CALMODE_MEASUREMENT){
								// Write it as a block to the chip so
								// a checksum can be calculated.
								uint16_t cs = em_write_block(EM_UGAIN, EM_QOFFSETN, eecal.measure_cal);
								// Write em chip checksuim
								em_write_transaction(EM_CS2, cs);
								// Exit measurement calibration
								em_write_transaction(EM_ADJSTART, 0x0);
								 // Write data back out to EEPROM
								eecal.cal_crc = calcCRC16(&eecal, (sizeof(eecal) - sizeof(uint16_t)));
								eeprom_update_block(&eecal, &eecal_eemem, sizeof(eecal));
								
								printf_P(PSTR("[cs]\n"));
							}
							clear_screen();
							calmode = CALMODE_OFF;
							break;
							
						default:
							break;
							
					}
				}
				break;
		
		
			default:
				break;
		}
	}				
}


/*
 * Build a command line from incoming characters
 */
 


static void serial_service(void)
{

	static char line[32];
	static uint8_t lpos = 0;
	uint16_t s;
	char c;
	
	s = uart0_peek();
	if(s == UART_NO_DATA)
		return; // Nothing to do...
	if(s > 0xFF){
		uart0_getc(); // Discard the error
		return;
	}
	c = (char) uart0_getc();
	
	if(c == '\r' || c == '\n'){
		if(lpos){
			line[lpos] = 0;
			lpos = 0;
			process_command(line);
	
		}
	}
	else if(lpos < 32){
		line[lpos] = c;
		lpos++;
	}
}


void check_buttons(void)
{
	uint8_t id, event;
	
	if(button_get_event(&id, &event)){

	}

}

/*
 * Main function
 */	


int main(void)
{
	static char volts[8], amps[8], kw[8], kva[8], hz[8], pf[8], kvar[8]; 
	static char pa[8];
	static char elap[32];
	uint16_t res;
	uint16_t cs;
	int16_t kvai;
	static uint64_t timer;
	
	

	
	init();
 
  
	em_write_transaction(0x00,0x789A); // Soft reset
    // Set splash time;
    set_future_ms(5000, &timer);
    
    
    eeprom_read_block(&eecal, &eecal_eemem, sizeof(eecal)); 
    res = calcCRC16(&eecal, (sizeof(eecal) - sizeof(uint16_t)));
    
    printf("Res: %04X, sig: %04X\n", res, eecal.sig);
    
    // Check state of calibration portion of EEPROM
    
	if((eecal.sig != 0x55AA) || (res != eecal.cal_crc)){ // BAD signature or bad CRC in EEPROM
		printf_P(PSTR("[s:EEPROM,AUTOINIT]\n"));
		// Read the defaults from the chip
		eecal.sig = 0x55AA;
		em_read_block(EM_PLCONSTL, EM_MMODE, eecal.meter_cal);
		em_read_block(EM_UGAIN, EM_QOFFSETN, eecal.measure_cal);
		// Change the power line constant
		eecal.meter_cal[PLCONSTL] = (uint16_t) PLC;
		eecal.meter_cal[PLCONSTH] = (uint16_t) (PLC >> 16);
		// Change the mode word
		eecal.meter_cal[MMODE] = MODE_WORD;
			
	    // Write data back out to EEPROM

		eecal.cal_crc = calcCRC16(&eecal, (sizeof(eecal) - sizeof(uint16_t)));
		printf("CRC: %04X\n", eecal.cal_crc);
		eeprom_update_block(&eecal, &eecal_eemem, sizeof(eecal));

	}	
  
    //Enter meter calibration
	em_write_transaction(EM_CALSTART, 0x5678);
    // Write out the meter cal values
    cs = em_write_block(EM_PLCONSTH, EM_MMODE, eecal.meter_cal);
    // Write CS1 checksum
    em_write_transaction(EM_CS1, cs);
    // Exit meter calibration
    em_write_transaction(EM_CALSTART, 0x8765); 
	
	// Enter measurement calibration
	em_write_transaction(EM_ADJSTART, 0x5678);
	// Write out the measurement calibration values
	cs = em_write_block(EM_UGAIN, EM_QOFFSETN, eecal.measure_cal);
	// Write the CS2 checksum
	em_write_transaction(EM_CS2, cs);
	// Exit measurement calibration
	em_write_transaction(EM_ADJSTART, 0);

	
	delay_ms(10);
	// Send meter status
	printf_P(PSTR("[s:MODEREG,%04X]\n"), em_read_transaction(EM_SYSSTATUS));
	
	

	for(;;){ 		
		/*
		* Main event loop
		*/
		
		// Handle splash screen  
		if((dispmode == DISPMODE_SPLASH) && test_future_ms(&timer)){
			dispmode = DISPMODE_KW;
			clear_screen();
		}
	 
		check_buttons();
		serial_service();
		
		// Decide what data to gather
		switch(dispmode){
		
			case DISPMODE_KW:
				// Get data
				// kW
				twos_compl_to_fixed_decimal_int16(kw,8,3, 
					(int16_t) em_read_transaction(EM_PMEAN));
	

				// Vrms
				to_fixed_decimal_uint16(volts, 8, 2,
					em_read_transaction(EM_URMS));

				// Irms
				to_fixed_decimal_uint16(amps, 8, 3,
					em_read_transaction(EM_IRMS));

				// Apparent power (kVA)
				kvai = (int16_t) em_read_transaction(EM_SMEAN);
				twos_compl_to_fixed_decimal_int16(kva,8,3, kvai);

				// Line frequency
				to_fixed_decimal_uint16(hz, 8, 2, 
					em_read_transaction(EM_FREQ));


				// For Power Factor, kVAR and Phase angle:
				// Only display these if there is apparent power
				if(kvai){
					// Power Factor
					ones_compl_to_fixed_decimal_int16(pf, 8, 3,
					em_read_transaction(EM_POWERF));
					// Reactive power (kVA)
					twos_compl_to_fixed_decimal_int16(kvar, 8, 3,
					em_read_transaction(EM_QMEAN));
					// Phase angle
					ones_compl_to_fixed_decimal_int16(pa, 8, 1, 
					em_read_transaction(EM_PANGLE));
				}
				else{
					// Display double dash when above are invalid
					set_doubledash(kvar);
					set_doubledash(pf);
					set_doubledash(pa);
				}
		
	

				// Send measurement record
				if(switches.send_measurement_records){
					make_time(elap, 32);
					printf_P(PSTR("[m: %s, %s, %s, %s, %s, %s, %s, %s, %s]\n"),
					elap, kw, volts, amps, kva, hz, pf, kvar, pa);
				}
	
			default:
			break;
		}
				
		/* Update display */ 
		u8g_FirstPage(&u8g);
				
		do{
			switch(dispmode){
				case DISPMODE_SPLASH:
					draw_splash();
					break;
				
				case DISPMODE_KW:
					draw_meter_data(volts, amps, kw, kva, hz, pf, 
						kvar, pa);
					break;
		
				default:
					break;
			}
							
		} while ( u8g_NextPage(&u8g) );
	}
}

