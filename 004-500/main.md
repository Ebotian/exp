| 元件/模块 | 引脚号 | 信号名            | 连接对象/MCU 引脚         | 说明                   |
| --------- | ------ | ----------------- | ------------------------- | ---------------------- |
| X1        | 2      |                   | 49SAC08000182060-OSC2     | OSC_IN                 |
| U1        | 5      |                   | STM32F103C8T6-PD0-OSC_IN  | OSC_IN                 |
| R8        | 2      |                   | 0603WAF1004T5E-2          | OSC_IN                 |
| C8        | 1      |                   | CL10C200JB8NNNC-1         | OSC_IN                 |
| X1        | 1      |                   | 49SAC08000182060-OSC1     | OSC_OUT                |
| U1        | 6      |                   | STM32F103C8T6-PD1-OSC_OUT | OSC_OUT                |
| R8        | 1      |                   | 0603WAF1004T5E-1          | OSC_OUT                |
| C9        | 1      |                   | CL10C200JB8NNNC-1         | OSC_OUT                |
| U1        | 1      |                   | STM32F103C8T6-VBAT        | 3V3                    |
| U1        | 9      |                   | STM32F103C8T6-VDDA        | 3V3                    |
| U1        | 24     |                   | STM32F103C8T6-VDD_1       | 3V3                    |
| U1        | 36     |                   | STM32F103C8T6-VDD_2       | 3V3                    |
| U1        | 48     |                   | STM32F103C8T6-VDD_3       | 3V3                    |
| R1        | 2      |                   | 0603WAF1002T5E-2          | 3V3                    |
| SW1       | 2      |                   | TS-1101-C-W-2             | 3V3                    |
| H1        | 4      |                   | PZ254V-11-04P-4           | 3V3                    |
| C5        | 2      |                   | CC0603KRX7R9BB104-2       | 3V3                    |
| C1        | 2      |                   | CL10B104KO8NNNC-2         | 3V3                    |
| C2        | 2      |                   | CL10B104KO8NNNC-2         | 3V3                    |
| C3        | 2      |                   | CL10B104KO8NNNC-2         | 3V3                    |
| C4        | 2      |                   | CL10B104KO8NNNC-2         | 3V3                    |
| U7        | 2      | 3.3V_C173386-VOUT | AMS1117                   | 3V3                    |
| U7        | 4      | 3.3V_C173386-VOUT | AMS1117                   | 3V3                    |
| C14       | 2      |                   | CL10B104KO8NNNC-2         | 3V3                    |
| C15       | 2      |                   | CL10A226MQ8NRNC-2         | 3V3                    |
| SW3       | 2      |                   | TS-1101-C-W-2             | 3V3                    |
| SW4       | 2      |                   | TS-1101-C-W-2             | 3V3                    |
| SW5       | 2      |                   | TS-1101-C-W-2             | 3V3                    |
| SW2       | 2      |                   | TS-1101-C-W-2             | 3V3                    |
| U1        | 7      |                   | STM32F103C8T6-NRST        | REST                   |
| R1        | 1      |                   | 0603WAF1002T5E-1          | REST                   |
| C6        | 2      |                   | CL10B104KO8NNNC-2         | REST                   |
| SW6       | 1      |                   | TS-1101-C-W-1             | REST                   |
| U1        | 8      |                   | STM32F103C8T6-VSSA        | GND                    |
| U1        | 23     |                   | STM32F103C8T6-VSS_1       | GND                    |
| U1        | 35     |                   | STM32F103C8T6-VSS_2       | GND                    |
| U1        | 47     |                   | STM32F103C8T6-VSS_3       | GND                    |
| C6        | 1      |                   | CL10B104KO8NNNC-1         | GND                    |
| R2        | 1      |                   | 0603WAF4701T5E-1          | GND                    |
| R3        | 2      |                   | 0603WAF4701T5E-2          | GND                    |
| C8        | 2      |                   | CL10C200JB8NNNC-2         | GND                    |
| C9        | 2      |                   | CL10C200JB8NNNC-2         | GND                    |
| H1        | 1      |                   | PZ254V-11-04P-1           | GND                    |
| C5        | 1      |                   | CC0603KRX7R9BB104-1       | GND                    |
| SW6       | 2      |                   | TS-1101-C-W-2             | GND                    |
| C1        | 1      |                   | CL10B104KO8NNNC-1         | GND                    |
| C2        | 1      |                   | CL10B104KO8NNNC-1         | GND                    |
| C3        | 1      |                   | CL10B104KO8NNNC-1         | GND                    |
| C4        | 1      |                   | CL10B104KO8NNNC-1         | GND                    |
| USBC1     | 0      |                   | TYPE-C-31-M-12-0          | O,C,8,EL-PGND1 Passive |
| USBC1     | 0      |                   | TYPE-C-31-M-12-0          | O,C,8,EL-PGND1 Passive |
| USBC1     | 0      |                   | TYPE-C-31-M-12-0          | O,C,8,EL-PGND1 Passive |
| USBC1     | 0      |                   | TYPE-C-31-M-12-0          | O,C,8,EL-PGND1 Passive |
| U1        | 13     |                   | STM32F103C8T6-PA3         | OS                     |
| U21       | 1      |                   | TLV6001IDBVR-OUT          | OS                     |
| R29       | 2      |                   | MCT06030D6501BP100-2      | OS                     |
| U1        | 14     |                   | STM32F103C8T6-PA4         | K1                     |
| SW2       | 1      |                   | TS-1101-C-W-1             | K1                     |
| R4        | 2      |                   | 0603WAF4701T5E-2          | K1                     |
| U1        | 15     |                   | STM32F103C8T6-PA5         | K2                     |
| R5        | 2      |                   | 0603WAF4701T5E-2          | K2                     |
| SW3       | 1      |                   | TS-1101-C-W-1             | K2                     |
| U1        | 16     |                   | STM32F103C8T6-PA6         | K3                     |
| R6        | 2      |                   | 0603WAF4701T5E-2          | K3                     |
| SW4       | 1      |                   | TS-1101-C-W-1             | K3                     |
| U1        | 17     |                   | STM32F103C8T6-PA7         | K4                     |
| R7        | 2      |                   | 0603WAF4701T5E-2          | K4                     |
| SW5       | 1      |                   | TS-1101-C-W-1             | K4                     |
| U1        | 19     |                   | STM32F103C8T6-PB1         | ICG_IN                 |
| U9        | 1      |                   | SN74HC04DR-1A             | ICG_IN                 |
| U1        | 20     |                   | STM32F103C8T6-PB2         | BOOT1                  |
| R3        | 1      |                   | 0603WAF4701T5E-1          | BOOT1                  |
| U1        | 21     |                   | STM32F103C8T6-PB10        | M_IN                   |
| U9        | 3      |                   | SN74HC04DR-2A             | M_IN                   |
| U1        | 22     |                   | STM32F103C8T6-PB11        | SH_IN                  |
| U9        | 5      |                   | SN74HC04DR-3A             | SH_IN                  |
| U1        | 25     |                   | STM32F103C8T6-PB12        | PWMB                   |
| U1        | 26     |                   | STM32F103C8T6-PB13        | LCD_SCK                |
| U1        | 27     |                   | STM32F103C8T6-PB14        | PWMA                   |
| U1        | 28     |                   | STM32F103C8T6-PB15        | LCD_SDA                |
| U1        | 29     |                   | STM32F103C8T6-PA8         | IN4                    |
| U1        | 30     |                   | STM32F103C8T6-PA9         | IN3                    |
| U1        | 31     |                   | STM32F103C8T6-PA10        | IN2                    |
| U1        | 32     |                   | STM32F103C8T6-PA11        | USB_DM                 |
| R32       | 2      |                   | 0603WAF220JT5E-2          | USB_DM                 |
| U1        | 33     |                   | STM32F103C8T6-PA12        | USB_DP                 |
| R33       | 2      |                   | 0603WAF220JT5E-2          | USB_DP                 |
| U1        | 34     |                   | STM32F103C8T6-PA13        | DIO                    |
| H1        | 3      |                   | PZ254V-11-04P-3           | DIO                    |
| U1        | 37     |                   | STM32F103C8T6-PA14        | CLK                    |
| H1        | 2      |                   | PZ254V-11-04P-2           | CLK                    |
| U1        | 38     |                   | STM32F103C8T6-PA15        | LCD_CS                 |
| U1        | 39     |                   | STM32F103C8T6-PB3         | LCD_RES                |
| U1        | 40     |                   | STM32F103C8T6-PB4         | LCD_DC                 |
| U1        | 41     |                   | STM32F103C8T6-PB5         | LCD_BLK                |
| R16       | 2      |                   | RC0603FR-071KL-2          | LCD_BLK                |
| U1        | 42     |                   | STM32F103C8T6-PB6         | $1N3339                |
| U1        | 44     |                   | STM32F103C8T6-BOOT0       | BOOT0                  |
| SW1       | 1      |                   | TS-1101-C-W-1             | BOOT0                  |
| R2        | 2      |                   | 0603WAF4701T5E-2          | BOOT0                  |
| U1        | 46     |                   | STM32F103C8T6-PB9         | IN1                    |
| R14       | 2      |                   | FRC0603J151               | $1N2120                |
| Q1        | 1      |                   | 2SA1015_C727147-B         | $1N2120                |
| Q1        | 3      |                   | 2SA1015_C727147-C         | $1N2252                |
| R13       | 1      |                   | FRC0603J151               | $1N2252                |
| Q1        | 2      |                   | 2SA1015_C727147-E         | OS1                    |
| R15       | 1      |                   | 0603WAF2201T5E-1          | OS1                    |
| R26       | 2      |                   | 0603WAF3301T5E-2          | OS1                    |
| R15       | 2      |                   | 0603WAF2201T5E-2          | $1N8289                |
| R18       | 2      |                   | ARG03FTC4001N-2           | $1N8289                |
| R19       | 1      |                   | 0603WAF1001T5E-1          | $1N8289                |
| Q2        | 3      |                   | SS8050_C2150-C            | LEDK                   |
| Q2        | 1      |                   | SS8050_C2150-B            | $1N3324                |
| R16       | 1      |                   | RC0603FR-071KL-1          | $1N3324                |
| R17       | 1      |                   | RC0603FR-071KL-1          | $1N3324                |
| U8        | 1      | 5.0-VIN           | LM2575S                   | VIN                    |
| C17       | 1      |                   | RVT1H101M0607-1           | VIN                    |
| DC1       | 1      |                   | DC-005-A200-1             | VIN                    |
| C7        | 2      |                   | CGA0603X7R104K500JT-2     | VIN                    |
| U2        | 1      |                   | RVT1H101M0810             | VIN                    |
| U8        | 2      | 5.0-OUTPUT        | LM2575S                   | $1N3623                |
| D1        | 1      |                   | SS34_C727060-C            | $1N3623                |
| L1        | 1      |                   | XR1265-101M-1             | $1N3623                |
| U5        | 1      | 5.08-4P-1         | XY128V-A                  | OUT4                   |
| U5        | 2      | 5.08-4P-2         | XY128V-A                  | OUT3                   |
| U5        | 3      | 5.08-4P-3         | XY128V-A                  | OUT2                   |
| U5        | 4      | 5.08-4P-4         | XY128V-A                  | OUT1                   |
| U20       | 1      |                   | TLV6001IDBVR-OUT          | $1N9360                |
| R24       | 2      |                   | 0603WAF3301T5E-2          | $1N9360                |
| C30       | 1      |                   | CC0603KRX7R7BB331-1       | $1N9360                |
| U21       | 3      | +IN               | TLV6001IDBVR              | $1N9360                |
| U20       | 3      | +IN               | TLV6001IDBVR              | $1N9358                |
| R23       | 1      |                   | 0603WAF3301T5E-1          | $1N9358                |
| C27       | 1      |                   | CC0603KRX7R7BB331-1       | $1N9358                |
| R25       | 1      |                   | 0603WAF3301T5E-1          | $1N9358                |
| U20       | 4      |                   | TLV6001IDBVR--IN          | $1N9362                |
| R24       | 1      |                   | 0603WAF3301T5E-1          | $1N9362                |
| C30       | 2      |                   | CC0603KRX7R7BB331-2       | $1N9362                |
| R26       | 1      |                   | 0603WAF3301T5E-1          | $1N9362                |
| R25       | 2      |                   | 0603WAF3301T5E-2          | $1N9711                |
| R27       | 2      |                   | 0603WAF1501T5E-2          | $1N9711                |
| U21       | 2      |                   | TLV6001IDBVR-V-           | $1N10109               |
| R29       | 1      |                   | MCT06030D6501BP100-1      | $1N10109               |
| R31       | 2      |                   | 0603WAF1002T5E-2          | $1N10109               |
| R33       | 1      |                   | 0603WAF220JT5E-1          | USB_DP1                |
| R32       | 1      |                   | 0603WAF220JT5E-1          | USB_DM1                |
