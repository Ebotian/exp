#!/usr/bin/env python3
"""
脚本功能：
    1. 解析指定的网表(.net)文件，提取主要元件的引脚连接信息。
    2. 根据 board_init.c 文件头注释中定义的参数格式，生成同名的 .md 引脚连接表。

用法：
    python3 gen_pinmap_md.py Netlist_Schematic1_2025-05-20.net board_init.c

输出：
    board_init.md
"""
import sys
import re
import os

# 简单正则示例，实际可根据网表格式调整
PINMAP_HEADER = "| 元件/模块 | 引脚号 | 信号名 | 连接对象/MCU引脚 | 说明 |\n|-----------|--------|--------|------------------|------|\n"

def parse_netlist(netfile):
    pinmap = []
    with open(netfile, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()
    # 1. 解析元件块，收集元件名、型号、封装等
    comp_info = {}
    blocks = content.split('[')
    for block in blocks:
        lines = block.strip().splitlines()
        if not lines:
            continue
        designator = ''
        device = ''
        name = ''
        for i, line in enumerate(lines):
            if line.strip() == 'DESIGNATOR' and i+1 < len(lines):
                designator = lines[i+1].strip()
            if line.strip() == 'Device' and i+1 < len(lines):
                device = lines[i+1].strip()
            if line.strip() == 'Name' and i+1 < len(lines):
                name = lines[i+1].strip()
        if designator:
            comp_info[designator] = {'device': device, 'name': name}
    # 2. 解析网络连接段，提取引脚连接
    nets = re.findall(r'\(([^\)]+)\)', content, re.MULTILINE)
    for net in nets:
        net_lines = net.strip().split('\n')
        if not net_lines:
            continue
        net_name = net_lines[0].strip()
        for item in net_lines[1:]:
            m = re.match(r'([A-Za-z0-9_\-]+)-(\d+)\s+([A-Za-z0-9_\-]+)(?:-(.*?))?\s+(\S+)', item.strip())
            # 例: U1-5 STM32F103C8T6-PD0-OSC_IN Input
            if m:
                comp, pin, device, signal, _ = m.groups()
                dev_info = comp_info.get(comp, {})
                pinmap.append([
                    comp,
                    pin,
                    signal if signal else '',
                    device if device else dev_info.get('device', ''),
                    net_name
                ])
    return pinmap

def write_pinmap_md(pinmap, out_md):
    with open(out_md, 'w', encoding='utf-8') as f:
        f.write(PINMAP_HEADER)
        for row in pinmap:
            f.write('| ' + ' | '.join(row) + ' |\n')

def main():
    if len(sys.argv) < 3:
        print('用法: python3 gen_pinmap_md.py <netlist.net> <board_init.c>')
        sys.exit(1)
    netfile = sys.argv[1]
    cfile = sys.argv[2]
    out_md = os.path.splitext(cfile)[0] + '.md'
    pinmap = parse_netlist(netfile)
    write_pinmap_md(pinmap, out_md)
    print(f'已生成: {out_md}')

if __name__ == '__main__':
    main()
