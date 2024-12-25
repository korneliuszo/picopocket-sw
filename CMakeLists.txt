cmake_minimum_required(VERSION 3.16)

include(${CMAKE_CURRENT_LIST_DIR}/picopocket-hw/pico-sdk/pico_sdk_init.cmake)

option(TARGET_GUS "Build for picogus" OFF)
if(${TARGET_GUS})
	set(WORKER isa_worker_gus)
	set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/picogus/boards)
	set(PICO_BOARD picogus)
else()
	set(WORKER isa_worker)
endif()

add_subdirectory(picopocket-hw)

set(CMAKE_BUILD_TYPE "RelWithDebInfo")

project(picopocket-sw)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/picow-http)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/ne2000xt)

if(${TARGET_GUS})
	add_subdirectory(picogus)
endif()

add_subdirectory(ioface)
add_subdirectory(or)
add_subdirectory(16550)

add_executable(picopocket-sw
	main.cpp
	usb_descriptors.cpp
	usb_config.cpp
	network_l2.cpp
	www.cpp
	${CMAKE_CURRENT_LIST_DIR}/pico-examples/pico_w/wifi/access_point/dhcpserver/dhcpserver.c
    ${CMAKE_CURRENT_LIST_DIR}/pico-examples/pico_w/wifi/access_point/dnsserver/dnsserver.c
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
	${WORKER}
	hardware_flash
	hardware_sync
	tinyusb_device
	tinyusb_board
	pico_cyw43_arch_none
	pico_lwip_nosys
	picow_http
	or
	ioiface
	uart_16550
	jmpcoro
)

target_include_directories(picopocket-sw 
	PRIVATE 
		${CMAKE_CURRENT_LIST_DIR}
		${CMAKE_CURRENT_LIST_DIR}/pico-examples/pico_w/wifi/access_point/dhcpserver
		${CMAKE_CURRENT_LIST_DIR}/pico-examples/pico_w/wifi/access_point/dnsserver
	)


target_link_options(picopocket-sw PRIVATE
	"--specs=nosys.specs"
	"--specs=nano.specs"
)
