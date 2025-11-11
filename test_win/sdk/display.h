#ifndef _DISPALAY_H_
#define _DISPALAY_H_
#define  _CRT_SECURE_NO_WARNINGS
#include <stdint.h>
#include "data.h"
#include "libirparse.h"
#include "libirprocess.h"
#include "cmd.h"
#include "temperature.h"

#define OPENCV_ENABLE
#ifdef OPENCV_ENABLE
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp> 
#include <opencv2/highgui/highgui_c.h> 
//using namespace cv;
#endif

//initial the parameters for displaying
void display_init(StreamFrameInfo_t* stream_frame_info);

//release the parameters
void display_release(void);

//display one frame
void display_one_frame(StreamFrameInfo_t* stream_frame_info);

//display thread
void* display_function(void* threadarg);

// 人体温度分割参数
#define HUMAN_TEMP_MIN_CELSIUS 28.0f
#define HUMAN_TEMP_MAX_CELSIUS 40.0f

// 人体分割模式开关
extern uint8_t human_segmentation_enabled;

// 基于真实温度的人体分割函数
void segment_human_by_real_temperature(uint16_t* y14_data, int width, int height, uint8_t* dst_frame);

#endif
