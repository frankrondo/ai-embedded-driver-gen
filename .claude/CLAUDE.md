# STM32 HAL库开发规则

## 基本要求
- 目标平台：STM32F103C8T6（Cortex-M3, 72MHz, 64KB Flash, 20KB RAM）
- 开发环境：STM32CubeIDE（CubeMX 6.17.0 + HAL FW_F1 V1.8.7）
- 固件库：**仅使用HAL库**，禁止使用标准外设库（SPL）或LL库
- 代码风格：遵循CubeMX生成的代码组织方式

## 构建与烧录
- IDE构建：STM32CubeIDE 打开项目目录 → Project → Build All
- CLI构建：`STM32CubeIDE --launcher.suppressErrors -nosplash -application org.eclipse.cdt.managedbuilder.core.headlessbuild -data . -import . -build "Drive_KY-023/Release"`
- 烧录/调试：通过 ST-LINK 经 SWD 接口（PA13/SWDIO, PA14/SWCLK）连接，CubeIDE 中 Run → Debug

## 引脚映射

| 引脚  | 功能            | 连接目标                  |
|-------|-----------------|---------------------------|
| PA0   | ADC1_IN0        | KY-023 VRx（X轴模拟量）    |
| PA1   | ADC1_IN1        | KY-023 VRy（Y轴模拟量）    |
| PB12  | GPIO输入（上拉） | KY-023 SW（按键，低电平有效）|
| PB8   | GPIO开漏输出    | OLED SCL（软件模拟I2C）    |
| PB9   | GPIO开漏输出    | OLED SDA（软件模拟I2C）    |
| PA13  | SWDIO           | ST-LINK 调试器             |
| PA14  | SWCLK           | ST-LINK 调试器             |
| PD0   | HSE OSC IN      | 8MHz 外部晶振              |
| PD1   | HSE OSC OUT     | 8MHz 外部晶振              |

JTAG 已禁用，仅保留 SWD：`__HAL_AFIO_REMAP_SWJ_NOJTAG()`（在 `HAL_MspInit()` 中调用）。

## 时钟配置

HSE 8MHz → PLL ×9 → SYSCLK 72MHz → AHB/APB2 72MHz, APB1 36MHz, ADC时钟 PCLK2/6=12MHz。

## 项目架构

```
main.c (超级循环, 无RTOS, 无DMA, 仅SysTick中断)
  ├── SystemClock_Config()     → HSE+PLL → 72MHz
  ├── MX_GPIO_Init()           → PB8/PB9(OD), PB12(输入上拉)
  ├── MX_ADC1_Init()           → ADC1 CH0+CH1
  ├── KY023_Init()             → 绑定 hadc1 + 按键引脚
  ├── OLED_Init()              → SSD1306 软I2C初始化序列
  └── 主循环:
       每10ms:  KY023_ReadRaw() → 单通道ADC轮询X/Y
                KY023_ReadKeyDebounced() → 3样本软件去抖
                KY023_GetDirection() → 8方向+中心计算
       每100ms: OLED显示更新
```

### 关键文件

| 文件 | 类型 | 作用 |
|------|------|------|
| `Core/Src/main.c` | 用户代码 | 系统初始化、ADC校准、主循环时序 |
| `Core/Src/ky023.c` + `.h` | 用户驱动 | 摇杆驱动：ADC读取、软件去抖、方向算法 |
| `Core/Src/OLED.c` + `.h` | 用户驱动 | SSD1306驱动：PB8/PB9软件模拟I2C、字符显示 |
| `Core/Inc/OLED_Font.h` | 用户资源 | 8×16 ASCII字模位图 |
| `Core/Src/adc.c` + `.h` | CubeMX生成 | ADC1初始化（CH0/CH1, 55.5周期采样） |
| `Core/Src/gpio.c` + `.h` | CubeMX生成 | GPIO引脚初始化 |
| `Core/Src/stm32f1xx_hal_msp.c` | CubeMX生成 | MSP初始化（AFIO SWJ重映射） |
| `Core/Src/stm32f1xx_it.c` | CubeMX生成 | 仅SysTick中断服务 |

### 关键设计细节

- **OLED I2C 是软件模拟的**，未使用硬件I2C1外设。`OLED.c` 通过 PB8/PB9 开漏输出模拟I2C时序，`stm32f1xx_hal_conf.h` 中 I2C 模块未启用。`OLED_Init()` 内部重新调用了 `MX_GPIO_Init()`（无害副作用）。
- **ADC 是单通道轮询模式**：`KY023_ReadRaw()` 每次配置一个通道、启动ADC、轮询等待（10ms超时）、停止ADC。尽管 CubeMX 配置了连续转换模式，驱动程序实际执行单次转换（每样本两次启停）。
- **按键去抖是纯软件实现**：3样本驻留计数器，主循环10ms轮询周期 = 30ms去抖窗口。低电平有效，内部上拉。未使用 EXTI。
- **方向算法**（ky023.c）：中心值 2048（ADC中位），死区 ±600，轴余量 300——比较 `|dx|-|dy|` 区分主轴与对角线方向，返回9状态枚举（CENTER, UP, DOWN, LEFT, RIGHT 及4个对角线）。

## 硬件抽象层使用规范
1. 所有外设操作**必须**通过HAL库函数完成，禁止直接操作寄存器
2. 外设初始化使用CubeMX生成的`MX_XXX_Init()`函数
3. 外设句柄统一使用CubeMX生成的`hXXX`命名（如`hadc1`、`htim2`）

## 代码组织规范
- 用户代码写在`/* USER CODE BEGIN */`和`/* USER CODE END */`注释之间
- 外设驱动采用独立的`.c/.h`文件，命名格式：`[外设名]_driver.c/h`
- 引脚、地址等硬件相关配置使用宏定义，集中在头文件顶部

## 函数使用规范
1. 所有HAL库函数返回值必须检查，处理错误状态
2. 延时使用`HAL_Delay()`，禁止使用`for`循环延时
3. 中断回调函数使用HAL库标准的`HAL_XXX_Callback()`格式

## 禁止事项
- 禁止使用`HAL_GPIO_EXTI_IRQHandler()`以外的中断处理方式
- 禁止在中断服务程序中执行耗时操作
- 禁止绕过HAL库直接读写外设寄存器
- 禁止使用`printf`重定向到串口时绕过HAL库

## 常用函数速查
- GPIO: `HAL_GPIO_WritePin()`, `HAL_GPIO_ReadPin()`, `HAL_GPIO_TogglePin()`
- UART: `HAL_UART_Transmit()`, `HAL_UART_Receive_IT()`, `HAL_UART_IRQHandler()`
- TIM: `HAL_TIM_Base_Start()`, `HAL_TIM_PWM_Start()`, `HAL_TIM_PeriodElapsedCallback()`
- ADC: `HAL_ADC_Start()`, `HAL_ADC_PollForConversion()`, `HAL_ADCEx_Calibration_Start()`
- I2C: `HAL_I2C_Master_Transmit()`, `HAL_I2C_Mem_Read()`