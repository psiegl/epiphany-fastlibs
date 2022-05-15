cmake_minimum_required(VERSION 2.8)
project( ehal )
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Adapteva EPIPHANY efficient HAL")
set(CPACK_PACKAGE_VERSION 0.1.0)
set(CMAKE_C_STANDARD 89)

#set_target_properties(ehal PROPERTIES VERSION ${PROJECT_VERSION})
#set_target_properties(ehal PROPERTIES PUBLIC_HEADER inc/elib.h)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)
add_library(ehal SHARED src/state/ident-adapteva-epiphany.c
			src/state/ident-xilinx-zynq.c
			src/loader/ehal_generic_file_loader.c
			src/loader/ehal_hdf_loader.c
			src/loader/ehal_srec_loader.c)

install(TARGETS ehal DESTINATION /usr/lib)
#install(FILES inc/ehal.h DESTINATION include)
