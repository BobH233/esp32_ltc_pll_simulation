/**
 * ESP32-S3 LTC时码生成器 - DS3231 RTC 测试
 *
 * 硬件: ESP32-S3-N16R8 (16MB Flash + 8MB PSRAM)
 * 框架: ESP-IDF (via PlatformIO)
 *
 * 目标：验证 DS3231 RTC 模块的功能和 SQW 输出精度
 *
 * 测试内容：
 * 1. I2C 总线扫描（检测 DS3231 地址 0x68）
 * 2. 读取 DS3231 时间寄存器
 * 3. 配置 SQW 输出为 1 Hz
 * 4. 测量 SQW 频率精度（目标：±2 ppm）
 * 5. 长期稳定性测试（60 秒）
 *
 * 硬件连接：
 * - GPIO 8:  I2C SDA
 * - GPIO 9:  I2C SCL
 * - GPIO 10: DS3231 SQW (1 Hz 输入，用于测量)
 * - GPIO 2:  测试状态指示
 *
 * 验收标准：
 * - I2C 通信成功（能读取时间）
 * - SQW 频率误差 < 5 ppm（目标频率 1.000000 Hz）
 * - 60 秒内无漂移
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
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_attr.h"

static const char *TAG = "DS3231_TEST";

// ==================== 配置参数 ====================

// GPIO 定义
#define I2C_SDA_GPIO            GPIO_NUM_8   // I2C 数据线
#define I2C_SCL_GPIO            GPIO_NUM_9   // I2C 时钟线
#define SQW_INPUT_GPIO          GPIO_NUM_10  // DS3231 SQW 输入（1 Hz）
#define TEST_STATUS_GPIO        GPIO_NUM_2   // 测试状态指示

// I2C 配置
#define I2C_MASTER_NUM          I2C_NUM_0    // I2C 端口号
#define I2C_MASTER_FREQ_HZ      100000       // I2C 频率：100 kHz（标准模式）
#define I2C_MASTER_TIMEOUT_MS   1000         // I2C 超时

// DS3231 配置
#define DS3231_I2C_ADDR         0x68         // DS3231 I2C 地址
#define DS3231_REG_SECONDS      0x00         // 秒寄存器
#define DS3231_REG_CONTROL      0x0E         // 控制寄存器
#define DS3231_REG_STATUS       0x0F         // 状态寄存器
#define DS3231_REG_TEMP_MSB     0x11         // 温度寄存器（高字节）

// 测试参数
#define TEST_DURATION_SEC       60           // 测试时长：60 秒
#define FREQ_MEASURE_INTERVAL   10           // 每 10 秒测量一次频率

// ==================== 全局变量 ====================

// SQW 边沿计数
static volatile uint64_t sqw_edge_count = 0;

// 频率测量
static int64_t last_measure_time = 0;
static uint64_t last_edge_count = 0;

// 统计数据
struct SQWStatistics {
    double target_freq_hz;       // 目标频率（1.0 Hz）
    double measured_freq_hz;     // 实测频率
    double freq_error_ppm;       // 频率误差（ppm）
    uint64_t total_edges;        // 总边沿数
    double test_duration_sec;    // 测试时长
};

// ==================== I2C 函数 ====================

/**
 * 初始化 I2C 总线
 */
static esp_err_t i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,   // 启用内部上拉（备用）
        .scl_pullup_en = GPIO_PULLUP_ENABLE,   // 启用内部上拉（备用）
        .master = {
            .clk_speed = I2C_MASTER_FREQ_HZ,
        },
        .clk_flags = 0,
    };

    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) {
        return err;
    }

    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

/**
 * I2C 总线扫描
 */
