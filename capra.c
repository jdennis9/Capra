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
#include <libinput.h>
#include <linux/input-event-codes.h>
#include <wlr/backend/libinput.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_damage_ring.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_drm.h>
//#include <wlr/types/wlr_drm_lease_v1.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
//#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
//#include <wlr/types/wlr_fullscreen_shell_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_idle.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_input_method_v2.h>
#include <wlr/types/wlr_keyboard.h>
//#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_layer_shell_v1.h>
//#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output.h>
//#include <wlr/types/wlr_output_damage.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output_management_v1.h>
//#include <wlr/types/wlr_output_power_management_v1.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
//#include <wlr/types/wlr_pointer_gestures_v1.h>
//#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_primary_selection_v1.h>
//#include <wlr/types/wlr_region.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_server_decoration.h>
//#include <wlr/types/wlr_session_lock_v1.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/types/wlr_subcompositor.h>
//#include <wlr/types/wlr_touch.h>
#include <wlr/types/wlr_viewporter.h>
//#include <wlr/types/wlr_virtual_keyboard_v1.h>
//#include <wlr/types/wlr_virtual_pointer_v1.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_activation_v1.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/vulkan.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

#if USE_XWAYLAND
#include <wlr/xwayland/xwayland.h>
#include <X11/Xdefs.h>
#endif

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define ARRAY_LENGTH(array) (sizeof(array) / sizeof(array[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static FILE *log_file;

static inline void debug_printf(const char *format, ...) {
	va_list va;
	va_start(va, format);
	vfprintf(log_file, format, va);
	va_end(va);
	fflush(log_file);
}

/* ================================================================================
 * Enums
 * ================================================================================*/
enum Client_Type {
	CLIENT_TYPE_XDG_TOPLEVEL,
	CLIENT_TYPE_LAYER,
#if USE_XWAYLAND
	CLIENT_TYPE_XWAYLAND,
#endif
};

enum View_Layer {
	VIEW_LAYER_TILES,
	VIEW_LAYER_FLOATING,
	VIEW_LAYER_FULLSCREEN,
	NUM_VIEW_LAYERS,
};

enum Output_Layer {
	OUTPUT_LAYER_BACKGROUND,
	OUTPUT_LAYER_BOTTOM,
	OUTPUT_LAYER_STICKY,
	OUTPUT_LAYER_POPUPS,
	OUTPUT_LAYER_TOP,
	OUTPUT_LAYER_OVERLAY,
	NUM_OUTPUT_LAYERS,
};

enum Layer {
	LAYER_OUTPUT_BACKGROUND,
	LAYER_OUTPUT_BOTTOM,
	LAYER_VIEW_TILES,
	LAYER_VIEW_FLOATING,
	LAYER_VIEW_FULLSCREEN,
	LAYER_OUTPUT_STICKY,
	LAYER_OUTPUT_POPUPS,
	LAYER_OUTPUT_TOP,
	LAYER_OUTPUT_OVERLAY,
	NUM_LAYERS,
};

enum Focus_Mode {
	FOCUS_MODE_ON_HOVER,
	FOCUS_MODE_ON_CLICK,
};

enum Cursor_Mode {
	CURSOR_MODE_NORMAL,
	CURSOR_MODE_MOVE,
	CURSOR_MODE_RESIZE
};

#define layer_is_view_layer(layer) (((layer) == LAYER_VIEW_TILES) || ((layer) == LAYER_VIEW_FLOATING))
#define layer_is_output_layer(layer) (((layer) >= LAYER_OUTPUT_STICKY) && ((layer) <= LAYER_OUTPUT_BOTTOM))
#define client_is_fullscreen(client) ((client)->config.flags & CLIENT_CONFIG_FULLSCREEN)
#define can_interact_with_client(client) (((client)->type != CLIENT_TYPE_LAYER) && !client_is_fullscreen(client))
#define OUTPUT_CURRENT_VIEW(output) (&((output)->views[(output)->current_view]))

/* ================================================================================
 * Structs
 * ================================================================================*/
typedef struct Client Client;
typedef struct Keyboard Keyboard;
typedef struct Layout Layout;
typedef struct Output Output;
typedef struct View View;
typedef struct Pointer_Constraint Pointer_Contraint;

enum {
	CLIENT_CONFIG_FULLSCREEN = 0x1,
	CLIENT_CONFIG_MINIMIZED = 0x2,
};

typedef struct Client {
	struct wl_list link;
	struct wl_list update_link;
	enum Client_Type type;
	uint16_t layer;
	uint16_t old_layer;
	Output *output;
	View *view;
	struct {
		uint32_t requesting_fullscreen : 1;
		uint32_t mapped : 1;
	};
	struct {
		int32_t x, y;
		int32_t width, height;
		uint32_t flags;
	} config;
	
	struct wl_listener on_destroy;
	struct wl_listener on_map;
	struct wl_listener on_request_configure;
	struct wl_listener on_request_fullscreen;
	struct wl_listener on_request_minimize;
	struct wl_listener on_unmap;
	union {
#if USE_XWAYLAND
		struct wlr_xwayland_surface *xwayland_surface;
#endif
		struct wlr_xdg_surface *xdg_surface;
		struct wlr_layer_surface_v1 *layer_surface;
	};
} Client;

typedef struct Keyboard {
	struct wlr_keyboard *wlr;
	struct wl_listener on_key;
	struct wl_listener on_modifiers;
} Keyboard;

typedef struct View {
	struct wl_list view_layers[NUM_VIEW_LAYERS];
	struct wl_list *layers[NUM_LAYERS];
	void (*layout)(Output*);
	uint32_t layer_show_mask;
} View;

typedef struct Output {
	struct wl_list link;
	struct wl_list output_layers[NUM_OUTPUT_LAYERS];
	View views[9];
	uint32_t current_view;
	uint32_t bar_height;
	
	struct wlr_output *wlr;
	struct wl_listener on_frame;
	struct wl_listener on_destroy;
} Output;

typedef struct Pointer_Constraint {
	struct wlr_pointer_constraint_v1 *constraint;
	struct wl_listener on_set_region;
	struct wl_listener on_destroy;
} Pointer_Constraint;

#include "functions.h"
#include "config.h"

/*static const enum Output_Layer MAP_LAYER_TO_OUTPUT_LAYER[] = {
	[LAYER_OUTPUT_BOTTOM] = OUTPUT_LAYER_BOTTOM,
	[LAYER_OUTPUT_STICKY] = OUTPUT_LAYER_STICKY,
	[LAYER_OUTPUT_TOP] = OUTPUT_LAYER_TOP,
	[LAYER_OUTPUT_OVERLAY] = OUTPUT_LAYER_OVERLAY,
};

static const enum View_Layer MAP_LAYER_TO_VIEW_LAYER[] = {
	[LAYER_VIEW_TILES] = VIEW_LAYER_TILES,
	[LAYER_VIEW_FLOATING] = VIEW_LAYER_FLOATING,
};*/


static inline struct wl_list *get_layer_list(enum Layer layer, Output *output, View *view) {
	return view->layers[layer];
}

static inline void listen(struct wl_listener *listener, wl_notify_func_t function, struct wl_signal *event) {
	listener->notify = function;
	wl_signal_add(event, listener);
}

static inline struct wlr_surface *get_client_wlr_surface(const Client *client) {
	switch (client->type) {
		case CLIENT_TYPE_XDG_TOPLEVEL: return client->xdg_surface->surface;
		case CLIENT_TYPE_LAYER: return client->layer_surface->surface;
#if USE_XWAYLAND
		case CLIENT_TYPE_XWAYLAND: return client->xwayland_surface->surface;
#endif
	}
	return NULL;
}

static inline void get_output_client_area(const Output *output, struct wlr_box *box) {
	int width, height;
	wlr_output_effective_resolution(output->wlr, &width, &height);
	box->x = 0;
	box->y = output->bar_height;
	box->width = width;
	box->height = height - output->bar_height;
}

static inline const char *get_client_title(const Client *client) {
	static const char *no_title = "[no title]";
	if (client->type == CLIENT_TYPE_XDG_TOPLEVEL) {
		const char *ret = client->xdg_surface->toplevel->title;
		return ret ? ret : no_title;
	}
#if USE_XWAYLAND
	else if (client->type == CLIENT_TYPE_XWAYLAND) {
		const char *ret = client->xwayland_surface->title;
		return ret ? ret : no_title;
	}
#endif
	return no_title;
}

// Helper for input functions that need to cycle through a range of values
static inline long next_value_in_cycle(long *cycle, long current_value) {
	long base = *cycle;
	
	for (; *cycle != INT64_MAX; ++cycle) {
		if (*cycle == current_value) {
			return cycle[1] == INT64_MAX ? base : cycle[1];
		}
	}
	
	return base;
}

static inline bool should_render_layer(View *view, enum Layer layer) {
	return !wl_list_empty(view->layers[layer]) && (view->layer_show_mask & (1<<layer));
}

static struct {
	struct wlr_output_layout *output_layout;
	Output *focused_output; // never NULL
	Client *focused_client;
	struct {
		uint32_t focus_grabbed : 1;
	};
	/*enum Focus_Mode*/ uint32_t focus_mode;
	/*enum Cursor_Mode*/ uint32_t cursor_mode;
	// When grabbing a client, this is the offset from the cursor to
	// the client's top left corner
	int32_t interact_grab_x;
	int32_t interact_grab_y;
	struct wl_list client_update_list;
	
	uint64_t time_of_last_status_update;
	
	struct wl_display *display;
	
	struct wlr_backend *backend;
	struct wl_listener on_new_input;
	struct wl_listener on_new_output;
	
	struct wlr_seat *seat;
	struct wl_listener on_request_set_cursor;
	struct wl_listener on_request_set_selection;

	struct wlr_xdg_shell *xdg_shell;
	struct wl_listener on_new_xdg_surface;
	
	struct wlr_cursor *cursor;
	struct wl_listener on_cursor_axis;
	struct wl_listener on_cursor_button;
	struct wl_listener on_cursor_frame;
	struct wl_listener on_cursor_motion;
	struct wl_listener on_cursor_motion_absolute;
	
	struct wlr_output_manager_v1 *output_manager;
	struct wl_listener on_output_manager_apply;
	struct wl_listener on_output_manager_test;
	
	struct wlr_xcursor_manager *xcursor_manager;
	
	struct wlr_xdg_decoration_manager_v1 *xdg_decoration_manager;
	struct wl_listener on_new_xdg_decoration;
	
	struct wlr_xdg_output_manager_v1 *xdg_output_manager;
	
	struct wlr_layer_shell_v1 *layer_shell;
	struct wl_listener on_new_layer_surface;
	
	struct wlr_pointer_constraints_v1 *pointer_constraints;
	struct wl_listener on_new_pointer_constraint;
	
	struct wlr_relative_pointer_manager_v1 *relative_pointer_manager;
	
	struct wlr_idle_notifier_v1 *idle_notifier;
	
	struct wlr_idle *idle;
	
	struct wlr_idle_inhibit_manager_v1 *idle_inhibit_manager;
	
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_compositor *compositor;
#if USE_XWAYLAND
	struct wlr_xwayland *xwayland;
	struct wl_listener on_xwayland_ready;
	struct wl_listener on_new_xwayland_surface;
#endif
	struct wl_list output_list;
} server;

#define NUM_STATUS_BLOCKS ARRAY_LENGTH(STATUS_BLOCKS)

static struct {
	char block_buffers[NUM_STATUS_BLOCKS][128];
	size_t block_lengths[NUM_STATUS_BLOCKS];
	pthread_t update_thread;
	// The status thread will only lock this when it wants
	// to write to all of the block buffers in one motion.
	pthread_mutex_t lock;
} status_bar;

// @Note: Keeping the netwm stuff here even though we don't use it right now
#if /*USE_XWAYLAND*/ 0
enum {
	NET_WM_TYPE_DIALOG,
	NET_WM_TYPE_MENU,
	NET_WM_TYPE_SPLASH,
	NET_WM_TYPE_TOOLBAR,
	NET_WM_TYPE_UTILITY,
	NUM_NET_WM_TYPES,
};

static Atom NET_WM_ATOMS[NUM_NET_WM_TYPES];
#endif

#include "font.c"
//#include "status.c"

static inline void prevent_idle() {
	wlr_idle_notifier_v1_notify_activity(server.idle_notifier, server.seat);
	wlr_idle_notify_activity(server.idle, server.seat);
}

// Server functions
static void arrange_output(Output *output);
static void configure_client(Client *client, int32_t x, int32_t y, int32_t width, int32_t height, uint32_t flags);
static void focus_client(Client *client);
static Client *get_client_under_cursor(Output *output);
static Client *get_client_under_cursor_in_list(struct wlr_box *output_box, struct wl_list *list);
static void handle_destroy_surface(struct wl_listener *listener, void *data);
static void handle_unmap_surface(struct wl_listener *listener, void *data);
static void make_client_fullscreen(Client *client);
static void move_client_to_layer(Client *client, enum Layer destination_layer);
static void process_cursor_move(uint32_t time_msec);
static void set_cursor_mode(enum Cursor_Mode mode);
static void send_client_close(Client *client);
static bool try_key_bind(uint32_t modifiers, xkb_keysym_t key);
static void update_output_configuration(); // Disables/enables clients based on visibility
static void update_focus();
static void update_visibility(); // Disables/enables clients based on visibility

// Client rendering
static void render_client(Client *client, const Output *output, const float *matrix, double x_offset, double y_offset, struct timespec *when);
static void render_client_list(struct wl_list *list, const Output *output, double x_offset, double y_offset, struct timespec *when);
static void render_surface(struct wlr_surface *surface, const float *matrix, double x, double y);

// Status bar
static void render_status_bar(Output *output);
static void *status_update_thread(void *dont_care);
/* ================================================================================
 * Helpers
 * ================================================================================*/
// This is for layout functions
static inline void set_client_box(Client *client, struct wlr_box *box, int gaps) {	
	configure_client(client, box->x + gaps, box->y + gaps, 
					 box->width - (gaps * 2), box->height - (gaps * 2), UINT32_MAX);
}

/* ================================================================================
 * Input function implementation
 * ================================================================================*/
static void close_client(Input_Arg arg) {
	if (server.focused_client) send_client_close(server.focused_client);
}

static void close_server(Input_Arg arg) {
	printf("Closing server...\n");
	wl_display_terminate(server.display);
}

static void cycle_client_layer(Input_Arg arg) {
	Client *client = server.focused_client;
	if (!client) return;
	long dest_layer = next_value_in_cycle(arg.numbers, client->layer);
	move_client_to_layer(client, dest_layer);
}

static void increment_view(Input_Arg arg) {
	uint32_t current = server.focused_output->current_view;
	if (current + arg.number >= 9) select_view((Input_Arg){.number = 0});
	else select_view((Input_Arg){.number = current + arg.number});
}

static void move_client(Input_Arg arg) {
	set_cursor_mode(CURSOR_MODE_MOVE);
}

static void move_resize_client(Input_Arg arg) {
	set_cursor_mode(CURSOR_MODE_RESIZE);
}

static void move_to_view(Input_Arg arg) {
	Client *client = server.focused_client;
	if (!client) return;
	View *dest_view = &client->output->views[arg.number];
	if (client->view == dest_view || !client->link.next) return;
	wl_list_remove(&client->link);
	wl_list_insert(dest_view->layers[client->layer], &client->link);
	client->view = dest_view;
}

static void select_view(Input_Arg arg) {
	Output *output = server.focused_output;
	Client *client = server.focused_client;
	
	output->current_view = arg.number;
	if (output->current_view >= 9) output->current_view = 8;
	if (client && layer_is_view_layer(client->layer)) focus_client(NULL);
	update_focus();
	update_visibility();
}

static void set_client_layer(Input_Arg arg) {
	Client *client = get_client_under_cursor(server.focused_output);
	if (!client) return;
	move_client_to_layer(client, arg.number);
}

static void set_layout(Input_Arg arg) {
	Output *output = server.focused_output;
	View *view = &output->views[output->current_view];
	view->layout = arg.layout;
	arrange_output(output);
}

static void spawn(Input_Arg arg) {
	if (fork() == 0) {
		execv(arg.argv[0], arg.argv);
	}
}

static void toggle_fullscreen(Input_Arg arg) {
	Client *client = server.focused_client;
	if (!client) return;
	Output *output = client->output;
	configure_client(client, 0, 0, output->wlr->width, output->wlr->height, 
					 client->config.flags ^ CLIENT_CONFIG_FULLSCREEN);
	if (client->config.flags & CLIENT_CONFIG_FULLSCREEN) {
		move_client_to_layer(client, LAYER_VIEW_FULLSCREEN);
	} else {
		move_client_to_layer(client, client->old_layer);
		arrange_output(client->output);
	}
}

static void toggle_layer(Input_Arg arg) {
	Output *output = server.focused_output;
	View *view = output->views;
	uint32_t layer_bit = 1u << (uint32_t)arg.number;
	for (int i = 0; i < 9; ++i, ++view) {
		view->layer_show_mask ^= layer_bit;
	}
}

/* ================================================================================
 * Layouts
 * ================================================================================*/
static void layout_recursive(Output *output) {
	struct wl_list *tiles = output->views[output->current_view].layers[LAYER_VIEW_TILES];
	struct wlr_box usable_area;
	uint32_t count = wl_list_length(tiles);
	if (!count) return;
	
	get_output_client_area(output, &usable_area);
	uint32_t i = 1;
	const uint32_t gaps = CONFIG.gap_size;
	Client *client;
	
	wl_list_for_each(client, tiles, link) {
		if (i++ != count) {
			bool horizontal_split = usable_area.width < usable_area.height;
            
            if (horizontal_split) {
				usable_area.height /= 2;
				set_client_box(client, &usable_area, gaps);
				usable_area.y += usable_area.height;
            } 
            else {
				usable_area.width /= 2;
				set_client_box(client, &usable_area, gaps);
				usable_area.x += usable_area.width;
            }
        } else {
        	set_client_box(client, &usable_area, gaps);
        }
	}
}

/* ================================================================================
 * Client rendering
 * ================================================================================*/
struct Surface_Iterator_Data {
	Client *parent;
	int draw_parent;
	double x_offset, y_offset;
	struct timespec *when;
	const float *matrix;
};

static void surface_render_iterator(struct wlr_surface *surface, int sx, int sy, void *user_data) {
	struct Surface_Iterator_Data *data = user_data;
	if (!data->draw_parent && (surface == get_client_wlr_surface(data->parent))) return;
	render_surface(surface, data->matrix, 
				   sx + data->parent->config.x + data->x_offset, 
				   sy + data->parent->config.y + data->y_offset);
	wlr_surface_send_frame_done(surface, data->when);
}

static void render_client(Client *client, const Output *output, const float *matrix, double x_offset, double y_offset, struct timespec *when) {
	struct wlr_surface *wlr_surface = get_client_wlr_surface(client);
	struct Surface_Iterator_Data iterator_data;
	
	iterator_data.parent = client;
	iterator_data.x_offset = x_offset;
	iterator_data.y_offset = y_offset;
	iterator_data.when = when;
	iterator_data.matrix = matrix;
	iterator_data.draw_parent = 1;
	
	if (client->type != CLIENT_TYPE_LAYER && client->layer != LAYER_OUTPUT_POPUPS && !client_is_fullscreen(client)) {
		struct wlr_box border = {
			.x = x_offset + client->config.x - CONFIG.border_pixels,
			.y = y_offset + client->config.y - CONFIG.border_pixels,
			.width = wlr_surface->current.width + CONFIG.border_pixels*2,
			.height = wlr_surface->current.height + CONFIG.border_pixels*2,
		};
		
		const float *border_color = client == server.focused_client ? 
			CONFIG.active_border_color : CONFIG.inactive_border_color;
		
		wlr_render_rect(server.renderer, &border, border_color, matrix);
	}
	
	//render_surface(wlr_surface, matrix, client->config.x + x_offset, client->config.y + y_offset);
	wlr_surface_for_each_surface(wlr_surface, &surface_render_iterator, &iterator_data);
	if (client->type == CLIENT_TYPE_XDG_TOPLEVEL) {
		wlr_xdg_surface_for_each_popup_surface(client->xdg_surface, &surface_render_iterator, &iterator_data);
	}
	
	wlr_surface_send_frame_done(wlr_surface, when);
	
	// @Hack to make sure that XWayland focus matches whats on the screen. The server tries to disable hidden clients
#if USE_XWAYLAND
	if (client->type == CLIENT_TYPE_XWAYLAND) {
		wlr_xwayland_surface_restack(client->xwayland_surface, NULL, XCB_STACK_MODE_ABOVE);
	}
#endif
}

static void render_client_list(struct wl_list *list, const Output *output, double x_offset, double y_offset, struct timespec *when) {
	Client *client;
	float matrix[9];
	memcpy(matrix, output->wlr->transform_matrix, sizeof(matrix));

	wl_list_for_each_reverse(client, list, link) {
		render_client(client, output, matrix, x_offset, y_offset, when);
	}
}

static void render_surface(struct wlr_surface *surface, const float *matrix, double x, double y) {
	struct wlr_texture *texture = wlr_surface_get_texture(surface);
	if (!texture) return;
	wlr_render_texture(server.renderer, texture, matrix, x, y, 1.f);
}

/* ================================================================================
 * Status bar
 * ================================================================================*/
static void *status_update_thread(void *dont_care) {
	char buffers[NUM_STATUS_BLOCKS][128];
	FILE *pipes[NUM_STATUS_BLOCKS];
	
	for (int i = 0; i < NUM_STATUS_BLOCKS; ++i) {
	}
	
	while (1) {
		for (int i = 0; i < NUM_STATUS_BLOCKS; ++i) {
			pipes[i] = popen(STATUS_BLOCKS[i], "r");
		}
		
		for (int i = 0; i < NUM_STATUS_BLOCKS; ++i) {
			if (pipes[i]) {
				fread(buffers[i], sizeof(buffers[i])-1, 1, pipes[i]);
				pclose(pipes[i]);
			}
		}
		
		pthread_mutex_lock(&status_bar.lock);
		for (int i = 0; i < NUM_STATUS_BLOCKS; ++i) {
			if (!pipes[i]) continue;
			memset(status_bar.block_buffers[i], 0, sizeof(status_bar.block_buffers[i]));
			char *in = buffers[i], *out = status_bar.block_buffers[i];
			size_t length = 0;
			
			for (length = 0; 
				 (length < (sizeof(buffers[i])-1)) && *in && (*in != '\n'); 
				 ++length, ++in, ++out) {
				*out = *in;
			}
			status_bar.block_lengths[i] = length;
		}
		pthread_mutex_unlock(&status_bar.lock);
	
		printf("Nunights\n");
		sleep(CONFIG.status_update_interval_seconds);
		printf("Wake up\n");
	}
	return NULL;
}

static void render_status_bar(Output *output) {
	char text_buffer[256];
	Glyph glyph_buffer[256];
	struct wlr_box draw_rect;
	struct wlr_box output_box;
	float matrix[9];
	const int text_y = output->bar_height - 4;
	if (text_y < 0) return;
	
	//wlr_matrix_identity(matrix);
	memcpy(matrix, output->wlr->transform_matrix, sizeof(matrix));
	wlr_output_layout_get_box(server.output_layout, output->wlr, &output_box);

	if (!strcmp(output->wlr->name, "DP-2")) {
		printf("%ux%u\n", output_box.width, output_box.height);
	}

	// Background
	draw_rect.x  = 0;
	draw_rect.y = 0;
	draw_rect.width = output_box.width;
	draw_rect.height = output->bar_height;
	
	wlr_render_rect(server.renderer, &draw_rect, CONFIG.bar_background_color, matrix);
	
	// View indicators
	const int view_indicator_padding = 10;
	int x_offset = view_indicator_padding;
	
	for (int i = 0; i < 9; ++i) {
		char string[2] = {'1' + i, 0};
		Glyph glyph;
		View *view = &output->views[i];
		
		get_string_glyphs(string, &glyph, NULL);
		
		if (output->current_view == i) {
			draw_rect.x = x_offset - (view_indicator_padding/2);
			draw_rect.y = 0;
			draw_rect.width = glyph.width + view_indicator_padding;
			draw_rect.height = output->bar_height;
			wlr_render_rect(server.renderer, &draw_rect, CONFIG.bar_selection_color, matrix);
		}

		if (!wl_list_empty(view->layers[LAYER_VIEW_FLOATING]) || !wl_list_empty(view->layers[LAYER_VIEW_TILES]) ||
			!wl_list_empty(view->layers[LAYER_VIEW_FULLSCREEN])) {
			draw_rect.x = x_offset - view_indicator_padding/2 + 1;
			draw_rect.y = 1;
			draw_rect.width = 2;
			draw_rect.height = 2;
			wlr_render_rect(server.renderer, &draw_rect, (float[]){1,1,1,1}, matrix);
		}
		
		x_offset += render_glyphs(matrix, &glyph, 1, x_offset, text_y) + view_indicator_padding;
	}
	
	// Gap to client title
	x_offset += 30;
	
	// Print focused client name
	if (server.focused_client) {
		const char *client_title = get_client_title(server.focused_client);
		size_t client_title_length;
		snprintf(text_buffer, sizeof(text_buffer), "%s [%d]", client_title, server.focused_client->layer);
		get_string_glyphs(text_buffer, glyph_buffer, &client_title_length);
		render_glyphs(matrix, glyph_buffer, client_title_length, x_offset, text_y);
	}
	
	// Status blocks
	Glyph status_glyph_buffer[ARRAY_LENGTH(STATUS_BLOCKS)][128];
	
	size_t block_draw_lengths[NUM_STATUS_BLOCKS];
	x_offset = output_box.width - CONFIG.status_padding;
	
	pthread_mutex_lock(&status_bar.lock);
	for (int i = 0; i < ARRAY_LENGTH(STATUS_BLOCKS); ++i) {
		size_t string_length = status_bar.block_lengths[i];
		block_draw_lengths[i] = get_string_glyphs(status_bar.block_buffers[i], status_glyph_buffer[i], NULL);
		
		x_offset -= block_draw_lengths[i];
		render_glyphs(matrix, status_glyph_buffer[i], string_length, x_offset, text_y);
		x_offset -= CONFIG.status_padding;
		
		draw_rect.x = x_offset;
		draw_rect.y = 0;
		draw_rect.width = CONFIG.status_separator_thickness;
		draw_rect.height = output->bar_height;
		wlr_render_rect(server.renderer, &draw_rect, CONFIG.status_separator_color, matrix);
		x_offset -= CONFIG.status_padding + draw_rect.width;
	}
	pthread_mutex_unlock(&status_bar.lock);
}

/* ================================================================================
 * Server functions
 * ================================================================================*/
static void arrange_output(Output *output) {
	View *view = &output->views[output->current_view];
	view->layout(output);
}

static void configure_client(Client *client, int32_t x, int32_t y, int32_t width, int32_t height, uint32_t flags) {
	if (x == INT32_MAX) x = client->config.x;
	if (y == INT32_MAX) y = client->config.y;
	if (width == INT32_MAX) width = client->config.width;
	if (height == INT32_MAX) height = client->config.height;
	if (flags == UINT32_MAX) flags = client->config.flags;
	
	switch (client->type) {
		case CLIENT_TYPE_XDG_TOPLEVEL: {
			struct wlr_xdg_toplevel *toplevel = client->xdg_surface->toplevel;
			wlr_xdg_toplevel_set_size(toplevel, width, height);
			wlr_xdg_toplevel_set_fullscreen(toplevel, (flags & CLIENT_CONFIG_FULLSCREEN) != 0);
			break;
		}
#if USE_XWAYLAND
		case CLIENT_TYPE_XWAYLAND: {
			struct wlr_box output_box = {0};
			struct wlr_xwayland_surface *surface = client->xwayland_surface;
			xcb_size_hints_t *hints = surface->size_hints;
			if (client->output) wlr_output_layout_get_box(server.output_layout, client->output->wlr, &output_box);
			if (hints) {
				if (hints->min_width > 0) width = MAX(hints->min_width, width);
				if (hints->min_height > 0) height = MAX(hints->min_height, height);
				if (hints->max_width > 0) width = MIN(hints->max_width, width);
				if (hints->max_height > 0) height = MIN(hints->max_height, height);
			}
			wlr_xwayland_surface_configure(surface, x + output_box.x, y + output_box.y, width, height);
			wlr_xwayland_surface_set_fullscreen(surface, (flags & CLIENT_CONFIG_FULLSCREEN) != 0);
			wlr_xwayland_surface_set_minimized(surface, (flags & CLIENT_CONFIG_MINIMIZED) != 0);
			break;
		}
#endif
		default: break;
	}

	client->config.x = x;
	client->config.y = y;
	client->config.width = width;
	client->config.height = height;
	client->config.flags = flags;
}

static void focus_client(Client *client) {
	if (server.focus_grabbed || (client && client == server.focused_client)) return;
	
	if (server.focused_client) {
		Client *old_client = server.focused_client;
		wlr_seat_pointer_notify_clear_focus(server.seat);
		//wlr_seat_keyboard_notfiy_clear_focus(server.seat);
		
		if (old_client->type == CLIENT_TYPE_XDG_TOPLEVEL) {
			wlr_xdg_toplevel_set_activated(old_client->xdg_surface->toplevel, false);
		}
#if USE_XWAYLAND
		else if (old_client->type == CLIENT_TYPE_XWAYLAND) {
			wlr_xwayland_surface_activate(old_client->xwayland_surface, false);
		}
#endif
		struct wlr_surface *wlr_surface = get_client_wlr_surface(old_client);
		struct wlr_pointer_constraint_v1 *constraint = 
			wlr_pointer_constraints_v1_constraint_for_surface(server.pointer_constraints, wlr_surface, server.seat);
		if (constraint) {
			printf("Satistfactory\n");
			wlr_pointer_constraint_v1_send_deactivated(constraint);
		}
	}
	
	if (!client) {
		server.focused_client = NULL; 
		return;
	}
		
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server.seat);
	struct wlr_surface *client_surface = get_client_wlr_surface(client);
	struct wlr_box output_box;
	
	wlr_output_layout_get_box(server.output_layout, client->output->wlr, &output_box);
	
	if (client->type == CLIENT_TYPE_XDG_TOPLEVEL) {
		wlr_xdg_toplevel_set_activated(client->xdg_surface->toplevel, true);
	}
#if USE_XWAYLAND
	else if (client->type == CLIENT_TYPE_XWAYLAND) {
		wlr_xwayland_surface_activate(client->xwayland_surface, true);
	}
#endif

	printf("Pointer enter at (%g, %g)\n", server.cursor->x - client->config.x - output_box.x, server.cursor->y - client->config.y - output_box.y);
	wlr_seat_pointer_notify_enter(server.seat, client_surface, 
								  server.cursor->x - client->config.x - output_box.x, 
								  server.cursor->y - client->config.y - output_box.y);
	
	if (keyboard) {
		wlr_seat_keyboard_notify_enter(server.seat, client_surface, 
									   keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
	}
	
	server.focused_client = client;
}

static Client *get_client_under_cursor(Output *focused_output) {
	struct wlr_box output_box;
	
	
	Client *client = NULL;

	enum Layer search_order[] = {
		LAYER_OUTPUT_OVERLAY,
		LAYER_OUTPUT_TOP,
		LAYER_OUTPUT_STICKY,
		LAYER_VIEW_FULLSCREEN,
		LAYER_VIEW_FLOATING,
		LAYER_VIEW_TILES,
		LAYER_OUTPUT_BOTTOM,
	};
	
	
	for (int i = 0; i < ARRAY_LENGTH(search_order); ++i) {
		Output *output;
		wl_list_for_each(output, &server.output_list, link) {
			View *view = &output->views[output->current_view];
			int min_layer = !wl_list_empty(view->layers[LAYER_VIEW_FULLSCREEN]) ? LAYER_VIEW_FULLSCREEN : 0;

			wlr_output_layout_get_box(server.output_layout, output->wlr, &output_box);
			int layer = search_order[i];
			if (layer < min_layer) break;
			if (!should_render_layer(view, layer)) continue;
			client = get_client_under_cursor_in_list(&output_box, view->layers[layer]);
			if (client) return client;
		}
	}
	
	return NULL;
}

// @TODO: Iterate over children of the client first
static Client *get_client_under_cursor_in_list(struct wlr_box *output_box, struct wl_list *list) {
	Client *client;
	
	if (wl_list_empty(list)) return NULL;
	
	wl_list_for_each(client, list, link) {
		struct wlr_surface *client_surface = get_client_wlr_surface(client);
		struct wlr_box client_box = {
			.x = client->config.x + output_box->x,
			.y = client->config.y + output_box->y,
			.width = client_surface->current.width,
			.height = client_surface->current.height,
		};
		
		if (wlr_box_contains_point(&client_box, server.cursor->x, server.cursor->y)) {
			return client;
		}
	}
	
	return NULL;
}

static void handle_destroy_surface(struct wl_listener *listener, void *data) {
	Client *client = wl_container_of(listener, client, on_destroy);
	free(client);
}

static void handle_unmap_surface(struct wl_listener *listener, void *data) {
	Client *client = wl_container_of(listener, client, on_unmap);
	if (client == server.focused_client) {
		server.focus_grabbed = 0;
		focus_client(NULL);
	}
	if (client->link.next) wl_list_remove(&client->link);
	arrange_output(client->output);
	if (client->update_link.next) wl_list_remove(&client->update_link);
	
	client->mapped = 0;
}

static void handle_map_surface(struct wl_listener *listener, void *data) {
	Client *client = wl_container_of(listener, client, on_map);
	Output *output = client->output ? client->output : server.focused_output;
	View *view = client->view ? client->view : &output->views[output->current_view];
	
	client->mapped = 1;
	client->output = output;
	client->view = view;
	
	if (client->type == CLIENT_TYPE_XDG_TOPLEVEL) {
		wl_list_insert(&server.client_update_list, &client->update_link);
		client->layer = LAYER_VIEW_TILES;
		// If its a popup we don't care because it will still be rendered
		if (client->xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
			return;
		}
	
		if (client->requesting_fullscreen) {
			configure_client(client, 0, 0, output->wlr->width, output->wlr->height, CLIENT_CONFIG_FULLSCREEN);
		}

		wlr_xdg_toplevel_set_activated(client->xdg_surface->toplevel, true);
	}
#if USE_XWAYLAND
	else if (client->type == CLIENT_TYPE_XWAYLAND) {
		wl_list_insert(&server.client_update_list, &client->update_link);
		struct wlr_xwayland_surface *surface = client->xwayland_surface;
		printf("Map XWayland surface %s (%p) (override_redirect = %d modal = %d)\n", 
			   get_client_title(client), client, surface->override_redirect, surface->modal);
		printf("Geometry: (%d, %d) saved: (%d, %d)\n",
			   surface->width, surface->height, surface->saved_width, surface->saved_height);
		if (client->requesting_fullscreen) {
			configure_client(client, 0, 0, output->wlr->width, output->wlr->height, CLIENT_CONFIG_FULLSCREEN);
			client->requesting_fullscreen = 0;
			client->old_layer = LAYER_VIEW_FLOATING;
			client->layer = LAYER_VIEW_FULLSCREEN;
			//client->layer = LAYER_VIEW_FLOATING;
		}
		else {
			client->layer = LAYER_VIEW_TILES;
			client->old_layer = LAYER_VIEW_TILES;
#if 0
			uint32_t netwm_mask = 0;
			int is_popup = 0;

			for (int i = 0; i < surface->window_type_len; ++i) {
				is_popup |= surface->window_type[i] == NET_WM_ATOMS[NET_WM_TYPE_MENU];
				is_popup |= surface->window_type[i] == NET_WM_ATOMS[NET_WM_TYPE_DIALOG];
				is_popup |= surface->window_type[i] == NET_WM_ATOMS[NET_WM_TYPE_SPLASH];
				is_popup |= surface->window_type[i] == NET_WM_ATOMS[NET_WM_TYPE_TOOLBAR];
				is_popup |= surface->window_type[i] == NET_WM_ATOMS[NET_WM_TYPE_UTILITY];

				netwm_mask |= (surface->window_type[i] == NET_WM_ATOMS[NET_WM_TYPE_MENU]) << NET_WM_TYPE_MENU;
				netwm_mask |= (surface->window_type[i] == NET_WM_ATOMS[NET_WM_TYPE_DIALOG]) << NET_WM_TYPE_DIALOG;
				netwm_mask |= (surface->window_type[i] == NET_WM_ATOMS[NET_WM_TYPE_SPLASH]) << NET_WM_TYPE_SPLASH;
				netwm_mask |= (surface->window_type[i] == NET_WM_ATOMS[NET_WM_TYPE_TOOLBAR]) << NET_WM_TYPE_TOOLBAR;
				netwm_mask |= (surface->window_type[i] == NET_WM_ATOMS[NET_WM_TYPE_UTILITY]) << NET_WM_TYPE_UTILITY;
			}

			printf("%s NET_WM mask: 0x%x\n", get_client_title(client), netwm_mask);

			if (is_popup) {
				client->config.x = surface->x;
				client->config.y = surface->y;
				client->layer = LAYER_OUTPUT_POPUPS;
				wl_list_insert(view->layers[LAYER_OUTPUT_POPUPS], &client->link);
				return;
			}
#else
			if (!wlr_xwayland_or_surface_wants_focus(client->xwayland_surface)) {
				configure_client(client, surface->x, surface->y, surface->width, surface->height, 0);
				client->layer = LAYER_OUTPUT_POPUPS;
				client->old_layer = LAYER_OUTPUT_POPUPS;
				wl_list_insert(view->layers[LAYER_OUTPUT_POPUPS], &client->link);
				return;
			}
#endif		
			if (surface->size_hints) {
				bool client_is_fixed_size = surface->size_hints->min_width == surface->size_hints->max_width;
				client_is_fixed_size &= surface->size_hints->min_height == surface->size_hints->max_height;
				if (client_is_fixed_size) {
					client->layer = LAYER_VIEW_FLOATING;
					uint32_t width = surface->width;
					uint32_t height = surface->height;
					configure_client(client, 
									 (output->wlr->width / 2) - (width / 2),
									 (output->wlr->height / 2)  - (height / 2),
									 width, height, 0);
				}	
			}
		}
		
	}
#endif
	else if (client->type == CLIENT_TYPE_LAYER) {
		struct wlr_layer_surface_v1 *layer_surface = client->layer_surface;
		output = layer_surface->output ? layer_surface->output->data : server.focused_output;
		view = &output->views[output->current_view];
		
		enum Layer layer_map[] = {
			[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND] = LAYER_OUTPUT_BACKGROUND,
			[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM] = LAYER_OUTPUT_BOTTOM,
			[ZWLR_LAYER_SHELL_V1_LAYER_TOP] = LAYER_OUTPUT_TOP,
			[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY] = LAYER_OUTPUT_OVERLAY,
		};
		
		client->layer = layer_map[layer_surface->pending.layer];
		client->output = output;
		client->view = view;
		
		printf("Map layer surface on %u\n", client->layer);
		
		if (client->layer <= LAYER_OUTPUT_TOP && client->layer > LAYER_OUTPUT_BOTTOM) {
			client->config.y += output->bar_height;
		}
		
		wlr_layer_surface_v1_configure(layer_surface, 
									   layer_surface->pending.actual_width, 
									   layer_surface->pending.actual_height);
		
		wl_list_insert(view->layers[client->layer], &client->link);
		
		if (layer_surface->current.keyboard_interactive != ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE) {
			focus_client(client);
			server.focus_grabbed = 1;
		}
		return;
	}
	
	wl_list_insert(view->layers[client->layer], &client->link);
	if (client->layer == LAYER_VIEW_TILES) arrange_output(output);
	focus_client(client);
}

static void move_client_to_layer(Client *client, enum Layer dest_layer) {
	Output *output = client->output;
	View *view = &output->views[output->current_view];
	struct wl_list *dest_list = view->layers[dest_layer];
	
	if (client->link.next) wl_list_remove(&client->link);
	wl_list_insert(dest_list, &client->link);
	
	if (dest_layer == LAYER_VIEW_TILES) arrange_output(output);
	
	client->old_layer = client->layer;
	client->layer = dest_layer;
}

static void process_cursor_move(uint32_t time_msec) {
	Client *client = server.focused_client;
	
	if (server.cursor_mode == CURSOR_MODE_NORMAL) {
		if (server.focus_mode == FOCUS_MODE_ON_HOVER) {
			update_focus();
		}
		client = server.focused_client;
		
		if (!client) {
			wlr_xcursor_manager_set_cursor_image(server.xcursor_manager, "left_ptr", server.cursor);
			wlr_seat_pointer_notify_motion(server.seat, time_msec, server.cursor->x, server.cursor->y);
		} 
		else {
			struct wlr_box offset;
			wlr_output_layout_get_box(server.output_layout, client->output->wlr, &offset);
			wlr_seat_pointer_notify_motion(server.seat, time_msec, server.cursor->x - client->config.x - offset.x, server.cursor->y - client->config.y - offset.y);
		}
		
		return;
	} 
	else if (!client) {
		server.cursor_mode = CURSOR_MODE_NORMAL;
		return;
	}
	
	struct wlr_box output_box;
	Output *output;
	
	if (server.cursor_mode == CURSOR_MODE_MOVE) {
		struct wlr_output *wlr_output = wlr_output_layout_output_at(server.output_layout, server.cursor->x, server.cursor->y);
		if (!wlr_output) return;
		wlr_output_layout_get_box(server.output_layout, wlr_output, &output_box);
		output = wlr_output->data;
		client = get_client_under_cursor(output);
		if (!client) return;

		if (output != client->output) {
			struct wlr_box old_output;
			wlr_output_layout_get_box(server.output_layout, client->output->wlr, &old_output);
			client->output = output;
			client->view = &output->views[output->current_view];
			//client->config.x += (old_output.x - output_box.x);
			//client->config.y += (old_output.y - output_box.y);
			configure_client(client, client->config.x + (old_output.x - output_box.x),
							 client->config.y + (old_output.x - output_box.x),
							 INT32_MAX, INT32_MAX, UINT32_MAX);
			printf("Move to new output (%d, %d) (%d, %d)\n", output_box.x, output_box.y, client->config.x, client->config.y);
			if (client->link.next) wl_list_remove(&client->link);
			wl_list_insert(client->view->layers[client->layer], &client->link);
		}

		configure_client(client, server.cursor->x - server.interact_grab_x - output_box.x,
						 server.cursor->y - server.interact_grab_y - output_box.y,
						 INT32_MAX, INT32_MAX, UINT32_MAX);
	}
	else if (server.cursor_mode == CURSOR_MODE_RESIZE) {
		output = client->output;
		wlr_output_layout_get_box(server.output_layout, output->wlr, &output_box);
		configure_client(client, INT32_MAX, INT32_MAX, 
						 server.cursor->x - (client->config.x + output_box.x),
						 server.cursor->y - (client->config.y + output_box.y), UINT32_MAX);
	}
	
}

static void set_cursor_mode(enum Cursor_Mode mode) {
	Client *client = server.focused_client;
	Output *output = server.focused_output;

	if (mode == server.cursor_mode) return;

	if (mode == CURSOR_MODE_NORMAL) {
		server.cursor_mode = mode;
		//if (!server.seat->pointer_state.focused_client)
			wlr_xcursor_manager_set_cursor_image(server.xcursor_manager, "left_ptr", server.cursor);
		return;
	}
	else if (!client || !can_interact_with_client(client)) return;
	
	// Mode is interactive
	output = client->output;
	struct wlr_box output_box;
	struct wlr_surface *client_surface;
	wlr_output_layout_get_box(server.output_layout, output->wlr, &output_box);
	client_surface = get_client_wlr_surface(client);
	
	if (client->layer == LAYER_VIEW_TILES) 
		move_client_to_layer(client, LAYER_VIEW_FLOATING);
	
	if (mode == CURSOR_MODE_RESIZE) {
		wlr_xcursor_manager_set_cursor_image(server.xcursor_manager, "bottom_right_corner", server.cursor);
		wlr_cursor_warp(server.cursor, NULL, 
						client->config.x + output_box.x + client_surface->current.width, 
						client->config.y + output_box.y + client_surface->current.height);
	}
	else if (mode == CURSOR_MODE_MOVE) {
		wlr_xcursor_manager_set_cursor_image(server.xcursor_manager, "hand1", server.cursor);
		server.interact_grab_x = server.cursor->x - (client->config.x + output_box.x);
		server.interact_grab_y = server.cursor->y - (client->config.y + output_box.y);
	}
	
	server.cursor_mode = mode;
}

static void send_client_close(Client *client) {
	if (client == server.focused_client) 
		focus_client(NULL);
	
	if (client->type == CLIENT_TYPE_XDG_TOPLEVEL) {
		wlr_xdg_toplevel_send_close(client->xdg_surface->toplevel);
	}
#if USE_XWAYLAND
	else if (client->type == CLIENT_TYPE_XWAYLAND) {
		wlr_xwayland_surface_close(client->xwayland_surface);
	}
#endif
}


static bool try_key_bind(uint32_t modifiers, xkb_keysym_t key) {
	const struct Key_Bind *bind = KEY_BINDS;
	const struct Key_Bind *bind_end = &KEY_BINDS[ARRAY_LENGTH(KEY_BINDS)];
	bool found = false;
	
	for (; bind < bind_end; ++bind) {
		if (bind->modifiers == modifiers && bind->key == key) {
			found = true;
			bind->function(bind->arg);
		}
	}
	
	return found;
}

#if 0
static void update_clients() {
	Client *client;
	wl_list_for_each_reverse(client, &server.client_update_list, update_link) {
		struct wlr_box output_box;
		bool client_moved = (client->config.x != client->applied_state.x) || (client->config.y != client->applied_state.y);
		bool client_resized = (client->width != client->applied_state.width) || (client->height != client->applied_state.height);
		bool client_is_visible = should_render_layer(client->view, client->layer);
		
		wlr_output_layout_get_box(server.output_layout, client->output->wlr, &output_box);

		client_is_visible &= !layer_is_view_layer(client->layer) || (client->view == OUTPUT_CURRENT_VIEW(client->output));
		client_is_visible &= !(client->view->fullscreen_client && client->layer < LAYER_OUTPUT_STICKY);
		
		// Client wants fullscreen
		if (client->requesting_fullscreen && !client_is_fullscreen(client)) {
			make_client_fullscreen(client);
			client->requesting_fullscreen = 0;
		}
		
		if (client->type == CLIENT_TYPE_XDG_TOPLEVEL && client_resized) {
			wlr_xdg_toplevel_set_size(client->xdg_surface->toplevel, client->width, client->height);
		}
		
#if USE_XWAYLAND
		if (client->type == CLIENT_TYPE_XWAYLAND) {
			struct wlr_xwayland_surface *surface = client->xwayland_surface;
			if ((client_moved || client_resized) && !surface->override_redirect) {
				xcb_size_hints_t *hints = surface->size_hints;
				// Apply size hints
				if (hints) {
					if (hints->min_width>0) client->width = MAX(client->width, surface->size_hints->min_width);
					if (hints->max_width>0) client->width = MIN(client->width, surface->size_hints->max_width);
					if (hints->min_height>0) client->height = MAX(client->height, surface->size_hints->min_height);
					if (hints->max_height>0) client->height = MIN(client->height, surface->size_hints->max_height);
				}
				wlr_xwayland_surface_configure(surface, client->config.x + output_box.x, client->config.y + output_box.y, client->width, client->height);
			}
			else if (client_moved || client_resized) { // Client has override redirect. Update our state to reflect the client state
				client->config.x = client->xwayland_surface->x;
				client->config.y = client->xwayland_surface->y;
				client->width = client->xwayland_surface->width;
				client->height = client->xwayland_surface->height;
			}
			//wlr_xwayland_surface_set_minimized(client->xwayland_surface, !client_is_visible);
			//wlr_xwayland_surface_activate(client->xwayland_surface, client_is_visible);
		}
#endif
		client->applied_state.x = client->config.x;
		client->applied_state.y = client->config.y;
		client->applied_state.width = client->width;
		client->applied_state.height = client->height;
	}
}
#endif

static void update_focus() {
	if (server.focus_grabbed) return;
	struct wlr_output *wlr_output = 
		wlr_output_layout_output_at(server.output_layout, server.cursor->x, server.cursor->y);
	if (wlr_output)
		server.focused_output = wlr_output->data;
	
	Client *client = get_client_under_cursor(server.focused_output);
	if (client) focus_client(client);
}

static void update_visibility() {
	Client *client;
	if (wl_list_empty(&server.client_update_list)) return;
	wl_list_for_each(client, &server.client_update_list, update_link) {
		bool client_is_visible = false;
		client_is_visible |= client->view == OUTPUT_CURRENT_VIEW(client->output);
		client_is_visible |= !layer_is_view_layer(client->layer);
		client_is_visible &= should_render_layer(client->view, client->layer);
		
		if (!wl_list_empty(client->view->layers[LAYER_VIEW_FULLSCREEN])) {
			client_is_visible &= client->layer >= LAYER_VIEW_FULLSCREEN;
		}
		
		if (client->type == CLIENT_TYPE_XDG_TOPLEVEL) {
			wlr_xdg_toplevel_set_activated(client->xdg_surface->toplevel, client_is_visible);
		}
#if USE_XWAYLAND
		else if (client->type == CLIENT_TYPE_XWAYLAND) {
			wlr_xwayland_surface_activate(client->xwayland_surface, client_is_visible);
		}
#endif
	}
}

/* ================================================================================
 * Output management protocol
 * ================================================================================*/
static void output_manager_test_or_apply(struct wlr_output_configuration_v1 *config, int apply) {
	struct wlr_output_head_v1 *head;
	debug_printf("OUTPUT %s:\n", apply ? "APPLY" : "TEST");

	wl_list_for_each(head, &config->heads, link) {
		struct wlr_output *output = head->state.output;
		debug_printf("FOR %s\n", output->name);
		if (head->state.mode) {
			debug_printf("SET MODE %ux%u\n", head->state.mode->width, head->state.mode->height);
			wlr_output_set_mode(output, head->state.mode);
		} else {
			debug_printf("SET CUSTOM MODE\n");
			wlr_output_set_custom_mode(output, head->state.custom_mode.width, 
					head->state.custom_mode.height, head->state.custom_mode.refresh);
		}
		debug_printf("SET SCALE\n");
		wlr_output_set_scale(output, head->state.scale);
		debug_printf("SET TRANSFORM\n");
		wlr_output_set_transform(output, head->state.transform);
		debug_printf("SET ADAPTIVE SYNC\n");
		wlr_output_enable_adaptive_sync(output, head->state.adaptive_sync_enabled);
		debug_printf("TEST\n");
		if (!wlr_output_test(output)) {
			debug_printf("TEST FAILED\n");
			wlr_output_rollback(output);
			debug_printf("ROLLED BACK\n");
			wlr_output_configuration_v1_send_failed(config);
			return;
		}
		else if (apply) {
			debug_printf("TEST SUCCEEDED\n");
			wlr_output_commit(output);
			wlr_output_rollback(output);
			debug_printf("COMMITTED\n");
		}
	}

	update_output_configuration();
	wlr_output_configuration_v1_send_succeeded(config);
}

static void handle_output_manager_apply(struct wl_listener *listener, void *data) {
	output_manager_test_or_apply(data, 1);
}

static void handle_output_manager_test(struct wl_listener *listener, void *data) {
	output_manager_test_or_apply(data, 0);
}

/* ================================================================================
 * XDG shell
 * ================================================================================*/
static void handle_toplevel_request_fullscreen(struct wl_listener *listener, void *data) {
	Client *client = wl_container_of(listener, client, on_request_fullscreen);
	Output *output = client->output;
	struct wlr_xdg_toplevel *toplevel = client->xdg_surface->toplevel;
	move_client_to_layer(client, toplevel->requested.fullscreen ? LAYER_VIEW_FULLSCREEN : LAYER_VIEW_FLOATING);
	if (client->mapped)	{
		configure_client(client, 0, 0, output->wlr->width, output->wlr->height, 
					 	 CLIENT_CONFIG_FULLSCREEN * (client->xdg_surface->toplevel->requested.fullscreen == true));
	}
	else {
		client->requesting_fullscreen = 1;
	}
}

static void handle_new_xdg_surface(struct wl_listener *listener, void *data) {
	struct wlr_xdg_surface *xdg_surface = data;
	Client *client = calloc(1, sizeof(Client));
	
	client->type = CLIENT_TYPE_XDG_TOPLEVEL;
	client->xdg_surface = xdg_surface;
	
	listen(&client->on_destroy, &handle_destroy_surface, &xdg_surface->events.destroy);
	listen(&client->on_map, &handle_map_surface, &xdg_surface->events.map);
	listen(&client->on_unmap, &handle_unmap_surface, &xdg_surface->events.unmap);
	
	if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
		listen(&client->on_request_fullscreen, &handle_toplevel_request_fullscreen, &xdg_surface->toplevel->events.request_fullscreen);
	}
	
}

/* ================================================================================
 * Layer shell
 * ================================================================================*/
static void handle_new_layer_surface(struct wl_listener *listener, void *data) {
	struct wlr_layer_surface_v1 *layer_surface = data;
	Client *client = calloc(1, sizeof(Client));
	
	client->type = CLIENT_TYPE_LAYER;
	client->layer_surface = layer_surface;
	
	{
		uint32_t anchor = layer_surface->current.anchor;
		uint32_t width = 0;
		uint32_t height = 0;
		Output *output;
		
		if (layer_surface->output) output = layer_surface->output->data;
		else output = server.focused_output;

		int effective_output_width, effective_output_height;
		wlr_output_effective_resolution(output->wlr, &effective_output_width, &effective_output_height);

		// Anchored to top and bottom
		if ((anchor & (1|2)) == (1|2)) {
			height = effective_output_height;
		}
		
		// Anchored to left and right
		if ((anchor & (4|8)) == (4|8)) {
			width = effective_output_width;
		}
		
		if (!width) width = layer_surface->current.desired_width;
		if (!height) height = layer_surface->current.desired_height;
		
		wlr_layer_surface_v1_configure(layer_surface, width, height);
	}
	
	listen(&client->on_destroy, &handle_destroy_surface, &layer_surface->events.destroy);
	listen(&client->on_map, &handle_map_surface, &layer_surface->events.map);
	listen(&client->on_unmap, &handle_unmap_surface, &layer_surface->events.unmap);
}

/* ================================================================================
 * XWayland
 * ================================================================================*/
#if USE_XWAYLAND

static void setup_xwayland(struct wl_listener *listener, void *data) {
	setenv("DISPLAY", server.xwayland->display_name, 1);
	/*xcb_connection_t *xcb = xcb_connect(server.xwayland->display_name, NULL);
	xcb_intern_atom_reply_t *reply;
	xcb_intern_atom_cookie_t cookie;
	
	const char *atom_names[] = {
		[NET_WM_TYPE_DIALOG] = "_NET_WM_WINDOW_TYPE_DIALOG",
		[NET_WM_TYPE_MENU] = "_NET_WM_WINDOW_TYPE_MENU",
		[NET_WM_TYPE_SPLASH] = "_NET_WM_WINDOW_TYPE_SPLASH",
		[NET_WM_TYPE_TOOLBAR] = "_NET_WM_WINDOW_TYPE_TOOLBAR",
		[NET_WM_TYPE_UTILITY] = "_NET_WM_WINDOW_TYPE_UTILITY",
	};
	
	for (int i = 0; i < NUM_NET_WM_TYPES; ++i) {
		cookie = xcb_intern_atom(xcb, 0, strlen(atom_names[i]), atom_names[i]);
		reply = xcb_intern_atom_reply(xcb, cookie, NULL);
		NET_WM_ATOMS[i] = reply->atom ? reply->atom : 0;
		free(reply);
	}
	
	xcb_disconnect(xcb);*/
	wlr_xwayland_set_seat(server.xwayland, server.seat);
	
	struct wlr_xcursor *xcursor =
		wlr_xcursor_manager_get_xcursor(server.xcursor_manager, "left_ptr", 1);
	
	if (xcursor) {
		struct wlr_xcursor_image *image = xcursor->images[0];
		wlr_xwayland_set_cursor(server.xwayland, image->buffer, image->width * 4, image->width, image->height,
								image->hotspot_x, image->hotspot_y);
	}
	
}

static void handle_xwayland_request_configure(struct wl_listener *listener, void *data) {
	struct wlr_xwayland_surface_configure_event *event = data;
	Client *client = wl_container_of(listener, client, on_request_configure);
	configure_client(client, event->x, event->y, event->width, event->height, UINT32_MAX);
	printf("Configure XWayland surface %s (%p) with (%d, %d, %d, %d)\n", 
			get_client_title(client), client, event->x, event->y, event->width, event->height);
}

static void handle_xwayland_request_minimize(struct wl_listener *listener, void *data) {
	struct wlr_xwayland_minimize_event *event = data;
	Client *client = wl_container_of(listener, client, on_request_configure);
	printf("Configure XWayland surface %s (%p) requesting minimize (event->minimize = %d)\n", 
		   get_client_title(client), client, event->minimize);
}

// @Note: XWayland clients sometimes request for fullscreen before being mapped, so we need
// to defer making it fullscreen to update_clients()
static void handle_xwayland_request_fullscreen(struct wl_listener *listener, void *data) {
	Client *client = wl_container_of(listener, client, on_request_fullscreen);
	struct wlr_xwayland_surface *surface = client->xwayland_surface;
	printf("XWayland surface %s (%p) requesting fullscreen (fullscreen = %d)\n", get_client_title(client), client, surface->fullscreen);
	if (!client->mapped) client->requesting_fullscreen = surface->fullscreen;
	else {
		configure_client(client, 0, 0, client->output->wlr->width, client->output->wlr->height, 
						 client->config.flags ^ CLIENT_CONFIG_FULLSCREEN);
		if (client->config.flags & CLIENT_CONFIG_FULLSCREEN) {
			move_client_to_layer(client, LAYER_VIEW_FULLSCREEN);
		}
		else {
			move_client_to_layer(client, LAYER_VIEW_FLOATING);
		}
	}
}

static void handle_new_xwayland_surface(struct wl_listener *listener, void *data) {
	struct wlr_xwayland_surface *surface = data;
	Client *client = calloc(1, sizeof(Client));
	
	client->type = CLIENT_TYPE_XWAYLAND;
	client->xwayland_surface = surface;
	
	listen(&client->on_map, &handle_map_surface, &surface->events.map);
	listen(&client->on_unmap, &handle_unmap_surface, &surface->events.unmap);
	listen(&client->on_request_configure, &handle_xwayland_request_configure, &surface->events.request_configure);
	listen(&client->on_request_fullscreen, &handle_xwayland_request_fullscreen, &surface->events.request_fullscreen);
	listen(&client->on_request_minimize, &handle_xwayland_request_minimize, &surface->events.request_minimize);
}

#endif
/* ================================================================================
 * Pointer contraints
 * ================================================================================*/
static void handle_pointer_constraint_destroy(struct wl_listener *listener, void *data) {
	Pointer_Constraint *constraint = wl_container_of(listener, constraint, on_destroy);
	free(constraint);
}

// @TODO
#if 0
static void handle_pointer_constraint_set_region(struct wl_listener *listener, void *data) {	
}
#endif

static void handle_new_pointer_constraint(struct wl_listener *listener, void *data) {
	struct wlr_pointer_constraint_v1 *constraint = data;
	Pointer_Constraint *server_constraint = calloc(1, sizeof(Pointer_Constraint));
	server_constraint->constraint = constraint;
	/*listen(&server_constraint->on_set_region, &handle_pointer_constraint_set_region, 
		   &constraint->events.set_region);*/
	listen(&server_constraint->on_destroy, &handle_pointer_constraint_destroy, 
		   &constraint->events.destroy);
	
	if (server.focused_client && (get_client_wlr_surface(server.focused_client) == constraint->surface)) {
		wlr_pointer_constraint_v1_send_activated(constraint);
	}
	
	printf("New pointer constraint:\n"
		   "type = %d\n"
		   "region = (%d, %d, %d, %d)\n",
		   constraint->type,
		   constraint->region.extents.x1, constraint->region.extents.y1,
		   constraint->region.extents.x2, constraint->region.extents.y2);
}

/* ================================================================================
 * Seat
 * ================================================================================*/
static void handle_seat_request_set_cursor(struct wl_listener *listener, void *data) {
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	// Only want to allow the client to set the cursor if it is focused
	if (server.seat->pointer_state.focused_client == event->seat_client) {
		wlr_cursor_set_surface(server.cursor, event->surface, event->hotspot_x, event->hotspot_y);
	}
}

static void handle_seat_request_set_selection(struct wl_listener *listener, void *data) {
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(server.seat, event->source, event->serial);
}

/* ================================================================================
 * Keyboard handling
 * ================================================================================*/
static void handle_keyboard_key(struct wl_listener *listener, void *data) {
	prevent_idle();
	
	struct wlr_keyboard_key_event *event = data;
	Keyboard *keyboard = wl_container_of(listener, keyboard, on_key);
	
	wlr_seat_set_keyboard(server.seat, keyboard->wlr);
	
	if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->wlr);
		uint32_t xkeycode = event->keycode + 8;
		const xkb_keysym_t *keysyms;
		uint32_t keysym_count = xkb_state_key_get_syms(keyboard->wlr->xkb_state, xkeycode, &keysyms);
		
		for (uint32_t i = 0; i < keysym_count; ++i) {
			if (try_key_bind(modifiers, keysyms[i])) return;
		}
	}
	
	wlr_seat_keyboard_notify_key(server.seat, event->time_msec, event->keycode, event->state);
}

