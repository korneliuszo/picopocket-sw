add_subdirectory(8088)
add_subdirectory(config)

add_library(or
	or.cpp
	int19_install.cpp
	monitor.cpp
	ramdisk.cpp
	config.cpp
)
	
if(PICOPOCKET_SIM)
	set(OR_HWLIBS)
else()
	set(OR_HWLIBS 
		hardware_flash
		pico_lwip_nosys
		pico_cyw43_arch_none
		psram_pio
		ar1021_pio
	)
endif()
target_link_libraries(or 
	PRIVATE
		isa_worker
		optionrom
		ioiface
		freedos_kernel
		config_img
		${OR_HWLIBS}
	PUBLIC
		jmpcoro
)	
target_include_directories(or PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../)
