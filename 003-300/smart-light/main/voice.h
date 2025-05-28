#ifndef VOICE_H
#define VOICE_H

// 初始化语音识别模块
void voice_init(void);
// 语音识别任务（FreeRTOS）
void voice_task(void *pvParameter);

#endif // VOICE_H
