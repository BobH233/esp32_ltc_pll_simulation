#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>

// ==================== 仿真参数配置 ====================

namespace Config {
    // 时钟参数
    constexpr double DS3231_ACCURACY_PPM = 2.0;           // DS3231精度：±2ppm
    constexpr double ESP32_INITIAL_ERROR_PPM = 20.0;      // ESP32初始误差：±20ppm
    constexpr double ESP32_TEMP_DRIFT_PPM_PER_C = 0.5;    // 温度漂移：±0.5ppm/°C
    constexpr double ESP32_RANDOM_JITTER_PPM = 0.1;       // 随机抖动：±0.1ppm

    // GPIO中断延迟模拟
    constexpr double GPIO_INTERRUPT_DELAY_US = 3.0;       // 平均延迟：3μs
    constexpr double GPIO_INTERRUPT_JITTER_US = 2.0;      // 延迟抖动：±2μs

    // PLL参数
    constexpr double PLL_UPDATE_INTERVAL_SEC = 5.0;       // PLL更新周期：5秒
    constexpr double PLL_FILTER_ALPHA = 0.15;             // 一阶滤波系数
    constexpr double PLL_PROPORTIONAL_GAIN = 0.8;         // 比例增益
    constexpr int    PLL_WARMUP_SAMPLES = 3;              // 冷启动预热次数

    // RMT时钟调整参数
    constexpr double RMT_TUNE_GRANULARITY_PPM = 0.01;     // RMT调整粒度：0.01ppm
    constexpr double RMT_MAX_ADJUSTMENT_PPM = 100.0;      // RMT最大调整范围：±100ppm（容纳极端情况）

    // LTC时码参数
    enum class LTCFrameRate {
        FPS_23976 = 0,      // 23.976 fps (24 * 1000/1001) - HD视频制作主流标准
        FPS_24 = 1,         // 24 fps - 传统胶片电影标准
        FPS_25 = 2,         // 25 fps - PAL制式（中国/欧洲/澳洲）
        FPS_2997_NDF = 3,   // 29.97 fps Non-Drop Frame - NTSC制式
        FPS_2997_DF = 4,    // 29.97 fps Drop Frame - NTSC制式（时码对齐）
        FPS_30 = 5          // 30 fps - 网络视频/特殊用途
    };

    constexpr LTCFrameRate DEFAULT_FRAME_RATE = LTCFrameRate::FPS_24;
    constexpr int LTC_BITS_PER_FRAME = 80;

    // 获取帧率对应的数值
    inline double getFrameRate(LTCFrameRate fr) {
        switch(fr) {
            case LTCFrameRate::FPS_23976: return 24.0 * 1000.0 / 1001.0;  // 23.976023...
            case LTCFrameRate::FPS_24: return 24.0;
            case LTCFrameRate::FPS_25: return 25.0;
            case LTCFrameRate::FPS_2997_NDF: return 30.0 * 1000.0 / 1001.0;  // 29.970029...
            case LTCFrameRate::FPS_2997_DF: return 30.0 * 1000.0 / 1001.0;   // 29.970029...
            case LTCFrameRate::FPS_30: return 30.0;
            default: return 24.0;
        }
    }

    // 获取帧率名称（用于显示）
    inline const char* getFrameRateName(LTCFrameRate fr) {
        switch(fr) {
            case LTCFrameRate::FPS_23976: return "23.976";
            case LTCFrameRate::FPS_24: return "24";
            case LTCFrameRate::FPS_25: return "25";
            case LTCFrameRate::FPS_2997_NDF: return "29.97 NDF";
            case LTCFrameRate::FPS_2997_DF: return "29.97 DF";
            case LTCFrameRate::FPS_30: return "30";
            default: return "24";
        }
    }

    // 判断是否为drop-frame模式
    inline bool isDropFrame(LTCFrameRate fr) {
        return fr == LTCFrameRate::FPS_2997_DF;
    }

    // 获取LTC比特率（bits/sec）
    inline double getLTCBitRate(LTCFrameRate fr) {
        return getFrameRate(fr) * LTC_BITS_PER_FRAME;
    }

    // 仿真参数
    constexpr double SIMULATION_DURATION_HOURS = 24.0;    // 仿真24小时（完整验证）
    constexpr double SIMULATION_TIME_STEP_US = 10.0;      // 时间步长：10μs（高精度验证）
    constexpr double TEMPERATURE_CHANGE_RATE_C_PER_HOUR = 2.0; // 温度变化速率
    constexpr double INITIAL_TEMPERATURE_C = 25.0;        // 初始温度

    // 统计参数
    constexpr double STATS_REPORT_INTERVAL_SEC = 3600.0;  // 每小时输出一次统计
    constexpr int    STATS_HISTOGRAM_BINS = 100;          // 误差分布直方图bins数量
}

#endif // CONFIG_H
