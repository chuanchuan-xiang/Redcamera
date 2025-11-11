# thermal_camera_sdk.h 与原始工程源文件的函数对应关系

## 📋 总体说明

`libtemperature.so` 包含了以下源文件编译的代码：
- ✅ **simple_camera.cpp** - 新增的简化接口（不在原工程中）
- ✅ **camera.cpp** - 相机控制
- ✅ **temperature.cpp** - 温度转换和测量
- ✅ **display.cpp** - 图像处理和显示
- ✅ **data.cpp** - 数据缓冲管理
- ✅ **cmd.cpp** - 命令处理

---

## 🎯 第一部分：简化相机接口（simple_camera_*）

这些函数是**新封装的接口**，不直接对应原工程中的单个函数，而是**组合调用**原有函数。

### 📌 simple_camera_create()
**对应关系**: 新封装函数
- **内部实现**: 分配 SimpleCameraHandle_t 结构体内存
- **对应的原工程概念**: 无直接对应，封装了数据结构初始化

---

### 📌 simple_camera_destroy()
**对应关系**: 新封装函数
- **内部实现**: 释放句柄及所有缓冲区
- **调用原工程函数**:
  - `destroy_data_demo()` (data.cpp) - 释放帧缓冲区
  - `display_release()` (display.cpp) - 释放显示资源

---

### 📌 simple_camera_open()
**对应关系**: 组合调用多个原工程函数
- **调用流程**:
  ```
  simple_camera_open()
    ├─> ir_camera_open() (camera.cpp:42)
    │    ├─> uvc_camera_init() (外部库)
    │    ├─> uvc_camera_list() (外部库)
    │    ├─> get_dev_index_with_pid_vid() (camera.cpp:11)
    │    ├─> uvc_camera_info_get() (外部库)
    │    ├─> uvc_camera_open() (外部库)
    │    └─> camera_para_set() (camera.cpp:27)
    └─> create_data_demo() (data.cpp:45)
         └─> 分配 raw_frame, image_frame, temp_frame
  ```

---

### 📌 simple_camera_close()
**对应关系**: 组合调用
- **调用流程**:
  ```
  simple_camera_close()
    ├─> ir_camera_stream_off() (camera.cpp:149) - 如果流还在运行
    ├─> ir_camera_close() (camera.cpp:106)
    │    ├─> uvc_camera_close() (外部库)
    │    └─> uvc_camera_release() (外部库)
    └─> destroy_data_demo() (data.cpp:58)
  ```

---

### 📌 simple_camera_start_stream()
**对应关系**: 直接调用
- **调用**: `ir_camera_stream_on()` (camera.cpp:123)
  ```cpp
  int ir_camera_stream_on(StreamFrameInfo_t* stream_frame_info)
  {
      uvc_camera_stream_start();
      pthread_create(&stream_thread, ...);
      // 启动数据接收线程 stream_function()
  }
  ```

---

### 📌 simple_camera_stop_stream()
**对应关系**: 直接调用
- **调用**: `ir_camera_stream_off()` (camera.cpp:149)
  ```cpp
  int ir_camera_stream_off(StreamFrameInfo_t* stream_frame_info)
  {
      is_streaming = 0;
      pthread_join(stream_thread, NULL);
      uvc_camera_stream_close();
  }
  ```

---

### 📌 simple_camera_get_frame()
**对应关系**: 简化版本
- **原工程使用方式**: 多线程 + 信号量同步
  - `stream_function()` (camera.cpp:313) - 持续获取帧
  - `sem_wait(&image_sem)` / `sem_wait(&temp_sem)` - 同步访问
- **simple_camera 方式**: 同步阻塞调用
  ```cpp
  // 直接调用 uvc_camera_stream_frame_get()
  // 然后用 raw_data_cut() 分离图像帧和温度帧
  ```

---

### 📌 simple_camera_get_temp_data()
**对应关系**: 返回内部缓冲区指针
- **对应数据**: `stream_frame_info->temp_frame` (data.cpp 中分配)
- **数据来源**: `raw_data_cut()` 从原始帧中提取

---

