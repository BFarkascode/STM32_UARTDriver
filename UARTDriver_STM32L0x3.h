/*
 * UARTDriver_STM32L0x3.h
 *
 *  Created on: Oct 23, 2023
 *      Author: BalazsFarkas
 */

#ifndef INC_UARTDRIVER_STM32L0x3_CUSTOM_H_
#define INC_UARTDRIVER_STM32L0x3_CUSTOM_H_

#include "stdint.h"
#include "main.h"

//LOCAL TYPEDEF
typedef enum {
	No,
	Yes
} enum_Yes_No_Selector;

//LOCAL CONSTANT
static const uint8_t UART_message_start_byte = 0xF0;		//the message start sequence is (twice this byte)

//LOCAL VARIABLE
static enum_Yes_No_Selector UART1_Start_Byte_Detected_Once = No;
static uint8_t Idle_frame_counter = 0;
static enum_Yes_No_Selector UART1_Message_Received;
static enum_Yes_No_Selector UART1_Message_Started;

//EXTERNAL VARIABLE
extern uint32_t Rx_Message_buf [64];						//we have a 32 bit MCU
extern uint8_t* Rx_Message_buf_ptr;							//UART data is only 8 bits

//FUNCTION PROTOTYPES
void UART1Config (void);
uint8_t UART1RxByte (void);
void UART1TxByte (uint8_t Tx_byte);
void UART1RxMessage(void);
void UART1DMAEnable (void);


#endif /* INC_UARTDRIVER_STM32L0x3_H_ */
