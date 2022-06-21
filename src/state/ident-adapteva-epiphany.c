// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText:  2022 Patrick Siegl <code@siegl.it>

#include <assert.h>
#include "ehal-print.h"
#include "state/ident-adapteva-epiphany.h"

/*
  parallella_manual.pdf REV 14.09.09 page 26
 */
eChip_t eChipType(eSysRegs* esysregs)
{
  assert(esysregs);

  switch(esysregs->esysinfo.platform) {
  default:
    eCoresError("Could not identify epiphany! 'undefined' %u\n",
      esysregs->esysinfo.platform);
    return E__UNKNOWN;
  case 1: // "parallella-1.x,e16,7z020,gpio"
  case 2: // "parallella-1.x,e16,7z020,no-gpio"
  case 3: // "parallella-1.x,e16,7z010,gpio"
  case 4: // "parallella-1.x,e16,7z010,no-gpio"
    return E16G301;
  case 5: // "parallella-1.x,e64,7z020,gpio"
    return E64G401;
  }
}

const char* eChipTypeToStr(eSysRegs* esysregs)
{
  assert(esysregs);
  
  const char *str[] = {
    "undefined",
    "parallella-1.x,e16,7z020,gpio",
    "parallella-1.x,e16,7z020,no-gpio",
    "parallella-1.x,e16,7z010,gpio",
    "parallella-1.x,e16,7z010,no-gpio",
    "parallella-1.x,e64,7z020,gpio"
  };

  if(esysregs->esysinfo.platform >= (sizeof(str)/sizeof(str[0]))) {
    eCoresError("Could not identify epiphany! 'undefined' %u\n",
      esysregs->esysinfo.platform);
    return str[0];
  }

  return str[esysregs->esysinfo.platform];
}

const char* eChipCapsToStr(eSysRegs* esysregs)
{
  assert(esysregs);

  const char *str[] = {
    "undefined",
    "hdmi, gpio unused",
    "headless, gpio unused"
  };

  if(esysregs->esysinfo.fpga_load_type >= (sizeof(str)/sizeof(str[0]))) {
    eCoresError("Could not identify FPGA load type! 'undefined' %u\n",
      esysregs->esysinfo.fpga_load_type);
    return str[0];
  }

  return str[esysregs->esysinfo.fpga_load_type];
}

unsigned eChipRevision(eSysRegs* esysregs)
{
  assert(esysregs);

  return esysregs->esysinfo.revision;
}
