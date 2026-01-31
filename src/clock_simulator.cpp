#include "clock_simulator.h"
#include "config.h"
#include <cmath>
#include <chrono>

// ==================== DS3231Clock ====================

DS3231Clock::DS3231Clock(double accuracy_ppm)
    : accuracy_ppm_(accuracy_ppm)
    , timestamp_(0.0)
    , frequency_error_(0.0)
    , last_pps_time_(0.0)
{
    // 使用固定种子以确保可重复性（调试用）
    // 生产版本可以使用随机种子：std::chrono::system_clock::now()
    rng_.seed(12345);

    // 生成初始频率误差（在±accuracy_ppm范围内）
    std::uniform_real_distribution<double> dist(-accuracy_ppm_, accuracy_ppm_);
    frequency_error_ = dist(rng_);
}

void DS3231Clock::update(double delta_time_sec) {
    // 考虑频率误差的时间流逝
    double error_factor = 1.0 + (frequency_error_ / 1e6);
    timestamp_ += delta_time_sec * error_factor;
}

bool DS3231Clock::checkOnePPSTick() {
    if (timestamp_ - last_pps_time_ >= 1.0) {
        last_pps_time_ = timestamp_;
        return true;
    }
    return false;
}

void DS3231Clock::reset() {
    timestamp_ = 0.0;
    last_pps_time_ = 0.0;
}

// ==================== ESP32Clock ====================

ESP32Clock::ESP32Clock(double initial_error_ppm,
                       double temp_drift_ppm_per_c,
                       double random_jitter_ppm)
    : initial_error_ppm_(initial_error_ppm)
    , temp_drift_ppm_per_c_(temp_drift_ppm_per_c)
    , random_jitter_ppm_(random_jitter_ppm)
    , timestamp_(0.0)
    , timestamp_compensation_(0.0)
    , real_elapsed_time_(0.0)
    , real_time_compensation_(0.0)
    , current_error_ppm_(initial_error_ppm)
    , calibration_ppm_(0.0)
    , last_temperature_(Config::INITIAL_TEMPERATURE_C)
    , jitter_dist_(0.0, random_jitter_ppm)
{
    // 使用固定种子以确保可重复性
    rng_.seed(54321);
}

void ESP32Clock::update(double delta_time_sec, double current_temperature_c) {
    // 计算温度引起的漂移
    double temp_drift = (current_temperature_c - Config::INITIAL_TEMPERATURE_C)
                       * temp_drift_ppm_per_c_;

    // 添加随机抖动
    double jitter = jitter_dist_(rng_);

    // 总频率误差 = 初始误差 + 温度漂移 + 抖动 - PLL校准
    current_error_ppm_ = initial_error_ppm_ + temp_drift + jitter - calibration_ppm_;

    // 考虑频率误差的时间流逝 - 使用Kahan求和算法消除浮点累积误差
    double error_factor = 1.0 + (current_error_ppm_ / 1e6);
    double increment = delta_time_sec * error_factor;

    // Kahan补偿求和
    double y = increment - timestamp_compensation_;
    double t = timestamp_ + y;
    timestamp_compensation_ = (t - timestamp_) - y;
    timestamp_ = t;

    // 真实时间流逝（用于测量） - 也使用Kahan求和
    double y_real = delta_time_sec - real_time_compensation_;
    double t_real = real_elapsed_time_ + y_real;
    real_time_compensation_ = (t_real - real_elapsed_time_) - y_real;
    real_elapsed_time_ = t_real;

    last_temperature_ = current_temperature_c;
}

void ESP32Clock::applyCalibration(double correction_ppm) {
    // 应用RMT调整粒度限制
    double quantized_correction = std::round(correction_ppm / Config::RMT_TUNE_GRANULARITY_PPM)
                                  * Config::RMT_TUNE_GRANULARITY_PPM;

    // 限制最大调整范围
    quantized_correction = std::max(-Config::RMT_MAX_ADJUSTMENT_PPM,
                                   std::min(Config::RMT_MAX_ADJUSTMENT_PPM, quantized_correction));

    calibration_ppm_ = quantized_correction;
}

void ESP32Clock::reset() {
    timestamp_ = 0.0;
    timestamp_compensation_ = 0.0;
    real_elapsed_time_ = 0.0;
    real_time_compensation_ = 0.0;
    calibration_ppm_ = 0.0;
}

// ==================== GPIOInterrupt ====================

GPIOInterrupt::GPIOInterrupt(double mean_delay_us, double jitter_us)
    : mean_delay_us_(mean_delay_us)
    , jitter_us_(jitter_us)
    , delay_dist_(mean_delay_us, jitter_us)
{
    // 使用固定种子以确保可重复性
    rng_.seed(98765);
}

double GPIOInterrupt::triggerInterrupt(double actual_time) {
    // 添加延迟和抖动
    double delay = std::max(0.0, delay_dist_(rng_)) / 1e6; // 转换为秒
    return actual_time + delay;
}
