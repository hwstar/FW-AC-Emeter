//
//		button.c
//
//		Copyright 2015 Stephen Rodgers
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; either version 2 of the License, or
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

#define BQ_LENGTH 8 // Must be power of 2 and less than 256
typedef struct {
	uint8_t head;
	uint8_t tail;
	uint8_t events[8];
	uint8_t buttons[8];
} button_queue_t;
	
	

enum {BST_WAIT_PRESS = 0, BST_WAIT_RELEASE};

static button_data_t *button_list = NULL;
static button_queue_t button_queue;


/*
 * Button handler
 * 
 * This is called in interrupt context
 */

static void button_queue_event(uint8_t id, uint8_t event)
{
	uint8_t next = ((button_queue.head + 1) & (BQ_LENGTH - 1));
	
	
	
	
	if(next == button_queue.tail){
		// Queue is full, toss the event.
		//uart0_putc('F');
		return;
	}
	//uart0_putc('.');
	// Save the event
	button_queue.events[next] = event;
	button_queue.buttons[next] = id;
	// Advance head
	button_queue.head = next;
		
}

/*
 * Check to see if there is an event in the button queue.
 *
 * This is usually called by the main loop in the foreground.
 */
 

bool button_get_event(uint8_t *id, uint8_t *event)
{
	uint8_t tail;

	if(!id || !event)
		return FALSE;

	
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE){
		tail = button_queue.tail;
	
		if(button_queue.head == tail){
			// Empty ring buffer, return false
			return FALSE;
		}
		*id = button_queue.buttons[tail];
		*event = button_queue.events[tail];
		// Advance tail
		tail = ((tail + 1) & (BQ_LENGTH - 1));
		button_queue.tail = tail;	
	}
	return TRUE;
	
}
		
/*
 * Add a button to the list
 */

void button_add(button_data_t *button, volatile uint8_t *port, uint8_t pin, uint8_t id)
{
	button_data_t *b;

	// Initialize the data structure
	
	button->port = port;
	button->mask = _BV(pin);
	button->pin = pin;
	button->id = id;
	button->state = BST_WAIT_PRESS;
	button->last_button_state = button->mask;
	button->next = NULL;
	
	// Insert it into the list
	 
	if(!button_list)
		button_list = button;
	else{
		b = button_list;
		while(b->next != NULL)
			b = b->next;
		b->next = button;
	}
}

/*
 * Service buttons (Must be called appx. every 10mSec)
 */
 
void button_service(void)
{
	button_data_t *b = button_list;
	
	while(b){
		uint8_t cur = *b->port & b->mask;
		switch(b->state){
			case BST_WAIT_PRESS: // Wait for button to be pressed
				if(!cur && b->last_button_state){
					button_queue_event(b->id, BUTTON_EVENT_PRESSED);
					b->last_button_state = cur;
					b->state = BST_WAIT_RELEASE;
				}
				break;
				
			case BST_WAIT_RELEASE: // Wait for button to be released
				if(cur && !b->last_button_state){
					button_queue_event(b->id, BUTTON_EVENT_RELEASED);
					b->last_button_state = cur;
					b->state = BST_WAIT_PRESS;
				}
				break;
				
		}
		b = b->next; // Next list item
	}
}
	

