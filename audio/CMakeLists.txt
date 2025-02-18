add_library(audio
	sbdsp.cpp
	mss.cpp
)

target_link_libraries(audio
	PRIVATE
		audio_dma
		${WORKER}
		jmpcoro
		common
)