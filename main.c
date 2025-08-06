//
// Included Files
//
#include "F28x_Project.h"
#include "shared.h"

//
// Defines
//

//#define ADCVIN              // 啟用 ADC 輸入作為 Vin
//#define ECAPDUTY            // 啟用 eCAP 計算工作週期
//#define _FLASH              // 在 Flash 模式下運行
#define WAITSTEP     asm(" RPT #255 || NOP") // 等待步驟的內嵌組合語言指令
#define FREQ         500      // kHz, CLA 模擬取樣率與 ADC 觸發頻率

// DAC 相關定義
#define REFERENCE_VDAC      0 // 使用 VDAC 作為參考電壓
#define REFERENCE_VREF      1 // 使用 VREF 作為參考電壓
#define DACA         1        // DAC A (J3 pin30)
#define DACB         2        // DAC B (J7 pin70)
#define DACC         3        // DAC C (LaunchPad 上找不到)
#define REFERENCE           REFERENCE_VREF  // 選擇 VREF 作為 DAC 參考電壓

//
// Globals
//
// ePWM 工作週期
float EPWMDuty;

// 條件變更
float loadChange = 5; // ohm, 負載變更值
float vinChange = 24; // V, 輸入電壓變更值

// DAC 模組暫存器指標陣列
volatile struct DAC_REGS* DAC_PTR[4] = {0x0,&DacaRegs,&DacbRegs,&DaccRegs};

//
// 與 CLA 分享的變數
//
#pragma DATA_SECTION(buckSPECS,"CpuToCla1MsgRAM")
ejBuckSPECS buckSPECS; // Buck 電路規格
#pragma DATA_SECTION(buckInput,"CpuToCla1MsgRAM")
ejBuckInput buckInput; // Buck 電路輸入
#pragma DATA_SECTION(DAC_V_O,"Cla1ToCpuMsgRAM")
float DAC_V_O; // 來自 CLA 的 DAC 輸出電壓
#pragma DATA_SECTION(DAC_I_L,"Cla1ToCpuMsgRAM")
float DAC_I_L; // 來自 CLA 的 DAC 電感電流


//
// Function Prototypes
//
void ConfigureADC(void); // 設定 ADC
void ConfigureEPWM(void); // 設定 ePWM
void SetupADCEpwm(void); // 設定 ADC 由 ePWM 觸發
interrupt void adca1_isr(void); // ADCA 中斷服務常式

void ejBuckInitSetupCPU(ejBuckSPECS*, ejBuckInput*); // 初始化 CPU 端的 Buck 電路參數

int getSampleFreq(int); // 取得取樣頻率

void CLA_configClaMemory(void); // 設定 CLA 記憶體
void CLA_initCpu1Cla1(void); // 初始化 CLA

__interrupt void ecap1_isr(void); // eCAP1 中斷服務常式
void InitECapture(void); // 初始化 eCAP

void configureDAC(Uint16 dac_num); // 設定 DAC

void InitEPwm2Example(); // 初始化 ePWM2

//
// Main
//
void main(void)
{
    // 初始化系統控制與 GPIO
    InitSysCtrl();

    // 初始化 GPIO
    InitGpio();
    // 初始化 eCAP1 的 GPIO (GPIO19)
    InitECap1Gpio(19);
    // 設定 GPIO19 為非同步輸入
    GPIO_SetupPinOptions(19, GPIO_INPUT, GPIO_ASYNC);
    EALLOW;
    // 停用 GPIO2 (EPWM2A) 的上拉電阻
    GpioCtrlRegs.GPAPUD.bit.GPIO2 = 1;
    // 設定 GPIO2 為 EPWM2A 功能
    GpioCtrlRegs.GPAMUX1.bit.GPIO2 = 1;
    EDIS;

    // 停用 CPU 中斷
    DINT;

    // 初始化 PIE 控制暫存器
    InitPieCtrl();

    // 停用 CPU 中斷並清除所有中斷旗標
    IER = 0x0000;
    IFR = 0x0000;

    // 初始化 PIE 中斷向量表
    InitPieVectTable();

    // 重新映射中斷向量
    EALLOW;
    PieVectTable.ADCA1_INT = &adca1_isr; // ADCA 中斷 1
    PieVectTable.ECAP1_INT = &ecap1_isr; // eCAP1 中斷
    EDIS;

    // 設定 eCAP 模組
    InitECapture();

    // 設定並啟動 ADC
    ConfigureADC();

    // 設定 ePWM
    ConfigureEPWM();

    // 設定 ADC 由 ePWM 觸發
    SetupADCEpwm();

    // 初始化 ePWM2
    InitEPwm2Example();

    // 設定 DAC
    configureDAC(DACA); // DACA
    configureDAC(DACB); // DACB

    // 設定 CLA 記憶體空間與任務向量
    CLA_configClaMemory();
    CLA_initCpu1Cla1();

    // 啟用全域中斷與高優先性即時除錯事件
    IER |= M_INT1; // 啟用第 1 組中斷
    IER |= M_INT4; // 啟用第 4 組中斷

    // 啟用 PIE 中斷
    PieCtrlRegs.PIEIER1.bit.INTx1 = 1;
    PieCtrlRegs.PIEIER4.bit.INTx1 = 1;

    EINT;  // 啟用全域中斷 INTM
    ERTM;  // 啟用全域即時中斷 DBGM

    // 在 CPU 端初始化 Buck 電路規格與輸入
    ejBuckInitSetupCPU(&buckSPECS, &buckInput);
    // 強制啟動 CLA 任務 8 並等待其完成，以初始化 CLA 端的狀態
    Cla1ForceTask8andWait();

    // 解凍 EPWM1 計數器並啟用 ADC 觸發
    EALLOW;
    EPwm1Regs.ETSEL.bit.SOCAEN = 1;  // 啟用 SOCA
    EPwm1Regs.TBCTL.bit.CTRMODE = 0; // 解凍並進入向上計數模式
    EDIS;

    // 設定 ePWM 工作週期
    EPWMDuty = 0.2083;

    // 進入無窮迴圈
    while(1);

}

