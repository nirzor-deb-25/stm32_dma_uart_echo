#ifndef UART_DMA_H
#define UART_DMA_H

#include <stdint.h>

#define UART_DMA_BLOCK_SIZE 32U

/* Configure USART3 and start receiving the first DMA block. */
void uart_dma_init(void);

/* Diagnostic counters, useful when inspecting the debugger. */
extern volatile uint32_t uart_dma_rx_blocks;
extern volatile uint32_t uart_dma_tx_blocks;
extern volatile uint32_t uart_dma_errors;

#endif