#ifndef STATISTICS_H
#define STATISTICS_H

#include <vector>
#include <string>

// 仿真统计收集器
class SimulationStatistics {
public:
    SimulationStatistics();

    // 记录数据点
    void recordClockError(double time_hours, double error_ppm, bool pll_enabled);
    void recordTimecodeError(double time_hours, double error_ms);
    void recordPLLCalibration(double time_hours, double calibration_ppm);

    // 生成报告
    void printHourlyReport(double current_hour);
    void printFinalReport();

    // 获取最终统计数据（用于安静模式）
    struct FinalStatistics {
        double avg_error_ppm;
        double error_24h_ms;
        double frame_error;
        double improvement_factor;
    };
    FinalStatistics getFinalStatistics() const;

    // 导出数据（可选）
    void exportToCSV(const std::string& filename);

private:
    struct DataPoint {
        double time_hours;
        double value;
    };

    std::vector<DataPoint> clock_error_without_pll_;
    std::vector<DataPoint> clock_error_with_pll_;
    std::vector<DataPoint> timecode_error_;
    std::vector<DataPoint> pll_calibration_;

    // 统计辅助函数
    double calculateMean(const std::vector<DataPoint>& data) const;
    double calculateStdDev(const std::vector<DataPoint>& data) const;
    double calculateMax(const std::vector<DataPoint>& data) const;
    double calculateMin(const std::vector<DataPoint>& data) const;
};

#endif // STATISTICS_H
