# STM32_UARTDriver

Note: I am learning github right now, so things will look and be kinda trash until I get a hang of things...

This is a are metal guide for implementing UART serial for STM32.

UART serial communciation is considered to be one of the - if not the - most commonly used communication protocol within the world of micros. This definitely reflects on the reference manual (refman) of STM32, where the UART section takes up probably the most amount of pages in the documentation.
When I first saw this, I was a bit baffled. After all, UART is considered to be simple, so why so much documentation? Well, as it turns out, you can modify UART left, right and center, add DMA to deal with flows of incoming data, implement multiple synchronization options, control communication quality using parity bits, add security, have extended/advanced modes and in general complicate the protocol past the point of necessity if you just want a damn byte passed from one micro to another. The good news is that once you ignore all this extra, UART does become a lot simpler. The absolutely relevant sections in the refman for a simple byte-to-byte communication are (I am using the refman for the L0x3 here):

- 29.4 UART implementation - a table to show, which of the UARTs have which features (there are multiple of UART options in the micro, in the L0x3, there are 5 different UARTs)
- 29.5.1 USART character description - how is a frame/character/word looks like in UART and what is an idle/break character
- 29.5.2 USART Transmitter - how to control the Tx side of the UART 
- 29.5.3 USART Receiver - how to control the Rx side of the UART 
- 29.5.4 USART baud rate generation - how to calculate the baud rate for the driver (Table 141 is crucial!)
- 29.7. UART interrupts - how UART interrupts work, what they are and how they can be triggered
- 29.8. UART registers - a list of the registers to control the UART. This section will be used a LOT during the project. As a matter of fact, bare metal coding IS the application of this section in practice.

In a practical sense, UART is very much the same as the other standard com protocols (I2C and SPI) where:
- it has a buffer for both incoming/outgoing messages that are connected to the bus through shift registers. We interfact with these buffers to receive/transmit a message.
- Rx, Tx and the UART itself have their own enable bits. The communication is controlled by the state of the buffer and the state of the UART enable bits
- the communication itself is non-blocking when active, thus adequate amount of time must be allowed for the shift registers to "shift" or the data will get corrupted. On the Rx side, this can be scheduled by checking if the Rx buffer is full or not, on the Tx side, this can either be done by defining a hard delay between Tx buffer updates or by checking the if the Tx buffer is empty.

Describing in layman's terms on how UART itself works:
- 1)We set up, how we want to communicate (word length, stop bits, clock phase, etc., we will discuss this more in the UART init function)
- 2)We enable the buffers for Tx or Rx or both. We enable the the UART and thus start to listen to the bus or prepare to send over whatever we put into the Tx buffer.
- 3)We ensure that the Rx buffer is emptied when full (so no incoming data lost), we funnel data into the Tx buffer when it is empty AND not do so again until the shift register on the Tx side has been emptied aswell. This latter is crucial if we want to send over more than  one byte in a sequence.

One particularity of UART compared to the other two common com protocols is that it does not have a "master": where with other system, one simply is synchronized to the bus due to the master's clock, there is no such thing in UART. Instead, the transmitter (Tx) and the receiver (Rx) "agree" ahead of time at what frequency (here called baud rate) the data is coming in on your bus, and then the Rx side samples the bus at a much higher speed to actually recognize these data bytes (called oversampling the bus by times 8 or 16).

Until this moment, the functions we are using are:
- void UART1Config (void)
- uint8_t UART1RxByte (void)
- uint8_t UART1TxByte (void)

Returning the the issue we touched upon above, added complexity emerges when we wish to receive or transmit multiple bytes in rapid succession. The issues are that while we can do a byte-by-byte communication where we shut down the UART between bytes, such approach would not work for an entire message worth of bytes. To be more precise, we don't actually know anything about:
-the start of a message: there is a start condition for the hardware to start listening to the bus for an incoming byte, but that doesn't tell anything about when we should capture the data on the bus. One can potentially listen on a whole bunch of trash (or even noise) this way. 
-the end of a message: the lack of a clear stop condition for a message means that one can not tell when the message has stopped.
Start message problem is relatively simple to solve where one will look for an exact sequence of bytes at the beginning of a message and if that sequence if found, the micro starts logging in the incoming data.
For the message ending, the solution from the start side can not be used since one can not control the content of a message and ensure that any random sequence defined for the end indicator would not come up already in the message, effectively cutting short the communication. The typical solution to this issue is to know before we send the message, how many bytes it would be, then send this expected number of bytes over to the receiver as the very first part of any message. This way the received will call it a day once the expected number of bytes have been received. I personnaly decided not to follow this solution since it limits the utility of the Rx to those scenarios where the message length is known prior the transmission. How I did it (see below) is by relying on the UART main interrupt to end the message. I will touch upon interrupt handling in an other project.






Another particularity is with controlling the UART communication. Unlike SPI where control is pretty clear using the slave select and the duplex nature of the bus or with I2C where designated start and stop conditions can be recognized readily by the hardware, UART does not have a distinct start or stop condition for messages. These issues will be discussed after we have generated a byte-by-byte Tx/Rx communication.