static void handle_keyboard_modifiers(struct wl_listener *listener, void *data) {
	prevent_idle();
	
	Keyboard *keyboard = wl_container_of(listener, keyboard, on_modifiers);
	wlr_seat_set_keyboard(server.seat, keyboard->wlr);
	wlr_seat_keyboard_notify_modifiers(server.seat, &keyboard->wlr->modifiers);
}

/* ================================================================================
 * Cursor handling
 * ================================================================================*/
static void handle_cursor_axis(struct wl_listener *listener, void *data) {
	prevent_idle();
	
	struct wlr_pointer_axis_event *event = data;
	wlr_seat_pointer_notify_axis(server.seat, event->time_msec, event->orientation, 
								 event->delta, event->delta_discrete, event->source);
}

static void handle_cursor_button(struct wl_listener *listener, void *data) {
	prevent_idle();
	
	struct wlr_pointer_button_event *event = data;
	
	if (event->state && server.focus_mode == FOCUS_MODE_ON_CLICK) {
		update_focus();
	}
	
	if (event->state) {
		struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server.seat);
		if (keyboard) {
			uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard);
			for (int i = 0; i < ARRAY_LENGTH(BUTTON_BINDS); ++i) {
				if (BUTTON_BINDS[i].modifiers == modifiers && BUTTON_BINDS[i].button == event->button) {
					BUTTON_BINDS[i].function(BUTTON_BINDS[i].arg);
					return;
				}
			}
		}
	}
	else {
		set_cursor_mode(CURSOR_MODE_NORMAL);
	}
	
	wlr_seat_pointer_notify_button(server.seat, event->time_msec, event->button, event->state);
}

