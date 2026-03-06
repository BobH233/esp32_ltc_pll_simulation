/**
 * ESP32-S3 LTC时码生成器 - 定时器精度验证测试
 *
 * 目标：验证esp_timer是否满足LTC时码生成的精度要求
 *
 * 测试内容：
 * 1. 周期抖动（Jitter）：每个周期与理想值的偏差
 * 2. 长期漂移（Wander）：累积误差
 * 3. 最坏情况延迟
 *
 * 硬件连接：
 * - GPIO 1: 定时器触发信号（用于示波器/逻辑分析仪测量）
 * - GPIO 2: 测量结束指示（高电平=测试进行中）
 *
 * 验收标准：
 * - 抖动（峰峰值） < 5μs  → ✅ 可用于LTC
 * - 抖动（峰峰值） < 2μs  → ✅ 优秀
 * - 抖动（峰峰值） > 10μs → ❌ 需要外部TCXO方案
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_attr.h"

static const char *TAG = "TIMER_JITTER_TEST";

// ==================== 配置参数 ====================

// GPIO定义
#define TEST_OUTPUT_GPIO        GPIO_NUM_1   // 定时器触发输出（测量用）
#define TEST_STATUS_GPIO        GPIO_NUM_2   // 测试状态指示

// 测试参数
#define TIMER_PERIOD_US         100          // 100μs = 10kHz（LTC生成所需频率）
#define SAMPLE_COUNT            100000       // 采集10万个样本（10秒数据）
#define WARMUP_SAMPLES          1000         // 预热样本数（排除启动抖动）

// 统计参数
#define HISTOGRAM_BINS          200          // 抖动分布直方图bins
#define HISTOGRAM_RANGE_US      10           // ±10μs范围

// ==================== 全局变量 ====================

// 测量数据（使用DMA-capable内存以提高性能）
static int64_t* sample_timestamps = nullptr;
static int32_t* jitter_data = nullptr;       // 抖动值（纳秒）
static volatile size_t sample_index = 0;
static volatile bool test_running = false;

// 统计数据
struct Statistics {
    int64_t mean_jitter_ns;
    int64_t std_deviation_ns;
    int64_t max_jitter_ns;
    int64_t min_jitter_ns;
    int64_t peak_to_peak_ns;
    double mean_jitter_us;
    double std_deviation_us;
    double peak_to_peak_us;
};

// ==================== 快速GPIO操作宏 ====================

#define GPIO_SET_HIGH(gpio_num)   GPIO.out_w1ts = (1ULL << (gpio_num))
#define GPIO_SET_LOW(gpio_num)    GPIO.out_w1tc = (1ULL << (gpio_num))
#define GPIO_TOGGLE(gpio_num)     (GPIO.out ^= (1ULL << (gpio_num)))

// ==================== 定时器回调（IRAM中执行，最小化延迟）====================

static void IRAM_ATTR timer_callback(void* arg) {
    // 1. 立即翻转GPIO（供示波器测量）
    GPIO_TOGGLE(TEST_OUTPUT_GPIO);

    // 2. 记录时间戳（仅在数据采集阶段）
    if (test_running && sample_index < SAMPLE_COUNT) {
        // 使用esp_timer_get_time()获取高精度时间戳（1μs分辨率）
        sample_timestamps[sample_index] = esp_timer_get_time();
        sample_index++;

        // 采集完成，停止测试
        if (sample_index >= SAMPLE_COUNT) {
            test_running = false;
            GPIO_SET_LOW(TEST_STATUS_GPIO);
        }
    }
}

// ==================== 数据分析函数 ====================

static void calculate_jitter(Statistics* stats) {
    ESP_LOGI(TAG, "开始计算抖动数据...");

    // 计算每个周期的抖动
    int64_t expected_period_us = TIMER_PERIOD_US;

    int64_t sum_jitter = 0;
    int64_t sum_squared_jitter = 0;
    int64_t max_jitter = INT64_MIN;
    int64_t min_jitter = INT64_MAX;

    size_t valid_samples = 0;

    for (size_t i = WARMUP_SAMPLES + 1; i < SAMPLE_COUNT; i++) {
        // 计算实际周期
        int64_t actual_period_us = sample_timestamps[i] - sample_timestamps[i-1];

        // 计算抖动（实际周期 - 期望周期）
        int64_t jitter_us = actual_period_us - expected_period_us;
        int64_t jitter_ns = jitter_us * 1000;  // 转换为纳秒

        jitter_data[valid_samples] = (int32_t)jitter_ns;

        // 累积统计
        sum_jitter += jitter_ns;
        sum_squared_jitter += jitter_ns * jitter_ns;

        if (jitter_ns > max_jitter) max_jitter = jitter_ns;
        if (jitter_ns < min_jitter) min_jitter = jitter_ns;

        valid_samples++;
    }

    // 计算统计值
    stats->mean_jitter_ns = sum_jitter / valid_samples;

    // 标准差
    int64_t variance = (sum_squared_jitter / valid_samples) -
                       (stats->mean_jitter_ns * stats->mean_jitter_ns);
    stats->std_deviation_ns = (int64_t)sqrt((double)variance);

    stats->max_jitter_ns = max_jitter;
    stats->min_jitter_ns = min_jitter;
    stats->peak_to_peak_ns = max_jitter - min_jitter;

    // 转换为微秒（便于阅读）
    stats->mean_jitter_us = stats->mean_jitter_ns / 1000.0;
    stats->std_deviation_us = stats->std_deviation_ns / 1000.0;
    stats->peak_to_peak_us = stats->peak_to_peak_ns / 1000.0;

    ESP_LOGI(TAG, "抖动计算完成，有效样本数: %zu", valid_samples);
}

static void print_histogram(int32_t* data, size_t count) {
    ESP_LOGI(TAG, "生成抖动分布直方图...");

    // 初始化直方图bins
    int histogram[HISTOGRAM_BINS] = {0};

    // 计算bin宽度（纳秒）
    int64_t bin_width_ns = (HISTOGRAM_RANGE_US * 2 * 1000) / HISTOGRAM_BINS;
    int64_t histogram_min_ns = -HISTOGRAM_RANGE_US * 1000;

    // 填充直方图
    size_t out_of_range = 0;
    for (size_t i = 0; i < count; i++) {
        int64_t jitter_ns = data[i];
        int bin_index = (jitter_ns - histogram_min_ns) / bin_width_ns;

        if (bin_index >= 0 && bin_index < HISTOGRAM_BINS) {
            histogram[bin_index]++;
        } else {
            out_of_range++;
        }
    }

    // 打印直方图（ASCII艺术）
    printf("\n");
    printf("=================================================================\n");
    printf("                   抖动分布直方图                                  \n");
    printf("=================================================================\n");
    printf("范围: ±%d μs, Bin宽度: %.2f μs\n",
           HISTOGRAM_RANGE_US, bin_width_ns / 1000.0);
    printf("-----------------------------------------------------------------\n");

    // 找到最大count用于归一化显示
    int max_count = 0;
    for (int i = 0; i < HISTOGRAM_BINS; i++) {
        if (histogram[i] > max_count) max_count = histogram[i];
    }

    // 只打印有显著数据的部分
    const int bar_width = 50;
    for (int i = 0; i < HISTOGRAM_BINS; i++) {
        if (histogram[i] > max_count * 0.001) {  // 只显示>0.1%的bins
            double jitter_us = (histogram_min_ns + i * bin_width_ns) / 1000.0;
            int bar_len = (histogram[i] * bar_width) / max_count;

            printf("%+7.2f μs [%5d] |", jitter_us, histogram[i]);
            for (int j = 0; j < bar_len; j++) printf("█");
            printf("\n");
        }
    }

    printf("-----------------------------------------------------------------\n");
    printf("超出范围样本数: %zu (%.2f%%)\n",
           out_of_range, (out_of_range * 100.0) / count);
    printf("=================================================================\n\n");
}

static void print_report(const Statistics* stats) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║          ESP32-S3 定时器精度测试报告                          ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");

    printf("【测试配置】\n");
    printf("  定时器周期:       %d μs (%.1f kHz)\n", TIMER_PERIOD_US, 1000.0 / TIMER_PERIOD_US);
    printf("  采样数量:         %d\n", SAMPLE_COUNT);
    printf("  预热样本:         %d\n", WARMUP_SAMPLES);
    printf("  有效样本:         %d\n", SAMPLE_COUNT - WARMUP_SAMPLES);
    printf("  测试时长:         %.1f 秒\n\n", (SAMPLE_COUNT * TIMER_PERIOD_US) / 1e6);

    printf("【抖动统计】（相对于%d μs周期）\n", TIMER_PERIOD_US);
    printf("  平均值:           %+.3f μs (%+" PRId64 " ns)\n",
           stats->mean_jitter_us, stats->mean_jitter_ns);
    printf("  标准差:           %.3f μs (%" PRId64 " ns)\n",
           stats->std_deviation_us, stats->std_deviation_ns);
    printf("  最大正偏差:       %+.3f μs (%+" PRId64 " ns)\n",
           stats->max_jitter_ns / 1000.0, stats->max_jitter_ns);
    printf("  最大负偏差:       %+.3f μs (%+" PRId64 " ns)\n",
           stats->min_jitter_ns / 1000.0, stats->min_jitter_ns);
    printf("  峰峰值:           %.3f μs (%" PRId64 " ns)\n\n",
           stats->peak_to_peak_us, stats->peak_to_peak_ns);

    printf("【验收标准评估】\n");
    if (stats->peak_to_peak_us < 2.0) {
        printf("  ✅ 优秀！峰峰值抖动 < 2 μs\n");
        printf("  → 完全满足LTC时码生成要求\n");
    } else if (stats->peak_to_peak_us < 5.0) {
        printf("  ✅ 合格！峰峰值抖动 < 5 μs\n");
        printf("  → 可用于LTC时码生成\n");
    } else if (stats->peak_to_peak_us < 10.0) {
        printf("  ⚠️  警告：峰峰值抖动 5-10 μs\n");
        printf("  → 边缘情况，建议优化（关闭WiFi/BLE，CPU核心隔离）\n");
    } else {
        printf("  ❌ 不合格：峰峰值抖动 > 10 μs\n");
        printf("  → 需要考虑外部TCXO方案或其他定时源\n");
    }

    printf("\n");
    printf("【对LTC时码的影响估算】\n");
    printf("  24fps LTC bit周期:  520.833 μs\n");
    printf("  抖动占比:           %.3f%%\n", (stats->peak_to_peak_us / 520.833) * 100);
    printf("  24小时累积误差:     ~%.1f ms\n",
           (stats->mean_jitter_us * 1920 * 86400) / 1000.0);
    printf("\n");

    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                       测试完成                                ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
}

// ==================== 主测试任务 ====================

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "ESP32-S3 LTC Timer Jitter Test 启动");
    ESP_LOGI(TAG, "编译时间: %s %s", __DATE__, __TIME__);

    // 打印芯片信息
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "芯片: %s, 核心数: %d, 频率: %d MHz",
             CONFIG_IDF_TARGET, chip_info.cores,
             esp_clk_cpu_freq() / 1000000);

    // ==================== 1. GPIO初始化 ====================

    ESP_LOGI(TAG, "初始化GPIO...");

    gpio_config_t io_conf = {};
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << TEST_OUTPUT_GPIO) | (1ULL << TEST_STATUS_GPIO);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);

    GPIO_SET_LOW(TEST_OUTPUT_GPIO);
    GPIO_SET_LOW(TEST_STATUS_GPIO);

    // ==================== 2. 分配内存 ====================

    ESP_LOGI(TAG, "分配测试缓冲区...");

    sample_timestamps = (int64_t*)malloc(SAMPLE_COUNT * sizeof(int64_t));
    jitter_data = (int32_t*)malloc((SAMPLE_COUNT - WARMUP_SAMPLES) * sizeof(int32_t));

    if (!sample_timestamps || !jitter_data) {
        ESP_LOGE(TAG, "内存分配失败！");
        return;
    }

    memset(sample_timestamps, 0, SAMPLE_COUNT * sizeof(int64_t));
    memset(jitter_data, 0, (SAMPLE_COUNT - WARMUP_SAMPLES) * sizeof(int32_t));

    ESP_LOGI(TAG, "缓冲区分配完成: %zu KB",
             (SAMPLE_COUNT * sizeof(int64_t) +
              (SAMPLE_COUNT - WARMUP_SAMPLES) * sizeof(int32_t)) / 1024);

    // ==================== 3. 创建定时器 ====================

    ESP_LOGI(TAG, "创建高精度定时器...");

    esp_timer_create_args_t timer_args = {
        .callback = &timer_callback,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,  // 使用专用任务（更高优先级）
        .name = "jitter_test_timer",
        .skip_unhandled_events = false
    };

    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));

    // ==================== 4. 开始测试 ====================

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "开始定时器精度测试");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "周期: %d μs, 采样数: %d", TIMER_PERIOD_US, SAMPLE_COUNT);
    ESP_LOGI(TAG, "预计测试时长: %.1f 秒", (SAMPLE_COUNT * TIMER_PERIOD_US) / 1e6);
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "请连接示波器/逻辑分析仪到 GPIO %d", TEST_OUTPUT_GPIO);
    ESP_LOGI(TAG, "GPIO %d = 高电平表示测试进行中", TEST_STATUS_GPIO);
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "3秒后开始...");

    vTaskDelay(pdMS_TO_TICKS(3000));

    // 开始测试
    GPIO_SET_HIGH(TEST_STATUS_GPIO);
    test_running = true;
    sample_index = 0;

    ESP_LOGI(TAG, "定时器启动！");
    int64_t test_start_time = esp_timer_get_time();

    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, TIMER_PERIOD_US));

    // 等待测试完成
    while (test_running) {
        vTaskDelay(pdMS_TO_TICKS(100));

        // 打印进度
        if (sample_index % 10000 == 0 && sample_index > 0) {
            ESP_LOGI(TAG, "进度: %zu / %d (%.1f%%)",
                     sample_index, SAMPLE_COUNT,
                     (sample_index * 100.0) / SAMPLE_COUNT);
        }
    }

    // 停止定时器
    esp_timer_stop(timer);
    int64_t test_end_time = esp_timer_get_time();

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "数据采集完成！");
    ESP_LOGI(TAG, "实际测试时长: %.3f 秒", (test_end_time - test_start_time) / 1e6);
    ESP_LOGI(TAG, "");

    // ==================== 5. 数据分析 ====================

    Statistics stats;
    calculate_jitter(&stats);

    // ==================== 6. 输出报告 ====================

    print_report(&stats);
    print_histogram(jitter_data, SAMPLE_COUNT - WARMUP_SAMPLES - 1);

    // ==================== 7. 清理 ====================

    ESP_LOGI(TAG, "测试完成，资源清理...");
    esp_timer_delete(timer);
    free(sample_timestamps);
    free(jitter_data);

    ESP_LOGI(TAG, "所有测试完成！系统将保持空闲状态。");
    ESP_LOGI(TAG, "提示：可以用示波器测量GPIO %d的波形质量", TEST_OUTPUT_GPIO);
}
