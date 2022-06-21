// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText:  2022 Patrick Siegl <code@siegl.it>

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h> 
#include <string.h>
#include <sys/mman.h> // PROT_NONE, PROT_READ, PROT_WRITE
#include "memmap-epiphany-cores.h"
#include "loader/ehal-gen-file-loader.h"
#include "loader/ehal-hdf-loader.h"
#include "ehal-print.h"

#define elemsof( x ) (sizeof(x)/sizeof(x[0]))
#define MIN_VALUE( x, y ) ((x) < (y) ? (x) : (y))


// The code assumes that it gets 4K pages.
// means: ESYS_REGS_BASE, EMEM_BASE_ADDRESS and EMEM_EPI_BASE need to be 4K-page
// aligned and EMEM_SIZE needs to be a multiple of it.
#define MASK_4K   ((uintptr_t)0xFFFFE000)

typedef enum {
  HDF_INVALID,
  HDF_CHIP,
  HDF_EMEM,
  HDF_PLATFORM_VERSION,
  HDF_NUM_CHIPS,
  HDF_NUM_EXT_MEMS,
  HDF_EMEM_TYPE,
  HDF_CHIP_ROW,
  HDF_CHIP_COL,
  HDF_ESYS_REGS_BASE,
  HDF_EMEM_BASE_ADDRESS,
  HDF_EMEM_EPI_BASE,
  HDF_EMEM_SIZE
} hdfKey_t;


#define CMP(a, b, b_size) \
({ \
  unsigned ret; \
  switch(b_size) { \
  case 1: ret = ((a)[0] == b[0]); \
    break; \
  case 2: ret = ((a)[0] == b[0] && (a)[1] == b[1]); \
    break; \
  case 3: ret = ((a)[0] == b[0] && (a)[1] == b[1] && (a)[2] == b[2]); \
    break; \
  case 4: ret = ((a)[0] == b[0] && (a)[1] == b[1] && (a)[2] == b[2] && (a)[3] == b[3]); \
    break; \
  case 5: ret = ((a)[0] == b[0] && (a)[1] == b[1] && (a)[2] == b[2] && (a)[3] == b[3] \
              && (a)[4] == b[4]); \
    break; \
  case 6: ret = ((a)[0] == b[0] && (a)[1] == b[1] && (a)[2] == b[2] && (a)[3] == b[3] \
              && (a)[4] == b[4] && (a)[5] == b[5]); \
    break; \
  case 7: ret = ((a)[0] == b[0] && (a)[1] == b[1] && (a)[2] == b[2] && (a)[3] == b[3] \
              && (a)[4] == b[4] && (a)[5] == b[5] && (a)[6] == b[6]); \
    break; \
  default: \
  { \
    unsigned i; \
    ret = ((a)[0] == b[0]); \
    for(i = 1; i < b_size; ++i) \
      ret = (ret && (a)[i] == b[i]); \
    break; \
  } \
  } \
  ret; \
})

#define CASE( b, cur, key, c, str, strlong, etype ) \
  case (c): { \
    if(CMP( (cur), str, (sizeof(str)-1) )) { \
      key = etype; \
      b += sizeof(strlong) - 1; \
    } \
  break; \
  } \

#define SKIP_WSPACE( str ) \
({ \
  unsigned x = 1; \
  do { \
    switch(*(str)) { \
    case ' ': \
    case '\t': \
    case '\v': \
      ++(str); \
      break; \
    default: \
      x = 0; \
      break; \
    } \
  } while(x); \
})