// 設定 ADC
void ConfigureADC(void)
{
    EALLOW;

    // 寫入設定
    AdcaRegs.ADCCTL2.bit.PRESCALE = 6; // 設定 ADCCLK 除頻器為 /4
    AdcSetMode(ADC_ADCA, ADC_RESOLUTION_12BIT, ADC_SIGNALMODE_SINGLE);

    // 設定脈衝位置為延後
    AdcaRegs.ADCCTL1.bit.INTPULSEPOS = 1;

    // 啟動 ADC
    AdcaRegs.ADCCTL1.bit.ADCPWDNZ = 1;

    // 延遲 1ms 等待 ADC 啟動完成
    DELAY_US(1000);

    EDIS;
}

// 設定 EPWM SOC 與比較值
void ConfigureEPWM(void)
{
    EALLOW;
    // 假設 ePWM 時脈已啟用
    EPwm1Regs.ETSEL.bit.SOCAEN    = 0;                   // 停用 A 組的 SOC
    EPwm1Regs.ETSEL.bit.SOCASEL    = 4;                  // 在向上計數時選擇 SOC
    EPwm1Regs.ETPS.bit.SOCAPRD = 1;                      // 在第一個事件產生脈衝
    EPwm1Regs.CMPA.bit.CMPA = getSampleFreq(FREQ)/2;     // 設定比較 A 值
    EPwm1Regs.TBPRD = getSampleFreq(FREQ);               // 設定計數器週期 (kHz)
    EPwm1Regs.TBCTL.bit.CTRMODE = 3;                     // 凍結計數器
    EDIS;
}

// 設定 ADC ePWM 取樣視窗
void SetupADCEpwm()
{
    Uint16 acqps;

    // 根據解析度決定最小取樣視窗 (in SYSCLKS)
    if(ADC_RESOLUTION_12BIT == AdcaRegs.ADCCTL2.bit.RESOLUTION)
    {
        acqps = 14; // 75ns
    }
    else // 16-bit 解析度
    {
        acqps = 63; // 320ns
    }

    // 選擇轉換通道與轉換結束旗標
    EALLOW;
    AdcaRegs.ADCSOC0CTL.bit.CHSEL = 2;     // SOC0 將轉換 A2 腳位
    AdcaRegs.ADCSOC0CTL.bit.ACQPS = acqps; // 取樣視窗為 100 SYSCLK 週期
    AdcaRegs.ADCSOC0CTL.bit.TRIGSEL = 5;   // 由 ePWM1 SOCA/C 觸發
    AdcaRegs.ADCINTSEL1N2.bit.INT1SEL = 0; // SOC0 結束時設定 INT1 旗標
    AdcaRegs.ADCINTSEL1N2.bit.INT1E = 1;   // 啟用 INT1 旗標
    AdcaRegs.ADCINTFLGCLR.bit.ADCINT1 = 1; // 確保 INT1 旗標已清除
    EDIS;
}

