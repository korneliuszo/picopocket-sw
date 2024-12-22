add_library(uart_16550
	16550.cpp
	uart_tcp_server.cpp
)

if(PICOPOCKET_SIM)
	set(OR_HWLIBS)
else()
	set(OR_HWLIBS 
		pico_lwip_nosys
		pico_cyw43_arch_none
	)
endif()

target_link_libraries(uart_16550 
	PRIVATE
		isa_worker
		${OR_HWLIBS}
)

target_include_directories(uart_16550 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../)
