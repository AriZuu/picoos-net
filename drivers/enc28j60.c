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

#include <picoos.h>
#include <picoos-net.h>

#if NETCFG_DRIVER_ENC28J60 > 0

#include "enc28j60.h"
//#include "uart.h"

/** Current ENC28J60 Ethernet Controller index (to select between the
 * two, or more, different ENC28J60 ethernet interfaces). */
uint8_t ENC28J60_Index;

/** The current bank for each ENC28J60 interface. */
static uint8_t ENC28J60_CurrentBank[ENC28J60_NUM_INTERFACES];
/** The Next Packet Pointer for each ENC28J60 interface */
static int16_t ENC28J60_NextPacketPointer[ENC28J60_NUM_INTERFACES];

/** Upper two bytes of the receive status vector, set after a frame
 * is successfully received. */
uint16_t ENC28J60_RecvStatus[ENC28J60_NUM_INTERFACES];

/* Full-duplex and half-duplex define's sanity check. */
#ifdef FULL_DUPLEX
#ifdef HALF_DUPLEX
#error "Error: More than one hardware flow control modes specified. Please specify only one! FULL_DUPLEX or HALF_DUPLEX."
#endif
#endif

#ifndef FULL_DUPLEX
#ifndef HALF_DUPLEX
#error "Error: No hardware flow control mode specify. Please specify one by defining FULL_DUPLEX or HALF_DUPLEX."
#endif
#endif

/**
 * Sends the specified read command and returns the response.
 * Performs the required dummy read for MAC/MII Registers.
 * See section 4.0 of the ENC28J60 datasheet. 
 * @param opcode the 3-bit opcode of the instruction.
 * @param address the 5-bit address/argument of the instruction.
 * @return the 8-bit data response to the command.
 */
uint8_t enc28j60_Command_Read(uint8_t opcode, uint8_t address) {
	uint8_t readData;

	enc28j60_spi_select();

	/* Section 4.2.1 of the ENC28J60 datasheet describes how
 	 * to perform the following read operation through SPI */

	/* The AND with the address mask is required because the
 	 * higher bits of the register address constants store the
 	 * bank the register is in and whether or not it is a MAC/PHY
 	 * register. */
	/* Write the read command:
 	 * The first 3 bits are the opcode, the last 5 bits are the
 	 * address. */
	enc28j60_spi_write(opcode | (address & ADDR_MASK));
	readData = enc28j60_spi_read();
	
  	/* See figure 4-4 in the ENC28J60 datasheet.
	 * If a dummy read is required for the MAC/PHY registers,
 	 * use enc28j60_spi_read() which first sends a dummy (0x00) byte. */
	/* If we are reading from a MAC/PHY register (the 8th bit, 0x80,
  	 * would be set to 1 in the register address constants in enc28j60.h),
  	 * then we need to perform a dummy read before getting the actual
  	 * data. */
	if (address & MAC_PHY_MASK) 
		readData = enc28j60_spi_read();

	enc28j60_spi_deselect();
	return readData;
}

/**
 * Sends the specified write command and the data to be written.
 * See section 4.0 of the ENC28J60 datasheet.
 * @param opcode the 3-bit opcode of the instruction.
 * @param address the 5-bit address/argument of the instruction.
 * @param data the 8-bit data that follows the command.
 */
void enc28j60_Command_Write(uint8_t opcode, uint8_t address, uint8_t data) {
	enc28j60_spi_select();

	/* See 4.2.3 and figure 4-5 of the ENC28J60 datasheet */
	/* See enc28j60_Command_Read() on the significance of the opcode and
 	 * adress variables. */
	enc28j60_spi_write(opcode | (address & ADDR_MASK));
	enc28j60_spi_write(data);

	enc28j60_spi_deselect();
}

/**
 * Reads the ENC28J60 buffer memory for 'len' bytes into the passed 'buffer'.
 * See section 4.2.2 of the ENC28J60 datasheet.
 * @param buffer the unsigned 8-bit array to store the read data.
 * @param len the number of bytes to read.
 */
