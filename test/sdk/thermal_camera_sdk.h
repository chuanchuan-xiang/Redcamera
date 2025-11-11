/**
 * ============================================================
 * 红外热成像相机 SDK 完整接口文档
 * Infrared Thermal Camera SDK - Complete API Reference
 * ============================================================
 * 
 * 文件: thermal_camera_sdk.h
 * 版本: 1.0.0
 * 日期: 2025-11-04
 * 库文件: libtemperature.so
 * 
 * 本文档详细说明了 libtemperature.so 中所有可用的函数接口
 * 包括相机控制、温度测量、图像处理等完整功能
 * 
 * 目录：
 * 1. 简化相机接口（simple_camera_*）- 推荐使用 ⭐
 * 2. 温度转换函数（temp_value_converter 等）
 * 3. 温度测量演示（point/line/rect_temp_demo）
 * 4. 环境校准（calculate_env_cali_parameter）
 * 5. 完整使用示例（C 和 Python）
 * 6. 常见问题解答
 */

#ifndef _THERMAL_CAMERA_SDK_H_
#define _THERMAL_CAMERA_SDK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// ============================================================
// 第一部分：简化相机接口（⭐ 推荐使用）
// ============================================================

/**
 * 简化相机句柄（不透明指针）
 * 
 * 说明：
 *   封装了相机的所有内部状态，用户只需通过接口函数操作
 */
typedef struct SimpleCameraHandle_t SimpleCameraHandle_t;


/**
 * 创建相机句柄
 * 
 * 功能：分配并初始化一个新的相机句柄
 * 返回值：相机句柄指针，失败返回 NULL
 * 
 * 使用示例：
 *   SimpleCameraHandle_t* camera = simple_camera_create();
 */
SimpleCameraHandle_t* simple_camera_create(void);


/**
 * 销毁相机句柄
 * 
 * 功能：释放相机句柄及所有资源
 * 参数：handle - 相机句柄
 */
void simple_camera_destroy(SimpleCameraHandle_t* handle);


/**
 * 打开红外相机
 * 
 * 功能：
 *   1. 初始化 USB 相机设备
 *   2. 查找 Realtek 0bda:5840 相机
 *   3. 选择 256x384 分辨率
 *   4. 分配帧缓冲区
 * 
 * 参数：handle - 相机句柄
 * 返回值：0=成功，<0=失败
 * 
 * 相机参数：
 *   - 总分辨率: 256x384
 *   - 图像帧: 256x192 (上半部分)
 *   - 温度帧: 256x192 (下半部分)
 *   - 帧率: 25 fps
 * 
 * 注意：需要 root 权限（sudo）
 */
int simple_camera_open(SimpleCameraHandle_t* handle);


/**
 * 关闭红外相机
 * 
 * 功能：停止流、关闭设备、释放资源
 * 参数：handle - 相机句柄
 * 返回值：0=成功，-1=失败
 */
int simple_camera_close(SimpleCameraHandle_t* handle);


/**
 * 开始流式传输
 * 
 * 功能：启动相机视频流
 * 参数：handle - 相机句柄
 * 返回值：0=成功，<0=失败
 * 
 * 注意：必须先调用 simple_camera_open()
 */
int simple_camera_start_stream(SimpleCameraHandle_t* handle);


/**
 * 停止流式传输
 * 
 * 功能：停止相机视频流
 * 参数：handle - 相机句柄
 * 返回值：0=成功，-1=失败
 */
int simple_camera_stop_stream(SimpleCameraHandle_t* handle);


/**
 * 获取一帧数据（阻塞式）
 * 
 * 功能：
 *   1. 从相机读取一帧原始数据
 *   2. 自动分离图像帧和温度帧
 *   3. 存储到内部缓冲区
 * 
 * 参数：
 *   handle     - 相机句柄
 *   timeout_ms - 超时时间（毫秒），0=默认1000ms
 * 
 * 返回值：0=成功，<0=失败
 * 
 * 使用示例：
 *   if (simple_camera_get_frame(camera, 0) == 0) {
 *       // 成功获取一帧
 *   }
 */
int simple_camera_get_frame(SimpleCameraHandle_t* handle, uint32_t timeout_ms);


/**
 * 获取温度帧数据指针
 * 
 * 功能：返回最近一次获取的温度帧指针
 * 
 * 参数：handle - 相机句柄
 * 返回值：温度数据指针（uint16_t*），失败返回 NULL
 * 
 * 数据格式：
 *   - 分辨率: 256 x 192
 *   - 格式: Y14（每像素 uint16_t）
 *   - 总像素: 49,152
 *   - 坐标访问: data[y * 256 + x]
 *   - 转摄氏度: (Y14值 / 64.0) - 273.15
 * 
 * 使用示例：
 *   uint16_t* temp = simple_camera_get_temp_data(camera);
 *   if (temp != NULL) {
 *       // 中心点温度 (128, 96)
 *       uint16_t y14 = temp[96 * 256 + 128];
 *       float celsius = (y14 / 64.0) - 273.15;
 *   }
 */
