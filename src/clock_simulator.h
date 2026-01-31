#ifndef CLOCK_SIMULATOR_H
#define CLOCK_SIMULATOR_H

#include <random>
#include <cstdint>

// DS3231 RTC时钟模拟器
class DS3231Clock {
public:
    DS3231Clock(double accuracy_ppm);

    // 更新时钟状态（传入时间步长，单位：秒）
    void update(double delta_time_sec);

    // 获取当前时间戳（秒，包含小数部分）
    double getTimestamp() const { return timestamp_; }

    // 检查是否到达1Hz tick（每秒一次上升沿）
    bool checkOnePPSTick();

    // 重置状态
    void reset();

private:
    double accuracy_ppm_;        // 精度（ppm）
    double timestamp_;           // 当前时间戳
    double frequency_error_;     // 频率误差（ppm）
    double last_pps_time_;       // 上次1PPS输出时间
    std::mt19937 rng_;
};

// ESP32内部晶振模拟器
class ESP32Clock {
public:
    ESP32Clock(double initial_error_ppm,
               double temp_drift_ppm_per_c,
               double random_jitter_ppm);

    // 更新时钟状态
    void update(double delta_time_sec, double current_temperature_c);

    // 获取当前时间戳（基于有误差的时钟）
    double getTimestamp() const { return timestamp_; }

    // 获取当前频率误差（ppm）
    double getCurrentErrorPPM() const { return current_error_ppm_; }

    // 应用PLL校准（调整频率误差）
    void applyCalibration(double correction_ppm);

    // 重置状态
    void reset();

    // 获取自由运行的真实经过时间（用于测量）
    double getRealElapsedTime() const { return real_elapsed_time_; }

private:
    double initial_error_ppm_;       // 初始频率误差
    double temp_drift_ppm_per_c_;    // 温度漂移系数
    double random_jitter_ppm_;       // 随机抖动

    double timestamp_;               // 当前时间戳（有误差）
    double timestamp_compensation_;  // Kahan求和补偿（消除浮点累积误差）
    double real_elapsed_time_;       // 真实经过的时间（用于测量）
    double real_time_compensation_;  // 真实时间的Kahan补偿
    double current_error_ppm_;       // 当前总频率误差
    double calibration_ppm_;         // PLL校准值
    double last_temperature_;        // 上次温度

    std::mt19937 rng_;
    std::normal_distribution<double> jitter_dist_;
};

// GPIO中断延迟模拟器
class GPIOInterrupt {
public:
    GPIOInterrupt(double mean_delay_us, double jitter_us);

    // 模拟中断触发，返回实际触发时间（加上延迟和抖动）
    double triggerInterrupt(double actual_time);

private:
    double mean_delay_us_;
    double jitter_us_;
    std::mt19937 rng_;
    std::normal_distribution<double> delay_dist_;
};

#endif // CLOCK_SIMULATOR_H
