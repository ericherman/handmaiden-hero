
#include "handmaiden.h"

#include <string.h>		/* memcpy */
#include <stdio.h>		/* fprintf */
#include <stdarg.h>		/* va_start */
#include <stdlib.h>		/* exit */
#include <math.h>		/* sin */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* watch out for side-effects of X or Y */
#define min(X, Y)  ((X) < (Y) ? (X) : (Y))

int debug(int debug_level, const char *fmt, ...)
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

void close_audio_debug_logging(struct audio_context *audio_ctx)
{
	FILE *audio_log;
	unsigned int i, items_read, sample_num;
	int buf[4096];

	if (!audio_ctx->log) {
		return;
	}

	fclose((FILE *) audio_ctx->log);
	audio_ctx->log = fopen(debug_audio_bin_log_path(), "r");

	if (!audio_ctx->log) {
		debug(0, "can not re-open %s?\n", debug_audio_bin_log_path());
		return;
	}
	audio_log = fopen(debug_audio_txt_log_path(), "w");
	if (!audio_log) {
		debug(0, "Can not open %s?\n", debug_audio_txt_log_path());
		return;
	}
	sample_num = 0;
	while ((items_read = fread(&buf, 4, 4096, audio_ctx->log))) {
		for (i = 0; i < items_read; i++) {
			fprintf(audio_log, "%u, %d\n", ++sample_num, buf[i]);
		}
	}
	if (audio_ctx->log) {
		fclose((FILE *) audio_ctx->log);
	}
	if (audio_log) {
		fclose(audio_log);
	}
}

void fill_virtual(struct game_context *ctx)
{
	struct pixel_buffer *virtual_win = ctx->virtual_win;
	int x, y;
	unsigned char red, blue;
	unsigned int foreground;

	for (y = 0; y < virtual_win->height; y++) {
		for (x = 0; x < virtual_win->width; x++) {
			/* unsigned char means % 256 */
			red = x + ctx->x_offset;
			blue = y + ctx->y_offset;
			foreground =
			    (((unsigned int)blue) << 16) + (unsigned int)red;
			*(virtual_win->pixels + (y * virtual_win->width) + x) =
			    foreground;
		}
	}
}

void resize_pixel_buffer(struct pixel_buffer *buf, int height, int width)
{
	debug(0, "Buffer was %d x %d, need %d x %d\n",
	      buf->width, buf->height, width, height);
	if (buf->pixels) {
		platform_free(buf->pixels, buf->pixels_bytes_len);
	}
	buf->width = width;
	buf->height = height;
	buf->pixels_bytes_len = buf->height * buf->width * buf->bytes_per_pixel;
	buf->pixels = platform_alloc(buf->pixels_bytes_len);
	if (!buf->pixels) {
		debug(0, "Could not alloc buf->pixels (%d)\n",
		      buf->pixels_bytes_len);
		exit(EXIT_FAILURE);
	}
	buf->pitch = buf->width * buf->bytes_per_pixel;
}

void fill_blit_buf_from_virtual(struct pixel_buffer *blit_buf,
				struct pixel_buffer *virtual_win)
{
	int x, y, virt_x, virt_y;
	float x_ratio, y_ratio;
	unsigned int foreground;
	unsigned int virt_pos, off_pos;

	y_ratio = virtual_win->height / (float)blit_buf->height;
	x_ratio = virtual_win->width / (float)blit_buf->width;

	for (y = 0; y < blit_buf->height; y++) {
		virt_y = (int)(y * y_ratio);
		for (x = 0; x < blit_buf->width; x++) {
			virt_x = (int)(x * x_ratio);
			virt_pos = (virt_y * virtual_win->width) + virt_x;
			off_pos = (blit_buf->width * y) + x;
			foreground = *(virtual_win->pixels + virt_pos);
			*(blit_buf->pixels + off_pos) = foreground;
		}
	}
}

void pixel_buffer_init(struct pixel_buffer *buf)
{
	buf->width = 0;
	buf->height = 0;
	buf->bytes_per_pixel = sizeof(unsigned int);
	buf->pixels = NULL;
	buf->pixels_bytes_len = 0;
	buf->pitch = 0;
}

