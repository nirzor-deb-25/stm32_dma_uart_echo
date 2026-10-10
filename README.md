# STM32 DMA UART Echo



A register-level UART echo project for the **NUCLEO-F439ZI**, written in C without HAL.



USART3 receives a 32-byte block using DMA, then transmits the same block using DMA. Interrupt handlers manage completed transfers, while the main loop waits for interrupts using `WFI`.



## Features



- DMA-based UART receive and transmit

- No CPU polling of UART data flags

- Fixed 32-byte binary-safe transfers

- Interrupt-driven receive/transmit sequencing

- Diagnostic counters and saved DMA error flags

- Handwritten Makefile with automatic header dependencies

- PowerShell test script that compares every echoed byte



## Hardware



- NUCLEO-F439ZI development board

- Micro-USB data cable connected to the ST-LINK USB connector

- Windows computer



The onboard ST-LINK virtual COM port connects to USART3 in the board's default configuration. No external USB-to-UART adapter is needed.



| Function | Configuration |

|---|---|

| USART | USART3 |

| TX pin | PD8, alternate function 7 |

| RX pin | PD9, alternate function 7 |

| Receive DMA | DMA1 Stream 1, Channel 4 |

| Transmit DMA | DMA1 Stream 3, Channel 4 |

| Receive DMA interrupt | IRQ 12 |

| Transmit DMA interrupt | IRQ 14 |

| USART3 interrupt | IRQ 39 |

| UART settings | 115200 baud, 8 data bits, no parity, 1 stop bit |

| Flow control | None |

| Block size | 32 bytes |

| Clock assumption | Reset-default 16 MHz HSI, APB1 prescaler 1 |



The tested Windows serial port was COM7. Other computers may assign a different port number.



## How It Works



1. Configure GPIO, USART3, DMA1 and interrupts.

2. Arm receive DMA for 32 bytes.

3. Wait for interrupts in the main loop.

4. Receive DMA completes and triggers its interrupt.

5. The receive handler starts transmit DMA using the same buffer.

6. Transmit DMA completes and triggers its interrupt.

7. The transmit handler rearms receive DMA for the next block.



The host must wait for the entire echo before sending the next block.



DMA transfer completion means all bytes have been handed to USART3. The final byte may still be shifting onto the wire at that moment.



## Files



| File | Purpose |

|---|---|

| `Inc/stm32_regs.h` | Memory-mapped register definitions |

| `Inc/uart_dma.h` | UART/DMA interface and block size |

| `Src/main.c` | Initialization, debug-in-sleep setting and WFI loop |

| `Src/system.c` | Vector-table setup and C-library initialization hook |

| `Src/uart_dma.c` | GPIO/UART/DMA configuration and interrupt handlers |

| `Startup/startup_stm32f439zitx.s` | MCU startup assembly and interrupt vector table |

| `STM32F439ZITX_FLASH.ld` | Flash/RAM linker layout |

| `Makefile` | Compile, link, firmware conversion and programming |

| `setup.ps1` | Locate STM32 tools and update the session PATH |

| `test_uart_echo.ps1` | Repeated binary echo test |

| `.gitignore` | Exclude generated and local files |



## Build Requirements



- STM32CubeIDE installation containing the GNU Arm compiler, GNU Make and STM32CubeProgrammer CLI

- Windows PowerShell



The setup script was tested with STM32CubeIDE 2.2.0. Adjust its installation path if your tools are installed elsewhere.



## Build and Flash



Open PowerShell in the repository directory:



```powershell

. .\setup.ps1

make -j4

make flash

```



Keep only the intended STM32 board connected when flashing with the default command.



To rebuild everything:



```powershell

make clean

make -j4

```



To display firmware section sizes:



```powershell

make size

```



Generated firmware files:



- `build/stm32_dma_uart_echo.elf`

- `build/stm32_dma_uart_echo.bin`

- `build/stm32_dma_uart_echo.hex`



## Run the Echo Test



Close other applications using the serial port. Reset the board before a fresh test if a previous transfer failed.



```powershell

.\test_uart_echo.ps1 -Port COM7 -Blocks 100

```



Expected final output:



```text

PASS: 100 blocks, 3200 bytes matched.

```



The script sends one 32-byte block, receives its complete echo, compares every byte, then sends the next block. It uses reproducible pseudorandom binary data.



The PowerShell receive loop runs on the computer. The STM32 uses DMA and interrupts.



## Validation



Observed results:



- Clean firmware build completed without warnings or errors.

- An unchanged build reported nothing to rebuild.

- STM32CubeProgrammer verified the programmed firmware.

- A 32-byte ASCII echo matched exactly.

- Two runs of 100 binary blocks passed, with 3,200 bytes compared per run.



Power-cycle validation can be performed by disconnecting and reconnecting USB, then rerunning the test without starting a debugger.



## Diagnostics



The source provides:



- `uart_dma_rx_blocks`

- `uart_dma_tx_blocks`

- `uart_dma_errors`

- `uart_dma_last_dma_flags`



Use the ELF symbol table to find their current addresses:



```powershell

arm-none-eabi-nm -n .\build\stm32_dma_uart_echo.elf |

&#x20;   Select-String 'uart_dma_'

```



For live register inspection, use CubeProgrammer's `mode=HOTPLUG`. Its normal connection mode resets and halts the target.



Example: read the UART configuration without reading its data register:



```powershell

STM32_Programmer_CLI.exe -c port=SWD mode=HOTPLUG -r32 0x40004808 16

```



The read-size argument is in bytes.



Debug access during CPU sleep is enabled in `main.c`. This project does not measure power consumption or CPU utilization.



## Limitations



- This is a fixed-block echo implementation.

- Fewer than 32 bytes will wait until the block is complete.

- There is one shared buffer; continuous simultaneous receive/transmit is not supported.

- The host must send one block and wait for its echo.

- Detected UART or enabled DMA errors stop the session; reset is required to restart.

- It is not a variable-length terminal echo or a UART bootloader.



## Learning Outcomes



- Configure UART pins using alternate functions.

- Map peripheral requests to DMA streams and channels.

- Configure DMA direction, addresses and transfer counts.

- Enable the correct NVIC interrupts.

- Distinguish DMA completion from final UART wire completion.

- Diagnose firmware with registers, flags and counters.

- Build and flash a multi-file embedded C application.

- Verify serial communication using automated binary comparisons.



## References



- [ST Nucleo-144 board manual, UM1974](https://www.st.com/resource/en/user_manual/um1974-stm32-nucleo144-boards-mb1137-stmicroelectronics.pdf)

- [STM32F429/439 documentation](https://www.st.com/en/microcontrollers-microprocessors/stm32f429-439/documentation.html)

- [STM32CubeProgrammer CLI documentation](https://dev.st.com/stm32cube-docs/prog/2.23.0/en/docs/markup/CubeProg_Command_Lines.html)