// ADCA 中斷服務常式 - 在 ISR 中讀取 ADC 緩衝區
interrupt void adca1_isr(void)
{

#ifdef ECAPDUTY
    // 使用 eCAP 計算的工作週期來設定 EPWM2 的比較值
    EALLOW;
    EPwm2Regs.CMPA.bit.CMPA = 6000*EPWMDuty;
    EDIS;
#endif

#ifdef ADCVIN
    // 使用 ADC 值來控制 Buck 模型的輸入電壓
    buckInput.v_i = AdcaResultRegs.ADCRESULT0 / 4095 * 10;
#endif

#ifndef ADCVIN
    // 使用固定的輸入電壓
    buckInput.v_i = vinChange;
#endif

    // 設定 Buck 模型的負載電阻
    buckSPECS.R = loadChange;

    // 執行 Buck 模型計算 (在 CLA 中)
    Cla1ForceTask1andWait();

    // 更新 DAC 輸出值
    EALLOW;
    DacaRegs.DACVALS.all = DAC_V_O / 25.0 * 4095.0;
    DacbRegs.DACVALS.all = DAC_I_L / 8.0 * 4095.0;
    EDIS;

    // 清除 INT1 旗標
    AdcaRegs.ADCINTFLGCLR.bit.ADCINT1 = 1;
    // 回應 PIE 中斷
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
}

// 初始化 CPU 端的 Buck 電路參數
void ejBuckInitSetupCPU(ejBuckSPECS* buckSPECS, ejBuckInput* buckInput){

    // 設定電路參數
    buckSPECS->L = 100e-6;  // H, 電感值
    buckSPECS->C = 100e-6;  // F, 電容值
    buckSPECS->r_L = 10e-3; // ohm, 電感等效串聯電阻 (ESR)
    buckSPECS->r_C = 1e-3;  // ohm, 電容等效串聯電阻 (ESR)
    buckSPECS->R = 5;       // ohm, 負載電阻
    buckSPECS->f = 100e3;   // Hz, 切換頻率

    buckInput->v_i = 24;    // V, 輸入電壓
    buckInput->duty = 0.208;  // 工作週期 (Duty Cycle)

}

// 計算取樣頻率對應的計數器值
int getSampleFreq(int freq){
    // 適用於 200MHz CPU, ePWM 預設除頻, 向上計數模式
    return 50000.0 / freq - 1;
}

// 設定 CLA 記憶體區段
void CLA_configClaMemory(void)
{
    extern uint32_t Cla1funcsRunStart, Cla1funcsLoadStart, Cla1funcsLoadSize;
    EALLOW;

#ifdef _FLASH
    // 從 FLASH 複製程式碼到 RAM
    memcpy((uint32_t *)&Cla1funcsRunStart, (uint32_t *)&Cla1funcsLoadStart,
           (uint32_t)&Cla1funcsLoadSize);
#endif //_FLASH

    // 初始化並等待 CLA1ToCPUMsgRAM
    MemCfgRegs.MSGxINIT.bit.INIT_CLA1TOCPU = 1;
    while(MemCfgRegs.MSGxINITDONE.bit.INITDONE_CLA1TOCPU != 1){};

    // 初始化並等待 CPUToCLA1MsgRAM
    MemCfgRegs.MSGxINIT.bit.INIT_CPUTOCLA1 = 1;
    while(MemCfgRegs.MSGxINITDONE.bit.INITDONE_CPUTOCLA1 != 1){};

    // 選擇 LS4RAM 與 LS5RAM 作為 CLA 的程式空間
    MemCfgRegs.LSxMSEL.bit.MSEL_LS4 = 1;
    MemCfgRegs.LSxCLAPGM.bit.CLAPGM_LS4 = 1;
    MemCfgRegs.LSxMSEL.bit.MSEL_LS5 = 1;
    MemCfgRegs.LSxCLAPGM.bit.CLAPGM_LS5 = 1;

    // 選擇 LS0RAM 與 LS1RAM 作為 CLA 的資料空間
    MemCfgRegs.LSxMSEL.bit.MSEL_LS0 = 1;
    MemCfgRegs.LSxCLAPGM.bit.CLAPGM_LS0 = 0;

    MemCfgRegs.LSxMSEL.bit.MSEL_LS1 = 1;
    MemCfgRegs.LSxCLAPGM.bit.CLAPGM_LS1 = 0;

    EDIS;
}

// 初始化 CLA1 任務向量與任務結束中斷
void CLA_initCpu1Cla1(void)
{
    // 計算所有 CLA 任務向量
    EALLOW;
    Cla1Regs.MVECT1 = (uint16_t)(&Cla1Task1);
    Cla1Regs.MVECT8 = (uint16_t)(&Cla1Task8);

    // 啟用 IACK 指令以在軟體中啟動 CLA 任務
    Cla1Regs.MCTL.bit.IACKE = 1;
    // 全域啟用所有 8 個 CLA 任務
    Cla1Regs.MIER.all = 0x00FF;

    EDIS;

}

