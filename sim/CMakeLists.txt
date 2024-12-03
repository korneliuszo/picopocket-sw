cmake_minimum_required(VERSION 3.16)

project(picopocket-sim)

set(PICOPOCKET_SIM TRUE)
add_compile_definitions(PICOPOCKET_SIM)

add_subdirectory(../picopocket-hw picopocket-hw)
add_subdirectory(../ioface ioface)
add_subdirectory(../or or)

add_executable(sim
	sim.cpp
)
	
target_link_libraries(sim PRIVATE
	or
	ioiface
	isa_worker
)	

target_include_directories(sim PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../)
