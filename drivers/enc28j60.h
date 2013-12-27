/*
 * Copyright (c) 2013 Ivan A. Sergeev
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * ENC28J60 Ethernet Controller Driver
 * Vanya Sergeev - vsergeev@gmail.com
 *
 * Pretty much architecture independent, but the enc28j60_util.c functions 
 * (SPI and delay loops) should be adpated for the target architecture.
 *
 * This driver uses the same constants as the enc28j60.h from the Procyon 
 * AVRlib written by Pascal Stang.
 * Everything else is written based on the ENC28J60 datasheet.
 *
 * Supports multiple ENC28J60 interfaces.
 *
 */

#ifndef _ENC28J60_H
#define _ENC28J60_H

#include <stdint.h>
#include "enc28j60_constants.h"

/*****************************************************************************/

/** SPI rate of the ENC28J60, in Hz */
#define ENC28J60_CLOCK	10000000

/** The number of ENC28J60 interfaces to be implemented. */
#define ENC28J60_NUM_INTERFACES	2

/** MAC address */
#define ENC28J60_MAC0	uip_lladdr.addr[0]
#define ENC28J60_MAC1	uip_lladdr.addr[1]
#define ENC28J60_MAC2	uip_lladdr.addr[2]
#define ENC28J60_MAC3	uip_lladdr.addr[3]
#define ENC28J60_MAC4	uip_lladdr.addr[4]
#define ENC28J60_MAC5	uip_lladdr.addr[5]

/** Ethernet controller's mode of operation: FULL_DUPLEX
 * or HALF_DUPLEX. */
#define FULL_DUPLEX

/** Maximum Frame Length of an Ethernet Frame - Default is 1518 bytes **/
#define MAX_FRAME_LEN	1518

/** The per packet control byte as specified by figure 7-1 in the ENC28J60
 * datasheet. Required for every frame transmission.
 */
#define PER_PACKET_CONTROL	0x00

/** Compiles the interrupts initialization code. */
//#define ENC28J60_USE_INTERRUPTS

/* The ENC28J60 Internal Ethernet Buffer memory distribution for receive 
 * and transmit buffers.
 * Transmit buffer gets the upper portion of the buffer memory of (maximum
 * frame length + 18 bytes extra) size.
 * Receive buffer gets the lower portion and majority of the buffer memory, 
 * starting from 0x0000 to (the start of the transmit buffer - 2).
 */
#define TX_BUFFER_START (0x1FFF-MAX_FRAME_LEN+18)	
#define TX_BUFFER_END	0x1FFF
#define RX_BUFFER_START	0x0000
#define RX_BUFFER_END	(TX_BUFFER_START-1)

/** Promiscuous Filter Configuration.
 * Disable all filters in the ERXFCON so we can promiscuously pick up all
 * packets.
 */
#define FILTER_PROMISC	0x00

/** Unicast Filter Configuration.
 * Accepts packets destined to the ethernet controller, broadcast packets, and
 * only accepts packets with a valid CRC.
 */
#define FILTER_UNICAST	(ERXFCON_UCEN)|(ERXFCON_CRCEN)|(ERXFCON_BCEN)

/*****************************************************************************/

/** Current ENC28J60 Ethernet Controller index (to select between the
 * two, or more, different ENC28J60 ethernet interfaces). */
extern uint8_t ENC28J60_Index;

/*****************************************************************************/
/*** enc28j60_util.c - Delay and SPI utility functions, hardware specific. ***/

/**
 * Delays the specified number of milliseconds.
 * @param ms milliseconds to delay.
 */
void delay_ms(uint32_t ms);

/**
 * Delays the specified number of microseconds (approximately).
 * @param us microseconds to delay.
 */
void delay_us(uint32_t us);

/**
 * Enable the host microcontroller's pin interrupts that are connected to the
 * ethernet controller's INT pins.
 */
void enc28j60_LPC_Interrupts_Enable(void);

/**
 * Disable the host microcontroller's pin interrupts that are connected to the
 * ethernet controller's INT pins.
 */
void enc28j60_LPC_Interrupts_Disble(void);

/**
 * Initializes the SPI pins, frequency, and SPI mode configuration.
 */
void enc28j60_spi_init(void);

/**
 * Selects an ENC28J60 chip by bringing the chip's CS low.
 * The ENC28J60 to talk to is determined by the global ENC28J60_Index variable.
 */
void enc28j60_spi_select(void);

/**
 * Deselects an ENC28J60 chip by bringing the chip's CS high.
 * The ENC28J60 to talk to is determined by the global ENC28J60_Index variable.
 */
void enc28j60_spi_deselect(void);

/**
 * Writes a byte to the ENC28J60 through SPI.
 * The chip must be selected prior to this write.
 * @param data the 8-bit data byte to write.
 */
void enc28j60_spi_write(uint8_t data);

/**
 * Explicitly read a byte from the ENC28J60 by first sending the dummy byte
 * 0x00.
 * The chip must be selected prior to this write.
 * @return the data read.
 */
uint8_t enc28j60_spi_read(void);

/*****************************************************************************/
/*** enc28j60.c - ENC28J60 Driver Declarations and Functions ***/

/**
 * Sends the specified read command and returns the response.
 * Performs the required dummy read for MAC/MII Registers.
 * See section 4.0 of the ENC28J60 datasheet. 
 * @param opcode the 3-bit opcode of the instruction.
 * @param address the 5-bit address/argument of the instruction.
 * @return the 8-bit data response to the command.
 */
uint8_t enc28j60_Command_Read(uint8_t opcode, uint8_t address);

