#pragma once

#include <stdint.h>
#include <stddef.h>

#include "jmpcoro_configured.hpp"

void optionrom_install(Thread * main);
void optionrom_start_worker(Thread * main);

