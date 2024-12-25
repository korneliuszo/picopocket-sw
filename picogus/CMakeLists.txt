add_library(isa_worker_gus STATIC
	isa_worker.cpp
)

pico_generate_pio_header(isa_worker_gus ${CMAKE_CURRENT_LIST_DIR}/isa_dma_gus.pio)


target_link_libraries(isa_worker_gus
	PUBLIC 
		hardware_pio
		hardware_clocks
		hardware_dma
		hardware_interp
		pico_multicore
		pico_stdio_semihosting
		pico_stdlib
		psram_pio
)