/**
 * Sends the specified write command and the data to be written.
 * See section 4.0 of the ENC28J60 datasheet.
 * @param opcode the 3-bit opcode of the instruction.
 * @param address the 5-bit address/argument of the instruction.
 * @param data the 8-bit data that follows the command.
 */
void enc28j60_Command_Write(uint8_t opcode, uint8_t address, uint8_t data);

/**
 * Reads the ENC28J60 buffer memory for 'len' bytes into the passed 'buffer'.
 * See section 4.2.2 of the ENC28J60 datasheet.
 * @param buffer the unsigned 8-bit array to store the read data.
 * @param len the number of bytes to read.
 */
void enc28j60_Buffer_Read(uint8_t *buffer, uint16_t len);

/**
 * Writes the ENC28J60 buffer memory 'len' bytes of the passed 'buffer'.
 * See section 4.2.4 of the ENC28J60 datasheet.
 * @param buffer the unsigned 8-bit array of data to write to the ENC28J60
 * @param len the number of bytes to write.
 */
void enc28j60_Buffer_Write(uint8_t *buffer, uint16_t len);

/**
 * Reads and returns a single byte from the ENC28J60 buffer memory.
 * See section 4.2.2 of the ENC28J60 datasheet.
 * @return the byte read.
 */
uint8_t enc28j60_Buffer_ReadByte(void);
 
/**
 * Writes a single byte to the ENC28J60 buffer memory.
 * See section 4.2.4 of the ENC28J60 datasheet.
 * @param data the byte to write.
 */
void enc28j60_Buffer_WriteByte(uint8_t data);


/**
 * Reads the data stored at the specified ENC28J60 register.
 * Changes banks if necessary to access the specified register.
 * Use enc28j60_PHY_Read() to read PHY registers.
 * See section 4.2.1 of the ENC28J60 datasheet.
 * @param address the address of the register to read.
 * @return the byte read from the specified register.
 */
uint8_t enc28j60_Register_Read(uint8_t address);

/**
 * Writes a byte to the specified ENC28J60 register.
 * Changes banks if necessary to access the specified register.
 * Use enc28j60_PHY_Write() to write to PHY registers.
 * See section 4.2.3 of the ENC28J60 datasheet.
 * @param address the address of the register to write to.
 * @param data the data byte to write to the register.
 */
void enc28j60_Register_Write(uint8_t address, uint8_t data);

/**
 * Performs a bitfield set on the specified ENC28J60 register.
 * Changes banks if necessary to access the specified register.
 * See section 4.2.5 of the ENC28J60 datasheet.
 * @param address the address of the register whose bits will be set.
 * @param bits the bits to set in the register.
 */
void enc28j60_Bitfield_Set(uint8_t address, uint8_t bits);

/**
 * Performs a bitfield clear on the specified ENC28J60 register.
 * Changes banks if necessary to access the specified register.
 * See section 4.2.6 of the ENC28J60 datasheet.
 * @param address the address of the register whose bits will be cleared.
 * @param bits the bits to clear in the register.
 */
void enc28j60_Bitfield_Clear(uint8_t address, uint8_t bits);

/**
 * Changes banks to enable access to the specified register.
 * The bank is stored in the 6th and 7th bits of the passed register address.
 * See section 3.1 of the ENC28J60 datasheet.
 * @param address the register address that banks must be changed for.
 */
void enc28j60_SelectBank(uint8_t address);

/**
 * Reads from the specified 16-bit ENC28J60 PHY register.
 * See section 3.3.1 of the ENC28J60 datasheet.
 * @param address the address of the PHY register.
 * @return the 16-bit data read from the PHY register.
 */
uint16_t enc28j60_PHY_Read(uint8_t address);

/**
 * Writes to the specified 16-bit ENC28J60 PHY register.
 * See section 3.3.2 of the ENC28J60 datasheet.
 * @param address the address of the PHY register.
 * @param data the 16-bit data to write to the PHY register.
 */ 
void enc28j60_PHY_Write(uint8_t address, uint16_t data);

/**
 * Performs a complete system reset of the ENC28J60, including the wait for the
 * ethernet controller to initialize.
 * See section 11.2 of the ENC28J60 datasheet.
 */
void enc28j60_System_Reset(void); 

/**
 * Enable ENC28J60's global interrupts so the ethernet controller can raise
 * an interrupt on its INT pin.
 */
void enc28j60_Enable_Global_Interrupts(void);

/**
 * Disable ENC28J60's global interrupts so the ethernet controller will not
 * raise interrupts on its INT pin.
 */
void enc28j60_Disable_Global_Interrupts(void); 

/**
 * Complete initializes the ENC28J60 in Full-Duplex operation with the
 * specific configuration details defined in the driver header file.
 */
void enc28j60_Init(void);

/**
 * Sends a frame.
 * CRC is calculated by the ENC28J60, so it need not be included in the frame
 * data.
 * @param frame unsigned 8-bit array of the frame data.
 * @param len length of the frame data with in the data byte array.
 * @return number of bytes sent (always 'len'), 0 if the frame was larger than
 *  the maximum frame length supported.
 */
int enc28j60_Frame_Send(uint8_t *frame, uint32_t len);

/**
 * Receives a frame.
 * @param frame unsigned 8-bit data buffer to copy the received frame into.
 * @param len number of bytes of the frame to read (the rest are discarded).
 * @return number of bytes read, 0 if there are no frames to receive.
 */
unsigned int enc28j60_Frame_Recv(unsigned char *frame, unsigned int len);

/*****************************************************************************/

#endif
