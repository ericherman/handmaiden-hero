/* handmaiden.h
   Copyright (C) 2014, 2015, 2017, 2018 Eric Herman <eric@freesa.org>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

	https://www.gnu.org/licenses/gpl-3.0.txt
 */
#ifndef HANDMAIDEN_H
#define HANDMAIDEN_H

/* writing to address NULL will allways segfault */
#ifndef HANDMAIDEN_ASSERT
#define HANDMAIDEN_ASSERT(condition) if(!(condition)) *((char *)0) = 0;
#endif

struct pixel_buffer {
	unsigned int width;
	unsigned int height;
	unsigned char bytes_per_pixel;
	unsigned int pixels_bytes_len;
	/* pitch is bytes in a row of pixel data, including padding */
	unsigned int pitch;
	unsigned int *pixels;
};

struct audio_buffer {
	unsigned long stream_pos;
	unsigned int num_samples;
	int *samples;
	unsigned int buf_len;
};

struct keyboard_key {
	unsigned int is_down:1;
	unsigned int was_down:1;
};

struct game_button_state {
	int half_transition_count;
	unsigned int ended_down;
};

struct game_controller_input {
	unsigned int is_analog;

	float start_x;
	float start_y;

	float min_x;
	float min_y;

	float max_x;
	float max_y;

	float end_x;
	float end_y;

	union {
		struct game_button_state buttons[6];
		struct six_buttons {
			struct game_button_state up;
			struct game_button_state down;
			struct game_button_state left;
			struct game_button_state right;
			struct game_button_state l_shoulder;
			struct game_button_state r_shoulder;
		} six_buttons;
	} but_u;
};

#define MAX_CONTROLLERS 1
struct human_input {
	struct keyboard_key up;
	struct keyboard_key w;

	struct keyboard_key left;
	struct keyboard_key a;

	struct keyboard_key down;
	struct keyboard_key s;

	struct keyboard_key right;
	struct keyboard_key d;

	struct keyboard_key m;
	struct keyboard_key space;
	struct keyboard_key esc;

	struct game_controller_input controllers[MAX_CONTROLLERS];
};

struct game_memory {
	unsigned int fixed_memory_size;
	unsigned char *fixed_memory;

	unsigned int transient_memory_size;
	unsigned char *transient_memory;

	unsigned int is_initialized:1;
};

/* services the game provides to the platform layer */
void init_game(struct game_memory *mem, unsigned int default_volume,
	       unsigned int initial_volume);

int process_input(struct game_memory *mem, struct human_input *input);

void update_pixel_buffer(struct game_memory *mem,
			 struct pixel_buffer **virtual_win);

void update_audio_buf(struct game_memory *mem, struct audio_buffer *audio_buf);

/* services the platform layer provides to the game */
void *DEBUG_platform_read_entire_file(char *filename, unsigned int *size);
void DEBUG_platform_free_file_memory(void *mem, unsigned int size);
int DEBUG_platform_write_entire_file(char *filename, unsigned int size,
				     void *mem);

#endif /* HANDMAIDEN_H */
