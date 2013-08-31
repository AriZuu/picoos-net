/*
 * uIP device driver for CS8900A chip in 8-bit mode.
 *
 * This is originally from uIP port to LPC-E2124 by Paul Curtis at Rowley Associates.
 * http://www.rowley.co.uk/msp430/uip.htm
 * 
 * The download is not available at current page, see web archive at:
 * http://web.archive.org/web/20050206190903/http://rowley.co.uk/arm/uip-e2124.zip
 *
 * Datasheet: www.cirrus.com/en/pubs/proDatasheet/CS8900A_F5.pdf
 * 8-bit application note: http://www.cirrus.com/en/pubs/appNote/an181.pdf
 */

#include <picoos.h>
#include <picoos-net.h>

#if NETCFG_DRIVER_CS8900A > 0

#include "lpc_reg.h"
#include "cs8900a.h"
#include "cs8900a_regs.h"

#define IOR                  (1<<12)  // CS8900's ISA-bus interface pins
#define IOW                  (1<<13)

#define IODIR                GPIO0_IODIR
#define IOSET                GPIO0_IOSET
#define IOCLR                GPIO0_IOCLR
#define IOPIN                GPIO0_IOPIN

// Struct for CS8900 init sequence

typedef struct
{
  unsigned int Addr;
  unsigned int Data;
} TInitSeq;

static void cs8900aSkipFrame(void);

static TInitSeq InitSeq[] =
{
#if UIP_FIXEDETHADDR == 1
    { PP_IA, UIP_ETHADDR0 + (UIP_ETHADDR1 << 8)},     // set our MAC as Individual Address
    { PP_IA + 2, UIP_ETHADDR2 + (UIP_ETHADDR3 << 8)},
    { PP_IA + 4, UIP_ETHADDR4 + (UIP_ETHADDR5 << 8)},
#endif
    { PP_LineCTL, SERIAL_RX_ON | SERIAL_TX_ON },           // configure the Physical Interface
    { PP_LAF + 0, 0xffff },
    { PP_LAF + 2, 0xffff },
    { PP_LAF + 4, 0xffff },
    { PP_LAF + 6, 0xffff },
    { PP_RxCTL, RX_OK_ACCEPT | RX_IA_ACCEPT | RX_BROADCAST_ACCEPT | RX_MULTCAST_ACCEPT }
};

/*
 * IOW must be low for 110ns min for CS8900 to get data.
 * IOR must be low for 135ns min for data to be valid.
 * According to KEIL profiling, with 60 MHz clock
 * single NOP is about 0.021 us. So seven times NOP
 * is about 147 ns, which should be ok.
 */

#define IO_DELAY()  asm volatile("nop\n\t" \
                                 "nop\n\t" \
                                 "nop\n\t" \
                                 "nop\n\t" \
                                 "nop\n\t" \
                                 "nop\n\t" \
                                 "nop");

// Writes a word in little-endian byte order to a specified port-address

static void cs8900aWrite(unsigned addr, unsigned int data)
{
  IODIR |= 0xff << 16;                           // Data port to output

  // Write low order byte first

  IOCLR = 0xf << 4;                              // Put address on bus
  IOSET = addr << 4;

  IOCLR = 0xff << 16;                            // Write low order byte to data bus
  IOSET = (data & 0xff) << 16;

  IOCLR = IOW;                                   // Toggle IOW-signal
  IO_DELAY();
  IOSET = IOW;

  // Write high order byte second

  IOSET = 1 << 4;                                // Put next address on bus

  IOCLR = 0xff << 16;                            // Write high order byte to data bus
  IOSET = data >> 8 << 16;

  IOCLR = IOW;                                   // Toggle IOW-signal
  IO_DELAY();
  IOSET = IOW;
}

// Reads a word in little-endian byte order from a specified port-address

static unsigned cs8900aRead(unsigned addr)
{
  unsigned int value;

  IODIR &= ~(0xff << 16);                        // Data port to input

  IOCLR = 0xf << 4;                              // Put address on bus
  IOSET = addr << 4;

  IOCLR = IOR;                                   // IOR-signal low
  IO_DELAY();
  value = (IOPIN >> 16) & 0xff;                  // get low order byte from data bus
  IOSET = IOR;

  IOSET = 1 << 4;                                // IOR high and put next address on bus

  IOCLR = IOR;                                   // IOR-signal low
  IO_DELAY();
  value |= ((IOPIN >> 8) & 0xff00);              // get high order byte from data bus
  IOSET = IOR;                                   // IOR-signal low

  return value;
}

// Reads a word in little-endian byte order from a specified port-address

static unsigned cs8900aReadAddrHighFirst(unsigned addr)
{
  unsigned int value;

  IODIR &= ~(0xff << 16);                        // Data port to input

  IOCLR = 0xf << 4;                              // Put address on bus
  IOSET = (addr + 1) << 4;

  IOCLR = IOR;                                   // IOR-signal low
  IO_DELAY();
  value = ((IOPIN >> 8) & 0xff00);               // get high order byte from data bus
  IOSET = IOR;                                   // IOR-signal high

  IOCLR = 1 << 4;                                // Put low address on bus

  IOCLR = IOR;                                   // IOR-signal low
  IO_DELAY();
  value |= (IOPIN >> 16) & 0xff;                 // get low order byte from data bus
  IOSET = IOR;

  return value;
}

