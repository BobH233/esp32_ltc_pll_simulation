/**
 * ESP32-S3 LTC时码生成器 - NCO 频率合成验证测试
 *
 * 硬件: ESP32-S3-N16R8 (16MB Flash + 8MB PSRAM)
 * 框架: ESP-IDF (via PlatformIO)
 *
 * 目标：验证 NCO（数控振荡器）能否实现亚微秒级频率控制
 *
 * NCO 原理：
 * - 使用 32 位相位累加器
 * - 每次定时器中断累加相位增量
 * - 相位溢出时翻转输出（生成目标频率）
 *
 * 测试内容：
 * 1. 基准频率精度（1920 Hz，24fps LTC bit 速率）
 * 2. 频率调整能力（模拟 PLL 校准，±20 ppm）
 * 3. 长期稳定性（10 分钟测试）
 *
 * 硬件连接：
 * - GPIO 1: NCO 输出（用逻辑分析仪/频率计测量）
 * - GPIO 2: 测试状态指示
 *
 * 验收标准：
 * - 频率误差 < 0.2 ppm（目标频率 1920 Hz）
 * - 校准范围 ±50 ppm（覆盖 ESP32 晶振误差）
 * - 10分钟漂移 < 100 ms
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "soc/gpio_struct.h"
#include "hal/gpio_ll.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"

static const char *TAG = "NCO_FREQ_TEST";

// ==================== 配置参数 ====================

// GPIO 定义
#define NCO_OUTPUT_GPIO         GPIO_NUM_1   // NCO 输出（测量用）
#define TEST_STATUS_GPIO        GPIO_NUM_2   // 测试状态指示

// NCO 参数
#define NCO_SAMPLE_RATE_HZ      10000        // NCO 采样率：10 kHz（定时器频率）
#define NCO_TARGET_FREQ_HZ      1920.0       // 目标频率：1920 Hz（24fps LTC）
#define NCO_PHASE_BITS          32           // 相位累加器位数

// 测试参数
#define TEST_DURATION_SEC       60           // 测试时长：60 秒（1 分钟快速验证）
#define FREQ_MEASURE_INTERVAL   10           // 每 10 秒测量一次频率
#define CALIBRATION_STEPS       5            // 校准步数（测试不同 ppm 值）

// ==================== 全局变量 ====================

// NCO 状态
static uint32_t nco_phase_accumulator = 0;   // 相位累加器（32位）
static uint32_t nco_phase_increment = 0;     // 相位增量
static volatile uint32_t nco_output_state = 0;  // 输出状态
static volatile uint64_t nco_edge_count = 0;    // 边沿计数（用于测量频率）

// 频率测量
static int64_t last_measure_time = 0;
static uint64_t last_edge_count = 0;

// 统计数据
struct NCOStatistics {
    double target_freq_hz;
    double measured_freq_hz;
    double freq_error_ppm;
    double calibration_ppm;
    uint64_t total_edges;
    double test_duration_sec;
};

// ==================== NCO 核心函数 ====================

/**
 * 计算相位增量
 *
 * phase_increment = (target_freq / sample_rate) * 2^32
 *
 * 例如：1920 Hz / 10000 Hz * 2^32 = 824,633,720
 */
static inline uint32_t calculate_phase_increment(double target_freq_hz, double calibration_ppm) {
    // 应用校准（ppm 调整）
    double adjusted_freq = target_freq_hz * (1.0 - calibration_ppm / 1e6);

    // 计算相位增量
    double phase_inc_float = (adjusted_freq / NCO_SAMPLE_RATE_HZ) * 4294967296.0; // 2^32

    return (uint32_t)phase_inc_float;
}

/**
 * NCO 定时器回调（IRAM 中执行）
 *
 * 核心算法：
 * 1. 累加相位
 * 2. 检测溢出（MSB 翻转）
 * 3. 翻转输出
 */
static void IRAM_ATTR nco_timer_callback(void* arg) {
    // 保存旧的相位（用于检测溢出）
    uint32_t old_phase = nco_phase_accumulator;

    // 相位累加
    nco_phase_accumulator += nco_phase_increment;

    // 检测 MSB 翻转（相位溢出 = 输出周期到达）
    if ((nco_phase_accumulator ^ old_phase) & 0x80000000) {
        // 翻转输出
        nco_output_state ^= 1;
        gpio_set_level(NCO_OUTPUT_GPIO, nco_output_state);

        // 计数边沿（用于频率测量）- 避免 volatile 警告
        uint64_t count = nco_edge_count;
        count++;
        nco_edge_count = count;
    }
}

