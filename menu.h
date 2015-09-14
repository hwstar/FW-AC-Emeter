#ifndef MENU_H
#define MENU_H




typedef struct menu_buttons_s {
	const char *label;							// Button label
	uint8_t row;								// label row
	uint8_t col;								// label column
	uint8_t id;									// button id;
	struct menu_buttons_s *next;				// Next button element
} menu_buttons_t;	

typedef void (* menu_action_t)(uint8_t item_id);

typedef struct {
	const char * const *strings;				// Array of string pointers for menu strings
	menu_buttons_t *buttons;					// Head of button list
	menu_action_t action;						// Action function
	uint8_t item_count;							// Number of items in the menu
	uint8_t selected;							// Selected entry
	uint8_t dirty;								// Menu needs to be redrawn
} menu_t;

void menu_init(menu_t *menu, const char * const *menu_strings, menu_action_t action);
void menu_add_button(menu_t *menu, menu_buttons_t *button, const char *label, uint8_t button_id, uint8_t row, uint8_t col);
void menu_draw(menu_t *menu, u8g_t *u8g);
void menu_show(menu_t *menu, uint8_t selected);
uint8_t menu_update(menu_t *menu);


#endif
