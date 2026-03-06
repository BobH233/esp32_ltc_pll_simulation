# ESP32-S3 定时器精度测试

## 📋 测试目的

验证ESP32-S3的`esp_timer`是否满足LTC时码生成的精度要求。

这是整个LTC生成器项目的**基础验证**，必须先通过此测试才能继续后续开发。

## 🎯 验收标准

| 峰峰值抖动 | 评估结果 | 说明 |
|-----------|---------|------|
| < 2 μs | ✅ 优秀 | 完全满足LTC要求 |
| 2-5 μs | ✅ 合格 | 可用于LTC生成 |
| 5-10 μs | ⚠️ 警告 | 需要优化（关闭WiFi、CPU核心隔离） |
| > 10 μs | ❌ 不合格 | 需要外部TCXO方案 |

## 🔌 硬件连接

```
ESP32-S3 开发板
├─ GPIO 1  → 示波器/逻辑分析仪通道1（定时器输出）
├─ GPIO 2  → 指示灯/示波器通道2（测试状态）
└─ GND     → 示波器GND
```

## 🚀 编译和烧录

### 1. 环境准备

```bash
# 确保已安装ESP-IDF
. $HOME/esp/esp-idf/export.sh

# 进入测试目录
cd hardware_tests/01_timer_jitter_test
```

### 2. 配置目标芯片

```bash
idf.py set-target esp32s3
```

### 3. 编译

```bash
idf.py build
```

### 4. 烧录并监控

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

按 `Ctrl+]` 退出监控。

## 📊 测试结果解读

### 示例输出

```
╔═══════════════════════════════════════════════════════════════╗
║          ESP32-S3 定时器精度测试报告                          ║
╚═══════════════════════════════════════════════════════════════╝

【测试配置】
  定时器周期:       100 μs (10.0 kHz)
  采样数量:         100000
  预热样本:         1000
  有效样本:         99000
  测试时长:         10.0 秒

【抖动统计】（相对于100 μs周期）
  平均值:           +0.125 μs (+125 ns)
  标准差:           0.487 μs (487 ns)
  最大正偏差:       +1.234 μs (+1234 ns)
  最大负偏差:       -0.987 μs (-987 ns)
  峰峰值:           2.221 μs (2221 ns)

【验收标准评估】
  ✅ 优秀！峰峰值抖动 < 2 μs
  → 完全满足LTC时码生成要求

【对LTC时码的影响估算】
  24fps LTC bit周期:  520.833 μs
  抖动占比:           0.426%
  24小时累积误差:     ~20.7 ms
```

### 关键指标说明

1. **峰峰值（Peak-to-Peak）**：最重要的指标
   - 表示定时器周期的最大波动范围
   - 直接影响LTC信号的边沿精度

2. **标准差（Standard Deviation）**：
   - 表示抖动的分散程度
   - 标准差越小，定时器越稳定

3. **平均值（Mean）**：
   - 理想值应接近0
   - 非零值表示系统性偏差（可通过校准消除）

## 📈 示波器验证

### 测试方法

1. 连接示波器到GPIO 1
2. 触发模式：边沿触发，上升沿
3. 时基：20 μs/div
4. 测量项：
   - 频率（应为10 kHz）
   - 周期（应为100 μs）
   - 抖动（应 < 2 μs）

### 预期波形

```
GPIO 1 输出（10 kHz方波）：
     ___     ___     ___
____|   |___|   |___|   |___
    |<-100μs->|

理想情况：每个周期完全相等
实际情况：存在微小抖动（<2μs）
```

## 🔧 故障排查

### 问题1：抖动 > 10 μs

可能原因：
- WiFi/BLE未完全禁用
- 看门狗中断干扰
- flash缓存miss

解决方法：
```bash
# 检查sdkconfig
idf.py menuconfig
# → Component config → ESP System Settings
#   → 关闭 WiFi/BT
#   → 关闭 Task WDT
```

### 问题2：编译错误

```
error: 'esp_timer_get_time' was not declared
```

解决：确保 ESP-IDF 版本 ≥ 4.4

```bash
cd $IDF_PATH
git describe --tags
# 应输出 v5.x 或 v4.4+
```

### 问题3：内存分配失败

```
[ERROR] 内存分配失败！
```

解决：减小 `SAMPLE_COUNT` 或增加 heap size

```c
// timer_jitter_test.cpp
#define SAMPLE_COUNT  50000  // 从100000减小到50000
```

## 📖 下一步

如果测试通过（抖动 < 5 μs）：
- ✅ 继续开发 **NCO频率合成器**（测试02）
- ✅ 实现 **PLL控制器**（测试03）

如果测试未通过（抖动 > 10 μs）：
- ❌ 考虑使用外部TCXO（温补晶振）
- ❌ 或改用硬件定时器（GPTIMER）

## 📚 相关文档

- [ESP-IDF esp_timer 文档](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/esp_timer.html)
- [ESP32-S3 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)
- [LTC时码标准 SMPTE 12M](https://en.wikipedia.org/wiki/Linear_timecode)