void cs8900aInit(void)
{
  unsigned int i;

  // Reset outputs, control lines high
  IOSET = IOR | IOW;

  // Port 3 output pins
  // Bits 4-7: SA 0-3
  // Bit 12: IOR
  // Bit 13: IOW
  // Bits 16-23: SD 0-7
  IODIR |= (0xff << 16) | IOR | IOW | (0xf << 4);

  // Reset outputs
  IOCLR = 0xff << 16;  // clear data outputs

  // Reset the CS8900A
  cs8900aWrite(ADD_PORT, PP_SelfCTL);
  cs8900aWrite(DATA_PORT, POWER_ON_RESET);

  // Wait until chip-reset is done
  cs8900aWrite(ADD_PORT, PP_SelfST);
  while ((cs8900aRead(DATA_PORT) & INIT_DONE) == 0)
    ;

  // Configure the CS8900A
#if UIP_FIXEDETHADDR == 0

  for (i = 0; i < 6; i += 2) {

    cs8900aWrite(ADD_PORT, PP_IA + i);
    cs8900aWrite(DATA_PORT, uip_lladdr.addr[i] + (uip_lladdr.addr[i + 1] << 8));
  }

#endif

  for (i = 0; i < sizeof InitSeq / sizeof(TInitSeq); ++i) {

    cs8900aWrite(ADD_PORT, InitSeq[i].Addr);
    cs8900aWrite(DATA_PORT, InitSeq[i].Data);
  }

  netEnableDevicePolling(MS(5));
}

static void cs8900aWriteTxBuf(uint8_t* bytes)
{
  IOCLR = 1 << 4;                                // put address on bus

  IOCLR = 0xff << 16;                            // Write low order byte to data bus
  IOSET = bytes[0] << 16;                        // write low order byte to data bus

  IOCLR = IOW;                                   // Toggle IOW-signal
  IO_DELAY();
  IOSET = IOW;

  IOSET = 1 << 4;                                // Put next address on bus

  IOCLR = 0xff << 16;                            // Write low order byte to data bus
  IOSET = bytes[1] << 16;                        // write low order byte to data bus

  IOCLR = IOW;                                   // Toggle IOW-signal
  IO_DELAY();
  IOSET = IOW;
}

static void cs8900aReadRxBuf(uint8_t* bytes)
{
  IOCLR = 1 << 4;                          // put address on bus

  IOCLR = IOR;                             // IOR-signal low
  IO_DELAY();
  bytes[0] = IOPIN >> 16;                  // get high order byte from data bus
  IOSET = IOR;                             // IOR-signal high

  IOSET = 1 << 4;                          // put address on bus

  IOCLR = IOR;                             // IOR-signal low
  IO_DELAY();
  bytes[1] = IOPIN >> 16;                  // get high order byte from data bus
  IOSET = IOR;                             // IOR-signal high
}

void cs8900aSend(void)
{
  unsigned u;

  // Transmit command
  cs8900aWrite(TX_CMD_PORT, TX_START_ALL_BYTES);
  cs8900aWrite(TX_LEN_PORT, uip_len);

  // Maximum number of retries
  u = 8;
  for (;;) {

    // Check for avaliable buffer space
    cs8900aWrite(ADD_PORT, PP_BusST);
    if (cs8900aRead(DATA_PORT) & READY_FOR_TX_NOW)
      break;

    if (u-- == 0)
      return;

    // No space avaliable, skip a received frame and try again
    cs8900aSkipFrame();
  }

  IODIR |= 0xff << 16;                             // Data port to output
  IOCLR = 0xf << 4;                                // Put address on bus
  IOSET = TX_FRAME_PORT << 4;

  // Send packet.
  for (u = 0; u < uip_len; u += 2)
    cs8900aWriteTxBuf(uip_buf + u);
}

static void cs8900aSkipFrame(void)
{
  // No space avaliable, skip a received frame and try again
  cs8900aWrite(ADD_PORT, PP_RxCFG);
  cs8900aWrite(DATA_PORT, cs8900aRead(DATA_PORT) | SKIP_1);
}

uint16_t cs8900aPoll(void)
{
  uint16_t len, u;

  // Check receiver event register to see if there are any valid frames avaliable
  cs8900aWrite(ADD_PORT, PP_RxEvent);
  if ((cs8900aRead(DATA_PORT) & 0xd00) == 0)
    return 0;

  // Read receiver status and discard it.
  cs8900aReadAddrHighFirst(RX_FRAME_PORT);

  // Read frame length
  len = cs8900aReadAddrHighFirst(RX_FRAME_PORT);

  // If the frame is too big to handle, throw it away
  if (len > UIP_BUFSIZE) {

    cs8900aSkipFrame();
    return 0;
  }

  // Data port to input
  IODIR &= ~(0xff << 16);

  IOCLR = 0xf << 4;                          // put address on bus
  IOSET = RX_FRAME_PORT << 4;

  // Read bytes into uip_buf
  u = 0;
  while (u < len) {

    cs8900aReadRxBuf(uip_buf + u);
    u += 2;
  }

  return len;
}

#endif
