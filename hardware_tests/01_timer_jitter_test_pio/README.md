# ESP32-S3 定时器精度测试 (PlatformIO 版本)

## 📋 测试目的

验证 ESP32-S3 的 `esp_timer` 是否满足 LTC 时码生成的精度要求。

这是整个 LTC 生成器项目的**基础验证**，必须先通过此测试才能继续后续开发。

## 🎯 验收标准

| 峰峰值抖动 | 评估结果 | 说明 |
|-----------|---------|------|
| < 2 μs | ✅ 优秀 | 完全满足LTC要求 |
| 2-5 μs | ✅ 合格 | 可用于LTC生成 |
| 5-10 μs | ⚠️ 警告 | 需要优化（关闭WiFi、CPU核心隔离） |
| > 10 μs | ❌ 不合格 | 需要外部TCXO方案 |

## 💻 硬件要求

- **开发板**: ESP32-S3-WROOM-1 (或兼容型号)
- **芯片**: ESP32-S3-N16R8
  - N16 = 16MB Flash
  - R8 = **8MB PSRAM** ✅
- **USB线**: USB-C 数据线
- **可选**: 示波器/逻辑分析仪（验证波形质量）

## 🔌 硬件连接

```
ESP32-S3-N16R8 开发板
├─ USB-C   → 电脑（供电 + 串口）
├─ GPIO 1  → 示波器/逻辑分析仪 通道1（定时器输出，10kHz方波）
├─ GPIO 2  → 示波器/逻辑分析仪 通道2（测试状态，高电平=测试中）
└─ GND     → 示波器 GND
```

**注意**: 如果没有示波器，也可以运行测试，串口会输出完整的统计报告。

## 🚀 快速开始（VSCode + PlatformIO）

### 1. 安装 PlatformIO

如果还未安装，在 VSCode 中：
1. 打开扩展面板 (Cmd+Shift+X / Ctrl+Shift+X)
2. 搜索 "PlatformIO IDE"
3. 点击 "Install"
4. 重启 VSCode

### 2. 打开项目

```bash
# 方法1: 命令行
cd hardware_tests/01_timer_jitter_test_pio
code .

# 方法2: VSCode 菜单
文件 → 打开文件夹 → 选择 01_timer_jitter_test_pio
```

### 3. 连接开发板

用 USB-C 线连接 ESP32-S3 到电脑。

**macOS 用户**: 可能需要安装 USB 驱动
```bash
# 检查串口
ls /dev/cu.* | grep usb
# 应看到类似: /dev/cu.usbserial-1420
```

### 4. 一键编译、上传、监控

VSCode 底部状态栏会显示 PlatformIO 工具栏：

| 按钮 | 功能 | 快捷键 |
|------|------|--------|
| ✓ (对勾) | 编译 | Cmd/Ctrl + Alt + B |
| → (箭头) | 上传到开发板 | Cmd/Ctrl + Alt + U |
| 🔌 (插头) | 打开串口监视器 | Cmd/Ctrl + Alt + S |

**推荐操作顺序**：
1. 点击 ✓ 编译（首次需要下载工具链，约 5-10 分钟）
2. 点击 → 上传到开发板
3. 点击 🔌 打开串口监视器，查看测试结果

**或使用命令面板** (Cmd/Ctrl + Shift + P)：
- `PlatformIO: Build` (编译)
- `PlatformIO: Upload` (上传)
- `PlatformIO: Serial Monitor` (监控)

## 📊 测试流程

### 启动画面

```
╔══════════════════════════════════════════════════════╗
║   ESP32-S3 LTC Timer Jitter Test                    ║
║   定时器精度验证 - PlatformIO 版本                   ║
╚══════════════════════════════════════════════════════╝

【硬件信息】
  芯片型号: ESP32-S3
  CPU 核心数: 2
  CPU 频率: 240 MHz
  Flash: 16 MB (external)

【内存信息】
✅ PSRAM 已启用
   总容量: 8 MB
   可用内存: 8192 KB
   模式: OPI (快速)
  Internal RAM: 384 KB

✅ 已从 PSRAM 分配 1171 KB
```

如果看到 `✅ PSRAM 已启用`，说明配置正确！