void enc28j60_Buffer_Read(uint8_t *buffer, uint16_t len) {
	enc28j60_spi_select();

	/* See 4.2.2 of the ENC28J60 datasheet */
	enc28j60_spi_write(ENC28J60_READ_BUF_MEM);
	/* Continue to read, for the number of bytes we want */
	for (; len > 0; len--) 
		*buffer++ = enc28j60_spi_read();

	enc28j60_spi_deselect();
}

/**
 * Writes the ENC28J60 buffer memory 'len' bytes of the passed 'buffer'.
 * See section 4.2.4 of the ENC28J60 datasheet.
 * @param buffer the unsigned 8-bit array of data to write to the ENC28J60
 * @param len the number of bytes to write.
 */
void enc28j60_Buffer_Write(uint8_t *buffer, uint16_t len) {
	enc28j60_spi_select();

	/* See 4.2.4 and figure 4-6 of the ENC28J60 datasheet */
	enc28j60_spi_write(ENC28J60_WRITE_BUF_MEM);
	/* Continue to write len bytes */
	for (; len > 0; len--)
		enc28j60_spi_write(*buffer++);

	/* When CS goes high, ENC28J60 will know we're done writing */
	enc28j60_spi_deselect();
}

/**
 * Reads and returns a single byte from the ENC28J60 buffer memory.
 * See section 4.2.2 of the ENC28J60 datasheet.
 * @return the byte read.
 */
uint8_t enc28j60_Buffer_ReadByte(void) {
	uint8_t data;

	enc28j60_spi_select();

	/* See 4.2.2 of the ENC28J60 datasheet */
	enc28j60_spi_write(ENC28J60_READ_BUF_MEM);
	data = enc28j60_spi_read();
	
	enc28j60_spi_deselect();
	return data;
}

/**
 * Writes a single byte to the ENC28J60 buffer memory.
 * See section 4.2.4 of the ENC28J60 datasheet.
 * @param data the byte to write.
 */
void enc28j60_Buffer_WriteByte(uint8_t data) {
	enc28j60_spi_select();
	
	/* See 4.2.4 and figure 4-6 of the ENC28J60 datasheet */
	enc28j60_spi_write(ENC28J60_WRITE_BUF_MEM);
	enc28j60_spi_write(data);

	enc28j60_spi_deselect();
}

/**
 * Reads the data stored at the specified ENC28J60 register.
 * Changes banks if necessary to access the specified register.
 * Use enc28j60_PHY_Read() to read PHY registers.
 * See section 4.2.1 of the ENC28J60 datasheet.
 * @param address the address of the register to read.
 * @return the byte read from the specified register.
 */
uint8_t enc28j60_Register_Read(uint8_t address) {
	/* Change banks to the one specified by the address */
	enc28j60_SelectBank(address);
	/* Perform the actual read */
	return enc28j60_Command_Read(ENC28J60_READ_CTRL_REG, address);
}

/**
 * Writes a byte to the specified ENC28J60 register.
 * Changes banks if necessary to access the specified register.
 * Use enc28j60_PHY_Write() to write to PHY registers.
 * See section 4.2.3 of the ENC28J60 datasheet.
 * @param address the address of the register to write to.
 * @param data the data byte to write to the register.
 */
void enc28j60_Register_Write(uint8_t address, uint8_t data) {
	/* Change banks to the one specified by the address */
	enc28j60_SelectBank(address);
	/* Perform the actual write */
	enc28j60_Command_Write(ENC28J60_WRITE_CTRL_REG, address, data);
}

/**
 * Performs a bitfield set on the specified ENC28J60 register.
 * Changes banks if necessary to access the specified register.
 * See section 4.2.5 of the ENC28J60 datasheet.
 * @param address the address of the register whose bits will be set.
 * @param bits the bits to set in the register.
 */
void enc28j60_Bitfield_Set(uint8_t address, uint8_t bits) {
	/* Change banks to the one specified by the address */
	enc28j60_SelectBank(address);
	/* Perform the actual bit set */
	enc28j60_Command_Write(ENC28J60_BIT_FIELD_SET, address, bits);	
}

/**
 * Performs a bitfield clear on the specified ENC28J60 register.
 * Changes banks if necessary to access the specified register.
 * See section 4.2.6 of the ENC28J60 datasheet.
 * @param address the address of the register whose bits will be cleared.
 * @param bits the bits to clear in the register.
 */
