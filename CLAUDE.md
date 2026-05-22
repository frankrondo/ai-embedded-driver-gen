# STM32 HAL 库开发规则（完善版）

> 适用芯片：STM32F103C8T6 | 环境：STM32CubeMX + STM32CubeIDE | 固件库：HAL 库

---

## 一、基本要求

| 项目 | 说明 |
|------|------|
| 目标芯片 | STM32F103C8T6（LQFP48, 64KB Flash, 20KB SRAM） |
| 开发环境 | STM32CubeMX 6.x + STM32CubeIDE |
| 固件库 | **仅使用 HAL 库**，严禁标准外设库（SPL）、LL 库 |
| 编程语言 | C99 / C11，CubeIDE 默认标准 |
| 代码风格 | 遵循 CubeMX 自动生成的代码组织方式 |
| 工具链 | ARM-GCC（CubeIDE 内置） |

---

## 二、工程结构规范

### 2.1 目录结构

```
Project/
├── Core/
│   ├── Inc/              # 头文件（main.h, gpio.h, adc.h, [外设]_driver.h）
│   ├── Src/              # 源文件（main.c, gpio.c, adc.c, [外设]_driver.c）
│   └── Startup/          # 启动文件（startup_stm32f103c8tx.s）
├── Drivers/
│   ├── CMSIS/            # CMSIS 核心文件
│   └── STM32F1xx_HAL_Driver/  # HAL 库源码
├── STM32CubeIDE/         # IDE 相关文件（链接脚本等）
├── Debug/                # 编译输出（不纳入版本控制）
├── [Project].ioc         # CubeMX 配置文件（纳入版本控制）
└── README.md
```

### 2.2 文件命名

- 外设驱动：`[外设小写]_driver.c/h`，如 `ky023.c/h`、`oled.c/h`
- 模块驱动：`[模块名].c/h`，如 `ssd1306.c/h`、`mpu6050.c/h`
- 中间件/协议栈放在独立目录，如 `Middleware/FreeRTOS/`

### 2.3 USER CODE 区域

CubeMX 生成的代码中有严格的分隔注释，**用户代码只能写在指定区域内**：

| 区域 | 位置 | 用途 |
|------|------|------|
| `USER CODE BEGIN Includes` | 文件头 `#include` 区 | 添加用户头文件引用 |
| `USER CODE BEGIN PD` | Private Define 区 | 宏定义 |
| `USER CODE BEGIN PV` | Private Variables 区 | 全局/静态变量 |
| `USER CODE BEGIN PFP` | Private Function Prototypes | 私有函数声明 |
| `USER CODE BEGIN 0` | `main()` 之前 | 私有函数实现 |
| `USER CODE BEGIN 2` | 外设初始化之后 | 用户外设初始化 |
| `USER CODE BEGIN WHILE` | while(1) 循环内部 | 主循环业务逻辑 |
| `USER CODE BEGIN 3` | while(1) 循环末尾 | 补充代码（极少使用） |
| `USER CODE BEGIN 4` | 文件末尾 | 其他私有函数实现 |

> **严格禁止**：在 USER CODE BEGIN/END 之外的区域写入任何代码，否则 CubeMX 重新生成代码时会丢失。

---

## 三、硬件抽象层使用规范

### 3.1 基本原则

1. 所有外设操作**必须**通过 HAL 库函数完成，**禁止直接操作寄存器**（如 `GPIOA->BSRR = ...`）
2. 外设初始化使用 CubeMX 生成的 `MX_XXX_Init()` 函数
3. 外设句柄统一使用 CubeMX 生成的 `hXXX` 命名（如 `hadc1`、`htim2`、`huart1`）
4. 句柄结构体在使用前**必须**初始化为 `{0}`，否则残留值会导致不可预期的行为

### 3.2 句柄使用模式

```c
/* 正确：初始化为零 */
UART_HandleTypeDef huart1 = {0};
TIM_HandleTypeDef htim2 = {0};

/* 错误：未初始化，残留随机值 */
UART_HandleTypeDef huart1;
```

---

## 四、函数使用规范

### 4.1 返回值检查（强制）

**所有 HAL 库函数调用必须检查返回值**，包括但不限于：

