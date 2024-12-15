cmake_minimum_required(VERSION 3.16)

include(${CMAKE_CURRENT_LIST_DIR}/picopocket-hw/pico-sdk/pico_sdk_init.cmake)

add_subdirectory(picopocket-hw)

set(CMAKE_BUILD_TYPE "RelWithDebInfo")

project(picopocket-sw)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/picow-http)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/ne2000xt)

set(SNAPPY_BUILD_TESTS OFF)
set(SNAPPY_BUILD_BENCHMARKS OFF)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/snappy)

add_subdirectory(ioface)
add_subdirectory(or)

add_executable(picopocket-sw
	main.cpp
	usb_descriptors.cpp
	usb_config.cpp
	network_l2.cpp
	www.cpp
)

target_compile_definitions(picopocket-sw PRIVATE
	PICO_PANIC_NO_STRINGS_ON_TARGET
#	PICO_PANIC_FUNCTION=
)


picow_http_gen_handlers(picopocket-sw
        ${CMAKE_CURRENT_SOURCE_DIR}/www/www.yaml
        ${CMAKE_CURRENT_SOURCE_DIR}/www
        ${CMAKE_CURRENT_SOURCE_DIR}/www/index.html
)


pico_set_program_name(picopocket-sw "picopocket-sw")
pico_set_program_version(picopocket-sw "0.1")
pico_set_binary_type(picopocket-sw default)
pico_set_linker_script(picopocket-sw ${CMAKE_CURRENT_LIST_DIR}/default_scratchy_saved.ld)
pico_add_extra_outputs(picopocket-sw)

target_link_libraries(picopocket-sw PRIVATE
	isa_worker
	hardware_flash
	hardware_sync
	tinyusb_device
	tinyusb_board
	pico_cyw43_arch_none
	pico_lwip_nosys
	picow_http
	or
	ioiface
	jmpcoro
)

target_include_directories(picopocket-sw PRIVATE ${CMAKE_CURRENT_LIST_DIR})


target_link_options(picopocket-sw PRIVATE
	"--specs=nosys.specs"
	"--specs=nano.specs"
)
