#include "statistics.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <limits>

SimulationStatistics::SimulationStatistics() {
}

void SimulationStatistics::recordClockError(double time_hours, double error_ppm, bool pll_enabled) {
    if (pll_enabled) {
        clock_error_with_pll_.push_back({time_hours, error_ppm});
    } else {
        clock_error_without_pll_.push_back({time_hours, error_ppm});
    }
}

void SimulationStatistics::recordTimecodeError(double time_hours, double error_ms) {
    timecode_error_.push_back({time_hours, error_ms});
}

void SimulationStatistics::recordPLLCalibration(double time_hours, double calibration_ppm) {
    pll_calibration_.push_back({time_hours, calibration_ppm});
}

double SimulationStatistics::calculateMean(const std::vector<DataPoint>& data) const {
    if (data.empty()) return 0.0;
    double sum = 0.0;
    for (const auto& point : data) {
        sum += point.value;
    }
    return sum / data.size();
}

double SimulationStatistics::calculateStdDev(const std::vector<DataPoint>& data) const {
    if (data.size() < 2) return 0.0;
    double mean = calculateMean(data);
    double sum_squared_diff = 0.0;
    for (const auto& point : data) {
        double diff = point.value - mean;
        sum_squared_diff += diff * diff;
    }
    return std::sqrt(sum_squared_diff / (data.size() - 1));
}

double SimulationStatistics::calculateMax(const std::vector<DataPoint>& data) const {
    if (data.empty()) return 0.0;
    double max_val = -std::numeric_limits<double>::infinity();
    for (const auto& point : data) {
        max_val = std::max(max_val, point.value);
    }
    return max_val;
}

double SimulationStatistics::calculateMin(const std::vector<DataPoint>& data) const {
    if (data.empty()) return 0.0;
    double min_val = std::numeric_limits<double>::infinity();
    for (const auto& point : data) {
        min_val = std::min(min_val, point.value);
    }
    return min_val;
}

void SimulationStatistics::printHourlyReport(double current_hour) {
    std::cout << "\n========================================\n";
    std::cout << "小时报告 - " << std::fixed << std::setprecision(1)
              << current_hour << " 小时\n";
    std::cout << "========================================\n";

    // 获取最近一小时的数据
    auto get_recent_data = [current_hour](const std::vector<DataPoint>& data) {
        std::vector<DataPoint> recent;
        for (const auto& point : data) {
            if (point.time_hours >= current_hour - 1.0 && point.time_hours <= current_hour) {
                recent.push_back(point);
            }
        }
        return recent;
    };

    auto recent_pll_error = get_recent_data(clock_error_with_pll_);
    auto recent_calibration = get_recent_data(pll_calibration_);
    auto recent_timecode_error = get_recent_data(timecode_error_);

    std::cout << std::fixed << std::setprecision(4);

    if (!recent_pll_error.empty()) {
        std::cout << "时钟误差 (有PLL):\n";
        std::cout << "  平均值: " << calculateMean(recent_pll_error) << " ppm\n";
        std::cout << "  标准差: " << calculateStdDev(recent_pll_error) << " ppm\n";
        std::cout << "  最大值: " << calculateMax(recent_pll_error) << " ppm\n";
        std::cout << "  最小值: " << calculateMin(recent_pll_error) << " ppm\n";
    }

    if (!recent_calibration.empty()) {
        std::cout << "\nPLL校准值:\n";
        std::cout << "  当前值: " << recent_calibration.back().value << " ppm\n";
        std::cout << "  平均值: " << calculateMean(recent_calibration) << " ppm\n";
    }

    if (!recent_timecode_error.empty()) {
        std::cout << "\n时间码累积误差:\n";
        std::cout << "  当前值: " << recent_timecode_error.back().value << " ms\n";
        std::cout << "  最大值: " << calculateMax(recent_timecode_error) << " ms\n";
    }
}