uint16_t* simple_camera_get_temp_data(SimpleCameraHandle_t* handle);


/**
 * 获取图像帧数据指针
 * 
 * 功能：返回最近一次获取的图像帧指针
 * 
 * 参数：handle - 相机句柄
 * 返回值：图像数据指针（uint8_t*），失败返回 NULL
 * 
 * 数据格式：
 *   - 分辨率: 256 x 192
 *   - 格式: Y16（每像素 2 字节）
 */
uint8_t* simple_camera_get_image_data(SimpleCameraHandle_t* handle);


/**
 * 获取温度帧尺寸
 * 
 * 参数：
 *   handle - 相机句柄
 *   width  - 输出：宽度（256）
 *   height - 输出：高度（192）
 * 
 * 返回值：0=成功，-1=失败
 */
int simple_camera_get_temp_size(SimpleCameraHandle_t* handle, 
                                uint32_t* width, uint32_t* height);


/**
 * 获取图像帧尺寸
 * 
 * 参数：
 *   handle - 相机句柄
 *   width  - 输出：宽度（256）
 *   height - 输出：高度（192）
 * 
 * 返回值：0=成功，-1=失败
 */
int simple_camera_get_image_size(SimpleCameraHandle_t* handle, 
                                 uint32_t* width, uint32_t* height);


/**
 * 获取相机完整信息
 * 
 * 参数：
 *   handle - 相机句柄
 *   width  - 输出：总分辨率宽度（256）
 *   height - 输出：总分辨率高度（384）
 *   fps    - 输出：帧率（25）
 * 
 * 返回值：0=成功，-1=失败
 */
int simple_camera_get_info(SimpleCameraHandle_t* handle, 
                           uint32_t* width, uint32_t* height, uint32_t* fps);


// ============================================================
// 第二部分：温度转换函数
// ============================================================

/**
 * Y14 单值转摄氏度
 * 
 * 功能：将单个 Y14 格式温度值转换为摄氏度
 * 
 * Y14 格式说明：
 *   - Y14 值 = 开尔文温度 × 64
 *   - 转换公式: 摄氏度 = (Y14值 / 64.0) - 273.15
 * 
 * 参数：y14_value - Y14 格式温度值
 * 返回值：摄氏度温度（float）
 * 
 * 温度对照表：
 *   Y14值    摄氏度     说明
 *   ------   -------   --------
 *   17500    0.3°C     接近冰点
 *   18688    18.9°C    室温偏低
 *   19200    26.9°C    舒适温度
 *   19712    34.9°C    体温附近
 *   20224    42.9°C    发烧温度
 *   20736    50.9°C    高温
 * 
 * 使用示例：
 *   uint16_t y14 = 19200;
 *   float celsius = temp_value_converter(y14);
 *   printf("温度: %.2f°C\n", celsius);
 * 
 * 注意：
 *   - 这是纯计算函数，不需要相机句柄
 *   - 函数在库中的实际名称（C++ 修饰）：_Z20temp_value_convertert
 *   - Python 调用时需要使用修饰后的名称
 */
float temp_value_converter(uint16_t y14_value);


/**
 * 获取温度校准信息
 * 
 * 功能：获取相机的温度校准参数
 * 
 * 返回值：指向 TempCalInfo_t 结构的指针
 * 
 * TempCalInfo_t 包含：
 *   - K, B 校准系数
 *   - 环境参数
 *   - NUC 校正表
 * 
 * 注意：
 *   - 函数实际名称：_Z17get_temp_cal_infov
 *   - 返回的指针指向内部数据，不要释放
 */
void* get_temp_cal_info(void);


/**
 * 打印校准信息
 * 
 * 功能：将温度校准信息打印到控制台
 * 
 * 参数：cal_info - 校准信息指针
 * 
 * 注意：函数实际名称：_Z15print_cali_infoP13TempCalInfo_t
 */
void print_cali_info(void* cal_info);


