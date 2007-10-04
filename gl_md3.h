#ifndef _GL_MD3_H_
#define _GL_MD3_H_

#define MD3_IDENT		(('3'<<24)+('P'<<16)+('D'<<8)+'I')

#define	MD3_XYZ_SCALE	(1.0 / 64)

void Mod_LoadAlias3Model (model_t *mod, void *buffer, int filesize);
struct entity_s;	//whoops
void R_DrawAlias3Model (struct entity_s *ent);

//structures from Tenebrae
typedef struct {
	int			ident;
	int			version;

	char		name[MAX_QPATH];

	int			flags;	//Does anyone know what these are?

	int			numFrames;
	int			numTags;			
	int			numSurfaces;

	int			numSkins;

	int			ofsFrames;
	int			ofsTags;
	int			ofsSurfaces;
	int			ofsEnd;
} md3Header_t;

//then has header->numFrames of these at header->ofs_Frames
typedef struct md3Frame_s {
	vec3_t		bounds[2];
	vec3_t		localOrigin;
	float		radius;
	char		name[16];
} md3Frame_t;

//there are header->numSurfaces of these at header->ofsSurfaces, following from ofsEnd
typedef struct {
	int		ident;				// 

	char	name[MAX_QPATH];	// polyset name

	int		flags;
	int		numFrames;			// all surfaces in a model should have the same

	int		numShaders;			// all surfaces in a model should have the same
	int		numVerts;

	int		numTriangles;
	int		ofsTriangles;

	int		ofsShaders;			// offset from start of md3Surface_t
	int		ofsSt;				// texture coords are common for all frames
	int		ofsXyzNormals;		// numVerts * numFrames

	int		ofsEnd;				// next surface follows
} md3Surface_t;

//at surf+surf->ofsXyzNormals
typedef struct {
	short		xyz[3];
	unsigned short normal;
} md3XyzNormal_t;

//surf->numTriangles at surf+surf->ofsTriangles
typedef struct {
	int			indexes[3];
} md3Triangle_t;

//surf->numVerts at surf+surf->ofsSt
typedef struct {
	float		s;
	float		t;
} md3St_t;

typedef struct {
	char			name[MAX_QPATH];
	int				shaderIndex;
} md3Shader_t;
//End of Tenebrae 'assistance'

typedef struct {
	char name[MAX_QPATH];
	vec3_t org;
	float ang[3][3];
} md3tag_t;

//extra surfinfo
typedef struct {
	char name[MAX_QPATH];	//FIXME: pointer to shader. Requires model recaching when shader info is wiped though.
	int texnum;
} surfinf_t;
typedef struct {
	int surfinf;		//ofs, surfs*skins
	int md3model;	//ofs md3Header_t

	int ofstags;
	int numtags;
	int numframes;
} md3model_t;
#endif
