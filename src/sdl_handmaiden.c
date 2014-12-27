#define FPS_PRINTER 0
#define HANDMAIDEN_TRY_TO_MAKE_VALGRIND_HAPPY 0

#include "handmaiden.c"

#define ERIC_DEBUG 0
#define DEBUG_LOG_AUDIO 0

/* audio config constants */
#define HANDMAIDEN_AUDIO_START_VOLUME 16
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
#define _GNU_SOURCE
/* clock_gettime in time.h requires _POSIX_C_SOURCE 199309 */
#define _POSIX_C_SOURCE 199309

#include <sys/mman.h>
/* end mmap includes */

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

#include <math.h>		/* sin */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

struct sdl_event_context {
	struct sdl_texture_buffer *texture_buf;
	SDL_Window *window;
	Uint32 win_id;
	SDL_Renderer *renderer;
	SDL_Event *event;
};

struct audio_context {
	unsigned int *sound_buffer;
	unsigned int sound_buffer_bytes;
	unsigned int write_cursor;
	unsigned int play_cursor;
	unsigned int volume;
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
	int x, y, from_x, from_y;
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
	if (width == pixel_buf->width && height == pixel_buf->height) {
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
				struct audio_context *audio_ctx,
				struct game_context *ctx)
{
	int was_down = ((event_ctx->event->key.repeat != 0)
			|| (event_ctx->event->key.state == SDL_RELEASED));

	switch (event_ctx->event->key.keysym.scancode) {
	case SDL_SCANCODE_ESCAPE:
		event_ctx->event->type = SDL_QUIT;
		SDL_PushEvent(event_ctx->event);
		if (was_down) {
			debug(0, "got escape again again?");
		}
		break;

	case SDL_SCANCODE_UP:
	case SDL_SCANCODE_W:
		ctx->y_shift++;
		break;
	case SDL_SCANCODE_LEFT:
	case SDL_SCANCODE_A:
		ctx->x_shift++;
		break;
	case SDL_SCANCODE_DOWN:
	case SDL_SCANCODE_S:
		ctx->y_shift--;
		break;
	case SDL_SCANCODE_RIGHT:
	case SDL_SCANCODE_D:
		ctx->x_shift--;
		break;
	case SDL_SCANCODE_SPACE:
		ctx->x_shift = 0;
		ctx->y_shift = 0;
		break;
	case SDL_SCANCODE_M:
		if (!was_down) {
			audio_ctx->volume =
			    (audio_ctx->volume) ?
			    0 : HANDMAIDEN_AUDIO_START_VOLUME;
		}
		break;
	default:
		break;
	};
}

internal int process_event(struct sdl_event_context *event_ctx,
			   struct audio_context *audio_ctx,
			   struct game_context *ctx)
{
	switch (event_ctx->event->type) {
	case SDL_QUIT:
		return 1;
		break;
	case SDL_KEYUP:
	case SDL_KEYDOWN:
		process_key_event(event_ctx, audio_ctx, ctx);
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

internal void copy_audio(struct audio_context *audio_ctx, unsigned char *stream,
			 int len)
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

internal unsigned int init_audio_context(struct audio_context *audio_ctx)
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

internal long fill_sound_buffer(struct audio_context *audio_ctx,
				struct game_context *ctx)
{
	unsigned int tone_hz, tone_volume, bytes_per_sample_all_chans,
	    bytes_to_write, region_1_bytes, region_2_bytes, sample_count, i,
	    buf_pos, starting_buf_pos, sample_value;
	unsigned long stream_pos;
	double sine;

	tone_hz = 128;
	tone_hz += 8 * ((ctx->x_shift < 0) ? -(ctx->x_shift) : ctx->x_shift);
	tone_hz += 8 * ((ctx->y_shift < 0) ? -(ctx->y_shift) : ctx->y_shift);
	tone_volume = audio_ctx->volume;
	bytes_per_sample_all_chans =
	    HANDMAIDEN_AUDIO_BYTES_PER_SAMPLE * HANDMAIDEN_AUDIO_CHANNELS;

	if ((audio_ctx->write_cursor - audio_ctx->play_cursor) >=
	    audio_ctx->sound_buffer_bytes) {
		debug(1, "fill: no room to write more sound data\n");
		return 0;
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
			return -1;
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
			return -1;
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
		return -1;
	}
	debug(1, "fill: done.\n");
	return (long)(region_1_bytes + region_2_bytes);
}

void sdl_audio_callback(void *userdata, unsigned char *stream, int len)
{
	copy_audio((struct audio_context *)userdata, stream, len);
}

internal SDL_AudioDeviceID sdl_init_audio(struct audio_context *audio_ctx)
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
	want.userdata = audio_ctx;
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

internal void close_audio_debug_logging(struct audio_context *audio_ctx)
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
	struct game_context ctx;
	struct pixel_buffer virtual_win;
	struct pixel_buffer blit_buf;
	struct sdl_texture_buffer texture_buf;
	struct sdl_event_context event_ctx;
	struct audio_context audio_ctx;

	pixel_buffer_init(&blit_buf);
	pixel_buffer_init(&virtual_win);

	texture_buf.texture = NULL;
	texture_buf.pixel_buf = &blit_buf;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
		return 1;
	}

	height = 480;
	width = 640;
	if (!resize_pixel_buffer(&virtual_win, height, width)) {
		debug(0, "Could not resize pixel_buffer\n");
		exit(EXIT_FAILURE);
	}

	ctx.virtual_win = &virtual_win;
	ctx.x_offset = 0;
	ctx.y_offset = 0;
	ctx.x_shift = 0;
	ctx.y_shift = 0;
	update_pixel_buffer(&ctx);

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

	if (init_audio_context(&audio_ctx)) {
		sdl_audio_dev = sdl_init_audio(&audio_ctx);
	} else {
		sdl_audio_dev = 0;
	}
	/* start audio playing by setting "pause" to zero */
	if (sdl_audio_dev) {
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
	while (!shutdown) {
		while (SDL_PollEvent(&event)) {
			shutdown = process_event(&event_ctx, &audio_ctx, &ctx);
		}
		ctx.x_offset += ctx.x_shift;
		ctx.y_offset += ctx.y_shift;
		update_pixel_buffer(&ctx);
		stretch_buffer(&virtual_win, &blit_buf);
		sdl_blit_texture(renderer, &texture_buf);
		if ((audio_ctx.write_cursor - audio_ctx.play_cursor) <
		    audio_ctx.sound_buffer_bytes) {
			SDL_LockAudio();
			if (fill_sound_buffer(&audio_ctx, &ctx) < 0) {
				shutdown = 1;
			}
			SDL_UnlockAudio();
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
				      audio_ctx.play_cursor,
				      audio_ctx.write_cursor,
				      ERIC_DEBUG ? "\n" : "\r");
			}
		}
	}
	debug(0, "\n");

	if (sdl_audio_dev) {
		SDL_CloseAudioDevice(sdl_audio_dev);
	}
	close_audio_debug_logging(&audio_ctx);

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
		munmap(virtual_win.pixels, virtual_win.pixels_bytes_len);
		if (blit_buf.pixels) {
			munmap(blit_buf.pixels, blit_buf.pixels_bytes_len);
		}
		if (audio_ctx.sound_buffer) {
			munmap(audio_ctx.sound_buffer,
			       audio_ctx.sound_buffer_bytes);
		}
	}

	return 0;
}
