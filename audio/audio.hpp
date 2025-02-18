#pragma once
#include "jmpcoro_configured.hpp"

void sbdsp_install(Thread * main);
void sbdsp_poll(Thread * main);

void mss_install(Thread * main);
void mss_poll(Thread * main);
