#!/usr/bin/env python3
"""
LTC PLL仿真器 - 综合性能评估脚本

测试所有帧率模式在不同时间步长下的表现
"""

import subprocess
import os
import time
from datetime import datetime
import json

# 测试配置
FRAME_RATES = [
    ("23.976", "23.976 fps (HD视频制作标准)"),
    ("24", "24 fps (传统胶片电影)"),
    ("25", "25 fps (PAL制式 - 中国/欧洲)"),
    ("29.97", "29.97 fps NDF (NTSC制式)"),
    ("29.97df", "29.97 fps DF (NTSC丢帧模式)"),
    ("30", "30 fps (网络视频)")
]

TIME_STEPS = [10, 100]  # 微秒

DURATION_HOURS = 24  # 仿真时长

# 性能标准（根据商业级LTC时码器要求）
ACCEPTANCE_CRITERIA = {
    "max_avg_error_ppm": 1.0,      # 平均误差不超过1ppm
    "max_24h_error_ms": 100.0,     # 24小时累积误差不超过100ms
    "max_frame_error": 0.5,        # 帧误差不超过0.5帧
    "min_improvement": 100.0,      # 精度提升至少100x
    "min_decode_rate": 99.9        # 解码成功率至少99.9%
}

def run_simulation(fps, timestep, duration=24):
    """运行单次仿真，实时显示进度"""

    cmd = [
        "./ltc_simulator",
        "--fps", fps,
        "--timestep", str(timestep),
        "--duration", str(duration),
        "--progress"  # 显示进度 + 输出最终指标
    ]

    print(f"\n  {'='*56}")
    print(f"  测试: {fps} fps @ {timestep}μs")
    print(f"  {'='*56}")

    start_time = time.time()
    try:
        # 使用实时输出模式
        process = subprocess.Popen(
            cmd,
            cwd="build",
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1  # 行缓冲，实时输出
        )

        # 收集所有输出用于解析指标
        output_lines = []

        # 实时显示进度
        for line in process.stdout:
            line = line.rstrip()
            output_lines.append(line)

            # 显示进度信息
            if line.startswith("Progress:") or line.startswith("Simulating:") or line.startswith("Completed!"):
                print(f"  {line}")

        process.wait(timeout=3600)
        elapsed_time = time.time() - start_time

        if process.returncode != 0:
            stderr = process.stderr.read()
            print(f"  ❌ 错误: {stderr}")
            return None

        # 解析最终输出的KEY=VALUE指标
        metrics = {}
        for line in output_lines:
            if '=' in line and not line.startswith("Simulating"):
                key, value = line.split('=', 1)
                metrics[key] = value

        metrics['elapsed_time_sec'] = elapsed_time

        return metrics

    except subprocess.TimeoutExpired:
        process.kill()
        print(f"  ⏱️  超时 (>1小时)")
        return None
    except Exception as e:
        print(f"  ❌ 异常: {e}")
        return None

def evaluate_result(metrics):
    """评估仿真结果是否符合标准"""
    if not metrics:
        return False, "运行失败"

    try:
        avg_error = abs(float(metrics.get('AVG_ERROR_PPM', '999')))
        error_24h = abs(float(metrics.get('ERROR_24H_MS', '999')))
        frame_error = float(metrics.get('FRAME_ERROR', '999'))
        improvement = float(metrics.get('IMPROVEMENT', '1').replace('x', ''))
        decode_rate = float(metrics.get('DECODE_SUCCESS_RATE', '0').replace('%', ''))

        failures = []

        if avg_error > ACCEPTANCE_CRITERIA['max_avg_error_ppm']:
            failures.append(f"平均误差过大 ({avg_error:.4f} > {ACCEPTANCE_CRITERIA['max_avg_error_ppm']} ppm)")

        if error_24h > ACCEPTANCE_CRITERIA['max_24h_error_ms']:
            failures.append(f"24h累积误差过大 ({error_24h:.2f} > {ACCEPTANCE_CRITERIA['max_24h_error_ms']} ms)")

        if frame_error > ACCEPTANCE_CRITERIA['max_frame_error']:
            failures.append(f"帧误差过大 ({frame_error:.3f} > {ACCEPTANCE_CRITERIA['max_frame_error']} 帧)")

        if improvement < ACCEPTANCE_CRITERIA['min_improvement']:
            failures.append(f"精度提升不足 ({improvement:.1f}x < {ACCEPTANCE_CRITERIA['min_improvement']}x)")

        if decode_rate < ACCEPTANCE_CRITERIA['min_decode_rate']:
            failures.append(f"解码成功率过低 ({decode_rate:.2f}% < {ACCEPTANCE_CRITERIA['min_decode_rate']}%)")

        if failures:
            return False, "; ".join(failures)
        else:
            return True, "✓ 通过所有标准"

    except Exception as e:
        return False, f"数据解析错误: {e}"