### 测试过程

```
========================================
开始定时器精度测试
========================================
周期: 100 μs, 采样数: 100000
预计测试时长: 10.0 秒

硬件连接:
  GPIO 1 → 示波器/逻辑分析仪（测试输出）
  GPIO 2 → LED/示波器（状态指示：高=测试中）

3秒后开始...
⏱️  定时器启动！

进度: 10000 / 100000 (10.0%)
进度: 20000 / 100000 (20.0%)
...
进度: 100000 / 100000 (100.0%)

✅ 数据采集完成！
实际测试时长: 10.000 秒
```

### 测试结果（示例）

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

╔═══════════════════════════════════════════════════════════════╗
║                       测试完成                                ║
╚═══════════════════════════════════════════════════════════════╝
```

## 🔍 PSRAM 验证方法

### 方法1: 查看串口日志（推荐）

运行测试后，在串口监视器中查找：

```
✅ PSRAM 已启用
   总容量: 8 MB
   可用内存: 8192 KB
   模式: OPI (快速)
```

如果看到这个输出，说明 PSRAM 工作正常！

### 方法2: 检查编译日志

在 PlatformIO 编译输出中查找：

```
Building in release mode
Configuring...
CONFIG_SPIRAM=y
CONFIG_SPIRAM_TYPE_ESPPSRAM64=y
```

### 如果未检测到 PSRAM

串口会显示警告：

```
⚠️  未检测到 PSRAM！
请检查:
  1. sdkconfig 中是否启用 CONFIG_SPIRAM=y
  2. 芯片型号是否包含 'R' (如 N16R8)
  3. platformio.ini 中的 psram_type 配置
```

**解决步骤**：
1. 确认芯片型号包含 'R'（如 ESP32-S3-N16**R**8）
2. 清理重新编译：
   ```
   PlatformIO: Clean
   PlatformIO: Build
   ```
3. 检查 `platformio.ini` 中是否有：
   ```ini
   board_build.psram_type = opi
   ```

## ⚠️ 常见问题和解决方案

### 问题1: 找不到串口设备

**症状**：
```
Error: Serial port not found
```

**解决方案**：

**macOS**：
```bash
# 1. 检查串口
ls /dev/cu.* | grep usb

# 2. 如果没有输出，安装 USB 驱动
# CP210x: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
# CH340: http://www.wch.cn/downloads/CH341SER_MAC_ZIP.html

# 3. 手动指定串口（编辑 platformio.ini）
upload_port = /dev/cu.usbserial-1420
monitor_port = /dev/cu.usbserial-1420
```

**Windows**：
```
查看设备管理器 → 端口(COM和LPT)
应看到 "USB Serial Port (COM3)" 之类的设备

如果显示黄色感叹号，安装驱动：
- CP210x: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
- CH340: http://www.wch.cn/downloads/CH341SER_EXE.html
```

### 问题2: 编译错误

**症状**：
```
fatal error: esp_psram.h: No such file or directory
```

**解决方案**：
确保 `platformio.ini` 中使用 ESP-IDF 框架：
```ini
framework = espidf  # 不是 arduino
```

### 问题3: 上传后没有输出

**症状**：
串口监视器一片空白

**解决方案**：
1. 检查波特率是否正确（应为 115200）
2. 按下开发板上的 **RESET 按钮**
3. 检查 USB 线是否支持数据传输（不是仅供电线）

### 问题4: 抖动 > 5 μs

**症状**：
```
⚠️ 警告：峰峰值抖动 5-10 μs
```

**优化清单**：
- [ ] 确认 WiFi/BT 已禁用（检查 `sdkconfig.defaults`）
- [ ] 确认 CPU 频率为 240 MHz
- [ ] 使用优质 USB 供电（避免电源噪声）
- [ ] 减少后台任务/中断
- [ ] 尝试使用外部 5V 电源供电（而非 USB）

### 问题5: 内存分配失败

**症状**：
```
❌ 内存分配失败！
```

**解决方案**：

1. **如果有 PSRAM**：检查 PSRAM 配置（见上文）

2. **如果确实没有 PSRAM**：减小样本数量

   编辑 `src/main.cpp`：
   ```cpp
   // 将这行从 100000 改为 50000
   #define SAMPLE_COUNT  50000
   ```

3. **重新编译上传**

## 📈 示波器验证（可选）

### 测试方法

1. 连接示波器到 GPIO 1
2. 设置：
   - 触发模式：边沿触发，上升沿
   - 时基：20 μs/div
   - 电压：1V/div
3. 测量项：
   - 频率（应为 10.000 kHz）
   - 周期（应为 100.0 μs）
   - 周期抖动（应 < 2 μs）

### 预期波形

```
GPIO 1 输出（10 kHz 方波）：
     ___     ___     ___     ___
