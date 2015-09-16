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
	uint8_t item_count;							// Number of items in the menu
	uint8_t selected;							// Selected entry
	uint8_t dirty;								// Menu needs to be redrawn
} menu_t;

void menu_init(menu_t *menu, const char * const *menu_strings);
void menu_add_button(menu_t *menu, menu_buttons_t *button, const char *label, uint8_t button_id, uint8_t row, uint8_t col);
void menu_draw(menu_t *menu, u8g_t *u8g);
void menu_show(menu_t *menu, uint8_t selected);
uint8_t menu_update_required(menu_t *menu);
void menu_handle_button_event(menu_t *menu, uint8_t id, uint8_t event);
void menu_next(menu_t *menu);
uint8_t menu_selected(menu_t *menu);


#endif
