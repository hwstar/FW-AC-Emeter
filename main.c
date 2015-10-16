/*
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

  main.c 
  
  
*/

#include "includes.h"

/*
 * Constants
 */

#define NUM_JSON_TOKENS 8						// Maximum number of json tokens to use with parser (keep small, eats RAM).

#define IBASIC 1								// Basic current (A)
#define VREF 240								// Reference voltage (V)
#define IGAIN 8									// Current Gain
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
	uint16_t sig;								// EEPROM signature for calibration data
	uint16_t meter_cal[11];						// Meter calibration data
	uint16_t measure_cal[10];					// Measurement calibration data
	uint16_t cal_crc;							// CRC of the calibration data
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



// Display mode
typedef enum {DISPMODE_SPLASH=0, DISPMODE_MAIN_MENU, DISPMODE_KVA, DISPMODE_KW, 
	DISPMODE_ARMS, DISPMODE_VRMS} dispmode_t;
static dispmode_t dispmode, dispmode_saved;

// Total forward active energy
static uint32_t fae_total;

// U8clib data
static u8g_t u8g;

// The convoluted mess of putting the Menu data in program space
const char mmi1[] PROGMEM = "Reset kWh Counter";
const char mmiend[] PROGMEM = "";


PGM_P const main_menu_strings[] PROGMEM = {
	mmi1,
	mmiend
};

// Button labels
const char bl_select[] PROGMEM = "Select";
const char bl_next[] PROGMEM = "Next";
const char bl_exit[] PROGMEM = "Exit";

// Menu data structure declarations

static menu_buttons_t bt_left;
static menu_buttons_t bt_middle;
static menu_buttons_t bt_right;
static menu_t main_menu;
static char volts[8], amps[8], kw[8], kva[8], hz[8], pf[8], kvar[8]; 
static char pa[8], kwh[10];
static char elap[32];

/*
 * Timer0 overflow interrupt
 * 
 * This happens every 1.024 milliseconds
 */

ISR(TIMER0_OVF_vect)
{
	timer0_ticks64++;
	// Every 16 ticks, service the button list
	if(!(timer0_ticks64 & 0xF))
		button_service();		
}



/*
 * Debug function. Dump registers as a set
 */

