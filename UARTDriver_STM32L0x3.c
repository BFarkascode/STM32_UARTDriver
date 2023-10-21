/*
 * UARTDriver_STM32L0x3.c
 *
 *  Created on: Oct 27, 2023
 *      Author: BalazsFarkas
 *
 */

#include "UARTDriver_STM32L0x3.h"
#include "stm32l053xx.h"

//1)UART init (no DMA)
void UART1Config (void)
{
	/**We wish to recreate the CubeMX generate uart1 control. That uart1 has 115200 baud, 8 bit word length, no parity, 1 stop bit and 16 sample oversampling
	 * We do three things here:
	 * 
	 * 1)RCC registers: We enable the clocking of the peripheral (APB1ENR or APB2ENR, depending on where the UART is clocked from) and the GPIO port (IOPENR).
	 *   It is very important to know the frequency of the clocking for the UART (that is, what frequency APB1/APB2 is running at) since this will define the value we use below in the USARTx BRR register.
	 * 2)GPIOx registers: We set up the pins we wish to use (consult the datasheet of the micro to know, which pins can have the UART and what MODER/AFR need to be used for them)
	 * 3)USARTx CR1/CR2/CR3/BRR registers: we set the three control registers and the BRR register. The BRR register will define the baud rate for the UART.
	 *   Table 141 in the refman holds the BRR values for some of the typical baud rates at typical APBx frequencies. Within the obvious word length/stop bit/endian choice/clock polarity/clock phase selection, we choose an oversmapling of 16 to improve noise reaction, we activate one bit sampling to deactivate noise control and remove the overrun error.
	 *   These latter two are done to avoid the need of error handling in this iteration of the UART: we simply will "carry on" with the error and ignore them if they occur. (Of note, failing to remove these two bits would freeze the communication whenever when a noise ). This is also where the Tx/Rx enable bits are and the main USART enable bit that will turn on/off the UART. 
	 * 
	 * We don't activate the UART in this config function, instead we will do it locally whenever we call the function to receive a byte or a message.
	 * The registers GTPR and RTOR are not used since:
	 *   GTPR is a SMARTCARD/IrDA only register, which we don't need
	 *   RTOR is the timeout register used to tell after how many break bits we can consider that the UART has had a timeout. Not useful if we don't plan on deactivating the USART bus.
	 * The remaining registers (ISR, ICR, RQR, TDR, RDR) are all controller bits used to either control or supervise the UART (RQR is to generate a request, ISR is read-only register holding the immediate state of the UART, ICR is to remove the ISR flags, TDR/RDR are the Tx and Rx buffers).

	 * Steps to implement the configuration:
	 * 1)Set up clock for the UART and the GPIO - RCC registers for uart1 and for GPIOA (PA2 and PA3 are the uart1 pins)
	 * 2)Configure the pins - set mode register for pins (alternative mode), GPIO output speed, set alternate function registers (AF registers)
	 * 3)Config UART - word length (CR1), baud rate (BRR), stop bits (CR2), enable pin (CR1), DMA enable (CR3)
	 *
	 * 4)USARt will commence by controlling the TE bit. Write data to TDR (clears TXE bit). Wait until TC if high to indicate that Tx is done.
	 * 5)TXE indicates that TDR is empty.
	 * 6)TC can have an interrupt if TCIE is HIGH.
	 *
	 * Note: DMA must be deinitalized before we change to the app from the boot.
	 * Note: idle is a full frame of 1s. Break character is a full frame of 0s, followed by two stop bits.
	 *
	**/


	//1)Set the clock source, enable hardware
	RCC->APB2ENR |= (1<<14);															//APB2 is the peripheral clock enable. Bit 14 is to enable the uart1 clock
	RCC->IOPENR |= (1<<0);																//IOPENR enables the clock on the PORTA (PA10/D2 is Rx, PA9/D8 is Tx for USART1)
//	RCC->CCIPR |= (2<<0);																//we could clock the UART using HSI16 as source (16 MHz). We currently clock on APB2 isnetad, which is also 16 MHz.

	//2)Set the GPIOs
	GPIOA->MODER &= ~(1<<18);															//AF for PA9
	GPIOA->MODER &= ~(1<<20);															//AF for PA10
	GPIOA->OSPEEDR |= (3<<18) | (3<<20);												//very high speed PA9 and PA10
	GPIOA->AFR[1] |= (1<<6) | (1<<10);													//from page 46 of the device datasheet, USART1 is on AF4 for PA9 and PA10 on the HIGH register of the AFR
																						//OTYPER and PUPDR are not written to since we want push/pull and no pull resistors

	//3)Configure UART
	USART1->CR1 = 0x00;																	//we clear the UART1 control registers
	USART1->CR2 = 0x00;																	//we clear the UART1 control registers
	USART1->CR3 = 0x00;																	//we clear the UART1 control registers
																						//Note: registers can only be manipulated when UE bit in CR1 is reset
	USART1->CR1 &= ~(1<<12);															//M0 word length register. Can only be written to when UART is disabled. M[1:0] = 00 is 1 start bit, 8 data bits, n stop bits
	USART1->CR1 &= ~(1<<28);															//M1 word length register. Works with M0.

	USART1->CR1 &= ~(1<<15);															//oversampling is 16

	USART1->CR1 |= (1<<2);																//enable the Rx part of UART
	USART1->CR1 |= (1<<3);																//enable the Tx part of UART - currently removed

	USART1->CR2 &= ~(1<<12);															//One stop bit
	USART1->CR2 &= ~(1<<13);

	USART1->CR3 |= (1<<11);																//one bit sampling on data
	USART1->CR3 |= (1<<12);																//overrun error disabled
																						//LSB first, CPOL clock polarity is standard, CPHA clock phase is standard

//	USART1->BRR |= 0x683;																//we want to have a baud rate of 9600 with HSI16 as source (refman 779 proposes values for 32 MHz) and oversampling of 16

	USART1->BRR |= 0x116;																//57600 baud rate using 16 MHz clocking and oversampling of 16
																						//Note: 115200 baud rate is just barely too fast for the DMA to restart between incoming UART bytes

	//4)Enable the interrupts, set up errors
//	USART1->CR1 |= (1<<4);																//IDIE enabled. It activates the main USART1 IRQ.
																						//Note: an idle frame is the word length, plus stop bit, plus start bit. This will be a bit longer than 1 ms (1.04 ms to be precise) at 9600 baud rate
																						//Note: on an Adalogger, the Serial1 commands are NOT blocking!
																						//Note: this is currently removed since it is not necessary for byte-by-byte communication.

}