// 初始化 eCAP1 設定
void InitECapture()
{
    EALLOW;
    ECap1Regs.ECEINT.all = 0x0000;          // 停用所有擷取中斷
    ECap1Regs.ECCLR.all = 0xFFFF;           // 清除所有 CAP 中斷旗標
    ECap1Regs.ECCTL1.bit.CAPLDEN = 0;       // 停用 CAP1-CAP4 暫存器載入
    ECap1Regs.ECCTL2.bit.TSCTRSTOP = 0;     // 確保計數器已停止

    // 設定週邊暫存器
    ECap1Regs.ECCTL2.bit.CONT_ONESHT = 1;   // 單次模式 (One-shot)
    ECap1Regs.ECCTL2.bit.STOP_WRAP = 2;     // 在第 3 個事件停止
    ECap1Regs.ECCTL1.bit.CAP1POL = 0;       // 上升緣 (Rising edge)
    ECap1Regs.ECCTL1.bit.CAP2POL = 1;       // 下降緣 (Falling edge)
    ECap1Regs.ECCTL1.bit.CAP3POL = 0;       // 上升緣 (Rising edge)
    ECap1Regs.ECCTL1.bit.CAP4POL = 1;       // 下降緣 (Falling edge)

    ECap1Regs.ECCTL1.bit.CTRRST1 = 1;       // 差異運算
    ECap1Regs.ECCTL1.bit.CTRRST2 = 1;       // 差異運算
    ECap1Regs.ECCTL1.bit.CTRRST3 = 1;       // 差異運算
    ECap1Regs.ECCTL1.bit.CTRRST4 = 1;       // 差異運算

    ECap1Regs.ECCTL2.bit.SYNCI_EN = 1;      // 啟用同步輸入
    ECap1Regs.ECCTL2.bit.SYNCO_SEL = 0;     // 通透模式 (Pass through)
    ECap1Regs.ECCTL1.bit.CAPLDEN = 1;       // 啟用擷取單元

    ECap1Regs.ECCTL2.bit.TSCTRSTOP = 1;     // 啟動計數器
    ECap1Regs.ECCTL2.bit.REARM = 1;         // 準備單次擷取
    ECap1Regs.ECCTL1.bit.CAPLDEN = 1;       // 啟用 CAP1-CAP4 暫存器載入
    ECap1Regs.ECEINT.bit.CEVT3 = 1;         // 第 3 個事件產生中斷
    EDIS;
}

// eCAP1 中斷服務常式
__interrupt void ecap1_isr(void)
{
#ifdef ECAPDUTY
    // 計算工作週期
    buckInput.duty = (float) ECap1Regs.CAP2 / ((float) ECap1Regs.CAP2 + (float) ECap1Regs.CAP3);
#endif

    // 清除中斷旗標
    ECap1Regs.ECCLR.bit.CEVT3 = 1;
    ECap1Regs.ECCLR.bit.INT = 1;
    // 重新準備單次擷取
    ECap1Regs.ECCTL2.bit.REARM = 1;

    // 回應 PIE 中斷
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP4;
}

// 設定指定的 DAC 輸出
void configureDAC(Uint16 dac_num)
{
    EALLOW;
    // 設定 DAC 參考電壓
    DAC_PTR[dac_num]->DACCTL.bit.DACREFSEL = REFERENCE;
    // 啟用 DAC 輸出
    DAC_PTR[dac_num]->DACOUTEN.bit.DACOUTEN = 1;
    // 設定 DAC 輸出值為 0
    DAC_PTR[dac_num]->DACVALS.all = 0;
    // 延遲 10us 等待緩衝 DAC 啟動
    DELAY_US(10);
    EDIS;
}

// 初始化 ePWM2 設定
void InitEPwm2Example()
{
    EALLOW;
    EPwm2Regs.TBPRD = 6000;                       // 設定計時器週期
    EPwm2Regs.TBPHS.bit.TBPHS = 0x0000;           // 相位為 0
    EPwm2Regs.TBCTR = 0x0000;                     // 清除計數器

    // 設定 TBCLK
    EPwm2Regs.TBCTL.bit.CTRMODE = TB_COUNT_UPDOWN; // 向上向下計數模式
    EPwm2Regs.TBCTL.bit.PHSEN = TB_DISABLE;        // 停用相位載入
    EPwm2Regs.TBCTL.bit.HSPCLKDIV = TB_DIV1;       // 高速時脈除頻器
    EPwm2Regs.TBCTL.bit.CLKDIV = TB_DIV1;          // 時脈除頻器

    // 設定比較值
    EPwm2Regs.CMPA.bit.CMPA = 6000*EPWMDuty;

    // 設定動作
    EPwm2Regs.AQCTLA.bit.CAU = AQ_CLEAR;             // 在 CAU 時清除 PWM2A
    EPwm2Regs.AQCTLA.bit.CAD = AQ_SET;           // 在 CAD 時設定 PWM2A
    EDIS;
}
//
// End of file
//
