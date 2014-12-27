
#include "handmaiden.h"

void update_pixel_buffer(struct game_context *ctx)
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
