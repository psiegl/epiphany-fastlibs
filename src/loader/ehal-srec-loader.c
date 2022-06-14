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


#define EMEM_SIZE (0x02000000) // TODO: remove



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

  unsigned i;
  uintptr_t ret = 0;
  for(i = 0; i < srecBytes; ++i) {
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

  unsigned i, j;
  for(i = 0; i < srecPairs; ++i) {
    char ret = 0;
    for(j = 0; j < 2; ++j) {
      unsigned char c = srecPairCharIn[(i << 1) + j];
      SWITCH_HEX( c, ret ); // contains return -1!
    }
    *chksum += ret;
    bytesOut[i] = ret;
  }
  return 0;
}

#define EMEMCPY 1
#define ALIGNMENT sizeof(uint32_t)   // EPIPHANY reads 32 bit
#define MIN(a, b) (a > b ? a : b)

inline void ememcpy(void *dest, const void *src, size_t n)
{
  /*
    dest
    |<-            n            ->|
    |<- b_n ->|<- a_n ->|<- e_n ->|
              b_dest    e_dest
  */
  uintptr_t ob_dest = (uintptr_t)dest;
  uintptr_t b_dest = ob_dest, e_dest = ob_dest + n;

  // round up
  b_dest += (uintptr_t)(ALIGNMENT-1);
  b_dest &= ~(uintptr_t)(ALIGNMENT-1);
  /*
    best case
    dest
    |<-            n            ->|
    |<- b_n ->|
              b_dest

    worst case
    dest
    |<-n->|                     -> b_n = n
    |<-     ->|
              b_dest
  */
  size_t b_n = MIN(b_dest - ob_dest, n);

  // round down
  e_dest &= ~(uintptr_t)(ALIGNMENT-1);
  /*
    best case 
    dest
    |<-            n            ->|
    |<- b_n ->|<- a_n ->|
              b_dest    e_dest

    worst case (1)
    b_n is part of n
    dest
    |<-    n    ->|             -> a_n = n - b_n
    |<- b_n ->|<-     ->|
              b_dest    e_dest

    worst case (2)
    b_n is n
    dest
    |<-  n  ->|                 -> a_n = n - b_n = 0
    |<- b_n ->|<-     ->|
              b_dest    e_dest
  */
  size_t a_n = MIN(e_dest - ob_dest, n) - b_n;

  /*
    best case
    dest
    |<-            n            ->|
    |<- b_n ->|<- a_n ->|<- e_n ->|
              b_dest    e_dest

    worst case
    dest
    |<-       n       ->|       -> e_n = 0
    |<- b_n ->|<- a_n ->|
              b_dest    e_dest
  */
  size_t e_n = n - b_n - a_n;

  char *c_dest = (char*)dest;
  char *c_src = (char*)src;

  while( b_n-- )
    *(c_dest++) = *(c_src++);

#ifdef __builtin_memcpy_inline
  __builtin_memcpy_inline(c_dest, c_src, a_n);
#else
  memcpy(c_dest, c_src, a_n);
#endif
  c_dest += a_n;
  c_src += a_n;

  while( e_n-- )
    *(c_dest++) = *(c_src++);
}


