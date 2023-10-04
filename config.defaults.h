/*
   Copyright 2023 Jamie Dennis

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

/*
 * The main configuration.
 *
 * All color values are stored as {r, g, b, a} normalized (from 0 to 1).
 * For example, flat red would be {1, 0, 0, 1}.
 */
static const struct {
	int gap_size;
	int bar_height;
	int cursor_movement_prevents_idle;
	float bar_background_color[4];
	float bar_selection_color[4]; /*Color of highlight for the currently selected view*/
	int status_update_interval_seconds;
	int status_separator_thickness; /*Thickness of the bar between status blocks*/
	int status_padding; /*Padding between status blocks*/
	float status_separator_color[4]; /*Color of the bar between status blocks*/
	int border_pixels;
	float active_border_color[4];
	float inactive_border_color[4];
} CONFIG = {
	.gap_size = 4,
	.bar_height = 20,
	.cursor_movement_prevents_idle = 0,
	.bar_background_color = {0.1, 0.1, 0.1, 1},
	.bar_selection_color = {0.1, 0.1, 0.9, 1},
	.status_update_interval_seconds = 1,
	.status_separator_thickness = 1,
	.status_padding = 4,
	.status_separator_color = {0.3, 0.3, 0.3, 1},
	.border_pixels = 1,
	.active_border_color = {0.1, 0.1, 0.9, 1},
	.inactive_border_color = {0.1, 0.1, 0.3, 1},
};

/**
 * Capra will try to pick modes with a matching resolution and the closest refresh rate.
 */
static const struct {
	char *name;
	int width;
	int height;
	int refresh_rate;
} OUTPUTS[] = {
	{"DP-1", 2560, 1440, 144},
	{"DP-2", 2560, 1440, 60},
};

static const struct {
	char *name;
	int accel_profile;
	float accel_speed;
} POINTERS[] = {
	/**
	 * Default configuration. Don't remove this.
	 */
	{NULL, .accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT, .accel_speed = -0.2},
};

// These macros are for setting up arguments to input functions.
#define ARG_COMMAND(command) {.argv = (char *[]){"/bin/sh", "-c", command, NULL}}
#define ARG_LAYERS(...) {.numbers = (long[]){__VA_ARGS__, INT64_MAX}}
#define ARG_LAYOUT(layout_name) {.layout = layout_name}
#define ARG_NUMBER(value) {.number = value}
#define NO_ARG {0}

#define MOD_KEY WLR_MODIFIER_LOGO

/**
 * Look at functions.h for a list of input functions and layouts.
 */ 
static const struct Key_Bind {
	uint32_t modifiers;
	xkb_keysym_t key;
	void (*function)(Input_Arg arg);
	Input_Arg arg;
} KEY_BINDS[] = {
	// modifiers, key, function, arg}
	
	{MOD_KEY|WLR_MODIFIER_SHIFT, XKB_KEY_Escape, close_server, NO_ARG},
	{MOD_KEY, XKB_KEY_Return, spawn, ARG_COMMAND("foot")},
	{MOD_KEY, XKB_KEY_space, spawn, ARG_COMMAND("bemenu-run")},
	{MOD_KEY|WLR_MODIFIER_SHIFT, XKB_KEY_Q, close_client, NO_ARG},
	{MOD_KEY, XKB_KEY_r, set_layout, ARG_LAYOUT(layout_recursive)},
	{MOD_KEY, XKB_KEY_f, cycle_client_layer, ARG_LAYERS(LAYER_VIEW_FLOATING, LAYER_VIEW_TILES)},
	{MOD_KEY|WLR_MODIFIER_SHIFT, XKB_KEY_F, toggle_fullscreen, NO_ARG},
	{MOD_KEY, XKB_KEY_s, toggle_layer, ARG_NUMBER(LAYER_OUTPUT_STICKY)},
	{MOD_KEY|WLR_MODIFIER_SHIFT, XKB_KEY_S, cycle_client_layer, ARG_LAYERS(LAYER_OUTPUT_STICKY, LAYER_VIEW_FLOATING)},
	{MOD_KEY, XKB_KEY_comma, increment_view, ARG_NUMBER(-1)},
	{MOD_KEY, XKB_KEY_period, increment_view, ARG_NUMBER(+1)},
	{MOD_KEY, XKB_KEY_1, select_view, ARG_NUMBER(0)},
	{MOD_KEY, XKB_KEY_2, select_view, ARG_NUMBER(1)},
	{MOD_KEY, XKB_KEY_3, select_view, ARG_NUMBER(2)},
	{MOD_KEY, XKB_KEY_4, select_view, ARG_NUMBER(3)},
	{MOD_KEY, XKB_KEY_5, select_view, ARG_NUMBER(4)},
	{MOD_KEY, XKB_KEY_6, select_view, ARG_NUMBER(5)},
	{MOD_KEY, XKB_KEY_7, select_view, ARG_NUMBER(6)},
	{MOD_KEY, XKB_KEY_8, select_view, ARG_NUMBER(7)},
	{MOD_KEY, XKB_KEY_9, select_view, ARG_NUMBER(8)},
	{MOD_KEY|WLR_MODIFIER_SHIFT, XKB_KEY_exclam, move_to_view, ARG_NUMBER(0)},
	{MOD_KEY|WLR_MODIFIER_SHIFT, XKB_KEY_at, move_to_view, ARG_NUMBER(1)},
	{MOD_KEY|WLR_MODIFIER_SHIFT, XKB_KEY_numbersign, move_to_view, ARG_NUMBER(2)},
	{MOD_KEY|WLR_MODIFIER_SHIFT, XKB_KEY_dollar, move_to_view, ARG_NUMBER(3)},
	{MOD_KEY|WLR_MODIFIER_SHIFT, XKB_KEY_percent, move_to_view, ARG_NUMBER(4)},
	{MOD_KEY|WLR_MODIFIER_SHIFT, XKB_KEY_uparrow, move_to_view, ARG_NUMBER(5)},
	{MOD_KEY|WLR_MODIFIER_SHIFT, XKB_KEY_ampersand, move_to_view, ARG_NUMBER(6)},
	{MOD_KEY|WLR_MODIFIER_SHIFT, XKB_KEY_asterisk, move_to_view, ARG_NUMBER(7)},
	{MOD_KEY|WLR_MODIFIER_SHIFT, XKB_KEY_parenleft, move_to_view, ARG_NUMBER(8)},
};

/**
 * Mouse button binds. Work the same as key binds.
 */
static const struct Button_Binds {
	uint32_t modifiers;
	uint32_t button;
	void (*function)(Input_Arg arg);
	Input_Arg arg;
} BUTTON_BINDS[] = {
	{MOD_KEY, BTN_LEFT, move_client, NO_ARG},
	{MOD_KEY, BTN_RIGHT, move_resize_client, NO_ARG},
	{MOD_KEY, BTN_MIDDLE, set_client_layer, ARG_NUMBER(LAYER_VIEW_TILES)}
};

#define LAYOUT_DEFAULT layout_recursive


/**
 * Status blocks. See functions.h for a list of block functions
 */ 
static Status_Function *STATUS_BLOCKS[] = {
	status_date_and_time,
	status_memory_usage,
};

/**
 * First font takes priority. Fonts loaded after will only load characters that
 * aren't covered by the previous font.
 */
static const struct {
	char *path;
	int px_size;
} FONTS[] = {
	{"/usr/share/fonts/TTF/DejaVuSansMono.ttf", 12},
};

