
#ifndef EZQUAKE_GLM_LOCAL_HEADER
#define EZQUAKE_GLM_LOCAL_HEADER

void GL_BuildCommonTextureArrays(qbool vid_restart);
void GLM_DeletePrograms(qbool restarting);
void GLM_InitPrograms(void);
void GLM_Shutdown(qbool restarting);

void GLM_SamplerSetNearest(unsigned int texture_unit_number);
void GLM_SamplerClear(unsigned int texture_unit_number);
void GL_DeleteSamplers(void);

#endif // EZQUAKE_GLM_LOCAL_HEADER