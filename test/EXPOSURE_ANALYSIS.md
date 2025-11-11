# 未暴露函数的必要性分析

## 📊 分析维度

我们从以下几个维度分析这些函数是否需要暴露：
1. ✅ **功能独立性** - 能否独立使用？
2. ✅ **用户需求** - 用户是否真的需要？
3. ⚠️ **复杂度** - 使用难度如何？
4. ❌ **依赖关系** - 是否依赖内部状态？
5. 🔒 **封装原则** - 暴露是否破坏封装？

---

## 🎯 结论概览

| 类别 | 建议 | 理由 |
|-----|------|-----|
| **display.cpp 函数** | ❌ **不建议暴露** | OpenCV 依赖重，Python 更适合做显示 |
| **camera.cpp 内部函数** | ⚠️ **部分暴露** | 自动增益等高级功能可选暴露 |
| **data.cpp 函数** | ❌ **完全不暴露** | 纯内部实现细节 |

---

## 📋 第一类：display.cpp 的函数

### ❌ **不建议暴露的理由**

#### 1. **display_init() / display_release()**
```cpp
void display_init(StreamFrameInfo_t* stream_frame_info)
{
    image_tmp_frame1 = (uint8_t*)malloc(pixel_size * 3);
    image_tmp_frame2 = (uint8_t*)malloc(pixel_size * 3);
}
```

**问题**：
- ❌ 管理全局变量（`image_tmp_frame1/2`）
- ❌ 依赖复杂的 `StreamFrameInfo_t` 结构体
- ❌ simple_camera 已经处理了内存管理

**替代方案**：用户在 Python 中自己分配 NumPy 数组更灵活
```python
# Python 更简单
temp_buffer = np.zeros((192, 256), dtype=np.uint8)
```

---

#### 2. **display_one_frame()**
```cpp
void display_one_frame(StreamFrameInfo_t* stream_frame_info)
{
    // OpenCV 显示窗口
    cv::imshow("Thermal Camera", mat);
    cv::waitKey(1);
}
```

**问题**：
- ❌ 强依赖 OpenCV C++ 接口
- ❌ 创建 OpenCV 窗口（跨语言难以控制）
- ❌ 混合了数据处理和 UI 逻辑

**为什么不需要**：
```python
# Python 中使用 OpenCV 更自然、更灵活
import cv2

while True:
    temp = camera.get_temp_data()
    celsius = (temp / 64.0) - 273.15
    
    # 伪彩色渲染
    colored = cv2.applyColorMap(normalized, cv2.COLORMAP_JET)
    
    # 添加文字、温度标签（更灵活）
    cv2.putText(colored, f"Max: {celsius.max():.1f}°C", ...)
    cv2.imshow("My Custom Window", colored)
    
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break
```

**优势对比**：
| 功能 | C++ display_one_frame | Python cv2 |
|-----|----------------------|-----------|
| 自定义窗口标题 | 硬编码 | ✅ 随意定制 |
| 添加温度标签 | 固定位置 | ✅ 任意位置 |
| 颜色映射 | 固定 COLORMAP | ✅ 12+ 种可选 |
| 多窗口显示 | 困难 | ✅ 轻松实现 |
| 事件响应 | 有限 | ✅ 丰富的回调 |
| 保存视频 | 需自己实现 | ✅ VideoWriter |

**现有 thermal_camera.py 已实现**：
- ✅ 完整的 OpenCV 显示
- ✅ 温度标签、伪彩色
- ✅ 中心十字线
- ✅ 按键控制

---

#### 3. **enhance_image_frame() / color_image_frame()**
```cpp
int enhance_image_frame(uint16_t* src, FrameInfo_t* info, uint16_t* dst);
void color_image_frame(uint8_t* src, FrameInfo_t* info, uint8_t* dst);
```

**问题**：
- ❌ 依赖特定的 `FrameInfo_t` 结构体
- ❌ 参数复杂，难以从 Python 调用
- ❌ 功能固定，不如 OpenCV/NumPy 灵活

**Python 替代**：
```python
# 图像增强
enhanced = cv2.equalizeHist(image)
enhanced = cv2.normalize(image, None, 0, 255, cv2.NORM_MINMAX)

# 伪彩色
colored = cv2.applyColorMap(image, cv2.COLORMAP_JET)

# 对比度调整
alpha = 1.5  # 对比度
beta = 30    # 亮度
adjusted = cv2.convertScaleAbs(image, alpha=alpha, beta=beta)
```

---

#### 4. **segment_human_by_real_temperature()**
```cpp
void segment_human_by_real_temperature(uint16_t* y14_data, 
                                       int width, int height, 
                                       uint8_t* dst_frame)
{
    // 人体温度分割（25-40°C）
}
```

