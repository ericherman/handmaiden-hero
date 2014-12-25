#define ERIC_DEBUG 0
#define HANDMAIDEN_TRY_TO_MAKE_VALGRIND_HAPPY ERIC_DEBUG

/* memory allocation by mmap requires _GNU_SOURCE since it is linux specific */
#define _GNU_SOURCE
/* clock_gettime in time.h requires _POSIX_C_SOURCE 199309 */
#define _POSIX_C_SOURCE 199309
#include <sys/mman.h>
/* end mmap includes */

/* the Simple Directmedia Library requires the following includes */
#include <SDL.h>
/* end SDL */

#include <stdio.h>		/* fprintf */
#include <time.h>		/* clock_gettime */

#define FIRST_SUPPORTING -1
#define NONE 0

#define internal static
/*
#define global_variable static
#define local_persistant static
*/

/* audio config constants */
#define HANDMAIDEN_AUDIO_SAMPLES_PER_SECOND 48000
#define HANDMAIDEN_AUDIO_CHANNELS 2
#define HANDMAIDEN_AUDIO_BUF_SAMPLES_SIZE 16536
#define HANDMAIDEN_AUDIO_BYTES_PER_SAMPLE sizeof(unsigned int)

struct pixel_buffer {
	int width;
	int height;
	unsigned int *pixels;
	unsigned char bytes_per_pixel;
	unsigned int pixels_bytes_len;
	/* bytes in a row of pixel data, including padding */
	unsigned int pitch;
};

struct audio_context {
	unsigned int *sound_buffer;
	unsigned int sound_buffer_bytes;
	unsigned int write_cursor;
	unsigned int play_cursor;
};

struct game_context {
	struct pixel_buffer *virtual_win;
	struct audio_context *audio_ctx;
	unsigned char x_offset;
	unsigned char y_offset;
	int x_shift;
	int y_shift;
};

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

internal void fill_virtual(struct game_context *ctx)
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

