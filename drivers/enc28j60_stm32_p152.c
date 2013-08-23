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

#if 0
/* For Microcontroller Hardware Definitions */
#include "lpc214x.h"
#include "mcuconfig.h"

extern void ENC28J60_0_IRQ(void) __attribute__((interrupt("FIQ")));
extern void ENC28J60_1_IRQ(void) __attribute__((interrupt("FIQ")));

/* A few important SPI configuration bits in the SPI Control Register (S0SPCR)
 * See LPC2148 12.4.1 */
#define SPCR_CPHA (1<<3)    /* Clock Phase */
#define SPCR_CPOL (1<<4)    /* Clock Polarity */
#define SPCR_MSTR (1<<5)    /* Master=1/Slave=0 */
#define SPCR_SPIE (1<<7)    /* SPI Interrupt Enable */

/* Important status bits in the SPI Status Register (S0SPSR)
 * See LPC2148 12.4.2 */
#define SPSR_ABRT (1<<3)    /* Slave Abort */
#define SPSR_MODF (1<<4)    /* Mode Fault */
#define SPSR_ROVR (1<<5)    /* Read Overrun */
#define SPSR_WCOL (1<<6)    /* Write Collision */
#define SPSR_SPIF (1<<7)    /* Transfer Complete Flag */

/* Chip select pins of the ENC28J60s on port 0 */
#define ENC28J60_0_CS 2
#define ENC28J60_1_CS 10

/* Interrupt pins of the ENC28J60s on port 0 */
#define ENC28J60_0_INT  3
#define ENC28J60_1_INT  7

#endif

#define CS_PIN GPIO_Pin_12
// LED #define CS_PIN GPIO_Pin_10

/**
 * Delays the specified number of milliseconds.
 * @param ms milliseconds to delay.
 */
void delay_ms(uint32_t ms) {
  posTaskSleep(MS(ms));
}

/**
 * Delays the specified number of microseconds (approximately).
 * This approximate delay loop was hand tuned and is definitely not accurate.
 * @param us microseconds to delay.
 */
void delay_us(uint32_t us) {
  while (us-- > 0)
    __asm("nop; nop;");
}


/**
 * Initializes the SPI pins, frequency, and SPI mode configuration.
 */
void enc28j60_spi_init(void) {
  int i;
  uint8_t dummyData;
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

#if 0
  /* First configure the pins for SPI (LPC2148: 7.4.1)*/
  /* PINSEL0 bits 9:8, 11:10, and 13:12 need to be 01
   * to set SCK0, MISO0, and MOSI0 for SPI use.
   */
  PINSEL0 |= (1<<8)|(1<<10)|(1<<12);
  PINSEL0 &= ~((1<<9)|(1<<11)|(1<<13));
  /* Also configure the two interrupt pins as External Interrupt Function
   * pins */
  PINSEL0 |= (1<<6)|(1<<7)|(1<<14)|(1<<15);

  /* Set the chip select pins (GPIO) to output */
  IODIR0 |= (1<<ENC28J60_0_CS)|(1<<ENC28J60_1_CS);
  /* Set the interrupt pins to their appropriate EXTINTx functions */
  /* Bring the chip select pins high since we're not talking yet */
  IOSET0 |= (1<<ENC28J60_0_CS)|(1<<ENC28J60_1_CS);

  /* Set the SPI clock rate (LPC2148: 12.4.4)
   * The ENC28J60 supports SPI clock rates up to 20MHz (see section 1.0
   * of datasheet).
   * Set the SPI Clock Counter Register to the division factor by
   * dividing the processor clock rate by the desired ENC28J60 clock
   * rate.
   */
  S0SPCCR = (CCLK/VPB_DIV)/ENC28J60_CLOCK;

  /* Setup the SPI Control Register (12.4.1)
   *  ENC28J60 SPI specifications (see section 4.1 of the datasheet):
    - mode 0,0 (CPOL=0, CPHA=0)
    - 8 bits per transfer, MSB first
    - LPC2148 obviously is the master.
    The only bit we need to set in S0SPCR is the Master bit,
    everything else can remain default (0's).
   */
  S0SPCR = SPCR_MSTR;

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

  /* Clear the current data in the SPI receive buffer */
  for (i = 0; i < 8; i++)
    dummyData = S0SPDR;
#endif
}

#if 0
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

#if 0
  if (ENC28J60_Index == 0)
    IOCLR0 |= (1<<ENC28J60_0_CS);
  else if (ENC28J60_Index == 1)
    IOCLR0 |= (1<<ENC28J60_1_CS);
#endif
}

/**
 * Deselects an ENC28J60 chip by bringing the chip's CS high.
 * The ENC28J60 to talk to is determined by the global ENC28J60_Index variable.
 */
void enc28j60_spi_deselect(void) {

  GPIO_WriteBit(GPIOE, CS_PIN, Bit_SET);

#if 0
  if (ENC28J60_Index == 0)
    IOSET0 |= (1<<ENC28J60_0_CS);
  else if (ENC28J60_Index == 1)
    IOSET0 |= (1<<ENC28J60_1_CS);
#endif
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
#if 0
  S0SPDR = data;
  /* Wait until the transfer complete flag clears before
   * we write new data. */
  while (!(S0SPSR & SPSR_SPIF))
    ;
#endif
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
#if 0
  /* Send a dummy byte */
  S0SPDR = 0x00;
  /* Wait until the transfer complete flag clears */
  while (!(S0SPSR & SPSR_SPIF))
    ;
  /* Read the data */
  return S0SPDR;
#endif
}

#endif
