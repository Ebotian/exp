#ifndef WIFI_H
#define WIFI_H

#include <stdbool.h>

// 初始化WiFi，自动连接并重连
void wifi_init(void);
// 查询WiFi连接状态
bool wifi_is_connected(void);
// 获取已连接网络的IP地址（静态buffer）
const char *wifi_get_ip(void);

#endif // WIFI_H
