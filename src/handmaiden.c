/* handmaiden.c */
#include "handmaiden.h"

#include <math.h>		/* sin */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void init_game_context(struct game_context *ctx,
		       struct pixel_buffer *virtual_win, unsigned int volume)
{
	ctx->virtual_win = virtual_win;
	ctx->x_offset = 0;
	ctx->y_offset = 0;
	ctx->x_shift = 0;
	ctx->y_shift = 0;
	ctx->sound_volume = volume;
	ctx->tone_hz = 0;
}

void update_pixel_buffer(struct game_context *ctx)
{
	struct pixel_buffer *virtual_win = ctx->virtual_win;
	unsigned int x, y;
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

void update_audio_buf(struct game_context *ctx, struct audio_buffer *audio_buf)
{
	unsigned int i, tone_hz;
	int *left_sample, *right_sample;
	double sine;

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
