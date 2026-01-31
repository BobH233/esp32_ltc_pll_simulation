#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstring>
#include "config.h"
#include "clock_simulator.h"
#include "pll_controller.h"
#include "ltc_encoder.h"
#include "ltc_decoder.h"
#include "statistics.h"

// 解析帧率参数
Config::LTCFrameRate parseFrameRate(const char* arg) {
    if (strcmp(arg, "23.976") == 0 || strcmp(arg, "23976") == 0) return Config::LTCFrameRate::FPS_23976;
    if (strcmp(arg, "24") == 0) return Config::LTCFrameRate::FPS_24;
    if (strcmp(arg, "25") == 0) return Config::LTCFrameRate::FPS_25;
    if (strcmp(arg, "29.97") == 0 || strcmp(arg, "2997") == 0) return Config::LTCFrameRate::FPS_2997_NDF;
    if (strcmp(arg, "29.97df") == 0 || strcmp(arg, "2997df") == 0) return Config::LTCFrameRate::FPS_2997_DF;
    if (strcmp(arg, "30") == 0) return Config::LTCFrameRate::FPS_30;
    return Config::DEFAULT_FRAME_RATE;
}

void printUsage(const char* program_name) {
    std::cout << "用法: " << program_name << " [选项]\n\n";
    std::cout << "选项:\n";
    std::cout << "  --fps <帧率>       设置帧率: 23.976, 24, 25, 29.97, 29.97df, 30 (默认: 24)\n";
    std::cout << "  --timestep <微秒>  设置时间步长(微秒): 10 或 100 (默认: 10)\n";
    std::cout << "  --duration <小时>  设置仿真时长(小时) (默认: 24)\n";
    std::cout << "  --output <文件>    设置输出CSV文件名 (默认: simulation_results.csv)\n";
    std::cout << "  --quiet            安静模式，仅输出最终指标\n";
    std::cout << "  --progress         进度模式，显示每小时进度+最终指标\n";
    std::cout << "  --help             显示此帮助信息\n\n";
    std::cout << "示例:\n";
    std::cout << "  " << program_name << " --fps 25 --timestep 100\n";
    std::cout << "  " << program_name << " --fps 29.97df --timestep 10 --duration 24\n";
}

