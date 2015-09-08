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

enum {BST_WAIT_PRESS = 0, BST_WAIT_RELEASE};

static button_data_t *button_list = NULL;

/*
 * Add a button to the list
 */

void button_add(button_data_t *button, uint8_t *port, uint8_t pin, uint8_t id, button_event_t event_callback)
{
	button_data_t *b;

	// Initialize the data structure
	
	button->port = port;
	button->mask = _BV(pin);
	button->pin = pin;
	button->id = id;
	button->state = BST_WAIT_PRESS;
	button->last_button_state = button->mask;
	button->event = event_callback;
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
					if(b->event)
						(*b->event)(b->id, BUTTON_EVENT_PRESSED);
					b->last_button_state = cur;
					b->state = BST_WAIT_RELEASE;
				}
				break;
				
			case BST_WAIT_RELEASE: // Wait for button to be released
				if(cur && !b->last_button_state){
					if(b->event)
						(*b->event)(b->id, BUTTON_EVENT_RELEASED);
					b->last_button_state = cur;
					b->state = BST_WAIT_PRESS;
				}
				break;
				
		}
		b = b->next; // Next list item
	}
}
