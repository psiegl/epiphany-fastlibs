// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText:  2022 Patrick Siegl <code@siegl.it>

#ifndef __MEMMAP_EPIPHANY_SYSTEM_H
#define __MEMMAP_EPIPHANY_SYSTEM_H

#ifndef __epiphany__

#include <stdint.h>
#include "memmap-epiphany-cores.h"

//
//  Parallella-1.x Reference Manual
//  REV 14.09.09, page 25f
//  http://www.parallella.org/docs/parallella_manual.pdf
//
//  4.3 Epiphany Specific FPGA Resources
//  Developers that want leverage the Epiphany co-processors should use the Parallella programmable logic
//  reference design with minimal changes for best results. The following registers must be accessible by the
//  Epiphany drivers from the ARM for correct operation.
//
typedef struct __attribute__((__packed__)) {
  uint8_t ____PADDING0[ 0xf00 ];
  EREG_BITFIELD_MACRO( esysconfig, // 0x808f0f00
    volatile unsigned trans_read_timeout :  1; // Enable transaction timeout on read from Zynq
    volatile unsigned filter             :  2; // 00: Filter disable
                                               // 01: Inclusive range. Block transactions inside
                                               //  REG_FILTERL and REG_FILTERH range)
                                               // 10: Exclusive range. Block transactions outside
                                               //  REG_FILTERL and REG_FILTERH range)
                                               // 11: Reserved
    volatile unsigned elink_enable       :  1; // Epiphany eLink enable
                                               // 0:->
                                               // 1.) Forces RESET_N to zero,
                                               // 2.) turns off the epiphany input clock cclk
                                               // 3.) and turns off the elink TX/RX in the FPGA
                                               // 1:->
                                               // 1.) Turns on epiphany clock,
                                               // 2.) forces RESET_N to one (“out of reset”)
                                               // 3.) Turns on elink RX/TX in the FPGA
    unsigned ____reserved                : 24;
    volatile unsigned trans_ctrl_mode    :  4; // Epiphany transaction control mode
  );
  volatile uint32_t esysreset;     // 0x808f0f04  A write transaction to this register asserts a reset signal to
                                   //             Epiphany and the eLink logic within the Zynq. 
  EREG_BITFIELD_MACRO( esysinfo,   // 0x808f0f08
    volatile unsigned platform        :  8;    // 0=undefined
                                               // 1=parallella-1.x,e16,7z020,gpio
                                               // 2=parallella-1.x,e16,7z020,no-gpio
                                               // 3=parallella-1.x,e16,7z010,gpio
                                               // 4=parallella-1.x,e16,7z010,no-gpio
                                               // 5=parallella-1.x,e64,7x020,gpio
    volatile unsigned fpga_load_type  :  8;    // 0=undefined
                                               // 1=hdmi enabled, gpio unused
                                               // 2=headless, gpio unused
    volatile unsigned revision        :  8;    // 0=undefined
                                               // 1=first version
                                               // 2=second version
                                               // 3=etc..
    unsigned ____reserved             :  8;
  );
  volatile uint32_t esysfilterl;   // 0x808f0f0c  32-bit Transaction Filter (Low), [1:0] are ignored
  volatile uint32_t esysfilterh;   // 0x808f0f10  32-bit Transaction Filter (High), [1:0] are ignored
  EREG_BITFIELD_MACRO( esysfilterc,// 0x808f0f14
    volatile unsigned status             :  2; // Filter capture status
                                               // 00 - not a valid value
                                               // 01 - First violating transaction
                                               // 10 - Second violating transaction
                                               // 11 - There are more than 3 violating transactions
                                               // A write to this register clears value to zero
    volatile unsigned captured_addr      : 30; // Captured address of a filter violation
  );
  uint8_t ____PADDING1[ 0xE8 ];
} eSysRegs;


//
//  Parallella schematics
//  parallella_gen1
//  http://www.parallella.org/docs/parallella_schematic.pdf
//
// Parallella DRAM view from epiphany
// E16G301
//                           NORTH CONNECTOR
//                                   (elReg)
//                     [32, 8][32, 9][32,10][32,11]
// WEST CONN.          [33, 8][33, 9][33,10][33,11]         EAST CONNECTOR
// disconnected (elReg)[34, 8][34, 9][34,10][34,11](elReg)  connected Zynq bank 35
//                     [35, 8][35, 9][35,10][35,11]   ...   [35,32__DRAM__32,63] 0x8e000000+0x2000000
//                                   (elReg)
//                           SOUTH CONNECTOR
//
//
// From PA space of host
//                                  [15,32__DRAM__12,63] 0x3e000000+0x2000000
//
// [32, 8][32, 9][32,10][32,11]
// [33, 8][33, 9][33,10][33,11]
// [34, 8][34, 9][34,10][34,11]
// [35, 8][35, 9][35,10][35,11]
//
//
// E64G401
//                           NORTH CONNECTOR
//                                   (elReg?)      (elReg)
//                     [32, 8][32, 9][32,10]  ...  [32,14][32,15]
// WEST CONN.          [33, 8][33, 9][33,10]  ...  [33,14][33,15]         EAST CONNECTOR
// disconnected (elReg)[34, 8][34, 9][34,10]  ...  [34,14][34,15](elReg)  connected Zynq bank 35
//                       ...    ...                  ...    ...
//                     [39, 8][39, 9][39,10]  ...  [39,14][39,15]   ...   [35,32__DRAM__32,63] 0x8e000000+0x2000000
//                                   (elReg)
//                           SOUTH CONNECTOR

#endif /* __epiphany__ */
#endif /* __MEMMAP_EPIPHANY_SYSTEM_H */