static void handle_cursor_frame(struct wl_listener *listener, void *data) {
	wlr_seat_pointer_notify_frame(server.seat);
}

static void handle_cursor_motion(struct wl_listener *listener, void *data) {
	if (CONFIG.cursor_movement_prevents_idle) prevent_idle();
	
	struct wlr_pointer_motion_event *event = data;
	Client *client = server.focused_client;
	
	// Check for pointer constraint and apply
	if (client) {
		struct wlr_surface *wlr_surface = get_client_wlr_surface(client);
		struct wlr_pointer_constraint_v1 *constraint = 
			wlr_pointer_constraints_v1_constraint_for_surface(server.pointer_constraints, wlr_surface, server.seat);
		if (constraint && constraint->type == WLR_POINTER_CONSTRAINT_V1_LOCKED) {
			wlr_relative_pointer_manager_v1_send_relative_motion(server.relative_pointer_manager,
																 server.seat, (uint64_t)event->time_msec * 1000,
																 event->delta_x, event->delta_y, event->unaccel_dx, event->unaccel_dy);
			return;
		}
	}

	wlr_cursor_move(server.cursor, &event->pointer->base, event->delta_x, event->delta_y);
	process_cursor_move(event->time_msec);
}

static void handle_cursor_motion_absolute(struct wl_listener *listener, void *data) {
	struct wlr_pointer_motion_absolute_event *event = data;
	wlr_cursor_warp_absolute(server.cursor, &event->pointer->base, event->x, event->y);
	process_cursor_move(event->time_msec);
}

