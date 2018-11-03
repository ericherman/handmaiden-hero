/* handmaiden.c
   Copyright (C) 2014, 2015, 2018 Eric Herman <eric@freesa.org>

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
/* handmaiden.c */
#include "handmaiden.h"

#include <math.h>		/* sin */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct game_context {
	struct pixel_buffer *virtual_win;

	unsigned char x_offset;
	unsigned char y_offset;
	int x_shift;
	int y_shift;

	unsigned int sound_volume;
	unsigned int sound_default_volume;
	unsigned int tone_hz;
};

void init_game(struct game_memory *mem, unsigned int default_volume,
	       unsigned int initial_volume)
{
	struct game_context *ctx;
	void *file_contents;
	unsigned int size;
	int error;

	ctx = (struct game_context *)mem->fixed_memory;

	HANDMAIDEN_ASSERT(mem->fixed_memory_size >=
			  (sizeof(struct game_context) +
			   sizeof(struct pixel_buffer) +
			   (800 * 600 * sizeof(unsigned int))));

	ctx->x_offset = 0;
	ctx->y_offset = 0;
	ctx->x_shift = 0;
	ctx->y_shift = 0;
	ctx->sound_default_volume = default_volume;
	ctx->sound_volume = initial_volume;
	ctx->tone_hz = 0;

	ctx->virtual_win =
	    (struct pixel_buffer *)(mem->fixed_memory +
				    sizeof(struct game_context));
	ctx->virtual_win->width = 800;
	ctx->virtual_win->height = 600;
	ctx->virtual_win->bytes_per_pixel = sizeof(unsigned int);
	ctx->virtual_win->pitch =
	    (ctx->virtual_win->width * ctx->virtual_win->bytes_per_pixel);
	ctx->virtual_win->pixels_bytes_len =
	    (ctx->virtual_win->width * ctx->virtual_win->height *
	     ctx->virtual_win->bytes_per_pixel);
	ctx->virtual_win->pixels =
	    (unsigned int *)(mem->fixed_memory + sizeof(struct game_context) +
			     sizeof(struct pixel_buffer));

	/* this just tests the debug IO code */
	file_contents = DEBUG_platform_read_entire_file(__FILE__, &size);
	if (file_contents) {
		error =
		    DEBUG_platform_write_entire_file("test.out", size,
						     file_contents);
		if (error) {
			/* TODO log? */
		}
		DEBUG_platform_free_file_memory(file_contents, size);
	}
}

void update_pixel_buffer(struct game_memory *mem,
			 struct pixel_buffer **virtual_win)
{
	struct game_context *ctx;
	struct pixel_buffer *pixel_buf;
	unsigned int x, y;
	unsigned char red, blue;
	unsigned int foreground;

	ctx = (struct game_context *)mem->fixed_memory;

	pixel_buf = ctx->virtual_win;
	for (y = 0; y < pixel_buf->height; y++) {
		for (x = 0; x < pixel_buf->width; x++) {
			/* unsigned char means % 256 */
			red = x + ctx->x_offset;
			blue = y + ctx->y_offset;
			foreground =
			    (((unsigned int)blue) << 16) + (unsigned int)red;
			*(pixel_buf->pixels + (y * pixel_buf->width) + x) =
			    foreground;
		}
	}
	*virtual_win = pixel_buf;
}

void update_audio_buf(struct game_memory *mem, struct audio_buffer *audio_buf)
{
	struct game_context *ctx;
	unsigned int i, tone_hz;
	int *left_sample, *right_sample;
	double sine;

	ctx = (struct game_context *)mem->fixed_memory;

	if ((ctx->tone_hz < 20) || (ctx->tone_hz >= 10000)) {
		/* reset tone_hz */
		ctx->tone_hz = 128;
	}
	tone_hz = ctx->tone_hz;
	tone_hz += 8 * ((ctx->x_shift < 0) ? -(ctx->x_shift) : ctx->x_shift);
	tone_hz += 8 * ((ctx->y_shift < 0) ? -(ctx->y_shift) : ctx->y_shift);

	for (i = 0; i < audio_buf->num_samples; i++) {
		left_sample = audio_buf->samples + (i * 2);
		right_sample = left_sample + 1;

		sine = sin((audio_buf->stream_pos + i) * 2 * M_PI / tone_hz);
		*left_sample = sine * ctx->sound_volume;
		*right_sample = sine * ctx->sound_volume;
	}
}

