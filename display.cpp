#include "display.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "libiruvc.h"
#ifdef THERMAL_CAM_CMD
#include "thermal_cam_cmd.h"
#endif
time_t timer0, timer1;
uint8_t is_displaying = 0;
uint8_t* image_tmp_frame1 = NULL;
uint8_t* image_tmp_frame2 = NULL;

// 人体分割模式开关
uint8_t human_segmentation_enabled = 0;

// 颜色条参数
#define COLOR_BAR_WIDTH 40        // 颜色条宽度
#define COLOR_BAR_HEIGHT 256      // 颜色条高度
#define COLOR_BAR_MARGIN 20       // 颜色条与图像的间距
#define TEMP_LABEL_COUNT 11       // 温度标签数量（0-10，共11个）

// 动态温度范围 - 根据实际测量值自动调整
// 注意：如果需要固定范围，可以在这里设置，否则使用实际温度

#if defined(_WIN32)
int gettimeofday(struct timeval* tp, struct timezone* tzp)
{
	// Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
	// This magic number is the number of 100 nanosecond intervals since January 1, 1601 (UTC)
	// until 00:00:00 January 1, 1970 
	static const uint64_t EPOCH = ((uint64_t)116444736000000000ULL);

	SYSTEMTIME  system_time;
	FILETIME    file_time;
	uint64_t    time;

	GetSystemTime(&system_time);
	SystemTimeToFileTime(&system_time, &file_time);
	time = ((uint64_t)file_time.dwLowDateTime);
	time += ((uint64_t)file_time.dwHighDateTime) << 32;

	tp->tv_sec = (long)((time - EPOCH) / 10000000L);
	tp->tv_usec = (long)(system_time.wMilliseconds * 1000);
	return 0;
}
#endif 

//init the display parameters
void display_init(StreamFrameInfo_t* stream_frame_info)
{
	// start timers (use global variables, not local)
	timer0 = time(NULL);
	timer1 = timer0;

	int pixel_size = stream_frame_info->image_info.width * stream_frame_info->image_info.height;
	// allocate temporary buffers: worst-case 3 bytes per pixel for RGB/BGR or 2 for Y14
	if (image_tmp_frame1 == NULL)
	{
		image_tmp_frame1 = (uint8_t*)malloc((size_t)pixel_size * 3);
		if (image_tmp_frame1 == NULL) {
			fprintf(stderr, "display_init: failed to allocate image_tmp_frame1\n");
			return;
		}
	}

	if (image_tmp_frame2 == NULL)
	{
		image_tmp_frame2 = (uint8_t*)malloc((size_t)pixel_size * 3);
		if (image_tmp_frame2 == NULL) {
			fprintf(stderr, "display_init: failed to allocate image_tmp_frame2\n");
			// free previous to avoid leak
			free(image_tmp_frame1);
			image_tmp_frame1 = NULL;
			return;
		}
	}
}

//recyle the display parameters
void display_release(void)
{
	//is_displaying = 0;

	if (image_tmp_frame1 != NULL)
	{
		free(image_tmp_frame1);
		image_tmp_frame1 = NULL;
	}

	if (image_tmp_frame2 != NULL)
	{
		free(image_tmp_frame2);
		image_tmp_frame2 = NULL;
	}
}

//enhance the image frame by the frameinfo
int enhance_image_frame(uint16_t* src_frame, FrameInfo_t* frameinfo, uint16_t* dst_frame)
{
	int pix_num = frameinfo->width * frameinfo->height;
	
	// 未标定时，直接复制原始数据，不做温度增强
	if (frameinfo->img_enhance_status == IMG_ENHANCE_ON)
	{
		// 找到实际数据范围
		uint16_t min_val = 65535, max_val = 0;
		for (int i = 0; i < pix_num; i++)
		{
			if (src_frame[i] < min_val) min_val = src_frame[i];
			if (src_frame[i] > max_val) max_val = src_frame[i];
		}
		
		// 简单的线性拉伸到全范围
		for (int i = 0; i < pix_num; i++)
		{
			if (max_val > min_val)
			{
				dst_frame[i] = ((src_frame[i] - min_val) * 16383) / (max_val - min_val);
			}
			else
			{
				dst_frame[i] = src_frame[i];
			}
		}
	}
	else
	{
		memcpy(dst_frame, src_frame, pix_num * 2);
	}
	return 0;
}

