#ifndef LTC_DECODER_H
#define LTC_DECODER_H

#include "ltc_encoder.h"
#include <vector>
#include <cstdint>

// LTC解码器
class LTCDecoder {
public:
    explicit LTCDecoder(Config::LTCFrameRate frame_rate);

    // 解码比特流
    bool decodeBits(const std::vector<bool>& bits, LTCFrame& decoded_frame);

    // 从BCD转换为普通数字
    static uint8_t fromBCD(uint8_t bcd);

    // 验证同步字
    bool verifySyncWord(const std::vector<bool>& bits, size_t start_pos);

    // 获取解码统计
    struct DecodeStats {
        uint64_t total_frames;
        uint64_t successful_decodes;
        uint64_t sync_errors;
        uint64_t parity_errors;
        double success_rate;
    };

    DecodeStats getStatistics() const;
    void resetStatistics();

private:
    Config::LTCFrameRate frame_rate_;

    // 统计信息
    uint64_t total_frames_;
    uint64_t successful_decodes_;
    uint64_t sync_errors_;
    uint64_t parity_errors_;
};

#endif // LTC_DECODER_H
