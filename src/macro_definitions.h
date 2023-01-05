
#ifndef EZQUAKE_MACRO_DEFINITIONS_H
#define EZQUAKE_MACRO_DEFINITIONS_H

#define MACRO_DEF(x) macro_ ## x

typedef enum {
#include "macro_ids.h"
	num_macros
} macro_id;

#undef MACRO_DEF

#endif // EZQUAKE_MACRO_DEFINITIONS_H
