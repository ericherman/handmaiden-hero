#ifndef _HANDMAIDEN_H_
#define _HANDMAIDEN_H_

struct pixel_buffer {
	unsigned int width;
	unsigned int height;
	unsigned char bytes_per_pixel;
	unsigned int pixels_bytes_len;
	/* pitch is bytes in a row of pixel data, including padding */
	unsigned int pitch;
	unsigned int *pixels;
};

struct sample_buffer {
	unsigned long stream_pos;
	unsigned int num_samples;
	int *samples;
};

struct game_context {
	struct pixel_buffer *virtual_win;

	unsigned char x_offset;
	unsigned char y_offset;
	int x_shift;
	int y_shift;

	unsigned int sound_volume;
	unsigned int tone_hz;
};

void init_game_context(struct game_context *ctx,
		       struct pixel_buffer *virtual_win, unsigned int volume);

void update_pixel_buffer(struct game_context *ctx);

void update_sample_buf(struct game_context *ctx,
		       struct sample_buffer *sample_buf);

#endif /* _HANDMAIDEN_H_ */