//color the image frame
// src_frame: input Y14 buffer (uint8_t* but interpreted as uint16_t* when Y14)
// dst_frame: output buffer (size depends on frameinfo->output_format)
void color_image_frame(uint8_t* src_frame, FrameInfo_t* frameinfo, uint8_t* dst_frame)
{
	int pix_num = frameinfo->width * frameinfo->height;

	// we expect src_frame to be Y14 (2 bytes per pixel)
	// we will first map Y14 -> YUYV pseudocolor (YUV422, 2 bytes per pixel),
	// then convert to RGB/BGR if needed
	switch (frameinfo->output_format)
	{
	case OUTPUT_FMT_YUV422:
		// YUV422 destination: map directly to YUYV pseudocolor
		// byte_size = pix_num * 2
		y14_map_to_yuyv_pseudocolor((uint16_t*)src_frame, pix_num, IRPROC_COLOR_MODE_3, dst_frame);
		frameinfo->byte_size = pix_num * 2;
		break;

	case OUTPUT_FMT_RGB888:
		// map to YUYV pseudocolor first, then convert to RGB
		// image_tmp_frame2 will hold YUV422 (pix_num * 2)
		y14_map_to_yuyv_pseudocolor((uint16_t*)src_frame, pix_num, IRPROC_COLOR_MODE_3, image_tmp_frame2);
		// convert YUV422 -> RGB888
		yuv422_to_rgb((uint8_t*)image_tmp_frame2, pix_num, dst_frame);
		frameinfo->byte_size = pix_num * 3;
		break;

	case OUTPUT_FMT_BGR888:
	default:
		// map to YUYV pseudocolor first, convert to RGB then swap channels to BGR
		y14_map_to_yuyv_pseudocolor((uint16_t*)src_frame, pix_num, IRPROC_COLOR_MODE_6, image_tmp_frame2);
		yuv422_to_rgb((uint8_t*)image_tmp_frame2, pix_num, image_tmp_frame1); // temp rgb in image_tmp_frame1
		rgb_to_bgr(image_tmp_frame1, pix_num, dst_frame);
		frameinfo->byte_size = pix_num * 3;
		break;
	}
}

