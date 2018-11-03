/* sdl_handmaiden.c
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
/* sdl_handmaiden.c */
#ifndef FPS_PRINTER
#define FPS_PRINTER 0
#endif

#ifndef HANDMAIDEN_TRY_TO_MAKE_VALGRIND_HAPPY
#define HANDMAIDEN_TRY_TO_MAKE_VALGRIND_HAPPY 0
#endif

#include "handmaiden.h"

#ifndef ERIC_DEBUG
#define ERIC_DEBUG 0
#endif
#ifndef DEBUG_LOG_AUDIO
#define DEBUG_LOG_AUDIO 0
#endif

/* audio config constants */
#define HANDMAIDEN_AUDIO_DEFAULT_VOLUME 1
/* #define HANDMAIDEN_AUDIO_START_VOLUME 0 */
#define HANDMAIDEN_AUDIO_START_VOLUME HANDMAIDEN_AUDIO_DEFAULT_VOLUME
#define HANDMAIDEN_AUDIO_BUF_CHUNKS 4
#define HANDMAIDEN_AUDIO_BUF_SIZE (HANDMAIDEN_AUDIO_BUF_CHUNKS * 65536)
#define HANDMAIDEN_AUDIO_SAMPLES_PER_SECOND 48000
#define HANDMAIDEN_AUDIO_CHANNELS 2
#define HANDMAIDEN_AUDIO_BYTES_PER_SAMPLE sizeof(unsigned int)

#define internal static
/*
#define global_variable static
#define local_persistant static
*/

/* memory allocation by mmap requires _GNU_SOURCE since it is linux specific */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/* clock_gettime in time.h requires _POSIX_C_SOURCE 199309 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309
#endif

#include <sys/mman.h>
/* end mmap includes */
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
/* end open/close includes */

/* the Simple Directmedia Library requires the following includes */
#include <SDL.h>
/* end SDL */

#include <time.h>		/* clock_gettime */

#define FIRST_SUPPORTING -1
#define NONE 0
#define DEBUG_AUDIO_BIN_LOG_PATH "/tmp/debug_audio_log"
#define DEBUG_AUDIO_TXT_LOG_PATH DEBUG_AUDIO_BIN_LOG_PATH ".txt"

/* watch out for side-effects of X or Y */
#define min(X, Y)  ((X) < (Y) ? (X) : (Y))

internal int debug(int debug_level, const char *fmt, ...);

/* services the platform layer provides to the game */
void *DEBUG_platform_read_entire_file(char *filename, unsigned int *size)
{
	int fd;
	struct stat fst;
	void *mem;

	fd = open(filename, O_RDONLY);
	if (fd == -1) {
		debug(0, "open(\"%s\", O_RDONLY) == -1\n", filename);
		return 0;
	}

	if (fstat(fd, &fst) == -1) {
		debug(0, "fstat(fd, &fst) == -1\n");
		return 0;
	}

	if (!fst.st_size) {
		debug(0, "%s size is 0\n", filename);
		mem = 0;
	} else {
		*size = (unsigned int)fst.st_size;
		mem =
		    mmap(0, fst.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE,
			 fd, 0);
	}

	/* fd can not actually be -1 here, we would have returned above */
	if (fd != -1) {
		close(fd);
	}

	if (!mem) {
		debug(0, "mem is NULL\n");
	} else if (mem == MAP_FAILED) {
		debug(0, "mem == MAP_FAILED\n");
	} else {
		debug(2, "%s", (char *)mem);
	}

	return mem;
}

void DEBUG_platform_free_file_memory(void *mem, unsigned int size)
{
	if (!mem) {
		return;
	}

	munmap(mem, size);
}

