#include "quakedef.h"
#include "pmove.h"

float CL_TraceLine (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal)
{
	trace_t trace = PM_TraceLine (start, end); /* PM_TraceLine hits bmodels and players */
	VectorCopy (trace.endpos, impact);
	if (normal)
		VectorCopy (trace.plane.normal, normal);

	return 0.0;
}
