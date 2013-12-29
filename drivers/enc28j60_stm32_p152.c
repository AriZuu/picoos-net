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
#include <picoos-u.h>

#if NETCFG_DRIVER_ENC28J60_STM32_P152 > 0

#include "enc28j60.h"

#define CS_PIN GPIO_Pin_12

/**
 * Initializes the SPI pins, frequency, and SPI mode configuration.
 */
void enc28j60_spi_init(void) {
  /*
   * UEXT connections with STM32-P152:
   * 1 +3.3V
   * 2 GND
   * 3 LEDA = PC10
   * 4 WOL  = PC11
   * 5 INT  = PB6
   * 6 RST  = PB7
   * 7 MISO = SPI1 MISO = PE14
   * 8 MOSI = SPI1 MOSI = PE15
   * 9 SCK  = SPI1 SCK  = PE13
   * 10 CS  = PE12
   */

  GPIO_InitTypeDef  GPIO_InitStructure;
  SPI_InitTypeDef  SPI_InitStructure;

  // Enable clocks.

  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOE, ENABLE);
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);

  // Set CS to 1 (inactive)

  GPIO_WriteBit(GPIOE, CS_PIN, Bit_SET);

  // CS pin

  GPIO_InitStructure.GPIO_Pin = CS_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_40MHz;
  GPIO_Init(GPIOE, &GPIO_InitStructure);


  // SCK, MISO & MOSI pin

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_40MHz;
  GPIO_Init(GPIOE, &GPIO_InitStructure);

  GPIO_PinAFConfig(GPIOE, GPIO_PinSource13, GPIO_AF_SPI1);
  GPIO_PinAFConfig(GPIOE, GPIO_PinSource14, GPIO_AF_SPI1);
  GPIO_PinAFConfig(GPIOE, GPIO_PinSource15, GPIO_AF_SPI1);

  // Configure SPI bus

  SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
  SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
  SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
  SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
  SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
  SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
  SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4; //XXX 4
  SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
  SPI_InitStructure.SPI_CRCPolynomial = 7;
  SPI_Init(SPI1, &SPI_InitStructure);

  // Enable SPI1

  SPI_Cmd(SPI1, ENABLE);

#ifdef ENC28J60_USE_INTERRUPTS
  /* Setup the vectored interrupts for the EINT1 and EINT2 pins. */

  /* Set all interrupts to IRQ mode */
  VICIntSelect = 0x0;

  /* Enable the ENT1 interrupt and select the priority slot (15) */
  VICVectCntl1 = 0x20 | 15;
  /* Set the address of the interrupt to the C function handler */
  VICVectAddr1 = (unsigned long)ENC28J60_0_IRQ;

  /* Repeat the above for the EINT2 interrupt */
  VICVectCntl2 = 0x20 | 16;
  VICVectAddr2 = (unsigned long)ENC28J60_1_IRQ;
#endif
}

#if 0 // todo
/**
 * Enable the host microcontroller's pin interrupts that are connected to the
 * ethernet controller's INT pins.
 */
void enc28j60_LPC_Interrupts_Enable(void) {
  /* Enable EINT1 and EINT2 interrupts */
  VICIntEnable |= (0x00008000 | 0x0010000);
}

/**
 * Disable the host microcontroller's pin interrupts that are connected to the
 * ethernet controller's INT pins.
 */
void enc28j60_LPC_Interrupts_Disble(void) {
  /* Disable the EINT1 and EINT2 interrupts */
  VICIntEnClr |= (0x00008000 | 0x0010000);
}
#endif

/**
 * Selects an ENC28J60 chip by bringing the chip's CS low.
 * The ENC28J60 to talk to is determined by the global ENC28J60_Index variable.
 */
void enc28j60_spi_select(void) {

  GPIO_WriteBit(GPIOE, CS_PIN, Bit_RESET);
}

/**
 * Deselects an ENC28J60 chip by bringing the chip's CS high.
 * The ENC28J60 to talk to is determined by the global ENC28J60_Index variable.
 */
void enc28j60_spi_deselect(void) {

  GPIO_WriteBit(GPIOE, CS_PIN, Bit_SET);
}

/**
 * Writes a byte to the ENC28J60 through SPI.
 * The chip must be selected prior to this write.
 * @param data the 8-bit data byte to write.
 */
void enc28j60_spi_write(uint8_t data) {

  // Wait until transmit buffer is empty
  while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);

  // Send byte
  SPI_I2S_SendData(SPI1, data);

  // Wait receive (value is not needed, but it tells that operation is complete)
  while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);

  SPI_I2S_ReceiveData(SPI1);
}

/**
 * Explicitly read a byte from the ENC28J60 by first sending the dummy byte
 * 0x00.
 * The chip must be selected prior to this write.
 * @return the data read.
 */
uint8_t enc28j60_spi_read(void) {

  // Wait until transmit buffer is empty
  while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);

  // Send dummy byte
  SPI_I2S_SendData(SPI1, '\0');

  // Wait for receive
  while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);

  // Return received byte
  return SPI_I2S_ReceiveData(SPI1);
}

#endif
