/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 */
 
#include "includes.h"
 
#ifdef __AVR__
 
	#define SCLK_HIGH EM_SCLK_PORT |= _BV(EM_SCLK_PIN)
	#define SCLK_LOW EM_SCLK_PORT &= ~_BV(EM_SCLK_PIN)
 
	#define MOSI_HIGH EM_MOSI_PORT |= _BV(EM_MOSI_PIN)
	#define MOSI_LOW EM_MOSI_PORT &= ~_BV(EM_MOSI_PIN)
	#define MOSI_SET(x) if((x)) MOSI_HIGH; else MOSI_LOW
 
	#define MISO_STATE (((EM_MISO_PINPORT & _BV(EM_MISO_PIN)) > 0))
 
	#define CLK_DELAY _delay_us(5)
	#define START_DELAY _delay_us(500)

#endif
 
 

/*
 * Do a full duplex SPI transaction
 *
 * Start with clock low
 * 
 * Pass in the byte to be sent.
 * 
 * Returns the received byte
 * 
 * Ends with clock high
 *
 */
 
 static uint16_t em_transact_byte(uint8_t out_byte)
 {
	 uint8_t in_byte = 0;
	 uint8_t i;
	 
	 for(i = 0; i < 8; i++){
		 // Set MOSI
		 SCLK_LOW;
		 // Output data bit to MOSI
		 MOSI_SET(out_byte & 0x80);
		 out_byte <<= 1;
		 // Wait MISO setup time
		 CLK_DELAY;
		 // Sample input state
		 
		 in_byte <<= 1;
		 if(MISO_STATE){
			in_byte |= 0x01;
		 }

		 // Clock high
		 SCLK_HIGH;
		 // Wait clock high time
		 CLK_DELAY;
	 }
	 return (uint16_t) in_byte;

 }
 
/*
 * Initialize I/O pins
 */
 
 
 void em_init(void)
 {
	 
	 // Set the SCLK and MOSI bits high
	 
	 SCLK_HIGH;
	 MOSI_HIGH;
	 
	 // Set I/O direction on software SPI pins
	 
	 EM_SCLK_DDR |= _BV(EM_SCLK_PIN);
	 EM_MOSI_DDR |= _BV(EM_MOSI_PIN);
	 EM_MISO_DDR &= ~_BV(EM_MISO_PIN);
	 
	 // Wait for em chip SPI engine to time out and clear any spurious transaction
	 
	 _delay_ms(7.0); 
	 
 }
 
 /*
  * Do a write transaction
  */
 
 void em_write_transaction(uint8_t addr, uint16_t data)
 {
	 // Tell the chip we want to start a transaction
	 SCLK_LOW;
	 START_DELAY;
	 // Clock out the address, and the 16 bit data to the chip
	 em_transact_byte(addr);
	 em_transact_byte((uint8_t) (data >> 8));
	 em_transact_byte((uint8_t) data);
 }
 
 /*
  * Do a read transaction
  */
 
 uint16_t em_read_transaction(uint8_t addr)
 {
	 uint16_t res;
 
	 // Tell the chip we want to start a transaction
	 SCLK_LOW;
	 START_DELAY;
	 // Clock out the address to the chip
	 em_transact_byte(addr | 0x80);
	 //Clock in the 16 bit data from the chip
	 res = (em_transact_byte(0) << 8);
	 res |= em_transact_byte(0);
	 // Return the result
	 return res;
 }
 
/*
 * Write a block of values, and calculate the checksum on the fly.
 * Return the checksum to the caller.
 */
  
uint16_t em_write_block(uint8_t first, uint8_t last, uint16_t *block)
{
	uint8_t cshigh = 0, cslow = 0;
	uint8_t i;
	
	
	for(i = first; i < last + 1; i++){
		uint8_t j = i - first;
		em_write_transaction(i, block[j]);
		// Calculate checksum on the fly
		// Low byte is modulo 256 sum of all high and low bytes
		cslow += (uint8_t) ((block[j] & 0xff) + (block[j] >> 8));
		// High byte is the XOR of all the high and low bytes.
		cshigh ^= ((uint8_t) (block[j]));
		cshigh ^= ((uint8_t) (block[j] >> 8));
	}
	// Return the checksum
	return (((uint16_t) cshigh) << 8) + cslow;
		
}

 
/*
 * Read a block of values, and calculate the checksum on the fly.
 * Return the checksum to the caller.
 */

uint16_t em_read_block(uint8_t first, uint8_t last, uint16_t *block)
{
	uint8_t cshigh = 0, cslow = 0;
	uint8_t i;
	for(i = first; i < last + 1; i++){
		uint8_t j = i - first;
		block[j] = em_read_transaction(i);
		// Calculate checksum on the fly
		// Low byte is modulo 256 sum of all high and low bytes
		cslow += (uint8_t) ((block[j] & 0xff) + (block[j] >> 8));
		// High byte is the XOR of all the high and low bytes.
		cshigh ^= ((uint8_t) (block[j]));
		cshigh ^= ((uint8_t) (block[j] >> 8));
	}
	// Return the checksum
	return (((uint16_t) cshigh) << 8) + cslow;
	
}

		
	
	

