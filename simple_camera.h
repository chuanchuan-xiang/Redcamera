#ifndef _SIMPLE_CAMERA_H_
#define _SIMPLE_CAMERA_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct SimpleCameraHandle_t SimpleCameraHandle_t;

// 相机控制函数
SimpleCameraHandle_t* simple_camera_create(void);
void simple_camera_destroy(SimpleCameraHandle_t* handle);
int simple_camera_open(SimpleCameraHandle_t* handle);
int simple_camera_close(SimpleCameraHandle_t* handle);
int simple_camera_start_stream(SimpleCameraHandle_t* handle);
int simple_camera_stop_stream(SimpleCameraHandle_t* handle);
int simple_camera_get_frame(SimpleCameraHandle_t* handle, uint32_t timeout_ms);

// 数据访问函数
uint16_t* simple_camera_get_temp_data(SimpleCameraHandle_t* handle);
uint8_t* simple_camera_get_image_data(SimpleCameraHandle_t* handle);
int simple_camera_get_temp_size(SimpleCameraHandle_t* handle, uint32_t* width, uint32_t* height);
int simple_camera_get_image_size(SimpleCameraHandle_t* handle, uint32_t* width, uint32_t* height);
int simple_camera_get_info(SimpleCameraHandle_t* handle, uint32_t* width, uint32_t* height, uint32_t* fps);

#ifdef __cplusplus
}
#endif

#endif
