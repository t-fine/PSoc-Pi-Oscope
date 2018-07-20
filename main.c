/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
*/
#include "project.h"

/* USB device number. */
#define USBFS_DEVICE  (0)

/* Active endpoints of USB device. */
#define IN_EP_NUM     (1)
#define OUT_EP_NUM    (2)

/* Size of SRAM buffer to store endpoint data. */
#define BUFFER_SIZE   (64)
#define DATASIZE    64

#if (USBFS_16BITS_EP_ACCESS_ENABLE)
    /* To use the 16-bit APIs, the buffer has to be:
    *  1. The buffer size must be multiple of 2 (when endpoint size is odd).
    *     For example: the endpoint size is 63, the buffer size must be 64.
    *  2. The buffer has to be aligned to 2 bytes boundary to not cause exception
    *     while 16-bit access.
    */
    #ifdef CY_ALIGN
        /* Compiler supports alignment attribute: __ARMCC_VERSION and __GNUC__ */
        CY_ALIGN(2) uint8 buffer[BUFFER_SIZE];
    #else
        /* Complier uses pragma for alignment: __ICCARM__ */
        #pragma data_alignment = 2
        uint8 buffer[BUFFER_SIZE];
    #endif /* (CY_ALIGN) */
#else
    /* There are no specific requirements to the buffer size and alignment for 
    * the 8-bit APIs usage.
    */
    uint8 buffer[BUFFER_SIZE];
#endif /* (USBFS_GEN_16BITS_EP_ACCESS) */


/*******************************************************************************
* Function Name: main
********************************************************************************
*
* Summary:
*  The main function performs the following actions:
*   1. Starts the USBFS component.
*   2. Waits until the device is enumerated by the host.
*   3. Enables the OUT endpoint to start communication with the host.
*   4. Waits for OUT data coming from the host and sends it back on a
*      subsequent IN request.
*
* Parameters:
*  None.
*
* Return:
*  None.
*
*******************************************************************************/

/* Defines for DMA_1 */
#define DMA_1_BYTES_PER_BURST 1
#define DMA_1_REQUEST_PER_BURST 1
#define DMA_1_SRC_BASE (CYDEV_PERIPH_BASE)
#define DMA_1_DST_BASE (CYDEV_SRAM_BASE)

/* Defines for DMA_2 */
#define DMA_2_BYTES_PER_BURST 1
#define DMA_2_REQUEST_PER_BURST 1
#define DMA_2_SRC_BASE (CYDEV_PERIPH_BASE)
#define DMA_2_DST_BASE (CYDEV_SRAM_BASE)

/* Variable declarations for DMA_1 */
uint8 DMA_1_Chan;
uint8 DMA_1_TD[2];

/* Variable declarations for DMA_2 */
uint8 DMA_2_Chan;
uint8 DMA_2_TD[2];

//arrays TX will read into 
uint8 adcFirstArray[DATASIZE];
uint8 adcSecondArray[DATASIZE];
int adcArrayWriteFlag = 1;
uint8 potVal[2];
int oldPot1Val;
int pot1Val;
int oldPot2Val;
int pot2Val;


//arrays RX will read into
uint8 wave2FirstArray[DATASIZE];
uint8 wave2SecondArray[DATASIZE];
int wave2ArrayWriteFlag = 1;

CY_ISR_PROTO(dma_1_Int);
CY_ISR_PROTO(dma_2_Int);

CY_ISR(dma_1_Int){
    //if I'm here that means the first 64 bytes are transferred 
    //toggle the array flag, thus reading into the first array first, then the second       
    AMux_Select(0);
    pot1Val = ADC_Read16(); 
        if((pot1Val != oldPot1Val)){
            if((pot1Val < 1))
                pot1Val = 0; 
            if((pot1Val > 254))
                pot1Val = 255;
            potVal[0] = pot1Val;
            USBFS_LoadInEP(3, potVal, 2);
        }
    oldPot1Val = pot1Val;
    
    AMux_Select(1);    
    pot2Val = ADC_Read16(); 
        if((pot2Val != oldPot2Val)){
            if((pot2Val < 1))
                pot2Val = 0; 
            if((pot2Val > 254))
                pot2Val = 255;
            potVal[1] = pot2Val;
            USBFS_LoadInEP(3, potVal, 2);
        }
    oldPot1Val = pot1Val;
        
    adcArrayWriteFlag = 1 - adcArrayWriteFlag;
    if (USBFS_IN_BUFFER_EMPTY == USBFS_GetEPState(IN_EP_NUM)){
        if(adcArrayWriteFlag == 0){ //transfer 64 bytes from adcFirstArray on USB
            //USBFS_LoadInEP(IN_EP_NUM, adcFirstArray, BUFFER_SIZE);
            USBFS_LoadInEP(IN_EP_NUM, adcFirstArray, BUFFER_SIZE);
        }
        else{
            USBFS_LoadInEP(IN_EP_NUM, adcSecondArray, BUFFER_SIZE);
        }
    }
    ISR_DMA1_ClearPending();
}

