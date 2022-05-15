// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText:  2022 Patrick Siegl <code@siegl.it>

#ifndef __MEMMAP_EPIPHANY_CORES__PUBLIC_API__H
#define __MEMMAP_EPIPHANY_CORES__PUBLIC_API__H

#include <stdint.h>

#define EREG_BITFIELD_MACRO( BITFIELDNAME, BITFIELDS ) \
  union { \
    volatile uint32_t reg; \
    struct __attribute__((__packed__)) { \
      BITFIELDS \
    }; \
  } BITFIELDNAME
typedef volatile unsigned vunsigned;
    
//
//  E16G301 EPIPHANYTM 16-CORE MICROPROCESSOR Datasheet
//  REV 14.03.11, page 11f
//  https://www.adapteva.com/docs/e16g301_datasheet.pdf
//
//  https://github.com/adapteva/epiphany-examples/tree/2016.11/io/link_lowpower_mode/src/e_link_lowpower_mode.c
//  https://github.com/adapteva/epiphany-examples/blob/2016.11/apps/e-toggle-led/src/device-e-toggle-led.c
//  https://github.com/parallella/parallella-utils/blob/master/board_debug/src/e_regs.h
//  https://github.com/adapteva/epiphany-libs/blob/2016.11/e-lib/include/e_regs.h
//
//  E64G301 EPIPHANYTM 64-CORE MICROPROCESSOR Datasheet
//  REV 14.03.11, page 11f
//  https://www.adapteva.com/docs/e64g401_datasheet.pdf
//
//  2.2 Memory Mapped Registers
//
//  In the table, the link registers additionally have an offset
//  that is dependent on the link in question as follows:
//                      E16G301               E64G401
//  North link offset:  0x00200000 [ 0, 2]    0x00600000 [ 0, 6]  (https://github.com/adapteva/epiphany-libs/blob/2016.11/e-hal/src/epiphany-hal.c -> 0x00200000 for E64G401)
//  East link offset:   0x08300000 [ 2, 3]    0x08700000 [ 2, 7]
//  South link offset:  0x0c200000 [ 3, 2]    0x1c200000 [ 7, 2]
//  West link offset:   0x08000000 [ 2, 0]    0x08400000 [ 2, 4]  (https://github.com/adapteva/epiphany-libs/blob/2016.11/e-hal/src/epiphany-hal.c -> 0x08000000 for E64G401)
//
//  e_link_power_mode.c shows that below works as well:
//                      E16G301
//  North link offset:  0x80A00000 [32,10]
//  South link offset:  0x8CA00000 [35,10]
//  West link offset:   0x88800000 [34, 8]
//  * ioflag seen with 0x80A as well (device-e-toggle-led.c)
//
//  In order to write to these memory mapped registers, the store transaction must be configured with
//  a special control mode that allows the transaction to bypass the regular eMesh routing protocol..
//  The special routing mode is controlled through bits [15:12] of the CONFIG register of the core
//  initiating the write transaction to the IO registers and should be set according to Table 3.
//
//  Table 3: CONFIG Register Routing Mode Selection
//
//  Register Name                           CONFIG[15:12]
//  NORTH ELINK REGISTERS                   0001
//  EAST ELINK REGISTERS                    0101
//  SOUTH ELINK REGISTERS                   1000
//  WEST ELINK REGISTERS                    1101
//  IOFLAG, CHIPRESET, CHIPSYNC, CHIPHALT   1101
//  
//   To return the routing behavior to normal mode, CONFIG[15:12] should be reset to 0000. 
//   1. Set the CONFIG[15:12] appropriately according to Table 3 using a MOVTS instruction.
//   2. Write to the chip level register address as specified in Table 2
//   3. Reset CONFIG[15:12] to 0000 using a MOVTS instruction.
typedef struct __attribute__((__packed__))
{ //                   Register Name     Address      Access  Comment
  EREG_BITFIELD_MACRO( elinkmodecfg,  // 0xoff F0300
    vunsigned lclk_transmit_freq_ctrl           :  4; //      LCLK Transmit Frequency control: Divide cclk by 0->2, 1->4, 2->8
    vunsigned ____reserved                      : 28;
  );
  EREG_BITFIELD_MACRO( elinktxcfg,    // 0xoff F0304
    vunsigned tx_low_power_mode                 : 12; //      Transmitter low power mode: 0xFFF=turned off, 0x000=turned on
    vunsigned ____reserved                      : 20;
  );
  EREG_BITFIELD_MACRO( elinkrxcfg,    // 0xoff F0308
    vunsigned rx_low_power_mode                 : 12; //      Receiver low power mode: 0xFFF=turned off, 0x000=turned on
    vunsigned ____reserved                      : 20;
  );
  volatile uint32_t gpiocfg;          // 0x80A F030c          0x03FFFFFF (seems to address ioflag for red LED) see e-toggle-led.c / e_regs.h
  uint8_t ____PADDING0[0x8];
  EREG_BITFIELD_MACRO( flagcfg,       //*0x006 F0318
    vunsigned set_monitor_pin                   :  6; //      Sets MONITOR pin to 0->0, 1->1
    vunsigned ____reserved                      : 26;
  ) ;                      
  volatile uint32_t chipsync;         // 0x083 F031C  WR      Writing to register creates a chip wide “SYNC” interrupt
  volatile uint32_t chiphalt;         // 0x083 F0320  WR      Writing to register puts all cores in the HALT debug state.
  volatile uint32_t chipreset;        // 0x083 F0324  WR      Writing to register creates a three clock cycle long pulse
                                      //                      that resets the rest of the chip.
  EREG_BITFIELD_MACRO( elinkdebug,    // 0xoff F0328
    vunsigned set_constant_on_link_tx           :  1;
    vunsigned loopback_rx_to_tx                 :  1;
    vunsigned constant_to_drive_on_tx           : 12;
    vunsigned force_chip_trans_match_rx_trans   :  1; //      Force a chip transaction match on RX transactions
    vunsigned ____reserved                      : 17;
  );
  uint8_t ____PADDING1[0xD4];
} eChipIoRegs_t;


