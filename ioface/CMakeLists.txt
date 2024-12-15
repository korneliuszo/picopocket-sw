add_library(ioiface
	STATIC
	ioiface.cpp
	config_iface.cpp
)

if(PICOPOCKET_SIM)
	set(OR_HWLIBS)
else()
	set(OR_HWLIBS
		pico_unique_id
	)
endif()

target_link_libraries(ioiface
	PRIVATE
		isa_worker
		${OR_HWLIBS}
)	
target_include_directories(ioiface PUBLIC ${CMAKE_CURRENT_LIST_DIR})