### 📌 simple_camera_get_image_data()
**对应关系**: 返回内部缓冲区指针
- **对应数据**: `stream_frame_info->image_frame` (data.cpp 中分配)
- **数据来源**: `raw_data_cut()` 从原始帧中提取

---

### 📌 simple_camera_get_temp_size()
**对应关系**: 返回温度帧尺寸
- **对应字段**: `stream_frame_info->temp_info.width/height`
- **典型值**: 256 x 192

---

### 📌 simple_camera_get_image_size()
**对应关系**: 返回图像帧尺寸
- **对应字段**: `stream_frame_info->image_info.width/height`
- **典型值**: 256 x 192

---

### 📌 simple_camera_get_info()
**对应关系**: 返回相机参数
- **对应字段**:
  - `camera_param.width` = 256
  - `camera_param.height` = 384
  - `camera_param.fps` = 25

---

## 🔥 第二部分：温度转换函数

### 📌 temp_value_converter(uint16_t y14_value)
**直接对应**: `temperature.cpp:26`
```cpp
float temp_value_converter(uint16_t temp_val)
{
    return ((double)temp_val / 64 - 273.15);
}
```
- **功能**: Y14 转摄氏度
- **公式**: Celsius = (Y14 / 64.0) - 273.15
- **C++ 修饰名**: `_Z20temp_value_convertert`

---

### 📌 get_temp_cal_info()
**直接对应**: `temperature.cpp:18`
```cpp
TempCalInfo_t* get_temp_cal_info(void)
{
    return &temp_cal_info;
}
```
- **返回**: 温度校准信息结构体指针
- **包含**: 环境参数、NUC校正表、增益标志
- **C++ 修饰名**: `_Z17get_temp_cal_infov`

---

### 📌 print_cali_info(void* cal_info)
**直接对应**: `temperature.cpp:155`
```cpp
void print_cali_info(TempCalInfo_t* temp_cal_info)
{
    printf("org_env_param.EMS=%d\n", temp_cal_info->org_env_param->EMS);
    printf("org_env_param.TAU=%d\n", temp_cal_info->org_env_param->TAU);
    // ... 打印所有校准参数
}
```
- **功能**: 打印校准信息到控制台
- **C++ 修饰名**: `_Z15print_cali_infoP13TempCalInfo_t`

---

### 📌 calculate_new_env_cali_parameter(...)
**直接对应**: `temperature.cpp:39`
```cpp
int calculate_new_env_cali_parameter(uint16_t* correct_table, 
                                     double ems,    // 发射率
                                     double ta,     // 大气温度
                                     double tu,     // 反射温度
                                     double dist,   // 距离
                                     double hum)    // 湿度
{
    uint16_t tau = 0;
    read_tau(correct_table, hum, ta, dist, &tau);
    new_env_param.EMS = ems * (1 << 14);
    new_env_param.TAU = tau;
    // ... 计算环境校正参数
}
```
- **功能**: 根据环境参数计算温度校正表
- **C++ 修饰名**: `_Z32calculate_new_env_cali_parameterPtddddd`

---

### 📌 calculate_org_env_cali_parameter()
**部分对应**: `temperature.cpp` 中的初始化逻辑
- **功能**: 使用出厂默认参数计算校正表
- **C++ 修饰名**: `_Z32calculate_org_env_cali_parameterv`

---

## 📏 第三部分：温度测量演示函数

### 📌 point_temp_demo(uint16_t* temp_data, void* temp_res)
**直接对应**: `temperature.cpp:178`
```cpp
void point_temp_demo(uint16_t* temp_data, TempDataRes_t temp_res)
{
    printf("point_temp...\n");
    temp_res.max_temp = temp_value_converter(temp_data[temp_res.roi_point.y * 256 + temp_res.roi_point.x]);
    printf("point_temp=%.2f\n", temp_res.max_temp);
}
```
- **功能**: 测量指定点的温度
- **C++ 修饰名**: `_Z15point_temp_demoPt13TempDataRes_t`

---

### 📌 line_temp_demo(uint16_t* temp_data, void* temp_res)
**直接对应**: `temperature.cpp:189`
```cpp
void line_temp_demo(uint16_t* temp_data, TempDataRes_t temp_res)
{
    // 沿直线测量温度分布
    printf("line_temp...\n");
    // 计算起点到终点的温度
}
```
- **功能**: 沿线测温
- **C++ 修饰名**: `_Z14line_temp_demoPt13TempDataRes_t`