//2)UART1 get one byte

uint8_t UART1RxByte (void)
{
	/* Code to acquire one byte - one Rx transmission.
	
	 * After enabling the UART, we do two things here:
	 * 1)We wait until the RXNE flag within the ISR register goes high. It indicates that the Rx buffer is Not Empty (thus RXNE). We use a while loop to time the readout to when we actually have something in the buffer. This makes the function blocking though.
	 * 2)We read out the Rx buffer. The act of reading out will remove the RXNE flag in the ISR register, we don't need to do it by writing to the ICR register.
	 * We finish by disabling the UART. One benefit of disabling the UART that it clears all ISR flags.

	 * Implementation steps are:
	 * 1)Check UART state/interrupt register for ready flag
	 * 2)Extract data from the UART RX buffer
	 * **/

	USART1->CR1 |= (1<<0);																//enable the UART1
	uint8_t rx_buf;

	//1)
	while(!((USART1->ISR & (1<<5)) == (1<<5)));											//RXNE bit. Goes HIGH when the data register is ready to be read out.

	//2)
	rx_buf = USART1->RDR;																//reading the RDR clears the RXE flag
	USART1->CR1 &= ~(1<<0);																//disable the UART1
	return rx_buf;
}


//3)UART1 send one byte

//TBD

void UART1TxByte (uint8_t) {

}