/**
 * 计算新的环境校准参数
 * 
 * 功能：
 *   根据环境参数（发射率、温度、距离、湿度）计算温度校正表
 * 
 * 参数：
 *   correct_table - 输出：校正表（需预分配 65536 个 uint16_t）
 *   emissivity    - 发射率（0.0~1.0，典型值 0.95）
 *   atm_temp      - 环境温度（摄氏度）
 *   refl_temp     - 反射温度（摄氏度）
 *   distance      - 测量距离（米）
 *   humidity      - 相对湿度（0.0~1.0）
 * 
 * 返回值：0=成功，<0=失败
 * 
 * 使用示例：
 *   uint16_t* table = malloc(65536 * sizeof(uint16_t));
 *   int ret = calculate_new_env_cali_parameter(
 *       table, 0.95, 25.0, 25.0, 1.0, 0.5
 *   );
 * 
 * 注意：函数实际名称：_Z32calculate_new_env_cali_parameterPtddddd
 */
int calculate_new_env_cali_parameter(uint16_t* correct_table,
                                     double emissivity,
                                     double atm_temp,
                                     double refl_temp,
                                     double distance,
                                     double humidity);


/**
 * 计算原始环境校准参数
 * 
 * 功能：使用出厂默认环境参数计算校正表
 * 返回值：0=成功，<0=失败
 * 
 * 注意：函数实际名称：_Z32calculate_org_env_cali_parameterv
 */
int calculate_org_env_cali_parameter(void);


// ============================================================
// 第三部分：温度测量演示函数
// ============================================================

/**
 * 点测温演示
 * 
 * 功能：对温度帧进行点测温分析
 * 
 * 参数：
 *   temp_data - 温度帧数据（Y14 格式）
 *   temp_res  - 温度结果结构
 * 
 * 注意：
 *   - 函数实际名称：_Z15point_temp_demoPt13TempDataRes_t
 *   - 会在控制台打印测温结果
 */
void point_temp_demo(uint16_t* temp_data, void* temp_res);


/**
 * 线测温演示
 * 
 * 功能：沿指定直线测量温度分布
 * 
 * 参数：
 *   temp_data - 温度帧数据
 *   temp_res  - 温度结果结构
 * 
 * 注意：函数实际名称：_Z14line_temp_demoPt13TempDataRes_t
 */
void line_temp_demo(uint16_t* temp_data, void* temp_res);


/**
 * 矩形区域测温演示
 * 
 * 功能：计算矩形区域内的温度统计（平均、最高、最低）
 * 
 * 参数：
 *   temp_data - 温度帧数据
 *   temp_res  - 温度结果结构
 * 
 * 注意：函数实际名称：_Z14rect_temp_demoPt13TempDataRes_t
 */
void rect_temp_demo(uint16_t* temp_data, void* temp_res);


// ============================================================
// 第四部分：完整使用示例
// ============================================================

/**
 * ============================================================
 * C 语言完整示例
 * ============================================================
 * 
 * #include "thermal_camera_sdk.h"
 * #include <stdio.h>
 * 
 * int main(void) {
 *     // 1. 创建相机
 *     SimpleCameraHandle_t* camera = simple_camera_create();
 *     
 *     // 2. 打开相机
 *     if (simple_camera_open(camera) != 0) {
 *         printf("打开相机失败\n");
 *         return -1;
 *     }
 *     
 *     // 3. 开始流
 *     simple_camera_start_stream(camera);
 *     
 *     // 4. 获取 100 帧
 *     for (int i = 0; i < 100; i++) {
 *         // 获取一帧
 *         if (simple_camera_get_frame(camera, 1000) == 0) {
 *             // 获取温度数据
 *             uint16_t* temp = simple_camera_get_temp_data(camera);
 *             
 *             // 中心点温度
 *             uint16_t center_y14 = temp[96 * 256 + 128];
 *             float celsius = temp_value_converter(center_y14);
 *             
 *             printf("帧 %d: %.2f°C\n", i+1, celsius);
 *         }
 *     }
 *     
 *     // 5. 清理
 *     simple_camera_stop_stream(camera);
 *     simple_camera_close(camera);
 *     simple_camera_destroy(camera);
 *     
 *     return 0;
 * }
 * 
 * 编译：
 *   gcc -o app main.c -L./sdk -ltemperature -Wl,-rpath=./sdk
 * 
 * 运行：
 *   sudo ./app
 */


