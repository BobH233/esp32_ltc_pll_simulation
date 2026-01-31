#include "ltc_encoder.h"
#include <cstring>

LTCEncoder::LTCEncoder(Config::LTCFrameRate frame_rate)
    : frame_rate_(frame_rate)
    , bit_rate_(Config::getLTCBitRate(frame_rate))
{
    std::memset(&current_frame_, 0, sizeof(current_frame_));
}

void LTCEncoder::setTimecode(uint8_t hour, uint8_t minute, uint8_t second, uint8_t frame) {
    current_frame_.hour = hour;
    current_frame_.minute = minute;
    current_frame_.second = second;
    current_frame_.frame = frame;
}

uint8_t LTCEncoder::toBCD(uint8_t value) {
    return ((value / 10) << 4) | (value % 10);
}

uint16_t LTCEncoder::getSyncWord() {
    // LTC标准同步字：0011 1111 1111 1101
    return 0x3FFD;
}

std::vector<bool> LTCEncoder::encodeFrame() {
    std::vector<bool> bits(80);

    // LTC帧格式（简化版本，仅包含核心时间信息）
    // Bits 0-3: Frame units (BCD)
    uint8_t frame_bcd = toBCD(current_frame_.frame);
    bits[0] = frame_bcd & 0x01;
    bits[1] = (frame_bcd & 0x02) >> 1;
    bits[2] = (frame_bcd & 0x04) >> 2;
    bits[3] = (frame_bcd & 0x08) >> 3;

    // Bits 4-7: User bits 1
    for (int i = 0; i < 4; i++) {
        bits[4 + i] = (current_frame_.user_bits[0] & (1 << i)) != 0;
    }

    // Bits 8-9: Frame tens
    bits[8] = (frame_bcd & 0x10) >> 4;
    bits[9] = (frame_bcd & 0x20) >> 5;

    // Bit 10: Drop frame flag (for 29.97)
    bits[10] = (frame_rate_ == Config::LTCFrameRate::FPS_2997_DF);

    // Bit 11: Color frame flag
    bits[11] = false;

    // Bits 12-15: User bits 2
    for (int i = 0; i < 4; i++) {
        bits[12 + i] = (current_frame_.user_bits[1] & (1 << i)) != 0;
    }

    // Bits 16-19: Second units
    uint8_t second_bcd = toBCD(current_frame_.second);
    bits[16] = second_bcd & 0x01;
    bits[17] = (second_bcd & 0x02) >> 1;
    bits[18] = (second_bcd & 0x04) >> 2;
    bits[19] = (second_bcd & 0x08) >> 3;

    // Bits 20-23: User bits 3
    for (int i = 0; i < 4; i++) {
        bits[20 + i] = (current_frame_.user_bits[2] & (1 << i)) != 0;
    }

    // Bits 24-26: Second tens
    bits[24] = (second_bcd & 0x10) >> 4;
    bits[25] = (second_bcd & 0x20) >> 5;
    bits[26] = (second_bcd & 0x40) >> 6;

    // Bit 27: Polarity correction (fixed)
    bits[27] = false;

    // Bits 28-31: User bits 4
    for (int i = 0; i < 4; i++) {
        bits[28 + i] = (current_frame_.user_bits[3] & (1 << i)) != 0;
    }

    // Bits 32-35: Minute units
    uint8_t minute_bcd = toBCD(current_frame_.minute);
    bits[32] = minute_bcd & 0x01;
    bits[33] = (minute_bcd & 0x02) >> 1;
    bits[34] = (minute_bcd & 0x04) >> 2;
    bits[35] = (minute_bcd & 0x08) >> 3;

    // Bits 36-39: User bits 5
    for (int i = 0; i < 4; i++) {
        bits[36 + i] = (current_frame_.user_bits[4] & (1 << i)) != 0;
    }

    // Bits 40-42: Minute tens
    bits[40] = (minute_bcd & 0x10) >> 4;
    bits[41] = (minute_bcd & 0x20) >> 5;
    bits[42] = (minute_bcd & 0x40) >> 6;

    // Bit 43: Binary group flag
    bits[43] = false;

    // Bits 44-47: User bits 6
    for (int i = 0; i < 4; i++) {
        bits[44 + i] = (current_frame_.user_bits[5] & (1 << i)) != 0;
    }

    // Bits 48-51: Hour units
    uint8_t hour_bcd = toBCD(current_frame_.hour);
    bits[48] = hour_bcd & 0x01;
    bits[49] = (hour_bcd & 0x02) >> 1;
    bits[50] = (hour_bcd & 0x04) >> 2;
    bits[51] = (hour_bcd & 0x08) >> 3;

    // Bits 52-55: User bits 7
    for (int i = 0; i < 4; i++) {
        bits[52 + i] = (current_frame_.user_bits[6] & (1 << i)) != 0;
    }

    // Bits 56-57: Hour tens
    bits[56] = (hour_bcd & 0x10) >> 4;
    bits[57] = (hour_bcd & 0x20) >> 5;

    // Bits 58-59: Reserved/unused
    bits[58] = false;
    bits[59] = false;

    // Bits 60-63: User bits 8
    for (int i = 0; i < 4; i++) {
        bits[60 + i] = (current_frame_.user_bits[7] & (1 << i)) != 0;
    }

    // Bits 64-79: Sync word (0x3FFD = 0011111111111101)
    uint16_t sync = getSyncWord();
    for (int i = 0; i < 16; i++) {
        bits[64 + i] = (sync & (1 << i)) != 0;
    }

    return bits;
}

void LTCEncoder::incrementFrame() {
    current_frame_.frame++;

    uint8_t max_frame = static_cast<uint8_t>(Config::getFrameRate(frame_rate_));

    if (current_frame_.frame >= max_frame) {
        current_frame_.frame = 0;
        current_frame_.second++;

        if (current_frame_.second >= 60) {
            current_frame_.second = 0;
            current_frame_.minute++;

            if (current_frame_.minute >= 60) {
                current_frame_.minute = 0;
                current_frame_.hour++;

                if (current_frame_.hour >= 24) {
                    current_frame_.hour = 0;
                }
            }
        }
    }
}
