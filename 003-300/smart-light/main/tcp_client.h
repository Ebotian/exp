#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <stdbool.h>
#include <stdint.h> // Add this line

// 启动TCP客户端任务，连接指定服务器和端口
void tcp_client_start(const char *server_ip, uint16_t port);
// 发送数据到服务器
bool tcp_client_send(const char *data, int len);
// 查询连接状态
bool tcp_client_is_connected(void);
// 应用当前 TCP 接收到的 LED 状态，保持灯带显示
void tcp_client_apply_led_state(void);
// 新增：从远程获取电源状态
bool tcp_client_get_power_state_from_remote(bool *was_explicitly_commanded_this_session);
// 新增：应用当前 LED 设置
void tcp_client_apply_current_led_settings(void);

#endif // TCP_CLIENT_H
