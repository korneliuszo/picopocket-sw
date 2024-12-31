add_library(audio
	sbdsp.cpp
)

target_link_libraries(audio
	PRIVATE
		audio_dma
		${WORKER}
		jmpcoro
		common
)