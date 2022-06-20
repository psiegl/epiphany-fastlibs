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

/*
CHECK ALSO:
S30B00000628E20FE8FFFFFFF0
S3150000062E5CD600278B19F2000B00021052010B014B
S3150000063E22010B00021052014CD600241BB40224D8
S3090000064E4F19020434
S315000006607C9402070B000200FCD701040B01021068
S3150000067012C17C170124FC5601247CD60124FC15EA
S3150000068001847C550184FC9401847CD40184FC178C
S3150000069000A47C5700A4920312217A24022192011D
S315000006A08B0802000B2802000B2002100B00021020


#if 0
  memset(Epiphany.core[1][1].mems.mapped_base, 0, 0x8000);

  const char* str[] = { "S30B00000628E20FE8FFFFFFF0\r\n",
                        "S3150000062E5CD600278B19F2000B00021052010B014B\r\n" };
  printf("%p\n", Epiphany.core[1][1].mems.mapped_base);

  parse_srec(str[0], str[0] + strlen(str[0]), (void*)0x84900000, (void*)0x84900000); //Epiphany.core[1][1].mems.mapped_base, Epiphany.core[1][1].mems.mapped_base);
  parse_srec(str[1], str[1] + strlen(str[1]), (void*) 0x84900000, (void*)0x84900000); //Epiphany.core[1][1].mems.mapped_base, Epiphany.core[1][1].mems.mapped_base);
  printf("start dumping\n");
  dump_mem((uint32_t*)0x84900000, 0x8000);
#endif
*/


// S3  11  00000058  0B 6E E2 00 0B 60 02 10 52 0D 00 00  5F
  const char *srecbgn =  "S311000000580B6EE2000B600210520D00005F\r\n";
  char* srecend = (char*)srecbgn + strlen(srecbgn);
  uintptr_t address = 0x58;
  uint8_t data[] = { 0x0B, 0x6E, 0xE2, 0x00,
                     0x0B, 0x60, 0x02, 0x10,
                     0x52, 0x0D, 0x00, 0x00 };
  
  int ret = MEASURE("parse_srec", parse_srec(srecbgn, srecend,
                                  eCoreBgn, eCoreBgn));

  char* emem = (char*)&eCoreBgn->sram[ address ];

  return memcmp(emem, data, sizeof(data));
}
