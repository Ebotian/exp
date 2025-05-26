import serial
import numpy as np
import matplotlib.pyplot as plt
import time

# 配置参数
SERIAL_PORT = '/dev/ttyUSB0'
BAUDRATE = 115200
PIXELS = 3648
SCAN_LINES = 200  # 二维图像高度

def send_command(ser, cmd):
    """发送命令到STM32"""
    ser.write((cmd + '\n').encode())
    time.sleep(0.1)
    # 清空回显
    while ser.in_waiting:
        ser.readline()

def read_frame(ser):
    """读取一帧数据"""
    while True:
        line = ser.readline().decode(errors='ignore').strip()
        if line.startswith('FRAME_ID:'):
            break

    # 等待RAW_DATA_START
    while True:
        line = ser.readline().decode(errors='ignore').strip()
        if line == 'RAW_DATA_START':
            break

    # 读取像素数据
    frame = []
    while True:
        line = ser.readline().decode(errors='ignore').strip()
        if line == 'RAW_DATA_END':
            break
        if line.isdigit():
            frame.append(int(line))

    return np.array(frame) if len(frame) == PIXELS else None

def main():
    # 连接串口
    try:
        ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=2)
        time.sleep(1)
        print(f"已连接到 {SERIAL_PORT}")
    except:
        print(f"无法连接到 {SERIAL_PORT}")
        return

    # 设置STM32为RAW连续模式
    send_command(ser, 'FORMAT RAW')
    send_command(ser, 'CONTINUOUS')
    print("已设置为连续扫描模式")

    # 初始化显示
    image_data = np.zeros((SCAN_LINES, PIXELS), dtype=np.uint16)
    plt.ion()
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 8))

    # 二维扫描图像
    im = ax1.imshow(image_data, cmap='gray', vmin=0, vmax=4095,
                    aspect='auto', origin='upper')
    ax1.set_title('TCD1304 二维扫描图像 (移动CCD进行扫描)')
    ax1.set_xlabel('像素位置 (0-3647)')
    ax1.set_ylabel('扫描行 (移动方向)')
    plt.colorbar(im, ax=ax1, label='ADC值')

    # 当前行光谱
    line, = ax2.plot(np.zeros(PIXELS))
    ax2.set_title('当前扫描行光谱')
    ax2.set_xlabel('像素位置')
    ax2.set_ylabel('ADC值')
    ax2.set_ylim(0, 4095)
    ax2.grid(True)

    plt.tight_layout()

    print("开始扫描显示，按 Ctrl+C 退出")
    print("请缓慢移动TCD1304传感器进行二维扫描")

    scan_line = 0
    try:
        while True:
            frame = read_frame(ser)
            if frame is not None:
                # 滚动更新二维图像
                image_data = np.roll(image_data, -1, axis=0)
                image_data[-1, :] = frame

                # 更新显示
                im.set_data(image_data)
                line.set_ydata(frame)

                plt.draw()
                plt.pause(0.01)

                # 打印统计信息
                avg = np.mean(frame)
                minv = np.min(frame)
                maxv = np.max(frame)
                print(f"行:{scan_line:4d} 平均:{avg:7.1f} 最小:{minv:4d} 最大:{maxv:4d} 范围:{maxv-minv:4d}")
                scan_line += 1
            else:
                print("数据读取异常，跳过")

    except KeyboardInterrupt:
        print("\n扫描停止")
        send_command(ser, 'STOP')
        ser.close()
        plt.ioff()
        plt.show()

if __name__ == '__main__':
    main()