---

### 📌 rect_temp_demo(uint16_t* temp_data, void* temp_res)
**直接对应**: `temperature.cpp:208`
```cpp
void rect_temp_demo(uint16_t* temp_data, TempDataRes_t temp_res)
{
    printf("rect_temp...\n");
    // 计算矩形区域内的最高温、最低温、平均温
}
```
- **功能**: 矩形区域测温（最高、最低、平均）
- **C++ 修饰名**: `_Z14rect_temp_demoPt13TempDataRes_t`

---

## 🖼️ 第四部分：未在 SDK 头文件中暴露的原工程函数

这些函数已编译进 libtemperature.so，但没有在 thermal_camera_sdk.h 中声明：

### 🔸 display.cpp 中的函数（已编译但未暴露）

| 函数名 | 位置 | 功能 |
|--------|------|------|
| `display_init()` | display.cpp:51 | 初始化显示参数 |
| `display_release()` | display.cpp:82 | 释放显示资源 |
| `enhance_image_frame()` | display.cpp:100 | 图像增强 |
| `color_image_frame()` | display.cpp:138 | 图像着色 |
| `display_image_process()` | display.cpp:175 | 图像处理流程 |
| `display_one_frame()` | display.cpp:592 | 显示单帧（OpenCV） |
| `display_function()` | display.cpp:778 | 显示线程函数 |
| `rotate_demo()` | display.cpp:303 | 图像旋转 |
| `mirror_flip_demo()` | display.cpp:331 | 镜像翻转 |
| `segment_human_by_real_temperature()` | display.cpp:455 | 人体温度分割 |
| `add_temperature_labels()` | display.cpp:428 | 添加温度标签 |

---

### 🔸 camera.cpp 中的函数（已编译但未完全暴露）

| 函数名 | 位置 | 功能 | SDK 暴露 |
|--------|------|------|----------|
| `ir_camera_open()` | camera.cpp:42 | 打开相机 | ✅ 通过 simple_camera_open |
| `ir_camera_close()` | camera.cpp:106 | 关闭相机 | ✅ 通过 simple_camera_close |
| `ir_camera_stream_on()` | camera.cpp:123 | 开始流 | ✅ 通过 simple_camera_start_stream |
| `ir_camera_stream_off()` | camera.cpp:149 | 停止流 | ✅ 通过 simple_camera_stop_stream |
| `get_dev_index_with_pid_vid()` | camera.cpp:11 | 查找设备索引 | ❌ 内部函数 |
| `camera_para_set()` | camera.cpp:27 | 设置相机参数 | ❌ 内部函数 |
| `auto_gain_switch()` | camera.cpp:166 | 自动增益切换 | ❌ 内部函数 |
| `avoid_overexposure()` | camera.cpp:244 | 避免过曝 | ❌ 内部函数 |
| `stream_function()` | camera.cpp:313 | 流接收线程 | ❌ 内部函数 |
| `ir_camera_stream_on_with_callback()` | camera.cpp:407 | 带回调的流 | ❌ 未暴露 |

---

### 🔸 temperature.cpp 中的函数（部分未暴露）

| 函数名 | 位置 | SDK 暴露 |
|--------|------|----------|
| `temp_value_converter()` | temperature.cpp:26 | ✅ 已暴露 |
| `calculate_new_env_cali_parameter()` | temperature.cpp:39 | ✅ 已暴露 |
| `get_temp_cal_info()` | temperature.cpp:18 | ✅ 已暴露 |
| `print_cali_info()` | temperature.cpp:155 | ✅ 已暴露 |
| `point_temp_demo()` | temperature.cpp:178 | ✅ 已暴露 |
| `line_temp_demo()` | temperature.cpp:189 | ✅ 已暴露 |
| `rect_temp_demo()` | temperature.cpp:208 | ✅ 已暴露 |
| `reverse_temp_frame_to_nuc()` | temperature.cpp:62 | ❌ 未暴露 |
| `temp_calc_with_new_env_calibration()` | temperature.cpp:85 | ❌ 未暴露 |
| `temp_calc_without_any_correct()` | temperature.cpp:127 | ❌ 未暴露 |
| `temperature_function()` | temperature.cpp:224 | ❌ 线程函数 |

