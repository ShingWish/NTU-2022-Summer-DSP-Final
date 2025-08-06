# TI F28379D Buck Converter Simulation Project

This project is an educational example demonstrating a real-time simulation of a synchronous buck converter on the Texas Instruments F28379D DSP. It showcases the powerful co-processing capabilities of the main CPU and the Control Law Accelerator (CLA), along with the use of essential peripherals like ADC, ePWM, DAC, and eCAP.

---

## How It Works

The project is structured to offload the computationally intensive simulation tasks to the CLA, freeing up the main CPU for control, I/O, and other system-level tasks.

### Core Components:
1.  **CPU (`main.c`)**:
    *   Initializes the system clock, GPIO, and all required peripherals (ADC, ePWM, DAC, eCAP).
    *   Configures the shared memory spaces for CPU-CLA communication.
    *   Sets the initial parameters for the buck converter model (e.g., L, C, R, Vin, duty cycle) and passes them to the CLA.
    *   Manages the main interrupt-driven loop.

2.  **CLA (`cla.c`)**:
    *   Executes the core mathematical model of the buck converter.
    *   The simulation is based on the converter's state-space equations, which are solved numerically using the Euler method in each time step.
    *   `Cla1Task1` contains the main simulation logic, calculating the inductor current and capacitor voltage.
    *   `Cla1Task8` is used for initializing variables on the CLA side.

3.  **Shared Header (`shared.h`)**:
    *   Defines the data structures (`ejBuckSPECS`, `ejBuckInput`) and function prototypes that are shared between the CPU and the CLA, ensuring seamless data exchange.

### Execution Flow:
1.  **Initialization**: The CPU configures the system and sends the initial converter parameters to the CLA via shared memory.
2.  **Triggering**: The ePWM1 module is configured to generate a periodic interrupt at the simulation frequency (`FREQ`), which triggers an ADC conversion.
3.  **ISR Execution**: When the ADC conversion is complete, the `adca1_isr` is called.
4.  **CLA Task Invocation**: Inside the ISR, the CPU forces `Cla1Task1` to run on the CLA. This command sends the latest inputs (like `v_i` and `duty`) to the CLA.
5.  **Simulation**: The CLA calculates the next values for the inductor current (`i_L`) and capacitor voltage (`v_C`).
6.  **Data Return**: The results are placed back into shared memory.
7.  **Output**: The CPU reads the simulation results from shared memory and outputs them to DAC channels A and B, allowing the simulated waveforms to be viewed on an oscilloscope.

---

## How to Use (Controlling Modes with Macros)

This project uses preprocessor macros to easily switch between different operating modes without changing the core logic. To change a mode, simply comment or uncomment the corresponding `#define` line at the top of the specified file.

### File: `main.c`

*   **`ADCVIN`**: Controls the source of the converter's input voltage (`v_i`).
    *   **To enable**: Uncomment the line `#define ADCVIN`. The simulation will use the voltage read from the **ADCINA2** pin as its input.
    *   **To disable**: Comment out the line `//#define ADCVIN`. The simulation will use the hardcoded value in the `vinChange` variable.

*   **`ECAPDUTY`**: Controls the source of the converter's duty cycle.
    *   **To enable**: Uncomment the line `#define ECAPDUTY`. The duty cycle will be calculated from an external PWM signal measured by the **eCAP1** module on GPIO19.
    *   **To disable**: Comment out the line `//#define ECAPDUTY`. The simulation will use the hardcoded value in the `EPWMDuty` variable.

### File: `cla.c`

*   **`EULER` / `IMPROVEDEULER`**: Selects the numerical integration method.
    *   **Usage**: Ensure only one of these macros is uncommented at a time.
    *   `#define EULER`: Uses the standard Euler method (less accurate, but faster).
    *   `//#define IMPROVEDEULER`: Uses the Improved Euler method (more accurate, but more computationally intensive).

*   **`DEBUG`**: Enables debugging breakpoints within the CLA task.
    *   **To enable**: Uncomment `#define DEBUG`. This activates the `__mdebugstop()` instruction inside `Cla1Task1`, which will pause the CLA during a debug session in Code Composer Studio (CCS). This is extremely useful for inspecting variable values in real-time.
    *   **To disable**: Comment out `//#define DEBUG`. The simulation will run continuously without pausing, which is necessary for normal operation and performance testing.

---
Copyright © 2025 Hsueh-Ju Wu @ NTU. All rights reserved.
---

# TI F28379D Buck 降壓轉換器模擬專案

本專案是一個教學範例，旨在展示如何在德州儀器 (TI) F28379D DSP 上進行同步 Buck 降壓轉換器的即時模擬。它著重於展示主 CPU 與控制律加速器 (CLA) 之間的協同處理能力，並結合了 ADC、ePWM、DAC 和 eCAP 等關鍵周邊的使用。

