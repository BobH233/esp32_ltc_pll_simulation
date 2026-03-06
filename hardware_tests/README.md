# ESP32-S3 LTC时码生成器 - 硬件验证测试

## 📋 测试路线图

按顺序执行以下测试，每个测试通过后才能进行下一步：

### 阶段1：基础验证（第1-3天）

| 测试 | 目录 | 目的 | 成功标准 |
|-----|------|------|---------|
| 01 | `01_timer_jitter_test` | 验证esp_timer精度 | 抖动 < 5 μs |
| 02 | `02_nco_frequency_test` | 验证NCO频率合成精度 | 频率误差 < 0.1 ppm |
| 03 | `03_gpio_speed_test` | 验证GPIO翻转速度 | 上升/下降时间 < 100 ns |

### 阶段2：PLL集成（第4-7天）

| 测试 | 目录 | 目的 | 成功标准 |
|-----|------|------|---------|
| 04 | `04_ds3231_pps_test` | 验证DS3231 1PPS捕获 | 捕获延迟 < 10 μs |
| 05 | `05_pll_convergence_test` | 验证PLL收敛性能 | 收敛时间 < 60s |
| 06 | `06_temperature_drift_test` | 温度漂移跟踪测试 | 0-50°C跟踪误差 < 1 ppm |

### 阶段3：LTC信号生成（第8-14天）

| 测试 | 目录 | 目的 | 成功标准 |
|-----|------|------|---------|
| 07 | `07_manchester_encoding_test` | Manchester编码正确性 | 解码成功率 > 99.9% |
| 08 | `08_ltc_signal_quality_test` | LTC信号质量测试 | 符合SMPTE 12M标准 |
| 09 | `09_long_term_stability_test` | 24小时稳定性测试 | 累积误差 < 1帧 |
| 10 | `10_commercial_decoder_test` | 商业解码器兼容性 | 与Tentacle Sync等设备兼容 |

## 🚀 快速开始

### 环境准备

```bash
# 1. 安装ESP-IDF（v5.0或更高版本）
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
. ./export.sh

# 2. 克隆本项目
cd ~/Documents/Coding
git clone <your-repo-url> esp32_timecode_project
cd esp32_timecode_project/hardware_tests
```

### 运行第一个测试

```bash
cd 01_timer_jitter_test
idf.py set-target esp32s3
idf.py build flash monitor
```

## 🔌 硬件需求

### 必需设备

- ESP32-S3 开发板（推荐：ESP32-S3-DevKitC-1）
- USB-C数据线
- 逻辑分析仪/示波器（推荐：Saleae Logic或100MHz示波器）

### 可选设备

- DS3231 RTC模块（带电池）
- 温度箱（0-50°C可调）
- 商业LTC解码器（如Tentacle Sync E）
- GPS模块（时间基准对比）

## 📊 测试数据记录

建议为每个测试创建测试报告：

```
test_reports/
├── 01_timer_jitter_YYYYMMDD.txt
├── 02_nco_frequency_YYYYMMDD.txt
└── ...
```

## 🎓 预期结果总结

### 乐观情况（所有测试通过）

- ✅ 定时器抖动：1-2 μs
- ✅ NCO精度：0.1-0.2 ppm
- ✅ PLL稳态误差：< 0.2 ppm
- ✅ 24小时累积误差：< 20 ms（< 0.5帧@24fps）

**结论**：ESP32-S3完全满足商业级LTC生成器要求

### 悲观情况（部分测试未通过）

- ⚠️ 定时器抖动：5-10 μs
- ⚠️ 需要软件优化（关闭WiFi、CPU核心隔离）
- ⚠️ 或考虑外部TCXO方案

**结论**：需要额外优化，但方案仍可行

### 最坏情况（关键测试失败）

- ❌ 定时器抖动 > 10 μs
- ❌ NCO精度 > 1 ppm

**结论**：需要重新评估硬件方案（外部FPGA/TCXO）

## 🛠️ 调试技巧

### 查看实时日志

```bash
idf.py monitor
```

### 提高日志级别

```bash
idf.py menuconfig
# → Component config → Log output
#   → Default log verbosity → Verbose
```

### 导出GPIO波形

使用逻辑分析仪捕获数据后，可以用Python分析：

```python
import pandas as pd
import matplotlib.pyplot as plt

# 读取逻辑分析仪导出的CSV
data = pd.read_csv('gpio_capture.csv')
periods = data['Time'].diff()

plt.hist(periods, bins=100)
plt.xlabel('Period (μs)')
plt.ylabel('Count')
plt.title('Timer Period Distribution')
plt.show()
```

## 📚 参考资料

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- [SMPTE 12M LTC Standard](https://en.wikipedia.org/wiki/Linear_timecode)
- [NCO原理](https://en.wikipedia.org/wiki/Numerically_controlled_oscillator)
- [PLL控制理论](https://en.wikipedia.org/wiki/Phase-locked_loop)

## 💬 获取帮助

如果测试过程中遇到问题：

1. 检查 `README.md` 中的故障排查章节
2. 查看 ESP-IDF 官方文档
3. 在项目 Issues 中搜索类似问题
4. 提交新的 Issue（附上完整日志）

祝测试顺利！🎉
