
#include "quakedef.h"
#include "glc_fog.h"
#include "r_local.h"

// FIXME: Move functions under here
void R_AddWaterfog(int contents)
{
	if (R_UseImmediateOpenGL()) {
		GLC_EnableWaterFog(contents);
	}
}

void R_ConfigureFog(void)
{
	if (R_UseImmediateOpenGL()) {
		GLC_ConfigureFog();
	}
}