```c
/* 模式 1：返回错误码时直接 Error_Handler */
if (HAL_ADC_Start(&hadc1) != HAL_OK)
{
    Error_Handler();
}

/* 模式 2：逐级返回，让调用者处理 */
if (HAL_UART_Transmit(&huart1, data, len, timeout) != HAL_OK)
{
    return HAL_ERROR;
}

/* 模式 3：带清理的错误处理 */
if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK)
{
    (void)HAL_ADC_Stop(&hadc1);  /* 清理资源 */
    return HAL_ERROR;
}
```

### 4.2 延时

- **必须使用** `HAL_Delay(ms)` 进行毫秒级延时
- **严禁** `for`/`while` 循环延时（阻塞 CPU 且时间不准）
- 微秒级延时在 F103 上没有硬件支持，可接受短循环（<100us）或使用定时器实现
- 非阻塞延时使用 `HAL_GetTick()` 配合时间差判断，这是**强烈推荐**的做法

```c
/* 推荐：非阻塞延时 */
uint32_t lastTick = HAL_GetTick();
while (1)
{
    if ((HAL_GetTick() - lastTick) >= PERIOD_MS)
    {
        lastTick = HAL_GetTick();
        /* 执行周期性任务 */
    }
}
```

### 4.3 中断回调函数

- 使用 HAL 库标准的 `__weak` 回调函数格式，**不要重写** IRQHandler：

```c
/* 正确：重写回调 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
    {
        /* 用户逻辑 */
    }
}

/* 错误：重写中断服务程序 */
void TIM2_IRQHandler(void)  /* 不要这样做！ */
{
    /* ... */
}
```

- 回调函数中**严禁**执行耗时操作（如大量计算、`printf` 打印、长时间等待）
- 回调中应设置标志位，由主循环处理实际逻辑

---

## 五、中断管理规范

### 5.1 NVIC 优先级

- F103 使用 4 位抢占优先级（默认 `NVIC_PRIORITYGROUP_4`）
- 优先级数值越小，优先级越高（0 最高）
- 各外设建议优先级：

| 外设中断 | 建议优先级 | 原因 |
|----------|-----------|------|
| SysTick | 0（最高） | HAL 时基，不可被抢占 |
| 关键通信（UART DMA） | 1 | 防止数据丢失 |
| 定时器控制类 | 2-4 | 实时控制 |
| 普通外设（EXTI, I2C） | 5-8 | 普通响应 |
| 低优先级（按键等） | 9-15 | 可延迟处理 |

### 5.2 中断服务程序规则

- **禁止在 ISR 中**：
  - 执行超过 1ms 的操作
  - 调用 `HAL_Delay()`（依赖 SysTick 中断，优先级冲突会死锁）
  - 调用 `printf()` 等重量级函数
  - 进行浮点运算（F103 无硬件 FPU）
- ISR 中使用 `volatile` 修饰共享变量

---

## 六、DMA 使用规范

### 6.1 何时使用 DMA

- 大量数据传输（ADC 多通道扫描、UART 大数据包、SPI 显示刷新）
- 需要释放 CPU 的场景
- F103 的 DMA 有 7 个通道（DMA1），各通道与特定外设绑定，**配置前查阅参考手册确认通道映射**

### 6.2 DMA 配置注意事项

- DMA 缓冲区必须声明为全局或静态变量（不能在栈上）
- 使用 `HAL_XXX_Start_DMA()` 而非 `HAL_XXX_Start()`
- 传输完成回调中处理数据：

```c
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
        /* 处理 ADC DMA 数据 */
    }
}
```

### 6.3 F103 内存对齐

- DMA 缓冲区地址建议 4 字节对齐
- 使用 `__ALIGN_BEGIN` / `__ALIGN_END` 宏或在链接脚本中确保

---

## 七、时钟配置规范

### 7.1 时钟源选择

| 时钟源 | 频率 | 用途 |
|--------|------|------|
| HSE（外部晶振） | 8 MHz（推荐） | PLL 输入，精度高 |
| HSI（内部 RC） | 8 MHz | 无需外部晶振，精度较低 |
| LSE（外部 32.768kHz） | 32.768 kHz | RTC 时钟 |
| LSI（内部 40kHz） | ~40 kHz | 独立看门狗 |

### 7.2 PLL 配置（F103 典型值）