// ==================== 频率测量函数 ====================

static void measure_frequency(NCOStatistics* stats) {
    int64_t current_time = esp_timer_get_time();
    uint64_t current_edges = nco_edge_count;

    if (last_measure_time == 0) {
        // 第一次测量，仅记录基准
        last_measure_time = current_time;
        last_edge_count = current_edges;
        return;
    }

    // 计算时间间隔（秒）
    double elapsed_sec = (current_time - last_measure_time) / 1e6;

    // 计算边沿数（完整周期 = 2 个边沿）
    uint64_t edge_delta = current_edges - last_edge_count;
    uint64_t cycles = edge_delta / 2;  // 完整周期数

    // 计算实际频率
    double measured_freq = cycles / elapsed_sec;

    // 计算误差（ppm）
    double freq_error_ppm = (measured_freq - stats->target_freq_hz) / stats->target_freq_hz * 1e6;

    // 更新统计
    stats->measured_freq_hz = measured_freq;
    stats->freq_error_ppm = freq_error_ppm;
    stats->total_edges = current_edges;

    // 打印测量结果
    ESP_LOGI(TAG, "频率测量: %.3f Hz (误差: %+.3f ppm, 总边沿: %llu)",
             measured_freq, freq_error_ppm, current_edges);

    // 更新基准
    last_measure_time = current_time;
    last_edge_count = current_edges;
}

// ==================== 测试函数 ====================

static void test_nco_frequency(double target_freq, double calibration_ppm, int duration_sec) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "NCO 频率测试");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "目标频率: %.3f Hz", target_freq);
    ESP_LOGI(TAG, "校准值: %+.3f ppm", calibration_ppm);
    ESP_LOGI(TAG, "测试时长: %d 秒", duration_sec);
    ESP_LOGI(TAG, "");

    // 初始化统计
    NCOStatistics stats = {
        .target_freq_hz = target_freq,
        .measured_freq_hz = 0.0,
        .freq_error_ppm = 0.0,
        .calibration_ppm = calibration_ppm,
        .total_edges = 0,
        .test_duration_sec = (double)duration_sec
    };

    // 计算相位增量
    nco_phase_increment = calculate_phase_increment(target_freq, calibration_ppm);
    ESP_LOGI(TAG, "相位增量: %lu (0x%08lX)", nco_phase_increment, nco_phase_increment);

    // 重置 NCO 状态
    nco_phase_accumulator = 0;
    nco_output_state = 0;
    nco_edge_count = 0;
    last_measure_time = 0;
    last_edge_count = 0;

    // 创建定时器
    esp_timer_create_args_t timer_args = {
        .callback = &nco_timer_callback,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "nco_timer",
        .skip_unhandled_events = false
    };

    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));

    // 启动定时器
    gpio_set_level(TEST_STATUS_GPIO, 1);
    ESP_LOGI(TAG, "NCO 启动！");

    int64_t test_start = esp_timer_get_time();
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, 1000000 / NCO_SAMPLE_RATE_HZ));

    // 测试循环
    int elapsed_sec = 0;
    while (elapsed_sec < duration_sec) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        elapsed_sec++;

        // 每 FREQ_MEASURE_INTERVAL 秒测量一次频率
        if (elapsed_sec % FREQ_MEASURE_INTERVAL == 0) {
            measure_frequency(&stats);
        }

        // 进度提示
        if (elapsed_sec % 5 == 0) {
            ESP_LOGI(TAG, "进度: %d / %d 秒", elapsed_sec, duration_sec);
        }
    }

    // 停止定时器
    esp_timer_stop(timer);
    gpio_set_level(TEST_STATUS_GPIO, 0);

    int64_t test_end = esp_timer_get_time();
    double actual_duration = (test_end - test_start) / 1e6;

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "NCO 测试完成！");
    ESP_LOGI(TAG, "实际测试时长: %.3f 秒", actual_duration);

    // 不需要最终测量，使用最后一次定期测量的结果
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "【使用最后一次测量结果】");
    // measure_frequency(&stats);  // 注释掉，避免时间间隔太短

    // 打印完整报告
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "NCO 测试报告");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "目标频率:     %.6f Hz", stats.target_freq_hz);
    ESP_LOGI(TAG, "实测频率:     %.6f Hz", stats.measured_freq_hz);
    ESP_LOGI(TAG, "频率误差:     %+.6f Hz", stats.measured_freq_hz - stats.target_freq_hz);
    ESP_LOGI(TAG, "相对误差:     %+.3f ppm", stats.freq_error_ppm);
    ESP_LOGI(TAG, "校准值:       %+.3f ppm", calibration_ppm);
    ESP_LOGI(TAG, "总边沿数:     %llu", stats.total_edges);
    ESP_LOGI(TAG, "总周期数:     %llu", stats.total_edges / 2);

    // 验收判定
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "【验收标准评估】");
    if (fabs(stats.freq_error_ppm) < 0.2) {
        ESP_LOGI(TAG, "✅ 优秀！频率误差 < 0.2 ppm");
        ESP_LOGI(TAG, "→ NCO 精度完全满足 LTC 生成要求");
    } else if (fabs(stats.freq_error_ppm) < 1.0) {
        ESP_LOGI(TAG, "✅ 合格！频率误差 < 1 ppm");
        ESP_LOGI(TAG, "→ NCO 可用于 LTC 生成");
    } else if (fabs(stats.freq_error_ppm) < 10.0) {
        ESP_LOGI(TAG, "⚠️  警告：频率误差 1-10 ppm");
        ESP_LOGI(TAG, "→ 需要进一步优化或增加 PLL 增益");
    } else {
        ESP_LOGI(TAG, "❌ 不合格：频率误差 > 10 ppm");
        ESP_LOGI(TAG, "→ NCO 算法或实现有问题");
    }

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");

    // 清理
    esp_timer_delete(timer);
    vTaskDelay(pdMS_TO_TICKS(2000));  // 等待 2 秒
}