void enc28j60_Bitfield_Clear(uint8_t address, uint8_t bits) {
	/* Change banks to the one specified by the address */
	enc28j60_SelectBank(address);
	/* Perform the actual bit clear */
	enc28j60_Command_Write(ENC28J60_BIT_FIELD_CLR, address, bits);	
}

/**
 * Changes banks to enable access to the specified register.
 * The bank is stored in the 6th and 7th bits of the passed register address.
 * See section 3.1 of the ENC28J60 datasheet.
 * @param address the register address that banks must be changed for.
 */
void enc28j60_SelectBank(uint8_t address) {
	/* If we've already selected this bank, then bail out. */
	if ((address & BANK_MASK) == ENC28J60_CurrentBank[ENC28J60_Index])
		return;

 	/* See 3.1 of the ENC28J60 datasheet */
	
	/* ECON1 is a available to all banks, so we don't need to
 	 * change banks to change banks (impossible anyway). */

	/* Clear the current two bank select bits in ECON1 */ 
	enc28j60_Command_Write(ENC28J60_BIT_FIELD_CLR, ECON1, (ECON1_BSEL0|ECON1_BSEL1));
	/* Now set the required bank, as specified in the higher bits 
 	 * (BANK_MASK) in the address passed. */
	enc28j60_Command_Write(ENC28J60_BIT_FIELD_SET, ECON1, (address & BANK_MASK)>>5);

	/* Set our current bank variable */
	ENC28J60_CurrentBank[ENC28J60_Index] = address & BANK_MASK;
}

/**
 * Reads from the specified 16-bit ENC28J60 PHY register.
 * See section 3.3.1 of the ENC28J60 datasheet.
 * @param address the address of the PHY register.
 * @return the 16-bit data read from the PHY register.
 */
uint16_t enc28j60_PHY_Read(uint8_t address) {
	uint16_t readData;
	
	/* See section 3.3.1 of the ENC28J60 datasheet */

	/* 1. Set the MII Register Address */
	enc28j60_Register_Write(MIREGADR, address);	
	/* 2. Set the MII Read bit of the MICMD Register */
	enc28j60_Bitfield_Set(MICMD, MICMD_MIIRD);

	/* 3. Wait until the PHY register read completes */
	delay_us(11);
	while (enc28j60_Register_Read(MISTAT) & MISTAT_BUSY)
		;

	/* 4. Clear the MICMD.MIIIRD bit */
	enc28j60_Bitfield_Clear(MICMD, MICMD_MIIRD);

	/* 5. Read the high and low MII register data bytes into readData */
	readData = enc28j60_Register_Read(MIRDL);
	readData |= enc28j60_Register_Read(MIRDH)<<8;

	return readData;
}

/**
 * Writes to the specified 16-bit ENC28J60 PHY register.
 * See section 3.3.2 of the ENC28J60 datasheet.
 * @param address the address of the PHY register.
 * @param data the 16-bit data to write to the PHY register.
 */ 
void enc28j60_PHY_Write(uint8_t address, uint16_t data) {
	/* See section 3.3.2 of the ENC28J60 datasheet */
	
	/* 1. Set the MII Register Address */
	enc28j60_Register_Write(MIREGADR, address);	
	/* 2-3. Write the data to the MIWR register */
	enc28j60_Register_Write(MIWRL, (uint8_t)(data));
	enc28j60_Register_Write(MIWRH, (uint8_t)((data>>8)));

	/* 3. Wait until the PHY register write completes */
	delay_us(11);
	while (enc28j60_Register_Read(MISTAT) & MISTAT_BUSY)
		;
}

/**
 * Performs a complete system reset of the ENC28J60, including the wait for the
 * ethernet controller to initialize.
 * See section 11.2 of the ENC28J60 datasheet.
 */
