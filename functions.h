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
#ifndef FUNCTIONS_H
#define FUNCTIONS_H

typedef struct {
	long int number;
	long int *numbers;
	double decimal;
	char *string;
	char **argv;
	void (*layout)(Output*);
} Input_Arg;

/* ================================================================================
 * Input functions
 * ================================================================================*/
static void close_client(Input_Arg arg);
static void close_server(Input_Arg arg);
static void cycle_client_layer(Input_Arg arg); // ARG_LAYERS
static void increment_view(Input_Arg arg); // ARG_NUMBER
static void move_client(Input_Arg arg);
static void move_resize_client(Input_Arg arg);
static void move_to_view(Input_Arg arg); // ARG_NUMBER
static void select_view(Input_Arg arg); // ARG_NUMBER
static void set_client_layer(Input_Arg arg); // ARG_NUMBER
static void set_layout(Input_Arg arg); // ARG_LAYOUT
static void spawn(Input_Arg arg); // ARG_COMMAND
static void toggle_fullscreen(Input_Arg arg);
static void toggle_layer(Input_Arg arg); // ARG_NUMBER

/* ================================================================================
 * Layouts
 * ================================================================================*/
static void layout_recursive(Output *output);

#endif