void copy_audio(struct audio_context *audio_ctx, unsigned char *stream, int len)
{
	int region_1_bytes, region_2_bytes;

	if (audio_ctx->write_cursor <= audio_ctx->play_cursor) {
		debug(0, "callback: we do not have any new sound data\n");
		return;
	}
	if ((audio_ctx->write_cursor - audio_ctx->play_cursor) <
	    (unsigned long)len) {
		len = (audio_ctx->write_cursor - audio_ctx->play_cursor);
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
	if ((audio_ctx->play_cursor % audio_ctx->sound_buffer_bytes) + len >
	    audio_ctx->sound_buffer_bytes) {
		region_1_bytes =
		    audio_ctx->sound_buffer_bytes -
		    (audio_ctx->play_cursor % audio_ctx->sound_buffer_bytes);
		region_2_bytes = len - region_1_bytes;
	}

	if (region_1_bytes) {
		memcpy(stream,
		       ((unsigned char *)(audio_ctx->sound_buffer)) +
		       (audio_ctx->play_cursor % audio_ctx->sound_buffer_bytes),
		       region_1_bytes);
		if (audio_ctx->log) {
			fwrite(((unsigned char *)(audio_ctx->sound_buffer)) +
			       (audio_ctx->play_cursor %
				audio_ctx->sound_buffer_bytes), 1,
			       region_1_bytes, (FILE *) audio_ctx->log);
		}
		audio_ctx->play_cursor += region_1_bytes;
	}

	if (region_2_bytes) {
		memcpy(&stream[region_1_bytes], audio_ctx->sound_buffer,
		       region_2_bytes);
		if (audio_ctx->log) {
			fwrite(audio_ctx->sound_buffer, 1,
			       region_2_bytes, (FILE *) audio_ctx->log);
		}
		debug(1, "callback region_2_bytes: %u\n", region_2_bytes);
		audio_ctx->play_cursor += region_2_bytes;
	}
}

unsigned int init_audio_context(struct audio_context *audio_ctx)
{
	int samples_per_buffer;
	double seconds_per_buffer;

	audio_ctx->volume = HANDMAIDEN_AUDIO_START_VOLUME;
	audio_ctx->write_cursor = 0;
	audio_ctx->play_cursor = 0;
	audio_ctx->sound_buffer_bytes = HANDMAIDEN_AUDIO_BUF_SIZE;

	samples_per_buffer =
	    HANDMAIDEN_AUDIO_BUF_SIZE / (HANDMAIDEN_AUDIO_BYTES_PER_SAMPLE *
					 HANDMAIDEN_AUDIO_CHANNELS);

	seconds_per_buffer =
	    (1.0 * (double)samples_per_buffer) /
	    (1.0 * (double)HANDMAIDEN_AUDIO_SAMPLES_PER_SECOND);

	debug(1, "sound buffer = %d bytes (%d samples) (%f seconds)\n",
	      HANDMAIDEN_AUDIO_BUF_SIZE, samples_per_buffer,
	      seconds_per_buffer);

	audio_ctx->sound_buffer = platform_alloc(audio_ctx->sound_buffer_bytes);

	if (!audio_ctx->sound_buffer) {
		debug(0, "could not allocate sound buffer\n");
	}

	if (DEBUG_LOG_AUDIO) {
		audio_ctx->log = fopen(debug_audio_bin_log_path(), "w");
		if (!audio_ctx->log) {
			debug(0, "could not fopen %s\n",
			      debug_audio_bin_log_path());
		}
	} else {
		audio_ctx->log = NULL;
	}

	return audio_ctx->sound_buffer ? 1 : 0;
}

void fill_sound_buffer(struct game_context *ctx)
{
	unsigned int tone_hz, tone_volume, bytes_per_sample_all_chans,
	    bytes_to_write, region_1_bytes, region_2_bytes, sample_count, i,
	    buf_pos, starting_buf_pos, sample_value;
	unsigned long stream_pos;
	struct audio_context *audio_ctx;
	double sine;

	audio_ctx = ctx->audio_ctx;

	tone_hz = 128;
	tone_hz += 8 * ((ctx->x_shift < 0) ? -(ctx->x_shift) : ctx->x_shift);
	tone_hz += 8 * ((ctx->y_shift < 0) ? -(ctx->y_shift) : ctx->y_shift);
	tone_volume = audio_ctx->volume;
	bytes_per_sample_all_chans =
	    HANDMAIDEN_AUDIO_BYTES_PER_SAMPLE * HANDMAIDEN_AUDIO_CHANNELS;

	if ((audio_ctx->write_cursor - audio_ctx->play_cursor) >=
	    audio_ctx->sound_buffer_bytes) {
		debug(1, "fill: no room to write more sound data\n");
		return;
	}

	if (((audio_ctx->write_cursor % audio_ctx->sound_buffer_bytes) >
	     (audio_ctx->play_cursor % audio_ctx->sound_buffer_bytes))
	    || (audio_ctx->write_cursor == audio_ctx->play_cursor)) {
		bytes_to_write =
		    audio_ctx->sound_buffer_bytes - (audio_ctx->write_cursor -
						     audio_ctx->play_cursor);
	} else {
		bytes_to_write =
		    (audio_ctx->play_cursor % audio_ctx->sound_buffer_bytes) -
		    (audio_ctx->write_cursor % audio_ctx->sound_buffer_bytes);
	}

	if ((audio_ctx->play_cursor % audio_ctx->sound_buffer_bytes) >=
	    (audio_ctx->write_cursor % audio_ctx->sound_buffer_bytes)) {
		region_1_bytes = bytes_to_write;
		region_2_bytes = 0;
		if ((audio_ctx->play_cursor != audio_ctx->write_cursor)
		    && (region_1_bytes >
			((audio_ctx->play_cursor %
			  audio_ctx->sound_buffer_bytes) -
			 (audio_ctx->write_cursor %
			  audio_ctx->sound_buffer_bytes)))) {
			debug(0, "region 1 over-writing play_cursor\n");
			exit(EXIT_FAILURE);
		}
	} else {
		region_1_bytes =
		    min(bytes_to_write,
			(audio_ctx->sound_buffer_bytes -
			 ((audio_ctx->play_cursor %
			   audio_ctx->sound_buffer_bytes))));
		region_2_bytes = bytes_to_write - region_1_bytes;
		if (region_2_bytes >
		    ((audio_ctx->play_cursor %
		      audio_ctx->sound_buffer_bytes))) {
			debug(0, "region 2 over-writing play_cursor\n");
			exit(EXIT_FAILURE);
		}
	}

	debug(1,
	      "fill: (in)  play_cursor: %u (%u) write_cursor: %u (%u) bytes to write: %u\n",
	      audio_ctx->play_cursor % audio_ctx->sound_buffer_bytes,
	      audio_ctx->play_cursor,
	      audio_ctx->write_cursor % audio_ctx->sound_buffer_bytes,
	      audio_ctx->write_cursor, bytes_to_write);

	debug(1, "fill: region_1_bytes: %u region_2_bytes: %u\n",
	      region_1_bytes, region_2_bytes);

	if (audio_ctx->write_cursor % HANDMAIDEN_AUDIO_BYTES_PER_SAMPLE) {
		debug(0, "fill: unexpected audio position\n");
	}

	buf_pos = audio_ctx->write_cursor % audio_ctx->sound_buffer_bytes;
	starting_buf_pos = buf_pos;
	sample_count = region_1_bytes / bytes_per_sample_all_chans;
	for (i = 0; i < sample_count; i++) {
		stream_pos =
		    i + (audio_ctx->write_cursor / bytes_per_sample_all_chans);
		sine = sin(stream_pos * 2 * M_PI / tone_hz);

		sample_value = sine * tone_volume;

		*(int *)(((unsigned char *)audio_ctx->sound_buffer) + buf_pos) =
		    sample_value;
		buf_pos += HANDMAIDEN_AUDIO_BYTES_PER_SAMPLE;
		*(int *)(((unsigned char *)audio_ctx->sound_buffer) + buf_pos) =
		    sample_value;
		buf_pos += HANDMAIDEN_AUDIO_BYTES_PER_SAMPLE;
	}
	debug(1, "fill: region 1 wrote %u bytes (%u samples)\n",
	      (buf_pos - starting_buf_pos), sample_count);
	audio_ctx->write_cursor += (buf_pos - starting_buf_pos);
	debug(1, "fill: write cursor moved to %u (%u)\n",
	      audio_ctx->write_cursor,
	      audio_ctx->write_cursor % audio_ctx->sound_buffer_bytes);

	sample_count = region_2_bytes / bytes_per_sample_all_chans;
	buf_pos = 0;
	starting_buf_pos = buf_pos;
	for (i = 0; i < sample_count; i++) {
		stream_pos =
		    i + (audio_ctx->write_cursor / bytes_per_sample_all_chans);
		sine = sin(stream_pos * 2 * M_PI / tone_hz);

		sample_value = sine * tone_volume;

		*(unsigned int *)(((unsigned char *)audio_ctx->sound_buffer) +
				  buf_pos) = sample_value;
		buf_pos += HANDMAIDEN_AUDIO_BYTES_PER_SAMPLE;
		*(unsigned int *)(((unsigned char *)audio_ctx->sound_buffer) +
				  buf_pos) = sample_value;
		buf_pos += HANDMAIDEN_AUDIO_BYTES_PER_SAMPLE;
	}
	debug(1, "fill: region 2 wrote %u bytes (%u samples)\n",
	      (buf_pos - starting_buf_pos), sample_count);
	audio_ctx->write_cursor += (buf_pos - starting_buf_pos);
	debug(1, "fill: write cursor moved to %u (%u)\n",
	      audio_ctx->write_cursor,
	      audio_ctx->write_cursor % audio_ctx->sound_buffer_bytes);
	debug(1,
	      "fill: (out) play_cursor: %u (%u) write_cursor: %u (%u) bytes avail: %u\n",
	      audio_ctx->play_cursor % audio_ctx->sound_buffer_bytes,
	      audio_ctx->play_cursor,
	      audio_ctx->write_cursor % audio_ctx->sound_buffer_bytes,
	      audio_ctx->write_cursor,
	      audio_ctx->write_cursor - audio_ctx->play_cursor);

	if ((audio_ctx->write_cursor - audio_ctx->play_cursor) >
	    audio_ctx->sound_buffer_bytes) {
		debug(0,
		      "write_cursor: %u, play_cursor: %u diff: %d, bufsize: %u\n",
		      audio_ctx->write_cursor, audio_ctx->play_cursor,
		      (audio_ctx->write_cursor - audio_ctx->play_cursor),
		      audio_ctx->sound_buffer_bytes);
		exit(EXIT_FAILURE);
	}
	debug(1, "fill: done.\n");
}
