// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText:  2022 Patrick Siegl <code@siegl.it>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "memmap-epiphany-cores.h"
#include "ehal-print.h"
#include "loader/ehal-gen-file-loader.h"


#define elemsof( x ) (sizeof(x)/sizeof(x[0]))


#define SWITCH_HEX( c, ret )                       \
  switch( c ) {                                    \
  case 'A': case 'B':                              \
  case 'C': case 'D':                              \
  case 'E': case 'F':                              \
    ret = (ret << 4) | (uintptr_t)(c - '7');       \
    break;                                         \
  case 'a': case 'b':                              \
  case 'c': case 'd':                              \
  case 'e': case 'f':                              \
    ret = (ret << 4) | (uintptr_t)(c - 'W');       \
    break;                                         \
  case '0': case '1':                              \
  case '2': case '3':                              \
  case '4': case '5':                              \
  case '6': case '7':                              \
  case '8': case '9':                              \
    ret = (ret << 4) | (uintptr_t)(c - '0');       \
    break;                                         \
  default:                                         \
    eCoresError("not a hex: '%c'\n", c );          \
    return -1;                                     \
  }

// Address bytes are used as a group
// (input is formated into big endian)
int srecGroupToBytes(uintptr_t* bytesOut,
                     unsigned char *srecPairCharIn, unsigned srecBytes,
                     unsigned char *chksum)
{
  assert(bytesOut);
  assert(srecPairCharIn);
  assert(chksum);

  uintptr_t ret = 0;
  for(unsigned i = 0; i < srecBytes; ++i) {
    unsigned char c = srecPairCharIn[i];
    SWITCH_HEX( c, ret ); // contains return -1!
  }
  *bytesOut = ret;
  *chksum += ((ret >> 24) & 0xFF) + ((ret >> 16) & 0xFF)
             + ((ret >> 8) & 0xFF) + (ret & 0xFF);
  return 0;
}

// Count, Data and Checksum bytes are used from pairs
// (input is formated into host endianness likel little endian)
int srecPairsToBytes(unsigned char* bytesOut,
                     unsigned char *srecPairCharIn, unsigned srecPairs,
                     unsigned char *chksum)
{
  assert(bytesOut);
  assert(srecPairCharIn);
  assert(chksum);

  for(unsigned i = 0; i < srecPairs; ++i) {
    char ret = 0;
    for(unsigned j = 0; j < 2; ++j) {
      unsigned char c = srecPairCharIn[(i << 1) + j];
      SWITCH_HEX( c, ret ); // contains return -1!
    }
    *chksum += ret;
    bytesOut[i] = ret;
  }
  return 0;
}


// Count, Data and Checksum bytes are used from pairs
// (input is formated into host endianness like little endian)
char buf[0xFF] __attribute__ ((aligned (sizeof(uintptr_t))));
int srecPairsToBytes_eCoreLocal(unsigned char* addr,
                                eCoreMemMap_t* eCoreBgn, eCoreMemMap_t* eCoreEnd,
                                unsigned char *srecPairCharIn, unsigned srecPairs,
                                unsigned char *chksum)
{
  assert(eCoreBgn);
  assert(eCoreEnd);
  assert(srecPairCharIn);
  assert(chksum);

  for(unsigned i = 0; i < srecPairs; ++i) {
    char ret = 0;
    for(unsigned j = 0; j < 2; ++j) {
      unsigned char c = srecPairCharIn[(i << 1) + j];
      SWITCH_HEX( c, ret ); // contains return -1!
    }
    buf[i] = ret;
    *chksum += ret;
  }

/*
    epiphany_arch_ref.pdf, REV 14.03.11 page 27
    eMesh -> Maximum bandwidth is obtained with double word transactions.
*/
  for(uintptr_t r = ECORE_MASK_ROWID( eCoreEnd );
      r >= ECORE_MASK_ROWID( eCoreBgn ); r -= ECORE_ONE_ROW) {
    for(uintptr_t c = ECORE_MASK_COLID( eCoreEnd );
        c >= ECORE_MASK_COLID( eCoreBgn ); c -= ECORE_ONE_COL) {

      eCoreMemMap_t* cur = (eCoreMemMap_t*)(r | c);
      volatile unsigned char* eaddr = cur->sram + (uintptr_t)addr;

#if 1
      unsigned s;
      for(s = 0; s < srecPairs; s++)
        eaddr[s] = buf[s];
#else
      memcpy(eaddr, buf, srecPairs);
#endif
    }
  }
  return 0;
}



#define COUNT__SREC_BYTES       sizeof(uint16_t)
#define COUNT__SREC_PAIRS       (COUNT__SREC_BYTES >> 1)

#define ADDR_16bit__SREC_BYTES  sizeof(uint32_t)
#define ADDR_16bit__SREC_PAIRS  (ADDR_16bit__SREC_BYTES >> 1)
#define ADDR_16bit__BYTES       sizeof(uint16_t)

