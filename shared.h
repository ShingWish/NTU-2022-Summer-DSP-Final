
//
// Included Files
//
#include "F2837xD_device.h"
#include "F2837xD_Cla_defines.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//
// Globals
//

// Buck 降壓轉換器規格 (Specifications)
// 這個結構定義了 Buck 電路的主要元件參數
typedef struct ejBuckSPECS {
   float L;    // H, 電感值
   float C;    // F, 電容值
   float r_L;  // ohm, 電感的等效串聯電阻 (ESR)
   float r_C;  // ohm, 電容的等效串聯電阻 (ESR)
   float R;    // ohm, 負載電阻
   float f;    // Hz, 切換頻率
} ejBuckSPECS;

// Buck 降壓轉換器輸入 (Input)
// 這個結構定義了 Buck 電路的輸入條件
typedef struct ejBuckInput {
   float v_i;  // V, 輸入電壓
   float duty; // 工作週期 (Duty Cycle)
} ejBuckInput;

// 狀態變數 (State Variable)
// 這個結構用於儲存一個狀態變數在不同時間點的值
typedef struct ejStateVariable {
   float preStep;  // 前一個時間步的值
   float step;     // 目前時間步的值
   float nextStep; // 下一個時間步的預測值
} ejStateVariable;

// Buck 降壓轉換器狀態 (State)
// 這個結構包含了 Buck 電路的所有狀態變數
typedef struct ejBuckState {
   ejStateVariable i_L; // 電感電流 (Inductor Current)
   ejStateVariable v_C; // 電容電壓 (Capacitor Voltage)
} ejBuckState;

// CLA 使用的 Buck 狀態指標
// 為了讓 CLA 能存取，使用 union 確保 32-bit 對齊
typedef union{
    ejBuckState *ptr; // 指向 ejBuckState 結構的指標
    uint32_t pad;     // 32-bit 填充，確保對齊
}CLA_ejBuckState;

// Buck 降壓轉換器輸出 (Output)
// 這個結構定義了 Buck 電路的輸出變數
typedef struct ejBuckOutput {
   float v_L; // V, 電感電壓
   float i_C; // A, 電容電流
   float v_o; // V, 輸出電壓
} ejBuckOutput;

// CLA 使用的 Buck 輸出指標
// 為了讓 CLA 能存取，使用 union 確保 32-bit 對齊
typedef union{
    ejBuckOutput *ptr; // 指向 ejBuckOutput 結構的指標
    uint32_t pad;      // 32-bit 填充，確保對齊
}CLA_ejBuckOutput;


//
// Function Prototypes
//
// 宣告在 CLA 上執行的任務 (Task)
// 這些是 CLA 任務的中斷服務常式原型
// CPU 可以透過這些原型來啟動 CLA 任務
//
__interrupt void Cla1Task1(); // CLA 任務 1: Buck 電路模擬計算
__interrupt void Cla1Task2(); // CLA 任務 2: (未使用)
__interrupt void Cla1Task3(); // CLA 任務 3: (未使用)
__interrupt void Cla1Task4(); // CLA 任務 4: (未使用)
__interrupt void Cla1Task5(); // CLA 任務 5: (未使用)
__interrupt void Cla1Task6(); // CLA 任務 6: (未使用)
__interrupt void Cla1Task7(); // CLA 任務 7: (未使用)
__interrupt void Cla1Task8(); // CLA 任務 8: 初始化 CLA 端的參數

#ifdef __cplusplus
}
#endif


//
// End of file
//