//convert the image process  image_tmp_frame2 is the default output frame
void display_image_process(uint8_t* image_frame, int pix_num, FrameInfo_t* frameinfo)
{
	ImageRes_t image_res = { frameinfo->width,frameinfo->height };
	if (frameinfo->input_format == INPUT_FMT_Y14 || frameinfo->input_format == INPUT_FMT_Y16)
	{
		if (frameinfo->input_format == INPUT_FMT_Y16)
		{
			// convert in-place Y16 -> Y14
			y16_to_y14((uint16_t*)image_frame, pix_num, (uint16_t*)image_frame);
		}

		// enhance (src -> image_tmp_frame1 as Y14)
		enhance_image_frame((uint16_t*)image_frame, frameinfo, (uint16_t*)image_tmp_frame1);

		// If pseudo color is enabled, handle via color_image_frame()
		if (frameinfo->pseudo_color_status == PSEUDO_COLOR_ON)
		{
			// color_image_frame will write final bytes into image_tmp_frame2 (or dst)
			color_image_frame(image_tmp_frame1, frameinfo, image_tmp_frame2);
			return;
		}

		// non-pseudocolor paths
		switch (frameinfo->output_format)
		{
			case OUTPUT_FMT_Y14:
			{
				frameinfo->byte_size = pix_num * 2;
				memcpy(image_tmp_frame2, image_tmp_frame1, frameinfo->byte_size);
				break;
			}
			case OUTPUT_FMT_YUV444:
			{
				frameinfo->byte_size = pix_num * 3;
				y14_to_yuv444((uint16_t *)image_tmp_frame1, pix_num, (uint8_t*)image_tmp_frame2);
				break;
			}
			case OUTPUT_FMT_YUV422:
			{
				frameinfo->byte_size = pix_num * 2;
				// no pseudo color: convert y14 -> yuv444 -> yuv422 (reuse tmp buffers)
				y14_to_yuv444((uint16_t*)image_tmp_frame1, pix_num, (uint8_t*)image_tmp_frame2);
				memcpy(image_tmp_frame1, image_tmp_frame2, frameinfo->byte_size); // NOTE: here byte_size is pix_num*2 (not exact for yuv444), but keep logic similar to original
				yuv444_to_yuv422((uint8_t*)image_tmp_frame1, pix_num, (uint8_t*)image_tmp_frame2);
				break;
			}
			case OUTPUT_FMT_RGB888:
			{
				frameinfo->byte_size = pix_num * 3;
				y14_to_rgb((uint16_t*)image_tmp_frame1, pix_num, image_tmp_frame2);
				break;
			}
			case OUTPUT_FMT_BGR888:
			default:
			{
				frameinfo->byte_size = pix_num * 3;
				y14_to_rgb((uint16_t*)image_tmp_frame1, pix_num, image_tmp_frame2);
				rgb_to_bgr(image_tmp_frame2, pix_num, image_tmp_frame2); // in-place if supported
				break;
			}
		}
	}
	else if (frameinfo->input_format == INPUT_FMT_YUV422)
	{
		switch (frameinfo->output_format)
		{
			case OUTPUT_FMT_Y14:
			{
				frameinfo->byte_size = 0;
				printf("convert error!\n");
				break;
			}
			case OUTPUT_FMT_YUV444:
			{
				frameinfo->byte_size = 0;
				printf("convert error!\n");
				break;
			}
			case OUTPUT_FMT_YUV422:
			{
				frameinfo->byte_size = pix_num * 2;
				memcpy(image_tmp_frame2, image_frame, pix_num * 2);
				break;
			}
			case OUTPUT_FMT_RGB888:
			{
				frameinfo->byte_size = pix_num * 3;
				yuv422_to_rgb((uint8_t*)image_frame, pix_num, image_tmp_frame2);
				break;
			}
			case OUTPUT_FMT_BGR888:
			default:
			{
				frameinfo->byte_size = pix_num * 3;
				yuv422_to_rgb((uint8_t*)image_frame, pix_num, image_tmp_frame1);
				rgb_to_bgr(image_tmp_frame1, pix_num, image_tmp_frame2);
				break;
			}
		}
	}
}

irproc_src_fmt_t format_converter(OutputFormat_t output_format)
{
	switch (output_format)
	{
	case OUTPUT_FMT_Y14:
		return IRPROC_SRC_FMT_Y14;
		break;
	case OUTPUT_FMT_YUV422:
		return IRPROC_SRC_FMT_YUV422;
		break;
	case OUTPUT_FMT_YUV444:
		return IRPROC_SRC_FMT_YUV444;
		break;
	case OUTPUT_FMT_RGB888:
		return IRPROC_SRC_FMT_RGB888;
		break;
	case OUTPUT_FMT_BGR888:
		return IRPROC_SRC_FMT_BGR888;
		break;
	default:
		return IRPROC_SRC_FMT_Y14;
		break;
	}
}

//rotate the frame data according to rotate_side
void rotate_demo(FrameInfo_t* frame_info, uint8_t* frame, RotateSide_t rotate_side)
{
	ImageRes_t image_res = { frame_info->width,frame_info->height };
	irproc_src_fmt_t tmp_fmt = format_converter(frame_info->output_format);

	switch (rotate_side)
	{
	case NO_ROTATE:
		// no-op
		break;
	case LEFT_90D:
		rotate_left_90(frame, image_res, tmp_fmt, image_tmp_frame1);
		memcpy(frame, image_tmp_frame1, frame_info->byte_size);
		break;
	case RIGHT_90D:
		rotate_right_90(frame, image_res, tmp_fmt, image_tmp_frame1);
		memcpy(frame, image_tmp_frame1, frame_info->byte_size);
		break;
	case ROTATE_180D:
		rotate_180(frame, image_res, tmp_fmt, image_tmp_frame1);
		memcpy(frame, image_tmp_frame1, frame_info->byte_size);
		break;
	default:
		break;
	}
}

