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

void init_input(struct human_input *input);

int process_input(struct game_context *ctx, struct human_input *input);

void update_pixel_buffer(struct game_context *ctx);

void update_audio_buf(struct game_context *ctx, struct audio_buffer *audio_buf);

#endif /* _HANDMAIDEN_H_ */
