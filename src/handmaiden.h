#ifndef _HANDMAIDEN_H_
#define _HANDMAIDEN_H_

struct pixel_buffer {
	int width;
	int height;
	unsigned char bytes_per_pixel;
	unsigned int pixels_bytes_len;
	/* pitch is bytes in a row of pixel data, including padding */
	unsigned int pitch;
	unsigned int *pixels;
};

struct game_context {
	struct pixel_buffer *virtual_win;
	unsigned char x_offset;
	unsigned char y_offset;
	int x_shift;
	int y_shift;
};

void update_pixel_buffer(struct game_context *ctx);

#endif /* _HANDMAIDEN_H_ */
