cmake_minimum_required(VERSION 2.0)
project( ehal )
#project(ehal VERSION 0.1.0 DESCRIPTION "Adapteva EPIPHANY efficient HAL")
set(CMAKE_C_STANDARD 89)

#set_target_properties(ehal PROPERTIES VERSION ${PROJECT_VERSION})
#set_target_properties(ehal PROPERTIES PUBLIC_HEADER inc/elib.h)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc
                    ${CMAKE_CURRENT_SOURCE_DIR}/inc/state)
add_library(ehal SHARED src/state/ident-adapteva-epiphany.c
                        src/state/ident-xilinx-zynq.c)

install(TARGETS ehal DESTINATION /usr/lib)
#install(FILES inc/ehal.h DESTINATION include)