//
//  Epiphany Architecture Reference
//  REV 14.03.11, page 128f
//  https://www.adapteva.com/docs/epiphany_arch_ref.pdf
//
//  Table 30: DMA Registers
typedef struct __attribute__((__packed__))
{ //                   Register Name     Address      Access  Comment
  EREG_BITFIELD_MACRO( config,    // 0xF0500 (+0x20)  RD/WR   DMA channel{0/1} configuration
    vunsigned dmaen           :  1; // [0]                    Turns on DMA channel: 1=enabled, 0=disabled
    vunsigned master          :  1; // [1]                    Sets up DMA channel to work in master made:
                                    //                        1=master mode, 0=slave mode
    vunsigned chainmode       :  1; // [2]                    Sets up DMA in chaining mode so that a new descriptor is
                                    //                        automatically fetched from the next descriptor address at
                                    //                        the end of the current configuration.
                                    //                        1=Chain mode, 0=One-shot mode
    vunsigned startup         :  1; // [3]                    Used to kick start the DMA configuration. When this bit is
                                    //                        set to 1, the DMA sequencer looks at bits [31:16] to find the
                                    //                        descriptor address to fetch the complete DMA configuration
                                    //                        from. Once the descriptor has been completely fetched, the
                                    //                        DMA will start data transfers.
                                    //                        1=Fetch descriptor, 0=Normal operation
    vunsigned irqen           :  1; // [4]                    Enables interrupt at the end of the complete DMA channel.
                                    //                        In the case of chained interrupts, the interrupt is set
                                    //                        before the next descriptor is fetched.
                                    //                        1=Enable interrupt at end of DMA transfer
                                    //                        0=Disable interrupt at end of DMA transfer.
    vunsigned datasize        :  2; // [6:5]                  Size of data transfer.
                                    //                        00=byte, 01=half-word, 10=word, 11=double-word 
    vunsigned ____reserved0   :  3; // [9:7]                  (maddr_defs.h) Could be 'dma throttle mode': 000=no burst    001=2-burst    010=8-burst    011=32 burst   100=128 burst   101=512 burst   110=2048 burst   111=inifinite burst	YES	arbiters held while in burst
    vunsigned msgmode         :  1; // [10]                   (LABS) Attach a special message to the last data item of a DMA
                                    //                        channel transfer. If the destination address is local memory,
                                    //                        then the transition to DMA_IDLE only occurs after the last
                                    //                        data item has returned. If the destination address is in
                                    //                        another core, then a message interrupt (IRQ5) is sent along 
                                    //                        with the last data item.
    vunsigned ____reserved1   :  1; // [11]
    vunsigned shift_src_in    :  1; // [12]                   (LABS) Left shift inner loop source stride address by 16 bits
    vunsigned shift_dst_in    :  1; // [13]                   (LABS) Left shift inner loop destination stride address by 16 bits
    vunsigned shift_src_out   :  1; // [14]                   (LABS) Left shift outer loop source stride address by 16 bits
    vunsigned shift_dst_out   :  1; // [15]                   (LABS) Left shift outer loop destination stride address by 16 bits
    vunsigned next_ptr        : 16; // [31:16]                Address of next DMA descriptor for normal operation.
                                    //                        Address of immediate descriptor to fetch in case of startup
                                    //                        mode.
  );
  volatile uint32_t stride;       // 0xF0504 (+0x20)  RD/WR   DMA channel{0/1} stride
  EREG_BITFIELD_MACRO( count,     // 0xF0508 (+0x20)  RD/WR   DMA channel{0/1} count
    vunsigned inner_count     : 16; //                        Transactions remaining within inner loop.
    vunsigned outer_count     : 16; //                        Number of outer loop iterations remaining. (“2D”)
  );
  volatile uint32_t srcaddr;      // 0xF050C (+0x20)  RD/WR   DMA channel{0/1} source address
  volatile uint32_t dstaddr;      // 0xF0510 (+0x20)  RD/WR   DMA channel{0/1} destination address
  volatile uint32_t autodma[2];   // 0xF0514 (+0x20)  RD/WR   (LABS) DMA channel{0/1} slave lower data
                                  // 0xF0518 (+0x20)  RD/WR   (LABS) DMA channel{0/1} slave upper data
  EREG_BITFIELD_MACRO( status,    // 0xF051C (+0x20)  RD/WR   DMA channel{0/1} status
    vunsigned dmastate        :  4;
    unsigned ____reserved     : 12;
    vunsigned curr_ptr        : 16; //                        The address of DMA descriptor currently being processed.
  );
} eCoreDMA_t;

