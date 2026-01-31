#ifndef LTC_ENCODER_H
#define LTC_ENCODER_H

#include "config.h"
#include <cstdint>
#include <vector>

// LTC时码结构
struct LTCFrame {
    uint8_t frame;      // 0-29 (or 23, 24, etc.)
    uint8_t second;     // 0-59
    uint8_t minute;     // 0-59
    uint8_t hour;       // 0-23
    uint8_t user_bits[8]; // User bits
};

// LTC编码器
class LTCEncoder {
public:
    explicit LTCEncoder(Config::LTCFrameRate frame_rate);

    // 设置当前时间码
    void setTimecode(uint8_t hour, uint8_t minute, uint8_t second, uint8_t frame);

    // 编码一帧LTC数据（80 bits）
    std::vector<bool> encodeFrame();

    // 获取当前比特率（bits/sec）
    double getBitRate() const { return bit_rate_; }

    // 获取每比特的时长（秒）
    double getBitDuration() const { return 1.0 / bit_rate_; }

    // 自动递增时间码（用于连续生成）
    void incrementFrame();

private:
    Config::LTCFrameRate frame_rate_;
    double bit_rate_;
    LTCFrame current_frame_;

    // BCD编码辅助函数
    uint8_t toBCD(uint8_t value);

    // 计算同步字
    uint16_t getSyncWord();
};

#endif // LTC_ENCODER_H
