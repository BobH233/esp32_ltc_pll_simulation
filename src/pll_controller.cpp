#include "pll_controller.h"
#include "config.h"
#include <cmath>
#include <algorithm>
#include <limits>

PLLController::PLLController(double update_interval_sec,
                             double filter_alpha,
                             double proportional_gain,
                             int warmup_samples)
    : update_interval_sec_(update_interval_sec)
    , filter_alpha_(filter_alpha)
    , proportional_gain_(proportional_gain)
    , warmup_samples_(warmup_samples)
    , filtered_error_ppm_(0.0)
    , current_calibration_ppm_(0.0)
    , sample_count_(0)
    , sum_error_(0.0)
    , sum_squared_error_(0.0)
    , max_error_(-std::numeric_limits<double>::infinity())
    , min_error_(std::numeric_limits<double>::infinity())
{
}

double PLLController::processMeasurement(double measured_interval) {
    // 计算频率误差（ppm）
    // 期望间隔是1.0秒，实际测量的间隔可能有误差
    // error_ppm = (measured - expected) / expected * 1e6
    double error_ppm = (measured_interval - 1.0) / 1.0 * 1e6;

    // 记录统计数据
    error_history_.push_back(error_ppm);
    sum_error_ += error_ppm;
    sum_squared_error_ += error_ppm * error_ppm;
    max_error_ = std::max(max_error_, error_ppm);
    min_error_ = std::min(min_error_, error_ppm);

    // 一阶指数加权移动平均滤波
    if (sample_count_ == 0) {
        filtered_error_ppm_ = error_ppm;
    } else {
        filtered_error_ppm_ = filter_alpha_ * error_ppm +
                             (1.0 - filter_alpha_) * filtered_error_ppm_;
    }

    sample_count_++;

    // 预热期间使用较小的增益，避免过度反应
    double gain = isWarmedUp() ? proportional_gain_ : (proportional_gain_ * 0.3);

    // 积分控制：校准值累加增量
    // 注意：这里是负反馈，如果ESP32跑得快（正误差），需要减小频率（负校准）
    // PLL的本质是积分器，需要持续累加校准值来消除稳态误差
    current_calibration_ppm_ += gain * filtered_error_ppm_;

    // Anti-windup保护：限制校准值在合理范围内，避免积分饱和
    current_calibration_ppm_ = std::max(-Config::RMT_MAX_ADJUSTMENT_PPM,
                                       std::min(Config::RMT_MAX_ADJUSTMENT_PPM,
                                               current_calibration_ppm_));

    return current_calibration_ppm_;
}

void PLLController::reset() {
    filtered_error_ppm_ = 0.0;
    current_calibration_ppm_ = 0.0;
    sample_count_ = 0;
    error_history_.clear();
    sum_error_ = 0.0;
    sum_squared_error_ = 0.0;
    max_error_ = -std::numeric_limits<double>::infinity();
    min_error_ = std::numeric_limits<double>::infinity();
}

PLLController::Statistics PLLController::getStatistics() const {
    Statistics stats;
    stats.total_samples = sample_count_;

    if (sample_count_ == 0) {
        stats.mean_error_ppm = 0.0;
        stats.std_deviation_ppm = 0.0;
        stats.max_error_ppm = 0.0;
        stats.min_error_ppm = 0.0;
        return stats;
    }

    stats.mean_error_ppm = sum_error_ / sample_count_;

    // 标准差计算
    double variance = (sum_squared_error_ / sample_count_) -
                     (stats.mean_error_ppm * stats.mean_error_ppm);
    stats.std_deviation_ppm = std::sqrt(std::max(0.0, variance));

    stats.max_error_ppm = max_error_;
    stats.min_error_ppm = min_error_;

    return stats;
}
