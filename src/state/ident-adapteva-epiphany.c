// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText:  2022 Patrick Siegl <code@siegl.it>

#include <assert.h>
#include <stdio.h>
#include "ehal-print.h"
#include "state/ident-adapteva-epiphany.h"

/*
  parallella_manual.pdf REV 14.09.09 page 26
 */
static char* parallellaVersionToString(eSysRegs* esysregs)
{
  assert(esysregs);

  const char* platform[] = {
    "undefined",
    "parallella-1.x,e16,7z020,gpio",
    "parallella-1.x,e16,7z020,no-gpio",
    "parallella-1.x,e16,7z010,gpio",
    "parallella-1.x,e16,7z010,no-gpio",
    "parallella-1.x,e64,7x020,gpio"
  };
  const char* fpga_load_type[] = {
    "undefined",
    "hdmi, gpio unused",
    "headless, gpio unused"
  };
  // let's set in the longest ... change if above changes!
  static char o[33 + 2 + 25 + 11 + 1];
  sprintf(o, "%s (%s, rev. %d)",
          platform[esysregs->esysinfo.platform < elemsof(platform) ? esysregs->esysinfo.platform : 0],
          fpga_load_type[esysregs->esysinfo.fpga_load_type < elemsof(fpga_load_type) ? esysregs->esysinfo.fpga_load_type : 0],
          esysregs->esysinfo.revision);
  return o;
}


eChip_t eChipType(eSysRegs* esysregs)
{
  assert(esysregs);

  switch(esysregs->esysinfo.platform) {
  case 0: // "undefined"
    eCoresError("Could not identify epiphany! 'unknown'\n");
    return E__UNKNOWN;
  case 1: // "parallella-1.x,e16,7z020,gpio"
  case 2: // "parallella-1.x,e16,7z020,no-gpio"
  case 3: // "parallella-1.x,e16,7z010,gpio"
  case 4: // "parallella-1.x,e16,7z010,no-gpio"
    return E16G301;
  case 5: // "parallella-1.x,e64,7x020,gpio"
    return E64G401;
  default:
    eCoresError("Could not identify epiphany at all! %d\n", esysregs->esysinfo.platform);
    return E__UNKNOWN;
  }
}

const char* eChipTypeToStr(eSysRegs* esysregs)
{
  assert(esysregs);

  switch(esysregs->esysinfo.platform) {
    eCoresError("Could not identify epiphany! 'unknown'\n");
    return "undefined";
  case 1:
    return "parallella-1.x,e16,7z020,gpio";
  case 2:
    return "parallella-1.x,e16,7z020,no-gpio";
  case 3:
    return "parallella-1.x,e16,7z010,gpio";
  case 4:
    return "parallella-1.x,e16,7z010,no-gpio";
  case 5: 
    return "parallella-1.x,e64,7x020,gpio";
  default:
    eCoresError("Could not identify epiphany at all! %d\n", esysregs->esysinfo.platform);
    return "unknown";
  }
}

const char* eChipCapsToStr(eSysRegs* esysregs)
{
  assert(esysregs);

  switch(esysregs->esysinfo.fpga_load_type) {
  case 1:
    return "hdmi, gpio unused";
  case 2:
    return "headless, gpio unused";
  default:
    return "undefined";
  }
}

unsigned eChipRevision(eSysRegs* esysregs)
{
  assert(esysregs);

  return esysregs->esysinfo.revision;
}