void enc28j60_System_Reset(void) {
	/* See section 11.2 of the ENC28J60 datasheet */

	/* Send the soft reset instruction */
	enc28j60_spi_select();
	enc28j60_spi_write(ENC28J60_SOFT_RESET);
	enc28j60_spi_deselect();	
	
	/* Wait until all PHY registers have been reset */
	delay_us(50);
	
	/* We can't poll ESTAT_CLKRDY because Microchip screwed up.
 	 * See section 2 of ENC28j60 Rev. B1 Silicon Errata. 
 	 * Instead we just wait 1ms. */
	delay_ms(1);

	/* Wait until the system reset has completed 
 	 * See section 6.4 of the ENC28J60 datasheet */
	//while (!(enc28j60_Register_Read(ESTAT) & ESTAT_CLKRDY))
	//	;
}

/**
 * Enable ENC28J60's global interrupts so the ethernet controller can raise
 * an interrupt on its INT pin.
 */
void enc28j60_Enable_Global_Interrupts(void) {
	/* Enable global interrupts */
	enc28j60_Bitfield_Set(EIE, EIE_INTIE);
}

/**
 * Disable ENC28J60's global interrupts so the ethernet controller will not
 * raise interrupts on its INT pin.
 */
void enc28j60_Disable_Global_Interrupts(void) {
	/* Disable global interrupts */
	enc28j60_Bitfield_Clear(EIE, EIE_INTIE);
}

/**
 * Complete initializes the ENC28J60 in Full-Duplex operation with the
 * specific configuration details defined in the driver header file.
 */
