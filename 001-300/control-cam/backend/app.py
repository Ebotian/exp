from flask import Flask, request, jsonify
import serial
import time
import os

app = Flask(__name__)

# 串口配置（请根据实际情况修改端口和波特率）
SERIAL_PORT = '/dev/ttyUSB0'  # 或 '/dev/ttyACM0'，视实际情况而定
BAUDRATE = 115200

# 尝试打开串口
ser = None
try:
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=1)
except Exception as e:
    print(f"串口打开失败: {e}")

@app.route('/api/control', methods=['POST'])
def control():
    data = request.get_json()
    cmd = data.get('command', '').strip().upper()
    if cmd not in ['LEFT', 'RIGHT', 'STOP', 'AUTO']:
        return jsonify({'success': False, 'message': '无效指令'}), 400
    if ser and ser.is_open:
        try:
            ser.write((cmd + '\n').encode('utf-8'))
            return jsonify({'success': True, 'message': f'指令 {cmd} 已发送'})
        except Exception as e:
            return jsonify({'success': False, 'message': f'串口发送失败: {e}'})
    else:
        return jsonify({'success': False, 'message': '串口未打开'})

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
