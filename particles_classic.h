
#ifndef PARTICLES_CLASSIC_HEADER
#define PARTICLES_CLASSIC_HEADER

typedef enum {
	pt_static, pt_grav, pt_slowgrav, pt_fire, pt_explode, pt_explode2, pt_blob, pt_blob2, pt_rail
} ptype_t;

typedef struct particle_s {
	vec3_t      org;
	float       color;
	vec3_t      vel;
	float       ramp;
	float       die;
	ptype_t     type;
	struct particle_s *next;
} particle_t;

//#define DEFAULT_NUM_PARTICLES	2048
#define ABSOLUTE_MIN_PARTICLES	512
#define ABSOLUTE_MAX_PARTICLES	8192

#endif // PARTICLES_CLASSIC_HEADER
