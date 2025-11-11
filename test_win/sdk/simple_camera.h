#ifndef _SIMPLE_CAMERA_H_
#define _SIMPLE_CAMERA_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// DLL 导出宏
#ifdef _WIN32
    #ifdef BUILDING_DLL
        #define SIMPLE_CAMERA_API __declspec(dllexport)
    #else
        #define SIMPLE_CAMERA_API __declspec(dllimport)
    #endif
#else
    #define SIMPLE_CAMERA_API
#endif

typedef struct SimpleCameraHandle_t SimpleCameraHandle_t;

// 相机控制函数
SIMPLE_CAMERA_API SimpleCameraHandle_t* simple_camera_create(void);
SIMPLE_CAMERA_API void simple_camera_destroy(SimpleCameraHandle_t* handle);
SIMPLE_CAMERA_API int simple_camera_open(SimpleCameraHandle_t* handle);
SIMPLE_CAMERA_API int simple_camera_close(SimpleCameraHandle_t* handle);
SIMPLE_CAMERA_API int simple_camera_start_stream(SimpleCameraHandle_t* handle);
SIMPLE_CAMERA_API int simple_camera_stop_stream(SimpleCameraHandle_t* handle);
SIMPLE_CAMERA_API int simple_camera_get_frame(SimpleCameraHandle_t* handle, uint32_t timeout_ms);

// 数据访问函数
SIMPLE_CAMERA_API uint16_t* simple_camera_get_temp_data(SimpleCameraHandle_t* handle);
SIMPLE_CAMERA_API uint8_t* simple_camera_get_image_data(SimpleCameraHandle_t* handle);
SIMPLE_CAMERA_API int simple_camera_get_temp_size(SimpleCameraHandle_t* handle, uint32_t* width, uint32_t* height);
SIMPLE_CAMERA_API int simple_camera_get_image_size(SimpleCameraHandle_t* handle, uint32_t* width, uint32_t* height);
SIMPLE_CAMERA_API int simple_camera_get_info(SimpleCameraHandle_t* handle, uint32_t* width, uint32_t* height, uint32_t* fps);

// 辅助函数
SIMPLE_CAMERA_API float simple_camera_temp_converter(uint16_t temp_val);
SIMPLE_CAMERA_API int simple_camera_set_temp_data(SimpleCameraHandle_t* handle, uint16_t* data, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif
