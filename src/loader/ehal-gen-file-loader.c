// SPDX-License-Identifier: BSD-2-Clause
// SPDX-FileCopyrightText:  2022 Patrick Siegl <code@siegl.it>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
//#include <asm-generic/mman.h>
#include "ehal-print.h"

int load_file(const char *fname, unsigned extc, const char** ext,
              int (*fnc)(unsigned char* fileBgn, unsigned char* fileEnd, void *pass),
              void *pass)
{
  assert(ext);
  assert(fnc);

  if(!fname) {
    eCoresError("No file supplied\n");
    return -1;
  }

  char *end;
  if((end = strrchr(fname, '.')) == NULL) {
    eCoresError("No file extension\n");
    return -1;
  }
  ++end; // skip dot

  const char **e;
  for(e = &ext[0]; e < &ext[extc] && strcmp(end, *e); ++e);
  if(e == &ext[extc]) {
    eCoresError("No supported file extension given\n");
    return -1;
  }

  eCoresPrintf(E_DBG, "Reading file %s\n", fname);

  int fd, ret = -1;
  if( (fd = open(fname, O_RDONLY)) >= 0 ) {
    struct stat s;
    if( fstat(fd, &s) >= 0 ) {
      size_t size = s.st_size;
      
      // TODO: check if file is too huge to read at one shot ...
      
      unsigned char *mappedFile;
      if( (mappedFile = mmap(0, size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0)) != MAP_FAILED ) {
        if( madvise(mappedFile, size, MADV_SEQUENTIAL | MADV_WILLNEED) )
          eCoresWarn("Could not madvise %s, %s\n", fname, strerror(errno));

        ret = (*fnc)(mappedFile, mappedFile + size, pass);

        munmap( (void*)mappedFile, size );
      }
      else
        eCoresError("Could not mmap %s, %s\n", fname, strerror(errno));
    }
    else
      eCoresError("Could not fstat %s, %s\n", fname, strerror(errno));

    close( fd );
  }
  else
    eCoresError("Could not open %s, %s\n", fname, strerror(errno));

  return ret;
}