void dump_words(const uint16_t *buffer, uint8_t addr, uint8_t len)
{
	for(uint8_t i = 0; i < len; i++, addr++)
		printf_P(PSTR("%02X:%04X\n"), addr, buffer[i]);
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

/**
 * Convert a hex string into a 16 bit unsigned integer
 */

static uint8_t str2hex(uint16_t *val, const char *str)
{
	uint8_t i;
	char c;
	
	if(!val)
		return FALSE;

	*val = 0;
	for(i = 0; i < 4; i++){
		c = str[i];
		if(!c)
			break;
		*val <<= 4;
		if((c >= '0') && (c <= '9'))
			*val |= (c - '0');
		else if((c >= 'A') && (c <= 'F'))
			*val |= (c - 0x37);
		else if((c >= 'a') && (c <= 'f'))
			*val |= (c - 0x57);
		else
			return FALSE;
	}
	return TRUE;
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
	
	if(2 == places){
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
 * Convert twos complement signed 16 bit integer to fixed point number
 */

static char *twos_compl_to_fixed_decimal_int16(char *dest, uint8_t len, 
uint8_t places, int16_t val){
	int16_t rem;
	int16_t quot;
	const char *format = NULL;
	
	if(1 == places){
		rem = val%10;
		quot = val/10;
		format = PSTR("%d.%d");
	}
	else if(2 == places){
		rem = val%100;
		quot = val/100;
		format = PSTR("%d.%02d");

	}
	else{
		rem = val%1000;
		quot = val/1000;
		format = PSTR("%d.%03d");
	}
	
	// Adjust value when it is negative.
	
	if(val < 0){
		quot *= -1;
		rem *= -1;
		dest[0] = '-';
	}
		
	// Generate string 
	snprintf_P((val < 0) ? dest + 1 : dest, len, format, quot, rem);
	
	return dest;
}

/*
 * Convert ones complement signed 16 bit integer to fixed point number
 */

static char *ones_compl_to_fixed_decimal_int16(char *dest, uint8_t len, 
uint8_t places, uint16_t val){
	int16_t twos_compl;
	uint8_t negative = val & 0x8000 ? TRUE : FALSE;
	
	twos_compl = val & 0x7FFF;// Strip sign bit
	
	if(negative)
		twos_compl *= -1;
	
	return twos_compl_to_fixed_decimal_int16(dest, len, places, twos_compl);
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
 * Reset kWh
 */
 
static void reset_kwh(void)
{
	em_read_transaction(EM_APENERGY);
	fae_total = 0UL;
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
	drawstr_P(&u8g, 0, 20, PSTR("Energy Meter Version 0.0"));
	drawstr_P(&u8g, 24, 30, PSTR("Copyright 2015"));
	drawstr_P(&u8g, 0, 40, PSTR("Stephen Rodgers (HWSTAR)"));
	drawstr_P(&u8g, 10, 50, PSTR("All Rights Reserved"));	
}

/*
 * Draw meter data on graphic display
 */

static void draw_meter_data(char *volts, char *amps, char *kw, 
	char *kva, char *hz, char *pf, char *kvar, char *pa, char *kwh)
{
	
	const char *l_kw = PSTR("kW");
	const char *l_vrms = PSTR("Vrms");
	const char *l_arms = PSTR("Arms");
	const char *l_kva = PSTR("kVA");
	const char *l_hz = PSTR("Hz");
	const char *l_pf = PSTR("PF");
	const char *l_kvar = PSTR("kVAR");
	const char *l_ph = PSTR("PH<");
	const char *l_kwh = PSTR("kWh");
	const uint8_t column1 = 0;
	const uint8_t column2 = 35;
	const uint8_t column3 = 60;
	const uint8_t column4 = 105;
	const uint8_t line1 = 24;
	const uint8_t line2 = 32;
	const uint8_t line3 = 40;
	const uint8_t line4 = 48;
	const uint8_t line5 = 56;
	const uint8_t line6 = 64;
	
	
	
	
	

	switch(dispmode){
		case DISPMODE_KW:
			u8g_SetFont(&u8g, u8g_font_helvR24n);
			u8g_DrawStr(&u8g, column1, line1, kw);
			u8g_SetFont(&u8g, u8g_font_5x7);
			drawstr_P(&u8g, column4, line1, l_kw);
			u8g_DrawStr(&u8g, column1, line2, volts); 
			drawstr_P(&u8g, column2, line2, l_vrms); 
			u8g_DrawStr(&u8g, column3, line2, amps); 
			drawstr_P(&u8g, column4, line2, l_arms); 
			u8g_DrawStr(&u8g, column1, line3, kva); 
			drawstr_P(&u8g, column2, line3, l_kva);
			 

			break;
			
		case DISPMODE_KVA:
			u8g_SetFont(&u8g, u8g_font_helvR24n);
			u8g_DrawStr(&u8g, column1, line1, kva);
			u8g_SetFont(&u8g, u8g_font_5x7);
			drawstr_P(&u8g, column4, line1, l_kva);
			u8g_DrawStr(&u8g, column1, line2, volts); 
			drawstr_P(&u8g, column2, line2, l_vrms); 
			u8g_DrawStr(&u8g, column3, line2, amps); 
			drawstr_P(&u8g, column4, line2, l_arms); 
			u8g_DrawStr(&u8g, column1, line3, kw); 
			drawstr_P(&u8g, column2, line3, l_kw);
			 

			break;
			
		case DISPMODE_ARMS:
			u8g_SetFont(&u8g, u8g_font_helvR24n);
			u8g_DrawStr(&u8g, column1, line1, amps);
			u8g_SetFont(&u8g, u8g_font_5x7);
			drawstr_P(&u8g, column4, line1, l_arms);
			u8g_DrawStr(&u8g, column1, line2, volts); 
			drawstr_P(&u8g, column2, line2, l_vrms); 
			u8g_DrawStr(&u8g, column3, line2, kva); 
			drawstr_P(&u8g, column4, line2, l_kva); 
			u8g_DrawStr(&u8g, column1, line3, kw); 
			drawstr_P(&u8g, column2, line3, l_kw);

			break;
			
		case DISPMODE_VRMS:
			u8g_SetFont(&u8g, u8g_font_helvR24n);
			u8g_DrawStr(&u8g, column1, line1, volts);
			u8g_SetFont(&u8g, u8g_font_5x7);
			drawstr_P(&u8g, column4, line1, l_vrms);
			u8g_DrawStr(&u8g, column1, line2, amps); 
			drawstr_P(&u8g, column2, line2, l_arms); 
			u8g_DrawStr(&u8g, column3, line2, kva); 
			drawstr_P(&u8g, column4, line2, l_kva); 
			u8g_DrawStr(&u8g, column1, line3, kw); 
			drawstr_P(&u8g, column2, line3, l_kw);

			break;
				
		default:
			break;
	}
	
		
	// These fields stay the same from page to page		 
	u8g_DrawStr(&u8g, column3, line3, hz); 
	drawstr_P(&u8g, column4, line3, l_hz); 
	u8g_DrawStr(&u8g, column1, line4, pf); 
	drawstr_P(&u8g, column2, line4, l_pf); 
	u8g_DrawStr(&u8g, column3, line4, kvar); 
	drawstr_P(&u8g, column4, line4, l_kvar); 
	u8g_DrawStr(&u8g, column1, line5, pa); 
	drawstr_P(&u8g, column2, line5, l_ph); 
	u8g_DrawStr(&u8g, column3, line5, kwh);
	drawstr_P(&u8g, column4, line5, l_kwh);
	
	// Soft buttons
	drawstr_P(&u8g, 8, line6, PSTR("Next"));
	drawstr_P(&u8g, 100, line6, PSTR("Menu"));
  
}

/*
 * Search for a key in the json string.
 * Return -1 if not found, or the key index if found
 * 
 */

int16_t json_key_index(const char *json, jsmntok_t *tokens, PGM_P key)
{
	uint8_t i;
	// Test every other token starting with the second token.
	for(i = 1; i < NUM_JSON_TOKENS; i += 2){
		if(tokens[i].type == JSMN_STRING){
			uint8_t len = tokens[i].end - tokens[i].start;
			if(strlen_P(key) == len){
				if(!strncmp_P(json + tokens[i].start, key, len)){
					return i;
				}
			}
		}			
	}
	return -1;
}

/*
 * Copy the json value from the json string
 */

void json_value(const char *json, jsmntok_t *tokens, int16_t index, char *value, uint8_t vslen)
{
	uint8_t copylen;
	// If user passed in NULL for value, return
	if(!value)
		return;
	value[0] = 0;
	// if User passed in zero length, return
	if(!vslen)
		return;
	if(tokens[index].type == JSMN_STRING){
		// Get actual string length
		copylen = tokens[index].end - tokens[index].start;
		// Clip to buffer length - 1;
		if(copylen > vslen - 1)
			copylen = vslen - 1;
		// Copy the string to user supplied string pointer
		strncpy(value, json + tokens[index].start, copylen);
		value[copylen] = 0; // Add terminating character
	}
}

/*
 * Perform register command
 */
static void do_register_command(const char *line, jsmntok_t *tokens)
{
	int16_t addrtok, valuetok;
	char addr_s[3], value_s[5];
	uint16_t addr, value;
	
	// Check for address 
	
	addrtok = json_key_index(line, tokens, PSTR("addr"));
	if(addrtok < 1)
		return; // Address not present
		
	// Extract address
	json_value(line, tokens, addrtok + 1, addr_s, sizeof(addr_s));
	if(!str2hex(&addr, addr_s))
		return; // Bad address

	if(addr > 0x6F)
		return; // Address out of range
		
	printf_P(PSTR("Addr: %s\n"), addr_s);
	
	// Get the value index if it exists
	valuetok = json_key_index(line, tokens, PSTR("value"));

	// If value specified, then it is a write
	if(valuetok > 0){
		uint8_t offset;
		uint16_t cs;
		// Check for valid write address
		if((addr != 0) && (addr != 2) && (addr != 3)  && (addr != 4)){
			if((addr < 0x21) || (addr > 0x3B))
				return;
			if((addr > 0x2B) && (addr < 0x30))
				return;
		}
	
			
		// Extract value
		json_value(line, tokens, valuetok + 1, value_s, sizeof(value_s));
		if(!str2hex(&value, value_s))
		return; // Bad value
		
		// Determine range
		
		if(addr < 0x20){
			// Status and special registers
			// Write not implemented
			return;
		}
			
		else if(addr < 0x30){			
			// Metering calibration range
			offset = addr - EM_CAL_FIRST;
			eecal.meter_cal[offset] = value;
			// Unlock meter cal
			em_write_transaction(EM_CALSTART, 0x5678);
			// Rewrite the block to the em chip
			cs = em_write_block(EM_CAL_FIRST, EM_CAL_LAST, eecal.meter_cal);
			// Write the new checksum
			em_write_transaction(EM_CS1, cs);
			// Lock the meter cal
			em_write_transaction(EM_CALSTART, 0x8765);
		}
		
		else{
			// Measurement calibration range
			offset = addr - EM_MEAS_FIRST;
			eecal.measure_cal[offset] = value;
			// Unlock meter cal
			em_write_transaction(EM_ADJSTART, 0x5678);
			// Rewrite the block to the em chip
			cs = em_write_block(EM_MEAS_FIRST, EM_MEAS_LAST, eecal.measure_cal);
			// Write the new checksum
			em_write_transaction(EM_CS2, cs);
			// Lock the meter cal
			em_write_transaction(EM_ADJSTART, 0x8765);
		}
		// Update EEPROM
		eecal.cal_crc = calcCRC16(&eecal, (sizeof(eecal) - sizeof(uint16_t)));
		eeprom_update_block(&eecal, &eecal_eemem, sizeof(eecal));
	}
	else{
			// Read value from em chip
			value = em_read_transaction(addr);
	}
	printf_P(PSTR("{\"value\":\"%04X\"}\n"), value);
}
	

/*
 * Process a command line
 */

 
static void process_command(char *line)
{
	//char *p;
	//uint16_t reg,val;
	//uint8_t len = strlen(line);
	//static uint8_t upper = 0, lower = 0;
	//static uint16_t *cal_data = NULL;
	//static uint16_t dump_buf[16];
	//uint16_t cs;
	jsmnerr_t json_res;
	jsmn_parser json_parser;
	int16_t res;
	char command[12];
	jsmntok_t tokens[NUM_JSON_TOKENS];
	
	// Initialize JSON parser
	jsmn_init(&json_parser);
	
	// Parse line of text as a json object
	json_res = jsmn_parse(&json_parser, line, strlen(line), tokens, NUM_JSON_TOKENS);
	
	if (json_res < 0) {
		//printf_P(PSTR("Failed to parse JSON: %d\n"), json_res);
		//printf_P(PSTR("Line: %s\n"), line);
		return;
	}

	// First item must be an object 
	if (json_res < 1 || tokens[0].type != JSMN_OBJECT) {
		//printf_P(PSTR("Object expected\n"));
		return;
	}
	
	// Check for command string
	res = json_key_index(line, tokens, PSTR("command"));
	if(res < 0){
		//printf_P(PSTR("command not present\n"));
		return;
	}
	// Extract command keyword
	json_value(line, tokens, res + 1, command, sizeof(command));
	
	// Decode JSON command */
	
	if(!strcmp_P(command, PSTR("query"))){
		printf_P(PSTR("{\"elap\":\"%s\",\"irms\":\"%s\",\"urms\":\"%s\",\"pmean\":\"%s\",\"qmean\":\"%s\",\"freq\":\"%s\",\"powerf\":\"%s\",\"pangle\":\"%s\",\"smean\":\"%s\",\"kwh\":\"%s\"}"),
			elap, amps, volts, kw, kvar, hz, pf, pa, kva, kwh);
		// Query command
	}
	if(!strcmp_P(command, PSTR("resetkwh"))){
		reset_kwh();
		// Query command
	}
	if(!strcmp_P(command, PSTR("register"))){
		do_register_command(line, tokens);
		// Query command
	}

				
}


/*
 * Build a command line from incoming characters
 */
 


static void serial_service(void)
{

	static char line[64];
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
	
	if(('\r' == c) || ('\n' == c)){
		if(lpos){
			line[lpos] = 0;
			lpos = 0;
			process_command(line);
	
		}
	}
	else if(lpos < sizeof(line)){
		line[lpos] = c;
		lpos++;
	}
}

/* 
 * Check button event queue and act on a button
 */

void check_buttons(void)
{
	uint8_t id, event;
	uint8_t show_data = ((dispmode >= DISPMODE_KVA) && (dispmode <= DISPMODE_VRMS));
	
	if(button_get_event(&id, &event)){
		// If displaying data
		if(show_data){
			// If button #1 is released
			if((id == 1) && (event == BUTTON_EVENT_RELEASED)){
				clear_screen();
				// Advance to next display screen
				switch(dispmode){
					case DISPMODE_KW:
						dispmode = DISPMODE_KVA;
						break;
						
					case DISPMODE_KVA:
						dispmode = DISPMODE_ARMS;
						break;
						
					case DISPMODE_ARMS:
						dispmode = DISPMODE_VRMS;
						break;
						
					case DISPMODE_VRMS:
						dispmode = DISPMODE_KW;
						break;
																
					default:
						dispmode = DISPMODE_KVA;
						break;
						
				}
			}
			if((id == 3) && (event == BUTTON_EVENT_RELEASED)){ /* Menu */
				// Clear screen 
				clear_screen();
				// Set display mode
				dispmode_saved = dispmode;
				dispmode = DISPMODE_MAIN_MENU;
				// Show menu
				menu_show(&main_menu, 0);
			}
				
		}
		else{ /* Not showing data, must be in the menu system */
			switch(dispmode){
				case DISPMODE_MAIN_MENU:
					// Act on main menu event
					if(BUTTON_EVENT_RELEASED == event){
						switch(id){
							case 1: // Next
								menu_next(&main_menu);
								break;
							
							case 2: // Select
								if(menu_selected(&main_menu) == 0){
									reset_kwh();
								}
								// Lack of break deliberate
									
							case 3: // Exit
								clear_screen();
								dispmode = dispmode_saved;	
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
}

void gather_data(void)
{


	uint32_t calc_kwh;
	int16_t kvai;

	
			
	// Decide what data to gather
	switch(dispmode){
		
		case DISPMODE_MAIN_MENU:
		case DISPMODE_KW:
		case DISPMODE_KVA:
		case DISPMODE_ARMS:
		case DISPMODE_VRMS:
		
		
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
				
			// KWH
			uint16_t fae =  em_read_transaction(EM_APENERGY);
			
			// Add what was read to the total.
			fae_total += fae;
		
			// KWH is equivalent to  fae_total divided by MC integer pulses 
			// Since the fractional pulses are included in fae_total,
			// we need to account for them.  We do this by multiplying
			// by 1000 so that we get a kwh number which can be represented
			// with 4 decimal digits.
			//
			calc_kwh = ((fae_total * 1000L)/ MC);
			sprintf_P(kwh, PSTR("%03d.%04d"),((uint16_t) calc_kwh / 10000), ((uint16_t) calc_kwh % 10000));
		


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
			
	
		  	timer0_elapsed_time(elap, 32);
		
		default:
			break;
	}
	
		
}

static void update_display(void)
{
	/*
	 * Picture loop
	 */

	u8g_FirstPage(&u8g);
			
	do{
		switch(dispmode){
			case DISPMODE_SPLASH:
				draw_splash();
				break;
				
			case DISPMODE_MAIN_MENU:
				menu_draw(&main_menu, &u8g);
				break;
			
			case DISPMODE_KW:
			case DISPMODE_KVA:
			case DISPMODE_VRMS:
			case DISPMODE_ARMS:
				draw_meter_data(volts, amps, kw, kva, hz, pf, 
					kvar, pa, kwh);
				break;
	
	
			default:
				break;
		}
						
	} while ( u8g_NextPage(&u8g) );
}



/*
 * Main function
 */	


int main(void)
{
	uint16_t res;
	uint16_t cs;
	static uint64_t timer;
	
	
	init();
 
  
    // Set splash time;
    timer0_future_ms(5000, &timer);
    
    // Per Atmel app note AN-643, change the Temperature coefficient from 0x8077 to 0x8097
    _delay_us(20000);
	em_write_transaction(EM_CALSTART, 0x9779);
	em_write_transaction(EM_TCOEFF_ADJ, 0x8097);
	em_write_transaction(EM_CALSTART, 0x8765);
	_delay_us(100000);
    
    eeprom_read_block(&eecal, &eecal_eemem, sizeof(eecal)); 
    res = calcCRC16(&eecal, (sizeof(eecal) - sizeof(uint16_t)));
    
    
    // Check state of calibration portion of EEPROM
    
	if((0x55AA != eecal.sig) || (res != eecal.cal_crc)){ // BAD signature or bad CRC in EEPROM
		printf_P(PSTR("{\"eepromdefaulted\":\"1\"}\n"));
		// Read the defaults from the chip
		eecal.sig = 0x55AA;
		em_read_block(EM_PLCONSTH, EM_MMODE, eecal.meter_cal);
		em_read_block(EM_UGAIN, EM_QOFFSETN, eecal.measure_cal);
		
	    // Override the power line constant
	    eecal.meter_cal[PLCONSTL] = (uint16_t) PLC;
	    eecal.meter_cal[PLCONSTH] = (uint16_t) (PLC >> 16);
	    // Override the mode word
	    eecal.meter_cal[MMODE] = MODE_WORD;
			
	    // Write data back out to EEPROM

		eecal.cal_crc = calcCRC16(&eecal, (sizeof(eecal) - sizeof(uint16_t)));
		printf("{\"eepromcrc\":\"%04X\"}\n", eecal.cal_crc);
		eeprom_update_block(&eecal, &eecal_eemem, sizeof(eecal));

	}	
	
	// Initialize main menu and buttons
	menu_init(&main_menu, main_menu_strings);

	menu_add_button(&main_menu, &bt_left, bl_next, 1, 56, 8);
	menu_add_button(&main_menu, &bt_middle, bl_select, 2, 56, 50);
	menu_add_button(&main_menu, &bt_right, bl_exit, 3, 56, 100);
	
 
    //Enter meter calibration
	em_write_transaction(EM_CALSTART, 0x5678);
	// Override the power line constant
	eecal.meter_cal[PLCONSTL] = (uint16_t) PLC;
	eecal.meter_cal[PLCONSTH] = (uint16_t) (PLC >> 16);
	// Override the mode word
	eecal.meter_cal[MMODE] = MODE_WORD;
    // Write out the meter cal values
    cs = em_write_block(EM_PLCONSTH, EM_MMODE, eecal.meter_cal);
    // Write CS1 checksum
    em_write_transaction(EM_CS1, cs);
    // Exit meter calibration
    em_write_transaction(EM_CALSTART, 0x8765); 
    timer0_delay_ms(100);
    // Send meter status
    printf_P(PSTR("{\"calinit\":\"%04X\"}\n"), em_read_transaction(EM_SYSSTATUS));
	
	// Enter measurement calibration
	em_write_transaction(EM_ADJSTART, 0x5678);
	// Write out the measurement calibration values
	cs = em_write_block(EM_UGAIN, EM_QOFFSETN, eecal.measure_cal);
	// Write the CS2 checksum
	em_write_transaction(EM_CS2, cs);
	// Exit measurement calibration
	em_write_transaction(EM_ADJSTART, 0x8765);
	timer0_delay_ms(100);

	
	// Send meter status
	printf_P(PSTR("{\"measinit\":\"%04X\"}\n"), em_read_transaction(EM_SYSSTATUS));
	



	for(;;){ 		
		/*
		* Main event loop
		*/
		
		// Handle splash screen  
		if((dispmode == DISPMODE_SPLASH) && timer0_test_future_ms(&timer)){
			// Go to default display mode
			dispmode = DISPMODE_KVA;
			clear_screen();
		}
	 
		check_buttons();
		serial_service();
		gather_data();
		
		// If display mode is main menu, only do
		// screen refresh if absolutely required
	
		if(dispmode == DISPMODE_MAIN_MENU){
			if(menu_update_required(&main_menu) == FALSE){
				// Go back to the top of the event loop	
				continue;
			}
			clear_screen(); // Clear screen for rewrite	
		}
		
		update_display();
	}
}