- `HSE = 8 MHz` → `PLLMUL = 9` → `SYSCLK = 72 MHz`（最大）
- `AHB = 72 MHz`，`APB1 = 36 MHz`，`APB2 = 72 MHz`
- `ADC 时钟 = PCLK2 / 6 = 12 MHz`（F103 ADC 最大 14 MHz）
- 配置对应的 `FLASH_LATENCY`（72 MHz → `FLASH_LATENCY_2`）

### 7.3 时钟配置检查清单

- [ ] HSE 起振是否稳定？（建议加 100ms 延时后检查 `HAL_RCC_OscConfig` 返回值）
- [ ] `AHB`/`APB1`/`APB2` 分频是否正确？
- [ ] USB 时钟是否需要 48 MHz？（F103 需 `PLLMUL=12, PLLDIV=1.5`）
- [ ] ADC 时钟是否 ≤14 MHz？

---

## 八、外设使用补充规则

### 8.1 GPIO

```
HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);     // 置高
HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);   // 置低
HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_8);                   // 翻转
GPIO_PinState state = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_12);  // 读取
```

- 引脚模式在 CubeMX 中配置（输入/输出/复用/模拟），代码中**不应动态更改**
- 需要动态切换模式的场景，使用 `HAL_GPIO_Init()` 重新配置

### 8.2 UART

```
HAL_UART_Transmit(&huart1, data, len, timeout);           // 阻塞发送
HAL_UART_Receive_IT(&huart1, data, len);                  // 中断接收
HAL_UART_Transmit_DMA(&huart1, data, len);                // DMA 发送
HAL_UART_IRQHandler(&huart1);                             // 中断处理（IRQ 中调用）
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);  // 接收完成回调
```

- 中断接收时，务必在 `USARTx_IRQHandler` 中调用 `HAL_UART_IRQHandler()`
- 中断接收一次只能注册一个 buffer，接收完成后需再次调用 `HAL_UART_Receive_IT()` 启动下一次接收
- `printf` 重定向建议通过 `__io_putchar()` 或重写 `_write()` 实现，**必须**调用 `HAL_UART_Transmit()`

### 8.3 TIM

```
HAL_TIM_Base_Start(&htim2);                               // 基本定时器启动
HAL_TIM_Base_Start_IT(&htim2);                            // 定时器中断启动
HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);                 // PWM 输出启动
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim); // 周期中断回调
__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, duty);       // 动态修改占空比
```

- 定时器句柄的 `Instance` 成员用于区分不同定时器（`htim->Instance == TIM2`）
- 输入捕获用 `HAL_TIM_IC_Start_IT()` + `HAL_TIM_IC_CaptureCallback()`
- 编码器模式用 `HAL_TIM_Encoder_Start()` + `__HAL_TIM_GET_COUNTER()`

### 8.4 ADC

```
HAL_ADC_Start(&hadc1);                                    // 启动 ADC
HAL_ADC_PollForConversion(&hadc1, timeout);               // 等待转换完成
uint32_t value = HAL_ADC_GetValue(&hadc1);                // 读取结果
HAL_ADC_Stop(&hadc1);                                     // 停止 ADC
HAL_ADCEx_Calibration_Start(&hadc1);                      // 校准（F103 必须调用）
HAL_ADC_Start_DMA(&hadc1, buffer, length);                // DMA 多通道扫描
```

- **F103 特殊规则**：
  - ADC 使用前**必须**调用 `HAL_ADCEx_Calibration_Start()` 校准，否则精度严重下降
  - 单通道轮询模式下，每次切换通道需重新调用 `HAL_ADC_ConfigChannel()`
  - 多通道扫描推荐使用 DMA 模式
  - 连续转换模式（`ContinuousConvMode = ENABLE`）下 `HAL_ADC_Start()` 只需调用一次

### 8.5 I2C

```
HAL_I2C_Master_Transmit(&hi2c1, devAddr, data, len, timeout);    // 主机发送
HAL_I2C_Master_Receive(&hi2c1, devAddr, data, len, timeout);     // 主机接收
HAL_I2C_Mem_Write(&hi2c1, devAddr, memAddr, size, data, len, timeout); // 写寄存器
HAL_I2C_Mem_Read(&hi2c1, devAddr, memAddr, size, data, len, timeout);  // 读寄存器
```