int handle_hdf(unsigned char* fileBgn, unsigned char* fileEnd, void* pass)
{
  assert(fileBgn);
  assert(fileEnd);
  assert(pass);

  eConfig_t* cfg = (eConfig_t*) pass;
  cfg->chip[0].eCoreRoot = 0x0;
  cfg->chip[0].xyDim = 0;
  cfg->chip[0].type = E__UNKNOWN;
  cfg->emem[0].prot = PROT_NONE;

  int eCores = 0, gen = 0, version = 0;
  unsigned char platform_version[21] = { '\0' };
  unsigned char emem[11] = { '\0' };

  unsigned char *c = fileBgn;
  while(c < fileEnd) {

    SKIP_WSPACE( c );

    // RADIX tree
    hdfKey_t ekey = HDF_INVALID;
    switch(c[0]) {
    CASE( c, &c[1], ekey, 'P', "LATFORM_VERSION", "PLATFORM_VERSION", HDF_PLATFORM_VERSION );
    
    case 'C':
      if(CMP( &c[1], "HIP", sizeof("HIP")-1)) {
        if(c[4] == '_') {
          switch(c[5]) {
          CASE( c, &c[6], ekey, 'R', "OW", "CHIP_ROW", HDF_CHIP_ROW );
          CASE( c, &c[6], ekey, 'C', "OL", "CHIP_COL", HDF_CHIP_COL );
          }          
        }
        else {
          ekey = HDF_CHIP;
          c += sizeof("CHIP")-1;
        }
      }
      break;
    
    case 'N':
      if(CMP( &c[1], "UM_", sizeof("UM_")-1)) {
        switch(c[4]) {
        CASE( c, &c[5], ekey, 'C', "HIPS", "NUM_CHIPS", HDF_NUM_CHIPS );
        CASE( c, &c[5], ekey, 'E', "XT_MEMS", "NUM_EXT_MEMS", HDF_NUM_EXT_MEMS );
        }
      }
      break;
    
    case 'E':
      switch(c[1]) {
      case 'M':
        if(CMP( &c[2], "EM", sizeof("EM")-1)) {
          if(c[4] == '_') {
            switch(c[5]) {
            CASE( c, &c[6], ekey, 'B', "ASE_ADDRESS", "EMEM_BASE_ADDRESS", HDF_EMEM_BASE_ADDRESS );
            CASE( c, &c[6], ekey, 'E', "PI_BASE", "EMEM_EPI_BASE", HDF_EMEM_EPI_BASE );
            CASE( c, &c[6], ekey, 'S', "IZE", "EMEM_SIZE", HDF_EMEM_SIZE );
            CASE( c, &c[6], ekey, 'T', "YPE", "EMEM_TYPE", HDF_EMEM_TYPE );
            }
          }
          else {
            ekey = HDF_EMEM;
            c += sizeof("EMEM")-1;
          }
        }
        break;
      CASE( c, &c[2], ekey, 'S', "YS_REGS_BASE", "ESYS_REGS_BASE", HDF_ESYS_REGS_BASE );
      }
      break;
    }

    // https://www.cplusplus.com/reference/cctype/isspace/
    switch(*c) {
    case ' ':
    case '\t':
    case '\v':
      // whatever ekey detected, it is now valid!
      ++c;

      SKIP_WSPACE( c );

      switch(ekey) {
      case HDF_CHIP:
      {
        if(sscanf((const char*)c, "E%2dG%1d%2d", &eCores, &gen, &version) == 3) {
          switch(eCores << 16 | gen << 8 | version) {
          case 16 << 16 | 3 << 8 | 1:
            cfg->chip[0].xyDim = 4;
            cfg->chip[0].type = E16G301;
            break;
          case 64 << 16 | 4 << 8 | 1:
            cfg->chip[0].xyDim = 8;
            cfg->chip[0].type = E64G401;
            break;
          }
        }
        else
          eCoresWarn("CHIP is not known\n");
        break;
      }
      case HDF_EMEM:
        assert(sizeof(emem) == 11);
        sscanf((const char*)c, "%10s", emem);
        break;

      case HDF_PLATFORM_VERSION:
        assert(sizeof(platform_version) == 21);
        sscanf((const char*)c, "%20s", platform_version);
        break;

      case HDF_NUM_CHIPS:
        sscanf((const char*)c, "%d", &cfg->num_chips);
        break;

      case HDF_NUM_EXT_MEMS:
        sscanf((const char*)c, "%d", &cfg->num_ext_mems);
        break;

      case HDF_EMEM_TYPE:
        if(!strncmp((const char*)c, "RD", 2)) {
          cfg->emem[0].prot = PROT_READ;
          if(!strncmp((const char*)c + 2, "WR", 2))
            cfg->emem[0].prot |= PROT_WRITE;
          break;
        }
        else if(!strncmp((const char*)c, "WR", 2)) {
          cfg->emem[0].prot = PROT_WRITE;
          if(!strncmp((const char*)c + 2, "RD", 2))
            cfg->emem[0].prot |= PROT_READ;
          break;
        }

        eCoresWarn("EMEM_TYPE could not identify. Will keep PROT_NONE\n");
        break;

      case HDF_CHIP_ROW:
      {
        int row;
        if(sscanf((const char*)c, "%d", &row) == 1)
          cfg->chip[0].eCoreRoot = &cfg->chip[0].eCoreRoot[row];
        break;
      }

      case HDF_CHIP_COL:
      {
        int col;
        if(sscanf((const char*)c, "%d", &col) == 1)
          cfg->chip[0].eCoreRoot = (eCoresGMemMap)&cfg->chip[0].eCoreRoot[0][col];
        break;
      }

      case HDF_ESYS_REGS_BASE:
      {
//        sscanf(c, "%s", value);
//        char *endp; // TODO: check error
//        uintptr_t base = strtoul(value, &endp, 16);
        uintptr_t base;
        sscanf((const char*)c, "%p", (void**)&base);
        if(base & ~MASK_4K) {
          eCoresWarn("ESYS_REGS_BASE not 4K page aligned: 0x%08x\n", base);
          base &= MASK_4K;
        }
        cfg->esys_regs_base = (__typeof__(cfg->esys_regs_base))base;
        break;
      }

      case HDF_EMEM_BASE_ADDRESS:
      {
//        sscanf(c, "%s", value);
//        char *endp; // TODO: check error
//        cfg->emem[0].base_address = strtoul(value, &endp, 16);
        sscanf((const char*)c, "%p", (void**)&cfg->emem[0].base_address);
        if(cfg->emem[0].base_address & ~MASK_4K) {
          eCoresWarn("EMEM_BASE_ADDRESS not 4K page aligned: 0x%08x\n",
                  cfg->emem[0].base_address);
          cfg->emem[0].base_address &= MASK_4K;
        }
        break;
      }

      case HDF_EMEM_EPI_BASE:
      {
//        sscanf((const char*)c, "%s", value);
//        char *endp; // TODO: check error
//        uintptr_t base = strtoul(value, &endp, 16);
        uintptr_t base;
        sscanf((const char*)c, "%p", (void**)&base);
        if(base & ~MASK_4K) {
          eCoresWarn("EMEM_EPI_BASE not 4K page aligned: 0x%08x\n", base);
          base &= MASK_4K;
        }
        cfg->emem[0].epi_base = (__typeof__(cfg->emem[0].epi_base))base;
        break;
      }

      case HDF_EMEM_SIZE:
      {
//        sscanf((const char*)c, "%s", value);
//        char *endp; // TODO: check error
//        cfg->emem[0].size = strtoul(value, &endp, 16);
        sscanf((const char*)c, "%p", (void**)&cfg->emem[0].size);
        if(cfg->emem[0].size & ~MASK_4K) {
          eCoresWarn("EMEM_SIZE not 4K page aligned: 0x%08x\n",
                  cfg->emem[0].size);
          cfg->emem[0].size &= MASK_4K;
        }
        break;
      }

      case HDF_INVALID:
        break;
      }

      break;
    default:
      break;
    }

    // get to next line
    unsigned x = 1;
    do {
      switch(*(c++)) {
      case '\n':
      case '\f':
      case '\r':

      case '\0':
        x = 0;
        break;
      }
    } while(x);
  }
  
  if(! cfg->chip[0].eCoreRoot
     || ! cfg->chip[0].xyDim
     || ! cfg->chip[0].type) {
    eCoresError("Could not determine eCores type, root and end\n");
    return -1;
  }
  
  //FIXME: needs to iterate !
  if(!((((uintptr_t)cfg->esys_regs_base) ^ (uintptr_t)cfg->chip[0].eCoreRoot) & ~ECORE_ADDR_LCLMASK)) {
    eCoresWarn("ESYS_REGS_BASE eCores FPGA regs overlap with root eCore.\n");
  }

  eCoresPrintf(E_DBG, "├ PLATFORM_VERSION   %s\n"
         "[xx,xx] ├ ESYS_REGS_BASE     %p\n"
         "[xx,xx] ├ NUM_CHIPS          %d\n"
         "[xx,xx] ├ CHIP               E%02dG%1d%02d\n"
         "[xx,xx] ├ CHIP_ROW           %d\n"
         "[xx,xx] ├ CHIP_COL           %d\n"
         "[xx,xx] ├ NUM_EXT_MEMS       %d\n"
         "[xx,xx] ├ EMEM               %s\n"
         "[xx,xx] ├ EMEM_BASE_ADDRESS  0x%08x\n"
         "[xx,xx] ├ EMEM_EPI_BASE      %p\n"
         "[xx,xx] ├ EMEM_SIZE          0x%08x\n"
         "[xx,xx] └ EMEM_TYPE          %s%s\n",
    platform_version,
    cfg->esys_regs_base,
    cfg->num_chips,
    eCores, gen, version,
    ECORE_ADDR_ROWID(cfg->chip[0].eCoreRoot),
    ECORE_ADDR_COLID(cfg->chip[0].eCoreRoot),
    cfg->num_ext_mems,
    emem,
    cfg->emem[0].base_address,
    cfg->emem[0].epi_base,
    cfg->emem[0].size,
    (cfg->emem[0].prot & PROT_READ) ? "RD" : "",
    (cfg->emem[0].prot & PROT_WRITE) ? "WR" : "");

  return 0;
} 

// TODO: backup with getenv("EPIPHANY_HOME")"/bsps/current/platform.hdf"
int load_default_hdf(eConfig_t *cfg)
{
  assert(cfg);

  const char *ext[] = { "hdf" };
  return load_file(getenv("EPIPHANY_HDF"), elemsof(ext), ext, handle_hdf, cfg);
}