---

## 運作原理

本專案的架構是將計算密集型的模擬任務交給 CLA 處理，從而釋放主 CPU 資源以專注於控制、I/O 及其他系統級任務。

### 核心元件：
1.  **CPU (`main.c`)**:
    *   初始化系統時脈、GPIO 及所有必要的周邊 (ADC, ePWM, DAC, eCAP)。
    *   設定用於 CPU 與 CLA 之間通訊的共享記憶體空間。
    *   設定 Buck 轉換器模型的初始參數 (例如 L, C, R, Vin, 工作週期) 並將其傳遞給 CLA。
    *   管理由中斷驅動的主迴圈。

2.  **CLA (`cla.c`)**:
    *   執行 Buck 轉換器的核心數學模型。
    *   此模擬基於轉換器的狀態空間方程式，並在每個時間步長內使用歐拉法進行數值求解。
    *   `Cla1Task1` 包含主模擬邏輯，用於計算電感電流與電容電壓。
    *   `Cla1Task8` 用於初始化 CLA 端的變數。

3.  **共享標頭檔 (`shared.h`)**:
    *   定義了 CPU 與 CLA 之間共享的資料結構 (`ejBuckSPECS`, `ejBuckInput`) 與函式原型，以確保順暢的資料交換。

### 執行流程：
1.  **初始化**: CPU 設定系統組態，並透過共享記憶體將轉換器的初始參數傳送給 CLA。
2.  **觸發**: ePWM1 模組被設定為以模擬頻率 (`FREQ`) 產生週期性中斷，此中斷會觸發 ADC 進行轉換。
3.  **ISR 執行**: ADC 轉換完成後，會呼叫 `adca1_isr` 中斷服務常式。
4.  **CLA 任務調用**: 在 ISR 內部，CPU 強制 CLA 執行 `Cla1Task1` 任務，並將最新的輸入值 (如 `v_i` 和 `duty`) 傳遞給 CLA。
5.  **模擬**: CLA 根據當前狀態計算電感電流 (`i_L`) 與電容電壓 (`v_C`) 的下一個時間步的值。
6.  **資料回傳**: 計算結果被寫回共享記憶體。
7.  **輸出**: CPU 從共享記憶體讀取模擬結果，並將其輸出至 DAC 的 A 通道和 B 通道，讓模擬的電壓電流波形能透過示波器進行觀察。

---

## 如何使用 (透過宏定義控制不同模式)

本專案使用前置處理器宏 (preprocessor macros) 來讓您輕鬆切換不同的運作模式，而無需修改核心邏輯。要切換模式，只需在指定檔案的開頭將對應的 `#define` 行註解或取消註解即可。

### 檔案: `main.c`

*   **`ADCVIN`**: 控制轉換器輸入電壓 (`v_i`) 的來源。
    *   **如何啟用**: 取消註解 `#define ADCVIN` 這一行。模擬將會使用從 **ADCINA2** 腳位讀取到的電壓作為輸入電壓。
    *   **如何停用**: 註解掉 `//#define ADCVIN` 這一行。模擬將會使用 `vinChange` 變數中的硬編碼值。

*   **`ECAPDUTY`**: 控制轉換器工作週期的來源。
    *   **如何啟用**: 取消註解 `#define ECAPDUTY` 這一行。工作週期將會由 **eCAP1** 模組在 GPIO19 腳位上測量外部 PWM 訊號計算而來。
    *   **如何停用**: 註解掉 `//#define ECAPDUTY` 這一行。模擬將會使用 `EPWMDuty` 變數中的硬編碼值。

### 檔案: `cla.c`

*   **`EULER` / `IMPROVEDEULER`**: 選擇數值積分的演算法。
    *   **使用方式**: 請確保這兩個宏當中只有一個處於非註解狀態。
    *   `#define EULER`: 使用標準歐拉法 (較不精確，但計算速度快)。
    *   `//#define IMPROVEDEULER`: 使用改良型歐拉法 (較精確，但計算量較大)。

*   **`DEBUG`**: 在 CLA 任務中啟用除錯中斷點。
    *   **如何啟用**: 取消註解 `#define DEBUG`。這會啟用 `Cla1Task1` 中的 `__mdebugstop()` 指令，當您在 Code Composer Studio (CCS) 中進行除錯時，CLA 將會在此暫停，這對於即時檢查變數值非常有用。
    *   **如何停用**: 註解掉 `//#define DEBUG`。模擬將會連續運行而不會暫停，這對於正常操作和效能測試是必要的。

---
Copyright © 2025 Hsueh-Ju Wu @ NTU. All rights reserved.
---
