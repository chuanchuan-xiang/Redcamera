#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
红外相机实时显示程序 - 使用封装的 SDK
展示如何使用 thermal_camera_sdk 进行简单开发

作者：AI Assistant
日期：2025-11-10
"""

import sys
import time
import cv2
import numpy as np
from thermal_camera_sdk import ThermalCameraSDK


def main():
    """主函数"""
    
    print("=" * 70)
    print("红外相机实时显示 - SDK 版本")
    print("=" * 70)
    print()
    
    # 创建 SDK 实例（使用上下文管理器自动清理）
    with ThermalCameraSDK(sdk_path='sdk') as sdk:
        
        # 1. 初始化
        print("1. 初始化 SDK...")
        sdk.initialize()
        print(f"   版本: {sdk.get_version_info()}")
        print("   ✓ 初始化成功")
        
        # 2. 列出设备（可选）
        print("\n2. 扫描设备...")
        devices = sdk.list_devices()
        for dev in devices:
            print(f"   [{dev['index']}] VID:0x{dev['vid']:04X} PID:0x{dev['pid']:04X} - {dev['name']}")
        
        # 3. 打开相机（使用 256x384 模式进行数据分离）
        print("\n3. 打开相机...")
        camera_info = sdk.open_camera(use_384_mode=True)
        print(f"   分辨率: {camera_info['width']}x{camera_info['height']}")
        print(f"   温度帧: {camera_info['temp_width']}x{camera_info['temp_height']}")
        print(f"   帧率: {camera_info['fps']} FPS")
        print("   ✓ 相机打开成功")
        
        # 4. 启动视频流
        print("\n4. 启动视频流...")
        sdk.start_stream()
        print("   ✓ 视频流启动成功")
        
        # 5. 实时显示
        print("\n" + "=" * 70)
        print("开始实时显示（按 Q 或 ESC 退出）")
        print("=" * 70)
        print()
        
        # 创建显示窗口
        window_name = "Thermal Camera - SDK Version"
        cv2.namedWindow(window_name, cv2.WINDOW_NORMAL)
        cv2.resizeWindow(window_name, 768, 576)
        
        start_time = time.time()
        frame_count = 0
        
        try:
            while True:
                # 获取一帧数据
                frame_data = sdk.get_frame(max_retries=50)
                
                if frame_data is None:
                    print("\r获取帧失败，重试中...", end="", flush=True)
                    time.sleep(0.01)
                    continue
                
                frame_count += 1
                
                # 提取温度数据
                temp_celsius = frame_data['temperature_celsius']
                stats = frame_data['stats']
                
                # 只在第一帧打印详细信息
                if frame_count == 1:
                    print(f"\n首帧数据分析:")
                    print(f"  原始数据: {stats['raw_min']} - {stats['raw_max']} (平均: {stats['raw_avg']:.0f})")
                    print(f"  温度范围: {stats['temp_min']:.1f}°C - {stats['temp_max']:.1f}°C")
                    print(f"  平均温度: {stats['temp_avg']:.1f}°C")
                    
                    if 16000 <= stats['raw_avg'] <= 22000:
                        print(f"  ✓ 温度数据正常！")
                    else:
                        print(f"  ⚠️  温度数据可能异常")
                    print()
                
                # 获取中心点温度
                height, width = temp_celsius.shape
                center_temp = temp_celsius[height // 2, width // 2]
                
                # 归一化显示
                min_temp = stats['temp_min']
                max_temp = stats['temp_max']
                normalized = np.clip(
                    (temp_celsius - min_temp) / (max_temp - min_temp + 1e-6),
                    0, 1
                )
                gray = (normalized * 255).astype(np.uint8)
                
                # 应用伪彩色
                colored = cv2.applyColorMap(gray, cv2.COLORMAP_JET)
                
                # 放大显示
                display = cv2.resize(colored, (768, 576), interpolation=cv2.INTER_NEAREST)
                
                # 添加十字准星
                center_x, center_y = 768 // 2, 576 // 2
                cv2.line(display, (center_x - 30, center_y), (center_x + 30, center_y), 
                        (0, 255, 0), 2)
                cv2.line(display, (center_x, center_y - 30), (center_x, center_y + 30), 
                        (0, 255, 0), 2)
                
                # 显示中心温度
                cv2.putText(display, f"{center_temp:.1f}C", 
                           (center_x + 35, center_y + 5),
                           cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)
                
                # 计算 FPS
                elapsed = time.time() - start_time
                fps = frame_count / elapsed if elapsed > 0 else 0
                
                # 添加信息面板
                info_lines = [
                    f"Frame: {frame_count}",
                    f"FPS: {fps:.1f}",
                    f"Resolution: {width}x{height}",
                    "",
                    "Temperature:",
                    f"  Min: {stats['temp_min']:.1f} C",
                    f"  Max: {stats['temp_max']:.1f} C",
                    f"  Avg: {stats['temp_avg']:.1f} C",
                    f"  Center: {center_temp:.1f} C",
                ]
                
                # 绘制半透明背景
                overlay = display.copy()
                cv2.rectangle(overlay, (5, 5), (280, 240), (0, 0, 0), -1)
                cv2.addWeighted(overlay, 0.7, display, 0.3, 0, display)
                
                # 绘制文本
                y_pos = 25
                for line in info_lines:
                    cv2.putText(display, line, (10, y_pos), 
                               cv2.FONT_HERSHEY_SIMPLEX, 0.45, (0, 255, 0), 1, cv2.LINE_AA)
                    y_pos += 20 if line else 10
                
                # 显示
                cv2.imshow(window_name, display)
                
                # 终端输出
                print(f"\r帧 {frame_count:4d} | "
                      f"温度: {stats['temp_min']:5.1f} - {stats['temp_max']:5.1f}°C | "
                      f"平均: {stats['temp_avg']:5.1f}°C | "
                      f"中心: {center_temp:5.1f}°C | "
                      f"FPS: {fps:5.1f}", end="", flush=True)
                
                # 检查按键
                key = cv2.waitKey(1) & 0xFF
                if key == ord('q') or key == 27:  # Q 或 ESC
                    print("\n\n✓ 用户退出")
                    break
        
        except KeyboardInterrupt:
            print("\n\n✓ 收到中断信号")
        
        except Exception as e:
            print(f"\n\n✗ 错误: {e}")
            import traceback
            traceback.print_exc()
        
        finally:
            # 清理
            cv2.destroyAllWindows()
            
            # 打印统计信息
            elapsed = time.time() - start_time
            fps = frame_count / elapsed if elapsed > 0 else 0
            
            print(f"\n程序运行统计:")
            print(f"  总帧数: {frame_count}")
            print(f"  运行时间: {elapsed:.1f} 秒")
            print(f"  平均帧率: {fps:.1f} FPS")
            print("=" * 70)
    
    # SDK 会在退出 with 语句时自动清理资源
    print("✓ SDK 资源已释放")


if __name__ == '__main__':
    main()