//mirror/flip the frame data according to mirror_flip_status
void mirror_flip_demo(FrameInfo_t* frame_info, uint8_t* frame, MirrorFlipStatus_t mirror_flip_status)
{
	ImageRes_t image_res = { frame_info->width,frame_info->height };
	irproc_src_fmt_t tmp_fmt = format_converter(frame_info->output_format);

	switch (mirror_flip_status)
	{
	case STATUS_NO_MIRROR_FLIP:
		// no-op
		break;
	case STATUS_ONLY_MIRROR:
		mirror(frame, image_res, tmp_fmt, image_tmp_frame1);
		memcpy(frame, image_tmp_frame1, frame_info->byte_size);
		break;
	case STATUS_ONLY_FLIP:
		flip(frame, image_res, tmp_fmt, image_tmp_frame1);
		memcpy(frame, image_tmp_frame1, frame_info->byte_size);
		break;
	case STATUS_MIRROR_FLIP:
		mirror(frame, image_res, tmp_fmt, image_tmp_frame1);
		flip(image_tmp_frame1, image_res, tmp_fmt, frame);
		break;
	default:
		break;
	}
}

// 创建动态颜色对比条
// 参数:
//   height: 颜色条的高度
//   width: 颜色条的宽度
//   color_mode: 伪彩色模式
//   max_temp: 当前实际最高温度（摄氏度）
//   min_temp: 当前实际最低温度（摄氏度）
// 返回值: OpenCV Mat对象（BGR格式）
#ifdef OPENCV_ENABLE
cv::Mat create_color_bar(int height, int width, irproc_color_mode_t color_mode, 
                         float max_temp, float min_temp)
{
	// 创建一个临时的Y14数组，从高到低线性分布
	int bar_pixels = height * width;
	uint16_t* y14_gradient = (uint16_t*)malloc(bar_pixels * sizeof(uint16_t));
	uint8_t* yuv_buffer = (uint8_t*)malloc(bar_pixels * 2);  // YUV422格式
	uint8_t* rgb_buffer = (uint8_t*)malloc(bar_pixels * 3);  // RGB格式
	uint8_t* bgr_buffer = (uint8_t*)malloc(bar_pixels * 3);  // BGR格式
	
	if (!y14_gradient || !yuv_buffer || !rgb_buffer || !bgr_buffer) {
		if (y14_gradient) free(y14_gradient);
		if (yuv_buffer) free(yuv_buffer);
		if (rgb_buffer) free(rgb_buffer);
		if (bgr_buffer) free(bgr_buffer);
		return cv::Mat(height, width, CV_8UC3, cv::Scalar(0, 0, 0));
	}
	
	// 生成Y14梯度：从上到下，从高温到低温（16383到0）
	for (int y = 0; y < height; y++) {
		// 计算当前行的Y14值（线性插值）
		uint16_t y14_value = (uint16_t)(16383 - (y * 16383 / (height - 1)));
		for (int x = 0; x < width; x++) {
			y14_gradient[y * width + x] = y14_value;
		}
	}
	
	// 使用与主图像相同的伪彩色映射函数
	y14_map_to_yuyv_pseudocolor(y14_gradient, bar_pixels, color_mode, yuv_buffer);
	
	// 转换为RGB
	yuv422_to_rgb(yuv_buffer, bar_pixels, rgb_buffer);
	
	// 转换为BGR（OpenCV使用BGR格式）
	rgb_to_bgr(rgb_buffer, bar_pixels, bgr_buffer);
	
	// 创建OpenCV Mat
	cv::Mat color_bar(height, width, CV_8UC3);
	memcpy(color_bar.data, bgr_buffer, bar_pixels * 3);
	
	// 释放临时缓冲区
	free(y14_gradient);
	free(yuv_buffer);
	free(rgb_buffer);
	free(bgr_buffer);
	
	// 添加温度刻度线
	for (int i = 0; i < TEMP_LABEL_COUNT; i++) {
		int y_pos = (int)(i * (height - 1) / (TEMP_LABEL_COUNT - 1));
		
		// 绘制刻度线（白色）
		cv::line(color_bar, cv::Point(0, y_pos), cv::Point(5, y_pos), 
		         cv::Scalar(255, 255, 255), 1);
		cv::line(color_bar, cv::Point(width-5, y_pos), cv::Point(width-1, y_pos), 
		         cv::Scalar(255, 255, 255), 1);
	}
	
	return color_bar;
}

