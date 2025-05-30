#ifndef DISPLAY_H
#define DISPLAY_H

// 初始化OLED显示屏
void display_init(void);
// 在OLED上显示一行文本
void display_show_text(const char *text);
// 在OLED上显示两行文本（如时间和光强）
void display_show_2lines(const char *line1, const char *line2);

#endif // DISPLAY_H
