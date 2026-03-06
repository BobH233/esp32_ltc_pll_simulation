# DS3231 RTC 测试项目

## 📋 项目简介

本项目测试 DS3231 高精度 RTC 模块的以下功能：

1. **I2C 通信**：扫描总线、读取时间和温度
2. **SQW 输出**：配置并测量 1 Hz 方波的精度
3. **稳定性验证**：60 秒长期频率测量

---

## 🔌 硬件连接

### ESP32-S3 ↔ DS3231

| DS3231 引脚 | ESP32-S3 引脚 | 说明 |
|-------------|---------------|------|
| **VCC** | **3.3V** | 电源（⚠️ 不要接 5V） |
| **GND** | **GND** | 地线 |
| **SCL** | **GPIO 9** | I2C 时钟线 |
| **SDA** | **GPIO 8** | I2C 数据线 |
| **SQW** | **GPIO 10** | 1 Hz 方波输出 |
| **32K** | 不接 | 32 kHz 输出（暂不使用） |

### GPIO 定义

```cpp
#define I2C_SDA_GPIO        GPIO_NUM_8   // I2C 数据线
#define I2C_SCL_GPIO        GPIO_NUM_9   // I2C 时钟线
#define SQW_INPUT_GPIO      GPIO_NUM_10  // DS3231 SQW 输入（1 Hz）
#define TEST_STATUS_GPIO    GPIO_NUM_2   // 测试状态指示
```

---

## 🎯 测试内容

### 测试 1：I2C 总线扫描

- 扫描地址范围：0x03 - 0x77
- 预期发现：DS3231 (地址 0x68)

### 测试 2：读取时间

- 从寄存器 0x00 开始读取 7 个字节
- BCD 格式解码：年/月/日 时:分:秒

### 测试 3：读取温度

- 从寄存器 0x11 读取温度（分辨率 0.25°C）
- 验证 DS3231 内部 TCXO 工作状态

### 测试 4：SQW 频率测量

- 配置控制寄存器 (0x0E) 为 0x00 → 1 Hz 输出
- 使用 GPIO 中断计数边沿
- 每 10 秒计算一次频率和误差（ppm）

---

## 📊 验收标准

| 测试项 | 标准 | 说明 |
|--------|------|------|
| **I2C 通信** | 成功读取时间 | 通信正常 |
| **SQW 频率** | < 2 ppm | 符合数据手册（±2 ppm @ 25°C） |
| **长期稳定性** | 60 秒无漂移 | 适合作为 PLL 参考 |

---

## 🚀 编译和上传

### 方法 1：PlatformIO CLI

```bash
cd hardware_tests/03_ds3231_sqw_test_pio

# 编译
pio run

# 上传并监视
pio run --target upload --target monitor
```

### 方法 2：VSCode

1. 打开 VSCode
2. 点击底部 PlatformIO 图标
3. 选择 **Upload and Monitor**

---

## 📝 预期输出示例

```
I (1234) DS3231_TEST: 开始 I2C 总线扫描...
I (1250) DS3231_TEST: ✅ 发现设备：0x68
I (1251) DS3231_TEST:    → 这是 DS3231 RTC！

I (1500) DS3231_TEST: DS3231 时间: 2026-02-01 15:30:45
I (2500) DS3231_TEST: DS3231 温度: 24.50 °C

I (3000) DS3231_TEST: ✅ SQW 配置成功
I (3001) DS3231_TEST: SQW 输出: 1 Hz 方波

I (13000) DS3231_TEST: SQW 频率测量: 1.000002 Hz (误差: +2.0 ppm, 总边沿: 20)
I (23000) DS3231_TEST: SQW 频率测量: 0.999998 Hz (误差: -2.0 ppm, 总边沿: 40)

I (63000) DS3231_TEST: ✅ 优秀！频率误差 < 2 ppm
I (63001) DS3231_TEST: → DS3231 精度符合数据手册规格（±2 ppm）
```

---

## ⚠️ 故障排除

### 问题 1：未发现 I2C 设备

**现象**：扫描结果显示"未发现任何 I2C 设备"

**检查**：
1. SDA/SCL 接线是否正确（GPIO 8/9）
2. DS3231 VCC 是否连接到 3.3V
3. GND 是否连接
4. 上拉电阻是否存在（模块通常内置）

**解决**：
- 用万用表测量 VCC 和 GND 之间的电压（应为 3.3V）
- 检查杜邦线是否接触良好

---

### 问题 2：SQW 频率误差过大

**现象**：测量误差 > 10 ppm

**可能原因**：
1. DS3231 电池电量不足
2. 温度变化大（DS3231 精度与温度相关）
3. SQW 引脚未连接

**解决**：
- 更换 CR2032 电池（新电池 ~3.0V）
- 等待温度稳定后再测试
- 确认 GPIO 10 连接到 DS3231 SQW 引脚

---

### 问题 3：读取时间全为零

**现象**：时间显示 `2000-00-00 00:00:00`

**原因**：DS3231 未初始化（首次使用）

**解决**：
- DS3231 首次使用需要设置时间（通过 I2C 写入）
- 或使用 Arduino 库先初始化一次

---

## 📚 相关资料

- [DS3231 数据手册](https://www.analog.com/media/en/technical-documentation/data-sheets/DS3231.pdf)
- [ESP-IDF I2C 驱动文档](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/i2c.html)
- [项目主 README](../../README.md)

---

## 🎯 下一步

测试成功后，进入下一阶段：

**04_pll_loop_test** - 实现 NCO + DS3231 的完整 PLL 闭环控制
