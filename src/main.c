/* Includes ------------------------------------------------------------------*/
#include <stdlib.h>
#include <string.h> 
#include <math.h> 
#include "stm32f4xx.h" 
#include "leds.h" 
#include "main.h" 

uint16_t dacDMAData[DAC_DMA_BUF_SIZE * 2];
uint16_t *curDMAData; /* changed by DMA interrupt */

void Delay(__IO uint32_t nCount);
void DAC_Setup(void);

void phasewrap(float *phase)
{
    while (*phase > 1.) { *phase -= 1.; }
    while (*phase < 0.) { *phase += 1.; }
}

int main(void)
{
  /*!< At this stage the microcontroller clock setting is already configured, 
       this is done through SystemInit() function which is called from startup
       file (startup_stm32f4xx.s) before to branch to application main.
       To reconfigure the default setting of SystemInit() function, refer to
        system_stm32f4xx.c file
     */

    /* Enable LEDs so we can toggle them */
    LEDs_Init();

    /* zero output buffer */
    memset(dacDMAData,0,sizeof(uint16_t) * DAC_DMA_BUF_SIZE * 2);

    curDMAData = NULL;
    
    /* Enable DAC */
    DAC_Setup();

    float phase = 0;
    float slowphase = 0;
    float freq = 440.;
    float slowfreq = 0.1;

    while (1) {
        while (curDMAData == NULL);/* wait for request to fill with data */
        size_t numIters = DAC_DMA_BUF_SIZE;
        while (numIters--) {
            *curDMAData = (uint16_t)(0xfff 
                    * ((1. + sinf(phase * M_PI * 2.)) * 0.5)
                    * ((1. + sinf(slowphase * M_PI * 2.)) * 0.5));
            phase += freq / DAC_SAMPLE_RATE;
            slowphase += slowfreq / DAC_SAMPLE_RATE;
            phasewrap(&phase);
            phasewrap(&slowphase);
            curDMAData++;
        }
        curDMAData = NULL;
    }
}

/**
  * @brief  Delay Function.
  * @param  nCount:specifies the Delay time length.
  * @retval None
  */
void Delay(__IO uint32_t nCount)
{
  while(nCount--)
  {
  }
}

void DAC_Setup(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    DAC_InitTypeDef  DAC_InitStruct;
    DMA_InitTypeDef  DMA_InitStruct;
    TIM_TimeBaseInitTypeDef TIM_TimerInitStruct;

    /* Enable Clocks */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    /* Set up GPIO for DAC (alternate function) */
    GPIO_StructInit(&GPIO_InitStruct);
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AN;
    GPIO_InitStruct.GPIO_Pin  = GPIO_Pin_4;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* Set up timer 2 (to trigger DAC conversion) */
    TIM_TimeBaseStructInit(&TIM_TimerInitStruct);
    TIM_TimerInitStruct.TIM_Prescaler = 1;
    TIM_TimerInitStruct.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimerInitStruct.TIM_Period = 1000; /* Tick 45000 times/sec (weird sampling rate) */
    TIM_TimerInitStruct.TIM_ClockDivision = 0;
    TIM_TimeBaseInit(TIM2, &TIM_TimerInitStruct);
    TIM_SelectOutputTrigger(TIM2, TIM_TRGOSource_Update);
    TIM_Cmd(TIM2, ENABLE);

    /* Setup DAC */
    DAC_StructInit(&DAC_InitStruct);
    DAC_InitStruct.DAC_Trigger = DAC_Trigger_T2_TRGO; /* Timer 2 triggers conversion */
    DAC_Init(DAC_Channel_1, &DAC_InitStruct);
    DAC_SoftwareTriggerCmd(DAC_Channel_1, ENABLE);
    
    /* Setup DMA */
    DMA_DeInit(DMA1_Stream5);
    DMA_ITConfig(DMA1_Stream5, DMA_IT_HT | DMA_IT_TC, ENABLE); /* Trigger on half complete and fully complete */
    NVIC_EnableIRQ(DMA1_Stream5_IRQn);
    DMA_StructInit(&DMA_InitStruct);
    DMA_InitStruct.DMA_Channel = DMA_Channel_7; 
    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)(&(DAC->DHR12R1));
    DMA_InitStruct.DMA_Memory0BaseAddr = (uint32_t)dacDMAData;
    DMA_InitStruct.DMA_DIR = DMA_DIR_MemoryToPeripheral;
    DMA_InitStruct.DMA_BufferSize = DAC_DMA_BUF_SIZE * 2;
    DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStruct.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStruct.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStruct.DMA_Priority = DMA_Priority_High;
    DMA_InitStruct.DMA_FIFOMode = DMA_FIFOMode_Disable;
    /* DMA_FIFOThreshold ... N/A */
    DMA_InitStruct.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    DMA_InitStruct.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_Init(DMA1_Stream5, &DMA_InitStruct);
    
    /* Enable DMA stream */
    DMA_Cmd(DMA1_Stream5, ENABLE);

    /* Enable DAC */
    DAC_Cmd(DAC_Channel_1, ENABLE);

    /* Enable DMA for DAC */
    DAC_DMACmd(DAC_Channel_1, ENABLE);

    /* wait for DMA to be enabled */
    while (DMA_GetCmdStatus(DMA1_Stream5) != ENABLE);

    LEDs_redSet(); /* DMA Enabled */
}