- **F103 硬件 I2C 已知问题**：F103 的硬件 I2C 存在某些勘误，高速或复杂场景下可能不稳定。备选方案：
  - 使用软件模拟 I2C（GPIO 位带操作）
  - 如果使用硬件 I2C，务必配置超时参数并处理 `HAL_BUSY`、`HAL_TIMEOUT` 等异常状态
- 软件 I2C 需要精确的延时控制（建议用 `DWT` 或 `nop` 循环实现微秒级延时）

### 8.6 SPI

```
HAL_SPI_Transmit(&hspi1, data, len, timeout);             // 发送
HAL_SPI_Receive(&hspi1, data, len, timeout);              // 接收
HAL_SPI_TransmitReceive(&hspi1, txData, rxData, len, timeout); // 全双工
HAL_SPI_Transmit_DMA(&hspi1, data, len);                  // DMA 发送
```

### 8.7 EXTI（外部中断）

- **只能通过** `HAL_GPIO_EXTI_IRQHandler(GPIO_Pin)` 处理，**禁止**自定义中断处理
- 回调函数：`void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)`
- 同一 Pin 号的中断线在 F103 上共享（如 PA0/PB0/PC0 共用 EXTI0），回调中必须检查 `GPIO_Pin` 区分来源

### 8.8 看门狗

```
HAL_IWDG_Init(&hiwdg);                                    // 独立看门狗初始化
HAL_IWDG_Refresh(&hiwdg);                                 // 喂狗
```

- 调试时建议通过 `__HAL_DBGMCU_FREEZE_IWDG()` 在调试暂停时冻结看门狗
- 看门狗复位后检查 `__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)` 判断复位源

### 8.9 Flash 操作

```
HAL_FLASH_Unlock();                                       // 解锁
HAL_FLASH_Program(type, address, data);                   // 编程
HAL_FLASH_Lock();                                         // 上锁
HAL_FLASHEx_Erase(&eraseInit, &pageError);                // 页擦除
```

- F103 Flash 页大小 1KB（前 128 页）
- 写 Flash 前必须先擦除，擦除以页为单位
- 用户数据可存放在 Flash 末尾几页，避开代码区

---

## 九、错误处理与调试规范

### 9.1 Error_Handler 实现

默认的 `Error_Handler()` 仅死循环，建议增强：

```c
void Error_Handler(void)
{
    __disable_irq();
    /* 可选：通过 LED 闪烁指示错误码 */
    /* 可选：将错误信息写入备份寄存器供复位后读取 */
    while (1)
    {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);  /* LED 闪烁 */
        HAL_Delay(200);
    }
}
```

### 9.2 assert_param 断言

- 在 CubeMX 中启用 `USE_FULL_ASSERT`，开发阶段推荐开启
- 自定义 `assert_failed()` 输出文件名和行号，方便定位

### 9.3 SWD 调试接口

- **必须保留** SWD 引脚（PA13-SWDIO, PA14-SWCLK），不要复用为 GPIO
- 使用 `SYS → Debug → Serial Wire` 配置
- 调试模式下注意看门狗冻结、低功耗模式行为

### 9.4 HAL 返回值速查

| 返回值 | 含义 |
|--------|------|
| `HAL_OK` | 成功 |
| `HAL_ERROR` | 一般错误 |
| `HAL_BUSY` | 外设忙碌 |
| `HAL_TIMEOUT` | 超时 |

---

## 十、代码风格与命名规范

### 10.1 命名约定

| 类型 | 命名格式 | 示例 |
|------|---------|------|
| 全局变量 | `g_xxx` 前缀 | `g_joyX`、`g_direction` |
| 静态变量 | `s_xxx` 前缀 或 不加前缀 | `static uint8_t debounce_count` |
| 宏定义 | 全大写 + 下划线 | `KY023_ADC_DEADZONE` |
| 枚举类型 | `[模块]_XXXTypeDef` | `KY023_DirectionTypeDef` |
| 句柄类型 | `[模块]_HandleTypeDef` | `KY023_HandleTypeDef` |
| 函数名 | `[模块]_[动作][对象]()` | `KY023_ReadRaw()`、`KY023_GetDirection()` |
| 局部变量 | 小驼峰或下划线 | `xValue`、`current_tick` |

### 10.2 数据类型

