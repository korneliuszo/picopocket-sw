cmake_minimum_required(VERSION 3.16)

include(${CMAKE_CURRENT_LIST_DIR}/picopocket-hw/pico-sdk/pico_sdk_init.cmake)

add_subdirectory(picopocket-hw)

set(CMAKE_BUILD_TYPE "RelWithDebInfo")

project(picopocket-sw)

add_executable(picopocket-sw
	main.cpp
	usb_descriptors.cpp
	usb_config.cpp
)

pico_set_program_name(picopocket-sw "picopocket-sw")
pico_set_program_version(picopocket-sw "0.1")
pico_set_binary_type(picopocket-sw copy_to_ram)
pico_set_linker_script(picopocket-sw ${CMAKE_CURRENT_LIST_DIR}/ram_scratchy_saved.ld)
pico_add_extra_outputs(picopocket-sw)

target_link_libraries(picopocket-sw PRIVATE
	isa_worker
	hardware_flash
	tinyusb_device
	tinyusb_board
)

target_include_directories(picopocket-sw PRIVATE ${CMAKE_CURRENT_LIST_DIR})


target_link_options(picopocket-sw PRIVATE
	"--specs=nosys.specs"
	"--specs=nano.specs"
)
