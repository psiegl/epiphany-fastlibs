cmake_minimum_required(VERSION 2.8.12.2)
project( ehal )
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Adapteva EPIPHANY efficient HAL")
set(CPACK_PACKAGE_VERSION 0.1.0)
set(CMAKE_C_STANDARD 90)

set(SOURCE
	src/state/ident-adapteva-epiphany.c
	src/state/ident-xilinx-zynq.c
	src/loader/ehal-gen-file-loader.c
	src/loader/ehal-hdf-loader.c
	src/loader/ehal-srec-loader.c
	src/ehal-mmap.c
	src/ehal.c
)

set(HEADER
	${CMAKE_CURRENT_SOURCE_DIR}/inc
)


option(EHAL_BACKWARD_COMPATIBILITY ON)
if(EHAL_BACKWARD_COMPATIBILITY)
  # To provide backwards compatibility, get official header files
  # Keeps polusion of these header files to one C-file.

	include_directories(${CMAKE_BINARY_DIR}/extern/e-hal/inc/)
	file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/extern/e-hal/inc/)
	file(DOWNLOAD https://raw.githubusercontent.com/adapteva/epiphany-libs/2019.1/e-hal/src/epiphany-hal-data-local.h ${CMAKE_BINARY_DIR}/extern/e-hal/inc/epiphany-hal-data-local.h)
	file(DOWNLOAD https://raw.githubusercontent.com/adapteva/epiphany-libs/2019.1/e-hal/src/epiphany-hal-data.h ${CMAKE_BINARY_DIR}/extern/e-hal/inc/epiphany-hal-data.h)

  add_library(ehal-backward-comp OBJECT src/ehal-backward-comp.c)
  set_property(TARGET ehal-backward-comp PROPERTY POSITION_INDEPENDENT_CODE ON)
  target_include_directories(ehal-backward-comp PUBLIC
		${CMAKE_BINARY_DIR}/extern/e-hal/inc/)

endif(EHAL_BACKWARD_COMPATIBILITY)


include_directories(${HEADER})
add_library(ehal SHARED ${SOURCE})
if(EHAL_BACKWARD_COMPATIBILITY)
	target_link_libraries(ehal PUBLIC ehal-backward-comp)
endif(EHAL_BACKWARD_COMPATIBILITY)

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