internal void resize_pixel_buffer(struct pixel_buffer *buf, int height,
				  int width)
{
	if (ERIC_DEBUG) {
		fprintf(stderr, "Buffer was %d x %d, need %d x %d\n",
			buf->width, buf->height, width, height);
	}
	if (buf->pixels) {
		munmap(buf->pixels, buf->pixels_bytes_len);
	}
	buf->width = width;
	buf->height = height;
	buf->pixels_bytes_len = buf->height * buf->width * buf->bytes_per_pixel;
	buf->pixels =
	    mmap(0, buf->pixels_bytes_len, PROT_READ | PROT_WRITE,
		 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (!buf->pixels) {
		fprintf(stderr, "Could not alloc buf->pixels (%d)\n",
			buf->pixels_bytes_len);
		exit(EXIT_FAILURE);
	}
	buf->pitch = buf->width * buf->bytes_per_pixel;
}

internal void sdl_resize_texture_buf(SDL_Window *window,
				     SDL_Renderer *renderer,
				     struct sdl_texture_buffer *texture_buf)
{
	int height, width;
	struct pixel_buffer *pixel_buf = texture_buf->pixel_buf;

	SDL_GetWindowSize(window, &width, &height);
	if (width == pixel_buf->width && height == pixel_buf->height) {
		/* nothing to do */
		return;
	}
	if (ERIC_DEBUG) {
		fprintf(stderr, "Texture was %d x %d, need %d x %d\n",
			pixel_buf->width, pixel_buf->height, width, height);
	}
	if (texture_buf->texture) {
		SDL_DestroyTexture(texture_buf->texture);
	}
	texture_buf->texture = SDL_CreateTexture(renderer,
						 SDL_PIXELFORMAT_ARGB8888,
						 SDL_TEXTUREACCESS_STREAMING,
						 width, height);
	if (!texture_buf->texture) {
		fprintf(stderr, "Could not alloc texture_buf->texture\n");
		exit(EXIT_FAILURE);
	}

	resize_pixel_buffer(pixel_buf, height, width);
}

internal void fill_blit_buf_from_virtual(struct pixel_buffer *blit_buf,
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

internal void sdl_blit_bytes(SDL_Renderer *renderer, SDL_Texture *texture,
			     const SDL_Rect *rect, const void *pixels,
			     int pitch)
{
	if (SDL_UpdateTexture(texture, rect, pixels, pitch)) {
		fprintf(stderr, "Could not SDL_UpdateTexture\n");
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
				struct game_context *ctx)
{
	/*
	   int was_down = ((Event->key.repeat != 0)
	   || (Event->key.state == SDL_RELEASED));
	 */
	switch (event_ctx->event->key.keysym.scancode) {
	case SDL_SCANCODE_ESCAPE:
		event_ctx->event->type = SDL_QUIT;
		SDL_PushEvent(event_ctx->event);
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
	default:
		break;
	};
}

internal int process_event(struct sdl_event_context *event_ctx,
			   struct game_context *ctx)
{
	switch (event_ctx->event->type) {
	case SDL_QUIT:
		return 1;
		break;
	case SDL_KEYUP:
	case SDL_KEYDOWN:
		process_key_event(event_ctx, ctx);
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

internal void pixel_buffer_init(struct pixel_buffer *buf)
{
	buf->width = 0;
	buf->height = 0;
	buf->bytes_per_pixel = sizeof(unsigned int);
	buf->pixels = NULL;
	buf->pixels_bytes_len = 0;
	buf->pitch = 0;
}

void audio_callback(void *userdata, unsigned char *stream, int len)
{
	int region_1_bytes, region_2_bytes;
	struct audio_context *audio_ctx;

	audio_ctx = (struct audio_context *)userdata;

	if (audio_ctx->write_cursor <= audio_ctx->play_cursor) {
		/* we do not have any new sound data */
		return;
	}
	if ((audio_ctx->write_cursor - audio_ctx->play_cursor) <
	    (unsigned long)len) {
		/* we only have this much new sound available */
		len = (audio_ctx->write_cursor - audio_ctx->play_cursor);
	}

	region_1_bytes = len;
	region_2_bytes = 0;
	if ((audio_ctx->play_cursor % audio_ctx->sound_buffer_bytes) + len >
	    audio_ctx->sound_buffer_bytes) {
		region_1_bytes =
		    audio_ctx->sound_buffer_bytes -
		    (audio_ctx->play_cursor % audio_ctx->sound_buffer_bytes);
		region_2_bytes = len - region_1_bytes;
	}
	memcpy(stream,
	       ((unsigned char *)(audio_ctx->sound_buffer)) +
	       (audio_ctx->play_cursor % audio_ctx->sound_buffer_bytes),
	       region_1_bytes);
	if (region_2_bytes) {
		memcpy(&stream[region_1_bytes], audio_ctx->sound_buffer,
		       region_2_bytes);
	}

	audio_ctx->play_cursor += len;
}

internal unsigned int init_audio_context(struct audio_context *audio_ctx)
{
	audio_ctx->write_cursor = 0;
	audio_ctx->play_cursor = 0;
	audio_ctx->sound_buffer_bytes =
	    HANDMAIDEN_AUDIO_BUF_SAMPLES_SIZE *
	    HANDMAIDEN_AUDIO_BYTES_PER_SAMPLE * HANDMAIDEN_AUDIO_CHANNELS;
	audio_ctx->sound_buffer =
	    (unsigned int *)mmap(0, audio_ctx->sound_buffer_bytes,
				 PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (!audio_ctx->sound_buffer) {
		fprintf(stderr, "could not allocate sound buffer\n");
	}
	return audio_ctx->sound_buffer ? 1 : 0;
}

internal SDL_AudioDeviceID init_sdl_audio(struct audio_context *audio_ctx)
{
	int audio_allow_change;
	SDL_AudioSpec want, have;
	SDL_AudioDeviceID sdl_audio_dev;

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
	want.samples = HANDMAIDEN_AUDIO_BUF_SAMPLES_SIZE;
	want.callback = &audio_callback;
	want.userdata = audio_ctx;
	audio_allow_change =
	    SDL_AUDIO_ALLOW_FORMAT_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE;

	sdl_audio_dev =
	    SDL_OpenAudioDevice(NULL, 0, &want, &have, audio_allow_change);
	if (sdl_audio_dev == 0) {
		fprintf(stderr, "Failed to open audio: %s\n", SDL_GetError());
	}
	if (have.format != want.format) {
		printf("Expected Float32 (%x) audio format, got %x\n",
		       want.format, have.format);
	}
	if (have.channels != want.channels) {
		printf("Expected %d audio channels, got %d.\n", want.channels,
		       have.channels);
	}
	return sdl_audio_dev;
}

void fill_sound_buffer(struct game_context *ctx)
{
	unsigned int tone_hz, tone_volume, square_wave_period,
	    half_square_wave_period, bytes_per_sample, bytes_to_write,
	    region_1_bytes, region_2_bytes, sample_count, i, buf_pos,
	    sample_value;
	struct audio_context *audio_ctx;

	audio_ctx = ctx->audio_ctx;

	tone_hz = 128;
	tone_hz += 8 * ((ctx->x_shift < 0) ? -(ctx->x_shift) : ctx->x_shift);
	tone_hz += 8 * ((ctx->y_shift < 0) ? -(ctx->y_shift) : ctx->y_shift);
	tone_volume = 128;
	square_wave_period = (HANDMAIDEN_AUDIO_SAMPLES_PER_SECOND) / tone_hz;
	half_square_wave_period = square_wave_period / 2;
	bytes_per_sample =
	    HANDMAIDEN_AUDIO_BYTES_PER_SAMPLE * HANDMAIDEN_AUDIO_CHANNELS;

	if ((audio_ctx->write_cursor - audio_ctx->play_cursor) >=
	    audio_ctx->sound_buffer_bytes) {
		/* no room to write more sound data */
		return;
	}

	bytes_to_write =
	    audio_ctx->sound_buffer_bytes - (audio_ctx->write_cursor -
					     audio_ctx->play_cursor);

	if (((audio_ctx->write_cursor % audio_ctx->sound_buffer_bytes) >=
	     (audio_ctx->play_cursor % audio_ctx->sound_buffer_bytes)) &&
	    (bytes_to_write >
	     (audio_ctx->sound_buffer_bytes -
	      (audio_ctx->write_cursor % audio_ctx->sound_buffer_bytes)))) {
		region_1_bytes =
		    (audio_ctx->sound_buffer_bytes -
		     (audio_ctx->write_cursor % audio_ctx->sound_buffer_bytes));
		region_2_bytes = bytes_to_write - region_1_bytes;
	} else {
		region_1_bytes = bytes_to_write;;
		region_2_bytes = 0;
	}

	if (audio_ctx->write_cursor % HANDMAIDEN_AUDIO_BYTES_PER_SAMPLE) {
		fprintf(stderr, "unexpected audio position\n");
	}
	buf_pos = audio_ctx->write_cursor % audio_ctx->sound_buffer_bytes;
	sample_count = region_1_bytes / bytes_per_sample;
	for (i = 0; i < sample_count; i++) {
		sample_value =
		    (((audio_ctx->write_cursor + buf_pos / bytes_per_sample) /
		      half_square_wave_period) %
		     2) ? tone_volume : -tone_volume;
		*(unsigned int *)(((unsigned char *)audio_ctx->sound_buffer) +
				  buf_pos) = sample_value;
		buf_pos += HANDMAIDEN_AUDIO_BYTES_PER_SAMPLE;
		*(unsigned int *)(((unsigned char *)audio_ctx->sound_buffer) +
				  buf_pos) = sample_value;
		buf_pos += HANDMAIDEN_AUDIO_BYTES_PER_SAMPLE;
	}
	audio_ctx->write_cursor += buf_pos;

	sample_count = region_2_bytes / bytes_per_sample;
	buf_pos = 0;
	for (i = 0; i < sample_count; i++) {
		sample_value =
		    ((audio_ctx->write_cursor + buf_pos /
		      half_square_wave_period) %
		     2) ? tone_volume : -tone_volume;
		*(unsigned int *)(((unsigned char *)audio_ctx->sound_buffer) +
				  buf_pos) = sample_value;
		buf_pos += HANDMAIDEN_AUDIO_BYTES_PER_SAMPLE;
		*(unsigned int *)(((unsigned char *)audio_ctx->sound_buffer) +
				  buf_pos) = sample_value;
		buf_pos += HANDMAIDEN_AUDIO_BYTES_PER_SAMPLE;
	}
	audio_ctx->write_cursor += buf_pos;
}

void diff_timespecs(struct timespec start, struct timespec end,
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
	resize_pixel_buffer(&virtual_win, height, width);

	ctx.virtual_win = &virtual_win;
	ctx.audio_ctx = &audio_ctx;
	ctx.x_offset = 0;
	ctx.y_offset = 0;
	ctx.x_shift = 0;
	ctx.y_shift = 0;
	fill_virtual(&ctx);

	title = (argc > 1) ? argv[1] : "Handmaiden Hero";
	x = SDL_WINDOWPOS_UNDEFINED;
	y = SDL_WINDOWPOS_UNDEFINED;
	flags = SDL_WINDOW_RESIZABLE;
	window = SDL_CreateWindow(title, x, y, width, height, flags);
	if (!window) {
		fprintf(stderr, "Could not SDL_CreateWindow\n");
		return 2;
	}

	renderer = SDL_CreateRenderer(window, FIRST_SUPPORTING, NONE);
	if (!renderer) {
		return 3;
	}

	sdl_resize_texture_buf(window, renderer, &texture_buf);

	if (init_audio_context(&audio_ctx)) {
		sdl_audio_dev = init_sdl_audio(&audio_ctx);
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
			shutdown = process_event(&event_ctx, &ctx);
		}
		ctx.x_offset += ctx.x_shift;
		ctx.y_offset += ctx.y_shift;
		fill_virtual(&ctx);
		fill_blit_buf_from_virtual(&blit_buf, &virtual_win);
		sdl_blit_texture(renderer, &texture_buf);
		SDL_LockAudio();
		fill_sound_buffer(&ctx);
		SDL_UnlockAudio();
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
		}

		if (frames_since_print == 0) {
			fprintf(stderr,
				"fps: %.02f (avg fps: %.f) %u, %u\r",
				fps,
				(double)frame_count /
				(double)total_elapsed_seconds,
				audio_ctx.play_cursor, audio_ctx.write_cursor);
		}
	}
	fprintf(stderr, "\n");

	if (sdl_audio_dev) {
		SDL_CloseAudioDevice(sdl_audio_dev);
	}

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
