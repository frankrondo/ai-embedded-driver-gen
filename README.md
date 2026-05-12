# Drive_KY-023

STM32F103C8T6 上的 `KY-023` 摇杆 + `SSD1306` OLED 示例工程，基于 STM32 HAL 和 STM32CubeIDE 构建。

## 项目说明

这个工程演示了如何在没有 RTOS、没有 DMA、没有硬件 I2C 的前提下，完成摇杆输入采集、按键去抖、方向识别和 OLED 显示。

## 核心功能

- 读取 `KY-023` 摇杆的 X/Y 两路模拟量
- 对摇杆按键进行软件去抖
- 根据阈值计算 8 方向 + 中心状态
- 通过 PB8 / PB9 的软件模拟 I2C 驱动 OLED
- 在 OLED 上实时显示 X、Y、按键状态和方向

## 硬件连接

- PA0 -> `VRx`
- PA1 -> `VRy`
- PB12 -> `SW`，低电平有效
- PB8 -> `OLED SCL`
- PB9 -> `OLED SDA`
- PD0 / PD1 -> 8MHz 外部晶振
- PA13 / PA14 -> `SWD`

## 运行逻辑

主循环采用固定周期轮询：

- 每 10ms 读取一次摇杆数据并更新方向与按键状态
- 每 100ms 刷新一次 OLED 显示

## 主要文件

- `Core/Src/main.c`：系统初始化和主循环
- `Core/Src/ky023.c`：摇杆读取、去抖和方向计算
- `Core/Src/OLED.c`：OLED 软件 I2C 驱动和显示接口
- `Core/Inc/ky023.h`：摇杆驱动接口
- `Core/Inc/OLED.h`：OLED 驱动接口

## 构建方式

推荐使用 STM32CubeIDE 打开工程后直接构建。

如果使用命令行构建，请按照你的 CubeIDE 环境配置项目路径后执行相应的 headless build。

## 公开版说明

这个仓库保留了完整的可编译工程结构，适合学习、移植和二次开发。如果你只关心运行效果，可以优先查看 `Core/Src/main.c`、`Core/Src/ky023.c` 和 `Core/Src/OLED.c`。