- 使用 `stdint.h` 中的固定宽度类型（`uint8_t`、`uint16_t`、`uint32_t`、`int32_t` 等）
- 避免使用 `int`、`char` 等类型宽度不确定的类型
- HAL 库中的 `HAL_StatusTypeDef` 用于状态返回

### 10.3 注释规范

- 每个 `.c/.h` 文件头部有简要说明
- 公共函数使用 Doxygen 风格注释：

```c
/**
  * @brief  读取摇杆原始 ADC 值
  * @param  hky023: KY023 句柄指针
  * @param  xValue: 输出 X 轴 ADC 值
  * @param  yValue: 输出 Y 轴 ADC 值
  * @retval HAL_StatusTypeDef
  */
HAL_StatusTypeDef KY023_ReadRaw(KY023_HandleTypeDef *hky023,
                                uint16_t *xValue,
                                uint16_t *yValue);
```

---

## 十一、性能与资源管理

### 11.1 F103C8T6 资源限制

| 资源 | 容量 | 注意 |
|------|------|------|
| Flash | 64 KB | 代码+常量数据 |
| SRAM | 20 KB | 栈+堆+全局变量+DMA 缓冲区 |
| 栈大小 | 建议 >= 1KB | 在链接脚本中配置 |
| 堆大小 | 建议 >= 512B | `malloc` 使用，不用可设为 0 |

### 11.2 栈溢出防范

- 避免在函数中声明大数组（>256B），改用静态或全局变量
- 避免深度递归调用
- 开发阶段可在链接脚本中增大栈空间并监控 `__initial_sp`

### 11.3 编译优化

- Debug 配置：`-Og`（调试友好）
- Release 配置：`-Os`（优化体积）或 `-O2`（优化速度）
- 对时序敏感的变量使用 `volatile`（中断共享变量、硬件寄存器映射区）

---

## 十二、CubeMX 工作流规范

### 12.1 重新生成代码

1. 修改 `.ioc` 文件完成配置
2. 点击 "GENERATE CODE" 重新生成
3. CubeMX **不会覆盖** `USER CODE BEGIN/END` 之间的用户代码
4. 生成后重新编译，检查是否有冲突

### 12.2 ioc 文件管理

- `.ioc` 文件纳入版本控制（Git），它是硬件配置的唯一真相源
- 团队协作时，`.ioc` 文件不能多人同时编辑，需锁机制
- 重要配置变更后及时提交 `.ioc` + 生成后的代码

### 12.3 项目移植清单

- [ ] `.ioc` 文件
- [ ] `Core/Inc/` 和 `Core/Src/` 的用户代码
- [ ] 自定义链接脚本（如果有修改）
- [ ] 新增的驱动文件

---

## 十三、禁止事项（完整清单）

1. **禁止**直接操作寄存器（如 `GPIOA->BSRR`、`TIM2->CNT`）
2. **禁止**使用标准外设库（SPL）或 LL 库
3. **禁止**绕过 HAL 库的中断管理（如自定义 EXTI ISR）
4. **禁止**在中断服务程序中执行耗时操作（>1ms、浮点运算、printf）
5. **禁止**使用 `for`/`while` 循环延时，统一用 `HAL_Delay()` 或 `HAL_GetTick()` 时间差
6. **禁止**在 `USER CODE BEGIN/END` 之外写代码
7. **禁止**在 ISR 中调用 `HAL_Delay()`（会导致死锁）
8. **禁止**未初始化的句柄结构体（必须 `= {0}`）
9. **禁止**忽略 HAL 函数返回值
10. **禁止**在栈上分配 DMA 缓冲区
11. **禁止**在中断回调中使用非 `volatile` 变量跨线程共享
12. **禁止**在 `while(1)` 中使用阻塞等待（如 `while(HAL_GPIO_ReadPin(...) == RESET);`），改用状态机
13. **禁止**复用 SWD 引脚（PA13/PA14）为 GPIO

---

## 十四、常用函数速查（完整版）

### GPIO
| 函数 | 用途 |
|------|------|
| `HAL_GPIO_WritePin(port, pin, state)` | 写引脚 |
| `HAL_GPIO_ReadPin(port, pin)` | 读引脚 |
| `HAL_GPIO_TogglePin(port, pin)` | 翻转引脚 |
| `HAL_GPIO_EXTI_IRQHandler(pin)` | 外部中断处理（ISR 中调用） |
| `HAL_GPIO_EXTI_Callback(pin)` | 外部中断回调（用户重写） |

