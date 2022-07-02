/*
lowpower-mode.c

Copyright (C) 2012 Adapteva, Inc.
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

#include <e-hal.h>

int main(int argc, char *argv[])
{
	e_platform_t platform;
	e_epiphany_t dev;

	// initialize system, read platform params from
	// default HDF. Then, reset the platform and
	// get the actual system parameters.
	e_set_host_verbosity(H_D0);
	e_init(NULL);
	e_get_platform_info(&platform);
	
    	// Open a workgroup
	e_open(&dev, 0, 0, platform.rows, platform.cols);
	
	//Enable clock gating on every core the host
	for (unsigned i=0; i<platform.rows; i++) {
	  for (unsigned j=0; j<platform.cols; j++) {
	    //eCore clock gating
	    unsigned int data=0x00400000;
	    e_write(&dev, i, j, 0xf0400, &data, sizeof(data));
	    //eMesh clock gating
	    data=0x00000002;
	    e_write(&dev, i, j, 0xf0700, &data, sizeof(data));
	  }
	}
	
	
	// Close the workgroup
	e_close(&dev);
	
	// Release the allocated buffer and finalize the
	e_finalize();

	return 0;
}

 