int main(int argc, char* argv[]) {
    // 默认参数
    Config::LTCFrameRate frame_rate = Config::DEFAULT_FRAME_RATE;
    double time_step_us = Config::SIMULATION_TIME_STEP_US;
    double duration_hours = Config::SIMULATION_DURATION_HOURS;
    std::string output_file = "simulation_results.csv";
    bool quiet_mode = false;
    bool progress_mode = false;

    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--fps") == 0 && i + 1 < argc) {
            frame_rate = parseFrameRate(argv[++i]);
        } else if (strcmp(argv[i], "--timestep") == 0 && i + 1 < argc) {
            time_step_us = std::atof(argv[++i]);
        } else if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
            duration_hours = std::atof(argv[++i]);
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        } else if (strcmp(argv[i], "--quiet") == 0) {
            quiet_mode = true;
        } else if (strcmp(argv[i], "--progress") == 0) {
            progress_mode = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        }
    }
    if (!quiet_mode && !progress_mode) {
        std::cout << "╔════════════════════════════════════════════════════════════╗\n";
        std::cout << "║     LTC时码器 PLL校准系统 - 可行性验证仿真器                ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

        // 初始化配置
        std::cout << "仿真参数:\n";
        std::cout << "  DS3231精度:       ±" << Config::DS3231_ACCURACY_PPM << " ppm\n";
        std::cout << "  ESP32初始误差:    ±" << Config::ESP32_INITIAL_ERROR_PPM << " ppm\n";
        std::cout << "  温度漂移系数:     " << Config::ESP32_TEMP_DRIFT_PPM_PER_C << " ppm/°C\n";
        std::cout << "  PLL更新周期:      " << Config::PLL_UPDATE_INTERVAL_SEC << " 秒\n";
        std::cout << "  LTC帧率:          " << Config::getFrameRateName(frame_rate) << " fps\n";
        std::cout << "  时间步长:         " << time_step_us << " μs\n";
        std::cout << "  仿真时长:         " << duration_hours << " 小时\n";
        std::cout << "\n开始仿真...\n\n";
    } else if (progress_mode) {
        // 进度模式：简洁的开头
        std::cout << "Simulating: " << Config::getFrameRateName(frame_rate)
                  << " fps @ " << time_step_us << "μs for " << duration_hours << "h\n";
    }

    // 创建时钟模拟器
    DS3231Clock ds3231(Config::DS3231_ACCURACY_PPM);
    ESP32Clock esp32_with_pll(Config::ESP32_INITIAL_ERROR_PPM,
                              Config::ESP32_TEMP_DRIFT_PPM_PER_C,
                              Config::ESP32_RANDOM_JITTER_PPM);
    ESP32Clock esp32_without_pll(Config::ESP32_INITIAL_ERROR_PPM,
                                 Config::ESP32_TEMP_DRIFT_PPM_PER_C,
                                 Config::ESP32_RANDOM_JITTER_PPM);

    // 创建GPIO中断模拟器
    GPIOInterrupt gpio_interrupt(Config::GPIO_INTERRUPT_DELAY_US,
                                 Config::GPIO_INTERRUPT_JITTER_US);

    // 创建PLL控制器
    PLLController pll(Config::PLL_UPDATE_INTERVAL_SEC,
                     Config::PLL_FILTER_ALPHA,
                     Config::PLL_PROPORTIONAL_GAIN,
                     Config::PLL_WARMUP_SAMPLES);

    // 创建LTC编解码器（使用命令行指定的帧率）
    LTCEncoder encoder(frame_rate);
    LTCDecoder decoder(frame_rate);
    encoder.setTimecode(0, 0, 0, 0);

    // 创建统计收集器
    SimulationStatistics stats;

    // 仿真循环变量
    double current_time_sec = 0.0;
    double simulation_duration_sec = duration_hours * 3600.0;
    double time_step_sec = time_step_us / 1e6;

    double last_pps_time = 0.0;
    double last_pps_esp32_time_with_pll = 0.0;
    double pps_count = 0;

    // PLL更新控制
    double last_pll_update_time = 0.0;
    std::vector<double> pps_measurements;  // 存储多次1PPS测量用于平均

    double next_stats_report_time = Config::STATS_REPORT_INTERVAL_SEC;

    double reference_timecode_timestamp = 0.0;  // 理想时间码时间戳
    double actual_timecode_timestamp = 0.0;     // 实际生成的时间码时间戳

    int ltc_frame_count = 0;
    double next_ltc_frame_time = 0.0;
    double ltc_frame_period = 1.0 / Config::getFrameRate(frame_rate);

    // 主仿真循环
    while (current_time_sec < simulation_duration_sec) {
        // 计算当前环境温度（模拟昼夜温度变化）
        double hours_elapsed = current_time_sec / 3600.0;
        double temperature_amplitude = 10.0; // ±10°C变化
        double current_temperature = Config::INITIAL_TEMPERATURE_C +
            temperature_amplitude * std::sin(2.0 * M_PI * hours_elapsed / 24.0);

        // 更新所有时钟
        ds3231.update(time_step_sec);
        esp32_with_pll.update(time_step_sec, current_temperature);
        esp32_without_pll.update(time_step_sec, current_temperature);

        // 检查DS3231的1Hz tick
        if (ds3231.checkOnePPSTick()) {
            pps_count++;
            double actual_pps_time = ds3231.getTimestamp();

            // 模拟GPIO中断延迟
            double interrupt_time = gpio_interrupt.triggerInterrupt(actual_pps_time);

            // ESP32在中断中读取自己的时间戳（基于有误差的晶振）
            // 注意：这里必须用getTimestamp()而不是getRealElapsedTime()
            // 因为真实硬件中ESP32只能读取自己的计时器，无法知道"真实时间"
            double esp32_interrupt_time = esp32_with_pll.getTimestamp();

            // 计算两次1PPS之间ESP32测量的时间间隔
            if (pps_count > 1) {
                double measured_interval = esp32_interrupt_time - last_pps_esp32_time_with_pll;

                // 存储测量值
                pps_measurements.push_back(measured_interval);

                // 检查是否到达PLL更新周期（5秒）
                if (current_time_sec - last_pll_update_time >= Config::PLL_UPDATE_INTERVAL_SEC) {
                    // 计算这个周期内所有测量的平均值
                    double sum = 0.0;
                    for (double m : pps_measurements) {
                        sum += m;
                    }
                    double avg_interval = sum / pps_measurements.size();

                    // PLL处理平均测量值
                    double calibration = pll.processMeasurement(avg_interval);

                    // 应用校准到ESP32时钟
                    esp32_with_pll.applyCalibration(calibration);

                    // 记录统计数据
                    if (pll.isWarmedUp()) {
                        double hours = current_time_sec / 3600.0;
                        stats.recordClockError(hours, esp32_with_pll.getCurrentErrorPPM(), true);
                        stats.recordClockError(hours, esp32_without_pll.getCurrentErrorPPM(), false);
                        stats.recordPLLCalibration(hours, calibration);
                    }

                    // 重置
                    last_pll_update_time = current_time_sec;
                    pps_measurements.clear();
                }
            }

            last_pps_esp32_time_with_pll = esp32_interrupt_time;
            last_pps_time = actual_pps_time;
        }

        // LTC时码生成（使用校准后的ESP32时钟）
        if (current_time_sec >= next_ltc_frame_time) {
            // 生成LTC帧
            auto ltc_bits = encoder.encodeFrame();

            // 解码验证
            LTCFrame decoded;
            bool decode_success = decoder.decodeBits(ltc_bits, decoded);

            // 更新时间码
            encoder.incrementFrame();
            ltc_frame_count++;

            // 计算时间码误差
            reference_timecode_timestamp = current_time_sec;
            actual_timecode_timestamp = esp32_with_pll.getTimestamp();
            double timecode_error_ms = (actual_timecode_timestamp - reference_timecode_timestamp) * 1000.0;

            if (pll.isWarmedUp()) {
                stats.recordTimecodeError(current_time_sec / 3600.0, timecode_error_ms);
            }

            // 下一帧时间（基于校准后的时钟）
            next_ltc_frame_time += ltc_frame_period;
        }

        // 定期输出统计报告
        if (current_time_sec >= next_stats_report_time) {
            double current_hour = current_time_sec / 3600.0;

            if (progress_mode) {
                // 进度模式：只输出简洁的进度信息
                std::cout << "Progress: " << (int)current_hour << "/"
                          << (int)duration_hours << " hours completed\n" << std::flush;
            } else if (!quiet_mode) {
                // 详细模式：输出完整报告
                stats.printHourlyReport(current_hour);
            }
            next_stats_report_time += Config::STATS_REPORT_INTERVAL_SEC;
        }

        // 前进时间
        current_time_sec += time_step_sec;
    }

    // 输出最终报告
    if (!quiet_mode && !progress_mode) {
        // 详细模式：完整报告
        std::cout << "\n仿真完成！\n";
        stats.printFinalReport();

        // 输出LTC解码统计
        auto decode_stats = decoder.getStatistics();
        std::cout << "\n【LTC解码统计】:\n";
        std::cout << "  总帧数:       " << decode_stats.total_frames << "\n";
        std::cout << "  成功解码:     " << decode_stats.successful_decodes << "\n";
        std::cout << "  同步错误:     " << decode_stats.sync_errors << "\n";
        std::cout << "  校验错误:     " << decode_stats.parity_errors << "\n";
        std::cout << "  成功率:       " << std::fixed << std::setprecision(2)
                  << decode_stats.success_rate << "%\n";

        std::cout << "\n仿真完成！查看 " << output_file << " 获取详细数据。\n";
    } else {
        // 安静模式或进度模式：输出关键指标（便于脚本解析）
        auto final_stats = stats.getFinalStatistics();

        if (progress_mode) {
            std::cout << "\nCompleted!\n";  // 进度模式额外输出完成提示
        }

        std::cout << "FRAME_RATE=" << Config::getFrameRateName(frame_rate) << "\n";
        std::cout << "TIME_STEP_US=" << time_step_us << "\n";
        std::cout << "AVG_ERROR_PPM=" << final_stats.avg_error_ppm << "\n";
        std::cout << "ERROR_24H_MS=" << final_stats.error_24h_ms << "\n";
        std::cout << "FRAME_ERROR=" << final_stats.frame_error << "\n";
        std::cout << "IMPROVEMENT=" << final_stats.improvement_factor << "x\n";

        auto decode_stats = decoder.getStatistics();
        std::cout << "DECODE_SUCCESS_RATE=" << decode_stats.success_rate << "%\n";
    }

    // 导出数据（可选，默认禁用以提高性能）
    // stats.exportToCSV(output_file);

    return 0;
}