CY_ISR(dma_2_Int){
    //if I'm here that means the first 64 bytes are transferred 
    //toggle the array flag, thus reading into the first array first, then the second
    
    wave2ArrayWriteFlag = 1 - wave2ArrayWriteFlag;
    if (USBFS_IN_BUFFER_EMPTY == USBFS_GetEPState(2)){
        if(wave2ArrayWriteFlag == 0){ //transfer 64 bytes from adcFirstArray on USB
            //USBFS_LoadInEP(IN_EP_NUM, adcFirstArray, BUFFER_SIZE);
            USBFS_LoadInEP(2, wave2FirstArray, BUFFER_SIZE);
        }
        else{
            USBFS_LoadInEP(2, wave2SecondArray, BUFFER_SIZE);
        }
    }
    ISR_DMA2_ClearPending();
}

int main(void)
{
    CyGlobalIntEnable; /* Enable global interrupts. */
    /* Start USBFS operation with 5V operation. */
    USBFS_Start(USBFS_DEVICE, USBFS_5V_OPERATION);

    /* Wait until device is enumerated by host. */
    while (0u == USBFS_GetConfiguration())
    {
    }
    /* Enable OUT endpoint to receive data from host. */
    //USBFS_EnableOutEP(OUT_EP_NUM);
    
    WaveDAC8_Start();
    WaveDAC8_1_Start();
    Wave_1_ADC_Start();
    Wave_1_ADC_StartConvert();
    Wave_2_ADC_Start();
    Wave_2_ADC_StartConvert();
    
    ADC_Start();
    AMux_Start();
    ISR_DMA1_StartEx(dma_1_Int);
    ISR_DMA2_StartEx(dma_2_Int);
    
    AMux_Select(0);
    oldPot1Val = ADC_Read16();
    AMux_Select(1);
    oldPot2Val = ADC_Read16();
    
    // Configuration for DMA_1 & DMA_2
    DMA_1_Chan = DMA_1_DmaInitialize(DMA_1_BYTES_PER_BURST, DMA_1_REQUEST_PER_BURST, 
        HI16(DMA_1_SRC_BASE), HI16(DMA_1_DST_BASE));
    DMA_2_Chan = DMA_2_DmaInitialize(DMA_2_BYTES_PER_BURST, DMA_2_REQUEST_PER_BURST, 
        HI16(DMA_2_SRC_BASE), HI16(DMA_2_DST_BASE));
    
    DMA_1_TD[0] = CyDmaTdAllocate();
    DMA_1_TD[1] = CyDmaTdAllocate();
    DMA_2_TD[0] = CyDmaTdAllocate();
    DMA_2_TD[1] = CyDmaTdAllocate();
    
    CyDmaTdSetConfiguration(DMA_1_TD[0], DATASIZE, DMA_1_TD[1], CY_DMA_TD_INC_DST_ADR | DMA_1__TD_TERMOUT_EN);
    CyDmaTdSetConfiguration(DMA_1_TD[1], DATASIZE, DMA_1_TD[0], CY_DMA_TD_INC_DST_ADR | DMA_1__TD_TERMOUT_EN);
    CyDmaTdSetConfiguration(DMA_2_TD[0], DATASIZE, DMA_2_TD[1], CY_DMA_TD_INC_DST_ADR | DMA_2__TD_TERMOUT_EN);
    CyDmaTdSetConfiguration(DMA_2_TD[1], DATASIZE, DMA_2_TD[0], CY_DMA_TD_INC_DST_ADR | DMA_2__TD_TERMOUT_EN);
    
    CyDmaTdSetAddress(DMA_1_TD[0], LO16((uint32)Wave_1_ADC_SAR_WRK0_PTR), LO16((uint32)adcFirstArray));
    CyDmaTdSetAddress(DMA_1_TD[1], LO16((uint32)Wave_1_ADC_SAR_WRK0_PTR), LO16((uint32)adcSecondArray));
    CyDmaTdSetAddress(DMA_2_TD[0], LO16((uint32)Wave_2_ADC_SAR_WRK0_PTR), LO16((uint32)wave2FirstArray));
    CyDmaTdSetAddress(DMA_2_TD[1], LO16((uint32)Wave_2_ADC_SAR_WRK0_PTR), LO16((uint32)wave2SecondArray));
    
    CyDmaChSetInitialTd(DMA_1_Chan, DMA_1_TD[0]);
    CyDmaChSetInitialTd(DMA_2_Chan, DMA_2_TD[0]);
    
    CyDmaChEnable(DMA_1_Chan, 1);
    CyDmaChEnable(DMA_2_Chan, 1);
    
    
    /* Place your initialization/startup code here (e.g. MyInst_Start()) */

    for(;;)
    {
        /* Check if configuration is changed. */
        if (0u != USBFS_IsConfigurationChanged())
        {
            /* Re-enable endpoint when device is configured. */
            if (0u != USBFS_GetConfiguration())
            {
                /* Enable OUT endpoint to receive data from host. */
                USBFS_EnableOutEP(OUT_EP_NUM);
            }
        }

    }
}

/* [] END OF FILE */
