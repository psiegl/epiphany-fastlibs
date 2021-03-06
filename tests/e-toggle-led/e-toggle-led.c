/*
e-toggle-led.c

Copyright (C) 2013 Adapteva, Inc.
Contributed by Andreas Olofsson <support@adapteva.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program, see the file COPYING. If not, see
<http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <e-hal.h>

void usage();

int main(int argc, char *argv[])
{
  e_platform_t platform;
  e_epiphany_t dev;

  unsigned int led_state;

  if(argc < 2){
    usage();
    exit(1);
  }
  else{
    led_state = atoi(argv[1]);
  }
  // initialize system, read platform params from
  // default HDF. Then, reset the platform and
  // get the actual system parameters.

  e_set_host_verbosity(H_D0);
  e_init(NULL);
  e_reset_system( /* NEEDS A FIX - psiegl */ &dev  );
  e_get_platform_info(&platform);

  // Open a workgroup
  e_open(&dev, 0, 0, platform.rows, platform.cols);

  //Writing Message to Core (BAD MONKEY!)
  e_write(&dev, 0, 0, 0x6000, &led_state, sizeof(led_state));

  //Running "tooggle LED program" from core (0,0)
  e_load("/home/linaro/epiphany-fastlibs/bld/tests/e-toggle-led/device-e-toggle-led.srec", &dev, 0, 0, E_TRUE);
  
  // Close the workgroup
  e_close(&dev);
  
  // Release the allocated buffer and finalize the
  e_finalize();
  
  return 0;
}
void usage(){
  printf("Usage: e-toggle-led <state>\n\n");
  printf("<state>:\n");
  printf(" 1 = LED On\n");
  printf(" 0 = LED Offf\n");
}

 
