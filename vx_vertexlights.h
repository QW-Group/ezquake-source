extern byte anorm_pitch[162];
extern byte anorm_yaw[162];

extern byte vlighttable[256][256];

float VLight_LerpLight(int index1, int index2, float ilerp, float apitch, float ayaw);

extern	float	vlight_pitch;   // RIOT - Vertex lighting
extern	float	vlight_yaw;
extern	float	vlight_lowcut;
extern	float	vlight_highcut;
void VLight_ResetAnormTable();
void Init_VLights();