**这个函数有一定价值！但需要改造**

**当前问题**：
- ⚠️ 输出格式不明确（dst_frame 是什么格式？）
- ⚠️ 阈值硬编码（25-40°C）

**如果要暴露，建议改造为**：
```cpp
// 方案 1: 返回二值掩码
void segment_by_temperature(uint16_t* y14_data, int width, int height,
                           float min_celsius, float max_celsius,
                           uint8_t* mask_output);  // 0/255 二值掩码

// 方案 2: 直接在 Python 中实现（更推荐）
def segment_human(celsius_array):
    mask = (celsius_array >= 25) & (celsius_array <= 40)
    return mask.astype(np.uint8) * 255
```

**Python 实现更灵活**：
```python
# 多种分割策略
mask_human = (temp >= 25) & (temp <= 40)
mask_hot = temp > 50
mask_cold = temp < 15

# 形态学处理
kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5,5))
mask_clean = cv2.morphologyEx(mask_human, cv2.MORPH_CLOSE, kernel)

# 应用掩码
result = np.where(mask_clean[..., None], original_image, background)
```

---

#### 5. **rotate_demo() / mirror_flip_demo()**
```cpp
void rotate_demo(FrameInfo_t* frame_info, uint8_t* frame, RotateSide_t side);
void mirror_flip_demo(FrameInfo_t* frame_info, uint8_t* frame, MirrorFlipStatus_t status);
```

**完全不需要暴露**：
```python
# Python 一行搞定
rotated = cv2.rotate(image, cv2.ROTATE_90_CLOCKWISE)
flipped = cv2.flip(image, 1)  # 水平翻转
flipped = cv2.flip(image, 0)  # 垂直翻转
```

---

### 📊 display.cpp 总结

| 函数 | 是否暴露 | 理由 |
|-----|---------|-----|
| display_init/release | ❌ | 内部内存管理，用户不需要 |
| display_one_frame | ❌ | Python OpenCV 更好 |
| enhance_image_frame | ❌ | cv2.equalizeHist 等替代 |
| color_image_frame | ❌ | cv2.applyColorMap 替代 |
| segment_human | ⚠️ | 可考虑，但 Python 实现更灵活 |
| rotate_demo | ❌ | cv2.rotate 替代 |
| mirror_flip_demo | ❌ | cv2.flip 替代 |
| add_temperature_labels | ❌ | cv2.putText 替代 |

**结论**：display.cpp 的所有函数都 **不建议暴露**，Python + OpenCV 是更好的选择。

---

## ⚙️ 第二类：camera.cpp 的内部函数

### ⚠️ **部分值得暴露**

#### 1. **auto_gain_switch()** - ⚠️ 可以暴露
```cpp
void auto_gain_switch(uint16_t* temp_frame, FrameInfo_t* temp_info,
                      AutoGainSwitchInfo_t* auto_gain_switch_info)
```

**暴露价值**：⭐⭐⭐⭐☆ (4/5)

**理由**：
- ✅ 专业功能：自动增益切换在热成像中很重要
- ✅ 算法复杂：需要依赖底层 SDK（`gain_switch_detect()`）
- ✅ 实用场景：测量高温物体时需要切换增益档位
- ⚠️ 需要简化接口

**当前问题**：
```cpp
// 需要复杂的结构体参数
typedef struct {
    uint8_t switched_flag;
    int cur_switched_cnt;
    int waiting_frame_cnt;
    int cur_detected_low_cnt;
    int cur_detected_high_cnt;
    int detect_frame_cnt;
} AutoGainSwitchInfo_t;
```

**建议封装方案**：
```cpp
// 简化接口
typedef enum {
    GAIN_MODE_AUTO = 0,    // 自动增益
    GAIN_MODE_HIGH = 1,    // 高增益（测低温）
    GAIN_MODE_LOW = 2      // 低增益（测高温）
} GainMode_t;

// 新增到 SDK
int simple_camera_set_gain_mode(SimpleCameraHandle_t* handle, GainMode_t mode);
int simple_camera_get_gain_mode(SimpleCameraHandle_t* handle, GainMode_t* mode);
```

**Python 使用示例**：
```python
# 自动增益（默认）
camera.set_gain_mode(GAIN_MODE_AUTO)

# 测量炉温（>130°C），切换到低增益
camera.set_gain_mode(GAIN_MODE_LOW)

# 测量冰块，切换到高增益
camera.set_gain_mode(GAIN_MODE_HIGH)
```

---

