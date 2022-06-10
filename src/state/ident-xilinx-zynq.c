// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText:  2022 Patrick Siegl <code@siegl.it>

/*
  Zynq-7000 SoC Technical Reference Manual
  UG585 (v1.13) April 2, 2021
  page 1606f, page 1152f
  https://docs.xilinx.com/v/u/en-US/ug585-Zynq-7000-TRM
  
  and
  https://github.com/parallella/parallella-utils/blob/master/getfpga/getfpga.c
  (either GPL-3.0-only or GPLv3)
  
  and
  https://u-boot.denx.narkive.com/6UoyJCmP/patch-1-2-arm-zynq-add-support-for-zynq-7000s-7007s-7012s-7014s-devices
*/

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>


#define SLCR_BASE               0xF8000000
#define PSS_IDCODE_OFFS         0x530

#define SLCR_IDCODE_MASK        0x1F000
#define SLCR_IDCODE_SHIFT       12

#define XILINX_ZYNQ_7007S       0x3
#define XILINX_ZYNQ_7010        0x2
#define XILINX_ZYNQ_7012S       0x1c
#define XILINX_ZYNQ_7014S       0x8
#define XILINX_ZYNQ_7015        0x1b
#define XILINX_ZYNQ_7020        0x7
#define XILINX_ZYNQ_7030        0xc
#define XILINX_ZYNQ_7035        0x12
#define XILINX_ZYNQ_7045        0x11
#define XILINX_ZYNQ_7100        0x16


#define DEVCFG_BASE             0xF8007000
#define MCTRL_OFFS              0x80

#define MCTRL_PS_VERSION_MASK   0xF0000000
#define MCTRL_PS_VERSION_SHIFT  28

#define XILINX_SILICON_V1_0     0x0
#define XILINX_SILICON_V2_0     0x1
#define XILINX_SILICON_V3_0     0x2
#define XILINX_SILICON_V3_1     0x3


const char* xlxZynqDevice(int fd)
{
  assert(fd >= 0);

  uint32_t* slcr = mmap(NULL, 4096, PROT_READ, MAP_PRIVATE, fd, SLCR_BASE);
  if (slcr != MAP_FAILED) {
    __typeof__(*slcr) idcode = slcr[PSS_IDCODE_OFFS / sizeof(*slcr)];

    munmap(slcr, 4096);
  
    switch ((idcode & SLCR_IDCODE_MASK) >> SLCR_IDCODE_SHIFT) {
    case XILINX_ZYNQ_7007S:
      return "7z007s";
    case XILINX_ZYNQ_7010:
      return "7z010";
    case XILINX_ZYNQ_7012S:
      return "7z012s";
    case XILINX_ZYNQ_7014S:
      return "7z014s";
    case XILINX_ZYNQ_7015:
      return "7z015";
    case XILINX_ZYNQ_7020:
      return "7z020";
    case XILINX_ZYNQ_7030:
      return "7z030";
    case XILINX_ZYNQ_7035:
      return "7z035";
    case XILINX_ZYNQ_7045:
      return "7z045";
    case XILINX_ZYNQ_7100:
      return "7z100";
    default:
      break;
    }
  }
  return "unknown";
}

float xlxZynqSiliconRevision(int fd)
{
  assert(fd >= 0);

  uint32_t* devcfg = mmap(NULL, 4096, PROT_READ, MAP_PRIVATE, fd, DEVCFG_BASE);
  if (devcfg != MAP_FAILED) {
    __typeof__(*devcfg) mctrl = devcfg[MCTRL_OFFS / sizeof(*devcfg)];

    munmap(devcfg, 4096);
  
    switch ((mctrl & MCTRL_PS_VERSION_MASK) >> MCTRL_PS_VERSION_SHIFT) {
    case XILINX_SILICON_V1_0:
      return 1.0;
    case XILINX_SILICON_V2_0:
      return 2.0;
    case XILINX_SILICON_V3_0:
      return 3.0;
    case XILINX_SILICON_V3_1:
      return 3.1;
    default:
      break;
    }
  }
  return 0.0;
}