static void i2c_scan(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "开始 I2C 总线扫描...");
    ESP_LOGI(TAG, "扫描范围: 0x03 - 0x77");
    ESP_LOGI(TAG, "");

    int devices_found = 0;

    for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);

        esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
        i2c_cmd_link_delete(cmd);

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "✅ 发现设备：0x%02X", addr);
            devices_found++;

            if (addr == DS3231_I2C_ADDR) {
                ESP_LOGI(TAG, "   → 这是 DS3231 RTC！");
            }
        }
    }

    ESP_LOGI(TAG, "");
    if (devices_found == 0) {
        ESP_LOGW(TAG, "❌ 未发现任何 I2C 设备");
        ESP_LOGW(TAG, "   请检查：");
        ESP_LOGW(TAG, "   1. SDA/SCL 接线是否正确");
        ESP_LOGW(TAG, "   2. DS3231 模块是否供电");
        ESP_LOGW(TAG, "   3. 上拉电阻是否存在（4.7kΩ）");
    } else {
        ESP_LOGI(TAG, "扫描完成，共发现 %d 个设备", devices_found);
    }
    ESP_LOGI(TAG, "");
}

/**
 * 读取 DS3231 寄存器
 */
static esp_err_t ds3231_read_register(uint8_t reg_addr, uint8_t *data, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_start(cmd);  // 重复起始位
    i2c_master_write_byte(cmd, (DS3231_I2C_ADDR << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    return ret;
}

/**
 * 写入 DS3231 寄存器
 */
static esp_err_t ds3231_write_register(uint8_t reg_addr, uint8_t data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    return ret;
}

/**
 * BCD 转十进制
 */
static inline uint8_t bcd_to_dec(uint8_t bcd) {
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

/**
 * 读取 DS3231 时间
 */
static void ds3231_read_time(void) {
    uint8_t data[7];
    esp_err_t ret = ds3231_read_register(DS3231_REG_SECONDS, data, 7);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ 读取时间失败：%s", esp_err_to_name(ret));
        return;
    }

    // 解析时间（BCD 格式）
    uint8_t seconds = bcd_to_dec(data[0] & 0x7F);
    uint8_t minutes = bcd_to_dec(data[1] & 0x7F);
    uint8_t hours = bcd_to_dec(data[2] & 0x3F);
    uint8_t day = bcd_to_dec(data[4] & 0x3F);
    uint8_t month = bcd_to_dec(data[5] & 0x1F);
    uint8_t year = bcd_to_dec(data[6]);

    ESP_LOGI(TAG, "DS3231 时间: 20%02d-%02d-%02d %02d:%02d:%02d",
             year, month, day, hours, minutes, seconds);
}

/**
 * 读取 DS3231 温度
 */
static void ds3231_read_temperature(void) {
    uint8_t data[2];
    esp_err_t ret = ds3231_read_register(DS3231_REG_TEMP_MSB, data, 2);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ 读取温度失败：%s", esp_err_to_name(ret));
        return;
    }

    // 温度 = MSB + (LSB >> 6) * 0.25
    int8_t temp_msb = (int8_t)data[0];
    uint8_t temp_lsb = data[1] >> 6;
    float temperature = temp_msb + temp_lsb * 0.25f;

    ESP_LOGI(TAG, "DS3231 温度: %.2f °C", temperature);
}

/**
 * 配置 DS3231 SQW 输出为 1 Hz
 */
static esp_err_t ds3231_config_sqw_1hz(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "配置 DS3231 SQW 输出...");

    // 读取当前控制寄存器
    uint8_t control;
    esp_err_t ret = ds3231_read_register(DS3231_REG_CONTROL, &control, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ 读取控制寄存器失败：%s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "当前控制寄存器: 0x%02X", control);

    // 配置控制寄存器
    // Bit 7: /EOSC = 0（启用振荡器）
    // Bit 6: BBSQW = 0（电池模式下禁用方波）
    // Bit 5: CONV = 0（不强制温度转换）
    // Bit 4-3: RS2/RS1 = 00（1 Hz 方波）
    // Bit 2: INTCN = 0（启用 SQW 输出）
    // Bit 1-0: A2IE/A1IE = 0（禁用闹钟中断）
    uint8_t new_control = 0x00;  // 所有位清零 = 1 Hz SQW

    ret = ds3231_write_register(DS3231_REG_CONTROL, new_control);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ 写入控制寄存器失败：%s", esp_err_to_name(ret));
        return ret;
    }

    // 验证写入
    ret = ds3231_read_register(DS3231_REG_CONTROL, &control, 1);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ SQW 配置成功");
        ESP_LOGI(TAG, "新控制寄存器: 0x%02X", control);
        ESP_LOGI(TAG, "SQW 输出: 1 Hz 方波");
    }

    ESP_LOGI(TAG, "");
    return ret;
}

