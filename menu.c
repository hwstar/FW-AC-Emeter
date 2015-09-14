//
//		menu.c
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



/*
 * Init menu
 */
 
void menu_init(menu_t *menu, const char * const *menu_strings, menu_action_t action)
{
	if(!menu)
		return;
	// Initialize data structure
	menu->buttons = NULL;
	menu->strings = menu_strings;
	menu->action = action;
	for(menu->item_count = 0; menu->strings[menu->item_count]; menu->item_count++)
		;
}

/*
 * Add a button to a menu
 */

void menu_add_button(menu_t *menu, menu_buttons_t *button, const char *label, uint8_t button_id, uint8_t row, uint8_t col)
{
	menu_buttons_t *mb;;
	
	if(!menu || !button)
		return;	
	
	// Find the end of the button list
	if(!menu->buttons)
		menu->buttons = button;
	else{
		for(mb = menu->buttons; mb->next; mb = mb->next)
			;
		mb->next = button;
	}
	button->id = button_id;
	button->row = row;
	button->col = col;

}


/*
 * Draw a menu
 */
 
void menu_draw(menu_t *menu, u8g_t *u8g)
{
	uint8_t i, h;
	static char ramstr[32];
	u8g_uint_t w, d;
	
	if(!menu)
		return;

	// Calculate text size
	u8g_SetFont(u8g, u8g_font_5x7);
	u8g_SetFontRefHeightText(u8g);
	u8g_SetFontPosTop(u8g);
	h = u8g_GetFontAscent(u8g)-u8g_GetFontDescent(u8g);
	w = u8g_GetWidth(u8g);
	for( i = 0; menu->strings[i]; i++ ) {        // draw all menu items
		// Copy string from program memory to work string
		strncpy_P(ramstr, (PGM_P)pgm_read_word(&(menu->strings[i])), sizeof(ramstr));
		ramstr[31] = 0;
		// Get its length in pixels
		d = (w-u8g_GetStrWidth(u8g, menu->strings[i]))/2;
		// Set foreground color
		u8g_SetDefaultForegroundColor(u8g);
		// If item selected
		if ( i == menu->selected ) {               // current selected menu item
			u8g_DrawBox(u8g, 0, i*h+1, w, h);   // draw cursor bar
			u8g_SetDefaultBackgroundColor(u8g);
		}
		// Display the string
		u8g_DrawStr(u8g, d, i*h, menu->strings[i]);
	}

}

/*
 * Show menu
 */
 
void menu_show(menu_t *menu, uint8_t selected)
{
	if(!menu)
		return;
	menu->selected = selected;
	menu->dirty = TRUE;	
}

/*
 * Return true if the menu requires an update
 */

uint8_t menu_update(menu_t *menu)
{
	uint8_t dirty = menu->dirty;
	menu->dirty = FALSE;
	return dirty;
}