/* ================================================================================
 * Outputs
 * ================================================================================*/

static void handle_output_destroy(struct wl_listener *listener, void *data) {
	Output *output = wl_container_of(listener, output, on_destroy);
	free(output);
}

static void handle_output_frame(struct wl_listener *listener, void *data) {
	Output *output = wl_container_of(listener, output, on_frame);
	View *view = &output->views[output->current_view];
	float clear_color[4] = {0, 0, 0, 1};
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	{
		int width, height;
		wlr_output_attach_render(output->wlr, NULL);
		wlr_output_effective_resolution(output->wlr, &width, &height);
		wlr_renderer_begin(server.renderer, output->wlr->width, output->wlr->height);
		wlr_renderer_clear(server.renderer, clear_color);
	}
	
	// Render clients below status bar
	if (wl_list_empty(view->layers[LAYER_VIEW_FULLSCREEN])) {
		for (int layer = 0; layer <= LAYER_VIEW_FLOATING; ++layer) {
			if (should_render_layer(view, layer)) {
				render_client_list(view->layers[layer], output, 0, 0, &now);
			}
		}
	}
	else {
		Client *client;
		wl_list_for_each_reverse(client, view->layers[LAYER_VIEW_FULLSCREEN], link) {
			struct wlr_surface *wlr_surface = get_client_wlr_surface(client);
			struct wlr_texture *texture = wlr_surface_get_texture(wlr_surface);
			float matrix[9];
			struct Surface_Iterator_Data iterator = {
				.parent = client,
				.draw_parent = 0,
				.x_offset = 0,
				.y_offset = 0,
				.when = &now,
				.matrix = matrix,
			};
			
			if (texture) {
				memcpy(matrix, output->wlr->transform_matrix, sizeof(matrix));
				wlr_matrix_scale(matrix, output->wlr->width, output->wlr->height);
				wlr_render_texture_with_matrix(server.renderer, texture, matrix, 1.f);
				memcpy(matrix, output->wlr->transform_matrix, sizeof(matrix));
				wlr_surface_for_each_surface(wlr_surface, &surface_render_iterator, &iterator);
				wlr_surface_send_frame_done(wlr_surface, &now);
				break; // Break from the loop after we render the top-most client
			}
		}
	}
	
	// Render floating clients from other outputs
	if (wl_list_empty(view->layers[LAYER_VIEW_FULLSCREEN]) && wl_list_length(&server.output_list) > 1) {
		Output *other_output;
		struct wlr_box this_output_box;
		struct wlr_box other_output_box;
		wlr_output_layout_get_box(server.output_layout, output->wlr, &this_output_box);
		
		wl_list_for_each(other_output, &server.output_list, link) {
			if (other_output == output) continue;
			View *other_view;
			other_view = &other_output->views[other_output->current_view];
			wlr_output_layout_get_box(server.output_layout, other_output->wlr, &other_output_box);
			
			double x_offset = other_output_box.x - this_output_box.x;
			double y_offset = other_output_box.y - this_output_box.y;
			
			for (int layer = LAYER_VIEW_FLOATING; layer <= LAYER_OUTPUT_OVERLAY; ++layer) {
				if (should_render_layer(other_view, layer) && layer != LAYER_VIEW_FULLSCREEN) {
					render_client_list(other_view->layers[layer], output, x_offset, y_offset, &now);
				}
			}
		}
	}
	
	for (int layer = LAYER_OUTPUT_STICKY; layer <= LAYER_OUTPUT_POPUPS; ++layer) {
		if (should_render_layer(view, layer)) {
			render_client_list(view->layers[layer], output, 0, 0, &now);
		}
	}
	
	// Only render status bar if there are no fullscreen clients
	if (wl_list_empty(view->layers[LAYER_VIEW_FULLSCREEN]))
		render_status_bar(output);
	
	for (int layer = LAYER_OUTPUT_TOP; layer < NUM_LAYERS; ++layer) {
		if (should_render_layer(view, layer)) {
			render_client_list(view->layers[layer], output, 0, 0, &now);
		}
	}
	
	wlr_renderer_end(server.renderer);
	wlr_output_commit(output->wlr);
}

