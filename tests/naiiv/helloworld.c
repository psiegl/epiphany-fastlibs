// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText:  2022 Patrick Siegl <code@siegl.it>

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <memmap-epiphany-cores.h>
#include <loader/ehal-srec-loader.h>

#define MEASURE( str, X ) \
({ \
  struct timeval tbgn, tend; \
  gettimeofday(&tbgn, NULL); \
  int ret = X; \
  gettimeofday(&tend, NULL); \
  printf("%s: measured: %ld μs\n", \
         str, \
         ((tend.tv_sec * 1000000 + tend.tv_usec) \
         - (tbgn.tv_sec * 1000000 + tbgn.tv_usec))); \
  ret; \
})

// 2016.11 epiphany-hal.c

#ifndef ECORES_NEXT
#define ECORE_NEXT( ROWS, COLS ) ( (ROWS) * ECORES_MAX_DIM + (COLS) )
#endif


int main(int argc, char* argv[])
{
  // TODO: get from library
  eCoreMemMap_t* eCoreBgn = 0x0;
  eCoreBgn += ECORE_NEXT( 32, 8 );
  assert(eCoreBgn == (void*)0x80800000);
  
// S3  11  00000058  0B 6E E2 00 0B 60 02 10 52 0D 00 00  5F
  const char *srectest =  "S311000000580B6EE2000B600210520D00005F\r\n";
  uintptr_t address = 0x58;
  uint8_t data[] = { 0x0B, 0x6E, 0xE2, 0x00,
                     0x0B, 0x60, 0x02, 0x10,
                     0x52, 0x0D, 0x00, 0x00 };
  
  int ret = MEASURE("parse_srec", parse_srec(srectest, srectest + strlen(srectest),
                                  eCoreBgn, eCoreBgn));

  char* emem = (char*)&eCoreBgn->sram[ address ];

  return memcmp(emem, data, sizeof(data));
}
