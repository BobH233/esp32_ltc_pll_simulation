#include "ltc_decoder.h"
#include <cstring>

LTCDecoder::LTCDecoder(Config::LTCFrameRate frame_rate)
    : frame_rate_(frame_rate)
    , total_frames_(0)
    , successful_decodes_(0)
    , sync_errors_(0)
    , parity_errors_(0)
{
}

uint8_t LTCDecoder::fromBCD(uint8_t bcd) {
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

bool LTCDecoder::verifySyncWord(const std::vector<bool>& bits, size_t start_pos) {
    if (start_pos + 16 > bits.size()) {
        return false;
    }

    // 期望的同步字：0x3FFD = 0011111111111101
    uint16_t expected_sync = 0x3FFD;
    uint16_t actual_sync = 0;

    for (int i = 0; i < 16; i++) {
        if (bits[start_pos + i]) {
            actual_sync |= (1 << i);
        }
    }

    return actual_sync == expected_sync;
}

bool LTCDecoder::decodeBits(const std::vector<bool>& bits, LTCFrame& decoded_frame) {
    total_frames_++;

    if (bits.size() != 80) {
        return false;
    }

    // 验证同步字
    if (!verifySyncWord(bits, 64)) {
        sync_errors_++;
        return false;
    }

    // 解码帧号
    uint8_t frame_units = 0;
    for (int i = 0; i < 4; i++) {
        if (bits[i]) frame_units |= (1 << i);
    }
    uint8_t frame_tens = 0;
    if (bits[8]) frame_tens |= 0x10;
    if (bits[9]) frame_tens |= 0x20;
    decoded_frame.frame = fromBCD(frame_tens | frame_units);

    // 解码秒
    uint8_t second_units = 0;
    for (int i = 0; i < 4; i++) {
        if (bits[16 + i]) second_units |= (1 << i);
    }
    uint8_t second_tens = 0;
    if (bits[24]) second_tens |= 0x10;
    if (bits[25]) second_tens |= 0x20;
    if (bits[26]) second_tens |= 0x40;
    decoded_frame.second = fromBCD(second_tens | second_units);

    // 解码分钟
    uint8_t minute_units = 0;
    for (int i = 0; i < 4; i++) {
        if (bits[32 + i]) minute_units |= (1 << i);
    }
    uint8_t minute_tens = 0;
    if (bits[40]) minute_tens |= 0x10;
    if (bits[41]) minute_tens |= 0x20;
    if (bits[42]) minute_tens |= 0x40;
    decoded_frame.minute = fromBCD(minute_tens | minute_units);

    // 解码小时
    uint8_t hour_units = 0;
    for (int i = 0; i < 4; i++) {
        if (bits[48 + i]) hour_units |= (1 << i);
    }
    uint8_t hour_tens = 0;
    if (bits[56]) hour_tens |= 0x10;
    if (bits[57]) hour_tens |= 0x20;
    decoded_frame.hour = fromBCD(hour_tens | hour_units);

    // 简单的合理性检查
    uint8_t max_frame = static_cast<uint8_t>(Config::getFrameRate(frame_rate_));
    if (decoded_frame.frame >= max_frame ||
        decoded_frame.second >= 60 ||
        decoded_frame.minute >= 60 ||
        decoded_frame.hour >= 24) {
        parity_errors_++;
        return false;
    }

    successful_decodes_++;
    return true;
}

LTCDecoder::DecodeStats LTCDecoder::getStatistics() const {
    DecodeStats stats;
    stats.total_frames = total_frames_;
    stats.successful_decodes = successful_decodes_;
    stats.sync_errors = sync_errors_;
    stats.parity_errors = parity_errors_;
    stats.success_rate = total_frames_ > 0 ?
        (double)successful_decodes_ / total_frames_ * 100.0 : 0.0;
    return stats;
}

void LTCDecoder::resetStatistics() {
    total_frames_ = 0;
    successful_decodes_ = 0;
    sync_errors_ = 0;
    parity_errors_ = 0;
}