/* ================================================================================
 * Backend
 * ================================================================================*/
static void handle_new_input(struct wl_listener *listener, void *data) {
	struct wlr_input_device *device = data;
	static uint32_t seat_caps = 0;
	
	printf("Connecting input device %s\n", device->name);
	
	if (device->type == WLR_INPUT_DEVICE_KEYBOARD) {
		Keyboard *keyboard = calloc(1, sizeof(Keyboard));
		keyboard->wlr = wlr_keyboard_from_input_device(device);
		struct xkb_context *xkb = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
		struct xkb_keymap *keymap = xkb_keymap_new_from_names(xkb, NULL, 0);
		wlr_keyboard_set_keymap(keyboard->wlr, keymap);
		xkb_keymap_unref(keymap);
		xkb_context_unref(xkb);
		
		wlr_seat_set_keyboard(server.seat, keyboard->wlr);
		wlr_keyboard_set_repeat_info(keyboard->wlr, 25, 500);
		
		listen(&keyboard->on_key, &handle_keyboard_key, &keyboard->wlr->events.key);
		listen(&keyboard->on_modifiers, &handle_keyboard_modifiers, &keyboard->wlr->events.modifiers);
		
		seat_caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	} else if (device->type == WLR_INPUT_DEVICE_POINTER) {
		wlr_cursor_attach_input_device(server.cursor, device);
		wlr_xcursor_manager_set_cursor_image(server.xcursor_manager, "left_ptr", server.cursor);
		seat_caps |= WL_SEAT_CAPABILITY_POINTER;
		
		if (wlr_input_device_is_libinput(device)) {
			int config_index = 0;
			for (int i = 1; i < ARRAY_LENGTH(POINTERS); ++i) {
				if (!strcmp(POINTERS[i].name, device->name)) {config_index = i; break;}
			}
			
			struct libinput_device *pointer = wlr_libinput_get_device_handle(device);
			libinput_device_config_accel_set_profile(pointer, POINTERS[config_index].accel_profile);
			libinput_device_config_accel_set_speed(pointer, POINTERS[config_index].accel_speed);
		}
	}
	
	wlr_seat_set_capabilities(server.seat, seat_caps);
}

