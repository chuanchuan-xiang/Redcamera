#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
红外热成像相机程序 - 完整版
功能：实时显示红外相机的温度图像，支持伪彩色显示和温度测量

作者：CCCCCCC
日期：2025-11-04
"""

import ctypes
import numpy as np
import cv2
import time
import os
from ctypes import POINTER, Structure, c_void_p, c_int, c_uint, c_uint8, c_uint16, c_uint32, c_double


# ============================================================
# 第一部分：底层 C 库接口封装
# ============================================================

class ThermalCameraSDK:
    """
    红外相机 SDK 封装类
    
    这个类封装了 C++ 编写的底层库，提供 Python 可以直接调用的接口
    主要功能：
    1. 打开/关闭相机
    2. 获取温度数据帧
    3. 温度数据转换（Y14 格式 -> 摄氏度）
    """
    
    def __init__(self):
        """初始化 SDK，加载动态库"""
        # 获取当前脚本所在目录
        current_dir = os.path.dirname(os.path.abspath(__file__))
        
        # 库文件路径
        sdk_path = os.path.join(current_dir, "sdk/libtemperature.so")
        libs_path = os.path.join(current_dir, "libs")
        
        # 检查库文件是否存在
        if not os.path.exists(sdk_path):
            raise FileNotFoundError(f"找不到库文件: {sdk_path}")
        
        # 预加载依赖库（必须在加载 libtemperature.so 之前）
        # 使用 RTLD_GLOBAL 确保符号全局可见
        try:
            # 依赖库列表
            dep_libs = [
                "libirtemp.so",
                "libirprocess.so",
                "libiruvc.so",
                "libirparse.so"
            ]
            
            for lib_name in dep_libs:
                lib_path = os.path.join(libs_path, lib_name)
                if os.path.exists(lib_path):
                    ctypes.CDLL(lib_path, mode=ctypes.RTLD_GLOBAL)
                else:
                    print(f"⚠ 警告: 依赖库不存在: {lib_path}")
        except Exception as e:
            print(f"⚠ 加载依赖库时出错: {e}")
        
        # 加载主库
        self.lib = ctypes.CDLL(sdk_path)
        self.camera_handle = None
        
        # 设置函数接口
        self._setup_functions()
        
        print(f"✓ SDK 初始化成功")
        print(f"  核心库: {sdk_path}")
    
    def _setup_functions(self):
        """
        设置 C 函数的参数类型和返回值类型
        这是 Python ctypes 调用 C 函数的必要步骤
        """
        # 1. 相机句柄创建/销毁
        self.lib.simple_camera_create.restype = c_void_p
        self.lib.simple_camera_destroy.argtypes = [c_void_p]
        
        # 2. 相机打开/关闭
        self.lib.simple_camera_open.argtypes = [c_void_p]
        self.lib.simple_camera_open.restype = c_int
        
        self.lib.simple_camera_close.argtypes = [c_void_p]
        self.lib.simple_camera_close.restype = c_int
        
        # 3. 流式传输控制
        self.lib.simple_camera_start_stream.argtypes = [c_void_p]
        self.lib.simple_camera_start_stream.restype = c_int
        
        self.lib.simple_camera_stop_stream.argtypes = [c_void_p]
        self.lib.simple_camera_stop_stream.restype = c_int
        
        # 4. 帧数据获取
        self.lib.simple_camera_get_frame.argtypes = [c_void_p, c_uint32]
        self.lib.simple_camera_get_frame.restype = c_int
        
        self.lib.simple_camera_get_temp_data.argtypes = [c_void_p]
        self.lib.simple_camera_get_temp_data.restype = POINTER(c_uint16)
        
        self.lib.simple_camera_get_temp_size.argtypes = [c_void_p, POINTER(c_uint32), POINTER(c_uint32)]
        self.lib.simple_camera_get_temp_size.restype = c_int
        
        self.lib.simple_camera_get_info.argtypes = [c_void_p, POINTER(c_uint32), POINTER(c_uint32), POINTER(c_uint32)]
        self.lib.simple_camera_get_info.restype = c_int
    
    def open_camera(self):
        """
        打开红外相机
        
        返回:
            bool: True=成功, False=失败
        """
        # 创建相机句柄
        self.camera_handle = self.lib.simple_camera_create()
        if not self.camera_handle:
            print("✗ 创建相机句柄失败")
            return False
        
        # 打开相机
        ret = self.lib.simple_camera_open(self.camera_handle)
        if ret < 0:
            print(f"✗ 打开相机失败，错误码: {ret}")
            return False
        
        # 获取相机信息
        width = c_uint32()
        height = c_uint32()
        fps = c_uint32()
        self.lib.simple_camera_get_info(self.camera_handle, 
                                       ctypes.byref(width),
                                       ctypes.byref(height),
                                       ctypes.byref(fps))
        
        print(f"✓ 相机打开成功: {width.value}x{height.value} @ {fps.value}fps")
        return True
    
    def close_camera(self):
        """关闭红外相机"""
        if self.camera_handle:
            self.lib.simple_camera_close(self.camera_handle)
            self.lib.simple_camera_destroy(self.camera_handle)
            self.camera_handle = None
            print("✓ 相机已关闭")
    
    def start_stream(self):
        """
        开始数据流传输
        
        返回:
            bool: True=成功, False=失败
        """
        if not self.camera_handle:
            return False
        
        ret = self.lib.simple_camera_start_stream(self.camera_handle)
        if ret < 0:
            print(f"✗ 启动流传输失败，错误码: {ret}")
            return False
        
        print("✓ 流传输已启动")
        return True
    
    def stop_stream(self):
        """停止数据流传输"""
        if self.camera_handle:
            self.lib.simple_camera_stop_stream(self.camera_handle)
            print("✓ 流传输已停止")
    
    def get_temperature_frame(self):
        """
        获取一帧温度数据
        
        返回:
            numpy.ndarray: 温度帧数据（Y14格式），shape=(192, 256), dtype=uint16
            None: 获取失败
        """
        if not self.camera_handle:
            return None
        
        # 获取一帧数据
        ret = self.lib.simple_camera_get_frame(self.camera_handle, 1000)
        if ret < 0:
            return None
        
        # 获取温度帧尺寸
        width = c_uint32()
        height = c_uint32()
        self.lib.simple_camera_get_temp_size(self.camera_handle,
                                            ctypes.byref(width),
                                            ctypes.byref(height))
        
        # 获取温度数据指针
        data_ptr = self.lib.simple_camera_get_temp_data(self.camera_handle)
        if not data_ptr:
            return None
        
        # 转换为 numpy 数组
        size = width.value * height.value
        temp_array = np.ctypeslib.as_array(data_ptr, shape=(size,))
        temp_frame = temp_array.reshape((height.value, width.value)).copy()
        
        return temp_frame
    
    @staticmethod
    def y14_to_celsius(y14_value):
        """
        将 Y14 格式的温度值转换为摄氏度
        
        Y14 格式说明：
        - Y14 是红外相机的原始温度数据格式
        - 转换公式: 摄氏度 = (Y14值 / 64.0) - 273.15
        
        参数:
            y14_value: Y14 格式的温度值（uint16）
            
        返回:
            float: 摄氏度温度
        """
        return (float(y14_value) / 64.0) - 273.15
    
    @staticmethod
    def y14_frame_to_celsius(y14_frame):
        """
        将整帧 Y14 数据转换为摄氏度
        
        参数:
            y14_frame: Y14 格式的温度帧（numpy数组）
            
        返回:
            numpy.ndarray: 摄氏度温度帧
        """
        return (y14_frame.astype(np.float32) / 64.0) - 273.15


# ============================================================
# 第二部分：热成像显示应用
# ============================================================

class ThermalCameraApp:
    """
    热成像相机显示应用
    
    功能：
    1. 实时显示红外相机的温度图像
    2. 支持多种伪彩色方案
    3. 显示温度统计信息
    4. 支持十字准星和中心点温度显示
    """
    
    def __init__(self):
        """初始化应用"""
        self.sdk = None
        
        # 显示设置
        self.colormap = cv2.COLORMAP_JET  # 默认使用 JET 伪彩色
        self.colormap_names = {
            cv2.COLORMAP_JET: "JET",
            cv2.COLORMAP_HOT: "HOT",
            cv2.COLORMAP_RAINBOW: "RAINBOW",
            cv2.COLORMAP_COOL: "COOL",
            cv2.COLORMAP_BONE: "BONE"
        }
        
        # 温度范围设置
        self.auto_range = True   # 自动调整温度范围
        self.min_temp = 0.0
        self.max_temp = 60.0
        
        # 显示选项
        self.show_crosshair = True  # 显示十字准星
        self.show_stats = True      # 显示统计信息
        
        # 性能统计
        self.frame_count = 0
        self.start_time = time.time()
        self.fps = 0.0
    
    def initialize(self):
        """
        初始化相机和 SDK
        
        返回:
            bool: True=成功, False=失败
        """
        print("\n" + "=" * 60)
        print("  红外热成像相机程序")
        print("  Thermal Camera Application")
        print("=" * 60 + "\n")
        
        try:
            # 创建 SDK 实例
            self.sdk = ThermalCameraSDK()
            
            # 打开相机
            if not self.sdk.open_camera():
                return False
            
            # 开始流传输
            if not self.sdk.start_stream():
                self.sdk.close_camera()
                return False
            
            return True
            
        except Exception as e:
            print(f"✗ 初始化失败: {e}")
            import traceback
            traceback.print_exc()
            return False
    
    def process_frame(self, y14_frame):
        """
        处理温度帧，生成可视化图像
        
        处理步骤：
        1. 将 Y14 格式转换为摄氏度
        2. 计算温度统计信息（最小值、最大值、平均值）
        3. 归一化到 0-255 范围
        4. 应用伪彩色映射
        5. 放大图像以便观看
        6. 添加十字准星和信息叠加
        
        参数:
            y14_frame: Y14 格式的温度帧
            
        返回:
            numpy.ndarray: BGR 格式的可视化图像
        """
        # 步骤1: 转换为摄氏度
        celsius_frame = self.sdk.y14_frame_to_celsius(y14_frame)
        
        # 步骤2: 计算温度统计
        min_temp = np.min(celsius_frame)
        max_temp = np.max(celsius_frame)
        mean_temp = np.mean(celsius_frame)
        std_temp = np.std(celsius_frame)
        
        # 自动调整显示范围
        if self.auto_range:
            self.min_temp = min_temp
            self.max_temp = max_temp
        
        # 步骤3: 归一化（将温度值映射到 0-255）
        normalized = np.clip(
            (celsius_frame - self.min_temp) / (self.max_temp - self.min_temp + 1e-6),
            0, 1
        )
        gray = (normalized * 255).astype(np.uint8)
        
        # 步骤4: 应用伪彩色
        colored = cv2.applyColorMap(gray, self.colormap)
        
        # 步骤5: 放大图像 (192x256 -> 576x768)
        display = cv2.resize(colored, (768, 576), interpolation=cv2.INTER_NEAREST)
        
        # 步骤6: 添加十字准星
        if self.show_crosshair:
            h, w = display.shape[:2]
            center_x, center_y = w // 2, h // 2
            
            # 画十字线
            cv2.line(display, (center_x - 30, center_y), (center_x + 30, center_y), 
                    (0, 255, 0), 2)
            cv2.line(display, (center_x, center_y - 30), (center_x, center_y + 30), 
                    (0, 255, 0), 2)
            
            # 显示中心点温度
            orig_h, orig_w = celsius_frame.shape
            center_temp = celsius_frame[orig_h // 2, orig_w // 2]
            cv2.putText(display, f"{center_temp:.1f}C", 
                       (center_x + 35, center_y + 5),
                       cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)
        
        # 步骤7: 添加统计信息
        if self.show_stats:
            self._draw_info_panel(display, {
                'min': min_temp,
                'max': max_temp,
                'mean': mean_temp,
                'std': std_temp
            })
        
        return display
    
    def _draw_info_panel(self, image, stats):
        """
        在图像上绘制信息面板
        
        参数:
            image: 要绘制的图像
            stats: 温度统计信息字典
        """
        # 创建半透明背景
        overlay = image.copy()
        cv2.rectangle(overlay, (5, 5), (280, 220), (0, 0, 0), -1)
        cv2.addWeighted(overlay, 0.7, image, 0.3, 0, image)
        
        # 计算帧率
        elapsed = time.time() - self.start_time
        if elapsed > 0:
            self.fps = self.frame_count / elapsed
        
        # 准备显示文本
        info_lines = [
            "Thermal Camera",
            "=" * 25,
            f"Resolution: 256x192",
            f"Colormap: {self.colormap_names[self.colormap]}",
            "",
            "Temperature:",
            f"  Min: {stats['min']:.1f} C",
            f"  Max: {stats['max']:.1f} C",
            f"  Avg: {stats['mean']:.1f} C",
            f"  Std: {stats['std']:.1f} C",
            "",
            f"Frame: {self.frame_count}",
            f"FPS: {self.fps:.1f}",
        ]
        
        # 绘制文本
        y = 25
        for line in info_lines:
            cv2.putText(image, line, (10, y), 
                       cv2.FONT_HERSHEY_SIMPLEX, 0.45, (0, 255, 0), 1, cv2.LINE_AA)
            y += 20 if line and line != "=" * 25 else 10
    
    def run(self):
        """
        运行主循环
        
        返回:
            int: 退出码（0=正常退出）
        """
        # 初始化
        if not self.initialize():
            print("✗ 初始化失败，程序退出")
            return 1
        
        # 显示操作说明
        print("\n" + "=" * 60)
        print("键盘控制说明:")
        print("  1-5: 切换伪彩色方案")
        print("  A:   开启/关闭 自动温度范围")
        print("  C:   显示/隐藏 十字准星")
        print("  S:   显示/隐藏 统计信息")
        print("  Q/ESC: 退出程序")
        print("=" * 60 + "\n")
        
        # 创建显示窗口
        window_name = "红外热成像相机"
        cv2.namedWindow(window_name, cv2.WINDOW_NORMAL)
        cv2.resizeWindow(window_name, 768, 576)
        
        try:
            # 主循环
            while True:
                # 获取一帧温度数据
                temp_frame = self.sdk.get_temperature_frame()
                if temp_frame is None:
                    print(".", end="", flush=True)  # 显示进度点
                    continue
                
                # 处理并显示
                display_image = self.process_frame(temp_frame)
                cv2.imshow(window_name, display_image)
                
                self.frame_count += 1
                
                # 处理键盘输入
                key = cv2.waitKey(1) & 0xFF
                
                if key == ord('q') or key == 27:  # Q 或 ESC
                    print("\n✓ 用户退出")
                    break
                    
                elif key == ord('1'):
                    self.colormap = cv2.COLORMAP_JET
                    print(f"✓ Colormap: {self.colormap_names[self.colormap]}")
                    
                elif key == ord('2'):
                    self.colormap = cv2.COLORMAP_HOT
                    print(f"✓ Colormap: {self.colormap_names[self.colormap]}")
                    
                elif key == ord('3'):
                    self.colormap = cv2.COLORMAP_RAINBOW
                    print(f"✓ Colormap: {self.colormap_names[self.colormap]}")
                    
                elif key == ord('4'):
                    self.colormap = cv2.COLORMAP_COOL
                    print(f"✓ Colormap: {self.colormap_names[self.colormap]}")
                    
                elif key == ord('5'):
                    self.colormap = cv2.COLORMAP_BONE
                    print(f"✓ Colormap: {self.colormap_names[self.colormap]}")
                    
                elif key == ord('a') or key == ord('A'):
                    self.auto_range = not self.auto_range
                    status = "ON" if self.auto_range else "OFF"
                    print(f"✓ Auto Range: {status}")
                    
                elif key == ord('c') or key == ord('C'):
                    self.show_crosshair = not self.show_crosshair
                    status = "ON" if self.show_crosshair else "OFF"
                    print(f"✓ Crosshair: {status}")
                    
                elif key == ord('s') or key == ord('S'):
                    self.show_stats = not self.show_stats
                    status = "ON" if self.show_stats else "OFF"
                    print(f"✓ Statistics: {status}")
        
        except KeyboardInterrupt:
            print("\n✓ 收到中断信号")
            
        except Exception as e:
            print(f"\n✗ 错误: {e}")
            import traceback
            traceback.print_exc()
            
        finally:
            # 清理资源
            cv2.destroyAllWindows()
            if self.sdk:
                self.sdk.stop_stream()
                self.sdk.close_camera()
            
            # 显示统计信息
            elapsed = time.time() - self.start_time
            print(f"\n程序运行统计:")
            print(f"  总帧数: {self.frame_count}")
            print(f"  运行时间: {elapsed:.1f} 秒")
            print(f"  平均帧率: {self.fps:.1f} FPS")
            print("\n" + "=" * 60)
        
        return 0


# ============================================================
# 第三部分：程序入口
# ============================================================

def main():
    """主函数"""
    app = ThermalCameraApp()
    return app.run()


if __name__ == "__main__":
    exit(main())
