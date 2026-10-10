#include "uart_dma.h"
#include "stm32_regs.h"

/* Shared RX/TX buffer in DMA-accessible SRAM. */
static uint8_t echo_buffer[UART_DMA_BLOCK_SIZE];

/* Diagnostic counters and saved error flags. */
volatile uint32_t uart_dma_rx_blocks = 0U;
volatile uint32_t uart_dma_tx_blocks = 0U;
volatile uint32_t uart_dma_errors = 0U;
volatile uint32_t uart_dma_last_dma_flags = 0U;

/* DMA stream configuration bits */
#define DMA_CHANNEL_4      (4UL << 25)
#define DMA_PRIORITY_HIGH  (2UL << 16)
#define DMA_MEMORY_INC     (1UL << 10)
#define DMA_MEMORY_TO_PER  (1UL << 6)
#define DMA_TC_INTERRUPT   (1UL << 4)
#define DMA_TE_INTERRUPT   (1UL << 2)
#define DMA_DME_INTERRUPT  (1UL << 1)
#define DMA_ENABLE         (1UL << 0)

/* Stream 1 flags: receive */
#define RX_ALL_FLAGS       0x00000F40UL
#define RX_ERROR_FLAGS     0x00000300UL
#define RX_COMPLETE_FLAG   (1UL << 11)

/* Stream 3 flags: transmit */
#define TX_ALL_FLAGS       0x0F400000UL
#define TX_ERROR_FLAGS     0x03000000UL
#define TX_COMPLETE_FLAG   (1UL << 27)

static void start_receive(void)
{
    /* Receive Stream 1 must already be disabled. */
    DMA1_LIFCR = RX_ALL_FLAGS;
    DMA1_S1NDTR = UART_DMA_BLOCK_SIZE;

    __asm volatile ("dmb" ::: "memory");
    DMA1_S1CR |= DMA_ENABLE;
}

static void start_transmit(void)
{
    /* Transmit Stream 3 must already be disabled. */
    DMA1_LIFCR = TX_ALL_FLAGS;
    DMA1_S3NDTR = UART_DMA_BLOCK_SIZE;

    __asm volatile ("dmb" ::: "memory");
    DMA1_S3CR |= DMA_ENABLE;
}

static void stop_on_error(void)
{
    /* Preserve DMA flags before clearing them. */
    uart_dma_last_dma_flags = DMA1_LISR;

    /* Stop this session. Reset before testing again. */
    USART3_CR3 = 0U;
    USART3_CR1 = 0U;

    DMA1_S1CR &= ~DMA_ENABLE;
    DMA1_S3CR &= ~DMA_ENABLE;

    DMA1_LIFCR = RX_ALL_FLAGS | TX_ALL_FLAGS;
    uart_dma_errors++;
}