void update_output_configuration() {
	struct wlr_output_configuration_v1 *output_config = wlr_output_configuration_v1_create();
	Output *output;
	wl_list_for_each(output, &server.output_list, link) {
		struct wlr_box output_box;
		wlr_output_layout_get_box(server.output_layout, output->wlr, &output_box);
		struct wlr_output_configuration_head_v1 *head = wlr_output_configuration_head_v1_create(output_config, output->wlr);
		head->state.enabled = true;
		head->state.mode = output->wlr->current_mode;
		head->state.x = output_box.x;
		head->state.y = output_box.y;
	}

	wlr_output_manager_v1_set_configuration(server.output_manager, output_config);
}

static void handle_new_output(struct wl_listener *listener, void *data) {
	struct wlr_output *wlr_output = data;
	Output *new_output = calloc(1, sizeof(Output));
	
	printf("Configuring output %s\n", wlr_output->name);
	
	new_output->wlr = wlr_output;
	new_output->bar_height = CONFIG.bar_height;
	wlr_output->data = new_output;
	wlr_output_init_render(wlr_output, server.allocator, server.renderer);
	
	// Initialize layers
	for (int i = 0; i < NUM_OUTPUT_LAYERS; ++i) {
		wl_list_init(&new_output->output_layers[i]);
	}
	
	// Initialize views
	for (int i = 0; i < 9; ++i) {
		View *view = &new_output->views[i];
		view->layer_show_mask = UINT32_MAX;
		view->layout = LAYOUT_DEFAULT;
		for (int j = 0; j < NUM_VIEW_LAYERS; ++j)
			wl_list_init(&view->view_layers[j]);
		
		view->layers[LAYER_OUTPUT_BACKGROUND] = &new_output->output_layers[OUTPUT_LAYER_BACKGROUND];
		view->layers[LAYER_OUTPUT_BOTTOM] = &new_output->output_layers[OUTPUT_LAYER_BOTTOM];
		view->layers[LAYER_VIEW_TILES] = &view->view_layers[VIEW_LAYER_TILES];
		view->layers[LAYER_VIEW_FLOATING] = &view->view_layers[VIEW_LAYER_FLOATING];
		view->layers[LAYER_VIEW_FULLSCREEN] = &view->view_layers[VIEW_LAYER_FULLSCREEN];
		view->layers[LAYER_OUTPUT_POPUPS] = &new_output->output_layers[OUTPUT_LAYER_POPUPS];
		view->layers[LAYER_OUTPUT_STICKY] = &new_output->output_layers[OUTPUT_LAYER_STICKY];
		view->layers[LAYER_OUTPUT_TOP] = &new_output->output_layers[OUTPUT_LAYER_TOP];
		view->layers[LAYER_OUTPUT_OVERLAY] = &new_output->output_layers[OUTPUT_LAYER_OVERLAY];
	}
	
	
	// Apply configuration and commit
	{
		bool found_config = false;
		
		for (int i = 0; i < ARRAY_LENGTH(OUTPUTS); ++i) {
			if (!strcmp(OUTPUTS[i].name, new_output->wlr->name)) {
				uint32_t width = OUTPUTS[i].width;
				uint32_t height = OUTPUTS[i].height;
				int refresh_mhz = OUTPUTS[i].refresh_rate * 1000;
				
				if (wl_list_empty(&new_output->wlr->modes)) {
					printf("No modes for output %s\n", new_output->wlr->name);
					break;
				}

				found_config = true;
				
				struct wlr_output_mode *mode;
				struct wlr_output_mode *closest_mode = NULL;
				int closest_refresh_diff = INT32_MAX;

				printf("Using transform: %d\n", OUTPUTS[i].transform);
				wlr_output_set_transform(new_output->wlr, OUTPUTS[i].transform);
				
				wl_list_for_each(mode, &new_output->wlr->modes, link) {
					if ((mode->width == width) && (mode->height == height)) {
						int diff = abs(mode->refresh - refresh_mhz);
						if (diff < closest_refresh_diff) {
							closest_refresh_diff = diff;
							closest_mode = mode;
						}
					}
				}
				
				if (!closest_mode) {
					printf("No matching mode for output %s\n", new_output->wlr->name);
					wlr_output_set_mode(new_output->wlr, wlr_output_preferred_mode(new_output->wlr));
				} else {
					printf("Using mode %ux%u@%gHz for %s\n", width, height, closest_mode->refresh / 1000.f, new_output->wlr->name);
					wlr_output_set_mode(new_output->wlr, closest_mode);
				}

				break;
			}
		}
		
		if (!found_config) wlr_output_set_mode(new_output->wlr, wlr_output_preferred_mode(new_output->wlr));
		
		wlr_output_commit(new_output->wlr);
	}

	
	wlr_output_layout_add_auto(server.output_layout, wlr_output);
	listen(&new_output->on_destroy, &handle_output_destroy, &wlr_output->events.destroy);
	listen(&new_output->on_frame, &handle_output_frame, &wlr_output->events.frame);
	wl_list_insert(&server.output_list, &new_output->link);
	
	if (!server.focused_output) server.focused_output = new_output;
	
	// Update configuration for wlr_output_manager_v1
	update_output_configuration();
}

