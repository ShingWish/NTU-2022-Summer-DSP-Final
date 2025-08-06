//
// Included Files
//
#include "shared.h"

//
// Defines
//

#define DEBUG // 定義 DEBUG 宏，用於除錯

#define EULER // 定義 EULER 宏，使用歐拉法進行數值積分
//#define IMPROVEDEULER // 註解掉 IMPROVEDEULER 宏，不使用改良型歐拉法

#define sample 5   // 定義每個切換週期的取樣點數
#define window 1500 // 定義觀察的切換週期數
#define length sample*window // 定義總資料長度

//
// Globals
//
extern ejBuckSPECS buckSPECS; // 引用來自 CPU 的 Buck 電路規格結構
extern ejBuckInput buckInput; // 引用來自 CPU 的 Buck 電路輸入結構

extern float DAC_V_O; // 引用與 CPU 分享的 DAC 輸出電壓變數
extern float DAC_I_L; // 引用與 CPU 分享的 DAC 電感電流變數

float buckState_i_L_preStep;  // A, 前一步的電感電流
float buckState_i_L_step;     // A, 目前的電感電流
float buckState_i_L_nextStep; // A, 下一步的電感電流
float buckState_v_C_preStep;  // V, 前一步的電容電壓
float buckState_v_C_step;     // V, 目前的電容電壓
float buckState_v_C_nextStep; // V, 下一步的電容電壓

float buckOutput_v_L;    // V, 電感電壓
float buckOutput_i_C;    // A, 電容電流
float buckOutput_v_o;    // V, 輸出電壓


float prdCTR; // 切換週期計數器
float rC_p_R; // 電容等效串聯電阻 (r_C) 與負載電阻 (R) 的並聯等效電阻
float rC_s_R; // 電容等效串聯電阻 (r_C) 與負載電阻 (R) 的串聯等效電阻
float dt;     // 時間步長 (delta time)

//
// Function Definitions
//
float parallelAnB(float, float); // 計算兩個電阻的並聯值
float seriesAnB(float, float);   // 計算兩個電阻的串聯值
void ejBuckInitSetupCLA(void);   // 初始化 CLA 端的 Buck 電路參數
void debug(void);                // 除錯函式

// CLA 任務 1：Buck 電路模擬
__interrupt void Cla1Task1 ( void )
{
    // 將輸出電壓和電感電流寫入與 CPU 分享的變數
    DAC_V_O = buckOutput_v_o;
    DAC_I_L = buckState_i_L_step;

    // 若負載有變，重新計算等效電阻
    rC_p_R = parallelAnB(buckSPECS.r_C, buckSPECS.R);
    rC_s_R = seriesAnB(buckSPECS.r_C, buckSPECS.R);


#ifdef EULER
// 歐拉法 (Euler method)
    // 判斷目前是否為開關導通 (On) 狀態
    if(prdCTR<(int)(buckInput.duty*sample)){
        // Q On 狀態方程式
        // 計算電感電壓
        buckOutput_v_L = (- (buckSPECS.r_L + rC_p_R)*buckState_i_L_step
                        - (buckSPECS.R / rC_s_R)*buckState_v_C_step
                        + buckInput.v_i );

        // 計算下一步的電感電流
        buckState_i_L_nextStep = buckState_i_L_step +
                                buckOutput_v_L / buckSPECS.L * dt;

        // 計算電容電流
        buckOutput_i_C = ( (buckSPECS.R / rC_s_R)*buckState_i_L_step
                        - (1.0 / rC_s_R)*buckState_v_C_step );

        // 計算下一步的電容電壓
        buckState_v_C_nextStep = buckState_v_C_step +
                                buckOutput_i_C / buckSPECS.C * dt;
    }
    // 判斷目前是否為開關關斷 (Off) 狀態
    else if(prdCTR>=(int)(buckInput.duty*sample)) {
        // Q Off 狀態方程式
        // 計算電感電壓
        buckOutput_v_L = (- (buckSPECS.r_L + rC_p_R)*buckState_i_L_step
                        - (buckSPECS.R / rC_s_R)*buckState_v_C_step);

        // 計算下一步的電感電流
        buckState_i_L_nextStep = buckState_i_L_step +
                                buckOutput_v_L / buckSPECS.L * dt;

        // 計算電容電流
        buckOutput_i_C = ( (buckSPECS.R / rC_s_R)*buckState_i_L_step
                        - (1 / rC_s_R)*buckState_v_C_step );

        // 計算下一步的電容電壓
        buckState_v_C_nextStep = buckState_v_C_step +
                                buckOutput_i_C / buckSPECS.C * dt;
    }

    // 確保電感電流不為負值
    if(buckState_i_L_nextStep < 0) buckState_i_L_nextStep = 0;
#endif

#ifdef IMPROVEDEULER
// 改良型歐拉法 (Improved Euler method)
    if(prdCTR<(int)(buckInput.duty*sample)){
        debug();
        // Q On 狀態
        // 預測值
        buckOutput_v_L = (- (buckSPECS.r_L + rC_p_R)*buckState_i_L_step
                        - (buckSPECS.R / rC_s_R)*buckState_v_C_step
                        + buckInput.v_i );
        buckState_i_L_nextStep = buckState_i_L_step +
                                buckOutput_v_L / buckSPECS.L * dt;
        buckOutput_i_C = ( (buckSPECS.R / rC_s_R)*buckState_i_L_step
                        - (1 / rC_s_R)*buckState_v_C_step );
        buckState_v_C_nextStep = buckState_v_C_step +
                                buckOutput_i_C / buckSPECS.C * dt;
        // 校正值
        buckOutput_v_L += (- (buckSPECS.r_L + rC_p_R)*buckState_i_L_nextStep
                        - (buckSPECS.R / rC_s_R)*buckState_v_C_nextStep
                        + buckInput.v_i );
        buckOutput_v_L /= 2;
        buckState_i_L_nextStep = buckState_i_L_step +
                                buckOutput_v_L / buckSPECS.L * dt;
        buckOutput_i_C += ( (buckSPECS.R / rC_s_R)*buckState_i_L_nextStep
                        - (1 / rC_s_R)*buckState_v_C_nextStep );
        buckOutput_i_C /= 2;
        buckState_v_C_nextStep = buckState_v_C_step +
                                buckOutput_i_C / buckSPECS.C * dt;
    }
    else if(prdCTR>=(int)(buckInput.duty*sample)) {
        debug();
        // Q Off 狀態
        // 預測值
        buckOutput_v_L = (- (buckSPECS.r_L + rC_p_R)*buckState_i_L_step
                        - (buckSPECS.R / rC_s_R)*buckState_v_C_step);
        buckState_i_L_nextStep = buckState_i_L_step +
                                buckOutput_v_L / buckSPECS.L * dt;
        // 校正值
        buckOutput_v_L += (- (buckSPECS.r_L + rC_p_R)*buckState_i_L_nextStep
                        - (buckSPECS.R / rC_s_R)*buckState_v_C_nextStep);
        buckOutput_v_L /= 2;
        buckState_i_L_nextStep = buckState_i_L_step +
                                buckOutput_v_L / buckSPECS.L * dt;
        // 預測值
        buckOutput_i_C = ( (buckSPECS.R / rC_s_R)*buckState_i_L_step
                        - (1 / rC_s_R)*buckState_v_C_step );
        buckState_v_C_nextStep = buckState_v_C_step +
                                buckOutput_i_C / buckSPECS.C * dt;
        // 校正值
        buckOutput_i_C = ( (buckSPECS.R / rC_s_R)*buckState_i_L_nextStep
                        - (1 / rC_s_R)*buckState_v_C_nextStep );
        buckOutput_i_C /= 2;
        buckState_v_C_nextStep = buckState_v_C_step +
                                buckOutput_i_C / buckSPECS.C * dt;
    }

    if(buckState_i_L_nextStep < 0) buckState_i_L_nextStep = 0;
#endif

    // 計算輸出電壓
    buckOutput_v_o = rC_p_R * buckState_i_L_step + (buckSPECS.R / rC_s_R)*buckState_v_C_step;

    // 更新時間步
    buckState_i_L_step = buckState_i_L_nextStep;
    buckState_v_C_step = buckState_v_C_nextStep;

    // 切換週期計數器加一，並在達到取樣點數後歸零
    prdCTR++;
    if(prdCTR==sample) prdCTR = 0;

    // 觸發除錯中斷點
    debug();
}