void uart_dma_init(void)
{
    /* Enable GPIOD and DMA1 clocks. */
    RCC_AHB1ENR |= (1UL << 3) | (1UL << 21);
    (void)RCC_AHB1ENR;

    /* Enable and reset USART3. */
    RCC_APB1ENR |= (1UL << 18);
    (void)RCC_APB1ENR;

    RCC_APB1RSTR |= (1UL << 18);
    RCC_APB1RSTR &= ~(1UL << 18);

    /* PD8 and PD9: push-pull, medium speed, pull-up. */
    GPIOD_OTYPER &= ~((1UL << 8) | (1UL << 9));

    GPIOD_OSPEEDR =
        (GPIOD_OSPEEDR & ~((3UL << 16) | (3UL << 18))) |
        (1UL << 16) | (1UL << 18);

    GPIOD_PUPDR =
        (GPIOD_PUPDR & ~((3UL << 16) | (3UL << 18))) |
        (1UL << 16) | (1UL << 18);

    /* PD8/PD9 alternate function AF7: USART3. */
    GPIOD_AFRH =
        (GPIOD_AFRH & ~((15UL << 0) | (15UL << 4))) |
        (7UL << 0) | (7UL << 4);

    GPIOD_MODER =
        (GPIOD_MODER & ~((3UL << 16) | (3UL << 18))) |
        (2UL << 16) | (2UL << 18);

    /* 115200 baud with a 16 MHz APB1 clock.
     * 8 data bits, no parity, 1 stop bit.
     * Oversampling by 16.
     */
    USART3_CR1 = 0U;
    USART3_CR2 = 0U;
    USART3_CR3 = 0U;
    USART3_BRR = 139U;

    /* Initialize once after MCU reset. */
    DMA1_S1CR = 0U;
    DMA1_S3CR = 0U;
    DMA1_LIFCR = RX_ALL_FLAGS | TX_ALL_FLAGS;

    /* RX: DMA1 Stream 1, Channel 4.
     * Peripheral-to-memory, bytes, normal mode.
     */
    DMA1_S1PAR = (uint32_t)(uintptr_t)&USART3_DR;
    DMA1_S1M0AR = (uint32_t)(uintptr_t)echo_buffer;
    DMA1_S1NDTR = UART_DMA_BLOCK_SIZE;
    DMA1_S1FCR = 0U;

    DMA1_S1CR =
        DMA_CHANNEL_4 |
        DMA_PRIORITY_HIGH |
        DMA_MEMORY_INC |
        DMA_TC_INTERRUPT |
        DMA_TE_INTERRUPT |
        DMA_DME_INTERRUPT;

    /* TX: DMA1 Stream 3, Channel 4.
     * Memory-to-peripheral, bytes, normal mode.
     */
    DMA1_S3PAR = (uint32_t)(uintptr_t)&USART3_DR;
    DMA1_S3M0AR = (uint32_t)(uintptr_t)echo_buffer;
    DMA1_S3NDTR = UART_DMA_BLOCK_SIZE;
    DMA1_S3FCR = 0U;

    DMA1_S3CR =
        DMA_CHANNEL_4 |
        DMA_PRIORITY_HIGH |
        DMA_MEMORY_INC |
        DMA_MEMORY_TO_PER |
        DMA_TC_INTERRUPT |
        DMA_TE_INTERRUPT |
        DMA_DME_INTERRUPT;

    /* Correct interrupt numbers:
     * IRQ 12: DMA1 Stream 1
     * IRQ 14: DMA1 Stream 3
     */
    NVIC_ISER0 = (1UL << 12) | (1UL << 14);

    /* IRQ 39: USART3 error handling. */
    NVIC_ISER1 = (1UL << (39U - 32U));

    /* Enable receive/transmit DMA requests and UART errors. */
    USART3_CR3 = (1UL << 6) | (1UL << 7) | 1UL;

    /* Arm DMA before enabling USART3. */
    start_receive();

    /* Enable USART3, transmitter and receiver. */
    USART3_CR1 = (1UL << 13) | (1UL << 3) | (1UL << 2);
}

void DMA1_Stream1_IRQHandler(void)
{
    uint32_t flags = DMA1_LISR;

    /* Check transfer and direct-mode errors. */
    if ((flags & RX_ERROR_FLAGS) != 0U)
    {
        stop_on_error();
        return;
    }

    if ((flags & RX_COMPLETE_FLAG) != 0U)
    {
        DMA1_LIFCR = RX_ALL_FLAGS;

        /* RX DMA has finished and disabled itself.
         * Preserve the buffer until TX DMA finishes.
         */
        uart_dma_rx_blocks++;
        start_transmit();
    }
}

void DMA1_Stream3_IRQHandler(void)
{
    uint32_t flags = DMA1_LISR;

    /* Check transfer and direct-mode errors. */
    if ((flags & TX_ERROR_FLAGS) != 0U)
    {
        stop_on_error();
        return;
    }

    if ((flags & TX_COMPLETE_FLAG) != 0U)
    {
        DMA1_LIFCR = TX_ALL_FLAGS;

        /* All bytes have been handed to USART3.
         * The final byte may still be shifting out.
         * DMA no longer needs the buffer.
         */
        uart_dma_tx_blocks++;
        start_receive();
    }
}

void USART3_IRQHandler(void)
{
    uint32_t status = USART3_SR;

    if ((status & 0x0FUL) != 0U)
    {
        /* SR then DR read clears UART error flags.
         * Normal received bytes are transferred by DMA.
         */
        (void)USART3_DR;
        stop_on_error();
    }
}