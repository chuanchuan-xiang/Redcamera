#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
红外相机 SDK - Python 封装版本
基于根目录 C++ 实现的 Python 封装，提供简洁易用的 API

作者：AI Assistant
日期：2025-11-10
版本：1.0.0

功能：
- 相机初始化和管理
- 自动选择 256x384 分辨率
- 数据分离（raw_data_cut）
- 温度转换
- 帧获取和处理
"""

import os
import sys
from ctypes import *
import numpy as np


# ========================================================================
# 常量定义
# ========================================================================

# 相机 VID/PID
CAMERA_VID = 0x0BDA
CAMERA_PID = 0x5840

# 分辨率配置
RESOLUTION_192 = (256, 192)   # 单一模式（仅图像）
RESOLUTION_384 = (256, 384)   # 组合模式（图像+温度）

# 默认参数
DEFAULT_FPS = 25
DEFAULT_TIMEOUT = 2000


# ========================================================================
# C 结构体定义
# ========================================================================

class DevCfg(Structure):
    """设备配置结构"""
    _fields_ = [
        ("pid", c_uint),
        ("vid", c_uint),
        ("name", c_char_p),
    ]


class CameraStreamInfo(Structure):
    """相机流信息结构"""
    _fields_ = [
        ("format", c_char_p),
        ("width", c_uint),
        ("height", c_uint),
        ("frame_size", c_uint),
        ("fps", c_uint * 32),
    ]


class CameraParam(Structure):
    """相机参数结构"""
    _fields_ = [
        ("dev_cfg", DevCfg),
        ("format", c_char_p),
        ("width", c_uint),
        ("height", c_uint),
        ("frame_size", c_uint),
        ("fps", c_uint),
        ("timeout_ms_delay", c_uint),
    ]


# ========================================================================
# 温度转换函数
# ========================================================================

def temp_value_converter(temp_val):
    """
    温度值转换为摄氏度
    
    参数:
        temp_val: uint16 原始温度值
    
    返回:
        float: 摄氏度温度
    
    公式: (raw_value / 64.0) - 273.15
    """
    return (float(temp_val) / 64.0 - 273.15)


def temp_array_converter(temp_array):
    """
    批量转换温度数组（使用 NumPy 向量化操作）
    
    参数:
        temp_array: np.ndarray (uint16) 原始温度数据
    
    返回:
        np.ndarray (float32): 摄氏度温度数据
    """
    return (temp_array.astype(np.float32) / 64.0 - 273.15)


# ========================================================================
# 红外相机 SDK 类
# ========================================================================

class ThermalCameraSDK:
    """
    红外相机 SDK 主类
    
    用法示例:
        sdk = ThermalCameraSDK()
        sdk.initialize()
        sdk.open_camera()
        sdk.start_stream()
        
        while True:
            frame_data = sdk.get_frame()
            if frame_data:
                temp_celsius = frame_data['temperature_celsius']
                # 处理温度数据...
        
        sdk.stop_stream()
        sdk.close_camera()
    """
    
    def __init__(self, sdk_path='sdk'):
        """
        初始化 SDK
        
        参数:
            sdk_path: DLL 文件所在目录（默认 'sdk'）
        """
        self.sdk_path = os.path.abspath(sdk_path)
        self.libiruvc = None
        self.libirparse = None
        self.is_initialized = False
        self.is_camera_opened = False
        self.is_streaming = False
        
        # 相机参数
        self.camera_param = None
        self.width = 0
        self.height = 0
        self.frame_size = 0
        
        # 分离后的尺寸
        self.temp_width = 0
        self.temp_height = 0
        self.image_byte_size = 0
        self.temp_byte_size = 0
        
        # 缓冲区
        self.frame_buffer = None
        self.image_buffer = None
        self.temp_buffer = None
        
        # 统计信息
        self.frame_count = 0
        self.version_info = {}
    
    def _load_libraries(self):
        """加载 DLL 库"""
        try:
            os.add_dll_directory(self.sdk_path)
            
            self.libiruvc = CDLL(os.path.join(self.sdk_path, 'libiruvc.dll'))
            self.libirparse = CDLL(os.path.join(self.sdk_path, 'libirparse.dll'))
            
            # 设置函数签名 - libiruvc
            self.libiruvc.iruvc_version_number.argtypes = []
            self.libiruvc.iruvc_version_number.restype = c_char_p
            
            self.libiruvc.uvc_camera_init.argtypes = []
            self.libiruvc.uvc_camera_init.restype = c_int
            
            self.libiruvc.uvc_camera_list.argtypes = [POINTER(DevCfg)]
            self.libiruvc.uvc_camera_list.restype = c_int
            
            self.libiruvc.uvc_camera_info_get.argtypes = [DevCfg, POINTER(CameraStreamInfo)]
            self.libiruvc.uvc_camera_info_get.restype = c_int
            
            self.libiruvc.uvc_camera_open.argtypes = [DevCfg]
            self.libiruvc.uvc_camera_open.restype = c_int
            
            self.libiruvc.uvc_camera_stream_start.argtypes = [CameraParam, c_void_p]
            self.libiruvc.uvc_camera_stream_start.restype = c_int
            
            self.libiruvc.uvc_frame_get.argtypes = [c_void_p]
            self.libiruvc.uvc_frame_get.restype = c_int
            
            self.libiruvc.uvc_camera_stream_close.argtypes = [c_int]
            self.libiruvc.uvc_camera_stream_close.restype = c_int
            
            self.libiruvc.uvc_camera_close.argtypes = []
            self.libiruvc.uvc_camera_close.restype = None
            
            self.libiruvc.uvc_camera_release.argtypes = []
            self.libiruvc.uvc_camera_release.restype = None
            
            # 设置函数签名 - libirparse
            self.libirparse.raw_data_cut.argtypes = [
                POINTER(c_uint8), c_int, c_int, 
                POINTER(c_uint8), POINTER(c_uint8)
            ]
            self.libirparse.raw_data_cut.restype = c_int
            
            return True
            
        except Exception as e:
            raise RuntimeError(f"加载 DLL 失败: {e}")
    
    def initialize(self):
        """
        初始化 SDK 和 UVC 库
        
        返回:
            bool: 成功返回 True
        """
        if self.is_initialized:
            return True
        
        # 加载库
        self._load_libraries()
        
        # 获取版本信息
        version = self.libiruvc.iruvc_version_number()
        self.version_info['libiruvc'] = version.decode('utf-8')
        
        # 初始化 UVC
        ret = self.libiruvc.uvc_camera_init()
        if ret != 0:
            raise RuntimeError(f"UVC 初始化失败 (ret={ret})")
        
        self.is_initialized = True
        return True
    
    def list_devices(self):
        """
        列出所有可用设备
        
        返回:
            list: 设备信息列表 [{'index': 0, 'vid': 0x0BDA, 'pid': 0x5840, 'name': '...'}]
        """
        if not self.is_initialized:
            raise RuntimeError("SDK 未初始化，请先调用 initialize()")
        
        devs_cfg = (DevCfg * 64)()
        ret = self.libiruvc.uvc_camera_list(devs_cfg)
        if ret < 0:
            raise RuntimeError(f"列出设备失败 (ret={ret})")
        
        devices = []
        for i in range(64):
            if devs_cfg[i].vid == 0 and devs_cfg[i].pid == 0:
                break
            
            device_info = {
                'index': i,
                'vid': devs_cfg[i].vid,
                'pid': devs_cfg[i].pid,
                'name': devs_cfg[i].name.decode() if devs_cfg[i].name else 'Unknown'
            }
            devices.append(device_info)
        
        return devices
    
    def open_camera(self, use_384_mode=True):
        """
        打开相机
        
        参数:
            use_384_mode: 是否使用 256x384 模式（默认 True，用于温度数据分离）
        
        返回:
            dict: 相机信息 {'width': 256, 'height': 384, 'resolutions': [...]}
        """
        if not self.is_initialized:
            raise RuntimeError("SDK 未初始化，请先调用 initialize()")
        
        if self.is_camera_opened:
            return self.get_camera_info()
        
        # 列出设备
        devs_cfg = (DevCfg * 64)()
        ret = self.libiruvc.uvc_camera_list(devs_cfg)
        if ret < 0:
            raise RuntimeError(f"列出设备失败 (ret={ret})")
        
        # 查找目标设备
        found_dev_index = -1
        for i in range(64):
            if devs_cfg[i].vid == CAMERA_VID and devs_cfg[i].pid == CAMERA_PID:
                found_dev_index = i
                break
        
        if found_dev_index < 0:
            raise RuntimeError("未找到红外相机设备")
        
        # 获取流信息
        camera_stream_info = (CameraStreamInfo * 32)()
        ret = self.libiruvc.uvc_camera_info_get(
            devs_cfg[found_dev_index], 
            camera_stream_info
        )
        if ret < 0:
            raise RuntimeError(f"获取相机流信息失败 (ret={ret})")
        
        # 列出所有支持的分辨率
        resolutions = []
        stream_idx = -1
        target_height = 384 if use_384_mode else 192
        
        for i in range(32):
            if camera_stream_info[i].width == 0:
                break
            
            res_info = {
                'index': i,
                'width': camera_stream_info[i].width,
                'height': camera_stream_info[i].height,
                'format': camera_stream_info[i].format.decode() if camera_stream_info[i].format else 'Unknown'
            }
            resolutions.append(res_info)
            
            # 选择目标分辨率
            if (camera_stream_info[i].width == 256 and 
                camera_stream_info[i].height == target_height):
                stream_idx = i
        
        if stream_idx < 0:
            raise RuntimeError(f"未找到 256x{target_height} 分辨率")
        
        # 打开相机
        ret = self.libiruvc.uvc_camera_open(devs_cfg[found_dev_index])
        if ret < 0:
            raise RuntimeError(f"打开相机失败 (ret={ret})")
        
        # 设置相机参数
        self.camera_param = CameraParam()
        self.camera_param.dev_cfg = devs_cfg[found_dev_index]
        self.camera_param.format = camera_stream_info[stream_idx].format
        self.camera_param.width = camera_stream_info[stream_idx].width
        self.camera_param.height = camera_stream_info[stream_idx].height
        self.camera_param.frame_size = (
            camera_stream_info[stream_idx].width * 
            camera_stream_info[stream_idx].height * 2
        )
        self.camera_param.fps = DEFAULT_FPS
        self.camera_param.timeout_ms_delay = DEFAULT_TIMEOUT
        
        self.width = self.camera_param.width
        self.height = self.camera_param.height
        self.frame_size = self.camera_param.frame_size
        
        # 计算分离后的尺寸（仅在 384 模式下）
        if use_384_mode:
            self.temp_height = self.height // 2  # 192
            self.temp_width = self.width         # 256
            self.image_byte_size = self.temp_width * self.temp_height * 2
            self.temp_byte_size = self.temp_width * self.temp_height * 2
        else:
            self.temp_height = self.height
            self.temp_width = self.width
            self.image_byte_size = 0
            self.temp_byte_size = self.width * self.height * 2
        
        self.is_camera_opened = True
        
        return {
            'width': self.width,
            'height': self.height,
            'fps': self.camera_param.fps,
            'frame_size': self.frame_size,
            'temp_width': self.temp_width,
            'temp_height': self.temp_height,
            'resolutions': resolutions,
            'use_384_mode': use_384_mode
        }
    
    def start_stream(self):
        """
        启动视频流
        
        返回:
            bool: 成功返回 True
        """
        if not self.is_camera_opened:
            raise RuntimeError("相机未打开，请先调用 open_camera()")
        
        if self.is_streaming:
            return True
        
        # 启动流
        ret = self.libiruvc.uvc_camera_stream_start(self.camera_param, None)
        if ret < 0:
            raise RuntimeError(f"启动视频流失败 (ret={ret})")
        
        # 分配缓冲区
        self.frame_buffer = (c_uint8 * self.frame_size)()
        
        if self.height == 384:  # 组合模式
            self.image_buffer = (c_uint8 * self.image_byte_size)()
            self.temp_buffer = (c_uint8 * self.temp_byte_size)()
        
        self.is_streaming = True
        self.frame_count = 0
        
        return True
    
    def get_frame(self, max_retries=10):
        """
        获取一帧数据（包含温度数据分离）
        
        参数:
            max_retries: 最大重试次数（默认 10）
        
        返回:
            dict 或 None: {
                'frame_number': 帧号,
                'raw_frame': 原始数据（uint16 numpy array），
                'temperature_raw': 温度原始数据（uint16 numpy array），
                'temperature_celsius': 温度数据（摄氏度，float32 numpy array），
                'image_raw': 图像原始数据（uint16 numpy array，仅 384 模式），
                'stats': {
                    'raw_min': 原始最小值,
                    'raw_max': 原始最大值,
                    'raw_avg': 原始平均值,
                    'temp_min': 温度最小值（°C）,
                    'temp_max': 温度最大值（°C）,
                    'temp_avg': 温度平均值（°C）,
                }
            }
        """
        if not self.is_streaming:
            raise RuntimeError("视频流未启动，请先调用 start_stream()")
        
        # 获取原始帧
        retry_count = 0
        while retry_count < max_retries:
            ret = self.libiruvc.uvc_frame_get(self.frame_buffer)
            if ret == 0:
                break
            retry_count += 1
        
        if retry_count >= max_retries:
            return None
        
        self.frame_count += 1
        
        # 根据模式处理数据
        if self.height == 384:  # 组合模式 - 需要分离
            # 调用 raw_data_cut 分离数据
            ret_cut = self.libirparse.raw_data_cut(
                cast(self.frame_buffer, POINTER(c_uint8)),
                self.image_byte_size,
                self.temp_byte_size,
                self.image_buffer,
                self.temp_buffer
            )
            
            if ret_cut != 0:
                return None
            
            # 转换温度缓冲区为 NumPy 数组
            temp_ptr = cast(self.temp_buffer, POINTER(c_uint16))
            temp_pixel_count = self.temp_width * self.temp_height
            temp_raw = np.array(
                [temp_ptr[i] for i in range(temp_pixel_count)], 
                dtype=np.uint16
            ).reshape((self.temp_height, self.temp_width))
            
            # 转换图像缓冲区为 NumPy 数组
            image_ptr = cast(self.image_buffer, POINTER(c_uint16))
            image_pixel_count = self.temp_width * self.temp_height
            image_raw = np.array(
                [image_ptr[i] for i in range(image_pixel_count)], 
                dtype=np.uint16
            ).reshape((self.temp_height, self.temp_width))
            
        else:  # 192 模式 - 直接使用
            temp_ptr = cast(self.frame_buffer, POINTER(c_uint16))
            temp_pixel_count = self.width * self.height
            temp_raw = np.array(
                [temp_ptr[i] for i in range(temp_pixel_count)], 
                dtype=np.uint16
            ).reshape((self.height, self.width))
            image_raw = None
        
        # 转换为摄氏度
        temp_celsius = temp_array_converter(temp_raw)
        
        # 计算统计信息
        raw_min = np.min(temp_raw)
        raw_max = np.max(temp_raw)
        raw_avg = np.mean(temp_raw)
        
        temp_min = np.min(temp_celsius)
        temp_max = np.max(temp_celsius)
        temp_avg = np.mean(temp_celsius)
        
        return {
            'frame_number': self.frame_count,
            'raw_frame': temp_raw,
            'temperature_raw': temp_raw,
            'temperature_celsius': temp_celsius,
            'image_raw': image_raw,
            'stats': {
                'raw_min': int(raw_min),
                'raw_max': int(raw_max),
                'raw_avg': float(raw_avg),
                'temp_min': float(temp_min),
                'temp_max': float(temp_max),
                'temp_avg': float(temp_avg),
            }
        }
    
    def stop_stream(self):
        """停止视频流"""
        if not self.is_streaming:
            return
        
        self.libiruvc.uvc_camera_stream_close(1)
        self.is_streaming = False
        
        # 释放缓冲区
        self.frame_buffer = None
        self.image_buffer = None
        self.temp_buffer = None
    
    def close_camera(self):
        """关闭相机"""
        if self.is_streaming:
            self.stop_stream()
        
        if not self.is_camera_opened:
            return
        
        self.libiruvc.uvc_camera_close()
        self.is_camera_opened = False
    
    def release(self):
        """释放所有资源"""
        if self.is_streaming:
            self.stop_stream()
        
        if self.is_camera_opened:
            self.close_camera()
        
        if self.is_initialized:
            self.libiruvc.uvc_camera_release()
            self.is_initialized = False
    
    def get_camera_info(self):
        """获取当前相机信息"""
        if not self.is_camera_opened:
            return None
        
        return {
            'width': self.width,
            'height': self.height,
            'temp_width': self.temp_width,
            'temp_height': self.temp_height,
            'frame_size': self.frame_size,
            'fps': self.camera_param.fps,
            'is_streaming': self.is_streaming,
            'frame_count': self.frame_count,
        }
    
    def get_version_info(self):
        """获取版本信息"""
        return self.version_info
    
    def __enter__(self):
        """上下文管理器入口"""
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """上下文管理器退出"""
        self.release()
    
    def __del__(self):
        """析构函数"""
        self.release()


# ========================================================================
# 便捷函数
# ========================================================================

def create_thermal_camera_sdk(sdk_path='sdk'):
    """
    创建红外相机 SDK 实例（便捷函数）
    
    参数:
        sdk_path: DLL 文件所在目录
    
    返回:
        ThermalCameraSDK: SDK 实例
    """
    return ThermalCameraSDK(sdk_path)


# ========================================================================
# 模块信息
# ========================================================================

__version__ = '1.0.0'
__author__ = 'AI Assistant'
__all__ = [
    'ThermalCameraSDK',
    'create_thermal_camera_sdk',
    'temp_value_converter',
    'temp_array_converter',
    'CAMERA_VID',
    'CAMERA_PID',
    'RESOLUTION_192',
    'RESOLUTION_384',
]
