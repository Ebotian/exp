import socket
import threading
from flask import Flask, request, jsonify

app = Flask(__name__)

ESP32_CONN = None
ESP32_LOCK = threading.Lock()

# TCP服务端线程，等待ESP32连接

def tcp_server_thread():
    global ESP32_CONN
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("0.0.0.0", 9000))
    server.listen(1)
    print("[TCP] Listening on 0.0.0.0:9000...")
    while True:
        conn, addr = server.accept()
        print(f"[TCP] ESP32 connected from {addr}")
        with ESP32_LOCK:
            ESP32_CONN = conn
            # 添加日志，记录 ESP32_CONN 被设置的时刻和值
            print(f"[TCP_STATE] ESP32_CONN set to {ESP32_CONN} by tcp_server_thread. Client: {addr}")
        try:
            while True:
                data = conn.recv(1024)
                if not data:
                    print(f"[TCP] Connection gracefully closed by client {addr}") # 更明确的日志
                    break
                print(f"[TCP] ESP32 ({addr}) says: {data.decode(errors='ignore')}")
        except ConnectionResetError: # 更具体地捕获连接重置错误
            print(f"[TCP] Connection reset by client {addr}")
        except Exception as e:
            print(f"[TCP] Error with client {addr}: {e}")
        finally:
            with ESP32_LOCK:
                # 添加日志，记录 ESP32_CONN 被置为 None 的时刻
                print(f"[TCP_STATE] ESP32_CONN is being set to None. Previous value was {ESP32_CONN} for client {addr}. Lock acquired.")
                ESP32_CONN = None
                print(f"[TCP_STATE] ESP32_CONN successfully set to None for client {addr}. Lock will be released.")
            conn.close() #确保conn在finally块中被关闭
            print(f"[TCP] ESP32 disconnected from {addr}")

tcp_thread = threading.Thread(target=tcp_server_thread, daemon=True)

@app.route('/api/status', methods=['GET'])
def get_status():
    global ESP32_CONN
    with ESP32_LOCK:
        conn_status = bool(ESP32_CONN)
        # 添加日志，记录 /api/status 被调用时 ESP32_CONN 的状态
        print(f"[API_STATUS] Checking status. ESP32_CONN is {ESP32_CONN}. Current connection status will be: {conn_status}. Lock acquired.")
        if ESP32_CONN:
            # 可以在这里添加一个轻量级的检查，例如尝试发送一个字节，如果失败则认为连接已断开
            # try:
            #     ESP32_CONN.sendall(b'\\0') # 发送一个空字节作为心跳检查
            #     print(f"[API_STATUS] Heartbeat check to {ESP32_CONN.getpeername()} successful.")
            # except socket.error as e:
            #     print(f"[API_STATUS] Heartbeat check failed for {ESP32_CONN.getpeername()}: {e}. Setting to not connected.")
            #     ESP32_CONN = None # 更新状态
            #     conn_status = False # 更新本次请求的返回状态
            #     return jsonify({"esp32Connected": False})
            return jsonify({"esp32Connected": True}) # 如果心跳检查通过或未启用检查，返回True
        else:
            return jsonify({"esp32Connected": False})

@app.route('/api/led', methods=['POST'])
def led_control():
    global ESP32_CONN
    data = request.get_json(force=True)
    msg = (str(data) if isinstance(data, str) else request.data)
    with ESP32_LOCK:
        if ESP32_CONN:
            try:
                ESP32_CONN.sendall(request.data + b'\n')
                return jsonify({"result": True})
            except Exception as e:
                return jsonify({"result": False, "error": str(e)}), 500
        else:
            return jsonify({"result": False, "error": "ESP32 not connected"}), 500

if __name__ == '__main__':
    # 启动TCP服务器线程
    tcp_thread.start()
    # 以生产模式运行Flask，不启用调试器和自动重载
    app.run(host='0.0.0.0', port=5002, debug=False, use_reloader=False)
