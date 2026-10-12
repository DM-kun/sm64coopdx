#ifndef SMD_WRAPPER_H
#define SMD_WRAPPER_H

#ifdef __cplusplus
#include <cstdint>
extern "C" {
#endif

void sound_matrix_decoder_init(void);
void sound_matrix_decoder_shutdown(void);

void sound_matrix_decoder_reset_state(void);

const uint8_t* sound_matrix_decoder_process_buf(const uint8_t* buf, size_t len);
size_t sound_matrix_decoder_process_len(const uint8_t* buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif
