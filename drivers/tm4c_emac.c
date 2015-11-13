/*
 * Copyright (c) 2014, Ari Suutari <ari@stonepile.fi>.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote
 *     products derived from this software without specific prior written
 *     permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT,  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This file is partially derived from enet_uip.c revision 2.1.0.12573 of
 * the EK-TM4C1294XL Firmware Package, which has following copyright:
 *
 * Copyright (c) 2013-2014 Texas Instruments Incorporated.  All rights reserved.
 * Software License Agreement
 *
 * Texas Instruments (TI) is supplying this software for use solely and
 * exclusively on TI's microcontroller products. The software is owned by
 * TI and/or its suppliers, and is protected under applicable copyright
 * laws. You may not combine this software with "viral" open-source
 * software in order to form a larger program.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
 * NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
 * NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
 * CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
 * DAMAGES, FOR ANY REASON WHATSOEVER.
 */

#include <picoos.h>
#include <picoos-net.h>
#include <string.h>

#if NETCFG_DRIVER_TM4C1294 > 0

#include <port_irq.h>
#include "inc/hw_emac.h"
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/debug.h"
#include "driverlib/emac.h"
#include "driverlib/flash.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"

#include "tm4c_emac.h"

static void initDescriptors(void);

/*
 * Ethernet DMA descriptors. Although uIP uses a single buffer,
 * the MAC hardware needs a minimum of 3 receive descriptors to operate.
 */

#define NUM_TX_DESCRIPTORS 3
#define NUM_RX_DESCRIPTORS 3

tEMACDMADescriptor rxDescriptor[NUM_TX_DESCRIPTORS];
tEMACDMADescriptor txDescriptor[NUM_RX_DESCRIPTORS];

uint32_t rxDescIndex;
uint32_t txDescIndex;

/*
 * Transmit and receive buffers.
 */

#define RX_BUFFER_SIZE 1536
#define TX_BUFFER_SIZE 1536
uint8_t rxBuffer[RX_BUFFER_SIZE];
uint8_t txBuffer[TX_BUFFER_SIZE];


void tivaEmacInit()
{
  struct uip_eth_addr ethAddr;
  uint32_t user0, user1;

  /*
   * Read MAC address from flash.
   */
  FlashUserGet(&user0, &user1);
  P_ASSERT("MAC address not programmed", user0 != 0xffffffff && user1 != 0xffffffff);

  /*
   * Convert the 24/24 split MAC address from NV ram into a 32/16 split MAC
   * address needed to program the hardware registers, then program the MAC
   * address into the Ethernet Controller registers.
   */
  ethAddr.addr[0] = ((user0 >> 0) & 0xff);
  ethAddr.addr[1] = ((user0 >> 8) & 0xff);
  ethAddr.addr[2] = ((user0 >> 16) & 0xff);
  ethAddr.addr[3] = ((user1 >> 0) & 0xff);
  ethAddr.addr[4] = ((user1 >> 8) & 0xff);
  ethAddr.addr[5] = ((user1 >> 16) & 0xff);
  nosPrintf("MAC address is %02X-%02X-%02X-%02X-%02X-%02X\n",
            ethAddr.addr[0],
            ethAddr.addr[1],
            ethAddr.addr[2],
            ethAddr.addr[3],
            ethAddr.addr[4],
            ethAddr.addr[5]);

  /*
   * Set the local MAC address (for uIP).
   */
  uip_setethaddr(ethAddr);

  /*
   * Enable and reset the Ethernet modules.
   */
  SysCtlPeripheralEnable(SYSCTL_PERIPH_EMAC0);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_EPHY0);
  SysCtlPeripheralReset(SYSCTL_PERIPH_EMAC0);
  SysCtlPeripheralReset(SYSCTL_PERIPH_EPHY0);

  /*
   * Wait for the MAC to be ready.
   */
  nosPrint("Waiting for MAC to be ready...\n");
  while (!SysCtlPeripheralReady(SYSCTL_PERIPH_EMAC0)) {
  }

  /*
   * Configure for use with the internal PHY.
   */
  EMACPHYConfigSet(EMAC0_BASE, (EMAC_PHY_TYPE_INTERNAL |
                                    EMAC_PHY_INT_MDIX_EN |
                                    EMAC_PHY_AN_100B_T_FULL_DUPLEX));
  nosPrint("MAC ready.\n");

  /*
   * Reset the MAC.
   */
  EMACReset(EMAC0_BASE);

  /*
   * Initialize the MAC and set the DMA mode.
   */
  EMACInit(EMAC0_BASE, SystemCoreClock, EMAC_BCONFIG_MIXED_BURST | EMAC_BCONFIG_PRIORITY_FIXED, 4, 4, 0);

  /*
   * Set MAC configuration options.
   */
  EMACConfigSet(EMAC0_BASE,
                (EMAC_CONFIG_FULL_DUPLEX |
                 EMAC_CONFIG_7BYTE_PREAMBLE |
                 EMAC_CONFIG_IF_GAP_96BITS |
                 EMAC_CONFIG_USE_MACADDR0 |
                 EMAC_CONFIG_SA_FROM_DESCRIPTOR |
                 EMAC_CONFIG_BO_LIMIT_1024),
                (EMAC_MODE_RX_STORE_FORWARD |
                 EMAC_MODE_TX_STORE_FORWARD |
                 EMAC_MODE_TX_THRESHOLD_64_BYTES | EMAC_MODE_RX_THRESHOLD_64_BYTES),
                0);

  /*
   * Initialize the Ethernet DMA descriptors.
   */
  initDescriptors();

  /*
   * Program the hardware with its MAC address (for filtering).
   */
  EMACAddrSet(EMAC0_BASE, 0, (uint8_t *) &ethAddr);

  /*
   * Set MAC filtering options.  We receive all broadcast and multicast
   * packets along with those addressed specifically for us.
   */
  EMACFrameFilterSet(EMAC0_BASE,
                     (EMAC_FRMFILTER_SADDR |
                      EMAC_FRMFILTER_PASS_MULTICAST |
                      EMAC_FRMFILTER_PASS_NO_CTRL));

  /*
   * Clear any pending interrupts.
   */
  EMACIntClear(EMAC0_BASE, EMACIntStatus(EMAC0_BASE, false));


  /*
   * Enable the Ethernet MAC transmitter and receiver.
   */
  EMACTxEnable(EMAC0_BASE);
  EMACRxEnable(EMAC0_BASE);

  /*
   * Enable the Ethernet interrupt.
   */
 // IntEnable(INT_EMAC0);
  NVIC_ClearPendingIRQ(EMAC0_IRQn);
  NVIC_SetPriority(EMAC0_IRQn, PORT_PENDSV_PRI - 2);
  NVIC_EnableIRQ(EMAC0_IRQn);


  /*
   * Enable the Ethernet RX Packet interrupt source.
   */
  EMACIntEnable(EMAC0_BASE, EMAC_INT_RECEIVE);

  /*
   * Mark the first receive descriptor as available to the DMA to start
   * the receive processing.
   */
  rxDescriptor[rxDescIndex].ui32CtrlStatus |= DES0_RX_CTRL_OWN;
}

/*
 * Initialize the transmit and receive DMA descriptors.  We apparently need
 * a minimum of 3 descriptors in each chain.  This is overkill since uIP uses
 * a single, common transmit and receive buffer so we tag each descriptor
 * with the same buffer and will make sure we only hand the DMA one descriptor
 * at a time.
*/
static void initDescriptors()
{
  uint32_t loop;

  /*
   * Initialize each of the transmit descriptors.  Note that we leave the OWN
   * bit clear here since we have not set up any transmissions yet.
   */
  for (loop = 0; loop < NUM_TX_DESCRIPTORS; loop++) {

    txDescriptor[loop].ui32Count = (DES1_TX_CTRL_SADDR_INSERT | (TX_BUFFER_SIZE << DES1_TX_CTRL_BUFF1_SIZE_S));
    txDescriptor[loop].pvBuffer1 = txBuffer;
    txDescriptor[loop].DES3.pLink =
        (loop == (NUM_TX_DESCRIPTORS - 1)) ? txDescriptor : &txDescriptor[loop + 1];
    txDescriptor[loop].ui32CtrlStatus = (DES0_TX_CTRL_LAST_SEG | DES0_TX_CTRL_FIRST_SEG | DES0_TX_CTRL_INTERRUPT
        | DES0_TX_CTRL_CHAINED | DES0_TX_CTRL_IP_ALL_CKHSUMS);
  }

  /*
   * Initialize each of the receive descriptors.  We clear the OWN bit here
   * to make sure that the receiver doesn't start writing anything
   * immediately.
   */
  for (loop = 0; loop < NUM_RX_DESCRIPTORS; loop++) {

    rxDescriptor[loop].ui32CtrlStatus = 0;
    rxDescriptor[loop].ui32Count = (DES1_RX_CTRL_CHAINED | (RX_BUFFER_SIZE << DES1_RX_CTRL_BUFF1_SIZE_S));
    rxDescriptor[loop].pvBuffer1 = rxBuffer;
    rxDescriptor[loop].DES3.pLink =
        (loop == (NUM_RX_DESCRIPTORS - 1)) ? rxDescriptor : &rxDescriptor[loop + 1];
  }

  /*
   * Set the descriptor pointers in the hardware.
   */
  EMACRxDMADescriptorListSet(EMAC0_BASE, rxDescriptor);
  EMACTxDMADescriptorListSet(EMAC0_BASE, txDescriptor);

  /*
   * Start from the beginning of both descriptor chains.  We actually set
   * the transmit descriptor index to the last descriptor in the chain
   * since it will be incremented before use and this means the first
   * transmission we perform will use the correct descriptor.
   */
  rxDescIndex = 0;
  txDescIndex = NUM_TX_DESCRIPTORS - 1;
}

/*
 * The interrupt handler for the Ethernet interrupt.
 */
void ENET_Handler(void)
{
  uint32_t status;

  c_pos_intEnter();

  /*
   * Read and Clear the interrupt.
   */
  status = EMACIntStatus(EMAC0_BASE, true);
  EMACIntClear(EMAC0_BASE, status);

  /*
   * Check to see if an RX Interrupt has occurred.
   */
  if (status & EMAC_INT_RECEIVE) {

    netInterrupt();
  }

  c_pos_intExitQuick();
}

/*
 * Read a packet from the DMA receive buffer into the uIP packet buffer.
 */
int32_t tivaEmacPoll(uint8_t *buf, int32_t bufSize)
{
  int32_t frameLen;

  /*
   * By default, we assume we got a bad frame.
   */
  frameLen = 0;

  /*
   * Make sure that we own the receive descriptor.
   */
  if (!(rxDescriptor[rxDescIndex].ui32CtrlStatus & DES0_RX_CTRL_OWN)) {

    /*
     * We own the receive descriptor so check to see if it contains a valid
     * frame.  Look for a descriptor error, indicating that the incoming
     * packet was truncated or, if this is the last frame in a packet,
     * the receive error bit.
     */
    if (!(rxDescriptor[rxDescIndex].ui32CtrlStatus & DES0_RX_STAT_ERR)) {

      /*
       * We have a valid frame so copy the content to the supplied
       * buffer. First check that the "last descriptor" flag is set.  We
       * sized the receive buffer such that it can always hold a valid
       * frame so this flag should never be clear at this point but...
       */
      if (rxDescriptor[rxDescIndex].ui32CtrlStatus & DES0_RX_STAT_LAST_DESC) {

        frameLen = ((rxDescriptor[rxDescIndex].ui32CtrlStatus & DES0_RX_STAT_FRAME_LENGTH_M)
            >> DES0_RX_STAT_FRAME_LENGTH_S);

        /*
         * Sanity check.  This shouldn't be required since we sized the
         * uIP buffer such that it's the same size as the DMA receive
         * buffer but, just in case...
         */

        if (frameLen > bufSize)
          frameLen = bufSize;

        /*
         * Copy the data from the DMA receive buffer into the provided
         * frame buffer.
         */
        memcpy(buf, rxBuffer, frameLen);
      }
    }

    /*
     * Move on to the next descriptor in the chain.
     */
    rxDescIndex++;
    if (rxDescIndex == NUM_RX_DESCRIPTORS)
      rxDescIndex = 0;

    /*
     * Mark the next descriptor in the ring as available for the receiver
     * to write into.
     */
    rxDescriptor[rxDescIndex].ui32CtrlStatus = DES0_RX_CTRL_OWN;
  }

  return frameLen;
}

/*
 * Transmit a packet from the supplied buffer.
 */
void tivaEmacSend(uint8_t *buf, int32_t len)
{
  /*
   * Wait for the previous packet to be transmitted.
   */
  while (txDescriptor[txDescIndex].ui32CtrlStatus & DES0_TX_CTRL_OWN) {

    /*
     * Spin and waste time.
     */
  }

  /*
   * Check that we're not going to overflow the transmit buffer.  This
   * shouldn't be necessary since the uIP buffer is smaller than our DMA
   * transmit buffer but, just in case...
   */
  if (len > TX_BUFFER_SIZE)
    len = TX_BUFFER_SIZE;

  /*
   * Copy the packet data into the transmit buffer.
   */
  memcpy(txBuffer, buf, len);

  /*
   * Move to the next descriptor.
   */
  txDescIndex++;
  if (txDescIndex == NUM_TX_DESCRIPTORS)
    txDescIndex = 0;

  /*
   * Fill in the packet size and tell the transmitter to start work.
   */
  txDescriptor[txDescIndex].ui32Count = (uint32_t) len;
  txDescriptor[txDescIndex].ui32CtrlStatus = (DES0_TX_CTRL_LAST_SEG | DES0_TX_CTRL_FIRST_SEG
      | DES0_TX_CTRL_INTERRUPT | DES0_TX_CTRL_CHAINED | DES0_TX_CTRL_OWN);

  /*
   * Tell the DMA to reacquire the descriptor now that we've filled it in.
   */
  EMACTxDMAPollDemand(EMAC0_BASE);
}

#endif
