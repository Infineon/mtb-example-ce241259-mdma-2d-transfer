/*******************************************************************************
 * File Name:   main.c
 *
 * Description: This is the source code for the M-DMA 2D Transfer
 *              for ModusToolbox.
 *
 * Related Document: See README.md
 *
 *
********************************************************************************
 * (c) 2025-2026, Infineon Technologies AG, or an affiliate of Infineon
 * Technologies AG. All rights reserved.
 * This software, associated documentation and materials ("Software") is
 * owned by Infineon Technologies AG or one of its affiliates ("Infineon")
 * and is protected by and subject to worldwide patent protection, worldwide
 * copyright laws, and international treaty provisions. Therefore, you may use
 * this Software only as provided in the license agreement accompanying the
 * software package from which you obtained this Software. If no license
 * agreement applies, then any use, reproduction, modification, translation, or
 * compilation of this Software is prohibited without the express written
 * permission of Infineon.
 *
 * Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
 * IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
 * THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
 * SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
 * Infineon reserves the right to make changes to the Software without notice.
 * You are responsible for properly designing, programming, and testing the
 * functionality and safety of your intended application of the Software, as
 * well as complying with any legal requirements related to its use. Infineon
 * does not guarantee that the Software will be free from intrusion, data theft
 * or loss, or other breaches ("Security Breaches"), and Infineon shall have
 * no liability arising out of any Security Breaches. Unless otherwise
 * explicitly approved by Infineon, the Software may not be used in any
 * application where a failure of the Product or any consequences of the use
 * thereof can reasonably be expected to result in personal injury.
********************************************************************************/

#include "cybsp.h"
#include "cy_pdl.h"
#include "cy_retarget_io.h"
#include "mtb_hal.h"
#include <inttypes.h>

/*******************************************************************************
* Macros
********************************************************************************/
#define BUFFER_SIZE  (16ul)
#define DMAC_SW_TRIG (TRIG_OUT_MUX_3_MDMA_TR_IN0)
#define DMAC_INTR (MDMA_IRQ)
#define GPIO_INTERRUPT_PRIORITY (7u)
#define DELAY_MS (1)

/*******************************************************************************
* Global variables
********************************************************************************/
/* A flag when DMA transfer is completed, then it will change to true */
static bool g_isComplete;
/* A flag when button interrupt is occurred, then it will change to true */
static bool g_isInterrupt;

/* For the Retarget -IO (Debug UART) usage */
static cy_stc_scb_uart_context_t    UART_context;           /** UART context */
static mtb_hal_uart_t               UART_hal_obj;           /** Debug UART HAL object  */
/*******************************************************************************
* Private Variables/Constants
*********************************************************************************/
/* DMAC Interrupt configuration structure */
const cy_stc_sysint_t IRQ_CFG =
{
    .intrSrc = ((NvicMux4_IRQn << CY_SYSINT_INTRSRC_MUXIRQ_SHIFT) | DMAC_INTR),
    .intrPriority = 0UL
};

const cy_stc_sysint_t BTN_IRQ_CFG =
{
    .intrSrc = ((NvicMux3_IRQn << CY_SYSINT_INTRSRC_MUXIRQ_SHIFT) | CYBSP_USER_BTN_IRQ),
    .intrPriority = GPIO_INTERRUPT_PRIORITY
};

/* Data to be transferred */
const static  uint32_t    srcBuffer[BUFFER_SIZE] =
{0x10000000,0x20000001,0x30000002,0x40000003,
 0x10000004,0x20000005,0x30000006,0x40000007,
 0x10000008,0x20000009,0x3000000A,0x4000000B,
 0x1000000C,0x2000000D,0x3000000E,0x4000000F,
 };

/*******************************************************************************
* Function Prototypes
*********************************************************************************/
/* M-DMA Handler */
void HandleDMACIntr(void);

/* GPIO Handler */
void HandleGPIOIntr(void);

/*******************************************************************************
* Function Implementations
********************************************************************************/

/**********************************************************************************************************************
 * Function Name: HandleDMACIntr*
 * Summary:
 *  DMA interrupt handler
 * Parameters:
 *  None
 * Return:
 *  None
 **********************************************************************************************************************/
void HandleDMACIntr(void)
{
    uint32_t masked;

    masked = Cy_DMAC_Channel_GetInterruptStatusMasked(MDMA_HW, MDMA_CHANNEL);
    if ((masked & CY_DMAC_INTR_COMPLETION) != 0UL)
    {
        /* Clear Complete DMA interrupt flag */
        Cy_DMAC_Channel_ClearInterrupt(MDMA_HW, MDMA_CHANNEL,CY_DMAC_INTR_COMPLETION);

        /* Mark the transmission as complete */
        g_isComplete = true;
    }
    else
    {
        CY_ASSERT(false);
    }
}

/**********************************************************************************************************************
 * Function Name: HandleGPIOIntr
 * Summary:
 *   GPIO interrupt handler.
 * Parameters: 
 *   none  
 **********************************************************************************************************************/