// Count, Data and Checksum bytes are used from pairs
// (input is formated into host endianness likel little endian)
int srecPairsToBytes_eCoreLocal(unsigned char* addr,
                                eCoreMemMap_t* eCoreBgn, eCoreMemMap_t* eCoreEnd,
                                unsigned char *srecPairCharIn, unsigned srecPairs,
                                unsigned char *chksum)
{
  assert(eCoreBgn);
  assert(eCoreEnd);
  assert(srecPairCharIn);
  assert(chksum);

#define LCL_BUF
#ifdef LCL_BUF
  char t[0xFF];
#endif
  unsigned i, j;
  for(i = 0; i < srecPairs; ++i) {
    char ret = 0;
    for(j = 0; j < 2; ++j) {
      unsigned char c = srecPairCharIn[(i << 1) + j];
      SWITCH_HEX( c, ret ); // contains return -1!
    }
    *chksum += ret;
#ifdef LCL_BUF
    t[i] = ret;
#else
    uint32_t MASK_ROWID = 0xFC000000; // FIXME: cleanup
    uint32_t INCR_ROWID = 0x04000000;
    uint32_t MASK_COLID = 0x03F00000;
    uint32_t INCR_COLID = 0x00100000;

/*
    epiphany_arch_ref.pdf, REV 14.03.11 page 27
    eMesh -> Maximum bandwidth is obtained with double word transactions.
*/
    uint32_t r, c;
#if 0
    for(uint32_t r = (((uintptr_t)eCoreBgn) & MASK_ROWID);
        r <= (((uintptr_t)eCoreEnd) & MASK_ROWID); r += INCR_ROWID) {
      for(uint32_t c = (((uintptr_t)eCoreBgn) & MASK_COLID);
          c <= (((uintptr_t)eCoreEnd) & MASK_COLID); c += INCR_COLID) {
        uintptr_t eAddr = r | c | (uintptr_t)&addr[i];
        //printf("[%2d,%2d] eAddr: %08x  %p\n", ECORE_ADDR_ROWID( eAddr ), ECORE_ADDR_COLID( eAddr ), eAddr, &((char*)addr)[i]);
        *(unsigned char*)eAddr = ret;
      }
    }
#else
    // in theory it could be faster to first fill the far eCores
    for(r = (((uintptr_t)eCoreEnd) & MASK_ROWID);
        r >= (((uintptr_t)eCoreBgn) & MASK_ROWID); r -= INCR_ROWID) {
      for(c = (((uintptr_t)eCoreEnd) & MASK_COLID);
          c >= (((uintptr_t)eCoreBgn) & MASK_COLID); c -= INCR_COLID) {
        uintptr_t eAddr = r | c | (uintptr_t)&addr[i];
        //printf("[%2d,%2d] eAddr: %08x (%p, %d) write %x\n", ECORE_ADDR_ROWID( eAddr ), ECORE_ADDR_COLID( eAddr ), eAddr, &((char*)addr)[i], i, ret);
        *(unsigned char*)eAddr = ret;
      }
    }
#endif
#endif
  }

#ifdef LCL_BUF // use ECORE_NEXT
  uint32_t MASK_ROWID = 0xFC000000;
  uint32_t INCR_ROWID = 0x04000000;
  uint32_t MASK_COLID = 0x03F00000;
  uint32_t INCR_COLID = 0x00100000;
  uint32_t r, c;
  for(r = (((uintptr_t)eCoreEnd) & MASK_ROWID);
      r >= (((uintptr_t)eCoreBgn) & MASK_ROWID); r -= INCR_ROWID) {
    for(c = (((uintptr_t)eCoreEnd) & MASK_COLID);
        c >= (((uintptr_t)eCoreBgn) & MASK_COLID); c -= INCR_COLID) {

#ifdef EMEMCPY
      uintptr_t eAddr = r | c | (uintptr_t)&addr[0];
      ememcpy((uintptr_t*)eAddr, t, srecPairs);
#else
      if(!((uintptr_t)addr % sizeof(uint32_t)) // EPIPHANY reads 32bit
/*         && !(srecPairs % sizeof(uint32_t)) */) {
        uintptr_t eAddr = r | c | (uintptr_t)&addr[0];
        memcpy((char*)eAddr, t, srecPairs);
      }
      else {
//        printf("!! %p %d\n", addr, srecPairs);
        for(i = 0; i < srecPairs; ++i) {
          uintptr_t eAddr = r | c | (uintptr_t)&addr[i];
          if(! (((uintptr_t)&addr[i]) % sizeof(uint32_t))) { // EPIPHANY reads 32bit
//            printf("%p %p %d\n", &addr[i], &t[i], srecPairs - i );
            memcpy((uintptr_t*)eAddr, &t[i], srecPairs - i);
            break;
          }
          *(unsigned char*)eAddr = t[i];
        }
      }
#endif /* EMEMCPY */

    }
  }
#endif

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
} eCores;

//int parse_srec(unsigned char *srecBgn, unsigned char *srecEnd,
//               eCoreMemMap_t* eCoreBgn, eCoreMemMap_t* eCoreEnd)
int handle_srec(unsigned char* srecBgn, unsigned char* srecEnd, void* pass)
{
  eCoreMemMap_t* eCoreBgn = ((eCores*)pass)->eCoreBgn;
  eCoreMemMap_t* eCoreEnd = ((eCores*)pass)->eCoreEnd;

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
                || (0x8e000000 <= addr // TODO
                    && addr < (0x8e000000+EMEM_SIZE))) { // TODO
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

// public API
int parse_srec(unsigned char *srecBgn, unsigned char *srecEnd,
               eCoreMemMap_t* eCoreBgn, eCoreMemMap_t* eCoreEnd)
{
  eCores data = {
    .eCoreBgn = eCoreBgn,
    .eCoreEnd = eCoreEnd
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
    .eCoreEnd = eCoreEnd
  };
  return load_file(srecFile, elemsof(ext), ext, handle_srec, &data);
}