int main(int argc, char **argv) {
	wlr_log_init(WLR_ERROR, NULL);
	wl_list_init(&server.output_list);
	wl_list_init(&server.client_update_list);

	log_file = fopen("log.dump", "w");
	
	server.display = wl_display_create();
	server.output_layout = wlr_output_layout_create();
	
	server.xdg_decoration_manager = wlr_xdg_decoration_manager_v1_create(server.display);
	server.xdg_output_manager = wlr_xdg_output_manager_v1_create(server.display, server.output_layout);
	
	server.backend = wlr_backend_autocreate(server.display);
	listen(&server.on_new_input, &handle_new_input, &server.backend->events.new_input);
	listen(&server.on_new_output, &handle_new_output, &server.backend->events.new_output);
	
	server.seat = wlr_seat_create(server.display, "seat0");
	listen(&server.on_request_set_cursor, &handle_seat_request_set_cursor, &server.seat->events.request_set_cursor);
	listen(&server.on_request_set_selection, &handle_seat_request_set_selection, &server.seat->events.request_set_selection);
	
	server.xdg_shell = wlr_xdg_shell_create(server.display, 3);
	listen(&server.on_new_xdg_surface, &handle_new_xdg_surface, &server.xdg_shell->events.new_surface);
	
	server.cursor = wlr_cursor_create();
	listen(&server.on_cursor_axis, &handle_cursor_axis, &server.cursor->events.axis);
	listen(&server.on_cursor_button, &handle_cursor_button, &server.cursor->events.button);
	listen(&server.on_cursor_frame, &handle_cursor_frame, &server.cursor->events.frame);
	listen(&server.on_cursor_motion, &handle_cursor_motion, &server.cursor->events.motion);
	listen(&server.on_cursor_motion_absolute, &handle_cursor_motion_absolute, &server.cursor->events.motion_absolute);
	wlr_cursor_attach_output_layout(server.cursor, server.output_layout);
	
	server.output_manager = wlr_output_manager_v1_create(server.display);
	listen(&server.on_output_manager_apply, &handle_output_manager_apply, &server.output_manager->events.apply);
	listen(&server.on_output_manager_test, &handle_output_manager_test, &server.output_manager->events.test);
	
	server.xcursor_manager = wlr_xcursor_manager_create(NULL, 24);
	wlr_xcursor_manager_load(server.xcursor_manager, 1);
	wlr_xcursor_manager_set_cursor_image(server.xcursor_manager, "left_ptr", server.cursor);
	
	server.layer_shell = wlr_layer_shell_v1_create(server.display);
	listen(&server.on_new_layer_surface, &handle_new_layer_surface, &server.layer_shell->events.new_surface);
	
	wlr_server_decoration_manager_set_default_mode(
												   wlr_server_decoration_manager_create(server.display),
												   WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
	
	server.pointer_constraints = wlr_pointer_constraints_v1_create(server.display);
	listen(&server.on_new_pointer_constraint, &handle_new_pointer_constraint, &server.pointer_constraints->events.new_constraint);
	
	server.relative_pointer_manager = wlr_relative_pointer_manager_v1_create(server.display);
	
	server.idle = wlr_idle_create(server.display);
	
	server.idle_notifier = wlr_idle_notifier_v1_create(server.display);
	
	server.idle_inhibit_manager = wlr_idle_inhibit_v1_create(server.display);
	
#if USE_VULKAN
	server.renderer = wlr_vk_renderer_create_with_drm_fd(wlr_backend_get_drm_fd(server.backend));
#else
	server.renderer = wlr_renderer_autocreate(server.backend);
#endif
	server.allocator = wlr_allocator_autocreate(server.backend, server.renderer);
	server.compositor = wlr_compositor_create(server.display, server.renderer);
	
#if USE_XWAYLAND
	server.xwayland = wlr_xwayland_create(server.display, server.compositor, false);
	listen(&server.on_new_xwayland_surface, &handle_new_xwayland_surface, &server.xwayland->events.new_surface);
	listen(&server.on_xwayland_ready, &setup_xwayland, &server.xwayland->events.ready);
#endif
	
	wlr_renderer_init_wl_display(server.renderer, server.display);
	wlr_subcompositor_create(server.display);
	wlr_data_device_manager_create(server.display);
	wlr_primary_selection_v1_device_manager_create(server.display);
	wlr_data_control_manager_v1_create(server.display);
	wlr_gamma_control_manager_v1_create(server.display);
	wlr_screencopy_manager_v1_create(server.display);
	wlr_single_pixel_buffer_manager_v1_create(server.display);
	wlr_idle_inhibit_v1_create(server.display);
	
	setup_font_rendering();
	for (int i = 0; i < ARRAY_LENGTH(FONTS); ++i) {
		load_font(FONTS[i].path, FONTS[i].px_size);
	}
	
	const char *socket = wl_display_add_socket_auto(server.display);
	printf("WAYLAND_DISPLAY=%s\n", socket);
	setenv("WAYLAND_DISPLAY", socket, 1);
	
	pthread_mutex_init(&status_bar.lock, NULL);
	pthread_create(&status_bar.update_thread, NULL, &status_update_thread, NULL);
	
	wlr_backend_start(server.backend);
	wl_display_run(server.display);
	wl_display_destroy_clients(server.display);
	wl_display_destroy(server.display);
	fclose(log_file);

	return 0;
}