/**
 * ============================================================
 * Python 完整示例
 * ============================================================
 * 
 * import ctypes
 * import numpy as np
 * 
 * # 加载库
 * lib = ctypes.CDLL('./sdk/libtemperature.so')
 * 
 * # 设置函数签名
 * lib.simple_camera_create.restype = ctypes.c_void_p
 * lib.simple_camera_get_temp_data.argtypes = [ctypes.c_void_p]
 * lib.simple_camera_get_temp_data.restype = ctypes.POINTER(ctypes.c_uint16)
 * 
 * # 温度转换函数（注意使用 C++ 修饰名）
 * lib._Z20temp_value_convertert.argtypes = [ctypes.c_uint16]
 * lib._Z20temp_value_convertert.restype = ctypes.c_float
 * 
 * # 创建相机
 * camera = lib.simple_camera_create()
 * 
 * # 打开相机
 * if lib.simple_camera_open(camera) == 0:
 *     print("相机打开成功")
 *     
 *     # 开始流
 *     lib.simple_camera_start_stream(camera)
 *     
 *     # 获取 10 帧
 *     for i in range(10):
 *         if lib.simple_camera_get_frame(camera, 1000) == 0:
 *             # 获取温度数据
 *             temp_ptr = lib.simple_camera_get_temp_data(camera)
 *             temp_array = np.ctypeslib.as_array(temp_ptr, shape=(192, 256))
 *             
 *             # 批量转换为摄氏度
 *             celsius = (temp_array / 64.0) - 273.15
 *             
 *             print(f"帧 {i+1}:")
 *             print(f"  范围: {celsius.min():.1f} ~ {celsius.max():.1f}°C")
 *             print(f"  平均: {celsius.mean():.1f}°C")
 *             print(f"  中心: {celsius[96, 128]:.1f}°C")
 *     
 *     # 清理
 *     lib.simple_camera_stop_stream(camera)
 *     lib.simple_camera_close(camera)
 * 
 * lib.simple_camera_destroy(camera)
 */


// ============================================================
// 第五部分：常见问题解答
// ============================================================

/**
 * Q1: 为什么需要 sudo 权限？
 * A: 访问 USB 设备需要 root 权限。可以通过 udev 规则避免：
 *    创建 /etc/udev/rules.d/99-thermal-camera.rules：
 *    SUBSYSTEM=="usb", ATTR{idVendor}=="0bda", ATTR{idProduct}=="5840", MODE="0666"
 *    执行: sudo udevadm control --reload-rules
 * 
 * Q2: 如何查看库中的函数？
 * A: 使用 nm 命令：
 *    nm -D libtemperature.so | grep " T "
 * 
 * Q3: 如何在 Python 中使用 C++ 修饰名？
 * A: 1. 查看实际名称：
 *       nm -D libtemperature.so | grep temp_value_converter
 *       输出：_Z20temp_value_convertert
 *    
 *    2. 在 Python 中使用：
 *       lib._Z20temp_value_convertert.argtypes = [ctypes.c_uint16]
 *       lib._Z20temp_value_convertert.restype = ctypes.c_float
 *       celsius = lib._Z20temp_value_convertert(y14_value)
 * 
 * Q4: 温度数据的坐标系？
 * A: 数组索引 = y * 256 + x
 *    - 原点 (0,0) 在左上角
 *    - x 范围: 0~255
 *    - y 范围: 0~191
 *    - 中心点: (128, 96)
 * 
 * Q5: 如何提高帧率？
 * A: - 减少处理时间
 *    - 使用多线程
 *    - 不要在获取帧的循环中做耗时操作
 * 
 * Q6: 如何保存温度数据？
 * A: 方法 1 - 二进制：
 *       FILE* fp = fopen("temp.bin", "wb");
 *       fwrite(temp_data, sizeof(uint16_t), 256*192, fp);
 *       fclose(fp);
 *    
 *    方法 2 - Python NumPy：
 *       np.save('temp.npy', temp_array)
 *    
 *    方法 3 - CSV：
 *       np.savetxt('temp.csv', celsius_array, delimiter=',')
 */


// ============================================================
// 第六部分：库信息
// ============================================================

/**
 * 库信息：
 *   名称: libtemperature.so
 *   大小: ~100 KB
 *   
 *   依赖库:
 *     - libirtemp.so (温度处理)
 *     - libirprocess.so (图像处理)
 *     - libiruvc.so (USB 相机驱动)
 *     - libirparse.so (数据解析)
 *     - libopencv_core.so
 *     - libopencv_imgproc.so
 *     - libpthread.so
 * 
 *   支持的相机:
 *     - Realtek 0bda:5840 红外热成像相机
 *     - 分辨率: 256x384 (图像 192 + 温度 192)
 *     - 帧率: 25 fps
 *     - 接口: USB 2.0/3.0
 * 
 *   系统要求:
 *     - Linux (Ubuntu 18.04+)
 *     - GCC 7.0+
 *     - Python 3.6+ (如果使用 Python)
 *     - OpenCV 4.x
 */

#ifdef __cplusplus
}
#endif

#endif /* _THERMAL_CAMERA_SDK_H_ */
