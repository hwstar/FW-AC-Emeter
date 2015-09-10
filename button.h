//
//		button.h
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

// Button data structure

typedef struct button_id_tag {
	uint8_t state;
	uint8_t id;
	uint8_t last_button_state;
	volatile uint8_t *port;
	uint8_t pin;
	uint8_t mask;
	struct button_id_tag *next;
} button_data_t;



// Button events

#define BUTTON_EVENT_NONE 0
#define BUTTON_EVENT_PRESSED 1
#define BUTTON_EVENT_RELEASED 2

// Methods

void button_add(button_data_t *button, volatile uint8_t *port, uint8_t pin, uint8_t id);
void button_service(void);
bool button_get_event(uint8_t *id, uint8_t *event);
