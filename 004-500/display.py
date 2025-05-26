import serial
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.widgets import Button, Slider
import threading
import queue
import time
import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import json
import csv
from datetime import datetime
import re

class TCD1304Viewer:
    def __init__(self):
        # 串口参数
        self.serial_port = None
        self.port_name = "/dev/ttyUSB0"  # Linux默认端口，可修改为/dev/ttyACM0等
        self.baud_rate = 115200
        self.is_connected = False

        # 数据参数
        self.num_pixels = 3648
        self.max_frames = 200  # 减少内存使用
        self.current_frame = 0

        # 数据存储
        self.image_data = np.zeros((self.max_frames, self.num_pixels))
        self.frame_timestamps = []
        self.frame_ids = []

        # 线程和队列
        self.data_queue = queue.Queue()
        self.serial_thread = None
        self.is_collecting = False

        # GUI相关
        self.root = None
        self.fig = None
        self.ax_image = None
        self.ax_spectrum = None
        self.im = None
        self.line_spectrum = None
        self.animation = None

        # 控制参数
        self.integration_time = 10  # ms
        self.output_format = "RAW"
        self.auto_scale = True

        # 数据解析状态
        self.parsing_state = {
            'in_data_block': False,
            'current_data': [],
            'frame_id': 0,
            'expected_pixels': self.num_pixels
        }

        self.setup_gui()

    def setup_gui(self):
        """设置GUI界面"""
        self.root = tk.Tk()
        self.root.title("TCD1304 CCD光谱仪上位机 v1.0")
        self.root.geometry("1200x900")

        # 创建主框架
        main_frame = ttk.Frame(self.root)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        # 控制面板
        control_frame = ttk.LabelFrame(main_frame, text="控制面板", padding=5)
        control_frame.pack(fill=tk.X, pady=(0, 5))

        # 串口控制行
        port_frame = ttk.Frame(control_frame)
        port_frame.pack(fill=tk.X, pady=2)

        ttk.Label(port_frame, text="串口:").pack(side=tk.LEFT)
        self.port_var = tk.StringVar(value=self.port_name)
        port_entry = ttk.Entry(port_frame, textvariable=self.port_var, width=15)
        port_entry.pack(side=tk.LEFT, padx=(5, 10))

        self.connect_btn = ttk.Button(port_frame, text="连接", command=self.toggle_connection)
        self.connect_btn.pack(side=tk.LEFT, padx=5)

        self.status_label = ttk.Label(port_frame, text="未连接", foreground="red")
        self.status_label.pack(side=tk.LEFT, padx=10)

        # 采集控制行
        collect_frame = ttk.Frame(control_frame)
        collect_frame.pack(fill=tk.X, pady=2)

        self.start_btn = ttk.Button(collect_frame, text="开始连续采集", command=self.start_collection)
        self.start_btn.pack(side=tk.LEFT, padx=5)

        self.single_btn = ttk.Button(collect_frame, text="单次采集", command=self.single_capture)
        self.single_btn.pack(side=tk.LEFT, padx=5)

        self.stop_btn = ttk.Button(collect_frame, text="停止采集", command=self.stop_collection)
        self.stop_btn.pack(side=tk.LEFT, padx=5)

        self.clear_btn = ttk.Button(collect_frame, text="清除数据", command=self.clear_data)
        self.clear_btn.pack(side=tk.LEFT, padx=5)

        # 参数设置行
        param_frame = ttk.Frame(control_frame)
        param_frame.pack(fill=tk.X, pady=2)

        ttk.Label(param_frame, text="积分时间(ms):").pack(side=tk.LEFT)
        self.int_time_var = tk.StringVar(value=str(self.integration_time))
        int_time_entry = ttk.Entry(param_frame, textvariable=self.int_time_var, width=8)
        int_time_entry.pack(side=tk.LEFT, padx=(5, 10))

        ttk.Button(param_frame, text="设置", command=self.set_integration_time).pack(side=tk.LEFT, padx=5)

        ttk.Label(param_frame, text="格式:").pack(side=tk.LEFT, padx=(20, 5))
        self.format_var = tk.StringVar(value=self.output_format)
        format_combo = ttk.Combobox(param_frame, textvariable=self.format_var,
                                   values=["RAW", "FULL", "CSV", "BINARY"], width=8)
        format_combo.pack(side=tk.LEFT, padx=5)

        ttk.Button(param_frame, text="设置格式", command=self.set_output_format).pack(side=tk.LEFT, padx=5)

        # 文件操作行
        file_frame = ttk.Frame(control_frame)
        file_frame.pack(fill=tk.X, pady=2)

        ttk.Button(file_frame, text="保存数据", command=self.save_data_to_file).pack(side=tk.LEFT, padx=5)
        ttk.Button(file_frame, text="导出图像", command=self.export_image).pack(side=tk.LEFT, padx=5)
        ttk.Button(file_frame, text="设备状态", command=self.query_status).pack(side=tk.LEFT, padx=5)

        # 状态信息行
        status_frame = ttk.Frame(control_frame)
        status_frame.pack(fill=tk.X, pady=2)

        self.info_label = ttk.Label(status_frame, text="准备就绪", relief=tk.SUNKEN)
        self.info_label.pack(side=tk.LEFT, fill=tk.X, expand=True)

        # 图像显示区域
        plot_frame = ttk.Frame(main_frame)
        plot_frame.pack(fill=tk.BOTH, expand=True)

        self.setup_plots(plot_frame)

    def setup_plots(self, parent):
        """设置绘图区域"""
        from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk

        # 创建图形
        self.fig = plt.figure(figsize=(12, 8))
        self.fig.suptitle('TCD1304 CCD光谱仪实时显示', fontsize=14)

        # 创建子图
        gs = self.fig.add_gridspec(3, 1, height_ratios=[2, 1, 0.3])

        # 二维图像显示 (时间 vs 像素)
        self.ax_image = self.fig.add_subplot(gs[0])
        self.ax_image.set_title('光谱随时间变化 (瀑布图)')
        self.ax_image.set_xlabel('像素索引')
        self.ax_image.set_ylabel('时间 (帧)')

        # 初始化图像
        self.im = self.ax_image.imshow(self.image_data, aspect='auto', cmap='viridis',
                                      extent=[0, self.num_pixels, 0, self.max_frames],
                                      interpolation='nearest')

        # 添加颜色条
        cbar = self.fig.colorbar(self.im, ax=self.ax_image)
        cbar.set_label('ADC值')

        # 一维光谱显示 (当前帧)
        self.ax_spectrum = self.fig.add_subplot(gs[1])
        self.ax_spectrum.set_title('当前帧光谱')
        self.ax_spectrum.set_xlabel('像素索引')
        self.ax_spectrum.set_ylabel('ADC值')
        self.ax_spectrum.grid(True, alpha=0.3)

        # 初始化光谱线
        self.line_spectrum, = self.ax_spectrum.plot([], [], 'b-', linewidth=1)

        # 统计信息显示
        self.ax_stats = self.fig.add_subplot(gs[2])
        self.ax_stats.axis('off')
        self.stats_text = self.ax_stats.text(0.1, 0.5, '统计信息: 无数据',
                                           transform=self.ax_stats.transAxes,
                                           fontsize=10, verticalalignment='center')

        plt.tight_layout()

        # 嵌入到Tkinter
        canvas = FigureCanvasTkAgg(self.fig, parent)
        canvas.draw()
        canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)

        # 添加工具栏
        toolbar = NavigationToolbar2Tk(canvas, parent)
        toolbar.update()

        self.canvas = canvas

    def toggle_connection(self):
        """切换串口连接状态"""
        if not self.is_connected:
            self.connect_serial()
        else:
            self.disconnect_serial()

    def connect_serial(self):
        """连接串口"""
        try:
            self.port_name = self.port_var.get()
            self.serial_port = serial.Serial(self.port_name, self.baud_rate, timeout=1)
            time.sleep(2)  # 等待连接稳定

            # 发送状态查询命令测试连接
            self.send_command("STATUS")

            self.is_connected = True
            self.connect_btn.config(text="断开")
            self.status_label.config(text="已连接", foreground="green")
            self.update_info(f"已连接到 {self.port_name}")

        except Exception as e:
            messagebox.showerror("连接错误", f"无法连接到串口 {self.port_name}:\n{str(e)}")

    def disconnect_serial(self):
        """断开串口连接"""
        if self.is_collecting:
            self.stop_collection()

        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()

        self.is_connected = False
        self.connect_btn.config(text="连接")
        self.status_label.config(text="未连接", foreground="red")
        self.update_info("已断开连接")

    def send_command(self, command):
        """发送命令到设备"""
        if self.serial_port and self.serial_port.is_open:
            try:
                self.serial_port.write((command + '\n').encode())
                self.serial_port.flush()
                self.update_info(f"发送命令: {command}")
                return True
            except Exception as e:
                self.update_info(f"发送命令错误: {e}")
                return False
        return False

    def start_collection(self):
        """开始连续数据采集"""
        if not self.is_connected:
            messagebox.showwarning("警告", "请先连接设备")
            return

        self.is_collecting = True
        self.current_frame = 0

        # 设置输出格式和连续模式
        self.send_command(f"FORMAT {self.output_format}")
        time.sleep(0.1)
        self.send_command("CONTINUOUS")

        # 启动数据接收线程
        self.serial_thread = threading.Thread(target=self.data_receiver_thread)
        self.serial_thread.daemon = True
        self.serial_thread.start()

        # 启动动画更新
        self.animation = animation.FuncAnimation(self.fig, self.update_plots,
                                               interval=200, blit=False)

        self.start_btn.config(state=tk.DISABLED)
        self.single_btn.config(state=tk.DISABLED)
        self.stop_btn.config(state=tk.NORMAL)
        self.update_info("开始连续采集...")

    def single_capture(self):
        """单次采集"""
        if not self.is_connected:
            messagebox.showwarning("警告", "请先连接设备")
            return

        self.send_command(f"FORMAT {self.output_format}")
        time.sleep(0.1)
        self.send_command("START")
        self.update_info("执行单次采集...")

    def stop_collection(self):
        """停止数据采集"""
        self.is_collecting = False

        # 停止设备采集
        self.send_command("STOP")

        # 停止动画
        if self.animation:
            self.animation.event_source.stop()

        self.start_btn.config(state=tk.NORMAL)
        self.single_btn.config(state=tk.NORMAL)
        self.stop_btn.config(state=tk.DISABLED)
        self.update_info("采集已停止")

    def data_receiver_thread(self):
        """数据接收线程"""
        buffer = ""

        while self.is_collecting and self.serial_port and self.serial_port.is_open:
            try:
                # 读取数据
                if self.serial_port.in_waiting > 0:
                    data = self.serial_port.read(self.serial_port.in_waiting)
                    if data:
                        buffer += data.decode('utf-8', errors='ignore')

                        # 处理完整行
                        while '\n' in buffer:
                            line, buffer = buffer.split('\n', 1)
                            self.process_serial_line(line.strip())
                else:
                    time.sleep(0.01)  # 短暂等待

            except Exception as e:
                print(f"数据接收错误: {e}")
                break

    def process_serial_line(self, line):
        """处理串口接收到的行数据"""
        if not line:
            return

        # 打印调试信息
        if line.startswith(("INFO:", "ERROR:", "===")):
            print(f"设备: {line}")

        # 解析数据
        self.parse_data_line(line)

    def parse_data_line(self, line):
        """解析数据行"""
        state = self.parsing_state

        if line.startswith("FRAME_ID:"):
            try:
                state['frame_id'] = int(line.split(":")[1].strip())
            except ValueError:
                pass

        elif line == "RAW_DATA_START":
            state['in_data_block'] = True
            state['current_data'] = []

        elif line == "RAW_DATA_END":
            state['in_data_block'] = False
            if len(state['current_data']) == self.num_pixels:
                self.add_frame_data(state['current_data'], state['frame_id'])
            else:
                print(f"数据长度错误: 期望{self.num_pixels}, 实际{len(state['current_data'])}")

        elif state['in_data_block']:
            # 尝试解析数值
            try:
                if line.isdigit():
                    value = int(line)
                    if 0 <= value <= 4095:  # 12位ADC范围
                        state['current_data'].append(value)
                    else:
                        print(f"ADC值超出范围: {value}")
            except ValueError:
                pass

    def add_frame_data(self, data, frame_id):
        """添加一帧数据"""
        if self.current_frame >= self.max_frames:
            # 滚动缓冲区
            self.image_data[:-1] = self.image_data[1:]
            self.image_data[-1] = data
            self.frame_timestamps = self.frame_timestamps[1:] + [datetime.now()]
            self.frame_ids = self.frame_ids[1:] + [frame_id]
        else:
            self.image_data[self.current_frame] = data
            self.frame_timestamps.append(datetime.now())
            self.frame_ids.append(frame_id)
            self.current_frame += 1

        # 将数据放入队列供GUI更新
        self.data_queue.put({
            'frame_data': np.array(data),
            'frame_id': frame_id,
            'current_frame': self.current_frame
        })

    def update_plots(self, frame):
        """更新图形显示"""
        try:
            # 处理队列中的新数据
            latest_data = None
            while not self.data_queue.empty():
                latest_data = self.data_queue.get_nowait()
        except queue.Empty:
            pass

        if latest_data and self.current_frame > 0:
            # 更新信息标签
            self.update_info(f"采集中... 帧ID: {latest_data['frame_id']}, "
                           f"缓冲: {self.current_frame}/{self.max_frames}")

            # 更新二维图像
            display_data = self.image_data[:self.current_frame]
            self.im.set_array(display_data)
            self.im.set_extent([0, self.num_pixels, 0, self.current_frame])

            if self.auto_scale and display_data.size > 0:
                vmin, vmax = display_data.min(), display_data.max()
                if vmax > vmin:
                    self.im.set_clim(vmin=vmin, vmax=vmax)

            # 更新当前帧光谱
            current_spectrum = latest_data['frame_data']
            self.line_spectrum.set_data(range(self.num_pixels), current_spectrum)

            # 自动调整Y轴范围
            if self.auto_scale:
                margin = (current_spectrum.max() - current_spectrum.min()) * 0.1
                self.ax_spectrum.set_ylim(current_spectrum.min() - margin,
                                        current_spectrum.max() + margin)
            self.ax_spectrum.set_xlim(0, self.num_pixels)

            # 更新统计信息
            avg_val = current_spectrum.mean()
            min_val = current_spectrum.min()
            max_val = current_spectrum.max()
            std_val = current_spectrum.std()

            stats_text = f"帧ID: {latest_data['frame_id']} | 平均: {avg_val:.1f} | 范围: {min_val}-{max_val} | 标准差: {std_val:.1f}"
            self.stats_text.set_text(stats_text)

            # 刷新画布
            self.canvas.draw_idle()

        return []

    def set_integration_time(self):
        """设置积分时间"""
        try:
            new_time = int(self.int_time_var.get())
            if 1 <= new_time <= 10000:
                command = f"SET_INT {new_time}"
                if self.send_command(command):
                    self.integration_time = new_time
                    self.update_info(f"积分时间设置为 {new_time}ms")
            else:
                messagebox.showerror("错误", "积分时间范围: 1-10000ms")
        except ValueError:
            messagebox.showerror("错误", "请输入有效的数字")

    def set_output_format(self):
        """设置输出格式"""
        format_type = self.format_var.get()
        command = f"FORMAT {format_type}"
        if self.send_command(command):
            self.output_format = format_type
            self.update_info(f"输出格式设置为 {format_type}")

    def query_status(self):
        """查询设备状态"""
        self.send_command("STATUS")

    def clear_data(self):
        """清除数据"""
        self.image_data.fill(0)
        self.current_frame = 0
        self.frame_timestamps.clear()
        self.frame_ids.clear()

        # 清空队列
        while not self.data_queue.empty():
            self.data_queue.get_nowait()

        # 重置解析状态
        self.parsing_state = {
            'in_data_block': False,
            'current_data': [],
            'frame_id': 0,
            'expected_pixels': self.num_pixels
        }

        self.update_info("数据已清除")

    def save_data_to_file(self):
        """保存数据到文件"""
        if self.current_frame == 0:
            messagebox.showwarning("警告", "没有数据可保存")
            return

        filename = filedialog.asksaveasfilename(
            defaultextension=".npz",
            filetypes=[("NumPy压缩文件", "*.npz"), ("CSV文件", "*.csv"), ("所有文件", "*.*")]
        )

        if filename:
            try:
                if filename.endswith('.npz'):
                    # 保存为NumPy格式
                    np.savez_compressed(filename,
                                      image_data=self.image_data[:self.current_frame],
                                      frame_ids=self.frame_ids,
                                      timestamps=[t.isoformat() for t in self.frame_timestamps],
                                      integration_time=self.integration_time,
                                      num_pixels=self.num_pixels)
                elif filename.endswith('.csv'):
                    # 保存为CSV格式
                    with open(filename, 'w', newline='') as csvfile:
                        writer = csv.writer(csvfile)
                        writer.writerow(['Frame_ID', 'Timestamp'] + [f'Pixel_{i}' for i in range(self.num_pixels)])
                        for i in range(self.current_frame):
                            row = [self.frame_ids[i], self.frame_timestamps[i].isoformat()] + list(self.image_data[i])
                            writer.writerow(row)

                self.update_info(f"数据已保存到 {filename}")
                messagebox.showinfo("保存成功", f"数据已保存到:\n{filename}")

            except Exception as e:
                messagebox.showerror("保存错误", f"保存失败:\n{str(e)}")

    def export_image(self):
        """导出图像"""
        if self.current_frame == 0:
            messagebox.showwarning("警告", "没有数据可导出")
            return

        filename = filedialog.asksaveasfilename(
            defaultextension=".png",
            filetypes=[("PNG图像", "*.png"), ("PDF文件", "*.pdf"), ("所有文件", "*.*")]
        )

        if filename:
            try:
                self.fig.savefig(filename, dpi=300, bbox_inches='tight')
                self.update_info(f"图像已导出到 {filename}")
                messagebox.showinfo("导出成功", f"图像已导出到:\n{filename}")
            except Exception as e:
                messagebox.showerror("导出错误", f"导出失败:\n{str(e)}")

    def update_info(self, message):
        """更新信息标签"""
        self.info_label.config(text=f"{datetime.now().strftime('%H:%M:%S')} - {message}")
        print(f"[{datetime.now().strftime('%H:%M:%S')}] {message}")

    def run(self):
        """运行主程序"""
        try:
            self.root.protocol("WM_DELETE_WINDOW", self.on_closing)
            self.root.mainloop()
        except KeyboardInterrupt:
            self.on_closing()

    def on_closing(self):
        """程序关闭处理"""
        if self.is_collecting:
            self.stop_collection()
        if self.is_connected:
            self.disconnect_serial()
        self.root.quit()
        self.root.destroy()

if __name__ == "__main__":
    print("启动TCD1304光谱仪上位机...")
    app = TCD1304Viewer()
    app.run()