void enc28j60_Init(void) {
	uint16_t temp;

	/* See section 6.0 of the ENC28J60 datasheet */
	/* Do a complete system reset (this also will reset all of the ENC28J60
 	 * registers to their defaults). */
	enc28j60_System_Reset();
	
	/* On reset, ECON1 is initialized to 0x00, so the current bank is 0. */
	ENC28J60_CurrentBank[ENC28J60_Index] = 0x00;

	/* --- 6.1 Initialize the Receive Buffer --- */

	/* Set the Receive Buffer Start pointer to the beginning of the
 	 * defined receive buffer memory. */
	enc28j60_Register_Write(ERXSTL, (uint8_t)(RX_BUFFER_START)); 
	enc28j60_Register_Write(ERXSTH, (uint8_t)(RX_BUFFER_START>>8)); 
	/* Set the Receive Buffer End pointer to the end of the defined
 	 * receive buffer memory. */
	enc28j60_Register_Write(ERXNDL, (uint8_t)(RX_BUFFER_END)); 
	enc28j60_Register_Write(ERXNDH, (uint8_t)(RX_BUFFER_END>>8)); 
	/* Set the our Next Packet Pointer to the beginning of this memory
 	 * too, since it is where the first packet will be received in */
	ENC28J60_NextPacketPointer[ENC28J60_Index] = RX_BUFFER_START;
	/* Set the Receive Buffer pointer to the beginning of the receive
 	 * buffer as well. */
	enc28j60_Register_Write(ERXRDPTL, (uint8_t)(RX_BUFFER_START)); 
	enc28j60_Register_Write(ERXRDPTH, (uint8_t)(RX_BUFFER_START>>8)); 
		
	/* 6.2 Initialize the Transmit Buffer */
	/* Set the Transmit Buffer Start pointer to the beginning of the
 	 * defined transmit buffer memory. */
	enc28j60_Register_Write(ETXSTL, (uint8_t)(TX_BUFFER_START)); 
	enc28j60_Register_Write(ETXSTH, (uint8_t)(TX_BUFFER_START>>8));

	/* --- 6.5 Set the MAC Initialization Settings --- */

#ifdef FULL_DUPLEX
	/* 1. Follow the suggested configuration by setting the following bits:
 	 * MARXEN: "enable the MAC to receive frames", TXPAUS and RXPAUS 
 	 * "to allow IEEE defined flow control" for full duplex. */
	enc28j60_Register_Write(MACON1, MACON1_MARXEN|MACON1_TXPAUS|MACON1_RXPAUS);
	
	/* 2. Again, follow the suggested configuration with:
 	 * PADCFG: "automatic padding to at least 60 bytes", TXCRCEN: "always
 	 * append a valid CRC", FRMLNEN: "enable frame length status reporting",
 	 * FULDPX: set to operate in full-duplex mode 
 	 */
	enc28j60_Bitfield_Set(MACON3, MACON3_PADCFG0|MACON3_TXCRCEN|MACON3_FULDPX);
	
	/* 3. We don't need to mess with MACON4 because it's bits apply only to
 	 * half-duplex. */
#endif

#ifdef HALF_DUPLEX
	/* 1. Same suggested configuration as Full-Duplex, but without the
 	 * TXPAUS and RXPAUS flow-control bits (not needed for half-duplex). */
	enc28j60_Register_Write(MACON1, MACON1_MARXEN);

	/* 2. Same suggested configuration as Full-Duplex, but without the
 	 * FULDPX bit. */
	enc28j60_Bitfield_Set(MACON3, MACON3_PADCFG0|MACON3_TXCRCEN);
#endif

	/* 4. Set the maximum frame length permitted to be received/transmitted
 	 */
	enc28j60_Register_Write(MAMXFLL, (uint8_t)(MAX_FRAME_LEN));
	enc28j60_Register_Write(MAMXFLH, (uint8_t)(MAX_FRAME_LEN>>8));

#ifdef FULL_DUPLEX
	/* 5. Set the default setting for the Back-to-Back Inter-Packet Gap 
 	 * Register */
	enc28j60_Register_Write(MABBIPG, 0x15);
	
	/* 6. Set the default settings for the Non-Back-to-Back Inter-Packet 
 	 * Gap Register low and high bytes */
	enc28j60_Register_Write(MAIPGL, 0x12);
	
	/* Ignore steps 7-8 since they're for half-duplex mode. */	
#endif
	
#ifdef HALF_DUPLEX
	/* 5. Set the default setting for the Back-to-Back Inter-Packet Gap 
 	 * Register */
	enc28j60_Register_Write(MABBIPG, 0x12);
	
	/* 6-7. Set the default settings for the Non-Back-to-Back Inter-Packet 
 	 * Gap Register low and high bytes */
	enc28j60_Register_Write(MAIPGL, 0x12);
	enc28j60_Register_Write(MAIPGL, 0x0C);

	/* 8. Defaults are used for the MALCON1 and MALCON2 registers. */
#endif

	/* 9. Set the MAC address */
	enc28j60_Register_Write(MAADR5, ENC28J60_MAC0);
	enc28j60_Register_Write(MAADR4, ENC28J60_MAC1);
	enc28j60_Register_Write(MAADR3, ENC28J60_MAC2);
	enc28j60_Register_Write(MAADR2, ENC28J60_MAC3);
	enc28j60_Register_Write(MAADR1, ENC28J60_MAC4);
	enc28j60_Register_Write(MAADR0, ENC28J60_MAC5);

	/* --- 6.6 PHY Configuration --- */

#ifdef FULL_DUPLEX
	/* 1. The LEDB configuration in my schematic is set to source current
 	 * from the ethernet controller, so the ENC28J60 defaults to 
 	 * half-duplex. Since we want full-duplex, we have to set the PDPXMD 
 	 * bit of PHCON1 manually. MACON3 has already been set with FULDPX to
 	 * match this configuration. */
	/* Since PHCON1 is a 16 bit register, and PDPXMD is a bit in the higher
 	 * byte (9th bit), we must read the current value of the PHCON1
 	 * register, set the PDPXMD bit, and write it all back to PHCON1.
 	 * This is because we cannot modify individual bits directly with the 
 	 * ENC28J60's 16-bit PHY registers. */
	temp = enc28j60_PHY_Read(PHCON1);
	temp |= PHCON1_PDPXMD;
	enc28j60_PHY_Write(PHCON1, temp);
#endif

#ifdef HALF_DUPLEX
	/* ENC28J60 silicon B1-B5 errata notes that the ethernet controller may
 	 * not reliably detect the LEDB configuration to set the default half
 	 * or full duplex modes. So let's half-duplex mode manually as well,
 	 * by clearing the PDPXMD bit of PHCON1. */
	temp = enc28j60_PHY_Read(PHCON1);
	temp &= ~PHCON1_PDPXMD;
	enc28j60_PHY_Write(PHCON1, temp);
#endif	
	
	/* 2. Let's leave the LED configuration (PHLCON) to the defaults. */

	/* Enable the filters specified in the driver header file */
	enc28j60_Register_Write(ERXFCON, FILTER_PROMISC);

	/* --- 7.2.1 Enabling Frame Reception ---*/

#ifdef	ENC28J60_USE_INTERRUPTS
	/* 1-2. Enable interrupts */
	/* Enable the Packet Pending Interrupt, which will fire when new packets
 	 * arrive, and the RX Error Interrupt, which will fire if a receive
 	 * buffer overflow occurs (too many packets, too little attention to
 	 * handle them).. */
	enc28j60_Bitfield_Set(EIE, (EIE_PKTIE|EIE_RXERIE));
	enc28j60_Enable_Global_Interrupts();
#endif

	/* 3. Enable frame reception */
	enc28j60_Bitfield_Set(ECON1, ECON1_RXEN);
}