// ==================== SQW 频率测量 ====================

/**
 * SQW 边沿中断处理函数（IRAM 中执行）
 */
static void IRAM_ATTR sqw_edge_isr_handler(void* arg) {
    // 计数边沿（上升沿或下降沿）
    uint64_t count = sqw_edge_count;
    count++;
    sqw_edge_count = count;
}

/**
 * 测量 SQW 频率
 */
static void measure_sqw_frequency(SQWStatistics* stats) {
    int64_t current_time = esp_timer_get_time();
    uint64_t current_edges = sqw_edge_count;

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
    ESP_LOGI(TAG, "SQW 频率测量: %.6f Hz (误差: %+.3f ppm, 总边沿: %llu)",
             measured_freq, freq_error_ppm, current_edges);

    // 更新基准
    last_measure_time = current_time;
    last_edge_count = current_edges;
}

// ==================== 测试函数 ====================

static void test_sqw_frequency(int duration_sec) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "SQW 频率稳定性测试");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "目标频率: 1.000000 Hz");
    ESP_LOGI(TAG, "测试时长: %d 秒", duration_sec);
    ESP_LOGI(TAG, "");

    // 初始化统计
    SQWStatistics stats = {
        .target_freq_hz = 1.0,
        .measured_freq_hz = 0.0,
        .freq_error_ppm = 0.0,
        .total_edges = 0,
        .test_duration_sec = (double)duration_sec
    };

    // 重置计数器
    sqw_edge_count = 0;
    last_measure_time = 0;
    last_edge_count = 0;

    // 配置 GPIO 为输入（上升沿中断）
    gpio_config_t io_conf = {};
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << SQW_INPUT_GPIO);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.intr_type = GPIO_INTR_ANYEDGE;  // 任意边沿触发（上升+下降）
    gpio_config(&io_conf);

    // 安装 GPIO ISR 服务
    gpio_install_isr_service(0);
    gpio_isr_handler_add(SQW_INPUT_GPIO, sqw_edge_isr_handler, NULL);

    // 启动测试
    gpio_set_level(TEST_STATUS_GPIO, 1);
    ESP_LOGI(TAG, "开始监听 SQW 信号...");

    int64_t test_start = esp_timer_get_time();

    // 测试循环
    int elapsed_sec = 0;
    while (elapsed_sec < duration_sec) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        elapsed_sec++;

        // 每 FREQ_MEASURE_INTERVAL 秒测量一次频率
        if (elapsed_sec % FREQ_MEASURE_INTERVAL == 0) {
            measure_sqw_frequency(&stats);
        }

        // 进度提示
        if (elapsed_sec % 5 == 0) {
            ESP_LOGI(TAG, "进度: %d / %d 秒 (边沿数: %llu)",
                     elapsed_sec, duration_sec, sqw_edge_count);
        }
    }

    // 停止测试
    gpio_set_level(TEST_STATUS_GPIO, 0);
    gpio_isr_handler_remove(SQW_INPUT_GPIO);

    int64_t test_end = esp_timer_get_time();
    double actual_duration = (test_end - test_start) / 1e6;

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "SQW 测试完成！");
    ESP_LOGI(TAG, "实际测试时长: %.3f 秒", actual_duration);

    // 打印完整报告
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "SQW 测试报告");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "目标频率:     %.6f Hz", stats.target_freq_hz);
    ESP_LOGI(TAG, "实测频率:     %.6f Hz", stats.measured_freq_hz);
    ESP_LOGI(TAG, "频率误差:     %+.6f Hz", stats.measured_freq_hz - stats.target_freq_hz);
    ESP_LOGI(TAG, "相对误差:     %+.3f ppm", stats.freq_error_ppm);
    ESP_LOGI(TAG, "总边沿数:     %llu", stats.total_edges);
    ESP_LOGI(TAG, "总周期数:     %llu", stats.total_edges / 2);

    // 验收判定
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "【验收标准评估】");
    if (fabs(stats.freq_error_ppm) < 2.0) {
        ESP_LOGI(TAG, "✅ 优秀！频率误差 < 2 ppm");
        ESP_LOGI(TAG, "→ DS3231 精度符合数据手册规格（±2 ppm）");
        ESP_LOGI(TAG, "→ 可作为 PLL 的高精度参考源");
    } else if (fabs(stats.freq_error_ppm) < 5.0) {
        ESP_LOGI(TAG, "✅ 合格！频率误差 < 5 ppm");
        ESP_LOGI(TAG, "→ DS3231 精度良好，可用于 PLL");
    } else if (fabs(stats.freq_error_ppm) < 10.0) {
        ESP_LOGI(TAG, "⚠️  警告：频率误差 5-10 ppm");
        ESP_LOGI(TAG, "→ 精度稍低，可能是温度影响");
    } else {
        ESP_LOGI(TAG, "❌ 不合格：频率误差 > 10 ppm");
        ESP_LOGI(TAG, "→ 检查 DS3231 模块或电池电量");
    }

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");
}

