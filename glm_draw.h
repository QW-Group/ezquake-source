
#ifndef EZQUAKE_GLM_DRAW_HEADER
#define EZQUAKE_GLM_DRAW_HEADER

#define MAX_LINES_PER_FRAME 128

typedef struct glm_line_point_s {
	vec3_t position;
	byte color[4];
} glm_line_point_t;

typedef struct glm_line_framedata_s {
	glm_line_point_t line_points[MAX_LINES_PER_FRAME * 2];
	float line_thickness[MAX_LINES_PER_FRAME];
	int lineCount;
} glm_line_framedata_t;

#define CIRCLE_LINE_COUNT	40
#define FLOATS_PER_CIRCLE ((3 + 2 * CIRCLE_LINE_COUNT) * 2)
#define CIRCLES_PER_FRAME 256

typedef struct glm_circle_framedata_s {
	float drawCirclePointData[FLOATS_PER_CIRCLE * CIRCLES_PER_FRAME];
	float drawCircleColors[CIRCLES_PER_FRAME][4];
	qbool drawCircleFill[CIRCLES_PER_FRAME];
	int drawCirclePoints[CIRCLES_PER_FRAME];
	float drawCircleThickness[CIRCLES_PER_FRAME];

	int circleCount;
} glm_circle_framedata_t;

#define MAX_POLYGONS_PER_FRAME 8
#define MAX_POLYGON_POINTS     64

typedef struct glm_polygon_framedata_s {
	vec3_t polygonVertices[MAX_POLYGONS_PER_FRAME * MAX_POLYGON_POINTS];
	float polygonColor[MAX_POLYGONS_PER_FRAME][4];
	int polygonX[MAX_POLYGONS_PER_FRAME];
	int polygonY[MAX_POLYGONS_PER_FRAME];
	int polygonVerts[MAX_POLYGONS_PER_FRAME];
	int polygonCount;
} glm_polygon_framedata_t;

#define MAX_MULTI_IMAGE_BATCH 4096
typedef enum {
	imagetype_image,
	imagetype_circle,
	imagetype_polygon,
	imagetype_line
} glm_image_type_t;

typedef struct glm_image_s {
	float pos[2];
	float tex[2];
	unsigned char colour[4];
	int flags;
} glm_image_t;

typedef struct glc_image_s {
	float pos[2];
	float tex[2];
	unsigned char colour[4];
} glc_image_t;

typedef struct glm_image_framedata_s {
	glm_image_t images[MAX_MULTI_IMAGE_BATCH * 4];
	glc_image_t glc_images[MAX_MULTI_IMAGE_BATCH * 4];

	int imageCount;
} glm_image_framedata_t;

extern glm_circle_framedata_t circleData;
extern glm_image_framedata_t imageData;
extern glm_polygon_framedata_t polygonData;
extern glm_line_framedata_t lineData;

qbool GLM_LogCustomImageType(glm_image_type_t type, int index);
qbool GLM_LogCustomImageTypeWithTexture(glm_image_type_t type, int index, texture_ref texture);

void GLM_PreparePolygons(void);
void GLM_PrepareLines(void);
void GLM_PrepareImages(void);
void GLM_PrepareCircles(void);

#endif
