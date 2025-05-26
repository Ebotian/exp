# filepath: hc25_fullstack_control/backend/app.py
from flask import Flask, render_template, jsonify, request
from flask_cors import CORS # 导入 CORS
import socket
import threading
import time
from datetime import datetime

app = Flask(__name__)
CORS(app) # 启用 CORS，允许所有来源的请求 (开发时方便，生产环境应配置具体来源)

# --- TCP 服务器配置 ---
TCP_SERVER_HOST = '0.0.0.0'
TCP_SERVER_PORT = 9999
# --- ---------------- ---

module_client_socket = None
module_client_address = None
socket_lock = threading.Lock()
send_history = [] # 用于存储发送历史

def handle_module_connection(conn, addr):
    global module_client_socket, module_client_address
    with socket_lock:
        if module_client_socket:
            try:
                module_client_socket.close()
            except Exception as e:
                print(f"Error closing old module socket: {e}")
        module_client_socket = conn
        module_client_address = addr

    print(f"模块 {addr} 已连接。")
    # 将连接事件添加到历史记录
    with socket_lock:
         send_history.append({
            "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            "char_sent": "N/A",
            "event": "Module Connected",
            "status": "info",
            "message": f"模块 {addr} 已连接。"
        })


    try:
        while True:
            data = conn.recv(1024)
            if not data:
                print(f"模块 {addr} 已断开连接。")
                with socket_lock:
                    send_history.append({
                        "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                        "char_sent": "N/A",
                        "event": "Module Disconnected",
                        "status": "info",
                        "message": f"模块 {addr} 已断开连接。"
                    })
                break
            print(f"从模块 {addr} 收到: {data.decode('utf-8', errors='ignore')}")
            # 可以在这里处理模块主动发送的数据，并更新历史或状态

    except ConnectionResetError:
        print(f"模块 {addr} 连接被重置。")
        with socket_lock:
            send_history.append({
                "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                "char_sent": "N/A",
                "event": "Connection Reset",
                "status": "warning",
                "message": f"模块 {addr} 连接被重置。"
            })
    except Exception as e:
        print(f"与模块 {addr} 通信时发生错误: {e}")
    finally:
        with socket_lock:
            if module_client_socket == conn:
                module_client_socket = None
                module_client_address = None
        conn.close()
        print(f"已关闭与模块 {addr} 的连接处理。")

def start_tcp_server():
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        server_socket.bind((TCP_SERVER_HOST, TCP_SERVER_PORT))
        server_socket.listen(1)
        print(f"TCP 服务器已启动，正在监听 {TCP_SERVER_HOST}:{TCP_SERVER_PORT} ...")
        while True:
            conn, addr = server_socket.accept()
            client_thread = threading.Thread(target=handle_module_connection, args=(conn, addr))
            client_thread.daemon = True
            client_thread.start()
    except Exception as e:
        print(f"TCP 服务器启动失败或运行时错误: {e}")
    finally:
        server_socket.close()
        print("TCP 服务器已关闭。")

@app.route('/api/send_command', methods=['POST'])
def send_command_route():
    data = request.get_json()
    char_to_send = data.get('char')
    status_message = ""
    success = False

    if not char_to_send or char_to_send not in ['A', 'B', 'C', 'D']:
        status_message = "错误：无效的字符。只能发送 A, B, C, 或 D。"
        success = False
    else:
        with socket_lock:
            if module_client_socket:
                try:
                    print(f"准备向模块 {module_client_address} 发送: '{char_to_send}'")
                    module_client_socket.sendall(char_to_send.encode('utf-8'))
                    status_message = f"成功发送字符 '{char_to_send}' 到模块 {module_client_address}。"
                    success = True
                except socket.error as e:
                    status_message = f"向模块发送数据时发生 socket 错误: {e}。模块可能已断开。"
                except Exception as e:
                    status_message = f"向模块发送数据时发生未知错误: {e}"
            else:
                status_message = "错误：模块当前未连接到服务器。"

    # 记录到历史
    with socket_lock:
        send_history.append({
            "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            "char_sent": char_to_send if char_to_send in ['A', 'B', 'C', 'D'] else "Invalid",
            "event": "Send Command",
            "status": "success" if success else "error",
            "message": status_message
        })
        # 保持历史记录长度，例如最多100条
        if len(send_history) > 100:
            send_history.pop(0)

    return jsonify({"success": success, "message": status_message})

@app.route('/api/history', methods=['GET'])
def get_history_route():
    with socket_lock:
        # 返回历史记录的副本，按时间倒序
        return jsonify(list(reversed(send_history)))

@app.route('/api/status', methods=['GET'])
def get_status_route():
    with socket_lock:
        connected = module_client_socket is not None
        return jsonify({
            "module_connected": connected,
            "module_address": str(module_client_address) if connected else None
        })

if __name__ == '__main__':
    tcp_thread = threading.Thread(target=start_tcp_server)
    tcp_thread.daemon = True
    tcp_thread.start()
    # Flask 开发服务器默认监听 5000 端口
    app.run(host='0.0.0.0', port=5001, debug=False, use_reloader=False)