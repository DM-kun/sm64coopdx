#include "smd_wrapper.h"
#include "sound_matrix_decoder.h"
#include <memory>

static std::unique_ptr<SoundMatrixDecoder> sSoundMatrixDecoder;

extern "C" {

void sound_matrix_decoder_init(void) {
    if (sSoundMatrixDecoder) { return; }
    sSoundMatrixDecoder = std::make_unique<SoundMatrixDecoder>(32000);
}

void sound_matrix_decoder_shutdown(void) {
    sSoundMatrixDecoder.reset();
}

void sound_matrix_decoder_reset_state(void) {
    if (!sSoundMatrixDecoder) { return; }
    sSoundMatrixDecoder->ResetState();
}

const uint8_t* sound_matrix_decoder_process_buf(const uint8_t* buf, size_t len) {
    if (!sSoundMatrixDecoder) { return buf; }
    const auto [surroundBuf, surroundLen] = sSoundMatrixDecoder->Process(buf, len);
    return surroundBuf;
}

size_t sound_matrix_decoder_process_len(const uint8_t* buf, size_t len) {
    if (!sSoundMatrixDecoder) { return len; }
    const auto [surroundBuf, surroundLen] = sSoundMatrixDecoder->Process(buf, len);
    return surroundLen;
}

}