/**
 * Sends a frame.
 * CRC is calculated by the ENC28J60, so it need not be included in the frame
 * data.
 * @param frame unsigned 8-bit array of the frame data.
 * @param len length of the frame data with in the data byte array.
 * @return number of bytes sent (always 'len'), 0 if the frame was larger than
 *  the maximum frame length supported.
 */
int enc28j60_Frame_Send(uint8_t *frame, uint32_t len) {
	/* See sections 3.2.2 and 7.1 of the ENC28J60 datasheet */

	/* Exit if the frame is too big for us */
	if (len > MAX_FRAME_LEN)
		return 0; 

	/* Set the Buffer Write Pointer to the beginning of the transmit 
 	 * buffer */
	enc28j60_Register_Write(EWRPTL, (uint8_t)(TX_BUFFER_START));
	enc28j60_Register_Write(EWRPTH, (uint8_t)(TX_BUFFER_START>>8));
	/* Set the Transmit Buffer End (ETXND) pointer to the end of the frame
 	 * data */
	enc28j60_Register_Write(ETXNDL, (uint8_t)(TX_BUFFER_START+len));
	enc28j60_Register_Write(ETXNDH, (uint8_t)((TX_BUFFER_START+len)>>8));
	
	/* First write the per-packet control byte, as specified by figure 7-1
 	 * of the ENC28J60 datasheet */
	enc28j60_Command_Write(ENC28J60_WRITE_BUF_MEM, 0, PER_PACKET_CONTROL);

	/* Next write all bytes of the frame */
	enc28j60_Buffer_Write(frame, len);

	/* Start the transmission by setting the TXRTS bit of ECON1 */
	enc28j60_Bitfield_Set(ECON1, ECON1_TXRTS);

	/* Wait until the TXRTS bit of ECON1 clears, meaning transmission is
 	 * complete */
	while (enc28j60_Register_Read(ECON1) & ECON1_TXRTS)
		;

	return len;
}

/**
 * Receives a frame.
 * @param frame unsigned 8-bit data buffer to copy the received frame into.
 * @param len number of bytes of the frame to read (the rest are discarded).
 * @return number of bytes read, 0 if there are no frames to receive.
 */
