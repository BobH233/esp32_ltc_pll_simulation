#ifndef PLL_CONTROLLER_H
#define PLL_CONTROLLER_H

#include <vector>
#include <cstdint>

// PLL锁相环控制器
class PLLController {
public:
    PLLController(double update_interval_sec,
                  double filter_alpha,
                  double proportional_gain,
                  int warmup_samples);

    // 处理新的1PPS测量
    // measured_interval: ESP32测量的两次1PPS之间的时间间隔（秒）
    // 返回：应该应用的校准值（ppm）
    double processMeasurement(double measured_interval);

    // 获取当前校准值
    double getCurrentCalibration() const { return current_calibration_ppm_; }

    // 获取滤波后的误差
    double getFilteredError() const { return filtered_error_ppm_; }

    // 是否已经预热完成
    bool isWarmedUp() const { return sample_count_ >= warmup_samples_; }

    // 重置状态
    void reset();

    // 获取统计信息
    struct Statistics {
        double mean_error_ppm;
        double std_deviation_ppm;
        double max_error_ppm;
        double min_error_ppm;
        int total_samples;
    };
    Statistics getStatistics() const;

private:
    double update_interval_sec_;
    double filter_alpha_;           // 指数加权移动平均滤波系数
    double proportional_gain_;      // 比例增益
    int warmup_samples_;            // 预热样本数

    double filtered_error_ppm_;     // 滤波后的频率误差
    double current_calibration_ppm_; // 当前校准值
    int sample_count_;              // 已处理的样本数

    // 用于统计分析
    std::vector<double> error_history_;
    double sum_error_;
    double sum_squared_error_;
    double max_error_;
    double min_error_;
};

#endif // PLL_CONTROLLER_H