void HandleGPIOIntr(void)
{
    Cy_GPIO_ClearInterrupt(CYBSP_USER_BTN_PORT, CYBSP_USER_BTN_NUM);
   
    g_isInterrupt = true;
}

/**********************************************************************************************************************
 * Function Name: main
 * Summary:
 *  This is the main function for CPU. It...
 *    1.Transmit data from memory array to memory array via M-DMA
 *    2.Using button interrupt to trigger the M-DMA transfer
 *    3.The result will be like below
 *       srcBuffer: 0x10000000,0x20000001,0x30000002,0x40000003,......
 *       dstBuffer  : 0,4,8,C,1,5......
 * Parameters:
 *  none
 * Return:
 *  int
 **********************************************************************************************************************/
int main(void)
{
    uint32_t dstBuffer[BUFFER_SIZE];
    cy_rslt_t result;

    /* Initialize the device and board peripherals */
    result = cybsp_init();
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();

    SCB_DisableICache();
    SCB_DisableDCache();

    /* Debug UART init */
    result = (cy_rslt_t)Cy_SCB_UART_Init(UART_HW, &UART_config, &UART_context);
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    Cy_SCB_UART_Enable(UART_HW);

    /* Setup the HAL UART */
    result = mtb_hal_uart_setup(&UART_hal_obj, &UART_hal_config, &UART_context, NULL);
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    result = cy_retarget_io_init(&UART_hal_obj);
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Configure GPIO interrupt */
    result = Cy_SysInt_Init(&BTN_IRQ_CFG, &HandleGPIOIntr);
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }
    NVIC_ClearPendingIRQ((IRQn_Type)BTN_IRQ_CFG.intrSrc);
    NVIC_EnableIRQ((IRQn_Type) NvicMux3_IRQn);

    /* Disable M-DMA */
    Cy_DMAC_Disable(MDMA_HW);
    Cy_DMAC_Channel_DeInit(MDMA_HW, MDMA_CHANNEL);
 
    /* Set the Source and Destination address */
    MDMA_Descriptor_0_config.srcAddress = (void *)srcBuffer;
    MDMA_Descriptor_0_config.dstAddress = (void *)dstBuffer;

    /* Initialize the M-DMA descriptor */
    result = Cy_DMAC_Descriptor_Init(&MDMA_Descriptor_0, &MDMA_Descriptor_0_config);
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initialize the M-DMA channel */
    result = Cy_DMAC_Channel_Init(MDMA_HW, MDMA_CHANNEL, &MDMA_channelConfig);
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }
    Cy_DMAC_Channel_SetPriority(MDMA_HW, MDMA_CHANNEL, 0UL);
    Cy_DMAC_Channel_SetInterruptMask(MDMA_HW, MDMA_CHANNEL, CY_DMAC_INTR_COMPLETION);

    /* Enable the M-DMA */
    Cy_DMAC_Enable(MDMA_HW);

    /* Interrupt Initialization */
    result = Cy_SysInt_Init(&IRQ_CFG, HandleDMACIntr);
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }
    /* Enable the Interrupt */
    NVIC_EnableIRQ(NvicMux4_IRQn);

    printf("\x1b[2J\x1b[;H");
    printf("************************************************************\r\n");
    printf("M-DMA 2D Transfer code example\r\n");
    printf("************************************************************\r\n\n");
    printf(">> push USER_BTN1 for M-DMA 2D transfer \r\n\n");

    for (;;)
    {
        /* Clear the interrupt flag */
        g_isInterrupt = false;

        /* Wait for interrupt */
        while (g_isInterrupt == false)
        {
            Cy_SysLib_Delay(DELAY_MS);
        }

        /* Clear destination memory */
        memset(dstBuffer, 0, sizeof(dstBuffer));

        /* Set up the channel */
        Cy_DMAC_Channel_SetDescriptor(MDMA_HW, MDMA_CHANNEL, &MDMA_Descriptor_0);
        Cy_DMAC_Channel_Enable(MDMA_HW, MDMA_CHANNEL);

        /* Clear the transfer completion flag */
        g_isComplete = false;

        /* SW Trigger start */
        if (Cy_TrigMux_SwTrigger(DMAC_SW_TRIG, CY_TRIGGER_TWO_CYCLES) != CY_TRIGMUX_SUCCESS)
        {
            CY_ASSERT(0);
        }

        /* wait for DMA completion */
        while (g_isComplete == false)
        {
            Cy_SysLib_Delay(DELAY_MS);
        }
        printf("**************** Source(CODE FLASH): ****************\r\n");

        for (int idx = 0UL; idx < BUFFER_SIZE; idx++)
        {
            printf("0x%" PRIX32 " ", srcBuffer[idx]);
            if (idx % 4 == 3)
            {
                printf("\r\n");
            }
        }

        printf("**************** Destination(SRAM): ****************\r\n");

        for (int idx = 0UL; idx < BUFFER_SIZE; idx++)
        {
            printf("0x%" PRIX32 " ", dstBuffer[idx]);
            if (idx % 4 == 3)
            {
                printf("\r\n");
            }
        }
        printf("\r\nM-DMA 2D transfer done!\r\n\n");
    }
}

/* [] END OF FILE */
