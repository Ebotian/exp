#ifndef LDR_H
#define LDR_H

// 初始化LDR（可选，若需ADC配置）
void ldr_init(void);
// 读取LDR的ADC值
int ldr_read(void);

#endif // LDR_H