#### 2. **avoid_overexposure()** - ⚠️ 可以暴露
```cpp
void avoid_overexposure(uint16_t* temp_frame, FrameInfo_t* temp_info, 
                        int close_frame_cnt)
```

**暴露价值**：⭐⭐⭐☆☆ (3/5)

**理由**：
- ✅ 专业需求：避免传感器过曝损坏
- ⚠️ 大多数用户不关心
- ⚠️ 可以集成到 `simple_camera_get_frame()` 内部自动处理

**建议**：
```cpp
// 选项 1: 自动集成（推荐）
// 在 simple_camera_get_frame() 内部自动调用，用户无感

// 选项 2: 可选开关
int simple_camera_enable_overexposure_protection(SimpleCameraHandle_t* handle, 
                                                  bool enable);
```

---

#### 3. **get_dev_index_with_pid_vid()** - ❌ 不暴露
```cpp
int get_dev_index_with_pid_vid(int vid, int pid, DevCfg_t devs_cfg[])
```

**理由**：
- ❌ 纯内部实现细节
- ❌ 用户不关心设备索引
- ❌ `simple_camera_open()` 已经自动处理

---

#### 4. **camera_para_set()** - ❌ 不暴露
```cpp
CameraParam_t camera_para_set(DevCfg_t dev_cfg, int stream_index, ...)
```

**理由**：
- ❌ 参数设置已在 `simple_camera_open()` 中完成
- ❌ 用户不需要手动配置

---

#### 5. **stream_function()** - ❌ 不暴露
```cpp
void* stream_function(void* threadarg)
```

**理由**：
- ❌ 线程函数，仅供内部使用
- ❌ `simple_camera_start_stream()` 已封装

---

#### 6. **ir_camera_stream_on_with_callback()** - ⭐ **值得暴露！**
```cpp
int ir_camera_stream_on_with_callback(StreamFrameInfo_t* stream_frame_info, 
                                      void* test_func)
```

**暴露价值**：⭐⭐⭐⭐⭐ (5/5)

**理由**：
- ✅ **高级功能**：允许用户注册回调函数处理每一帧
- ✅ **高性能**：避免主线程轮询，帧到达立即回调
- ✅ **灵活性**：用户可以在回调中做任何处理

**建议封装方案**：
```cpp
// C 回调函数类型
typedef void (*frame_callback_t)(uint16_t* temp_data, uint8_t* image_data,
                                 int width, int height, void* user_data);

// SDK 接口
int simple_camera_set_frame_callback(SimpleCameraHandle_t* handle,
                                    frame_callback_t callback,
                                    void* user_data);
```

**Python 使用示例**：
```python
# 定义回调函数
def on_new_frame(temp_ptr, image_ptr, width, height, user_data):
    temp = np.ctypeslib.as_array(temp_ptr, shape=(height, width))
    celsius = (temp / 64.0) - 273.15
    print(f"新帧到达！平均温度: {celsius.mean():.1f}°C")

# 注册回调
callback_func = ctypes.CFUNCTYPE(None, ctypes.POINTER(ctypes.c_uint16),
                                 ctypes.POINTER(ctypes.c_uint8),
                                 ctypes.c_int, ctypes.c_int, ctypes.c_void_p)
lib.simple_camera_set_frame_callback(camera, callback_func(on_new_frame), None)

# 启动流（回调模式）
lib.simple_camera_start_stream(camera)
```

---

### 📊 camera.cpp 总结

| 函数 | 是否暴露 | 优先级 | 封装方式 |
|-----|---------|-------|---------|
| **auto_gain_switch** | ✅ 是 | ⭐⭐⭐⭐ | 简化为 set_gain_mode() |
| **avoid_overexposure** | ⚠️ 可选 | ⭐⭐⭐ | 集成到内部或提供开关 |
| **ir_camera_stream_on_with_callback** | ✅ 是 | ⭐⭐⭐⭐⭐ | 封装为 set_frame_callback() |
| get_dev_index_with_pid_vid | ❌ 否 | - | 内部函数 |
| camera_para_set | ❌ 否 | - | 内部函数 |
| stream_function | ❌ 否 | - | 线程函数 |

---

## 🗄️ 第三类：data.cpp 的函数

### ❌ **完全不需要暴露**

#### 1. **init_pthread_sem() / destroy_pthread_sem()**
```cpp
int init_pthread_sem()
{
    sem_init(&image_sem, 0, 1);
    sem_init(&temp_sem, 0, 1);
    // ...
}
```

**理由**：
- ❌ 纯实现细节：用于原工程的多线程同步
- ❌ SDK 已改为同步模式，不需要信号量
- ❌ 用户完全不关心

---