int DEBUG_platform_write_entire_file(char *filename, unsigned int size,
				     void *mem)
{
	int fd;
	void *out;

	fd = open(filename, O_RDWR | O_CREAT | O_TRUNC,
		  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	if (fd == -1) {
		debug(0, "open %s failed\n", filename);
		return 1;
	}

	if (!size) {
		debug(1, "nothing to do\n");
	} else if (lseek(fd, size - 1, SEEK_SET) == -1) {
		debug(0, "lseek 1\n");
	} else if (write(fd, "", 1) != 1) {
		debug(0, "write byte\n");
	} else if (lseek(fd, 0, SEEK_SET) == -1) {
		debug(0, "lseek 2\n");
	}

	out = mmap(0, size, PROT_WRITE, MAP_FILE | MAP_SHARED, fd, 0);

	if (!out) {
		debug(0, "out is NULL\n");
	} else if (out == MAP_FAILED) {
		debug(0, "out == MAP_FAILED\n");
	} else if (mem) {
		memcpy(out, mem, size);
	} else {
		debug(0, "no mem to copy.\n");
	}

	/* fd can not actually be -1 here, we would have returned above */
	if (fd != -1) {
		close(fd);
	}

	return 0;
}

internal void *platform_alloc(unsigned int len)
{
	return mmap(0, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
		    -1, 0);
}

internal int platform_free(void *addr, unsigned int len)
{
	return munmap(addr, len);
}

internal const char *debug_audio_bin_log_path()
{
	return DEBUG_AUDIO_BIN_LOG_PATH;
}

internal const char *debug_audio_txt_log_path()
{
	return DEBUG_AUDIO_TXT_LOG_PATH;
}

struct sdl_texture_buffer {
	SDL_Texture *texture;
	struct pixel_buffer *pixel_buf;
};

struct sdl_joystick {
	SDL_GameController *controller;
	SDL_Haptic *rumble;
};

struct sdl_event_context {
	struct sdl_texture_buffer *texture_buf;
	SDL_Window *window;
	Uint32 win_id;
	SDL_Renderer *renderer;
	SDL_Event *event;
	struct sdl_joystick players[MAX_CONTROLLERS];
};

struct audio_ring_buffer {
	unsigned int *buf;
	unsigned int buf_len;
	unsigned int write_cursor;
	unsigned int play_cursor;
	void *log;
};

internal int debug(int debug_level, const char *fmt, ...)
{
	int bytes;
	va_list args;

	if (ERIC_DEBUG < debug_level) {
		return 0;
	}

	va_start(args, fmt);
	bytes = vfprintf(stderr, fmt, args);
	va_end(args);
	return bytes;
}

internal void stretch_buffer(struct pixel_buffer *from_buf,
			     struct pixel_buffer *to_buf)
{
	unsigned int x, y, from_x, from_y;
	float x_ratio, y_ratio;
	unsigned int foreground;
	unsigned int from_pos, to_pos;

	y_ratio = from_buf->height / (float)to_buf->height;
	x_ratio = from_buf->width / (float)to_buf->width;

	for (y = 0; y < to_buf->height; y++) {
		from_y = (int)(y * y_ratio);
		for (x = 0; x < to_buf->width; x++) {
			from_x = (int)(x * x_ratio);
			from_pos = (from_y * from_buf->width) + from_x;
			to_pos = (to_buf->width * y) + x;
			foreground = *(from_buf->pixels + from_pos);
			*(to_buf->pixels + to_pos) = foreground;
		}
	}
}

internal void *resize_pixel_buffer(struct pixel_buffer *buf, int height,
				   int width)
{
	debug(1, "Buffer was %d x %d, need %d x %d\n",
	      buf->width, buf->height, width, height);
	if (buf->pixels) {
		platform_free(buf->pixels, buf->pixels_bytes_len);
	}
	buf->width = width;
	buf->height = height;
	buf->pixels_bytes_len = buf->height * buf->width * buf->bytes_per_pixel;
	buf->pitch = buf->width * buf->bytes_per_pixel;
	buf->pixels = platform_alloc(buf->pixels_bytes_len);
	if (!buf->pixels) {
		debug(0, "Could not alloc buf->pixels (%d)\n",
		      buf->pixels_bytes_len);
	}
	return buf->pixels;
}

internal void pixel_buffer_init(struct pixel_buffer *buf)
{
	buf->width = 0;
	buf->height = 0;
	buf->bytes_per_pixel = sizeof(unsigned int);
	buf->pixels = NULL;
	buf->pixels_bytes_len = 0;
	buf->pitch = 0;
}

internal void sdl_resize_texture_buf(SDL_Window *window,
				     SDL_Renderer *renderer,
				     struct sdl_texture_buffer *texture_buf)
{
	int height, width;
	struct pixel_buffer *pixel_buf;

	pixel_buf = texture_buf->pixel_buf;
	SDL_GetWindowSize(window, &width, &height);
	if ((width == (int)pixel_buf->width)
	    && (height == (int)pixel_buf->height)) {
		/* nothing to do */
		return;
	}
	debug(1, "Texture was %d x %d, need %d x %d\n",
	      pixel_buf->width, pixel_buf->height, width, height);
	if (texture_buf->texture) {
		SDL_DestroyTexture(texture_buf->texture);
	}
	texture_buf->texture = SDL_CreateTexture(renderer,
						 SDL_PIXELFORMAT_ARGB8888,
						 SDL_TEXTUREACCESS_STREAMING,
						 width, height);
	if (!texture_buf->texture) {
		debug(0, "Could not alloc texture_buf->texture\n");
		exit(EXIT_FAILURE);
	}

	if (!resize_pixel_buffer(pixel_buf, height, width)) {
		debug(0, "Could not resize pixel_buffer\n");
		exit(EXIT_FAILURE);
	}
}

internal void sdl_blit_bytes(SDL_Renderer *renderer, SDL_Texture *texture,
			     const SDL_Rect *rect, const void *pixels,
			     int pitch)
{
	if (SDL_UpdateTexture(texture, rect, pixels, pitch)) {
		debug(0, "Could not SDL_UpdateTexture\n");
		exit(EXIT_FAILURE);
	}
	SDL_RenderCopy(renderer, texture, 0, 0);
	SDL_RenderPresent(renderer);
}

internal void sdl_blit_texture(SDL_Renderer *renderer,
			       struct sdl_texture_buffer *texture_buf)
{
	SDL_Texture *texture = texture_buf->texture;
	const SDL_Rect *rect = 0;
	const void *pixels = texture_buf->pixel_buf->pixels;
	int pitch = texture_buf->pixel_buf->pitch;

	sdl_blit_bytes(renderer, texture, rect, pixels, pitch);
}

internal void process_key_event(struct sdl_event_context *event_ctx,
				struct human_input *input)
{
	int is_down, was_down;

	is_down = event_ctx->event->key.state == SDL_PRESSED;
	was_down = ((event_ctx->event->key.repeat != 0)
		    || (event_ctx->event->key.state == SDL_RELEASED));

	switch (event_ctx->event->key.keysym.scancode) {
	case SDL_SCANCODE_ESCAPE:
		input->esc.is_down = is_down;
		input->esc.was_down = was_down;
		break;
	case SDL_SCANCODE_SPACE:
		input->space.is_down = is_down;
		input->space.was_down = was_down;
		break;
	case SDL_SCANCODE_UP:
		input->up.is_down = is_down;
		input->up.was_down = was_down;
		break;
	case SDL_SCANCODE_LEFT:
		input->left.is_down = is_down;
		input->left.was_down = was_down;
		break;
	case SDL_SCANCODE_DOWN:
		input->down.is_down = is_down;
		input->down.was_down = was_down;
		break;
	case SDL_SCANCODE_RIGHT:
		input->right.is_down = is_down;
		input->right.was_down = was_down;
		break;
	case SDL_SCANCODE_A:
		input->a.is_down = is_down;
		input->a.was_down = was_down;
		break;
	case SDL_SCANCODE_D:
		input->d.is_down = is_down;
		input->d.was_down = was_down;
		break;
	case SDL_SCANCODE_M:
		input->m.is_down = is_down;
		input->m.was_down = was_down;
		break;
	case SDL_SCANCODE_S:
		input->s.is_down = is_down;
		input->s.was_down = was_down;
		break;
	case SDL_SCANCODE_W:
		input->w.is_down = is_down;
		input->w.was_down = was_down;
		break;
	default:
		break;
	};
}

internal int process_event(struct sdl_event_context *event_ctx,
			   struct human_input *input)
{
	switch (event_ctx->event->type) {
	case SDL_QUIT:
		return 1;
		break;
	case SDL_KEYUP:
	case SDL_KEYDOWN:
		process_key_event(event_ctx, input);
		break;
	case SDL_WINDOWEVENT:
		if (event_ctx->event->window.windowID != event_ctx->win_id) {
			/* not our event? */
			break;
		}
		switch (event_ctx->event->window.event) {
		case SDL_WINDOWEVENT_NONE:
			/* (docs say never used) */
			break;
		case SDL_WINDOWEVENT_SHOWN:
			break;
		case SDL_WINDOWEVENT_HIDDEN:
			break;
		case SDL_WINDOWEVENT_EXPOSED:
			/* window should be redrawn */
			break;
		case SDL_WINDOWEVENT_MOVED:
			/* window moved to data1, data2 */
			break;
		case SDL_WINDOWEVENT_RESIZED:
			/* window resized to data1 x data2 */
			/* always preceded by */
			/* SDL_WINDOWEVENT_SIZE_CHANGED */
			sdl_resize_texture_buf(event_ctx->window,
					       event_ctx->renderer,
					       event_ctx->texture_buf);
			break;
		case SDL_WINDOWEVENT_SIZE_CHANGED:
			/* either as a result of an API call */
			/* or through the system
			   or user action */
			/* this event is followed by */
			/* SDL_WINDOWEVENT_RESIZED
			   if the size was */
			/* changed by an external event,
			   (user or the window manager) */
			break;
		case SDL_WINDOWEVENT_MINIMIZED:
			break;
		case SDL_WINDOWEVENT_MAXIMIZED:
			break;
		case SDL_WINDOWEVENT_RESTORED:
			break;
		case SDL_WINDOWEVENT_ENTER:
			/* window has gained mouse focus */
			break;
		case SDL_WINDOWEVENT_LEAVE:
			/* window has lost mouse focus */
			break;
		case SDL_WINDOWEVENT_FOCUS_GAINED:
			/* window has gained keyboard focus */
			break;
		case SDL_WINDOWEVENT_FOCUS_LOST:
			/* window has lost keyboard focus */
			break;
		case SDL_WINDOWEVENT_CLOSE:
			/* the window manager requests close */
			event_ctx->event->type = SDL_QUIT;
			SDL_PushEvent(event_ctx->event);
			break;
		default:
			/* (how did we get here? */
			break;
		}
		break;
	default:
		break;
	}
	return 0;
}

internal void copy_audio(struct audio_ring_buffer *audio_ring_buf,
			 unsigned char *stream, int len)
{
	int region_1_bytes, region_2_bytes;

	if (audio_ring_buf->write_cursor <= audio_ring_buf->play_cursor) {
		debug(0, "callback: we do not have any new sound data\n");
		return;
	}
	if ((audio_ring_buf->write_cursor - audio_ring_buf->play_cursor) <
	    (unsigned long)len) {
		len =
		    (audio_ring_buf->write_cursor -
		     audio_ring_buf->play_cursor);
		debug(0, "callback: we only have %d much new sound available\n",
		      len);
	}
	if (len == 0) {
		debug(0, "callback: len==%d, why bother?\n", len);
		return;
	}
	debug(1, "callback: len==%d\n", len);

	region_1_bytes = len;
	region_2_bytes = 0;
	if ((audio_ring_buf->play_cursor % audio_ring_buf->buf_len) + len >
	    audio_ring_buf->buf_len) {
		region_1_bytes =
		    audio_ring_buf->buf_len -
		    (audio_ring_buf->play_cursor % audio_ring_buf->buf_len);
		region_2_bytes = len - region_1_bytes;
	}

	if (region_1_bytes) {
		memcpy(stream,
		       ((unsigned char *)(audio_ring_buf->buf)) +
		       (audio_ring_buf->play_cursor % audio_ring_buf->buf_len),
		       region_1_bytes);
		if (audio_ring_buf->log) {
			fwrite(((unsigned char *)(audio_ring_buf->buf)) +
			       (audio_ring_buf->play_cursor %
				audio_ring_buf->buf_len), 1,
			       region_1_bytes, (FILE *) audio_ring_buf->log);
		}
		audio_ring_buf->play_cursor += region_1_bytes;
	}

	if (region_2_bytes) {
		memcpy(&stream[region_1_bytes], audio_ring_buf->buf,
		       region_2_bytes);
		if (audio_ring_buf->log) {
			fwrite(audio_ring_buf->buf, 1,
			       region_2_bytes, (FILE *) audio_ring_buf->log);
		}
		debug(1, "callback region_2_bytes: %u\n", region_2_bytes);
		audio_ring_buf->play_cursor += region_2_bytes;
	}
}

internal unsigned int init_audio_ring_buffer(struct audio_ring_buffer
					     *audio_ring_buf)
{
	int samples_per_buffer;
	double seconds_per_buffer;

	audio_ring_buf->write_cursor = 0;
	audio_ring_buf->play_cursor = 0;
	audio_ring_buf->buf_len = HANDMAIDEN_AUDIO_BUF_SIZE;

	samples_per_buffer =
	    HANDMAIDEN_AUDIO_BUF_SIZE / (HANDMAIDEN_AUDIO_BYTES_PER_SAMPLE *
					 HANDMAIDEN_AUDIO_CHANNELS);

	seconds_per_buffer =
	    (1.0 * (double)samples_per_buffer) /
	    (1.0 * (double)HANDMAIDEN_AUDIO_SAMPLES_PER_SECOND);

	debug(1, "sound buffer = %d bytes (%d samples) (%f seconds)\n",
	      HANDMAIDEN_AUDIO_BUF_SIZE, samples_per_buffer,
	      seconds_per_buffer);

	audio_ring_buf->buf = platform_alloc(audio_ring_buf->buf_len);

	if (!audio_ring_buf->buf) {
		debug(0, "could not allocate sound buffer\n");
	}

	if (DEBUG_LOG_AUDIO) {
		audio_ring_buf->log = fopen(debug_audio_bin_log_path(), "w");
		if (!audio_ring_buf->log) {
			debug(0, "could not fopen %s\n",
			      debug_audio_bin_log_path());
		}
	} else {
		audio_ring_buf->log = NULL;
	}

	return audio_ring_buf->buf ? 1 : 0;
}

internal long fill_ring_buf(struct audio_ring_buffer *audio_ring_buf,
			    struct game_memory *mem,
			    struct audio_buffer *audio_buf)
{
	unsigned int bytes_per_sample_all_chans,
	    bytes_to_write, region_1_bytes, region_2_bytes, sample_count, i,
	    buf_pos, starting_buf_pos, left_sample_value, right_sample_value;
	unsigned long sample_pos;

	if (!audio_buf->samples) {
		debug(0, "no audio_buf->samples\n");
		return -1;
	}

	bytes_per_sample_all_chans =
	    HANDMAIDEN_AUDIO_BYTES_PER_SAMPLE * HANDMAIDEN_AUDIO_CHANNELS;

	if ((audio_ring_buf->write_cursor - audio_ring_buf->play_cursor) >=
	    audio_ring_buf->buf_len) {
		debug(1, "fill: no room to write more sound data\n");
		return 0;
	}

	if (((audio_ring_buf->write_cursor % audio_ring_buf->buf_len) >
	     (audio_ring_buf->play_cursor % audio_ring_buf->buf_len))
	    || (audio_ring_buf->write_cursor == audio_ring_buf->play_cursor)) {
		bytes_to_write =
		    audio_ring_buf->buf_len - (audio_ring_buf->write_cursor -
					       audio_ring_buf->play_cursor);
	} else {
		bytes_to_write =
		    (audio_ring_buf->play_cursor % audio_ring_buf->buf_len) -
		    (audio_ring_buf->write_cursor % audio_ring_buf->buf_len);
	}

	if (bytes_to_write > audio_buf->buf_len) {
		debug(0, "bytes_to_write > audio_buf->buf_len (%u > %u)\n",
		      bytes_to_write, audio_buf->buf_len);
		bytes_to_write = audio_buf->buf_len;
	}

	debug(2, "bytes_to_write: %u\n", bytes_to_write);

	audio_buf->stream_pos =
	    (audio_ring_buf->write_cursor / bytes_per_sample_all_chans);
	audio_buf->num_samples = bytes_to_write / bytes_per_sample_all_chans;

	update_audio_buf(mem, audio_buf);

	if ((audio_ring_buf->play_cursor % audio_ring_buf->buf_len) >=
	    (audio_ring_buf->write_cursor % audio_ring_buf->buf_len)) {
		region_1_bytes = bytes_to_write;
		region_2_bytes = 0;
		if ((audio_ring_buf->play_cursor !=
		     audio_ring_buf->write_cursor)
		    && (region_1_bytes >
			((audio_ring_buf->play_cursor %
			  audio_ring_buf->buf_len) -
			 (audio_ring_buf->write_cursor %
			  audio_ring_buf->buf_len)))) {
			debug(0, "region 1 over-writing play_cursor\n");
			return -1;
		}
	} else {
		region_1_bytes =
		    min(bytes_to_write,
			(audio_ring_buf->buf_len -
			 ((audio_ring_buf->play_cursor %
			   audio_ring_buf->buf_len))));
		region_2_bytes = bytes_to_write - region_1_bytes;
		if (region_2_bytes >
		    ((audio_ring_buf->play_cursor % audio_ring_buf->buf_len))) {
			debug(0, "region 2 over-writing play_cursor\n");
			return -1;
		}
	}

	debug(1,
	      "fill: (in)  play_cursor: %u (%u) write_cursor: %u (%u) bytes to write: %u\n",
	      audio_ring_buf->play_cursor % audio_ring_buf->buf_len,
	      audio_ring_buf->play_cursor,
	      audio_ring_buf->write_cursor % audio_ring_buf->buf_len,
	      audio_ring_buf->write_cursor, bytes_to_write);

	debug(1, "fill: region_1_bytes: %u region_2_bytes: %u\n",
	      region_1_bytes, region_2_bytes);

	if (audio_ring_buf->write_cursor % HANDMAIDEN_AUDIO_BYTES_PER_SAMPLE) {
		debug(0, "fill: unexpected audio position\n");
	}

	buf_pos = audio_ring_buf->write_cursor % audio_ring_buf->buf_len;
	starting_buf_pos = buf_pos;
	sample_count = region_1_bytes / bytes_per_sample_all_chans;
	for (i = 0; i < sample_count; i++) {
		sample_pos = (i * 2);
		left_sample_value = *(audio_buf->samples + sample_pos);
		right_sample_value = *(audio_buf->samples + sample_pos + 1);

		*(int *)(((unsigned char *)audio_ring_buf->buf) + buf_pos) =
		    left_sample_value;
		buf_pos += HANDMAIDEN_AUDIO_BYTES_PER_SAMPLE;
		*(int *)(((unsigned char *)audio_ring_buf->buf) + buf_pos) =
		    right_sample_value;
		buf_pos += HANDMAIDEN_AUDIO_BYTES_PER_SAMPLE;
	}
	debug(1, "fill: region 1 wrote %u bytes (%u samples)\n",
	      (buf_pos - starting_buf_pos), sample_count);
	audio_ring_buf->write_cursor += (buf_pos - starting_buf_pos);
	debug(1, "fill: write cursor moved to %u (%u)\n",
	      audio_ring_buf->write_cursor,
	      audio_ring_buf->write_cursor % audio_ring_buf->buf_len);

	sample_count = region_2_bytes / bytes_per_sample_all_chans;
	buf_pos = 0;
	starting_buf_pos = buf_pos;
	for (i = 0; i < sample_count; i++) {
		sample_pos = (i * 2);
		left_sample_value = *(audio_buf->samples + sample_pos);
		right_sample_value = *(audio_buf->samples + sample_pos + 1);

		*(int *)(((unsigned char *)audio_ring_buf->buf) + buf_pos) =
		    left_sample_value;
		buf_pos += HANDMAIDEN_AUDIO_BYTES_PER_SAMPLE;
		*(int *)(((unsigned char *)audio_ring_buf->buf) + buf_pos) =
		    right_sample_value;
		buf_pos += HANDMAIDEN_AUDIO_BYTES_PER_SAMPLE;
	}
	debug(1, "fill: region 2 wrote %u bytes (%u samples)\n",
	      (buf_pos - starting_buf_pos), sample_count);
	audio_ring_buf->write_cursor += (buf_pos - starting_buf_pos);
	debug(1, "fill: write cursor moved to %u (%u)\n",
	      audio_ring_buf->write_cursor,
	      audio_ring_buf->write_cursor % audio_ring_buf->buf_len);
	debug(1,
	      "fill: (out) play_cursor: %u (%u) write_cursor: %u (%u) bytes avail: %u\n",
	      audio_ring_buf->play_cursor % audio_ring_buf->buf_len,
	      audio_ring_buf->play_cursor,
	      audio_ring_buf->write_cursor % audio_ring_buf->buf_len,
	      audio_ring_buf->write_cursor,
	      audio_ring_buf->write_cursor - audio_ring_buf->play_cursor);

	if ((audio_ring_buf->write_cursor - audio_ring_buf->play_cursor) >
	    audio_ring_buf->buf_len) {
		debug(0,
		      "write_cursor: %u, play_cursor: %u diff: %d, bufsize: %u\n",
		      audio_ring_buf->write_cursor, audio_ring_buf->play_cursor,
		      (audio_ring_buf->write_cursor -
		       audio_ring_buf->play_cursor), audio_ring_buf->buf_len);
		return -1;
	}
	debug(1, "fill: done.\n");
	return (long)(region_1_bytes + region_2_bytes);
}

void sdl_audio_callback(void *userdata, unsigned char *stream, int len)
{
	copy_audio((struct audio_ring_buffer *)userdata, stream, len);
}

internal SDL_AudioDeviceID sdl_init_audio(struct audio_ring_buffer
					  *audio_ring_buf)
{
	int audio_allow_change;
	SDL_AudioSpec want, have;
	SDL_AudioDeviceID sdl_audio_dev;
	double seconds_per_sample;

	SDL_zero(want);
	want.freq = HANDMAIDEN_AUDIO_SAMPLES_PER_SECOND;
	want.format = AUDIO_F32;
	want.channels = HANDMAIDEN_AUDIO_CHANNELS;
	/*
	   samples specifies a unit of audio data.

	   When used with SDL_OpenAudioDevice() this refers to the size of the
	   audio buffer in sample frames.

	   A sample frame is a chunk of audio data of the size specified in
	   format multiplied by the number of channels.

	   When the SDL_AudioSpec is used with SDL_LoadWAV() samples is set to
	   4096.

	   This field's value must be a power of two.
	 */
	want.samples =
	    HANDMAIDEN_AUDIO_BUF_SIZE / (HANDMAIDEN_AUDIO_BUF_CHUNKS *
					 HANDMAIDEN_AUDIO_CHANNELS *
					 HANDMAIDEN_AUDIO_BYTES_PER_SAMPLE);

	want.callback = &sdl_audio_callback;
	want.userdata = audio_ring_buf;
	audio_allow_change =
	    SDL_AUDIO_ALLOW_FORMAT_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE;

	sdl_audio_dev =
	    SDL_OpenAudioDevice(NULL, 0, &want, &have, audio_allow_change);
	if (sdl_audio_dev == 0) {
		debug(0, "Failed to open audio: %s\n", SDL_GetError());
	}
	if (have.format != want.format) {
		printf("Expected Float32 (%x) audio format, got %x\n",
		       want.format, have.format);
	}
	if (have.channels != want.channels) {
		printf("Expected %d audio channels, got %d.\n", want.channels,
		       have.channels);
	}
	if (have.samples != want.samples) {
		printf("Expected %d audio samples, got %d.\n", want.samples,
		       have.samples);
	}

	seconds_per_sample =
	    (1.0 * (double)have.samples) /
	    (1.0 * (double)HANDMAIDEN_AUDIO_SAMPLES_PER_SECOND);

	debug(1, "freq: %d channels: %d samples: %d (%.3f seconds)\n",
	      have.freq, have.channels, have.samples, seconds_per_sample);

	return sdl_audio_dev;
}

internal void close_audio_debug_logging(struct audio_ring_buffer
					*audio_ring_buf)
{
	FILE *audio_log;
	unsigned int i, items_read, sample_num;
	int buf[4096];

	if (!audio_ring_buf->log) {
		return;
	}

	fclose((FILE *) audio_ring_buf->log);
	audio_ring_buf->log = fopen(debug_audio_bin_log_path(), "r");

	if (!audio_ring_buf->log) {
		debug(0, "can not re-open %s?\n", debug_audio_bin_log_path());
		return;
	}
	audio_log = fopen(debug_audio_txt_log_path(), "w");
	if (!audio_log) {
		debug(0, "Can not open %s?\n", debug_audio_txt_log_path());
		return;
	}
	sample_num = 0;
	while ((items_read = fread(&buf, 4, 4096, audio_ring_buf->log))) {
		for (i = 0; i < items_read; i++) {
			fprintf(audio_log, "%u, %d\n", ++sample_num, buf[i]);
		}
	}
	if (audio_ring_buf->log) {
		fclose((FILE *) audio_ring_buf->log);
	}
	if (audio_log) {
		fclose(audio_log);
	}
}

internal void diff_timespecs(struct timespec start, struct timespec end,
			     struct timespec *elapsed)
{
	if ((end.tv_nsec - start.tv_nsec) < 0) {
		elapsed->tv_sec = end.tv_sec - start.tv_sec - 1;
		elapsed->tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
	} else {
		elapsed->tv_sec = end.tv_sec - start.tv_sec;
		elapsed->tv_nsec = end.tv_nsec - start.tv_nsec;
	}
}

void init_input(struct human_input *input)
{
	input->up.is_down = 0;
	input->up.was_down = 0;

	input->w.is_down = 0;
	input->w.was_down = 0;

	input->left.is_down = 0;
	input->left.was_down = 0;

	input->a.is_down = 0;
	input->a.was_down = 0;

	input->down.is_down = 0;
	input->down.was_down = 0;

	input->s.is_down = 0;
	input->s.was_down = 0;

	input->right.is_down = 0;
	input->right.was_down = 0;

	input->d.is_down = 0;
	input->d.was_down = 0;

	input->m.is_down = 0;
	input->m.was_down = 0;

	input->space.is_down = 0;
	input->space.was_down = 0;

	input->esc.is_down = 0;
	input->esc.was_down = 0;

	/* TODO: init controller input */
}

internal void sdl_init_joysticks(struct sdl_event_context *ctx)
{
	unsigned int player;
	int num_joy_sticks, jsi;
	SDL_GameController *controller;
	SDL_Joystick *joystick;
	SDL_Haptic *rumble;

	for (player = 0; player < MAX_CONTROLLERS; ++player) {
		ctx->players[player].controller = NULL;
		ctx->players[player].rumble = NULL;
	}

	num_joy_sticks = SDL_NumJoysticks();
	debug(0, "SDL_NumJoysticks() == %d\n", num_joy_sticks);
	player = 0;
	for (jsi = 0; jsi < num_joy_sticks && player < MAX_CONTROLLERS; ++jsi) {
		if (!SDL_IsGameController(jsi)) {
			debug(0, "SDL_IsGameController(%d) == 0\n", jsi);
			continue;
		}
		controller = SDL_GameControllerOpen(jsi);
		ctx->players[player].controller = controller;
		debug(0, "ctx->players[%u].controller = %p\n", player,
		      (void *)controller);

		joystick = SDL_GameControllerGetJoystick(controller);
		rumble = SDL_HapticOpenFromJoystick(joystick);
		if (SDL_HapticRumbleInit(rumble)) {
			ctx->players[player].rumble = rumble;
		} else {
			SDL_HapticClose(rumble);
		}

		++player;
	}
}

internal void sdl_close_joysticks(struct sdl_event_context *ctx)
{
	unsigned int i;

	for (i = 0; i < MAX_CONTROLLERS; ++i) {
		if (ctx->players[i].controller) {
			if (ctx->players[i].rumble) {
				SDL_HapticClose(ctx->players[i].rumble);
			}
			SDL_GameControllerClose(ctx->players[i].controller);
		}
	}
}

internal void process_button(const struct game_button_state *old_state,
			     struct game_button_state *new_state,
			     SDL_GameController * controller,
			     SDL_GameControllerButton button)
{
	new_state->ended_down = SDL_GameControllerGetButton(controller, button);
	if (new_state->ended_down == old_state->ended_down) {
		++new_state->half_transition_count;
	}
}

internal void process_joysticks(struct sdl_event_context *ctx,
				struct human_input *old_input,
				struct human_input *new_input)
{
	unsigned int i;
	struct game_controller_input *old_controller_input,
	    *new_controller_input;
	int stick_x, stick_y;
	/* DPAD
	   unsigned int up, down, left, right, start, back;
	 */

	for (i = 0; i < MAX_CONTROLLERS; ++i) {
		if (!(ctx->players[i].controller)) {
			continue;
		}
		if (!SDL_GameControllerGetAttached(ctx->players[i].controller)) {
			continue;
		}
		old_controller_input = &old_input->controllers[i];
		new_controller_input = &new_input->controllers[i];

		new_controller_input->is_analog = 1;
		/*
		   up = SDL_GameControllerGetButton(ctx->players[i].controller,
		   SDL_CONTROLLER_BUTTON_DPAD_UP);
		   down = SDL_GameControllerGetButton(ctx->players[i].controller,
		   SDL_CONTROLLER_BUTTON_DPAD_DOWN);
		   left = SDL_GameControllerGetButton(ctx->players[i].controller,
		   SDL_CONTROLLER_BUTTON_DPAD_LEFT);
		   right = SDL_GameControllerGetButton(ctx->players[i].controller,
		   SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
		   start = SDL_GameControllerGetButton(ctx->players[i].controller,
		   SDL_CONTROLLER_BUTTON_START);
		   back = SDL_GameControllerGetButton(ctx->players[i].controller,
		   SDL_CONTROLLER_BUTTON_BACK);
		 */

		process_button(&
			       (old_controller_input->but_u.
				six_buttons.l_shoulder),
			       &(new_controller_input->but_u.
				 six_buttons.l_shoulder),
			       ctx->players[i].controller,
			       SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
		process_button(&
			       (old_controller_input->but_u.
				six_buttons.r_shoulder),
			       &(new_controller_input->but_u.
				 six_buttons.r_shoulder),
			       ctx->players[i].controller,
			       SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
		process_button(&(old_controller_input->but_u.six_buttons.down),
			       &(new_controller_input->but_u.six_buttons.down),
			       ctx->players[i].controller,
			       SDL_CONTROLLER_BUTTON_A);
		process_button(&(old_controller_input->but_u.six_buttons.right),
			       &(new_controller_input->but_u.six_buttons.right),
			       ctx->players[i].controller,
			       SDL_CONTROLLER_BUTTON_B);
		process_button(&(old_controller_input->but_u.six_buttons.left),
			       &(new_controller_input->but_u.six_buttons.left),
			       ctx->players[i].controller,
			       SDL_CONTROLLER_BUTTON_X);
		process_button(&(old_controller_input->but_u.six_buttons.up),
			       &(new_controller_input->but_u.six_buttons.up),
			       ctx->players[i].controller,
			       SDL_CONTROLLER_BUTTON_Y);

		stick_x =
		    SDL_GameControllerGetAxis(ctx->players[i].controller,
					      SDL_CONTROLLER_AXIS_LEFTX);
		stick_y =
		    SDL_GameControllerGetAxis(ctx->players[i].controller,
					      SDL_CONTROLLER_AXIS_LEFTY);

		if (stick_x < 0) {
			new_controller_input->end_x = stick_x / -32768.0f;
		} else {
			new_controller_input->end_x = stick_x / -32767.0f;
		}
		new_controller_input->min_x = new_controller_input->max_x =
		    new_controller_input->end_x;

		if (stick_y < 0) {
			new_controller_input->end_y = stick_y / -32768.0f;
		} else {
			new_controller_input->end_y = stick_y / -32767.0f;
		}
		new_controller_input->min_y = new_controller_input->max_y =
		    new_controller_input->end_y;
	}

}

int main(int argc, char *argv[])
{
	char *title;
	SDL_Window *window;
	Uint32 flags;
	int x, y, width, height, shutdown;
	SDL_Renderer *renderer;
	SDL_Event event;
	SDL_AudioDeviceID sdl_audio_dev;
	struct timespec start, last_print, now, elapsed;
	unsigned long total_elapsed_seconds;
	double fps;
	unsigned long int frame_count, frames_since_print;
	struct game_memory mem;
	struct pixel_buffer *virtual_win;
	struct pixel_buffer blit_buf;
	struct human_input input[2];
	struct human_input *tmp_input;
	struct human_input *new_input;
	struct human_input *old_input;
	struct sdl_texture_buffer texture_buf;
	struct sdl_event_context event_ctx;
	struct audio_ring_buffer audio_ring_buf;
	struct audio_buffer audio_buf;

	pixel_buffer_init(&blit_buf);

	texture_buf.texture = NULL;
	texture_buf.pixel_buf = &blit_buf;

	flags =
	    SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER |
	    SDL_INIT_HAPTIC;
	if (SDL_Init(flags) != 0) {
		return 1;
	}

	height = 480;
	width = 640;

	mem.is_initialized = 0;
	mem.fixed_memory_size = 4 * 1024 * 1024;
	mem.transient_memory_size = 1 * 1024 * 1024;
	mem.fixed_memory =
	    platform_alloc(mem.fixed_memory_size + mem.transient_memory_size);
	if (mem.fixed_memory == 0) {
		debug(0, "did not alloc game memory\n");
		return 1;
	}

	mem.transient_memory = mem.fixed_memory + mem.fixed_memory_size;

	init_game(&mem, HANDMAIDEN_AUDIO_DEFAULT_VOLUME,
		  HANDMAIDEN_AUDIO_START_VOLUME);

	virtual_win = NULL;
	update_pixel_buffer(&mem, &virtual_win);
	if (!virtual_win) {
		debug(0, "did not assign virtual_win\n");
	}

	sdl_init_joysticks(&event_ctx);

	title = (argc > 1) ? argv[1] : "Handmaiden Hero";
	x = SDL_WINDOWPOS_UNDEFINED;
	y = SDL_WINDOWPOS_UNDEFINED;
	flags = SDL_WINDOW_RESIZABLE;
	window = SDL_CreateWindow(title, x, y, width, height, flags);
	if (!window) {
		debug(0, "Could not SDL_CreateWindow\n");
		return 2;
	}

	renderer = SDL_CreateRenderer(window, FIRST_SUPPORTING, NONE);
	if (!renderer) {
		return 3;
	}

	sdl_resize_texture_buf(window, renderer, &texture_buf);

	if (init_audio_ring_buffer(&audio_ring_buf)) {
		sdl_audio_dev = sdl_init_audio(&audio_ring_buf);
	} else {
		sdl_audio_dev = 0;
	}
	if (sdl_audio_dev && audio_ring_buf.buf_len) {
		audio_buf.stream_pos = 0;
		audio_buf.num_samples = 0;
		audio_buf.samples =
		    (int *)platform_alloc(audio_ring_buf.buf_len);
		if (!audio_buf.samples) {
			debug(0, "could not allocate audio_buf.samples[%u]\n",
			      audio_ring_buf.buf_len);
			audio_buf.buf_len = 0;
		} else {
			audio_buf.buf_len = audio_ring_buf.buf_len;
		}
	}

	/* start audio playing by setting "pause" to zero */
	if (sdl_audio_dev && audio_buf.samples) {
		SDL_PauseAudioDevice(sdl_audio_dev, 0);
	}

	event_ctx.event = &event;
	event_ctx.texture_buf = &texture_buf;
	event_ctx.renderer = renderer;
	event_ctx.window = window;
	event_ctx.win_id = SDL_GetWindowID(window);

	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);
	last_print.tv_sec = start.tv_sec;
	last_print.tv_nsec = start.tv_nsec;
	total_elapsed_seconds = 0;
	frames_since_print = 0;
	frame_count = 0;
	shutdown = 0;
	old_input = &input[0];
	new_input = &input[1];
	init_input(old_input);
	init_input(new_input);
	while (!shutdown) {
		tmp_input = new_input;
		new_input = old_input;
		old_input = tmp_input;
		init_input(new_input);

		while (SDL_PollEvent(&event)) {
			if ((shutdown = process_event(&event_ctx, new_input))) {
				break;
			}
		}
		process_joysticks(&event_ctx, old_input, new_input);

		if (shutdown || (shutdown = process_input(&mem, new_input))) {
			break;
		}
		update_pixel_buffer(&mem, &virtual_win);
		stretch_buffer(virtual_win, &blit_buf);
		sdl_blit_texture(renderer, &texture_buf);
		if (sdl_audio_dev && audio_buf.samples) {
			if ((audio_ring_buf.write_cursor -
			     audio_ring_buf.play_cursor) <
			    audio_ring_buf.buf_len) {
				SDL_LockAudio();
				if (fill_ring_buf
				    (&audio_ring_buf, &mem, &audio_buf) < 0) {
					shutdown = 1;
				}
				SDL_UnlockAudio();
			}
		}
		frame_count++;
		frames_since_print++;

		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &now);
		diff_timespecs(last_print, now, &elapsed);
		if (((elapsed.tv_sec * 1000000000) + elapsed.tv_nsec) >
		    100000000) {
			fps =
			    ((double)frames_since_print) /
			    (((double)elapsed.tv_sec) * 1000000000.0 +
			     ((double)elapsed.tv_nsec) / 1000000000.0);
			frames_since_print = 0;
			last_print.tv_sec = now.tv_sec;
			last_print.tv_nsec = now.tv_nsec;
			diff_timespecs(start, now, &elapsed);
			total_elapsed_seconds = elapsed.tv_sec + 1;
			if (FPS_PRINTER) {
				debug(0, "fps: %.02f (avg fps: %.f) %u, %u%s",
				      fps,
				      (double)frame_count /
				      (double)total_elapsed_seconds,
				      audio_ring_buf.play_cursor,
				      audio_ring_buf.write_cursor,
				      ERIC_DEBUG ? "\n" : "\r");
			}
		}
	}
	debug(0, "\n");

	if (sdl_audio_dev) {
		SDL_CloseAudioDevice(sdl_audio_dev);
	}
	close_audio_debug_logging(&audio_ring_buf);
	sdl_close_joysticks(&event_ctx);

	/* we probably do not need to do these next steps */
	if (HANDMAIDEN_TRY_TO_MAKE_VALGRIND_HAPPY) {
		/* first cleanup SDL stuff */
		if (texture_buf.texture) {
			SDL_DestroyTexture(texture_buf.texture);
		}
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		SDL_Quit();

		/* then collect our own garbage */
		if (blit_buf.pixels) {
			platform_free(blit_buf.pixels,
				      blit_buf.pixels_bytes_len);
		}
		if (audio_ring_buf.buf) {
			platform_free(audio_ring_buf.buf,
				      audio_ring_buf.buf_len);
		}
		if (audio_buf.samples) {
			platform_free(audio_buf.samples, audio_buf.buf_len);
		}
		if (mem.fixed_memory) {
			platform_free(mem.fixed_memory,
				      mem.fixed_memory_size +
				      mem.transient_memory_size);
		}
	}

	return 0;
}