// 在颜色条右侧添加动态温度标签
void add_temperature_labels(cv::Mat& combined_image, int bar_x, int bar_y, 
                            int bar_height, float max_temp, float min_temp)
{
	char temp_text[32];
	for (int i = 0; i < TEMP_LABEL_COUNT; i++) {
		// 根据实际温度范围计算温度值（动态）
		float temp = max_temp - (max_temp - min_temp) * i / (TEMP_LABEL_COUNT - 1);
		int y_pos = bar_y + (int)(i * (bar_height - 1) / (TEMP_LABEL_COUNT - 1));
		
		sprintf(temp_text, "%.1f", temp);
		
		// 文字位置：颜色条右侧
		int text_x = bar_x + COLOR_BAR_WIDTH + 5;
		int text_y = y_pos + 4;  // 文字垂直居中对齐
		
		// 绘制文字（黑色阴影 + 白色文字）
		putText(combined_image, temp_text, cv::Point(text_x + 1, text_y + 1), 
		        cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 0, 0), 1, 8);
		putText(combined_image, temp_text, cv::Point(text_x, text_y), 
		        cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(255, 255, 255), 1, 8);
	}
}

// 基于真实温度的人体分割函数
// y14_data: Y14格式的图像数据
// width, height: 图像尺寸
// dst_frame: 输出BGR图像
void segment_human_by_real_temperature(uint16_t* y14_data, int width, int height, uint8_t* dst_frame)
{
	if (y14_data == NULL || dst_frame == NULL) {
		return;
	}
	
	int pix_num = width * height;
	TempDataRes_t temp_res = { (uint16_t)width, (uint16_t)height };
	
	// 创建掩码后的Y14数据
	uint16_t* masked_y14 = (uint16_t*)malloc(pix_num * sizeof(uint16_t));
	if (masked_y14 == NULL) {
		fprintf(stderr, "segment_human_by_real_temperature: failed to allocate masked_y14\n");
		return;
	}
	
	// 统计人体像素数量和温度范围
	int human_pixel_count = 0;
	float min_human_temp = 999.0f;
	float max_human_temp = -999.0f;
	uint16_t min_y14 = 65535;
	uint16_t max_y14 = 0;
	
	// 第一遍扫描：统计人体区域的温度和Y14范围
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			int pixel_index = y * width + x;
			Dot_t point = { (uint16_t)x, (uint16_t)y };
			uint16_t temp_raw = 0;  // 单位：1/64 K
			
			// 使用SDK函数获取该点的真实温度
			if (get_point_temp(y14_data, temp_res, point, &temp_raw) == IRTEMP_SUCCESS) {
				float temp_celsius = temp_value_converter(temp_raw);
				
				// 判断是否在人体温度范围内
				if (temp_celsius >= HUMAN_TEMP_MIN_CELSIUS && temp_celsius <= HUMAN_TEMP_MAX_CELSIUS) {
					human_pixel_count++;
					
					// 统计温度范围
					if (temp_celsius < min_human_temp) min_human_temp = temp_celsius;
					if (temp_celsius > max_human_temp) max_human_temp = temp_celsius;
					
					// 统计Y14范围
					if (y14_data[pixel_index] < min_y14) min_y14 = y14_data[pixel_index];
					if (y14_data[pixel_index] > max_y14) max_y14 = y14_data[pixel_index];
				}
			}
		}
	}
	
	// 第二遍扫描：生成掩码并进行线性拉伸
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			int pixel_index = y * width + x;
			Dot_t point = { (uint16_t)x, (uint16_t)y };
			uint16_t temp_raw = 0;  // 单位：1/64 K
			
			// 使用SDK函数获取该点的真实温度
			if (get_point_temp(y14_data, temp_res, point, &temp_raw) == IRTEMP_SUCCESS) {
				float temp_celsius = temp_value_converter(temp_raw);
				
				// 判断是否在人体温度范围内
				if (temp_celsius >= HUMAN_TEMP_MIN_CELSIUS && temp_celsius <= HUMAN_TEMP_MAX_CELSIUS) {
					// 在范围内，将Y14值线性拉伸到全范围(0-16383)以获得更好的伪彩色显示
					if (max_y14 > min_y14) {
						float normalized = (float)(y14_data[pixel_index] - min_y14) / (float)(max_y14 - min_y14);
						masked_y14[pixel_index] = (uint16_t)(normalized * 16383.0f);
					} else {
						// 如果所有人体像素Y14值相同，设为中间值
						masked_y14[pixel_index] = 8191;
					}
				} else {
					// 不在范围内，设为0（背景）
					masked_y14[pixel_index] = 0;
				}
			} else {
				// 解算失败，设为背景
				masked_y14[pixel_index] = 0;
			}
		}
	}
	
	// 打印分割统计信息
	printf("[Human Segmentation] 温度阈值: %.1f-%.1f°C\n", 
	       HUMAN_TEMP_MIN_CELSIUS, HUMAN_TEMP_MAX_CELSIUS);
	printf("[Human Segmentation] 人体像素: %d/%d (%.1f%%)\n",
	       human_pixel_count, pix_num, 100.0f * human_pixel_count / pix_num);
	if (human_pixel_count > 0) {
		printf("[Human Segmentation] 人体温度范围: %.2f-%.2f°C\n",
		       min_human_temp, max_human_temp);
		printf("[Human Segmentation] Y14拉伸: %d-%d → 0-16383 (线性映射到全伪彩色范围)\n",
		       min_y14, max_y14);
	}
	
	// 分配YUYV伪彩色缓冲区
	uint8_t* yuyv_buffer = (uint8_t*)malloc(pix_num * 2);
	if (yuyv_buffer == NULL) {
		fprintf(stderr, "segment_human_by_real_temperature: failed to allocate yuyv_buffer\n");
		free(masked_y14);
		return;
	}
	
	// 将掩码后的Y14转换为YUYV伪彩色
	y14_map_to_yuyv_pseudocolor(masked_y14, pix_num, IRPROC_COLOR_MODE_6, yuyv_buffer);
	
	// 分配RGB缓冲区
	uint8_t* rgb_buffer = (uint8_t*)malloc(pix_num * 3);
	if (rgb_buffer == NULL) {
		fprintf(stderr, "segment_human_by_real_temperature: failed to allocate rgb_buffer\n");
		free(masked_y14);
		free(yuyv_buffer);
		return;
	}
	
	// 将YUYV转换为RGB
	yuv422_to_rgb(yuyv_buffer, pix_num, rgb_buffer);
	
	// 将RGB转换为BGR (OpenCV使用BGR格式)
	rgb_to_bgr(rgb_buffer, pix_num, dst_frame);
	
	// 强制将背景像素设置为纯黑色 (0, 0, 0)
	for (int i = 0; i < pix_num; i++) {
		if (masked_y14[i] == 0) {
			dst_frame[i * 3 + 0] = 0;  // B
			dst_frame[i * 3 + 1] = 0;  // G
			dst_frame[i * 3 + 2] = 0;  // R
		}
	}
	
	// 释放临时缓冲区
	free(masked_y14);
	free(yuyv_buffer);
	free(rgb_buffer);
}
#endif

