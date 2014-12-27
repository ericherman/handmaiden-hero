#ifndef _HANDMAIDEN_H_
#define _HANDMAIDEN_H_

#define ERIC_DEBUG 0
#define DEBUG_LOG_AUDIO 0


/* audio config constants */
#define HANDMAIDEN_AUDIO_START_VOLUME 16
#define HANDMAIDEN_AUDIO_BUF_CHUNKS 4
#define HANDMAIDEN_AUDIO_BUF_SIZE (HANDMAIDEN_AUDIO_BUF_CHUNKS * 65536)
#define HANDMAIDEN_AUDIO_SAMPLES_PER_SECOND 48000
#define HANDMAIDEN_AUDIO_CHANNELS 2
#define HANDMAIDEN_AUDIO_BYTES_PER_SAMPLE sizeof(unsigned int)

void *platform_alloc(unsigned int len);
int platform_free(void *addr, unsigned int len);

const char *debug_audio_bin_log_path();
const char *debug_audio_txt_log_path();

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
	unsigned int volume;
	void *log;
};

struct game_context {
	struct pixel_buffer *virtual_win;
	struct audio_context *audio_ctx;
	unsigned char x_offset;
	unsigned char y_offset;
	int x_shift;
	int y_shift;
};

unsigned int init_audio_context(struct audio_context *audio_ctx);
void copy_audio(struct audio_context *audio_ctx, unsigned char *stream,
		int len);
void close_audio_debug_logging(struct audio_context *audio_ctx);

void fill_sound_buffer(struct game_context *ctx);

void fill_virtual(struct game_context *ctx);
void resize_pixel_buffer(struct pixel_buffer *buf, int height, int width);
void fill_blit_buf_from_virtual(struct pixel_buffer *blit_buf,
				struct pixel_buffer *virtual_win);
void pixel_buffer_init(struct pixel_buffer *buf);

int debug(int debug_level, const char *fmt, ...);
#endif /* _HANDMAIDEN_H_ */