//
//  Epiphany Architecture Reference
//  REV 14.03.11, page 127ff
//  https://www.adapteva.com/docs/epiphany_arch_ref.pdf
//
//  Table 27: eCore Registers
//  Table 28: Event Timer Registers
//  Table 29: Processor Control Registers
//  Table 31: Mesh Node Control Registers
typedef struct __attribute__((__packed__))
{ //                Register Name   Address          Access  Comment
  volatile uint32_t r[64];       // 0xF0000->0xF00FC RD/WR   General purpose registers
  uint8_t ____PADDING0[0x200];
  eChipIoRegs_t chipioregs;
  EREG_BITFIELD_MACRO( config,   // 0xF0400          RD/WR   Core configuration
    vunsigned rmode           :  1; // [0]                   IEEE Floating-Point Truncate Rounding Mode
                                    //                       0 = Round to nearest even rounding
                                    //                       1 = Truncate rounding
    vunsigned ien             :  1; // [1]                   Invalid floating-point exception enable
                                    //                       0 = Exception turned off
                                    //                       1 = Exception turned on
    vunsigned oen             :  1; // [2]                   Overflow floating-point exception enable
                                    //                       0 = Exception turned off
                                    //                       1 = Exception turned on
    vunsigned uen             :  1; // [3]                   Underflow floating-point exception enable
                                    //                       0 = Exception turned off
                                    //                       1 = Exception turned on
    vunsigned ctimer0cfg      :  4; // [7:4]                 Controls the events counted by CTIMER0.
                                    //                       0000 = off
                                    //                       0001 = clk
                                    //                       0010 = idle cycles
                                    //                       0011 = reserved
                                    //                       0100 = IALU valid instructions
                                    //                       0101 = FPU valid instructions
                                    //                       0110 = dual issue clock cycles
                                    //                       0111 = load (E1) stalls
                                    //                       1000 = register dependency (RA) stalls
                                    //                       1001 = reserved
                                    //                       1010 = local memory fetch stalls
                                    //                       1011 = local memory load stalls
                                    //                       1100 = external fetch stalls
                                    //                       1101 = external load stalls
                                    //                       1110 = mesh traffic monitor 0
                                    //                       1111 = mesh traffic monitor 1
    vunsigned ctimer1cfg      :  4; // [11:8]                Timer1 mode, same description as for CTIMER0.
                                    //                       A 0011 configuration selects the carry out from TIMER0,
                                    //                       effectively creating a 64 bit timer (NOTE: not available in
                                    //                       Epiphany-III).
    vunsigned ctrlmode        :  4; // [15:12]               This register controls certain routing modes within the eMesh.
                                    //                       More information can be found in eMesh chapter.
                                    //                       0000:Normal routing mode
                                    //                       0100: DMA channel0 last transaction indicator
                                    //                       1000: DMA channel1 last transaction indicator
                                    //                       1100: Message mode routing (LABS)
                                    //                       0001: Force routing to the NORTH at destination
                                    //                       0101: Force routing to the EAST at destination
                                    //                       1001: Force routing to the SOUTH at destination
                                    //                       1101: Force routing to the WEST at destination
                                    //                       xx10: Reserved
                                    //                       0011: Multicast routing (LABS)
                                    //                       1011: Reserved
                                    //                       0111: Reserved
                                    //                       1111: Reserved
    unsigned ____reserved0    :  1; // [16]                  (maddr_defs.h) split register file into 32 fpu/32 pointers	NO	for later...
    vunsigned arithmode       :  3; // [19:17]               Selects the operating mode of the data path unit.(“FPU”)
                                    //                       000 = 32bit IEEE float point mode
                                    //                       100 = 32bit signed integer mode
                                    //                       All other modes reserved.
    unsigned ____reserved1    :  1; // [20]                  (maddr_defs.h) emulation mode (stops iab prefetching)
    unsigned ____reserved2    :  1; // [21]                  (maddr_defs.h) single issue mode
    vunsigned lpmode          :  1; // [22]                  0=Only minimal clock gating in idle mode
                                    //                       1=Aggressive power down in idle mode (Recommended).
    
    unsigned ____reserved3    :  3; // [25-23]
    vunsigned timerwrap       :  1; // [26]                  G4-LABS 0=Timer stops when it reaches 0x0
                                    //                       1=Timer resets to 0xFFFFFFFF when it reaches 0x0 and keeps going.
                                    //                       (only available in Epiphany-IV) (LABS)
    unsigned ____reserved4    :  1; // [27]
    vunsigned ____clkdivratio :  4; // [31:28]               (maddr_defs.h) clock divider ratio            0000=no division            0001=divide by 2            0010=divide by 4            Etc
  );
  EREG_BITFIELD_MACRO( status,   // 0xF0404          RD/WR*  Core status
    vunsigned active          :  1; // core active indicator
    vunsigned gid             :  1; // global interrupt disable
    vunsigned ____processor_mode :  1; // (maddr_defs.h) (1=user mode, 0=kernel mode)
    vunsigned wand            :  1; // (LABS) wired AND global flag
    vunsigned az              :  1; // integer zero
    vunsigned an              :  1; // integer negative
    vunsigned ac              :  1; // integer carry
    vunsigned av              :  1; // integer overflow
    vunsigned bz              :  1; // fpu zero flag
    vunsigned bn              :  1; // fpu negative flag
    vunsigned bv              :  1; // fpu overflow flag
    vunsigned ____bc          :  1; // (maddr_defs.h) fpu carry flag(not used); will be used by integer unit!!
    vunsigned avs             :  1; // ialu overflow flag(sticky)
    vunsigned bis             :  1; // fpu invalid flag(sticky)
    vunsigned bvs             :  1; // fpu overflow flag(sticky)
    vunsigned bus             :  1; // fpu underflow flag(sticky)
    vunsigned excause         :  2; // 00=no exception 01=load-store exception 10=fpu exception 11=unimplemented instruction
    vunsigned ext_load_stalled:  1; // external load stalled
    vunsigned ext_fetch_stalled: 1; // external fetch stalled
    unsigned ____reserved2    : 12;
  );
  volatile uint32_t pc;          // 0xF0408          RD/WR   Program counter
  EREG_BITFIELD_MACRO( debugstatus, // 0xF040C       RD      Debug status
    vunsigned halt            :  1; //                       0: Processor operating normally
                                    //                       1: Processor in “halt” state
    vunsigned ext_pend        :  1; //                       0: No external load or fetch pending
                                    //                       1: External load or fetch pending
    vunsigned mbkpt_flag      :  1; //                       0: No multicore breakpoint active
                                    //                       (LABS) 1: Multicore breakpoint active
    vunsigned ____reserved    : 29;
  );
  uint8_t ____PADDING2[0x4];     //                          (maddr_defs.h) iab fifo register for external fetch
  volatile uint32_t lc;          // 0xF0414          RD/WR   (LABS) Hardware loop counter
  volatile uint32_t ls;          // 0xF0418          RD/WR   (LABS) Hardware loop start address
  volatile uint32_t le;          // 0xF041C          RD/WR   (LABS) Hardware loop end address
  volatile uint32_t iret;        // 0xF0420          RD/WR   Interrupt PC return value
  volatile uint32_t imask;       // 0xF0424          RD/WR   Interrupt mask
  volatile uint32_t ilat;        // 0xF0428          RD/WR   Interrupt latch
  volatile uint32_t ilatst;      // 0xF042C          WR      Alias for setting interrupts
  volatile uint32_t ilatcl;      // 0xF0430          WR      Alias for clearing interrupts
  volatile uint32_t ipend;       // 0xF0434          RD/WR   Interrupts currently in process
  volatile uint32_t ctimer[2];   // 0xF0438          RD/WR   Core timer0: Complete 32-bit timer or lower 32-bits of 64-bit timer
                                 // 0xF043C          RD/WR   Core timer1: Complete 32-bit timer or upper 32-bits of 64-bit timer
  volatile uint32_t fstatus;     // 0xF0440          WR      (LABS) status register: Alias for writing to all STATUS bits
  uint8_t ____PADDING3[0x4];
  EREG_BITFIELD_MACRO( debug,    // 0xF0448          WR      Debug command register
    vunsigned command         :  2; //                       00: Force the processor into a “running” state (i.e. resume)
                                    //                       01: Force the processor into a “halt” state (i.e. halt)
    unsigned ____reserved     : 30;
  );
  uint8_t ____PADDING4[0xB4];
  eCoreDMA_t dma[2];             // 0xF0500->0xF053C
  uint8_t ____PADDING5[0xC4];    // 0xF0600                  (maddr_defs.h) MEMORY [4:0]=memory speed setting NO
  EREG_BITFIELD_MACRO( memstatus,// 0xF0604          RD/WR   (LABS) Memory protection status
    unsigned ____reserved0    :  2;
    vunsigned mem_fault       :  1;
    unsigned ____reserved1    :  7;
    vunsigned read_breach     :  1; //                       G4
    vunsigned write_breach    :  1; //                       G4
    vunsigned cwrite_breach   :  1; //                       G4
    vunsigned xwrite_breach   :  1; //                       G4
    unsigned ____reserved2    : 18;
  );
  EREG_BITFIELD_MACRO(memprotect,// 0xF0608          RD/WR   (LABS) Memory protection configuration
    vunsigned pages           :  8;
    unsigned ____reserved0    :  2;
    vunsigned dis_ext_rd      :  1; //                       G4
    vunsigned dis_ext_wr_mmr  :  1; //                       G4
    vunsigned dis_ext_wr_mem  :  1; //                       G4
    vunsigned dis_core_cwr    :  1; //                       G4
    vunsigned dis_core_xwr    :  1; //                       G4
    vunsigned exc_en          :  1; //                       G4
    unsigned ____reserved1    : 16;
  );
  uint8_t ____PADDING6[0xF4];
  EREG_BITFIELD_MACRO(meshconfig,// 0xF0700          RD/WR   (LABS) Mesh node configuration
    vunsigned ____round_robin_en :  1; //                    (maddr_defs.h) round robin enable
    vunsigned lpmode          :  1;
    unsigned ____reserved1    :  2;
    vunsigned meshevent1      :  4;
    vunsigned meshevent0      :  4;
    vunsigned westedge        :  1;
    vunsigned eastedge        :  1;
    vunsigned northedge       :  1;
    vunsigned southedge       :  1;
    unsigned ____reserved2    : 16;
  ); 
  EREG_BITFIELD_MACRO( coreid,   // 0xF0704          RD      Processor node ID
    vunsigned column_id       :  6;
    vunsigned row_id          :  6;
    unsigned ____reserved     : 20;
  );
  EREG_BITFIELD_MACRO( multicast,// 0xF0708          RD/WR   (LABS) Multicast configuration 
    vunsigned id              : 12;
    unsigned ____reserved     : 20;
  );
  EREG_BITFIELD_MACRO( corereset,// 0xF070C          WR      (LABS) Per core software reset
    vunsigned reset           :  1; //                       writing a one puts core in reset state.  To get out of reset state, write a 0
    unsigned ____reserved     : 31;
  );
  EREG_BITFIELD_MACRO(cmeshroute,// 0xF0710          RD/WR   (G4-LABS) cMesh routing configuration
    vunsigned north_cfg       :  3; //                       0xx: normal routing
                                    //                       100: block northbound transactions
                                    //                       101: send northbound transactions east
                                    //                       110: send northbound transactions south
                                    //                       111: send northbound transactions west
    vunsigned east_cfg        :  3; //                       0xx: normal routing
                                    //                       100: block northbound transactions
                                    //                       101: send northbound transactions south
                                    //                       110: send northbound transactions west
                                    //                       111: send northbound transactions north
    vunsigned south_cfg       :  3; //                       0xx: normal routing
                                    //                       100: block northbound transactions
                                    //                       101: send northbound transactions west
                                    //                       110: send northbound transactions north
                                    //                       111: send northbound transactions east
    vunsigned west_cfg        :  3; //                       0xx: normal routing
                                    //                       100: block northbound transactions
                                    //                       101: send northbound transactions north
                                    //                       110: send northbound transactions east
                                    //                       111: send northbound transactions south
    unsigned ____reserved     : 20;
  );
  EREG_BITFIELD_MACRO(xmeshroute,// 0xF0714          RD/WR   (G4-LABS) xMesh routing configuration
    vunsigned north_cfg       :  3;
    vunsigned east_cfg        :  3;
    vunsigned south_cfg       :  3;
    vunsigned west_cfg        :  3;
    unsigned ____reserved     : 20;
  );
  EREG_BITFIELD_MACRO(rmeshroute,// 0xF0718          RD/WR   (G4-LABS) rMesh routing configuration
    vunsigned north_cfg       :  3;
    vunsigned east_cfg        :  3;
    vunsigned south_cfg       :  3;
    vunsigned west_cfg        :  3;
    unsigned ____reserved     : 20;
  );
  uint8_t ____PADDING7[0x8E4]; // aligned on pagesize (4KB)
} eCoreRegs_t;