def format_metrics(metrics):
    """格式化指标显示"""
    if not metrics:
        return "N/A"

    try:
        return (
            f"误差: {metrics.get('AVG_ERROR_PPM', 'N/A')} ppm | "
            f"24h: {metrics.get('ERROR_24H_MS', 'N/A')} ms | "
            f"帧: {metrics.get('FRAME_ERROR', 'N/A')} | "
            f"提升: {metrics.get('IMPROVEMENT', 'N/A')} | "
            f"解码: {metrics.get('DECODE_SUCCESS_RATE', 'N/A')}"
        )
    except:
        return "数据格式错误"

def main():
    print("╔════════════════════════════════════════════════════════════╗")
    print("║     LTC PLL仿真器 - 综合性能评估                            ║")
    print("╚════════════════════════════════════════════════════════════╝\n")

    print(f"测试配置:")
    print(f"  帧率: {len(FRAME_RATES)} 种")
    print(f"  时间步长: {TIME_STEPS}")
    print(f"  仿真时长: {DURATION_HOURS} 小时")
    print(f"  总测试数: {len(FRAME_RATES) * len(TIME_STEPS)} 个\n")

    print(f"性能标准:")
    print(f"  最大平均误差: {ACCEPTANCE_CRITERIA['max_avg_error_ppm']} ppm")
    print(f"  最大24h误差: {ACCEPTANCE_CRITERIA['max_24h_error_ms']} ms")
    print(f"  最大帧误差: {ACCEPTANCE_CRITERIA['max_frame_error']} 帧")
    print(f"  最小精度提升: {ACCEPTANCE_CRITERIA['min_improvement']}x")
    print(f"  最小解码率: {ACCEPTANCE_CRITERIA['min_decode_rate']}%\n")

    # 检查可执行文件
    if not os.path.exists("build/ltc_simulator"):
        print("❌ 错误: build/ltc_simulator 不存在")
        print("请先运行: cd build && cmake .. && make")
        return

    start_time = datetime.now()
    all_results = []
    passed_count = 0
    failed_count = 0

    # 运行所有测试
    for fps, fps_desc in FRAME_RATES:
        print(f"\n{'='*60}")
        print(f"测试帧率: {fps_desc}")
        print(f"{'='*60}")

        for timestep in TIME_STEPS:
            metrics = run_simulation(fps, timestep, DURATION_HOURS)
            passed, reason = evaluate_result(metrics)

            result_entry = {
                'fps': fps,
                'fps_desc': fps_desc,
                'timestep': timestep,
                'metrics': metrics,
                'passed': passed,
                'reason': reason
            }
            all_results.append(result_entry)

            if passed:
                print(f"  ✅ 通过")
                passed_count += 1
            else:
                print(f"  ❌ 失败: {reason}")
                failed_count += 1

            if metrics:
                print(f"  📊 {format_metrics(metrics)}")
                print(f"  ⏱️  用时: {metrics.get('elapsed_time_sec', 0):.1f} 秒")

    # 生成最终报告
    print("\n\n" + "="*60)
    print("最终评估报告")
    print("="*60 + "\n")

    print(f"测试时间: {start_time.strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"总测试数: {len(all_results)}")
    print(f"通过: {passed_count} ✅")
    print(f"失败: {failed_count} ❌")
    print(f"成功率: {100.0 * passed_count / len(all_results):.1f}%\n")

    # 详细结果表
    print("详细结果:")
    print(f"{'帧率':<20} {'步长':<10} {'状态':<8} {'平均误差':<12} {'24h误差':<12} {'帧误差':<10} {'提升':<10}")
    print("-" * 90)

    for r in all_results:
        status = "✅ 通过" if r['passed'] else "❌ 失败"
        m = r['metrics'] if r['metrics'] else {}

        print(f"{r['fps_desc']:<20} {r['timestep']:>4} μs   {status:<8} "
              f"{m.get('AVG_ERROR_PPM', 'N/A'):<12} "
              f"{m.get('ERROR_24H_MS', 'N/A'):<12} "
              f"{m.get('FRAME_ERROR', 'N/A'):<10} "
              f"{m.get('IMPROVEMENT', 'N/A'):<10}")

    # 导出JSON报告
    report_file = f"test_report_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
    with open(report_file, 'w', encoding='utf-8') as f:
        json.dump({
            'test_time': start_time.isoformat(),
            'duration_hours': DURATION_HOURS,
            'acceptance_criteria': ACCEPTANCE_CRITERIA,
            'summary': {
                'total': len(all_results),
                'passed': passed_count,
                'failed': failed_count,
                'success_rate': 100.0 * passed_count / len(all_results)
            },
            'results': all_results
        }, f, indent=2, ensure_ascii=False)

    print(f"\n详细报告已保存到: {report_file}")

    # 最终判断
    print("\n" + "="*60)
    if failed_count == 0:
        print("🎉 恭喜！所有测试通过，PLL算法符合商业级标准！")
        print("建议：可以进入硬件开发阶段")
    else:
        print(f"⚠️  有 {failed_count} 个测试未通过")
        print("建议：检查失败原因并优化算法")
    print("="*60 + "\n")

    return 0 if failed_count == 0 else 1

if __name__ == "__main__":
    exit(main())
