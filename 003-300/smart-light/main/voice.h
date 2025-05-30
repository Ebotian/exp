#ifndef VOICE_H
#define VOICE_H

#include <stdbool.h>

// 初始化语音识别模块
void voice_init(void);
// 语音识别任务（FreeRTOS）
void voice_task(void *pvParameter);
// 获取灯光状态
void voice_get_light_state(bool *on, bool *valid);

#endif // VOICE_H