//
//  Epiphany Architecture Reference
//  REV 14.03.11, page 64
//  https://www.adapteva.com/docs/epiphany_arch_ref.pdf
//
//  Table 24: Interrupt Support Summary
typedef struct __attribute__((__packed__))
{ //                Interrupt             IRQ Priority IVT  Address Event
  volatile uint32_t sync;              // 0 (highest)  0x0  Sync hardware signal asserted
  volatile uint32_t softwareexception; // 1            0x4  Floating-point exception, invalid instruction, alignment error
  volatile uint32_t memoryfault;       // 2            0x8  Memory protection fault
  volatile uint32_t timerinterrupt[2]; // 3            0xC  Timer0 has expired
                                       // 4            0x10 Timer1 has expired
  volatile uint32_t message;           // 5            0x14 Message interrupt
  volatile uint32_t dmainterrupt[2];   // 6            0x18 Local DMA channel-0 finished data transfer
                                       // 7            0x1C Local DMA channel-1 finished data transfer
  volatile uint32_t wandinterrupt;     // 8            0x20 Wired-AND signal interrupt
  volatile uint32_t userinterrupt;     // 9 (lowest)   0x24 Software generated user interrupt
} eCoreIVT_t;


//
//  Epiphany Architecture Reference
//  REV 14.03.11, page 40
//  https://www.adapteva.com/docs/epiphany_arch_ref.pdf
//
//  Table 6: eCore Local Memory Map Summary
typedef struct __attribute__((__packed__))
{  // Name                                BgnAddr EndAddr Size    Comment
  eCoreIVT_t ivt;                       // 0x00    0x3F    64B     Local Memory
  uint8_t ____PADDING[ 0xFFFA8 + 0x30 ];
} eCoreMemMapSW_t;