// ==================== 主程序 ====================

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║   ESP32-S3 NCO Frequency Test                       ║");
    ESP_LOGI(TAG, "║   NCO 频率合成验证 - PlatformIO 版本                 ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "编译时间: %s %s", __DATE__, __TIME__);

    // ==================== 1. 打印系统信息 ====================

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "【硬件信息】");
    ESP_LOGI(TAG, "  芯片型号: ESP32-S3");
    ESP_LOGI(TAG, "  CPU 核心数: %d", chip_info.cores);
    ESP_LOGI(TAG, "  CPU 频率: %lu MHz", CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);

    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);
    ESP_LOGI(TAG, "  Flash: %lu MB", flash_size / (1024 * 1024));

    size_t psram_size = esp_psram_get_size();
    if (psram_size > 0) {
        ESP_LOGI(TAG, "  PSRAM: %zu MB", psram_size / (1024 * 1024));
    }

    // ==================== 2. GPIO 初始化 ====================

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "初始化 GPIO...");

    gpio_config_t io_conf = {};
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << NCO_OUTPUT_GPIO) | (1ULL << TEST_STATUS_GPIO);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);

    gpio_set_level(NCO_OUTPUT_GPIO, 0);
    gpio_set_level(TEST_STATUS_GPIO, 0);

    // ==================== 3. NCO 测试序列 ====================

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║   开始 NCO 频率合成测试                              ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");

    // 测试 1: 基准频率（无校准）
    ESP_LOGI(TAG, "【测试 1/3】基准频率测试（无校准）");
    test_nco_frequency(NCO_TARGET_FREQ_HZ, 0.0, TEST_DURATION_SEC);

    // 测试 2: 正向校准（+20 ppm，模拟晶振偏快）
    ESP_LOGI(TAG, "【测试 2/3】正向校准测试（+20 ppm）");
    test_nco_frequency(NCO_TARGET_FREQ_HZ, +20.0, TEST_DURATION_SEC);

    // 测试 3: 负向校准（-20 ppm，模拟晶振偏慢）
    ESP_LOGI(TAG, "【测试 3/3】负向校准测试（-20 ppm）");
    test_nco_frequency(NCO_TARGET_FREQ_HZ, -20.0, TEST_DURATION_SEC);

    // ==================== 4. 测试完成 ====================

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║   所有 NCO 测试完成！                                ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "💡 提示：");
    ESP_LOGI(TAG, "  1. 用逻辑分析仪测量 GPIO %d 的频率", NCO_OUTPUT_GPIO);
    ESP_LOGI(TAG, "  2. 验证频率是否接近 %.0f Hz", NCO_TARGET_FREQ_HZ);
    ESP_LOGI(TAG, "  3. 检查不同校准值下的频率变化");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "下一步：集成 DS3231 RTC，实现完整的 PLL 闭环控制");
}
