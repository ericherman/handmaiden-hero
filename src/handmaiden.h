#ifndef _HANDMAIDEN_H_
#define _HANDMAIDEN_H_

void *platform_alloc(unsigned int len);
int platform_free(void *addr, unsigned int len);

const char *debug_audio_bin_log_path();
const char *debug_audio_txt_log_path();

#endif /* _HANDMAIDEN_H_ */
