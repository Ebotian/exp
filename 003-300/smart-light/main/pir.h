#ifndef PIR_H
#define PIR_H
#include <stdbool.h>

// 初始化PIR传感器
void pir_init(void);
// 检测是否有人存在
bool pir_detected(void);

#endif // PIR_H