// ==================== 主程序 ====================

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║   ESP32-S3 DS3231 RTC Test                          ║");
    ESP_LOGI(TAG, "║   DS3231 通信 + SQW 频率测量 - PlatformIO 版本       ║");
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

    // ==================== 2. GPIO 初始化 ====================

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "初始化 GPIO...");

    gpio_config_t io_conf = {};
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << TEST_STATUS_GPIO);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);

    gpio_set_level(TEST_STATUS_GPIO, 0);

    // ==================== 3. I2C 初始化 ====================

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "初始化 I2C 总线...");
    ESP_LOGI(TAG, "  SDA: GPIO %d", I2C_SDA_GPIO);
    ESP_LOGI(TAG, "  SCL: GPIO %d", I2C_SCL_GPIO);
    ESP_LOGI(TAG, "  频率: %d Hz", I2C_MASTER_FREQ_HZ);

    esp_err_t ret = i2c_master_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ I2C 初始化失败：%s", esp_err_to_name(ret));
        ESP_LOGE(TAG, "测试终止！");
        return;
    }

    ESP_LOGI(TAG, "✅ I2C 初始化成功");

    // ==================== 4. I2C 总线扫描 ====================

    vTaskDelay(pdMS_TO_TICKS(500));
    i2c_scan();

    // ==================== 5. DS3231 通信测试 ====================

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║   开始 DS3231 通信测试                               ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");

    // 读取时间
    ESP_LOGI(TAG, "【测试 1/4】读取 DS3231 时间");
    ds3231_read_time();

    vTaskDelay(pdMS_TO_TICKS(1000));

    // 读取温度
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "【测试 2/4】读取 DS3231 温度");
    ds3231_read_temperature();

    vTaskDelay(pdMS_TO_TICKS(1000));

    // 配置 SQW
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "【测试 3/4】配置 SQW 输出为 1 Hz");
    ret = ds3231_config_sqw_1hz();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ SQW 配置失败，测试终止！");
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(2000));

    // SQW 频率测试
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "【测试 4/4】SQW 频率稳定性测试");
    test_sqw_frequency(TEST_DURATION_SEC);

    // ==================== 6. 测试完成 ====================

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║   所有 DS3231 测试完成！                             ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "💡 提示：");
    ESP_LOGI(TAG, "  1. 如果 SQW 频率误差 < 2 ppm → DS3231 工作正常");
    ESP_LOGI(TAG, "  2. 如果 I2C 扫描未发现设备 → 检查接线和供电");
    ESP_LOGI(TAG, "  3. 如果频率误差较大 → 检查电池电量或温度");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "下一步：实现 NCO + DS3231 的 PLL 闭环控制");
}