//display the frame by opencv 
void display_one_frame(StreamFrameInfo_t* stream_frame_info)
{
	if (stream_frame_info == NULL)
	{
		return;
	}

	char key_press = 0;
	int rst = 0;
	static struct timeval now_time,last_time;
	gettimeofday(&now_time, NULL);
	float frame = 1000000 / (double)((now_time.tv_sec - last_time.tv_sec)*1000000+\
					(now_time.tv_usec - last_time.tv_usec));
	memcpy(&last_time, &now_time, sizeof(now_time));

	char frameText[10] = { " " };
	sprintf(frameText, "%.2f", frame);

	// 获取最高温度和最低温度
	uint16_t max_temp_raw = 0;
	uint16_t min_temp_raw = 0;
	float max_temp_celsius = 0.0f;
	float min_temp_celsius = 0.0f;
	
#ifdef THERMAL_CAM_CMD
	// 调用温度解算函数
	if (tpd_get_max_temp(&max_temp_raw) == 0) {
		// 温度单位是 1/16 K，转换为摄氏度: °C = K - 273.15
		max_temp_celsius = (max_temp_raw / 16.0f) - 273.15f;
	}
	if (tpd_get_min_temp(&min_temp_raw) == 0) {
		min_temp_celsius = (min_temp_raw / 16.0f) - 273.15f;
	}
#endif

	int pix_num = stream_frame_info->image_info.width * stream_frame_info->image_info.height;
	int width = stream_frame_info->image_info.width;
	int height = stream_frame_info->image_info.height;
#ifndef OPENCV_ENABLE
	printf("raw data=%d\n", ((uint16_t*)stream_frame_info->image_frame)[1000]);
#endif

	// 如果启用了人体分割模式，使用温度数据进行分割
	if (human_segmentation_enabled && stream_frame_info->temp_frame != NULL) {
		// 直接使用Y14数据和温度解算函数进行人体分割
		segment_human_by_real_temperature((uint16_t*)stream_frame_info->temp_frame,
		                                   stream_frame_info->temp_info.width,
		                                   stream_frame_info->temp_info.height,
		                                   image_tmp_frame2);
		
		// 更新宽高（人体分割输出使用temp_info的尺寸）
		width = stream_frame_info->temp_info.width;
		height = stream_frame_info->temp_info.height;
	} else {
		// 常规图像处理流程
		display_image_process(stream_frame_info->image_frame, pix_num, &stream_frame_info->image_info);
		if ((stream_frame_info->image_info.rotate_side == LEFT_90D)|| \
			(stream_frame_info->image_info.rotate_side == RIGHT_90D))
		{
			width = stream_frame_info->image_info.height;
			height = stream_frame_info->image_info.width;
		}

		mirror_flip_demo(&stream_frame_info->image_info, image_tmp_frame2, \
						stream_frame_info->image_info.mirror_flip_status);
		rotate_demo(&stream_frame_info->image_info, image_tmp_frame2, \
					stream_frame_info->image_info.rotate_side);
	}

#ifdef OPENCV_ENABLE
	cv::Mat image = cv::Mat(height, width, CV_8UC3, image_tmp_frame2);
	
	// 获取当前使用的颜色模式
	irproc_color_mode_t current_color_mode = IRPROC_COLOR_MODE_3;
	if (stream_frame_info->image_info.output_format == OUTPUT_FMT_BGR888) {
		current_color_mode = IRPROC_COLOR_MODE_6;
	}
	
	// 创建颜色对比条（只在伪彩色模式下显示）
	cv::Mat combined_image;
	if (stream_frame_info->image_info.pseudo_color_status == PSEUDO_COLOR_ON) {
		// 创建动态颜色条（根据实际温度范围）
		cv::Mat color_bar = create_color_bar(COLOR_BAR_HEIGHT, COLOR_BAR_WIDTH, 
		                                      current_color_mode, max_temp_celsius, min_temp_celsius);
		
		// 计算组合图像的尺寸（图像 + 间距 + 颜色条 + 温度标签空间）
		int label_width = 60;  // 温度标签预留空间
		int combined_width = width + COLOR_BAR_MARGIN + COLOR_BAR_WIDTH + label_width;
		int combined_height = (height > COLOR_BAR_HEIGHT) ? height : COLOR_BAR_HEIGHT;
		
		// 创建组合图像（黑色背景）
		combined_image = cv::Mat(combined_height, combined_width, CV_8UC3, cv::Scalar(0, 0, 0));
		
		// 将原始图像复制到左侧
		cv::Rect image_roi(0, 0, width, height);
		image.copyTo(combined_image(image_roi));
		
		// 将颜色条复制到右侧（垂直居中）
		int bar_y = (combined_height - COLOR_BAR_HEIGHT) / 2;
		int bar_x = width + COLOR_BAR_MARGIN;
		cv::Rect bar_roi(bar_x, bar_y, COLOR_BAR_WIDTH, COLOR_BAR_HEIGHT);
		color_bar.copyTo(combined_image(bar_roi));
		
		// 添加动态温度标签（根据实际温度）
		add_temperature_labels(combined_image, bar_x, bar_y, COLOR_BAR_HEIGHT, 
		                       max_temp_celsius, min_temp_celsius);
		
		// 在组合图像上添加帧率和温度信息
		putText(combined_image, frameText, cv::Point(11, 11), cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar::all(0), 1, 8);
		putText(combined_image, frameText, cv::Point(10, 10), cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar::all(255), 1, 8);
		
#ifdef THERMAL_CAM_CMD
		// 显示最高温度
		char maxTempText[64];
		sprintf(maxTempText, "Max: %.2f C", max_temp_celsius);
		putText(combined_image, maxTempText, cv::Point(11, 31), cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar::all(0), 1, 8);
		putText(combined_image, maxTempText, cv::Point(10, 30), cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar::all(255), 1, 8);
		
		// 显示最低温度
		char minTempText[64];
		sprintf(minTempText, "Min: %.2f C", min_temp_celsius);
		putText(combined_image, minTempText, cv::Point(11, 51), cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar::all(0), 1, 8);
		putText(combined_image, minTempText, cv::Point(10, 50), cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar::all(255), 1, 8);
#endif
		
		cv::imshow("Test", combined_image);
	} else {
		// 非伪彩色模式，只显示原始图像
		// 显示帧率
		putText(image, frameText, cv::Point(11, 11), cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar::all(0), 1, 8);
		putText(image, frameText, cv::Point(10, 10), cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar::all(255), 1, 8);
		
#ifdef THERMAL_CAM_CMD
		// 显示最高温度
		char maxTempText[64];
		sprintf(maxTempText, "Max: %.2f C", max_temp_celsius);
		putText(image, maxTempText, cv::Point(11, 31), cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar::all(0), 1, 8);
		putText(image, maxTempText, cv::Point(10, 30), cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar::all(255), 1, 8);
		
		// 显示最低温度
		char minTempText[64];
		sprintf(minTempText, "Min: %.2f C", min_temp_celsius);
		putText(image, minTempText, cv::Point(11, 51), cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar::all(0), 1, 8);
		putText(image, minTempText, cv::Point(10, 50), cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar::all(255), 1, 8);
#endif
		
		cv::imshow("Test", image);
	}
	
	// 键盘控制：按 's' 键切换人体分割模式
	key_press = cvWaitKey(5);
	if (key_press == 's' || key_press == 'S') {
		human_segmentation_enabled = !human_segmentation_enabled;
		printf("\n========================================\n");
		printf("[Human Segmentation] 模式: %s\n", 
		       human_segmentation_enabled ? "已开启 ✓" : "已关闭 ✗");
		printf("[Human Segmentation] 温度阈值: %.1f-%.1f°C\n",
		       HUMAN_TEMP_MIN_CELSIUS, HUMAN_TEMP_MAX_CELSIUS);
		printf("[Human Segmentation] 按 's' 键切换模式\n");
		printf("========================================\n\n");
	}
	
	// 在屏幕上显示人体分割状态
	if (human_segmentation_enabled) {
		char status_text[128];
		sprintf(status_text, "[S] Human Seg: ON (%.0f-%.0f C)", 
		        HUMAN_TEMP_MIN_CELSIUS, HUMAN_TEMP_MAX_CELSIUS);
		
		if (stream_frame_info->image_info.pseudo_color_status == PSEUDO_COLOR_ON) {
			// 在组合图像上显示
			putText(combined_image, status_text, cv::Point(11, 71), 
			        cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar::all(0), 1, 8);
			putText(combined_image, status_text, cv::Point(10, 70), 
			        cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar(0, 255, 0), 1, 8);
		} else {
			// 在普通图像上显示
			putText(image, status_text, cv::Point(11, 71), 
			        cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar::all(0), 1, 8);
			putText(image, status_text, cv::Point(10, 70), 
			        cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar(0, 255, 0), 1, 8);
		}
	}
#endif
}

//display thread function
void* display_function(void* threadarg)
{
	StreamFrameInfo_t* stream_frame_info;
	stream_frame_info = (StreamFrameInfo_t*)threadarg;
	if (stream_frame_info == NULL)
	{
		return NULL;
	}

	display_init(stream_frame_info);

	int i = 0;
	int timer = 0;
	while (is_streaming || (i <= stream_time * fps))
	{
#if defined(_WIN32)
		WaitForSingleObject(image_sem, INFINITE);	//waitting for image singnal 
#elif defined(linux) || defined(unix)
		sem_wait(&image_sem);
#endif
		display_one_frame(stream_frame_info);
#if defined(_WIN32)
		ReleaseSemaphore(image_done_sem, 1, NULL);
#elif defined(linux) || defined(unix)
		sem_post(&image_done_sem);
#endif
		i++;
	}
	display_release();
#ifdef OPENCV_ENABLE
	cv::destroyAllWindows();
#endif
	printf("display thread exit!!\n");
	return NULL;
}