//4)UART1 get the message - with message start sequence
void UART1RxMessage_custom(void) {
	/*
	 * Small note, I used enum typedefs for Yes/No selection (see the header file). This is to have something similar as a boolean type for clarity's sake.
	 
	 * We do four things here:
	 * 1)We tell the function to run until the message received flag has not been cleared.
	 * 2)We start the first state machine where the first state is that message has not started yet. In this state, we start listening to the bus and wait until a byte comes in (while loop ensures a blocking operation, we don't move on until something is received).
	 * If this byte is the byte we expect for our start sequence, we start a second state machine and set the flags accordingly. The "break" at the end of these states will reset the function back to the very first while loop (nested switches the way how they are used here will reset all the switches, not just the immediate one).
	 * We transition to the second state of the first state machine if we have received the start sequence byte twice. (If we don't, we reset all the flags and start from the beginning)
	 * 3)In the second state of the first state machine, we start listening to the bus again and read out the values into an message buffer using a pointer. This will mean that consegutive bytes will be fed into an array, allowing us to receive the entiretiy of the message.
	 * (The pointer is to be placed at the start address of the array external to the UART driver.) We also enable the UART IRQ which will then trigger when UART has experienced an idle frame. It is important to allow the IRQ to run only when we are within a message, otherwise it will be triggered unnecessarily.
	 * The IRQ will eventually break the very first while loop in the 1) point and allow us to move forward.
	 * 4)At the end, once the message is over, we clear all the flags, put the pointer back to the start of the message array and shut off the IRQ. The IRQ must be shut off since we aren't in a message anymore.

	 * 1)We enable the UART.
	 * 2)While the timeout is not reached
	 * 3)We check if we have the incoming message sequence detected
	 * 4)If yes, we transfer incoming bytes to a memory buffer
	 * 5)After the IRQ is triggered and we leave the state machine-in-the-loop, we reset everything and shut down the UART
	 *
	 * Note: the start of the message is detected when 0xFOFO comes through the bus.
	 * Note: the end of the message is detected when the bus has been idle for 2 bytes. This MUST be matched on the transmitter side by implementing an adequately sized delay between messages. Failing to do so freezes the bus.
	 * Note: the bus is VERY noisy. If we don't sample the bus exactly where we should, we can capture fake data.
	 *
	 * */

	//1)
	USART1->CR1 |= (1<<0);																//enable the UART1

	//2)
	while (UART1_Message_Received == No){												//we execute the following section until the IRQ is triggered by an idle line
		switch (UART1_Message_Started){
		case No:
			while(!((USART1->ISR & (1<<5)) == (1<<5)));									//RXNE bit. Goes HIGH when the data register is ready to be read out.
			uint8_t Rx_byte_buf = USART1->RDR;											//reading the RDR clears the RXE flag

			//3)
			if(Rx_byte_buf == UART_message_start_byte) {								//we detected the start byte
				switch (UART1_Start_Byte_Detected_Once){
				case No:																//if this was the first time
					UART1_Start_Byte_Detected_Once = Yes;
					break;
				case Yes:																//if this was the second time
					UART1_Start_Byte_Detected_Once = No;
					UART1_Message_Started = Yes;
					break;
				default:
					break;
				}

			} else {
				UART1_Start_Byte_Detected_Once = No;									//if the start byte is not detected twice in a sequence, we discard
			}
			break;

		//4)
		case Yes:

			if((USART1->ISR & (1<<5)) == (1<<5)){										//if we have data in the RX buffer
																						//Note: we don't use a while loop here to avoid being stuck (RXNE test comes after the idle test, which could freeze the bus)
				USART1->ICR |= (1<<4);													//we clear the idle flag
				NVIC_EnableIRQ(USART1_IRQn);											//we activate the IRQ
																						//Note: the IRQ should only be active when we are receiving a message. Activating it earlier can lead to false triggers.
																						//Note: we use an IRQ to end the message since we can't use a specific sequence to stop it. That sequence can very much be in the machine code.
																						//Note: 0xFFFF is different to two idle frames due to the position of the start bit
				*(Rx_Message_buf_ptr++) = USART1->RDR;									//we read out and reset the RXNE flag
			} else {
				//do nothing
			}
			break;

		default:
			break;
		}
	}

	//5)
	NVIC_DisableIRQ(USART1_IRQn);
	USART1->CR1 &= ~(1<<0);																//disable the UART1
	UART1_Message_Received = No;														//we reset the message received flag
	UART1_Message_Started = No;															//we reset the message started flag
	Rx_Message_buf_ptr = Rx_Message_buf;												//we reset the buf pointer to its original spot
}


//5)UART1 IRQ setup
	/*
	 *We do two things here:
	 *1)We set the IRQ priority. Every IRQ has to have a priority in case the end up overlapping. Here we only have one IRQ running so it is safe to give it a priority of 1. One will need to adjust this priority in case of multiple IRQs running within the code.
	 *2)We set the bit in the CR1 register that will activate the UART IRQ when an idle frame is detected. The refman does not specifically say here that enabling and disabling the UART is necessary to implement this bit change, albeit it is recommended to not do any changes within a driver while it is running.

	 *shut off UART to modify it
	 *CR1 bit 4 for IDIE to allow IRQ
	 *turn UART back on
	 *	
	 *
	 */

//TBD	 
	 
void N/A(void) {

}



//6) UART1 IRQ
void USART1_IRQHandler(void) {
	/* 
	 * We do three things here:
	 * 	 
	 * 1)We increase a counter of idle frames (i.e. we count, how many times we have this IRQ trigger)
	 * 2)If we had this IRQ trigger 2 times, we set the message received flag (and thus break the main while loop in the RxMessage function) and wipe the idle frame counter.
	 * 3)We remove the ISR flag connected to the IRQ. This MUST be done all the time, otherwise the IRQ will keep on triggering unnecessarily.

	 * This IRQ currently only activates on the detection of an idle frame.
	 * Idle frames are the indicators that we don't have incoming data anymore.
	 *
	 * Note: since we are parallel receiving data AND doing other stuff, we MUST leave some time for any concurrent process to activate or conclude.
	 * Note: with an idle frame counter set to 2, we have a delay of roughly 1 ms.
	 */

	Idle_frame_counter++;
	if(Idle_frame_counter >=2){
		UART1_Message_Received = Yes;
		Idle_frame_counter = 0;
	}
	USART1->ICR |= (1<<4);														//Idle detect flag clearing
}


//7)UART1 DMA

//TBD

void N/A(void) {

}