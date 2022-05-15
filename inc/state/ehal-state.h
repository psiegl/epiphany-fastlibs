// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText:  2022 Patrick Siegl <code@siegl.it>

#ifndef __EHAL_STATE__H
#define __EHAL_STATE__H

#include <stdint.h>
#include "memmap-epiphany-cores.h"
#include "memmap-epiphany-system.h"

typedef enum
{
  E__UNKNOWN = 0x0,
	E16G301 = ((0x10 << 16) | (0x4 << 12) | (0x3 << 8) | 0x1),  // the << 12 is used to not require square()
	E64G401 = ((0x40 << 16) | (0x8 << 12) | (0x4 << 8) | 0x1)
} eChip_t;
#define ECHIP_GET_DIM( x ) ( ((x) >> 12) & 0xF )

typedef enum
{
  // e16g301_datasheet.pdf page 12 SOUTH 0x1000 (wrong?)
  // epiphany-examples (2016.11)/io/link_lowpower_mode/src/e_link_lowpower_mode.c SOUTH 0x1001
  // epiphany_arch_ref.pdf page 133 CTRLMODE -> SOUTH 0x1001
  ELINK_REG_NORTH = 0,      // 0b0001  0x1
  ELINK_REG_EAST = 1,       // 0b0101  0x5
  ELINK_REG_SOUTH = 2,      // 0b1001  0x9
  ELINK_REG_WEST = 3        // 0b1101  0xd
} eLinkReg_t;
#define ELINK_REG_IOFLAG    ELINK_REG_WEST
#define ELINK_REG_CHIPRESET ELINK_REG_WEST
#define ELINK_REG_CHIPSYNC  ELINK_REG_WEST
#define ELINK_REG_CHIPHALT  ELINK_REG_WEST
#define ELINK_REG_MASK( x ) ( ((x) << 2) | 0x1 )

// 
// This is the minimal state that needs to be present to bootstrap.
// 
// Information is either retrieved by:
// /opt/adapteva/esdk/bsps/current/platform.hdf
// or by employing the esysregs
//
// TODO: add hw filedesc.
typedef struct                      // default values:
{                                   // ----------------
                                    // PLATFORM_VERSION   PARALLELLA1601
  eFpgaMemMapRegs esys_regs_base;   //*                       0x808f0f00  -> 0x808f0000

  unsigned num_chips;               //                                 1
  struct {
    eCoresGMemMap eCoreRoot;        //* CHIP_ROW                      32  ┬> 0x80800000
                                    //* CHIP_COL                       8  ┘
    unsigned xyDim;                 //* CHIP                     E16G301  ┬> 4
    eChip_t type;                   //*                                   └> E16G301
  } chip[1];
  /* TODO: ptr to local chip */

  unsigned num_ext_mems;            //                                 1
                                    // EMEM                     ext-DRAM
  struct {
    uintptr_t base_address;         //*                       0x3e000000
    char* epi_base;                 //*                       0x8e000000
    uint32_t size;                  //*                       0x02000000
    int prot;                       //* EMEM_TYPE                   RDWR  -> PROT_READ|PROT_WRITE
  } emem[1];
} eConfig_t;

#endif /* __EHAL_STATE__H */
