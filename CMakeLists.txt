cmake_minimum_required(VERSION 2.8)
project( ehal )
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Adapteva EPIPHANY efficient HAL")
set(CPACK_PACKAGE_VERSION 0.1.0)
set(CMAKE_C_STANDARD 90)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)
add_library(ehal SHARED
	src/state/ident-adapteva-epiphany.c
	src/state/ident-xilinx-zynq.c
	src/loader/ehal_generic_file_loader.c
	src/loader/ehal_hdf_loader.c
	src/loader/ehal_srec_loader.c
	src/ehal-mmap.c
	src/ehal.c
)

set_target_properties(ehal PROPERTIES
	PUBLIC_HEADER "inc/memmap-epiphany-cores.h;inc/loader/ehal_srec_loader.h"
)
#set_target_properties(ehal PROPERTIES VERSION ${PROJECT_VERSION})

install(TARGETS ehal
	LIBRARY DESTINATION /usr/lib
	PUBLIC_HEADER DESTINATION /usr/include
)
#install(FILES inc/ehal.h DESTINATION include)



include(CTest)



include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)
link_directories(${CMAKE_BINARY_DIR}/)
add_executable(helloworld.elf tests/naiiv/helloworld.c)
target_link_libraries(helloworld.elf PRIVATE libehal.so)
add_test(NAME helloworld
        COMMAND sudo ELOGLEVEL=0 EPIPHANY_HDF=${CMAKE_CURRENT_SOURCE_DIR}/misc/platform.hdf ${CMAKE_BINARY_DIR}/helloworld.elf)

add_executable(naiiv.elf tests/naiiv/main.c)
target_link_libraries(naiiv.elf PRIVATE libehal.so)
add_test(NAME naiiv
	COMMAND sudo ELOGLEVEL=5 EPIPHANY_HDF=${CMAKE_CURRENT_SOURCE_DIR}/misc/platform.hdf ${CMAKE_BINARY_DIR}/naiiv.elf)