void SimulationStatistics::printFinalReport() {
    std::cout << "\n\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║           仿真最终报告 - 24小时统计                          ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    std::cout << std::fixed << std::setprecision(4);

    // 没有PLL的情况
    if (!clock_error_without_pll_.empty()) {
        std::cout << "【无PLL校准】时钟误差统计:\n";
        std::cout << "  平均误差: " << calculateMean(clock_error_without_pll_) << " ppm\n";
        std::cout << "  标准差:   " << calculateStdDev(clock_error_without_pll_) << " ppm\n";
        std::cout << "  最大误差: " << calculateMax(clock_error_without_pll_) << " ppm\n";
        std::cout << "  最小误差: " << calculateMin(clock_error_without_pll_) << " ppm\n";

        // 计算24小时累积时间误差
        double mean_error_ppm = calculateMean(clock_error_without_pll_);
        double time_error_24h_ms = mean_error_ppm * 24.0 * 3600.0 * 1000.0 / 1e6;
        std::cout << "  24小时累积误差: " << time_error_24h_ms << " ms\n\n";
    }

    // 有PLL的情况
    if (!clock_error_with_pll_.empty()) {
        std::cout << "【有PLL校准】时钟误差统计:\n";
        std::cout << "  平均误差: " << calculateMean(clock_error_with_pll_) << " ppm\n";
        std::cout << "  标准差:   " << calculateStdDev(clock_error_with_pll_) << " ppm\n";
        std::cout << "  最大误差: " << calculateMax(clock_error_with_pll_) << " ppm\n";
        std::cout << "  最小误差: " << calculateMin(clock_error_with_pll_) << " ppm\n";

        double mean_error_ppm = calculateMean(clock_error_with_pll_);
        double time_error_24h_ms = mean_error_ppm * 24.0 * 3600.0 * 1000.0 / 1e6;
        std::cout << "  24小时累积误差: " << time_error_24h_ms << " ms\n\n";
    }

    // PLL校准值
    if (!pll_calibration_.empty()) {
        std::cout << "【PLL校准值】统计:\n";
        std::cout << "  最终校准值: " << pll_calibration_.back().value << " ppm\n";
        std::cout << "  平均校准值: " << calculateMean(pll_calibration_) << " ppm\n";
        std::cout << "  校准范围:   " << calculateMin(pll_calibration_) << " ~ "
                  << calculateMax(pll_calibration_) << " ppm\n\n";
    }

    // 时间码误差
    if (!timecode_error_.empty()) {
        std::cout << "【时间码解码】误差统计:\n";
        std::cout << "  最终累积误差: " << timecode_error_.back().value << " ms\n";
        std::cout << "  平均误差:     " << calculateMean(timecode_error_) << " ms\n";
        std::cout << "  最大误差:     " << calculateMax(timecode_error_) << " ms\n";
        std::cout << "  最小误差:     " << calculateMin(timecode_error_) << " ms\n\n";

        // 换算成帧误差（以24fps为例）
        double frame_error = std::abs(timecode_error_.back().value) / (1000.0 / 24.0);
        std::cout << "  最终误差(帧): " << std::setprecision(3) << frame_error << " 帧 @24fps\n";
    }

    // 改进程度评估
    if (!clock_error_without_pll_.empty() && !clock_error_with_pll_.empty()) {
        double improvement_ratio = std::abs(calculateMean(clock_error_without_pll_)) /
                                  std::abs(calculateMean(clock_error_with_pll_));
        std::cout << "\n【改进程度】:\n";
        std::cout << "  精度提升: " << std::setprecision(1) << improvement_ratio << "x\n";
    }

    std::cout << "\n" << std::string(60, '=') << "\n";
}

SimulationStatistics::FinalStatistics SimulationStatistics::getFinalStatistics() const {
    FinalStatistics stats;

    if (!clock_error_with_pll_.empty()) {
        stats.avg_error_ppm = calculateMean(clock_error_with_pll_);
        stats.error_24h_ms = stats.avg_error_ppm * 24.0 * 3600.0 * 1000.0 / 1e6;
    } else {
        stats.avg_error_ppm = 0.0;
        stats.error_24h_ms = 0.0;
    }

    if (!timecode_error_.empty()) {
        // 使用最终累积误差，换算成帧误差（以24fps为基准）
        stats.frame_error = std::abs(timecode_error_.back().value) / (1000.0 / 24.0);
    } else {
        stats.frame_error = 0.0;
    }

    if (!clock_error_without_pll_.empty() && !clock_error_with_pll_.empty()) {
        stats.improvement_factor = std::abs(calculateMean(clock_error_without_pll_)) /
                                   std::abs(calculateMean(clock_error_with_pll_));
    } else {
        stats.improvement_factor = 1.0;
    }

    return stats;
}

void SimulationStatistics::exportToCSV(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "无法打开文件: " << filename << std::endl;
        return;
    }

    file << "Time(hours),ClockError_WithPLL(ppm),PLL_Calibration(ppm),TimecodeError(ms)\n";

    size_t max_size = std::max({clock_error_with_pll_.size(),
                                pll_calibration_.size(),
                                timecode_error_.size()});

    for (size_t i = 0; i < max_size; i++) {
        if (i < clock_error_with_pll_.size()) {
            file << clock_error_with_pll_[i].time_hours << ","
                 << clock_error_with_pll_[i].value << ",";
        } else {
            file << ",,";
        }

        if (i < pll_calibration_.size()) {
            file << pll_calibration_[i].value << ",";
        } else {
            file << ",";
        }

        if (i < timecode_error_.size()) {
            file << timecode_error_[i].value;
        }

        file << "\n";
    }

    file.close();
    std::cout << "数据已导出到: " << filename << std::endl;
}