#define ADDR_32bit__SREC_BYTES  sizeof(uint64_t)
#define ADDR_32bit__SREC_PAIRS  (ADDR_32bit__SREC_BYTES >> 1)
#define ADDR_32bit__BYTES       sizeof(uint32_t)

#define CHKSUM__SREC_BYTES      sizeof(uint16_t)
#define CHKSUM__SREC_PAIRS      (CHKSUM__SREC_BYTES >> 1)
#define CHKSUM__BYTES           sizeof(uint8_t)

typedef struct {
  eCoreMemMap_t* eCoreBgn;
  eCoreMemMap_t* eCoreEnd;
  char* eMemBase;
  uint32_t eMemSize;
} eCores;

//int parse_srec(unsigned char *srecBgn, unsigned char *srecEnd,
//               eCoreMemMap_t* eCoreBgn, eCoreMemMap_t* eCoreEnd)
int handle_srec(unsigned char* srecBgn, unsigned char* srecEnd, void* pass)
{
  unsigned recCount = 0;
  for( ; srecBgn < srecEnd ; ) {
    unsigned char chksum = 0;

    if( *(srecBgn++) != 'S' ) {
      eCoresError("hex of 'S' wrong\n"); 
      return -1;
    }
    
    unsigned recType = *(srecBgn++); // checked later

    uint8_t countBytes;
    if(srecPairsToBytes(&countBytes, srecBgn, COUNT__SREC_PAIRS, &chksum)) {
      eCoresError("hex of 'count' wrong\n"); 
      return -1;
    }
    unsigned char *srecAddr = srecBgn + COUNT__SREC_BYTES;

    switch( recType ) {
      default:
        return -1;

      case '0':
      {
        // ----------- calc. lenghts and pointers
        if(countBytes < (ADDR_16bit__BYTES + CHKSUM__BYTES)) {
          eCoresError("countBytes wrong\n"); 
          return -1;
        }
        unsigned dataBytes = countBytes - ADDR_16bit__BYTES - CHKSUM__BYTES;
        unsigned data__srecBytes = dataBytes << 1;
        unsigned data__srecPairs = dataBytes;

        unsigned char *srecData = srecAddr + ADDR_16bit__SREC_BYTES;
        srecBgn = srecData + data__srecBytes;


        // ----------- retrieve data
        if(*(srecAddr++) != '0'
           || *(srecAddr++) != '0'
           || *(srecAddr++) != '0'
           || *(srecAddr++) != '0') {
          eCoresError("hex of 'addr' wrong\n"); 
          return -1;
        }

        unsigned char comment[10+1+1+18]; // Definition: mname[20], ver[2], rev[2], descr[0-36]
        comment[dataBytes] = '\0';
        if(srecPairsToBytes(comment, srecData, data__srecPairs, &chksum)) {
          eCoresError("hex of 'comment' wrong\n"); 
          return -1;
        }

        eCoresPrintf(E_DBG, "└ SREC header: %s\n", comment);
        break;
      }

      case '1':
      case '2':
      case '3':
      {
        // ----------- calc. lenghts and pointers
        unsigned addrBytes = ADDR_16bit__BYTES + (recType - '1');
        unsigned addr__srecBytes = addrBytes << 1;

        if(countBytes < (addrBytes + CHKSUM__BYTES)) {
          eCoresError("countBytes wrong\n"); 
          return -1;
        }
        unsigned dataBytes = countBytes - addrBytes - CHKSUM__BYTES;
        unsigned data__srecBytes = dataBytes << 1;
        unsigned data__srecPairs = dataBytes;

        unsigned char *srecData = srecAddr + addr__srecBytes;
        srecBgn = srecData + data__srecBytes;

        // ----------- retrieve data
        uintptr_t addr;
        if(srecGroupToBytes(&addr, srecAddr, addr__srecBytes, &chksum)) {
          eCoresError("hex of 'addr' wrong\n"); 
          return -1;
        }

        // There can be 3 types of addresses:
        // 1) 000 -> 'local'
        // 2) XXX -> a) 'global' eCore
        //           b) eDRAM
        // In case 1), add eCore offset and write to eCore
        // In case 2), check if within the range of eCore or eDRAM. If success write, otherwise failure.

        // TODO: One could check in case of writing to eCore, if it hits bank and regs or if it is outside
        //       ECORE_ADDR_LOCAL(addr) < sizeof(((eCoreMemMap_t*)0x0)->bank)
        eCores* epass = (eCores*)pass;
        __typeof__(epass->eCoreBgn) eCoreBgn = epass->eCoreBgn;
        __typeof__(epass->eCoreEnd) eCoreEnd = epass->eCoreEnd;

        // 1) local
        if( ! ECORE_ADDR_ROWID(addr)
           && ! ECORE_ADDR_COLID(addr) ) {
          if(srecPairsToBytes_eCoreLocal((unsigned char*)addr, eCoreBgn, eCoreEnd,
                                         srecData, data__srecPairs, &chksum))
            return -1; // One could also just warn that a line is broken
        }
                // 2a) 'global' eCore
        else if((ECORE_ADDR_ROWID(eCoreBgn) <= ECORE_ADDR_ROWID(addr)
                 && ECORE_ADDR_ROWID(addr) <= ECORE_ADDR_ROWID(eCoreEnd)
                 && ECORE_ADDR_COLID(eCoreBgn) <= ECORE_ADDR_COLID(addr)
                 && ECORE_ADDR_COLID(addr) <= ECORE_ADDR_COLID(eCoreEnd))
                // 2b) eDRAM
                || (epass->eMemBase <= (char*)addr
                    && (char*)addr < (epass->eMemBase+epass->eMemSize))) {
          if(srecPairsToBytes((unsigned char*)addr,
                              srecData, data__srecPairs, &chksum))
            return -1; // One could also just warn that a line is broken
        }
        
        ++recCount;
        break;
      }

      case '7':
      case '8':
      case '9':
      {
        // ----------- calc. lenghts and pointers
        unsigned addrBytes = ADDR_32bit__BYTES - (recType - '7');
        unsigned addr__srecBytes = addrBytes << 1;

        if(countBytes != (addrBytes + CHKSUM__BYTES)) {
          eCoresError("countBytes wrong\n"); 
          return -1;
        }
        srecBgn = srecAddr + addr__srecBytes;


        // ----------- retrieve data
        uintptr_t addr;
        if(srecGroupToBytes(&addr, srecAddr, addr__srecBytes, &chksum)) {
          eCoresError("hex of 'addr' wrong\n"); 
          return -1;
        }

        // TODO: CHECK addr

        recCount = 0; // correct?
        break;
      }
      
      case '5':
      case '6':
      {
        // ----------- calc. lenghts and pointers
        unsigned dataBytes = sizeof(uint16_t) + (recType - '5');
        unsigned data__srecBytes = dataBytes << 1;

        if(countBytes != (dataBytes + CHKSUM__BYTES)) {
          eCoresError("countBytes wrong\n"); 
          return -1;
        }
        srecBgn = srecAddr + data__srecBytes;


        // ----------- retrieve data
        uintptr_t parsedRecCount;
        if(srecGroupToBytes(&parsedRecCount, srecBgn, data__srecBytes, &chksum)) {
          eCoresError("hex of 'count of prev. records' wrong\n"); 
          return -1;
        }

        if(recCount != parsedRecCount)
          eCoresWarn("record count differs: %d vs %d\n", recCount, parsedRecCount);

        break;
      }
    }

    // This is a trick. srecPairsToBytes adds parsedChecksum already to chksum.
    // the checksum itself is the negated form of parsedChecksum.
    // hence:
    // chksum + srecParisToBytes(parsedChecksum) = 0xFF
    // as ~chksum == srecParisToBytes(parsedChecksum)
    unsigned char parsedChecksum;
    if(srecPairsToBytes(&parsedChecksum, srecBgn, CHKSUM__BYTES, &chksum)) {
      eCoresError("hex of 'checksum' wrong\n"); 
      return -1;
    }
    if(chksum != 0xFF) {
      eCoresError("checksum is wrong! %x vs 0xFF\n", chksum);
      return -1;
    }
    
    srecBgn += CHKSUM__SREC_BYTES;

    // if below is not given, not a problem right now.
    // next round will expect 'S' again.      
    srecBgn += (*srecBgn == '\r');
    srecBgn += (*srecBgn == '\n');
  }

  return 0;
}

// TODO: check if supplied eCoreBgn and eCoreEnd are within the given range

// public API
int parse_srec(unsigned char *srecBgn, unsigned char *srecEnd,
               eCoreMemMap_t* eCoreBgn, eCoreMemMap_t* eCoreEnd)
{
  eCores data = {
    .eCoreBgn = eCoreBgn,
    .eCoreEnd = eCoreEnd,
    .eMemBase = (char*) 0x8e000000, // TODO: get directly!
    .eMemSize = 0x2000000           // TODO: get directly!
  };
  return handle_srec(srecBgn, srecEnd, &data);
}

// public API
int load_srec(const char *srecFile, eCoreMemMap_t* eCoreBgn, eCoreMemMap_t* eCoreEnd)
{
  const char *ext[] = {
    "srec", "sx", "mot", "mxt", "exo",
    "s19", "s28", "s37", "s", "s1", "s2", "s3"
  };

  eCores data = {
    .eCoreBgn = eCoreBgn,
    .eCoreEnd = eCoreEnd,
    .eMemBase = (char*) 0x8e000000, // TODO: get directly!
    .eMemSize = 0x2000000           // TODO: get directly!
  };
  return load_file(srecFile, elemsof(ext), ext, handle_srec, &data);
}

