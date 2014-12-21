#define ERIC_DEBUG 0
#define HANDMAIDEN_TRY_TO_MAKE_VALGRIND_HAPPY ERIC_DEBUG

/* memory allocation by mmap requires _GNU_SOURCE since it is linux specific */
#define _GNU_SOURCE
#include <sys/mman.h>
/* end mmap includes */

/* the Simple Directmedia Library requires the following includes */
#include <SDL.h>
/* end SDL */

#include <stdio.h>		/* fprintf */
#include <stdint.h>		/* unit8_t */

#define FIRST_SUPPORTING -1
#define NONE 0

#define internal static
/*
#define global_variable static
#define local_persistant static
*/

struct pixel_buffer {
	int width;
	int height;
	uint32_t *pixels;
	uint8_t bytes_per_pixel;
	size_t pixels_bytes_len;
	/* bytes in a row of pixel data, including padding */
	size_t pitch;
};

struct game_context {
	struct pixel_buffer *virtual_win;
	uint8_t x_offset;
	uint8_t y_offset;
	int x_shift;
	int y_shift;
};

struct texture_buffer {
	SDL_Texture *texture;
	struct pixel_buffer *pixel_buf;
};

struct sdl_event_context {
	struct texture_buffer *texture_buf;
	SDL_Window *window;
	Uint32 win_id;
	SDL_Renderer *renderer;
	SDL_Event *event;
};

internal void fill_virtual(struct game_context *ctx)
{
	struct pixel_buffer *virtual_win = ctx->virtual_win;
	int x, y;
	uint8_t red, blue;
	uint32_t foreground;

	for (y = 0; y < virtual_win->height; y++) {
		for (x = 0; x < virtual_win->width; x++) {
			/* uint8_t means % 256 */
			red = x + ctx->x_offset;
			blue = y + ctx->y_offset;
			foreground = (((uint32_t) blue) << 16) + (uint32_t) red;
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

internal void resize_texture_buffer(SDL_Window *window,
				    SDL_Renderer *renderer,
				    struct texture_buffer *texture_buf)
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
	uint32_t foreground;
	size_t virt_pos, off_pos;

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

internal void blit_bytes(SDL_Renderer *renderer, SDL_Texture *texture,
			 const SDL_Rect *rect, const void *pixels, int pitch)
{
	if (SDL_UpdateTexture(texture, rect, pixels, pitch)) {
		fprintf(stderr, "Could not SDL_UpdateTexture\n");
		exit(EXIT_FAILURE);
	}
	SDL_RenderCopy(renderer, texture, 0, 0);
	SDL_RenderPresent(renderer);
}

internal void blit_texture(SDL_Renderer *renderer,
			   struct texture_buffer *texture_buf)
{
	SDL_Texture *texture = texture_buf->texture;
	const SDL_Rect *rect = 0;
	const void *pixels = texture_buf->pixel_buf->pixels;
	int pitch = texture_buf->pixel_buf->pitch;

	blit_bytes(renderer, texture, rect, pixels, pitch);
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
			resize_texture_buffer(event_ctx->window,
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
	buf->bytes_per_pixel = sizeof(uint32_t);
	buf->pixels = NULL;
	buf->pixels_bytes_len = 0;
	buf->pitch = 0;
}

/*
userdata: an application-specific parameter saved in the SDL_AudioSpec
          structure's userdata field
stream:   a pointer to the audio data buffer filled in by SDL_AudioCallback()
len:      the length of that buffer in bytes
*/
void audio_callback(void *userdata, Uint8 *stream, int len)
{
	memset(stream, userdata ? *((int *)userdata) : 0, len);
}

internal SDL_AudioDeviceID init_audio()
{
	int audio_allow_change;
	SDL_AudioSpec want, have;
	SDL_AudioDeviceID audio_dev;

	SDL_zero(want);
	want.freq = 48000;
	want.format = AUDIO_F32;
	want.channels = 2;
	want.samples = 4096;
	want.callback = audio_callback;
	audio_allow_change =
	    SDL_AUDIO_ALLOW_FORMAT_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE;

	audio_dev =
	    SDL_OpenAudioDevice(NULL, 0, &want, &have, audio_allow_change);
	if (audio_dev == 0) {
		fprintf(stderr, "Failed to open audio: %s\n", SDL_GetError());
		return 0;
	}
	if (have.format != want.format) {
		printf("Expected Float32 (%x) audio format, got %x\n",
		       want.format, have.format);
	}
	if (have.channels != want.channels) {
		printf("Expected %d audio channels, got %d.\n", want.channels,
		       have.channels);
	}

	return audio_dev;
}

int main(int argc, char *argv[])
{
	char *title;
	SDL_Window *window;
	Uint32 flags;
	int x, y, width, height, shutdown;
	SDL_Renderer *renderer;
	SDL_Event event;
	struct game_context ctx;
	struct pixel_buffer virtual_win;
	struct pixel_buffer blit_buf;
	struct texture_buffer texture_buf;
	struct sdl_event_context event_ctx;

	SDL_AudioDeviceID audio_dev;

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

	resize_texture_buffer(window, renderer, &texture_buf);

	audio_dev = init_audio();
	/* start audio playing by setting "pause" to zero */
	SDL_PauseAudioDevice(audio_dev, 0);

	event_ctx.event = &event;
	event_ctx.texture_buf = &texture_buf;
	event_ctx.renderer = renderer;
	event_ctx.window = window;
	event_ctx.win_id = SDL_GetWindowID(window);

	shutdown = 0;
	while (!shutdown) {
		while (SDL_PollEvent(&event)) {
			shutdown = process_event(&event_ctx, &ctx);
		}
		ctx.x_offset += ctx.x_shift;
		ctx.y_offset += ctx.y_shift;
		fill_virtual(&ctx);
		fill_blit_buf_from_virtual(&blit_buf, &virtual_win);
		blit_texture(renderer, &texture_buf);
	}

	if (audio_dev) {
		SDL_CloseAudioDevice(audio_dev);
	}

	/* we probably do not need to do these next steps */
	if (HANDMAIDEN_TRY_TO_MAKE_VALGRIND_HAPPY) {
		munmap(virtual_win.pixels, virtual_win.pixels_bytes_len);
		if (texture_buf.texture) {
			SDL_DestroyTexture(texture_buf.texture);
		}
		if (blit_buf.pixels) {
			munmap(blit_buf.pixels, blit_buf.pixels_bytes_len);
		}
		/* other stuff */
	}

	return 0;
}