### UART
| 函数 | 用途 |
|------|------|
| `HAL_UART_Transmit(&huart, buf, len, timeout)` | 阻塞发送 |
| `HAL_UART_Receive(&huart, buf, len, timeout)` | 阻塞接收 |
| `HAL_UART_Transmit_IT(&huart, buf, len)` | 中断发送 |
| `HAL_UART_Receive_IT(&huart, buf, len)` | 中断接收 |
| `HAL_UART_Transmit_DMA(&huart, buf, len)` | DMA 发送 |
| `HAL_UART_Receive_DMA(&huart, buf, len)` | DMA 接收 |
| `HAL_UART_IRQHandler(&huart)` | 中断处理（USARTx_IRQHandler 中调用） |
| `HAL_UART_TxCpltCallback(&huart)` | 发送完成回调 |
| `HAL_UART_RxCpltCallback(&huart)` | 接收完成回调 |

### TIM
| 函数 | 用途 |
|------|------|
| `HAL_TIM_Base_Start(&htim)` | 启动基本定时器 |
| `HAL_TIM_Base_Start_IT(&htim)` | 启动定时器中断 |
| `HAL_TIM_PWM_Start(&htim, channel)` | 启动 PWM 输出 |
| `HAL_TIM_IC_Start_IT(&htim, channel)` | 启动输入捕获中断 |
| `HAL_TIM_Encoder_Start(&htim, channel)` | 启动编码器模式 |
| `__HAL_TIM_SET_COMPARE(&htim, channel, value)` | 设置比较值（动态 PWM 占空比） |
| `__HAL_TIM_GET_COUNTER(&htim)` | 读取计数器值 |
| `HAL_TIM_PeriodElapsedCallback(&htim)` | 周期中断回调 |
| `HAL_TIM_IC_CaptureCallback(&htim)` | 输入捕获回调 |

### ADC
| 函数 | 用途 |
|------|------|
| `HAL_ADC_Start(&hadc)` | 启动 ADC |
| `HAL_ADC_Stop(&hadc)` | 停止 ADC |
| `HAL_ADC_PollForConversion(&hadc, timeout)` | 等待转换完成 |
| `HAL_ADC_GetValue(&hadc)` | 读取转换结果 |
| `HAL_ADC_ConfigChannel(&hadc, &config)` | 配置通道 |
| `HAL_ADC_Start_DMA(&hadc, buf, len)` | 启动 DMA 扫描 |
| `HAL_ADCEx_Calibration_Start(&hadc)` | ADC 校准（F103 必须） |
| `HAL_ADC_ConvCpltCallback(&hadc)` | 转换完成回调 |

### I2C
| 函数 | 用途 |
|------|------|
| `HAL_I2C_Master_Transmit(&hi2c, addr, buf, len, timeout)` | 主机发送 |
| `HAL_I2C_Master_Receive(&hi2c, addr, buf, len, timeout)` | 主机接收 |
| `HAL_I2C_Mem_Write(&hi2c, addr, memAddr, size, buf, len, timeout)` | 写从设备寄存器 |
| `HAL_I2C_Mem_Read(&hi2c, addr, memAddr, size, buf, len, timeout)` | 读从设备寄存器 |
| `HAL_I2C_IsDeviceReady(&hi2c, addr, trials, timeout)` | 检测设备是否就绪 |

### SPI
| 函数 | 用途 |
|------|------|
| `HAL_SPI_Transmit(&hspi, buf, len, timeout)` | 阻塞发送 |
| `HAL_SPI_Receive(&hspi, buf, len, timeout)` | 阻塞接收 |
| `HAL_SPI_TransmitReceive(&hspi, txBuf, rxBuf, len, timeout)` | 全双工传输 |
| `HAL_SPI_Transmit_DMA(&hspi, buf, len)` | DMA 发送 |

### 系统
| 函数 | 用途 |
|------|------|
| `HAL_Init()` | HAL 库初始化 |
| `HAL_Delay(ms)` | 毫秒延时 |
| `HAL_GetTick()` | 获取系统滴答计数 |
| `HAL_NVIC_SetPriority(irq, preempt, sub)` | 设置中断优先级 |
| `HAL_NVIC_EnableIRQ(irq)` | 使能中断 |
