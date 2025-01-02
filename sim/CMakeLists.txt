cmake_minimum_required(VERSION 3.16)

project(picopocket-sim)

set(PICOPOCKET_SIM TRUE)
set(WORKER isa_worker)
add_compile_definitions(PICOPOCKET_SIM)

add_subdirectory(../picopocket-hw picopocket-hw)
add_subdirectory(../ioface ioface)
add_subdirectory(../or or)
add_subdirectory(../16550 16550)

add_executable(sim
	sim.cpp
)
	
target_link_options(sim
  PUBLIC
    LINKER:-Map=sim.map
)

target_link_libraries(sim PRIVATE
	or
	ioiface
	isa_worker
	uart_16550
	common
)	

#target_include_directories(sim PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../)