#### 2. **create_data_demo() / destroy_data_demo()**
```cpp
int create_data_demo(StreamFrameInfo_t* stream_frame_info)
{
    stream_frame_info->raw_frame = uvc_frame_buf_create(...);
    stream_frame_info->image_frame = malloc(...);
    stream_frame_info->temp_frame = malloc(...);
}
```

**理由**：
- ❌ 内部内存管理：已在 `simple_camera_open/close` 中处理
- ❌ 用户不需要手动管理缓冲区
- ❌ 破坏封装原则

---

### 📊 data.cpp 总结

| 函数 | 是否暴露 | 理由 |
|-----|---------|-----|
| init_pthread_sem | ❌ | 内部同步机制 |
| destroy_pthread_sem | ❌ | 内部同步机制 |
| create_data_demo | ❌ | 内部内存管理 |
| destroy_data_demo | ❌ | 内部内存管理 |

**结论**：data.cpp 所有函数都是内部实现，**完全不应该暴露**。

---

## 🎯 最终建议

### ✅ **推荐新增暴露的函数**（优先级排序）

#### 1. ⭐⭐⭐⭐⭐ **高优先级**
```cpp
// 帧回调机制（异步高性能）
typedef void (*frame_callback_t)(uint16_t* temp_data, uint8_t* image_data,
                                 int width, int height, void* user_data);

int simple_camera_set_frame_callback(SimpleCameraHandle_t* handle,
                                    frame_callback_t callback,
                                    void* user_data);
```

**使用场景**：
- 实时数据采集系统
- 高帧率处理（避免丢帧）
- 后台监控程序

---

#### 2. ⭐⭐⭐⭐ **中高优先级**
```cpp
// 增益模式控制
typedef enum {
    GAIN_MODE_AUTO = 0,
    GAIN_MODE_HIGH = 1,
    GAIN_MODE_LOW = 2
} GainMode_t;

int simple_camera_set_gain_mode(SimpleCameraHandle_t* handle, GainMode_t mode);
int simple_camera_get_gain_mode(SimpleCameraHandle_t* handle, GainMode_t* mode);
```

**使用场景**：
- 测量高温物体（>130°C）
- 测量低温物体（<-20°C）
- 扩展测温范围

---

#### 3. ⭐⭐⭐ **中优先级**
```cpp
// 过曝保护开关
int simple_camera_enable_overexposure_protection(SimpleCameraHandle_t* handle, 
                                                  bool enable);
```

**使用场景**：
- 专业测温应用
- 保护传感器

---

### ❌ **不建议暴露的函数**

| 类别 | 函数 | 原因 |
|-----|------|------|
| **display.cpp** | 所有函数 | Python + OpenCV 更好 |
| **data.cpp** | 所有函数 | 内部实现细节 |
| **camera.cpp** | get_dev_index_with_pid_vid | 内部工具函数 |
| **camera.cpp** | camera_para_set | 参数已自动设置 |
| **camera.cpp** | stream_function | 线程函数 |

---

## 📝 实施方案

### 第一阶段：核心功能（当前 SDK）✅ 已完成
- ✅ simple_camera_* 基础接口
- ✅ temp_value_converter 温度转换
- ✅ 测温演示函数

### 第二阶段：高级功能（建议新增）
```cpp
// 1. 增益控制
int simple_camera_set_gain_mode(SimpleCameraHandle_t* handle, GainMode_t mode);

// 2. 帧回调
int simple_camera_set_frame_callback(SimpleCameraHandle_t* handle,
                                    frame_callback_t callback,
                                    void* user_data);

// 3. 过曝保护
int simple_camera_enable_overexposure_protection(SimpleCameraHandle_t* handle, 
                                                  bool enable);
```

### 第三阶段：辅助功能（可选）
```cpp
// 相机参数查询
int simple_camera_get_sensor_info(SimpleCameraHandle_t* handle,
                                  char* sensor_model,  // 传感器型号
                                  float* pixel_pitch); // 像素间距

// 温度校准状态
int simple_camera_get_calibration_status(SimpleCameraHandle_t* handle,
                                         CalibrationStatus_t* status);
```

---

## 🏆 总结

### 暴露原则
1. ✅ **用户真正需要的功能** - 增益控制、回调机制
2. ✅ **C++ 实现更优的功能** - 底层算法（自动增益检测）
3. ❌ **Python 能轻松实现的** - 显示、图像处理
4. ❌ **内部实现细节** - 内存管理、同步机制

### 最终答案
**不需要全部暴露，只暴露 3 个高价值功能：**
1. 帧回调机制（高性能异步）
2. 增益模式控制（扩展测温范围）
3. 过曝保护开关（传感器保护）

其余函数保持隐藏，维护良好的封装性。