---

### 🔸 data.cpp 中的函数（内部使用）

| 函数名 | 位置 | 功能 | SDK 暴露 |
|--------|------|------|----------|
| `init_pthread_sem()` | data.cpp:11 | 初始化信号量 | ❌ 内部函数 |
| `destroy_pthread_sem()` | data.cpp:25 | 销毁信号量 | ❌ 内部函数 |
| `create_data_demo()` | data.cpp:45 | 创建帧缓冲 | ❌ 间接调用 |
| `destroy_data_demo()` | data.cpp:58 | 销毁帧缓冲 | ❌ 间接调用 |

---

## 📊 函数调用关系图

```
用户代码
   │
   ├─> simple_camera_create()          [新封装]
   │      └─> malloc(SimpleCameraHandle_t)
   │
   ├─> simple_camera_open()            [新封装]
   │      ├─> ir_camera_open()         [camera.cpp:42]
   │      │      ├─> uvc_camera_init()
   │      │      ├─> uvc_camera_list()
   │      │      ├─> get_dev_index_with_pid_vid()  [camera.cpp:11]
   │      │      ├─> uvc_camera_info_get()
   │      │      ├─> uvc_camera_open()
   │      │      └─> camera_para_set()  [camera.cpp:27]
   │      └─> create_data_demo()        [data.cpp:45]
   │
   ├─> simple_camera_start_stream()    [新封装]
   │      └─> ir_camera_stream_on()    [camera.cpp:123]
   │             ├─> uvc_camera_stream_start()
   │             └─> pthread_create(stream_function)
   │
   ├─> simple_camera_get_frame()       [新封装 - 阻塞式]
   │      ├─> uvc_camera_stream_frame_get()
   │      └─> raw_data_cut()            [分离图像/温度帧]
   │
   ├─> simple_camera_get_temp_data()   [新封装]
   │      └─> return temp_frame
   │
   ├─> temp_value_converter()          [temperature.cpp:26]
   │      └─> (Y14 / 64.0) - 273.15
   │
   ├─> point_temp_demo()               [temperature.cpp:178]
   ├─> line_temp_demo()                [temperature.cpp:189]
   └─> rect_temp_demo()                [temperature.cpp:208]
```

---

## 🎯 关键区别：原工程 vs SDK

### 原工程使用方式（多线程 + 信号量）
```cpp
// 1. 初始化
ir_camera_open(&camera_param);
create_data_demo(&stream_frame_info);

// 2. 启动多线程
pthread_create(&stream_thread, NULL, stream_function, &stream_frame_info);
pthread_create(&display_thread, NULL, display_function, &stream_frame_info);
pthread_create(&temp_thread, NULL, temperature_function, &stream_frame_info);

// 3. 使用信号量同步
sem_wait(&temp_sem);
process_temperature(stream_frame_info.temp_frame);
sem_post(&temp_done_sem);
```

### SDK 简化方式（单线程阻塞）
```cpp
// 1. 初始化
SimpleCameraHandle_t* camera = simple_camera_create();
simple_camera_open(camera);
simple_camera_start_stream(camera);

// 2. 同步获取帧
while (running) {
    simple_camera_get_frame(camera, 1000);
    uint16_t* temp = simple_camera_get_temp_data(camera);
    // 直接处理
}

// 3. 清理
simple_camera_stop_stream(camera);
simple_camera_close(camera);
simple_camera_destroy(camera);
```

---

## 📝 总结

1. **simple_camera_*** 函数是**新封装的简化接口**，组合调用原工程多个函数
2. **温度相关函数**直接来自 `temperature.cpp`，一一对应
3. **显示相关函数**（display.cpp）已编译进库，但未在 SDK 头文件中暴露
4. **camera.cpp** 的核心函数通过 simple_camera_* 间接调用
5. **data.cpp** 的函数作为内部工具使用

SDK 的设计理念是：**隐藏复杂的多线程同步，提供简洁的同步接口**。