typedef struct __attribute__((__packed__))
{  // Name                                BgnAddr EndAddr Size    Comment
  union {
    volatile uint8_t sram[ 0x8000 ];
    volatile uint8_t bank[4][ 0x2000 ]; // 0x0     0x1FFF  8KB     Local Memory Bank
  };                                    // 0x2000  0x3FFF  8KB     Local Memory Bank
                                        // 0x4000  0x5FFF  8KB     Local Memory Bank
                                        // 0x6000  0x7FFF  8KB     Local Memory Bank
  uint8_t ____RESERVED0[ 0xE8000 ];     // 0x8000  0xEFFFF n/a     Reserved for future memory expansion
  eCoreRegs_t regs;                     // 0xF0000 0xF0FFF 4KB     Memory mapped register access (rounded on pagesize)
  uint8_t ____RESERVED1[ 0xF000 ];      // 0xF1000 0xFFFFF n/a     N/A
} eCoreMemMap_t;


#define ECORES_MAX_DIM        64
typedef eCoreMemMap_t         (*eCoresGMemMap)[ECORES_MAX_DIM];
//#define ECORE_NEXT( ROWS, COLS ) ( (ROWS) * ECORES_MAX_DIM + (COLS) )
/*
 root + (r * 64 + c) = end
 r * 64 + c = end - root
 
 r == c -> d
 d * 64 + d = end - root
 d * 65 = end - root
 d = (end - root)/65
 -> add 1, as we want to get the length
*/
#define ECORE_SQUARE_LEN( BGN, END ) ( ((uintptr_t)(((eCoreMemMap_t*)END)-((eCoreMemMap_t*)BGN))) / 65 + 1 )



#define ECORE_ADDR_ROWMASK  ((ECORES_MAX_DIM-1) << 26)
#define ECORE_ADDR_COLMASK  ((ECORES_MAX_DIM-1) << 20)
#define ECORE_ADDR_LCLMASK  ((uintptr_t)0xFFFFF)

// VA: (row, column) local addr
#define ECORE_ADDR_ROWID( addr ) (( ((uintptr_t)addr) >> 26 ) & (ECORES_MAX_DIM-1))
#define ECORE_ADDR_COLID( addr ) (( ((uintptr_t)addr) >> 20 ) & (ECORES_MAX_DIM-1))
#define ECORE_ADDR_LOCAL( addr ) ( ((uintptr_t)addr) & ECORE_ADDR_LCLMASK ).


#endif /* __MEMMAP_EPIPHANY_CORES__PUBLIC_API__H */