// CLA 任務 2 (未使用)
__interrupt void Cla1Task2 ( void )
{

}

// CLA 任務 3 (未使用)
__interrupt void Cla1Task3 ( void )
{

}

// CLA 任務 4 (未使用)
__interrupt void Cla1Task4 ( void )
{

}

// CLA 任務 5 (未使用)
__interrupt void Cla1Task5 ( void )
{

}

// CLA 任務 6 (未使用)
__interrupt void Cla1Task6 ( void )
{

}

// CLA 任務 7 (未使用)
__interrupt void Cla1Task7 ( void )
{

}

// CLA 任務 8：初始化 CLA 端的參數
__interrupt void Cla1Task8 ( void )
{
    // 初始化 Buck 電路狀態變數
    ejBuckInitSetupCLA();

    // 初始化切換週期計數器
    prdCTR = 0;
    // 計算等效電阻
    rC_p_R = parallelAnB(buckSPECS.r_C, buckSPECS.R);
    rC_s_R = seriesAnB(buckSPECS.r_C, buckSPECS.R);
    // 計算時間步長
    dt = 1.0 / buckSPECS.f / sample;
    // 觸發除錯中斷點，通知 CPU 初始化完成
    __mdebugstop();

}

// 計算並聯電阻
float parallelAnB(float a, float b){
    return a*b/(a+b);
}

// 計算串聯電阻
float seriesAnB(float a, float b){
    return a+b;
}

// 初始化 CLA 端的 Buck 電路狀態變數
void ejBuckInitSetupCLA(){

    // 將所有狀態變數初始化為 0
    buckState_i_L_preStep = 0.0;  // A
    buckState_i_L_step = 0.0;     // A
    buckState_i_L_nextStep = 0.0; // A
    buckState_v_C_preStep = 0.0;  // V
    buckState_v_C_step = 0.0;     // V
    buckState_v_C_nextStep = 0.0; // V

    buckOutput_v_L = 0;    // V
    buckOutput_i_C = 0;    // A
    buckOutput_v_o = 0;    // V

}

// 除錯函式，在 DEBUG 模式下觸發中斷點
void debug(){
#ifdef DEBUG
    __mdebugstop();
#endif
}
//
// End of file
//
