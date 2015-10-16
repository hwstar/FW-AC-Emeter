//
//		menu.c
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



/*
 * Init menu
 */
 
void menu_init(menu_t *menu, const char * const *menu_strings)
{
	if(!menu)
		return;
	// Initialize data structure
	menu->buttons = NULL;
	menu->strings = menu_strings;
}

/*
 * Add a button to a menu
 */

void menu_add_button(menu_t *menu, menu_buttons_t *button, const char *label, uint8_t button_id, uint8_t row, uint8_t col)
{
	menu_buttons_t *mb;
	
	if(!menu || !button)
		return;	
		
	// Find the end of the button list
	// and insert the new item at the end
	if(!menu->buttons)
		menu->buttons = button;
	else{
		for(mb = menu->buttons; mb->next; mb = mb->next)
			;
		mb->next = button;
	}
	// Initialize menu button entry.
	button->label = label;
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
	menu_buttons_t *b;
	u8g_uint_t w, d;
	
	if(!menu)
		return;
	
	// Calculate text size
	u8g_SetFont(u8g, u8g_font_5x7);
	u8g_SetFontRefHeightText(u8g);
	u8g_SetFontPosTop(u8g);
	h = u8g_GetFontAscent(u8g)-u8g_GetFontDescent(u8g);
	w = u8g_GetWidth(u8g);
	for( i = 0;; i++ ) {        // draw all menu items
		// Copy string from program memory to a work string in RAM
		strncpy_P(ramstr, (PGM_P)pgm_read_word(&(menu->strings[i])), sizeof(ramstr));
		ramstr[31] = 0;
		// Zero length string marks end of string list. 
		if(!ramstr[0])
			break;
		// Get its length in pixels
		d = (w-u8g_GetStrWidth(u8g, ramstr))/2;
		// Set foreground color
		u8g_SetDefaultForegroundColor(u8g);
		// If item selected
		if ( i == menu->selected ) {            // current selected menu item
			u8g_DrawBox(u8g, 0, i*h+1, w, h);   // draw cursor bar
			u8g_SetDefaultBackgroundColor(u8g);
		}
		// Display the string
		u8g_DrawStr(u8g, d, i*h, ramstr);
	}
	
	// Remember menu item count
	menu->item_count = i;
	
	// Ensure text and background are set back to normal
	u8g_SetDefaultForegroundColor(u8g);
	
	// If there are soft buttons, draw them here
	if(menu->buttons){	
		for(b = menu->buttons; b; b = b->next){
			strncpy_P(ramstr, b->label, sizeof(ramstr));
			ramstr[31] = 0;
			u8g_DrawStr(u8g, b->col, b->row, ramstr);
		}
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

uint8_t menu_update_required(menu_t *menu)
{
	uint8_t dirty = menu->dirty;
	menu->dirty = FALSE;
	return dirty;
}

/*
 * Advance to the next menu entry
 */	

void menu_next(menu_t *menu)
{
	printf("selected = %d\n", menu->selected);
	menu->selected++;
	if(menu->selected >= menu->item_count)
		menu->selected = 0;
	printf("new selected = %d\n", menu->selected);
	menu->dirty = TRUE;
}

/*
 * Return menu item selected
 */

uint8_t menu_selected(menu_t *menu)
{
	return menu->selected;
}