____|   |___|   |___|   |___|   |___
    |<-100μs->|<-100μs->|

GPIO 2 输出（测试状态）：
_____________________________
                            |_____
                         测试完成后拉低
```

## 📊 结果解读

### 优秀结果 (< 2 μs)

```
✅ 优秀！峰峰值抖动 < 2 μs
→ 完全满足LTC时码生成要求
```

**结论**: ESP32-S3 的 esp_timer 完全满足要求，可以继续进行 NCO 和 PLL 开发。

### 合格结果 (2-5 μs)

```
✅ 合格！峰峰值抖动 < 5 μs
→ 可用于LTC时码生成
```

**结论**: 性能可接受，但需要在后续开发中注意优化。

### 警告结果 (5-10 μs)

```
⚠️ 警告：峰峰值抖动 5-10 μs
→ 建议优化（关闭WiFi/BLE，CPU核心隔离）
```

**结论**: 需要软件优化，检查是否有后台任务干扰。

### 不合格结果 (> 10 μs)

```
❌ 不合格：峰峰值抖动 > 10 μs
→ 需要考虑外部TCXO方案或其他定时源
```

**结论**: esp_timer 无法满足要求，需要考虑硬件方案（外部温补晶振）。

## 🎯 下一步

### 测试通过后

继续开发以下测试：

1. **02_nco_frequency_test** - NCO 频率合成验证（精度 < 0.2 ppm）
2. **03_ds3231_integration** - DS3231 RTC + PLL 集成
3. **04_ltc_generator** - 完整 LTC 时码生成器

### 测试未通过

根据上面的优化清单逐项排查，或考虑：
- 使用外部 TCXO（温补晶振）
- 改用 GPTIMER（硬件定时器）
- 咨询社区获取帮助

## 📚 技术细节

### 测试原理

1. **周期抖动测量**：
   - 使用 esp_timer 以 100 μs 周期触发回调
   - 在回调中记录精确时间戳
   - 计算每个周期与理想值的偏差

2. **统计分析**：
   - 平均值：系统性偏差（可通过校准消除）
   - 标准差：随机抖动程度
   - 峰峰值：最坏情况误差（决定性指标）

3. **对 LTC 的影响**：
   - 24fps LTC bit 周期 = 520.833 μs
   - 如果抖动 2 μs，占比仅 0.4%
   - 完全在 LTC 解码容差范围内（±5%）

### PSRAM 优化

- 使用 8MB PSRAM 存储 100,000 个样本（约 1.1 MB）
- 避免普通堆内存不足
- OPI 模式提供 80MHz 访问速度

### 编译优化

- `-O2` 编译优化（性能优先）
- 定时器回调标记为 `IRAM_ATTR`（在 RAM 中执行，避免 Flash 缓存 miss）
- FreeRTOS 1000 Hz tick（提高调度精度）

## 💬 获取帮助

如果测试过程中遇到问题：

1. 检查本文档的"常见问题"章节
2. 查看 PlatformIO 输出日志中的错误信息
3. 在项目 Issues 中搜索类似问题
4. 提交新的 Issue（附上完整日志和硬件信息）

## 📖 参考资料

- [ESP-IDF esp_timer 文档](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/esp_timer.html)
- [ESP32-S3 技术参考手册](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)
- [PlatformIO ESP32 平台文档](https://docs.platformio.org/en/latest/platforms/espressif32.html)
- [LTC 时码标准 SMPTE 12M](https://en.wikipedia.org/wiki/Linear_timecode)

祝测试顺利！🎉
