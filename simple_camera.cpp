#include "simple_camera.h"
#include "camera.h"
#include "data.h"
#include <stdlib.h>
#include <string.h>

struct SimpleCameraHandle_t {
    CameraParam_t camera_param;
    StreamFrameInfo_t stream_frame_info;
    uint8_t* raw_frame;
    uint16_t* temp_frame;
    uint8_t* image_frame;
};

extern "C" {

SimpleCameraHandle_t* simple_camera_create(void) {
    SimpleCameraHandle_t* handle = (SimpleCameraHandle_t*)malloc(sizeof(SimpleCameraHandle_t));
    if (handle) {
        memset(handle, 0, sizeof(SimpleCameraHandle_t));
    }
    return handle;
}

void simple_camera_destroy(SimpleCameraHandle_t* handle) {
    if (handle) {
        free(handle);
    }
}

int simple_camera_open(SimpleCameraHandle_t* handle) {
    if (!handle) return -1;
    
    int ret = ir_camera_open(&handle->camera_param);
    if (ret != 0) return ret;
    
    // 初始化 stream_frame_info
    handle->stream_frame_info.camera_param = handle->camera_param;
    handle->stream_frame_info.image_info.width = 256;
    handle->stream_frame_info.image_info.height = 192;
    handle->stream_frame_info.temp_info.width = 256;
    handle->stream_frame_info.temp_info.height = 192;
    handle->stream_frame_info.image_byte_size = 256 * 192 * 2;
    handle->stream_frame_info.temp_byte_size = 256 * 192 * 2;
    
    // 分配缓冲区
    create_data_demo(&handle->stream_frame_info);
    
    return 0;
}

int simple_camera_close(SimpleCameraHandle_t* handle) {
    if (!handle) return -1;
    
    destroy_data_demo(&handle->stream_frame_info);
    ir_camera_close();
    
    return 0;
}

int simple_camera_start_stream(SimpleCameraHandle_t* handle) {
    if (!handle) return -1;
    return ir_camera_stream_on(&handle->stream_frame_info);
}

int simple_camera_stop_stream(SimpleCameraHandle_t* handle) {
    if (!handle) return -1;
    return ir_camera_stream_off(&handle->stream_frame_info);
}

int simple_camera_get_frame(SimpleCameraHandle_t* handle, uint32_t timeout_ms) {
    if (!handle) return -1;
    
    // 这里应该从 stream_frame_info 获取数据
    // 简化版本：直接返回成功（实际应该等待新帧）
    return 0;
}

uint16_t* simple_camera_get_temp_data(SimpleCameraHandle_t* handle) {
    if (!handle) return NULL;
    return (uint16_t*)handle->stream_frame_info.temp_frame;
}

uint8_t* simple_camera_get_image_data(SimpleCameraHandle_t* handle) {
    if (!handle) return NULL;
    return handle->stream_frame_info.image_frame;
}

int simple_camera_get_temp_size(SimpleCameraHandle_t* handle, uint32_t* width, uint32_t* height) {
    if (!handle || !width || !height) return -1;
    *width = handle->stream_frame_info.temp_info.width;
    *height = handle->stream_frame_info.temp_info.height;
    return 0;
}

int simple_camera_get_image_size(SimpleCameraHandle_t* handle, uint32_t* width, uint32_t* height) {
    if (!handle || !width || !height) return -1;
    *width = handle->stream_frame_info.image_info.width;
    *height = handle->stream_frame_info.image_info.height;
    return 0;
}

int simple_camera_get_info(SimpleCameraHandle_t* handle, uint32_t* width, uint32_t* height, uint32_t* fps) {
    if (!handle || !width || !height || !fps) return -1;
    *width = handle->camera_param.width;
    *height = handle->camera_param.height;
    *fps = handle->camera_param.fps;
    return 0;
}

} // extern "C"