unsigned int input_as_string(struct human_input *input, char *buf,
			     unsigned int len)
{
	unsigned int i;

	i = 0;
	if (i < len) {
		buf[i++] = '[';
	}

	if (i < len) {
		buf[i++] = input->up.is_down ? '1' : '0';
	}
	if (i < len) {
		buf[i++] = input->up.was_down ? '1' : '0';
	}

	if (i < len) {
		buf[i++] = input->w.is_down ? '1' : '0';
	}
	if (i < len) {
		buf[i++] = input->w.was_down ? '1' : '0';
	}

	if (i < len) {
		buf[i++] = input->left.is_down ? '1' : '0';
	}
	if (i < len) {
		buf[i++] = input->left.was_down ? '1' : '0';
	}

	if (i < len) {
		buf[i++] = input->a.is_down ? '1' : '0';
	}
	if (i < len) {
		buf[i++] = input->a.was_down ? '1' : '0';
	}

	if (i < len) {
		buf[i++] = input->down.is_down ? '1' : '0';
	}
	if (i < len) {
		buf[i++] = input->down.was_down ? '1' : '0';
	}

	if (i < len) {
		buf[i++] = input->s.is_down ? '1' : '0';
	}
	if (i < len) {
		buf[i++] = input->s.was_down ? '1' : '0';
	}

	if (i < len) {
		buf[i++] = input->right.is_down ? '1' : '0';
	}
	if (i < len) {
		buf[i++] = input->right.was_down ? '1' : '0';
	}

	if (i < len) {
		buf[i++] = input->d.is_down ? '1' : '0';
	}
	if (i < len) {
		buf[i++] = input->d.was_down ? '1' : '0';
	}

	if (i < len) {
		buf[i++] = input->m.is_down ? '1' : '0';
	}
	if (i < len) {
		buf[i++] = input->m.was_down ? '1' : '0';
	}

	if (i < len) {
		buf[i++] = input->space.is_down ? '1' : '0';
	}
	if (i < len) {
		buf[i++] = input->space.was_down ? '1' : '0';
	}

	if (i < len) {
		buf[i++] = input->esc.is_down ? '1' : '0';
	}
	if (i < len) {
		buf[i++] = input->esc.was_down ? '1' : '0';
	}

	if (i < len) {
		buf[i++] = ']';
	}
	if (i < len) {
		buf[i++] = '\0';
	} else if (i != 0) {
		buf[i] = '\0';
	}

	return i;
}

int process_input(struct game_memory *mem, struct human_input *input)
{
	struct game_context *ctx;
	struct game_controller_input *input0;

	ctx = (struct game_context *)mem->fixed_memory;

	if (input->esc.is_down) {
		return 1;
	}

	input0 = &input->controllers[0];
	if (input0->is_analog) {
		ctx->x_shift += (int)(4.0f * input0->end_x);
		ctx->y_shift += (int)(4.0f * input0->end_y);
	}

	if (input->space.is_down) {
		ctx->x_shift = 0;
		ctx->y_shift = 0;
	}

	if ((input->w.is_down && !input->w.was_down) ||
	    (input->up.is_down && !input->up.was_down)) {
		ctx->y_shift++;
	}
	if ((input->s.is_down && !input->s.was_down) ||
	    (input->down.is_down && !input->down.was_down)) {
		ctx->y_shift--;
	}

	if ((input->a.is_down && !input->a.was_down) ||
	    (input->left.is_down && !input->left.was_down)) {
		ctx->x_shift++;
	}
	if ((input->d.is_down && !input->d.was_down) ||
	    (input->right.is_down && !input->right.was_down)) {
		ctx->x_shift--;
	}

	if ((input->m.is_down && !input->m.was_down)) {
		ctx->sound_volume =
		    (ctx->sound_volume) ? 0 : ctx->sound_default_volume;
	}

	ctx->x_offset += ctx->x_shift;
	ctx->y_offset += ctx->y_shift;

	return 0;
}
