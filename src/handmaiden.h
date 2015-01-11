#ifndef _HANDMAIDEN_H_
#define _HANDMAIDEN_H_

/* writing to address NULL will allways segfault */
#define HANDMAIDEN_ASSERT(condition) if(!(condition)) *((char *)0) = 0;

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

struct game_memory {
	unsigned int fixed_memory_size;
	unsigned char *fixed_memory;

	unsigned int transient_memory_size;
	unsigned char *transient_memory;

	unsigned int is_initialized:1;
};

/* services teh game provides to the platform layer */
void init_game(struct game_memory *mem, unsigned int initial_volume);

void init_input(struct human_input *input);

int process_input(struct game_memory *mem, struct human_input *input);

void update_pixel_buffer(struct game_memory *mem,
			 struct pixel_buffer **virtual_win);

void update_audio_buf(struct game_memory *mem, struct audio_buffer *audio_buf);

/* services the platform layer provides to the game */
void *DEBUG_platfrom_read_entire_file(char *filename, unsigned int *size);
void DEBUG_platform_free_file_memory(void *mem, unsigned int size);
int DEBUG_platform_write_entire_file(char *filename, unsigned int size,
				     void *mem);

#endif /* _HANDMAIDEN_H_ */