unsigned int enc28j60_Frame_Recv(unsigned char *frame, unsigned int len) {
	unsigned int frameLen;
	int rxError = 0;

	/* See section 3.2.1 and 7.2.3 of the ENC28J60 datasheet */
	
	/* Check for an RX error in the interrupt flag register,
	 * if there is one, we need to remember to clear the REXERIF
	 * flag after we are done handling it (section 12.1.2). */
	if (enc28j60_Register_Read(EIR) & EIR_RXERIF) {
		/* Unfortunately the chip doesn't seem to fare too well with
 		 * reading the ethernet buffer and clearing the RX error flag
 		 * bit. I found that EPKTCNT was often to 0 when an RX error
 		 * occured, which made it difficult to read the pending packets.
 		 * The datasheet indicates that the pending packets should be
 		 * read (more importantly, advancing the ERDPT pointer to free
 		 * the memory), and that the receive error flag should be
 		 * cleared, however I found that even after this process is
 		 * complete, the ethernet controller continued to lock up in
 		 * more RX errors.
 		 * For lack of a better solution, I reset the entire ethernet
 		 * controller, which allows the ethernet controller to receive
 		 * properly after a short delay (1ms). For the packets lost,
 		 * the other end will send retransmissions.
 		 */
		rxError = 1;
		enc28j60_Init();
		return 0;
	}
	
	/* Bail out if the packet count register reports there are no new 
 	 * packets to read in. */
	if (enc28j60_Register_Read(EPKTCNT) == 0x00)
		return 0;

	/* Set the Buffer Read Pointer to the location of the next packet */
	enc28j60_Register_Write(ERDPTL, (uint8_t)(ENC28J60_NextPacketPointer[ENC28J60_Index]));
	enc28j60_Register_Write(ERDPTH, (uint8_t)(ENC28J60_NextPacketPointer[ENC28J60_Index]>>8));
	/* The first two bytes of the packet buffer are the Next Packet Pointer,
 	 * read them into our Next Packet Pointer variable */
	ENC28J60_NextPacketPointer[ENC28J60_Index] = enc28j60_Buffer_ReadByte();
	ENC28J60_NextPacketPointer[ENC28J60_Index] |= enc28j60_Buffer_ReadByte()<<8;
	/* The next four bytes of the packet buffer is the Receive Status 
 	 * Vector. The lower two bytes of this is the frame length. */
	frameLen = enc28j60_Buffer_ReadByte();
	frameLen |= enc28j60_Buffer_ReadByte()<<8;
	/* The last two bytes of the Receive Status Vector are various receive
	 * statistics. */
	ENC28J60_RecvStatus[ENC28J60_Index] = enc28j60_Buffer_ReadByte();
	ENC28J60_RecvStatus[ENC28J60_Index] |= enc28j60_Buffer_ReadByte()<<8;

	/* Subtract 4 from the frame length so we can ignore the last 4 CRC 
 	 * bytes of the frame */
	frameLen -= 4;

	/* Take the lesser of the length of the frame we received and the 
 	 * desired frame length passed into this function. */
	if (len < frameLen)
		frameLen = len;
	
	/* Now actually read the frame */
	/* We don't need to worry about the ERXRDPT pointer with the buffer 
 	 * wrapping will be taken care of since the AUTOINC bit of ECON2 is set
 	 * by default on reset. With this set, the ERXRDPT pointer will
 	 * automatically be incremented and wrapped around the read buffer as
 	 * we read the frame data. */
	enc28j60_Buffer_Read(frame, frameLen);

	/* See section 7.2.4 of the ENC28J60 datasheet */

	/* "The receive hardware may corrupt the circular receive buffer
 	 * (including the Next Packet Pointer and receive status vector fields)
 	 * when an even value is programmed into the ERXRDPTH:ERXRDPTL 
 	 * registers" - Section 13 - ENC28J60 B1 Silicon Errata.
 	 * Thus, we must ensure that the Next Packet Pointer is odd. *
	ENC28J60_NextPacketPointer[ENC28J60_Index]--;
	* Implement the odd number & bounds check described in the errata *
	if (ENC28J60_NextPacketPointer[ENC28J60_Index] < RX_BUFFER_START ||
	  ENC28J60_NextPacketPointer[ENC28J60_Index] > RX_BUFFER_END) {
		ENC28J60_NextPacketPointer[ENC28J60_Index] = RX_BUFFER_END;
	}*/

	/* Update the Receive Buffer Read Pointer to the Next Packet Pointer so
 	 * we can free the memory we read this frame from */
	enc28j60_Register_Write(ERXRDPTL, (uint8_t)(ENC28J60_NextPacketPointer[ENC28J60_Index]));
	enc28j60_Register_Write(ERXRDPTL, (uint8_t)(ENC28J60_NextPacketPointer[ENC28J60_Index]>>8));

	/* Decrement the EPKTCNT to indicate that the packet has been received 
 	 * and to clear the PKTIF flag */
	enc28j60_Bitfield_Set(ECON2, ECON2_PKTDEC);
	
	/* If there was an RX error, we need to remember to clear the REXERIF
	 * flag */
	if (rxError == 1)
		enc28j60_Bitfield_Clear(EIR, EIR_RXERIF);

	return frameLen;	
}

#endif
