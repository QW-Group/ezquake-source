/*
Quake source code is Copyright (C) 1996-1997 Id Software, Inc.
D3D8 FakeGL Wrapper is Copyright (C) 2009 MH

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// inconsistent dll linkage warning
#pragma warning (disable: 4273)

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef signed char GLbyte;
typedef short GLshort;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned int GLuint;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;
typedef void GLvoid;

/* ************************************************************/

/* Version */
#define GL_VERSION_1_1 1

/* AccumOp */
#define GL_ACCUM 0x0100
#define GL_LOAD 0x0101
#define GL_RETURN 0x0102
#define GL_MULT 0x0103
#define GL_ADD 0x0104

/* AlphaFunction */
#define GL_NEVER 0x0200
#define GL_LESS 0x0201
#define GL_EQUAL 0x0202
#define GL_LEQUAL 0x0203
#define GL_GREATER 0x0204
#define GL_NOTEQUAL 0x0205
#define GL_GEQUAL 0x0206
#define GL_ALWAYS 0x0207

/* AttribMask */
#define GL_CURRENT_BIT 0x00000001
#define GL_POINT_BIT 0x00000002
#define GL_LINE_BIT 0x00000004
#define GL_POLYGON_BIT 0x00000008
#define GL_POLYGON_STIPPLE_BIT 0x00000010
#define GL_PIXEL_MODE_BIT 0x00000020
#define GL_LIGHTING_BIT 0x00000040
#define GL_FOG_BIT 0x00000080
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_ACCUM_BUFFER_BIT 0x00000200
#define GL_STENCIL_BUFFER_BIT 0x00000400
#define GL_VIEWPORT_BIT 0x00000800
#define GL_TRANSFORM_BIT 0x00001000
#define GL_ENABLE_BIT 0x00002000
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_HINT_BIT 0x00008000
#define GL_EVAL_BIT 0x00010000
#define GL_LIST_BIT 0x00020000
#define GL_TEXTURE_BIT 0x00040000
#define GL_SCISSOR_BIT 0x00080000
#define GL_ALL_ATTRIB_BITS 0x000fffff

/* BeginMode */
#define GL_POINTS 0x0000
#define GL_LINES 0x0001
#define GL_LINE_LOOP 0x0002
#define GL_LINE_STRIP 0x0003
#define GL_TRIANGLES 0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_TRIANGLE_FAN 0x0006
#define GL_QUADS 0x0007
#define GL_QUAD_STRIP 0x0008
#define GL_POLYGON 0x0009

/* BlendingFactorDest */
#define GL_ZERO 0
#define GL_ONE 1
#define GL_SRC_COLOR 0x0300
#define GL_ONE_MINUS_SRC_COLOR 0x0301
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_DST_ALPHA 0x0304
#define GL_ONE_MINUS_DST_ALPHA 0x0305

/* BlendingFactorSrc */
/*  GL_ZERO */
/*  GL_ONE */
#define GL_DST_COLOR 0x0306
#define GL_ONE_MINUS_DST_COLOR 0x0307
#define GL_SRC_ALPHA_SATURATE 0x0308
/*  GL_SRC_ALPHA */
/*  GL_ONE_MINUS_SRC_ALPHA */
/*  GL_DST_ALPHA */
/*  GL_ONE_MINUS_DST_ALPHA */

/* Boolean */
#define GL_TRUE 1
#define GL_FALSE 0

/* ClearBufferMask */
/*  GL_COLOR_BUFFER_BIT */
/*  GL_ACCUM_BUFFER_BIT */
/*  GL_STENCIL_BUFFER_BIT */
/*  GL_DEPTH_BUFFER_BIT */

/* ClientArrayType */
/*  GL_VERTEX_ARRAY */
/*  GL_NORMAL_ARRAY */
/*  GL_COLOR_ARRAY */
/*  GL_INDEX_ARRAY */
/*  GL_TEXTURE_COORD_ARRAY */
/*  GL_EDGE_FLAG_ARRAY */

/* ClipPlaneName */
#define GL_CLIP_PLANE0 0x3000
#define GL_CLIP_PLANE1 0x3001
#define GL_CLIP_PLANE2 0x3002
#define GL_CLIP_PLANE3 0x3003
#define GL_CLIP_PLANE4 0x3004
#define GL_CLIP_PLANE5 0x3005

/* ColorMaterialFace */
/*  GL_FRONT */
/*  GL_BACK */
/*  GL_FRONT_AND_BACK */

/* ColorMaterialParameter */
/*  GL_AMBIENT */
/*  GL_DIFFUSE */
/*  GL_SPECULAR */
/*  GL_EMISSION */
/*  GL_AMBIENT_AND_DIFFUSE */

/* ColorPointerType */
/*  GL_BYTE */
/*  GL_UNSIGNED_BYTE */
/*  GL_SHORT */
/*  GL_UNSIGNED_SHORT */
/*  GL_INT */
/*  GL_UNSIGNED_INT */
/*  GL_FLOAT */
/*  GL_DOUBLE */

/* CullFaceMode */
/*  GL_FRONT */
/*  GL_BACK */
/*  GL_FRONT_AND_BACK */

/* DataType */
#define GL_BYTE 0x1400
#define GL_UNSIGNED_BYTE 0x1401
#define GL_SHORT 0x1402
#define GL_UNSIGNED_SHORT 0x1403
#define GL_INT 0x1404
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_2_BYTES 0x1407
#define GL_3_BYTES 0x1408
#define GL_4_BYTES 0x1409
#define GL_DOUBLE 0x140A

/* DepthFunction */
/*  GL_NEVER */
/*  GL_LESS */
/*  GL_EQUAL */
/*  GL_LEQUAL */
/*  GL_GREATER */
/*  GL_NOTEQUAL */
/*  GL_GEQUAL */
/*  GL_ALWAYS */

/* DrawBufferMode */
#define GL_NONE 0
#define GL_FRONT_LEFT 0x0400
#define GL_FRONT_RIGHT 0x0401
#define GL_BACK_LEFT 0x0402
#define GL_BACK_RIGHT 0x0403
#define GL_FRONT 0x0404
#define GL_BACK 0x0405
#define GL_LEFT 0x0406
#define GL_RIGHT 0x0407
#define GL_FRONT_AND_BACK 0x0408
#define GL_AUX0 0x0409
#define GL_AUX1 0x040A
#define GL_AUX2 0x040B
#define GL_AUX3 0x040C

/* Enable */
/*  GL_FOG */
/*  GL_LIGHTING */
/*  GL_TEXTURE_1D */
/*  GL_TEXTURE_2D */
/*  GL_LINE_STIPPLE */
/*  GL_POLYGON_STIPPLE */
/*  GL_CULL_FACE */
/*  GL_ALPHA_TEST */
/*  GL_BLEND */
/*  GL_INDEX_LOGIC_OP */
/*  GL_COLOR_LOGIC_OP */
/*  GL_DITHER */
/*  GL_STENCIL_TEST */
/*  GL_DEPTH_TEST */
/*  GL_CLIP_PLANE0 */
/*  GL_CLIP_PLANE1 */
/*  GL_CLIP_PLANE2 */
/*  GL_CLIP_PLANE3 */
/*  GL_CLIP_PLANE4 */
/*  GL_CLIP_PLANE5 */
/*  GL_LIGHT0 */
/*  GL_LIGHT1 */
/*  GL_LIGHT2 */
/*  GL_LIGHT3 */
/*  GL_LIGHT4 */
/*  GL_LIGHT5 */
/*  GL_LIGHT6 */
/*  GL_LIGHT7 */
/*  GL_TEXTURE_GEN_S */
/*  GL_TEXTURE_GEN_T */
/*  GL_TEXTURE_GEN_R */
/*  GL_TEXTURE_GEN_Q */
/*  GL_MAP1_VERTEX_3 */
/*  GL_MAP1_VERTEX_4 */
/*  GL_MAP1_COLOR_4 */
/*  GL_MAP1_INDEX */
/*  GL_MAP1_NORMAL */
/*  GL_MAP1_TEXTURE_COORD_1 */
/*  GL_MAP1_TEXTURE_COORD_2 */
/*  GL_MAP1_TEXTURE_COORD_3 */
/*  GL_MAP1_TEXTURE_COORD_4 */
/*  GL_MAP2_VERTEX_3 */
/*  GL_MAP2_VERTEX_4 */
/*  GL_MAP2_COLOR_4 */
/*  GL_MAP2_INDEX */
/*  GL_MAP2_NORMAL */
/*  GL_MAP2_TEXTURE_COORD_1 */
/*  GL_MAP2_TEXTURE_COORD_2 */
/*  GL_MAP2_TEXTURE_COORD_3 */
/*  GL_MAP2_TEXTURE_COORD_4 */
/*  GL_POINT_SMOOTH */
/*  GL_LINE_SMOOTH */
/*  GL_POLYGON_SMOOTH */
/*  GL_SCISSOR_TEST */
/*  GL_COLOR_MATERIAL */
/*  GL_NORMALIZE */
/*  GL_AUTO_NORMAL */
/*  GL_VERTEX_ARRAY */
/*  GL_NORMAL_ARRAY */
/*  GL_COLOR_ARRAY */
/*  GL_INDEX_ARRAY */
/*  GL_TEXTURE_COORD_ARRAY */
/*  GL_EDGE_FLAG_ARRAY */
/*  GL_POLYGON_OFFSET_POINT */
/*  GL_POLYGON_OFFSET_LINE */
/*  GL_POLYGON_OFFSET_FILL */

/* ErrorCode */
#define GL_NO_ERROR 0
#define GL_INVALID_ENUM 0x0500
#define GL_INVALID_VALUE 0x0501
#define GL_INVALID_OPERATION 0x0502
#define GL_STACK_OVERFLOW 0x0503
#define GL_STACK_UNDERFLOW 0x0504
#define GL_OUT_OF_MEMORY 0x0505

/* FeedBackMode */
#define GL_2D 0x0600
#define GL_3D 0x0601
#define GL_3D_COLOR 0x0602
#define GL_3D_COLOR_TEXTURE 0x0603
#define GL_4D_COLOR_TEXTURE 0x0604

/* FeedBackToken */
#define GL_PASS_THROUGH_TOKEN 0x0700
#define GL_POINT_TOKEN 0x0701
#define GL_LINE_TOKEN 0x0702
#define GL_POLYGON_TOKEN 0x0703
#define GL_BITMAP_TOKEN 0x0704
#define GL_DRAW_PIXEL_TOKEN 0x0705
#define GL_COPY_PIXEL_TOKEN 0x0706
#define GL_LINE_RESET_TOKEN 0x0707

/* FogMode */
/*  GL_LINEAR */
#define GL_EXP 0x0800
#define GL_EXP2 0x0801


/* FogParameter */
/*  GL_FOG_COLOR */
/*  GL_FOG_DENSITY */
/*  GL_FOG_END */
/*  GL_FOG_INDEX */
/*  GL_FOG_MODE */
/*  GL_FOG_START */

/* FrontFaceDirection */
#define GL_CW 0x0900
#define GL_CCW 0x0901

/* GetMapTarget */
#define GL_COEFF 0x0A00
#define GL_ORDER 0x0A01
#define GL_DOMAIN 0x0A02

/* GetPixelMap */
/*  GL_PIXEL_MAP_I_TO_I */
/*  GL_PIXEL_MAP_S_TO_S */
/*  GL_PIXEL_MAP_I_TO_R */
/*  GL_PIXEL_MAP_I_TO_G */
/*  GL_PIXEL_MAP_I_TO_B */
/*  GL_PIXEL_MAP_I_TO_A */
/*  GL_PIXEL_MAP_R_TO_R */
/*  GL_PIXEL_MAP_G_TO_G */
/*  GL_PIXEL_MAP_B_TO_B */
/*  GL_PIXEL_MAP_A_TO_A */

/* GetPointerTarget */
/*  GL_VERTEX_ARRAY_POINTER */
/*  GL_NORMAL_ARRAY_POINTER */
/*  GL_COLOR_ARRAY_POINTER */
/*  GL_INDEX_ARRAY_POINTER */
/*  GL_TEXTURE_COORD_ARRAY_POINTER */
/*  GL_EDGE_FLAG_ARRAY_POINTER */

/* GetTarget */
#define GL_CURRENT_COLOR 0x0B00
#define GL_CURRENT_INDEX 0x0B01
#define GL_CURRENT_NORMAL 0x0B02
#define GL_CURRENT_TEXTURE_COORDS 0x0B03
#define GL_CURRENT_RASTER_COLOR 0x0B04
#define GL_CURRENT_RASTER_INDEX 0x0B05
#define GL_CURRENT_RASTER_TEXTURE_COORDS 0x0B06
#define GL_CURRENT_RASTER_POSITION 0x0B07
#define GL_CURRENT_RASTER_POSITION_VALID 0x0B08
#define GL_CURRENT_RASTER_DISTANCE 0x0B09
#define GL_POINT_SMOOTH 0x0B10
#define GL_POINT_SIZE 0x0B11
#define GL_POINT_SIZE_RANGE 0x0B12
#define GL_POINT_SIZE_GRANULARITY 0x0B13
#define GL_LINE_SMOOTH 0x0B20
#define GL_LINE_WIDTH 0x0B21
#define GL_LINE_WIDTH_RANGE 0x0B22
#define GL_LINE_WIDTH_GRANULARITY 0x0B23
#define GL_LINE_STIPPLE 0x0B24
#define GL_LINE_STIPPLE_PATTERN 0x0B25
#define GL_LINE_STIPPLE_REPEAT 0x0B26
#define GL_LIST_MODE 0x0B30
#define GL_MAX_LIST_NESTING 0x0B31
#define GL_LIST_BASE 0x0B32
#define GL_LIST_INDEX 0x0B33
#define GL_POLYGON_MODE 0x0B40
#define GL_POLYGON_SMOOTH 0x0B41
#define GL_POLYGON_STIPPLE 0x0B42
#define GL_EDGE_FLAG 0x0B43
#define GL_CULL_FACE 0x0B44
#define GL_CULL_FACE_MODE 0x0B45
#define GL_FRONT_FACE 0x0B46
#define GL_LIGHTING 0x0B50
#define GL_LIGHT_MODEL_LOCAL_VIEWER 0x0B51
#define GL_LIGHT_MODEL_TWO_SIDE 0x0B52
#define GL_LIGHT_MODEL_AMBIENT 0x0B53
#define GL_SHADE_MODEL 0x0B54
#define GL_COLOR_MATERIAL_FACE 0x0B55
#define GL_COLOR_MATERIAL_PARAMETER 0x0B56
#define GL_COLOR_MATERIAL 0x0B57
#define GL_FOG 0x0B60
#define GL_FOG_INDEX 0x0B61
#define GL_FOG_DENSITY 0x0B62
#define GL_FOG_START 0x0B63
#define GL_FOG_END 0x0B64
#define GL_FOG_MODE 0x0B65
#define GL_FOG_COLOR 0x0B66
#define GL_DEPTH_RANGE 0x0B70
#define GL_DEPTH_TEST 0x0B71
#define GL_DEPTH_WRITEMASK 0x0B72
#define GL_DEPTH_CLEAR_VALUE 0x0B73
#define GL_DEPTH_FUNC 0x0B74
#define GL_ACCUM_CLEAR_VALUE 0x0B80
#define GL_STENCIL_TEST 0x0B90
#define GL_STENCIL_CLEAR_VALUE 0x0B91
#define GL_STENCIL_FUNC 0x0B92
#define GL_STENCIL_VALUE_MASK 0x0B93
#define GL_STENCIL_FAIL 0x0B94
#define GL_STENCIL_PASS_DEPTH_FAIL 0x0B95
#define GL_STENCIL_PASS_DEPTH_PASS 0x0B96
#define GL_STENCIL_REF 0x0B97
#define GL_STENCIL_WRITEMASK 0x0B98
#define GL_MATRIX_MODE 0x0BA0
#define GL_NORMALIZE 0x0BA1
#define GL_VIEWPORT 0x0BA2
#define GL_MODELVIEW_STACK_DEPTH 0x0BA3
#define GL_PROJECTION_STACK_DEPTH 0x0BA4
#define GL_TEXTURE_STACK_DEPTH 0x0BA5
#define GL_MODELVIEW_MATRIX 0x0BA6
#define GL_PROJECTION_MATRIX 0x0BA7
#define GL_TEXTURE_MATRIX 0x0BA8
#define GL_ATTRIB_STACK_DEPTH 0x0BB0
#define GL_CLIENT_ATTRIB_STACK_DEPTH 0x0BB1
#define GL_ALPHA_TEST 0x0BC0
#define GL_ALPHA_TEST_FUNC 0x0BC1
#define GL_ALPHA_TEST_REF 0x0BC2
#define GL_DITHER 0x0BD0
#define GL_BLEND_DST 0x0BE0
#define GL_BLEND_SRC 0x0BE1
#define GL_BLEND 0x0BE2
#define GL_LOGIC_OP_MODE 0x0BF0
#define GL_INDEX_LOGIC_OP 0x0BF1
#define GL_COLOR_LOGIC_OP 0x0BF2
#define GL_AUX_BUFFERS 0x0C00
#define GL_DRAW_BUFFER 0x0C01
#define GL_READ_BUFFER 0x0C02
#define GL_SCISSOR_BOX 0x0C10
#define GL_SCISSOR_TEST 0x0C11
#define GL_INDEX_CLEAR_VALUE 0x0C20
#define GL_INDEX_WRITEMASK 0x0C21
#define GL_COLOR_CLEAR_VALUE 0x0C22
#define GL_COLOR_WRITEMASK 0x0C23
#define GL_INDEX_MODE 0x0C30
#define GL_RGBA_MODE 0x0C31
#define GL_DOUBLEBUFFER 0x0C32
#define GL_STEREO 0x0C33
#define GL_RENDER_MODE 0x0C40
#define GL_PERSPECTIVE_CORRECTION_HINT 0x0C50
#define GL_POINT_SMOOTH_HINT 0x0C51
#define GL_LINE_SMOOTH_HINT 0x0C52
#define GL_POLYGON_SMOOTH_HINT 0x0C53
#define GL_FOG_HINT 0x0C54
#define GL_TEXTURE_GEN_S 0x0C60
#define GL_TEXTURE_GEN_T 0x0C61
#define GL_TEXTURE_GEN_R 0x0C62
#define GL_TEXTURE_GEN_Q 0x0C63
#define GL_PIXEL_MAP_I_TO_I 0x0C70
#define GL_PIXEL_MAP_S_TO_S 0x0C71
#define GL_PIXEL_MAP_I_TO_R 0x0C72
#define GL_PIXEL_MAP_I_TO_G 0x0C73
#define GL_PIXEL_MAP_I_TO_B 0x0C74
#define GL_PIXEL_MAP_I_TO_A 0x0C75
#define GL_PIXEL_MAP_R_TO_R 0x0C76
#define GL_PIXEL_MAP_G_TO_G 0x0C77
#define GL_PIXEL_MAP_B_TO_B 0x0C78
#define GL_PIXEL_MAP_A_TO_A 0x0C79
#define GL_PIXEL_MAP_I_TO_I_SIZE 0x0CB0
#define GL_PIXEL_MAP_S_TO_S_SIZE 0x0CB1
#define GL_PIXEL_MAP_I_TO_R_SIZE 0x0CB2
#define GL_PIXEL_MAP_I_TO_G_SIZE 0x0CB3
#define GL_PIXEL_MAP_I_TO_B_SIZE 0x0CB4
#define GL_PIXEL_MAP_I_TO_A_SIZE 0x0CB5
#define GL_PIXEL_MAP_R_TO_R_SIZE 0x0CB6
#define GL_PIXEL_MAP_G_TO_G_SIZE 0x0CB7
#define GL_PIXEL_MAP_B_TO_B_SIZE 0x0CB8
#define GL_PIXEL_MAP_A_TO_A_SIZE 0x0CB9
#define GL_UNPACK_SWAP_BYTES 0x0CF0
#define GL_UNPACK_LSB_FIRST 0x0CF1
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#define GL_UNPACK_SKIP_ROWS 0x0CF3
#define GL_UNPACK_SKIP_PIXELS 0x0CF4
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_PACK_SWAP_BYTES 0x0D00
#define GL_PACK_LSB_FIRST 0x0D01
#define GL_PACK_ROW_LENGTH 0x0D02
#define GL_PACK_SKIP_ROWS 0x0D03
#define GL_PACK_SKIP_PIXELS 0x0D04
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_MAP_COLOR 0x0D10
#define GL_MAP_STENCIL 0x0D11
#define GL_INDEX_SHIFT 0x0D12
#define GL_INDEX_OFFSET 0x0D13
#define GL_RED_SCALE 0x0D14
#define GL_RED_BIAS 0x0D15
#define GL_ZOOM_X 0x0D16
#define GL_ZOOM_Y 0x0D17
#define GL_GREEN_SCALE 0x0D18
#define GL_GREEN_BIAS 0x0D19
#define GL_BLUE_SCALE 0x0D1A
#define GL_BLUE_BIAS 0x0D1B
#define GL_ALPHA_SCALE 0x0D1C
#define GL_ALPHA_BIAS 0x0D1D
#define GL_DEPTH_SCALE 0x0D1E
#define GL_DEPTH_BIAS 0x0D1F
#define GL_MAX_EVAL_ORDER 0x0D30
#define GL_MAX_LIGHTS 0x0D31
#define GL_MAX_CLIP_PLANES 0x0D32
#define GL_MAX_TEXTURE_SIZE 0x0D33
#define GL_MAX_PIXEL_MAP_TABLE 0x0D34
#define GL_MAX_ATTRIB_STACK_DEPTH 0x0D35
#define GL_MAX_MODELVIEW_STACK_DEPTH 0x0D36
#define GL_MAX_NAME_STACK_DEPTH 0x0D37
#define GL_MAX_PROJECTION_STACK_DEPTH 0x0D38
#define GL_MAX_TEXTURE_STACK_DEPTH 0x0D39
#define GL_MAX_VIEWPORT_DIMS 0x0D3A
#define GL_MAX_CLIENT_ATTRIB_STACK_DEPTH 0x0D3B
#define GL_SUBPIXEL_BITS 0x0D50
#define GL_INDEX_BITS 0x0D51
#define GL_RED_BITS 0x0D52
#define GL_GREEN_BITS 0x0D53
#define GL_BLUE_BITS 0x0D54
#define GL_ALPHA_BITS 0x0D55
#define GL_DEPTH_BITS 0x0D56
#define GL_STENCIL_BITS 0x0D57
#define GL_ACCUM_RED_BITS 0x0D58
#define GL_ACCUM_GREEN_BITS 0x0D59
#define GL_ACCUM_BLUE_BITS 0x0D5A
#define GL_ACCUM_ALPHA_BITS 0x0D5B
#define GL_NAME_STACK_DEPTH 0x0D70
#define GL_AUTO_NORMAL 0x0D80
#define GL_MAP1_COLOR_4 0x0D90
#define GL_MAP1_INDEX 0x0D91
#define GL_MAP1_NORMAL 0x0D92
#define GL_MAP1_TEXTURE_COORD_1 0x0D93
#define GL_MAP1_TEXTURE_COORD_2 0x0D94
#define GL_MAP1_TEXTURE_COORD_3 0x0D95
#define GL_MAP1_TEXTURE_COORD_4 0x0D96
#define GL_MAP1_VERTEX_3 0x0D97
#define GL_MAP1_VERTEX_4 0x0D98
#define GL_MAP2_COLOR_4 0x0DB0
#define GL_MAP2_INDEX 0x0DB1
#define GL_MAP2_NORMAL 0x0DB2
#define GL_MAP2_TEXTURE_COORD_1 0x0DB3
#define GL_MAP2_TEXTURE_COORD_2 0x0DB4
#define GL_MAP2_TEXTURE_COORD_3 0x0DB5
#define GL_MAP2_TEXTURE_COORD_4 0x0DB6
#define GL_MAP2_VERTEX_3 0x0DB7
#define GL_MAP2_VERTEX_4 0x0DB8
#define GL_MAP1_GRID_DOMAIN 0x0DD0
#define GL_MAP1_GRID_SEGMENTS 0x0DD1
#define GL_MAP2_GRID_DOMAIN 0x0DD2
#define GL_MAP2_GRID_SEGMENTS 0x0DD3
#define GL_TEXTURE_1D 0x0DE0
#define GL_TEXTURE_2D 0x0DE1
#define GL_FEEDBACK_BUFFER_POINTER 0x0DF0
#define GL_FEEDBACK_BUFFER_SIZE 0x0DF1
#define GL_FEEDBACK_BUFFER_TYPE 0x0DF2
#define GL_SELECTION_BUFFER_POINTER 0x0DF3
#define GL_SELECTION_BUFFER_SIZE 0x0DF4
/*  GL_TEXTURE_BINDING_1D */
/*  GL_TEXTURE_BINDING_2D */
/*  GL_VERTEX_ARRAY */
/*  GL_NORMAL_ARRAY */
/*  GL_COLOR_ARRAY */
/*  GL_INDEX_ARRAY */
/*  GL_TEXTURE_COORD_ARRAY */
/*  GL_EDGE_FLAG_ARRAY */
/*  GL_VERTEX_ARRAY_SIZE */
/*  GL_VERTEX_ARRAY_TYPE */
/*  GL_VERTEX_ARRAY_STRIDE */
/*  GL_NORMAL_ARRAY_TYPE */
/*  GL_NORMAL_ARRAY_STRIDE */
/*  GL_COLOR_ARRAY_SIZE */
/*  GL_COLOR_ARRAY_TYPE */
/*  GL_COLOR_ARRAY_STRIDE */
/*  GL_INDEX_ARRAY_TYPE */
/*  GL_INDEX_ARRAY_STRIDE */
/*  GL_TEXTURE_COORD_ARRAY_SIZE */
/*  GL_TEXTURE_COORD_ARRAY_TYPE */
/*  GL_TEXTURE_COORD_ARRAY_STRIDE */
/*  GL_EDGE_FLAG_ARRAY_STRIDE */
/*  GL_POLYGON_OFFSET_FACTOR */
/*  GL_POLYGON_OFFSET_UNITS */

/* GetTextureParameter */
/*  GL_TEXTURE_MAG_FILTER */
/*  GL_TEXTURE_MIN_FILTER */
/*  GL_TEXTURE_WRAP_S */
/*  GL_TEXTURE_WRAP_T */
#define GL_TEXTURE_WIDTH 0x1000
#define GL_TEXTURE_HEIGHT 0x1001
#define GL_TEXTURE_INTERNAL_FORMAT 0x1003
#define GL_TEXTURE_BORDER_COLOR 0x1004
#define GL_TEXTURE_BORDER 0x1005
/*  GL_TEXTURE_RED_SIZE */
/*  GL_TEXTURE_GREEN_SIZE */
/*  GL_TEXTURE_BLUE_SIZE */
/*  GL_TEXTURE_ALPHA_SIZE */
/*  GL_TEXTURE_LUMINANCE_SIZE */
/*  GL_TEXTURE_INTENSITY_SIZE */
/*  GL_TEXTURE_PRIORITY */
/*  GL_TEXTURE_RESIDENT */

/* HintMode */
#define GL_DONT_CARE 0x1100
#define GL_FASTEST 0x1101
#define GL_NICEST 0x1102

/* HintTarget */
/*  GL_PERSPECTIVE_CORRECTION_HINT */
/*  GL_POINT_SMOOTH_HINT */
/*  GL_LINE_SMOOTH_HINT */
/*  GL_POLYGON_SMOOTH_HINT */
/*  GL_FOG_HINT */
/*  GL_PHONG_HINT */

/* IndexPointerType */
/*  GL_SHORT */
/*  GL_INT */
/*  GL_FLOAT */
/*  GL_DOUBLE */

/* LightModelParameter */
/*  GL_LIGHT_MODEL_AMBIENT */
/*  GL_LIGHT_MODEL_LOCAL_VIEWER */
/*  GL_LIGHT_MODEL_TWO_SIDE */

/* LightName */
#define GL_LIGHT0 0x4000
#define GL_LIGHT1 0x4001
#define GL_LIGHT2 0x4002
#define GL_LIGHT3 0x4003
#define GL_LIGHT4 0x4004
#define GL_LIGHT5 0x4005
#define GL_LIGHT6 0x4006
#define GL_LIGHT7 0x4007

/* LightParameter */
#define GL_AMBIENT 0x1200
#define GL_DIFFUSE 0x1201
#define GL_SPECULAR 0x1202
#define GL_POSITION 0x1203
#define GL_SPOT_DIRECTION 0x1204
#define GL_SPOT_EXPONENT 0x1205
#define GL_SPOT_CUTOFF 0x1206
#define GL_CONSTANT_ATTENUATION 0x1207
#define GL_LINEAR_ATTENUATION 0x1208
#define GL_QUADRATIC_ATTENUATION 0x1209

/* InterleavedArrays */
/*  GL_V2F */
/*  GL_V3F */
/*  GL_C4UB_V2F */
/*  GL_C4UB_V3F */
/*  GL_C3F_V3F */
/*  GL_N3F_V3F */
/*  GL_C4F_N3F_V3F */
/*  GL_T2F_V3F */
/*  GL_T4F_V4F */
/*  GL_T2F_C4UB_V3F */
/*  GL_T2F_C3F_V3F */
/*  GL_T2F_N3F_V3F */
/*  GL_T2F_C4F_N3F_V3F */
/*  GL_T4F_C4F_N3F_V4F */

/* ListMode */
#define GL_COMPILE 0x1300
#define GL_COMPILE_AND_EXECUTE 0x1301

/* ListNameType */
/*  GL_BYTE */
/*  GL_UNSIGNED_BYTE */
/*  GL_SHORT */
/*  GL_UNSIGNED_SHORT */
/*  GL_INT */
/*  GL_UNSIGNED_INT */
/*  GL_FLOAT */
/*  GL_2_BYTES */
/*  GL_3_BYTES */
/*  GL_4_BYTES */

/* LogicOp */
#define GL_CLEAR 0x1500
#define GL_AND 0x1501
#define GL_AND_REVERSE 0x1502
#define GL_COPY 0x1503
#define GL_AND_INVERTED 0x1504
#define GL_NOOP 0x1505
#define GL_XOR 0x1506
#define GL_OR 0x1507
#define GL_NOR 0x1508
#define GL_EQUIV 0x1509
#define GL_INVERT 0x150A
#define GL_OR_REVERSE 0x150B
#define GL_COPY_INVERTED 0x150C
#define GL_OR_INVERTED 0x150D
#define GL_NAND 0x150E
#define GL_SET 0x150F

/* MapTarget */
/*  GL_MAP1_COLOR_4 */
/*  GL_MAP1_INDEX */
/*  GL_MAP1_NORMAL */
/*  GL_MAP1_TEXTURE_COORD_1 */
/*  GL_MAP1_TEXTURE_COORD_2 */
/*  GL_MAP1_TEXTURE_COORD_3 */
/*  GL_MAP1_TEXTURE_COORD_4 */
/*  GL_MAP1_VERTEX_3 */
/*  GL_MAP1_VERTEX_4 */
/*  GL_MAP2_COLOR_4 */
/*  GL_MAP2_INDEX */
/*  GL_MAP2_NORMAL */
/*  GL_MAP2_TEXTURE_COORD_1 */
/*  GL_MAP2_TEXTURE_COORD_2 */
/*  GL_MAP2_TEXTURE_COORD_3 */
/*  GL_MAP2_TEXTURE_COORD_4 */
/*  GL_MAP2_VERTEX_3 */
/*  GL_MAP2_VERTEX_4 */

/* MaterialFace */
/*  GL_FRONT */
/*  GL_BACK */
/*  GL_FRONT_AND_BACK */

/* MaterialParameter */
#define GL_EMISSION 0x1600
#define GL_SHININESS 0x1601
#define GL_AMBIENT_AND_DIFFUSE 0x1602
#define GL_COLOR_INDEXES 0x1603
/*  GL_AMBIENT */
/*  GL_DIFFUSE */
/*  GL_SPECULAR */

/* MatrixMode */
#define GL_MODELVIEW 0x1700
#define GL_PROJECTION 0x1701
#define GL_TEXTURE 0x1702

/* MeshMode1 */
/*  GL_POINT */
/*  GL_LINE */

/* MeshMode2 */
/*  GL_POINT */
/*  GL_LINE */
/*  GL_FILL */

/* NormalPointerType */
/*  GL_BYTE */
/*  GL_SHORT */
/*  GL_INT */
/*  GL_FLOAT */
/*  GL_DOUBLE */

/* PixelCopyType */
#define GL_COLOR 0x1800
#define GL_DEPTH 0x1801
#define GL_STENCIL 0x1802

/* PixelFormat */
#define GL_COLOR_INDEX 0x1900
#define GL_STENCIL_INDEX 0x1901
#define GL_DEPTH_COMPONENT 0x1902
#define GL_RED 0x1903
#define GL_GREEN 0x1904
#define GL_BLUE 0x1905
#define GL_ALPHA 0x1906
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_LUMINANCE 0x1909
#define GL_LUMINANCE_ALPHA 0x190A

/* PixelMap */
/*  GL_PIXEL_MAP_I_TO_I */
/*  GL_PIXEL_MAP_S_TO_S */
/*  GL_PIXEL_MAP_I_TO_R */
/*  GL_PIXEL_MAP_I_TO_G */
/*  GL_PIXEL_MAP_I_TO_B */
/*  GL_PIXEL_MAP_I_TO_A */
/*  GL_PIXEL_MAP_R_TO_R */
/*  GL_PIXEL_MAP_G_TO_G */
/*  GL_PIXEL_MAP_B_TO_B */
/*  GL_PIXEL_MAP_A_TO_A */

/* PixelStore */
/*  GL_UNPACK_SWAP_BYTES */
/*  GL_UNPACK_LSB_FIRST */
/*  GL_UNPACK_ROW_LENGTH */
/*  GL_UNPACK_SKIP_ROWS */
/*  GL_UNPACK_SKIP_PIXELS */
/*  GL_UNPACK_ALIGNMENT */
/*  GL_PACK_SWAP_BYTES */
/*  GL_PACK_LSB_FIRST */
/*  GL_PACK_ROW_LENGTH */
/*  GL_PACK_SKIP_ROWS */
/*  GL_PACK_SKIP_PIXELS */
/*  GL_PACK_ALIGNMENT */

/* PixelTransfer */
/*  GL_MAP_COLOR */
/*  GL_MAP_STENCIL */
/*  GL_INDEX_SHIFT */
/*  GL_INDEX_OFFSET */
/*  GL_RED_SCALE */
/*  GL_RED_BIAS */
/*  GL_GREEN_SCALE */
/*  GL_GREEN_BIAS */
/*  GL_BLUE_SCALE */
/*  GL_BLUE_BIAS */
/*  GL_ALPHA_SCALE */
/*  GL_ALPHA_BIAS */
/*  GL_DEPTH_SCALE */
/*  GL_DEPTH_BIAS */

/* PixelType */
#define GL_BITMAP 0x1A00
/*  GL_BYTE */
/*  GL_UNSIGNED_BYTE */
/*  GL_SHORT */
/*  GL_UNSIGNED_SHORT */
/*  GL_INT */
/*  GL_UNSIGNED_INT */
/*  GL_FLOAT */

/* PolygonMode */
#define GL_POINT 0x1B00
#define GL_LINE 0x1B01
#define GL_FILL 0x1B02

/* ReadBufferMode */
/*  GL_FRONT_LEFT */
/*  GL_FRONT_RIGHT */
/*  GL_BACK_LEFT */
/*  GL_BACK_RIGHT */
/*  GL_FRONT */
/*  GL_BACK */
/*  GL_LEFT */
/*  GL_RIGHT */
/*  GL_AUX0 */
/*  GL_AUX1 */
/*  GL_AUX2 */
/*  GL_AUX3 */

/* RenderingMode */
#define GL_RENDER 0x1C00
#define GL_FEEDBACK 0x1C01
#define GL_SELECT 0x1C02

/* ShadingModel */
#define GL_FLAT 0x1D00
#define GL_SMOOTH 0x1D01


/* StencilFunction */
/*  GL_NEVER */
/*  GL_LESS */
/*  GL_EQUAL */
/*  GL_LEQUAL */
/*  GL_GREATER */
/*  GL_NOTEQUAL */
/*  GL_GEQUAL */
/*  GL_ALWAYS */

/* StencilOp */
/*  GL_ZERO */
#define GL_KEEP 0x1E00
#define GL_REPLACE 0x1E01
#define GL_INCR 0x1E02
#define GL_DECR 0x1E03
/*  GL_INVERT */

/* StringName */
#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_EXTENSIONS 0x1F03

/* TextureCoordName */
#define GL_S 0x2000
#define GL_T 0x2001
#define GL_R 0x2002
#define GL_Q 0x2003

/* TexCoordPointerType */
/*  GL_SHORT */
/*  GL_INT */
/*  GL_FLOAT */
/*  GL_DOUBLE */

/* TextureEnvMode */
#define GL_MODULATE 0x2100
#define GL_DECAL 0x2101
/*  GL_BLEND */
/*  GL_REPLACE */

/* TextureEnvParameter */
#define GL_TEXTURE_ENV_MODE 0x2200
#define GL_TEXTURE_ENV_COLOR 0x2201

/* TextureEnvTarget */
#define GL_TEXTURE_ENV 0x2300

/* TextureGenMode */
#define GL_EYE_LINEAR 0x2400
#define GL_OBJECT_LINEAR 0x2401
#define GL_SPHERE_MAP 0x2402

/* TextureGenParameter */
#define GL_TEXTURE_GEN_MODE 0x2500
#define GL_OBJECT_PLANE 0x2501
#define GL_EYE_PLANE 0x2502

/* TextureMagFilter */
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601

/* TextureMinFilter */
/*  GL_NEAREST */
/*  GL_LINEAR */
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_LINEAR_MIPMAP_NEAREST 0x2701
#define GL_NEAREST_MIPMAP_LINEAR 0x2702
#define GL_LINEAR_MIPMAP_LINEAR 0x2703

/* TextureParameterName */
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
/*  GL_TEXTURE_BORDER_COLOR */
/*  GL_TEXTURE_PRIORITY */

/* TextureTarget */
/*  GL_TEXTURE_1D */
/*  GL_TEXTURE_2D */
/*  GL_PROXY_TEXTURE_1D */
/*  GL_PROXY_TEXTURE_2D */

/* TextureWrapMode */
#define GL_CLAMP 0x2900
#define GL_REPEAT 0x2901

/* VertexPointerType */
/*  GL_SHORT */
/*  GL_INT */
/*  GL_FLOAT */
/*  GL_DOUBLE */

/* ClientAttribMask */
#define GL_CLIENT_PIXEL_STORE_BIT 0x00000001
#define GL_CLIENT_VERTEX_ARRAY_BIT 0x00000002
#define GL_CLIENT_ALL_ATTRIB_BITS 0xffffffff

/* polygon_offset */
#define GL_POLYGON_OFFSET_FACTOR 0x8038
#define GL_POLYGON_OFFSET_UNITS 0x2A00
#define GL_POLYGON_OFFSET_POINT 0x2A01
#define GL_POLYGON_OFFSET_LINE 0x2A02
#define GL_POLYGON_OFFSET_FILL 0x8037

/* texture */
#define GL_ALPHA4 0x803B
#define GL_ALPHA8 0x803C
#define GL_ALPHA12 0x803D
#define GL_ALPHA16 0x803E
#define GL_LUMINANCE4 0x803F
#define GL_LUMINANCE8 0x8040
#define GL_LUMINANCE12 0x8041
#define GL_LUMINANCE16 0x8042
#define GL_LUMINANCE4_ALPHA4 0x8043
#define GL_LUMINANCE6_ALPHA2 0x8044
#define GL_LUMINANCE8_ALPHA8 0x8045
#define GL_LUMINANCE12_ALPHA4 0x8046
#define GL_LUMINANCE12_ALPHA12 0x8047
#define GL_LUMINANCE16_ALPHA16 0x8048
#define GL_INTENSITY 0x8049
#define GL_INTENSITY4 0x804A
#define GL_INTENSITY8 0x804B
#define GL_INTENSITY12 0x804C
#define GL_INTENSITY16 0x804D
#define GL_R3_G3_B2 0x2A10
#define GL_RGB4 0x804F
#define GL_RGB5 0x8050
#define GL_RGB8 0x8051
#define GL_RGB10 0x8052
#define GL_RGB12 0x8053
#define GL_RGB16 0x8054
#define GL_RGBA2 0x8055
#define GL_RGBA4 0x8056
#define GL_RGB5_A1 0x8057
#define GL_RGBA8 0x8058
#define GL_RGB10_A2 0x8059
#define GL_RGBA12 0x805A
#define GL_RGBA16 0x805B
#define GL_TEXTURE_RED_SIZE 0x805C
#define GL_TEXTURE_GREEN_SIZE 0x805D
#define GL_TEXTURE_BLUE_SIZE 0x805E
#define GL_TEXTURE_ALPHA_SIZE 0x805F
#define GL_TEXTURE_LUMINANCE_SIZE 0x8060
#define GL_TEXTURE_INTENSITY_SIZE 0x8061
#define GL_PROXY_TEXTURE_1D 0x8063
#define GL_PROXY_TEXTURE_2D 0x8064

/* texture_object */
#define GL_TEXTURE_PRIORITY 0x8066
#define GL_TEXTURE_RESIDENT 0x8067
#define GL_TEXTURE_BINDING_1D 0x8068
#define GL_TEXTURE_BINDING_2D 0x8069

/* vertex_array */
#define GL_VERTEX_ARRAY 0x8074
#define GL_NORMAL_ARRAY 0x8075
#define GL_COLOR_ARRAY 0x8076
#define GL_INDEX_ARRAY 0x8077
#define GL_TEXTURE_COORD_ARRAY 0x8078
#define GL_EDGE_FLAG_ARRAY 0x8079
#define GL_VERTEX_ARRAY_SIZE 0x807A
#define GL_VERTEX_ARRAY_TYPE 0x807B
#define GL_VERTEX_ARRAY_STRIDE 0x807C
#define GL_NORMAL_ARRAY_TYPE 0x807E
#define GL_NORMAL_ARRAY_STRIDE 0x807F
#define GL_COLOR_ARRAY_SIZE 0x8081
#define GL_COLOR_ARRAY_TYPE 0x8082
#define GL_COLOR_ARRAY_STRIDE 0x8083
#define GL_INDEX_ARRAY_TYPE 0x8085
#define GL_INDEX_ARRAY_STRIDE 0x8086
#define GL_TEXTURE_COORD_ARRAY_SIZE 0x8088
#define GL_TEXTURE_COORD_ARRAY_TYPE 0x8089
#define GL_TEXTURE_COORD_ARRAY_STRIDE 0x808A
#define GL_EDGE_FLAG_ARRAY_STRIDE 0x808C
#define GL_VERTEX_ARRAY_POINTER 0x808E
#define GL_NORMAL_ARRAY_POINTER 0x808F
#define GL_COLOR_ARRAY_POINTER 0x8090
#define GL_INDEX_ARRAY_POINTER 0x8091
#define GL_TEXTURE_COORD_ARRAY_POINTER 0x8092
#define GL_EDGE_FLAG_ARRAY_POINTER 0x8093
#define GL_V2F 0x2A20
#define GL_V3F 0x2A21
#define GL_C4UB_V2F 0x2A22
#define GL_C4UB_V3F 0x2A23
#define GL_C3F_V3F 0x2A24
#define GL_N3F_V3F 0x2A25
#define GL_C4F_N3F_V3F 0x2A26
#define GL_T2F_V3F 0x2A27
#define GL_T4F_V4F 0x2A28
#define GL_T2F_C4UB_V3F 0x2A29
#define GL_T2F_C3F_V3F 0x2A2A
#define GL_T2F_N3F_V3F 0x2A2B
#define GL_T2F_C4F_N3F_V3F 0x2A2C
#define GL_T4F_C4F_N3F_V4F 0x2A2D

/* Extensions */
#define GL_EXT_vertex_array 1
#define GL_EXT_bgra 1
#define GL_EXT_paletted_texture 1
#define GL_WIN_swap_hint 1
#define GL_WIN_draw_range_elements 1
// #define GL_WIN_phong_shading 1
// #define GL_WIN_specular_fog 1

/* EXT_vertex_array */
#define GL_VERTEX_ARRAY_EXT 0x8074
#define GL_NORMAL_ARRAY_EXT 0x8075
#define GL_COLOR_ARRAY_EXT 0x8076
#define GL_INDEX_ARRAY_EXT 0x8077
#define GL_TEXTURE_COORD_ARRAY_EXT 0x8078
#define GL_EDGE_FLAG_ARRAY_EXT 0x8079
#define GL_VERTEX_ARRAY_SIZE_EXT 0x807A
#define GL_VERTEX_ARRAY_TYPE_EXT 0x807B
#define GL_VERTEX_ARRAY_STRIDE_EXT 0x807C
#define GL_VERTEX_ARRAY_COUNT_EXT 0x807D
#define GL_NORMAL_ARRAY_TYPE_EXT 0x807E
#define GL_NORMAL_ARRAY_STRIDE_EXT 0x807F
#define GL_NORMAL_ARRAY_COUNT_EXT 0x8080
#define GL_COLOR_ARRAY_SIZE_EXT 0x8081
#define GL_COLOR_ARRAY_TYPE_EXT 0x8082
#define GL_COLOR_ARRAY_STRIDE_EXT 0x8083
#define GL_COLOR_ARRAY_COUNT_EXT 0x8084
#define GL_INDEX_ARRAY_TYPE_EXT 0x8085
#define GL_INDEX_ARRAY_STRIDE_EXT 0x8086
#define GL_INDEX_ARRAY_COUNT_EXT 0x8087
#define GL_TEXTURE_COORD_ARRAY_SIZE_EXT 0x8088
#define GL_TEXTURE_COORD_ARRAY_TYPE_EXT 0x8089
#define GL_TEXTURE_COORD_ARRAY_STRIDE_EXT 0x808A
#define GL_TEXTURE_COORD_ARRAY_COUNT_EXT 0x808B
#define GL_EDGE_FLAG_ARRAY_STRIDE_EXT 0x808C
#define GL_EDGE_FLAG_ARRAY_COUNT_EXT 0x808D
#define GL_VERTEX_ARRAY_POINTER_EXT 0x808E
#define GL_NORMAL_ARRAY_POINTER_EXT 0x808F
#define GL_COLOR_ARRAY_POINTER_EXT 0x8090
#define GL_INDEX_ARRAY_POINTER_EXT 0x8091
#define GL_TEXTURE_COORD_ARRAY_POINTER_EXT 0x8092
#define GL_EDGE_FLAG_ARRAY_POINTER_EXT 0x8093
#define GL_DOUBLE_EXT GL_DOUBLE

/* EXT_bgra */
#define GL_BGR_EXT 0x80E0
#define GL_BGRA_EXT 0x80E1

/* EXT_paletted_texture */

/* These must match the GL_COLOR_TABLE_*_SGI enumerants */
#define GL_COLOR_TABLE_FORMAT_EXT 0x80D8
#define GL_COLOR_TABLE_WIDTH_EXT 0x80D9
#define GL_COLOR_TABLE_RED_SIZE_EXT 0x80DA
#define GL_COLOR_TABLE_GREEN_SIZE_EXT 0x80DB
#define GL_COLOR_TABLE_BLUE_SIZE_EXT 0x80DC
#define GL_COLOR_TABLE_ALPHA_SIZE_EXT 0x80DD
#define GL_COLOR_TABLE_LUMINANCE_SIZE_EXT 0x80DE
#define GL_COLOR_TABLE_INTENSITY_SIZE_EXT 0x80DF

#define GL_COLOR_INDEX1_EXT 0x80E2
#define GL_COLOR_INDEX2_EXT 0x80E3
#define GL_COLOR_INDEX4_EXT 0x80E4
#define GL_COLOR_INDEX8_EXT 0x80E5
#define GL_COLOR_INDEX12_EXT 0x80E6
#define GL_COLOR_INDEX16_EXT 0x80E7

/* WIN_draw_range_elements */
#define GL_MAX_ELEMENTS_VERTICES_WIN 0x80E8
#define GL_MAX_ELEMENTS_INDICES_WIN 0x80E9

/* WIN_phong_shading */
#define GL_PHONG_WIN 0x80EA 
#define GL_PHONG_HINT_WIN 0x80EB 

/* WIN_specular_fog */
#define GL_FOG_SPECULAR_TEXTURE_WIN 0x80EC

/* For compatibility with OpenGL v1.0 */
#define GL_LOGIC_OP GL_INDEX_LOGIC_OP
#define GL_TEXTURE_COMPONENTS GL_TEXTURE_INTERNAL_FORMAT

/* ************************************************************/

void glAccum (GLenum op, GLfloat value);
void glAlphaFunc (GLenum func, GLclampf ref);
GLboolean glAreTexturesResident (GLsizei n, const GLuint *textures, GLboolean *residences);
void glArrayElement (GLint i);
void glBegin (GLenum mode);
void glBindTexture (GLenum target, GLuint texture);
void glBitmap (GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap);
void glBlendFunc (GLenum sfactor, GLenum dfactor);
void glCallList (GLuint list);
void glCallLists (GLsizei n, GLenum type, const GLvoid *lists);
void glClear (GLbitfield mask);
void glClearAccum (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void glClearColor (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
void glClearDepth (GLclampd depth);
void glClearIndex (GLfloat c);
void glClearStencil (GLint s);
void glClipPlane (GLenum plane, const GLdouble *equation);
void glColor3b (GLbyte red, GLbyte green, GLbyte blue);
void glColor3bv (const GLbyte *v);
void glColor3d (GLdouble red, GLdouble green, GLdouble blue);
void glColor3dv (const GLdouble *v);
void glColor3f (GLfloat red, GLfloat green, GLfloat blue);
void glColor3fv (const GLfloat *v);
void glColor3i (GLint red, GLint green, GLint blue);
void glColor3iv (const GLint *v);
void glColor3s (GLshort red, GLshort green, GLshort blue);
void glColor3sv (const GLshort *v);
void glColor3ub (GLubyte red, GLubyte green, GLubyte blue);
void glColor3ubv (const GLubyte *v);
void glColor3ui (GLuint red, GLuint green, GLuint blue);
void glColor3uiv (const GLuint *v);
void glColor3us (GLushort red, GLushort green, GLushort blue);
void glColor3usv (const GLushort *v);
void glColor4b (GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha);
void glColor4bv (const GLbyte *v);
void glColor4d (GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha);
void glColor4dv (const GLdouble *v);
void glColor4f (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void glColor4fv (const GLfloat *v);
void glColor4i (GLint red, GLint green, GLint blue, GLint alpha);
void glColor4iv (const GLint *v);
void glColor4s (GLshort red, GLshort green, GLshort blue, GLshort alpha);
void glColor4sv (const GLshort *v);
void glColor4ub (GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha);
void glColor4ubv (const GLubyte *v);
void glColor4ui (GLuint red, GLuint green, GLuint blue, GLuint alpha);
void glColor4uiv (const GLuint *v);
void glColor4us (GLushort red, GLushort green, GLushort blue, GLushort alpha);
void glColor4usv (const GLushort *v);
void glColorMask (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
void glColorMaterial (GLenum face, GLenum mode);
void glColorPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void glCopyPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum type);
void glCopyTexImage1D (GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLint border);
void glCopyTexImage2D (GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
void glCopyTexSubImage1D (GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
void glCopyTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
void glCullFace (GLenum mode);
void glDeleteLists (GLuint list, GLsizei range);
void glDeleteTextures (GLsizei n, const GLuint *textures);
void glDepthFunc (GLenum func);
void glDepthMask (GLboolean flag);
void glDepthRange (GLclampd zNear, GLclampd zFar);
void glDisable (GLenum cap);
void glDisableClientState (GLenum array);
void glDrawArrays (GLenum mode, GLint first, GLsizei count);
void glDrawBuffer (GLenum mode);
void glDrawElements (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
void glDrawPixels (GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
void glEdgeFlag (GLboolean flag);
void glEdgeFlagPointer (GLsizei stride, const GLvoid *pointer);
void glEdgeFlagv (const GLboolean *flag);
void glEnable (GLenum cap);
void glEnableClientState (GLenum array);
void glEnd (void);
void glEndList (void);
void glEvalCoord1d (GLdouble u);
void glEvalCoord1dv (const GLdouble *u);
void glEvalCoord1f (GLfloat u);
void glEvalCoord1fv (const GLfloat *u);
void glEvalCoord2d (GLdouble u, GLdouble v);
void glEvalCoord2dv (const GLdouble *u);
void glEvalCoord2f (GLfloat u, GLfloat v);
void glEvalCoord2fv (const GLfloat *u);
void glEvalMesh1 (GLenum mode, GLint i1, GLint i2);
void glEvalMesh2 (GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2);
void glEvalPoint1 (GLint i);
void glEvalPoint2 (GLint i, GLint j);
void glFeedbackBuffer (GLsizei size, GLenum type, GLfloat *buffer);
void glFinish (void);
void glFlush (void);
void glFogf (GLenum pname, GLfloat param);
void glFogfv (GLenum pname, const GLfloat *params);
void glFogi (GLenum pname, GLint param);
void glFogiv (GLenum pname, const GLint *params);
void glFrontFace (GLenum mode);
void glFrustum (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
GLuint glGenLists (GLsizei range);
void glGenTextures (GLsizei n, GLuint *textures);
void glGetBooleanv (GLenum pname, GLboolean *params);
void glGetClipPlane (GLenum plane, GLdouble *equation);
void glGetDoublev (GLenum pname, GLdouble *params);
GLenum APIENTRY glGetError (void);
void glGetFloatv (GLenum pname, GLfloat *params);
void APIENTRY glGetIntegerv (GLenum pname, GLint *params);
void glGetLightfv (GLenum light, GLenum pname, GLfloat *params);
void glGetLightiv (GLenum light, GLenum pname, GLint *params);
void glGetMapdv (GLenum target, GLenum query, GLdouble *v);
void glGetMapfv (GLenum target, GLenum query, GLfloat *v);
void glGetMapiv (GLenum target, GLenum query, GLint *v);
void glGetMaterialfv (GLenum face, GLenum pname, GLfloat *params);
void glGetMaterialiv (GLenum face, GLenum pname, GLint *params);
void glGetPixelMapfv (GLenum map, GLfloat *values);
void glGetPixelMapuiv (GLenum map, GLuint *values);
void glGetPixelMapusv (GLenum map, GLushort *values);
void glGetPointerv (GLenum pname, GLvoid **params);
void glGetPolygonStipple (GLubyte *mask);
const GLubyte * APIENTRY glGetString (GLenum name);
void glGetTexEnvfv (GLenum target, GLenum pname, GLfloat *params);
void glGetTexEnviv (GLenum target, GLenum pname, GLint *params);
void glGetTexGendv (GLenum coord, GLenum pname, GLdouble *params);
void glGetTexGenfv (GLenum coord, GLenum pname, GLfloat *params);
void glGetTexGeniv (GLenum coord, GLenum pname, GLint *params);
void glGetTexImage (GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels);
void glGetTexLevelParameterfv (GLenum target, GLint level, GLenum pname, GLfloat *params);
void glGetTexLevelParameteriv (GLenum target, GLint level, GLenum pname, GLint *params);
void glGetTexParameterfv (GLenum target, GLenum pname, GLfloat *params);
void glGetTexParameteriv (GLenum target, GLenum pname, GLint *params);
void glHint (GLenum target, GLenum mode);
void glIndexMask (GLuint mask);
void glIndexPointer (GLenum type, GLsizei stride, const GLvoid *pointer);
void glIndexd (GLdouble c);
void glIndexdv (const GLdouble *c);
void glIndexf (GLfloat c);
void glIndexfv (const GLfloat *c);
void glIndexi (GLint c);
void glIndexiv (const GLint *c);
void glIndexs (GLshort c);
void glIndexsv (const GLshort *c);
void glIndexub (GLubyte c);
void glIndexubv (const GLubyte *c);
void glInitNames (void);
void glInterleavedArrays (GLenum format, GLsizei stride, const GLvoid *pointer);
GLboolean glIsEnabled (GLenum cap);
GLboolean glIsList (GLuint list);
GLboolean glIsTexture (GLuint texture);
void glLightModelf (GLenum pname, GLfloat param);
void glLightModelfv (GLenum pname, const GLfloat *params);
void glLightModeli (GLenum pname, GLint param);
void glLightModeliv (GLenum pname, const GLint *params);
void glLightf (GLenum light, GLenum pname, GLfloat param);
void glLightfv (GLenum light, GLenum pname, const GLfloat *params);
void glLighti (GLenum light, GLenum pname, GLint param);
void glLightiv (GLenum light, GLenum pname, const GLint *params);
void glLineStipple (GLint factor, GLushort pattern);
void glLineWidth (GLfloat width);
void glListBase (GLuint base);
void glLoadIdentity (void);
void glLoadMatrixd (const GLdouble *m);
void glLoadMatrixf (const GLfloat *m);
void glLoadName (GLuint name);
void glLogicOp (GLenum opcode);
void glMap1d (GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *points);
void glMap1f (GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *points);
void glMap2d (GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points);
void glMap2f (GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points);
void glMapGrid1d (GLint un, GLdouble u1, GLdouble u2);
void glMapGrid1f (GLint un, GLfloat u1, GLfloat u2);
void glMapGrid2d (GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2);
void glMapGrid2f (GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2);
void glMaterialf (GLenum face, GLenum pname, GLfloat param);
void glMaterialfv (GLenum face, GLenum pname, const GLfloat *params);
void glMateriali (GLenum face, GLenum pname, GLint param);
void glMaterialiv (GLenum face, GLenum pname, const GLint *params);
void glMatrixMode (GLenum mode);
void glMultMatrixd (const GLdouble *m);
void glMultMatrixf (const GLfloat *m);
void glNewList (GLuint list, GLenum mode);
void glNormal3b (GLbyte nx, GLbyte ny, GLbyte nz);
void glNormal3bv (const GLbyte *v);
void glNormal3d (GLdouble nx, GLdouble ny, GLdouble nz);
void glNormal3dv (const GLdouble *v);
void glNormal3f (GLfloat nx, GLfloat ny, GLfloat nz);
void glNormal3fv (const GLfloat *v);
void glNormal3i (GLint nx, GLint ny, GLint nz);
void glNormal3iv (const GLint *v);
void glNormal3s (GLshort nx, GLshort ny, GLshort nz);
void glNormal3sv (const GLshort *v);
void glNormalPointer (GLenum type, GLsizei stride, const GLvoid *pointer);
void glOrtho (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
void glPassThrough (GLfloat token);
void glPixelMapfv (GLenum map, GLsizei mapsize, const GLfloat *values);
void glPixelMapuiv (GLenum map, GLsizei mapsize, const GLuint *values);
void glPixelMapusv (GLenum map, GLsizei mapsize, const GLushort *values);
void glPixelStoref (GLenum pname, GLfloat param);
void glPixelStorei (GLenum pname, GLint param);
void glPixelTransferf (GLenum pname, GLfloat param);
void glPixelTransferi (GLenum pname, GLint param);
void glPixelZoom (GLfloat xfactor, GLfloat yfactor);
void glPointSize (GLfloat size);
void glPolygonMode (GLenum face, GLenum mode);
void glPolygonOffset (GLfloat factor, GLfloat units);
void glPolygonStipple (const GLubyte *mask);
void glPopAttrib (void);
void glPopClientAttrib (void);
void glPopMatrix (void);
void glPopName (void);
void glPrioritizeTextures (GLsizei n, const GLuint *textures, const GLclampf *priorities);
void glPushAttrib (GLbitfield mask);
void glPushClientAttrib (GLbitfield mask);
void glPushMatrix (void);
void glPushName (GLuint name);
void glRasterPos2d (GLdouble x, GLdouble y);
void glRasterPos2dv (const GLdouble *v);
void glRasterPos2f (GLfloat x, GLfloat y);
void glRasterPos2fv (const GLfloat *v);
void glRasterPos2i (GLint x, GLint y);
void glRasterPos2iv (const GLint *v);
void glRasterPos2s (GLshort x, GLshort y);
void glRasterPos2sv (const GLshort *v);
void glRasterPos3d (GLdouble x, GLdouble y, GLdouble z);
void glRasterPos3dv (const GLdouble *v);
void glRasterPos3f (GLfloat x, GLfloat y, GLfloat z);
void glRasterPos3fv (const GLfloat *v);
void glRasterPos3i (GLint x, GLint y, GLint z);
void glRasterPos3iv (const GLint *v);
void glRasterPos3s (GLshort x, GLshort y, GLshort z);
void glRasterPos3sv (const GLshort *v);
void glRasterPos4d (GLdouble x, GLdouble y, GLdouble z, GLdouble w);
void glRasterPos4dv (const GLdouble *v);
void glRasterPos4f (GLfloat x, GLfloat y, GLfloat z, GLfloat w);
void glRasterPos4fv (const GLfloat *v);
void glRasterPos4i (GLint x, GLint y, GLint z, GLint w);
void glRasterPos4iv (const GLint *v);
void glRasterPos4s (GLshort x, GLshort y, GLshort z, GLshort w);
void glRasterPos4sv (const GLshort *v);
void glReadBuffer (GLenum mode);
void glReadPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
void glRectd (GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2);
void glRectdv (const GLdouble *v1, const GLdouble *v2);
void glRectf (GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);
void glRectfv (const GLfloat *v1, const GLfloat *v2);
void glRecti (GLint x1, GLint y1, GLint x2, GLint y2);
void glRectiv (const GLint *v1, const GLint *v2);
void glRects (GLshort x1, GLshort y1, GLshort x2, GLshort y2);
void glRectsv (const GLshort *v1, const GLshort *v2);
GLint glRenderMode (GLenum mode);
void glRotated (GLdouble angle, GLdouble x, GLdouble y, GLdouble z);
void glRotatef (GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void glScaled (GLdouble x, GLdouble y, GLdouble z);
void glScalef (GLfloat x, GLfloat y, GLfloat z);
void glScissor (GLint x, GLint y, GLsizei width, GLsizei height);
void glSelectBuffer (GLsizei size, GLuint *buffer);
void glShadeModel (GLenum mode);
void glStencilFunc (GLenum func, GLint ref, GLuint mask);
void glStencilMask (GLuint mask);
void glStencilOp (GLenum fail, GLenum zfail, GLenum zpass);
void glTexCoord1d (GLdouble s);
void glTexCoord1dv (const GLdouble *v);
void glTexCoord1f (GLfloat s);
void glTexCoord1fv (const GLfloat *v);
void glTexCoord1i (GLint s);
void glTexCoord1iv (const GLint *v);
void glTexCoord1s (GLshort s);
void glTexCoord1sv (const GLshort *v);
void glTexCoord2d (GLdouble s, GLdouble t);
void glTexCoord2dv (const GLdouble *v);
void glTexCoord2f (GLfloat s, GLfloat t);
void glTexCoord2fv (const GLfloat *v);
void glTexCoord2i (GLint s, GLint t);
void glTexCoord2iv (const GLint *v);
void glTexCoord2s (GLshort s, GLshort t);
void glTexCoord2sv (const GLshort *v);
void glTexCoord3d (GLdouble s, GLdouble t, GLdouble r);
void glTexCoord3dv (const GLdouble *v);
void glTexCoord3f (GLfloat s, GLfloat t, GLfloat r);
void glTexCoord3fv (const GLfloat *v);
void glTexCoord3i (GLint s, GLint t, GLint r);
void glTexCoord3iv (const GLint *v);
void glTexCoord3s (GLshort s, GLshort t, GLshort r);
void glTexCoord3sv (const GLshort *v);
void glTexCoord4d (GLdouble s, GLdouble t, GLdouble r, GLdouble q);
void glTexCoord4dv (const GLdouble *v);
void glTexCoord4f (GLfloat s, GLfloat t, GLfloat r, GLfloat q);
void glTexCoord4fv (const GLfloat *v);
void glTexCoord4i (GLint s, GLint t, GLint r, GLint q);
void glTexCoord4iv (const GLint *v);
void glTexCoord4s (GLshort s, GLshort t, GLshort r, GLshort q);
void glTexCoord4sv (const GLshort *v);
void glTexCoordPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void glTexEnvf (GLenum target, GLenum pname, GLfloat param);
void glTexEnvfv (GLenum target, GLenum pname, const GLfloat *params);
void glTexEnvi (GLenum target, GLenum pname, GLint param);
void glTexEnviv (GLenum target, GLenum pname, const GLint *params);
void glTexGend (GLenum coord, GLenum pname, GLdouble param);
void glTexGendv (GLenum coord, GLenum pname, const GLdouble *params);
void glTexGenf (GLenum coord, GLenum pname, GLfloat param);
void glTexGenfv (GLenum coord, GLenum pname, const GLfloat *params);
void glTexGeni (GLenum coord, GLenum pname, GLint param);
void glTexGeniv (GLenum coord, GLenum pname, const GLint *params);
void glTexImage1D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
void glTexImage2D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
void glTexParameterf (GLenum target, GLenum pname, GLfloat param);
void glTexParameterfv (GLenum target, GLenum pname, const GLfloat *params);
void glTexParameteri (GLenum target, GLenum pname, GLint param);
void glTexParameteriv (GLenum target, GLenum pname, const GLint *params);
void glTexSubImage1D (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels);
void glTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
void glTranslated (GLdouble x, GLdouble y, GLdouble z);
void glTranslatef (GLfloat x, GLfloat y, GLfloat z);
void glVertex2d (GLdouble x, GLdouble y);
void glVertex2dv (const GLdouble *v);
void glVertex2f (GLfloat x, GLfloat y);
void glVertex2fv (const GLfloat *v);
void glVertex2i (GLint x, GLint y);
void glVertex2iv (const GLint *v);
void glVertex2s (GLshort x, GLshort y);
void glVertex2sv (const GLshort *v);
void glVertex3d (GLdouble x, GLdouble y, GLdouble z);
void glVertex3dv (const GLdouble *v);
void glVertex3f (GLfloat x, GLfloat y, GLfloat z);
void glVertex3fv (const GLfloat *v);
void glVertex3i (GLint x, GLint y, GLint z);
void glVertex3iv (const GLint *v);
void glVertex3s (GLshort x, GLshort y, GLshort z);
void glVertex3sv (const GLshort *v);
void glVertex4d (GLdouble x, GLdouble y, GLdouble z, GLdouble w);
void glVertex4dv (const GLdouble *v);
void glVertex4f (GLfloat x, GLfloat y, GLfloat z, GLfloat w);
void glVertex4fv (const GLfloat *v);
void glVertex4i (GLint x, GLint y, GLint z, GLint w);
void glVertex4iv (const GLint *v);
void glVertex4s (GLshort x, GLshort y, GLshort z, GLshort w);
void glVertex4sv (const GLshort *v);
void glVertexPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void glViewport (GLint x, GLint y, GLsizei width, GLsizei height);

/* EXT_vertex_array */
typedef void (*PFNGLARRAYELEMENTEXTPROC) (GLint i);
typedef void (*PFNGLDRAWARRAYSEXTPROC) (GLenum mode, GLint first, GLsizei count);
typedef void (*PFNGLVERTEXPOINTEREXTPROC) (GLint size, GLenum type, GLsizei stride, GLsizei count, const GLvoid *pointer);
typedef void (*PFNGLNORMALPOINTEREXTPROC) (GLenum type, GLsizei stride, GLsizei count, const GLvoid *pointer);
typedef void (*PFNGLCOLORPOINTEREXTPROC) (GLint size, GLenum type, GLsizei stride, GLsizei count, const GLvoid *pointer);
typedef void (*PFNGLINDEXPOINTEREXTPROC) (GLenum type, GLsizei stride, GLsizei count, const GLvoid *pointer);
typedef void (*PFNGLTEXCOORDPOINTEREXTPROC) (GLint size, GLenum type, GLsizei stride, GLsizei count, const GLvoid *pointer);
typedef void (*PFNGLEDGEFLAGPOINTEREXTPROC) (GLsizei stride, GLsizei count, const GLboolean *pointer);
typedef void (*PFNGLGETPOINTERVEXTPROC) (GLenum pname, GLvoid **params);
typedef void (*PFNGLARRAYELEMENTARRAYEXTPROC)(GLenum mode, GLsizei count, const GLvoid *pi);

/* WIN_draw_range_elements */
typedef void (*PFNGLDRAWRANGEELEMENTSWINPROC) (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices);

/* WIN_swap_hint */
typedef void (*PFNGLADDSWAPHINTRECTWINPROC) (GLint x, GLint y, GLsizei width, GLsizei height);

/* EXT_paletted_texture */
typedef void (*PFNGLCOLORTABLEEXTPROC)
 (GLenum target, GLenum internalFormat, GLsizei width, GLenum format,
 GLenum type, const GLvoid *data);
typedef void (*PFNGLCOLORSUBTABLEEXTPROC)
 (GLenum target, GLsizei start, GLsizei count, GLenum format,
 GLenum type, const GLvoid *data);
typedef void (*PFNGLGETCOLORTABLEEXTPROC)
 (GLenum target, GLenum format, GLenum type, GLvoid *data);
typedef void (*PFNGLGETCOLORTABLEPARAMETERIVEXTPROC)
 (GLenum target, GLenum pname, GLint *params);
typedef void (*PFNGLGETCOLORTABLEPARAMETERFVEXTPROC)
 (GLenum target, GLenum pname, GLfloat *params);

// wgl interface
HGLRC WINAPI wglCreateContext (HDC hdc);
BOOL WINAPI wglDeleteContext (HGLRC hglrc);
HGLRC WINAPI wglGetCurrentContext (VOID);
HDC WINAPI wglGetCurrentDC (VOID);
BOOL WINAPI wglMakeCurrent (HDC hdc, HGLRC hglrc);
PROC WINAPI wglGetProcAddress (LPCSTR s);

BOOL WINAPI wglCopyContext(HGLRC, HGLRC, UINT);
HGLRC WINAPI wglCreateLayerContext(HDC, int);
BOOL WINAPI wglShareLists(HGLRC, HGLRC);
BOOL WINAPI wglUseFontBitmaps(HDC, DWORD, DWORD, DWORD);
BOOL WINAPI wglUseFontOutlines(HDC, DWORD, DWORD, DWORD, FLOAT, FLOAT, int, LPGLYPHMETRICSFLOAT);

BOOL WINAPI wglDescribeLayerPlane(HDC, int, int, UINT, LPLAYERPLANEDESCRIPTOR);
int WINAPI wglSetLayerPaletteEntries(HDC, int, int, int, CONST COLORREF *);
int WINAPI wglGetLayerPaletteEntries(HDC, int, int, int, COLORREF *);
BOOL WINAPI wglRealizeLayerPalette(HDC, int, BOOL);
BOOL WINAPI wglSwapLayerBuffers(HDC, UINT);

// override SPF
BOOL WINAPI SetPixelFormat (HDC hdc, int format, CONST PIXELFORMATDESCRIPTOR * ppfd);

// our fake CDS replacement
LONG ChangeDisplaySettings_FakeGL (LPDEVMODE lpDevMode, DWORD dwflags);

// remove cds
#ifdef ChangeDisplaySettings
#undef ChangeDisplaySettings
#define ChangeDisplaySettings ChangeDisplaySettings_FakeGL
#endif

// replacement for GDI SwapBuffers
void FakeSwapBuffers (void);

// replacement for mode resets
void D3D_ResetMode (int width, int height, int bpp, BOOL windowed);

// extension defines
// these are renamed so as to not clash with defines in the program
#define GLD3D_UNSIGNED_BYTE_3_3_2            0x8032
#define GLD3D_UNSIGNED_SHORT_4_4_4_4         0x8033
#define GLD3D_UNSIGNED_SHORT_5_5_5_1         0x8034
#define GLD3D_UNSIGNED_INT_8_8_8_8           0x8035
#define GLD3D_UNSIGNED_INT_10_10_10_2        0x8036
#define GLD3D_RESCALE_NORMAL                 0x803A
#define GLD3D_TEXTURE_BINDING_3D             0x806A
#define GLD3D_PACK_SKIP_IMAGES               0x806B
#define GLD3D_PACK_IMAGE_HEIGHT              0x806C
#define GLD3D_UNPACK_SKIP_IMAGES             0x806D
#define GLD3D_UNPACK_IMAGE_HEIGHT            0x806E
#define GLD3D_TEXTURE_3D                     0x806F
#define GLD3D_PROXY_TEXTURE_3D               0x8070
#define GLD3D_TEXTURE_DEPTH                  0x8071
#define GLD3D_TEXTURE_WRAP_R                 0x8072
#define GLD3D_MAX_3D_TEXTURE_SIZE            0x8073
#define GLD3D_UNSIGNED_BYTE_2_3_3_REV        0x8362
#define GLD3D_UNSIGNED_SHORT_5_6_5           0x8363
#define GLD3D_UNSIGNED_SHORT_5_6_5_REV       0x8364
#define GLD3D_UNSIGNED_SHORT_4_4_4_4_REV     0x8365
#define GLD3D_UNSIGNED_SHORT_1_5_5_5_REV     0x8366
#define GLD3D_UNSIGNED_INT_8_8_8_8_REV       0x8367
#define GLD3D_UNSIGNED_INT_2_10_10_10_REV    0x8368
#define GLD3D_BGR                            0x80E0
#define GLD3D_BGRA                           0x80E1
#define GLD3D_MAX_ELEMENTS_VERTICES          0x80E8
#define GLD3D_MAX_ELEMENTS_INDICES           0x80E9
#define GLD3D_CLAMP_TO_EDGE                  0x812F
#define GLD3D_TEXTURE_MIN_LOD                0x813A
#define GLD3D_TEXTURE_MAX_LOD                0x813B
#define GLD3D_TEXTURE_BASE_LEVEL             0x813C
#define GLD3D_TEXTURE_MAX_LEVEL              0x813D
#define GLD3D_LIGHT_MODEL_COLOR_CONTROL      0x81F8
#define GLD3D_SINGLE_COLOR                   0x81F9
#define GLD3D_SEPARATE_SPECULAR_COLOR        0x81FA
#define GLD3D_SMOOTH_POINT_SIZE_RANGE        0x0B12
#define GLD3D_SMOOTH_POINT_SIZE_GRANULARITY  0x0B13
#define GLD3D_SMOOTH_LINE_WIDTH_RANGE        0x0B22
#define GLD3D_SMOOTH_LINE_WIDTH_GRANULARITY  0x0B23
#define GLD3D_ALIASED_POINT_SIZE_RANGE       0x846D
#define GLD3D_ALIASED_LINE_WIDTH_RANGE       0x846E

#define GLD3D_TEXTURE0                       0x84C0
#define GLD3D_TEXTURE1                       0x84C1
#define GLD3D_TEXTURE2                       0x84C2
#define GLD3D_TEXTURE3                       0x84C3
#define GLD3D_TEXTURE4                       0x84C4
#define GLD3D_TEXTURE5                       0x84C5
#define GLD3D_TEXTURE6                       0x84C6
#define GLD3D_TEXTURE7                       0x84C7
#define GLD3D_TEXTURE8                       0x84C8
#define GLD3D_TEXTURE9                       0x84C9
#define GLD3D_TEXTURE10                      0x84CA
#define GLD3D_TEXTURE11                      0x84CB
#define GLD3D_TEXTURE12                      0x84CC
#define GLD3D_TEXTURE13                      0x84CD
#define GLD3D_TEXTURE14                      0x84CE
#define GLD3D_TEXTURE15                      0x84CF
#define GLD3D_TEXTURE16                      0x84D0
#define GLD3D_TEXTURE17                      0x84D1
#define GLD3D_TEXTURE18                      0x84D2
#define GLD3D_TEXTURE19                      0x84D3
#define GLD3D_TEXTURE20                      0x84D4
#define GLD3D_TEXTURE21                      0x84D5
#define GLD3D_TEXTURE22                      0x84D6
#define GLD3D_TEXTURE23                      0x84D7
#define GLD3D_TEXTURE24                      0x84D8
#define GLD3D_TEXTURE25                      0x84D9
#define GLD3D_TEXTURE26                      0x84DA
#define GLD3D_TEXTURE27                      0x84DB
#define GLD3D_TEXTURE28                      0x84DC
#define GLD3D_TEXTURE29                      0x84DD
#define GLD3D_TEXTURE30                      0x84DE
#define GLD3D_TEXTURE31                      0x84DF
#define GLD3D_ACTIVE_TEXTURE                 0x84E0
#define GLD3D_CLIENT_ACTIVE_TEXTURE          0x84E1
#define GLD3D_MAX_TEXTURE_UNITS              0x84E2
#define GLD3D_TRANSPOSE_MODELVIEW_MATRIX     0x84E3
#define GLD3D_TRANSPOSE_PROJECTION_MATRIX    0x84E4
#define GLD3D_TRANSPOSE_TEXTURE_MATRIX       0x84E5
#define GLD3D_TRANSPOSE_COLOR_MATRIX         0x84E6
#define GLD3D_MULTISAMPLE                    0x809D
#define GLD3D_SAMPLE_ALPHA_TO_COVERAGE       0x809E
#define GLD3D_SAMPLE_ALPHA_TO_ONE            0x809F
#define GLD3D_SAMPLE_COVERAGE                0x80A0
#define GLD3D_SAMPLE_BUFFERS                 0x80A8
#define GLD3D_SAMPLES                        0x80A9
#define GLD3D_SAMPLE_COVERAGE_VALUE          0x80AA
#define GLD3D_SAMPLE_COVERAGE_INVERT         0x80AB
#define GLD3D_MULTISAMPLE_BIT                0x20000000
#define GLD3D_NORMAL_MAP                     0x8511
#define GLD3D_REFLECTION_MAP                 0x8512
#define GLD3D_TEXTURE_CUBE_MAP               0x8513
#define GLD3D_TEXTURE_BINDING_CUBE_MAP       0x8514
#define GLD3D_TEXTURE_CUBE_MAP_POSITIVE_X    0x8515
#define GLD3D_TEXTURE_CUBE_MAP_NEGATIVE_X    0x8516
#define GLD3D_TEXTURE_CUBE_MAP_POSITIVE_Y    0x8517
#define GLD3D_TEXTURE_CUBE_MAP_NEGATIVE_Y    0x8518
#define GLD3D_TEXTURE_CUBE_MAP_POSITIVE_Z    0x8519
#define GLD3D_TEXTURE_CUBE_MAP_NEGATIVE_Z    0x851A
#define GLD3D_PROXY_TEXTURE_CUBE_MAP         0x851B
#define GLD3D_MAX_CUBE_MAP_TEXTURE_SIZE      0x851C
#define GLD3D_COMPRESSED_ALPHA               0x84E9
#define GLD3D_COMPRESSED_LUMINANCE           0x84EA
#define GLD3D_COMPRESSED_LUMINANCE_ALPHA     0x84EB
#define GLD3D_COMPRESSED_INTENSITY           0x84EC
#define GLD3D_COMPRESSED_RGB                 0x84ED
#define GLD3D_COMPRESSED_RGBA                0x84EE
#define GLD3D_TEXTURE_COMPRESSION_HINT       0x84EF
#define GLD3D_TEXTURE_COMPRESSED_IMAGE_SIZE  0x86A0
#define GLD3D_TEXTURE_COMPRESSED             0x86A1
#define GLD3D_NUM_COMPRESSED_TEXTURE_FORMATS 0x86A2
#define GLD3D_COMPRESSED_TEXTURE_FORMATS     0x86A3
#define GLD3D_CLAMP_TO_BORDER                0x812D
#define GLD3D_COMBINE                        0x8570
#define GLD3D_COMBINE_RGB                    0x8571
#define GLD3D_COMBINE_ALPHA                  0x8572
#define GLD3D_SOURCE0_RGB                    0x8580
#define GLD3D_SOURCE1_RGB                    0x8581
#define GLD3D_SOURCE2_RGB                    0x8582
#define GLD3D_SOURCE0_ALPHA                  0x8588
#define GLD3D_SOURCE1_ALPHA                  0x8589
#define GLD3D_SOURCE2_ALPHA                  0x858A
#define GLD3D_OPERAND0_RGB                   0x8590
#define GLD3D_OPERAND1_RGB                   0x8591
#define GLD3D_OPERAND2_RGB                   0x8592
#define GLD3D_OPERAND0_ALPHA                 0x8598
#define GLD3D_OPERAND1_ALPHA                 0x8599
#define GLD3D_OPERAND2_ALPHA                 0x859A
#define GLD3D_RGB_SCALE                      0x8573
#define GLD3D_ADD_SIGNED                     0x8574
#define GLD3D_INTERPOLATE                    0x8575
#define GLD3D_SUBTRACT                       0x84E7
#define GLD3D_CONSTANT                       0x8576
#define GLD3D_PRIMARY_COLOR                  0x8577
#define GLD3D_PREVIOUS                       0x8578
#define GLD3D_DOT3_RGB                       0x86AE
#define GLD3D_DOT3_RGBA                      0x86AF

#define GLD3D_BLEND_DST_RGB                  0x80C8
#define GLD3D_BLEND_SRC_RGB                  0x80C9
#define GLD3D_BLEND_DST_ALPHA                0x80CA
#define GLD3D_BLEND_SRC_ALPHA                0x80CB
#define GLD3D_POINT_SIZE_MIN                 0x8126
#define GLD3D_POINT_SIZE_MAX                 0x8127
#define GLD3D_POINT_FADE_THRESHOLD_SIZE      0x8128
#define GLD3D_POINT_DISTANCE_ATTENUATION     0x8129
#define GLD3D_GENERATE_MIPMAP                0x8191
#define GLD3D_GENERATE_MIPMAP_HINT           0x8192
#define GLD3D_DEPTH_COMPONENT16              0x81A5
#define GLD3D_DEPTH_COMPONENT24              0x81A6
#define GLD3D_DEPTH_COMPONENT32              0x81A7
#define GLD3D_MIRRORED_REPEAT                0x8370
#define GLD3D_FOG_COORDINATE_SOURCE          0x8450
#define GLD3D_FOG_COORDINATE                 0x8451
#define GLD3D_FRAGMENT_DEPTH                 0x8452
#define GLD3D_CURRENT_FOG_COORDINATE         0x8453
#define GLD3D_FOG_COORDINATE_ARRAY_TYPE      0x8454
#define GLD3D_FOG_COORDINATE_ARRAY_STRIDE    0x8455
#define GLD3D_FOG_COORDINATE_ARRAY_POINTER   0x8456
#define GLD3D_FOG_COORDINATE_ARRAY           0x8457
#define GLD3D_COLOR_SUM                      0x8458
#define GLD3D_CURRENT_SECONDARY_COLOR        0x8459
#define GLD3D_SECONDARY_COLOR_ARRAY_SIZE     0x845A
#define GLD3D_SECONDARY_COLOR_ARRAY_TYPE     0x845B
#define GLD3D_SECONDARY_COLOR_ARRAY_STRIDE   0x845C
#define GLD3D_SECONDARY_COLOR_ARRAY_POINTER  0x845D
#define GLD3D_SECONDARY_COLOR_ARRAY          0x845E
#define GLD3D_MAX_TEXTURE_LOD_BIAS           0x84FD
#define GLD3D_TEXTURE_FILTER_CONTROL         0x8500
#define GLD3D_TEXTURE_LOD_BIAS               0x8501
#define GLD3D_INCR_WRAP                      0x8507
#define GLD3D_DECR_WRAP                      0x8508
#define GLD3D_TEXTURE_DEPTH_SIZE             0x884A
#define GLD3D_DEPTH_TEXTURE_MODE             0x884B
#define GLD3D_TEXTURE_COMPARE_MODE           0x884C
#define GLD3D_TEXTURE_COMPARE_FUNC           0x884D
#define GLD3D_COMPARE_R_TO_TEXTURE           0x884E

#define GLD3D_BUFFER_SIZE                    0x8764
#define GLD3D_BUFFER_USAGE                   0x8765
#define GLD3D_QUERY_COUNTER_BITS             0x8864
#define GLD3D_CURRENT_QUERY                  0x8865
#define GLD3D_QUERY_RESULT                   0x8866
#define GLD3D_QUERY_RESULT_AVAILABLE         0x8867
#define GLD3D_ARRAY_BUFFER                   0x8892
#define GLD3D_ELEMENT_ARRAY_BUFFER           0x8893
#define GLD3D_ARRAY_BUFFER_BINDING           0x8894
#define GLD3D_ELEMENT_ARRAY_BUFFER_BINDING   0x8895
#define GLD3D_VERTEX_ARRAY_BUFFER_BINDING    0x8896
#define GLD3D_NORMAL_ARRAY_BUFFER_BINDING    0x8897
#define GLD3D_COLOR_ARRAY_BUFFER_BINDING     0x8898
#define GLD3D_INDEX_ARRAY_BUFFER_BINDING     0x8899
#define GLD3D_TEXTURE_COORD_ARRAY_BUFFER_BINDING 0x889A
#define GLD3D_EDGE_FLAG_ARRAY_BUFFER_BINDING 0x889B
#define GLD3D_SECONDARY_COLOR_ARRAY_BUFFER_BINDING 0x889C
#define GLD3D_FOG_COORDINATE_ARRAY_BUFFER_BINDING 0x889D
#define GLD3D_WEIGHT_ARRAY_BUFFER_BINDING    0x889E
#define GLD3D_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING 0x889F
#define GLD3D_READ_ONLY                      0x88B8
#define GLD3D_WRITE_ONLY                     0x88B9
#define GLD3D_READ_WRITE                     0x88BA
#define GLD3D_BUFFER_ACCESS                  0x88BB
#define GLD3D_BUFFER_MAPPED                  0x88BC
#define GLD3D_BUFFER_MAP_POINTER             0x88BD
#define GLD3D_STREAM_DRAW                    0x88E0
#define GLD3D_STREAM_READ                    0x88E1
#define GLD3D_STREAM_COPY                    0x88E2
#define GLD3D_STATIC_DRAW                    0x88E4
#define GLD3D_STATIC_READ                    0x88E5
#define GLD3D_STATIC_COPY                    0x88E6
#define GLD3D_DYNAMIC_DRAW                   0x88E8
#define GLD3D_DYNAMIC_READ                   0x88E9
#define GLD3D_DYNAMIC_COPY                   0x88EA
#define GLD3D_SAMPLES_PASSED                 0x8914
#define GLD3D_FOG_COORD_SRC                  GLD3D_FOG_COORDINATE_SOURCE
#define GLD3D_FOG_COORD                      GLD3D_FOG_COORDINATE
#define GLD3D_CURRENT_FOG_COORD              GLD3D_CURRENT_FOG_COORDINATE
#define GLD3D_FOG_COORD_ARRAY_TYPE           GLD3D_FOG_COORDINATE_ARRAY_TYPE
#define GLD3D_FOG_COORD_ARRAY_STRIDE         GLD3D_FOG_COORDINATE_ARRAY_STRIDE
#define GLD3D_FOG_COORD_ARRAY_POINTER        GLD3D_FOG_COORDINATE_ARRAY_POINTER
#define GLD3D_FOG_COORD_ARRAY                GLD3D_FOG_COORDINATE_ARRAY
#define GLD3D_FOG_COORD_ARRAY_BUFFER_BINDING GLD3D_FOG_COORDINATE_ARRAY_BUFFER_BINDING
#define GLD3D_SRC0_RGB                       GLD3D_SOURCE0_RGB
#define GLD3D_SRC1_RGB                       GLD3D_SOURCE1_RGB
#define GLD3D_SRC2_RGB                       GLD3D_SOURCE2_RGB
#define GLD3D_SRC0_ALPHA                     GLD3D_SOURCE0_ALPHA
#define GLD3D_SRC1_ALPHA                     GLD3D_SOURCE1_ALPHA
#define GLD3D_SRC2_ALPHA                     GLD3D_SOURCE2_ALPHA

#define GLD3D_VERTEX_ATTRIB_ARRAY_ENABLED    0x8622
#define GLD3D_VERTEX_ATTRIB_ARRAY_SIZE       0x8623
#define GLD3D_VERTEX_ATTRIB_ARRAY_STRIDE     0x8624
#define GLD3D_VERTEX_ATTRIB_ARRAY_TYPE       0x8625
#define GLD3D_CURRENT_VERTEX_ATTRIB          0x8626
#define GLD3D_VERTEX_PROGRAM_POINT_SIZE      0x8642
#define GLD3D_VERTEX_PROGRAM_TWO_SIDE        0x8643
#define GLD3D_VERTEX_ATTRIB_ARRAY_POINTER    0x8645
#define GLD3D_STENCIL_BACK_FUNC              0x8800
#define GLD3D_STENCIL_BACK_FAIL              0x8801
#define GLD3D_STENCIL_BACK_PASS_DEPTH_FAIL   0x8802
#define GLD3D_STENCIL_BACK_PASS_DEPTH_PASS   0x8803
#define GLD3D_MAX_DRAW_BUFFERS               0x8824
#define GLD3D_DRAW_BUFFER0                   0x8825
#define GLD3D_DRAW_BUFFER1                   0x8826
#define GLD3D_DRAW_BUFFER2                   0x8827
#define GLD3D_DRAW_BUFFER3                   0x8828
#define GLD3D_DRAW_BUFFER4                   0x8829
#define GLD3D_DRAW_BUFFER5                   0x882A
#define GLD3D_DRAW_BUFFER6                   0x882B
#define GLD3D_DRAW_BUFFER7                   0x882C
#define GLD3D_DRAW_BUFFER8                   0x882D
#define GLD3D_DRAW_BUFFER9                   0x882E
#define GLD3D_DRAW_BUFFER10                  0x882F
#define GLD3D_DRAW_BUFFER11                  0x8830
#define GLD3D_DRAW_BUFFER12                  0x8831
#define GLD3D_DRAW_BUFFER13                  0x8832
#define GLD3D_DRAW_BUFFER14                  0x8833
#define GLD3D_DRAW_BUFFER15                  0x8834
#define GLD3D_BLEND_EQUATION_ALPHA           0x883D
#define GLD3D_POINT_SPRITE                   0x8861
#define GLD3D_COORD_REPLACE                  0x8862
#define GLD3D_MAX_VERTEX_ATTRIBS             0x8869
#define GLD3D_VERTEX_ATTRIB_ARRAY_NORMALIZED 0x886A
#define GLD3D_MAX_TEXTURE_COORDS             0x8871
#define GLD3D_MAX_TEXTURE_IMAGE_UNITS        0x8872
#define GLD3D_FRAGMENT_SHADER                0x8B30
#define GLD3D_VERTEX_SHADER                  0x8B31
#define GLD3D_MAX_FRAGMENT_UNIFORM_COMPONENTS 0x8B49
#define GLD3D_MAX_VERTEX_UNIFORM_COMPONENTS  0x8B4A
#define GLD3D_MAX_VARYING_FLOATS             0x8B4B
#define GLD3D_MAX_VERTEX_TEXTURE_IMAGE_UNITS 0x8B4C
#define GLD3D_MAX_COMBINED_TEXTURE_IMAGE_UNITS 0x8B4D
#define GLD3D_SHADER_TYPE                    0x8B4F
#define GLD3D_FLOAT_VEC2                     0x8B50
#define GLD3D_FLOAT_VEC3                     0x8B51
#define GLD3D_FLOAT_VEC4                     0x8B52
#define GLD3D_INT_VEC2                       0x8B53
#define GLD3D_INT_VEC3                       0x8B54
#define GLD3D_INT_VEC4                       0x8B55
#define GLD3D_BOOL                           0x8B56
#define GLD3D_BOOL_VEC2                      0x8B57
#define GLD3D_BOOL_VEC3                      0x8B58
#define GLD3D_BOOL_VEC4                      0x8B59
#define GLD3D_FLOAT_MAT2                     0x8B5A
#define GLD3D_FLOAT_MAT3                     0x8B5B
#define GLD3D_FLOAT_MAT4                     0x8B5C
#define GLD3D_SAMPLER_1D                     0x8B5D
#define GLD3D_SAMPLER_2D                     0x8B5E
#define GLD3D_SAMPLER_3D                     0x8B5F
#define GLD3D_SAMPLER_CUBE                   0x8B60
#define GLD3D_SAMPLER_1D_SHADOW              0x8B61
#define GLD3D_SAMPLER_2D_SHADOW              0x8B62
#define GLD3D_DELETE_STATUS                  0x8B80
#define GLD3D_COMPILE_STATUS                 0x8B81
#define GLD3D_LINK_STATUS                    0x8B82
#define GLD3D_VALIDATE_STATUS                0x8B83
#define GLD3D_INFO_LOG_LENGTH                0x8B84
#define GLD3D_ATTACHED_SHADERS               0x8B85
#define GLD3D_ACTIVE_UNIFORMS                0x8B86
#define GLD3D_ACTIVE_UNIFORM_MAX_LENGTH      0x8B87
#define GLD3D_SHADER_SOURCE_LENGTH           0x8B88
#define GLD3D_ACTIVE_ATTRIBUTES              0x8B89
#define GLD3D_ACTIVE_ATTRIBUTE_MAX_LENGTH    0x8B8A
#define GLD3D_FRAGMENT_SHADER_DERIVATIVE_HINT 0x8B8B
#define GLD3D_SHADING_LANGUAGE_VERSION       0x8B8C
#define GLD3D_CURRENT_PROGRAM                0x8B8D
#define GLD3D_POINT_SPRITE_COORD_ORIGIN      0x8CA0
#define GLD3D_LOWER_LEFT                     0x8CA1
#define GLD3D_UPPER_LEFT                     0x8CA2
#define GLD3D_STENCIL_BACK_REF               0x8CA3
#define GLD3D_STENCIL_BACK_VALUE_MASK        0x8CA4
#define GLD3D_STENCIL_BACK_WRITEMASK         0x8CA5

#define GLD3D_TEXTURE0_ARB                   0x84C0
#define GLD3D_TEXTURE1_ARB                   0x84C1
#define GLD3D_TEXTURE2_ARB                   0x84C2
#define GLD3D_TEXTURE3_ARB                   0x84C3
#define GLD3D_TEXTURE4_ARB                   0x84C4
#define GLD3D_TEXTURE5_ARB                   0x84C5
#define GLD3D_TEXTURE6_ARB                   0x84C6
#define GLD3D_TEXTURE7_ARB                   0x84C7
#define GLD3D_TEXTURE8_ARB                   0x84C8
#define GLD3D_TEXTURE9_ARB                   0x84C9
#define GLD3D_TEXTURE10_ARB                  0x84CA
#define GLD3D_TEXTURE11_ARB                  0x84CB
#define GLD3D_TEXTURE12_ARB                  0x84CC
#define GLD3D_TEXTURE13_ARB                  0x84CD
#define GLD3D_TEXTURE14_ARB                  0x84CE
#define GLD3D_TEXTURE15_ARB                  0x84CF
#define GLD3D_TEXTURE16_ARB                  0x84D0
#define GLD3D_TEXTURE17_ARB                  0x84D1
#define GLD3D_TEXTURE18_ARB                  0x84D2
#define GLD3D_TEXTURE19_ARB                  0x84D3
#define GLD3D_TEXTURE20_ARB                  0x84D4
#define GLD3D_TEXTURE21_ARB                  0x84D5
#define GLD3D_TEXTURE22_ARB                  0x84D6
#define GLD3D_TEXTURE23_ARB                  0x84D7
#define GLD3D_TEXTURE24_ARB                  0x84D8
#define GLD3D_TEXTURE25_ARB                  0x84D9
#define GLD3D_TEXTURE26_ARB                  0x84DA
#define GLD3D_TEXTURE27_ARB                  0x84DB
#define GLD3D_TEXTURE28_ARB                  0x84DC
#define GLD3D_TEXTURE29_ARB                  0x84DD
#define GLD3D_TEXTURE30_ARB                  0x84DE
#define GLD3D_TEXTURE31_ARB                  0x84DF
#define GLD3D_ACTIVE_TEXTURE_ARB             0x84E0
#define GLD3D_CLIENT_ACTIVE_TEXTURE_ARB      0x84E1
#define GLD3D_MAX_TEXTURE_UNITS_ARB          0x84E2

#define GLD3D_TRANSPOSE_MODELVIEW_MATRIX_ARB 0x84E3
#define GLD3D_TRANSPOSE_PROJECTION_MATRIX_ARB 0x84E4
#define GLD3D_TRANSPOSE_TEXTURE_MATRIX_ARB   0x84E5
#define GLD3D_TRANSPOSE_COLOR_MATRIX_ARB     0x84E6

#define GLD3D_MULTISAMPLE_ARB                0x809D
#define GLD3D_SAMPLE_ALPHA_TO_COVERAGE_ARB   0x809E
#define GLD3D_SAMPLE_ALPHA_TO_ONE_ARB        0x809F
#define GLD3D_SAMPLE_COVERAGE_ARB            0x80A0
#define GLD3D_SAMPLE_BUFFERS_ARB             0x80A8
#define GLD3D_SAMPLES_ARB                    0x80A9
#define GLD3D_SAMPLE_COVERAGE_VALUE_ARB      0x80AA
#define GLD3D_SAMPLE_COVERAGE_INVERT_ARB     0x80AB
#define GLD3D_MULTISAMPLE_BIT_ARB            0x20000000

#define GLD3D_NORMAL_MAP_ARB                 0x8511
#define GLD3D_REFLECTION_MAP_ARB             0x8512
#define GLD3D_TEXTURE_CUBE_MAP_ARB           0x8513
#define GLD3D_TEXTURE_BINDING_CUBE_MAP_ARB   0x8514
#define GLD3D_TEXTURE_CUBE_MAP_POSITIVE_X_ARB 0x8515
#define GLD3D_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB 0x8516
#define GLD3D_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB 0x8517
#define GLD3D_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB 0x8518
#define GLD3D_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB 0x8519
#define GLD3D_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB 0x851A
#define GLD3D_PROXY_TEXTURE_CUBE_MAP_ARB     0x851B
#define GLD3D_MAX_CUBE_MAP_TEXTURE_SIZE_ARB  0x851C

#define GLD3D_COMPRESSED_ALPHA_ARB           0x84E9
#define GLD3D_COMPRESSED_LUMINANCE_ARB       0x84EA
#define GLD3D_COMPRESSED_LUMINANCE_ALPHA_ARB 0x84EB
#define GLD3D_COMPRESSED_INTENSITY_ARB       0x84EC
#define GLD3D_COMPRESSED_RGB_ARB             0x84ED
#define GLD3D_COMPRESSED_RGBA_ARB            0x84EE
#define GLD3D_TEXTURE_COMPRESSION_HINT_ARB   0x84EF
#define GLD3D_TEXTURE_COMPRESSED_IMAGE_SIZE_ARB 0x86A0
#define GLD3D_TEXTURE_COMPRESSED_ARB         0x86A1
#define GLD3D_NUM_COMPRESSED_TEXTURE_FORMATS_ARB 0x86A2
#define GLD3D_COMPRESSED_TEXTURE_FORMATS_ARB 0x86A3

#define GLD3D_CLAMP_TO_BORDER_ARB            0x812D

#define GLD3D_POINT_SIZE_MIN_ARB             0x8126
#define GLD3D_POINT_SIZE_MAX_ARB             0x8127
#define GLD3D_POINT_FADE_THRESHOLD_SIZE_ARB  0x8128
#define GLD3D_POINT_DISTANCE_ATTENUATION_ARB 0x8129

#define GLD3D_MAX_VERTEX_UNITS_ARB           0x86A4
#define GLD3D_ACTIVE_VERTEX_UNITS_ARB        0x86A5
#define GLD3D_WEIGHT_SUM_UNITY_ARB           0x86A6
#define GLD3D_VERTEX_BLEND_ARB               0x86A7
#define GLD3D_CURRENT_WEIGHT_ARB             0x86A8
#define GLD3D_WEIGHT_ARRAY_TYPE_ARB          0x86A9
#define GLD3D_WEIGHT_ARRAY_STRIDE_ARB        0x86AA
#define GLD3D_WEIGHT_ARRAY_SIZE_ARB          0x86AB
#define GLD3D_WEIGHT_ARRAY_POINTER_ARB       0x86AC
#define GLD3D_WEIGHT_ARRAY_ARB               0x86AD
#define GLD3D_MODELVIEW0_ARB                 0x1700
#define GLD3D_MODELVIEW1_ARB                 0x850A
#define GLD3D_MODELVIEW2_ARB                 0x8722
#define GLD3D_MODELVIEW3_ARB                 0x8723
#define GLD3D_MODELVIEW4_ARB                 0x8724
#define GLD3D_MODELVIEW5_ARB                 0x8725
#define GLD3D_MODELVIEW6_ARB                 0x8726
#define GLD3D_MODELVIEW7_ARB                 0x8727
#define GLD3D_MODELVIEW8_ARB                 0x8728
#define GLD3D_MODELVIEW9_ARB                 0x8729
#define GLD3D_MODELVIEW10_ARB                0x872A
#define GLD3D_MODELVIEW11_ARB                0x872B
#define GLD3D_MODELVIEW12_ARB                0x872C
#define GLD3D_MODELVIEW13_ARB                0x872D
#define GLD3D_MODELVIEW14_ARB                0x872E
#define GLD3D_MODELVIEW15_ARB                0x872F
#define GLD3D_MODELVIEW16_ARB                0x8730
#define GLD3D_MODELVIEW17_ARB                0x8731
#define GLD3D_MODELVIEW18_ARB                0x8732
#define GLD3D_MODELVIEW19_ARB                0x8733
#define GLD3D_MODELVIEW20_ARB                0x8734
#define GLD3D_MODELVIEW21_ARB                0x8735
#define GLD3D_MODELVIEW22_ARB                0x8736
#define GLD3D_MODELVIEW23_ARB                0x8737
#define GLD3D_MODELVIEW24_ARB                0x8738
#define GLD3D_MODELVIEW25_ARB                0x8739
#define GLD3D_MODELVIEW26_ARB                0x873A
#define GLD3D_MODELVIEW27_ARB                0x873B
#define GLD3D_MODELVIEW28_ARB                0x873C
#define GLD3D_MODELVIEW29_ARB                0x873D
#define GLD3D_MODELVIEW30_ARB                0x873E
#define GLD3D_MODELVIEW31_ARB                0x873F

#define GLD3D_MATRIX_PALETTE_ARB             0x8840
#define GLD3D_MAX_MATRIX_PALETTE_STACK_DEPTH_ARB 0x8841
#define GLD3D_MAX_PALETTE_MATRICES_ARB       0x8842
#define GLD3D_CURRENT_PALETTE_MATRIX_ARB     0x8843
#define GLD3D_MATRIX_INDEX_ARRAY_ARB         0x8844
#define GLD3D_CURRENT_MATRIX_INDEX_ARB       0x8845
#define GLD3D_MATRIX_INDEX_ARRAY_SIZE_ARB    0x8846
#define GLD3D_MATRIX_INDEX_ARRAY_TYPE_ARB    0x8847
#define GLD3D_MATRIX_INDEX_ARRAY_STRIDE_ARB  0x8848
#define GLD3D_MATRIX_INDEX_ARRAY_POINTER_ARB 0x8849

#define GLD3D_COMBINE_ARB                    0x8570
#define GLD3D_COMBINE_RGB_ARB                0x8571
#define GLD3D_COMBINE_ALPHA_ARB              0x8572
#define GLD3D_SOURCE0_RGB_ARB                0x8580
#define GLD3D_SOURCE1_RGB_ARB                0x8581
#define GLD3D_SOURCE2_RGB_ARB                0x8582
#define GLD3D_SOURCE0_ALPHA_ARB              0x8588
#define GLD3D_SOURCE1_ALPHA_ARB              0x8589
#define GLD3D_SOURCE2_ALPHA_ARB              0x858A
#define GLD3D_OPERAND0_RGB_ARB               0x8590
#define GLD3D_OPERAND1_RGB_ARB               0x8591
#define GLD3D_OPERAND2_RGB_ARB               0x8592
#define GLD3D_OPERAND0_ALPHA_ARB             0x8598
#define GLD3D_OPERAND1_ALPHA_ARB             0x8599
#define GLD3D_OPERAND2_ALPHA_ARB             0x859A
#define GLD3D_RGB_SCALE_ARB                  0x8573
#define GLD3D_ADD_SIGNED_ARB                 0x8574
#define GLD3D_INTERPOLATE_ARB                0x8575
#define GLD3D_SUBTRACT_ARB                   0x84E7
#define GLD3D_CONSTANT_ARB                   0x8576
#define GLD3D_PRIMARY_COLOR_ARB              0x8577
#define GLD3D_PREVIOUS_ARB                   0x8578

#define GLD3D_DOT3_RGB_ARB                   0x86AE
#define GLD3D_DOT3_RGBA_ARB                  0x86AF

#define GLD3D_MIRRORED_REPEAT_ARB            0x8370

#define GLD3D_DEPTH_COMPONENT16_ARB          0x81A5
#define GLD3D_DEPTH_COMPONENT24_ARB          0x81A6
#define GLD3D_DEPTH_COMPONENT32_ARB          0x81A7
#define GLD3D_TEXTURE_DEPTH_SIZE_ARB         0x884A
#define GLD3D_DEPTH_TEXTURE_MODE_ARB         0x884B

#define GLD3D_TEXTURE_COMPARE_MODE_ARB       0x884C
#define GLD3D_TEXTURE_COMPARE_FUNC_ARB       0x884D
#define GLD3D_COMPARE_R_TO_TEXTURE_ARB       0x884E

#define GLD3D_TEXTURE_COMPARE_FAIL_VALUE_ARB 0x80BF

#define GLD3D_COLOR_SUM_ARB                  0x8458
#define GLD3D_VERTEX_PROGRAM_ARB             0x8620
#define GLD3D_VERTEX_ATTRIB_ARRAY_ENABLED_ARB 0x8622
#define GLD3D_VERTEX_ATTRIB_ARRAY_SIZE_ARB   0x8623
#define GLD3D_VERTEX_ATTRIB_ARRAY_STRIDE_ARB 0x8624
#define GLD3D_VERTEX_ATTRIB_ARRAY_TYPE_ARB   0x8625
#define GLD3D_CURRENT_VERTEX_ATTRIB_ARB      0x8626
#define GLD3D_PROGRAM_LENGTH_ARB             0x8627
#define GLD3D_PROGRAM_STRING_ARB             0x8628
#define GLD3D_MAX_PROGRAM_MATRIX_STACK_DEPTH_ARB 0x862E
#define GLD3D_MAX_PROGRAM_MATRICES_ARB       0x862F
#define GLD3D_CURRENT_MATRIX_STACK_DEPTH_ARB 0x8640
#define GLD3D_CURRENT_MATRIX_ARB             0x8641
#define GLD3D_VERTEX_PROGRAM_POINT_SIZE_ARB  0x8642
#define GLD3D_VERTEX_PROGRAM_TWO_SIDE_ARB    0x8643
#define GLD3D_VERTEX_ATTRIB_ARRAY_POINTER_ARB 0x8645
#define GLD3D_PROGRAM_ERROR_POSITION_ARB     0x864B
#define GLD3D_PROGRAM_BINDING_ARB            0x8677
#define GLD3D_MAX_VERTEX_ATTRIBS_ARB         0x8869
#define GLD3D_VERTEX_ATTRIB_ARRAY_NORMALIZED_ARB 0x886A
#define GLD3D_PROGRAM_ERROR_STRING_ARB       0x8874
#define GLD3D_PROGRAM_FORMAT_ASCII_ARB       0x8875
#define GLD3D_PROGRAM_FORMAT_ARB             0x8876
#define GLD3D_PROGRAM_INSTRUCTIONS_ARB       0x88A0
#define GLD3D_MAX_PROGRAM_INSTRUCTIONS_ARB   0x88A1
#define GLD3D_PROGRAM_NATIVE_INSTRUCTIONS_ARB 0x88A2
#define GLD3D_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB 0x88A3
#define GLD3D_PROGRAM_TEMPORARIES_ARB        0x88A4
#define GLD3D_MAX_PROGRAM_TEMPORARIES_ARB    0x88A5
#define GLD3D_PROGRAM_NATIVE_TEMPORARIES_ARB 0x88A6
#define GLD3D_MAX_PROGRAM_NATIVE_TEMPORARIES_ARB 0x88A7
#define GLD3D_PROGRAM_PARAMETERS_ARB         0x88A8
#define GLD3D_MAX_PROGRAM_PARAMETERS_ARB     0x88A9
#define GLD3D_PROGRAM_NATIVE_PARAMETERS_ARB  0x88AA
#define GLD3D_MAX_PROGRAM_NATIVE_PARAMETERS_ARB 0x88AB
#define GLD3D_PROGRAM_ATTRIBS_ARB            0x88AC
#define GLD3D_MAX_PROGRAM_ATTRIBS_ARB        0x88AD
#define GLD3D_PROGRAM_NATIVE_ATTRIBS_ARB     0x88AE
#define GLD3D_MAX_PROGRAM_NATIVE_ATTRIBS_ARB 0x88AF
#define GLD3D_PROGRAM_ADDRESS_REGISTERS_ARB  0x88B0
#define GLD3D_MAX_PROGRAM_ADDRESS_REGISTERS_ARB 0x88B1
#define GLD3D_PROGRAM_NATIVE_ADDRESS_REGISTERS_ARB 0x88B2
#define GLD3D_MAX_PROGRAM_NATIVE_ADDRESS_REGISTERS_ARB 0x88B3
#define GLD3D_MAX_PROGRAM_LOCAL_PARAMETERS_ARB 0x88B4
#define GLD3D_MAX_PROGRAM_ENV_PARAMETERS_ARB 0x88B5
#define GLD3D_PROGRAM_UNDER_NATIVE_LIMITS_ARB 0x88B6
#define GLD3D_TRANSPOSE_CURRENT_MATRIX_ARB   0x88B7
#define GLD3D_MATRIX0_ARB                    0x88C0
#define GLD3D_MATRIX1_ARB                    0x88C1
#define GLD3D_MATRIX2_ARB                    0x88C2
#define GLD3D_MATRIX3_ARB                    0x88C3
#define GLD3D_MATRIX4_ARB                    0x88C4
#define GLD3D_MATRIX5_ARB                    0x88C5
#define GLD3D_MATRIX6_ARB                    0x88C6
#define GLD3D_MATRIX7_ARB                    0x88C7
#define GLD3D_MATRIX8_ARB                    0x88C8
#define GLD3D_MATRIX9_ARB                    0x88C9
#define GLD3D_MATRIX10_ARB                   0x88CA
#define GLD3D_MATRIX11_ARB                   0x88CB
#define GLD3D_MATRIX12_ARB                   0x88CC
#define GLD3D_MATRIX13_ARB                   0x88CD
#define GLD3D_MATRIX14_ARB                   0x88CE
#define GLD3D_MATRIX15_ARB                   0x88CF
#define GLD3D_MATRIX16_ARB                   0x88D0
#define GLD3D_MATRIX17_ARB                   0x88D1
#define GLD3D_MATRIX18_ARB                   0x88D2
#define GLD3D_MATRIX19_ARB                   0x88D3
#define GLD3D_MATRIX20_ARB                   0x88D4
#define GLD3D_MATRIX21_ARB                   0x88D5
#define GLD3D_MATRIX22_ARB                   0x88D6
#define GLD3D_MATRIX23_ARB                   0x88D7
#define GLD3D_MATRIX24_ARB                   0x88D8
#define GLD3D_MATRIX25_ARB                   0x88D9
#define GLD3D_MATRIX26_ARB                   0x88DA
#define GLD3D_MATRIX27_ARB                   0x88DB
#define GLD3D_MATRIX28_ARB                   0x88DC
#define GLD3D_MATRIX29_ARB                   0x88DD
#define GLD3D_MATRIX30_ARB                   0x88DE
#define GLD3D_MATRIX31_ARB                   0x88DF

#define GLD3D_FRAGMENT_PROGRAM_ARB           0x8804
#define GLD3D_PROGRAM_ALU_INSTRUCTIONS_ARB   0x8805
#define GLD3D_PROGRAM_TEX_INSTRUCTIONS_ARB   0x8806
#define GLD3D_PROGRAM_TEX_INDIRECTIONS_ARB   0x8807
#define GLD3D_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB 0x8808
#define GLD3D_PROGRAM_NATIVE_TEX_INSTRUCTIONS_ARB 0x8809
#define GLD3D_PROGRAM_NATIVE_TEX_INDIRECTIONS_ARB 0x880A
#define GLD3D_MAX_PROGRAM_ALU_INSTRUCTIONS_ARB 0x880B
#define GLD3D_MAX_PROGRAM_TEX_INSTRUCTIONS_ARB 0x880C
#define GLD3D_MAX_PROGRAM_TEX_INDIRECTIONS_ARB 0x880D
#define GLD3D_MAX_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB 0x880E
#define GLD3D_MAX_PROGRAM_NATIVE_TEX_INSTRUCTIONS_ARB 0x880F
#define GLD3D_MAX_PROGRAM_NATIVE_TEX_INDIRECTIONS_ARB 0x8810
#define GLD3D_MAX_TEXTURE_COORDS_ARB         0x8871
#define GLD3D_MAX_TEXTURE_IMAGE_UNITS_ARB    0x8872

#define GLD3D_BUFFER_SIZE_ARB                0x8764
#define GLD3D_BUFFER_USAGE_ARB               0x8765
#define GLD3D_ARRAY_BUFFER_ARB               0x8892
#define GLD3D_ELEMENT_ARRAY_BUFFER_ARB       0x8893
#define GLD3D_ARRAY_BUFFER_BINDING_ARB       0x8894
#define GLD3D_ELEMENT_ARRAY_BUFFER_BINDING_ARB 0x8895
#define GLD3D_VERTEX_ARRAY_BUFFER_BINDING_ARB 0x8896
#define GLD3D_NORMAL_ARRAY_BUFFER_BINDING_ARB 0x8897
#define GLD3D_COLOR_ARRAY_BUFFER_BINDING_ARB 0x8898
#define GLD3D_INDEX_ARRAY_BUFFER_BINDING_ARB 0x8899
#define GLD3D_TEXTURE_COORD_ARRAY_BUFFER_BINDING_ARB 0x889A
#define GLD3D_EDGE_FLAG_ARRAY_BUFFER_BINDING_ARB 0x889B
#define GLD3D_SECONDARY_COLOR_ARRAY_BUFFER_BINDING_ARB 0x889C
#define GLD3D_FOG_COORDINATE_ARRAY_BUFFER_BINDING_ARB 0x889D
#define GLD3D_WEIGHT_ARRAY_BUFFER_BINDING_ARB 0x889E
#define GLD3D_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING_ARB 0x889F
#define GLD3D_READ_ONLY_ARB                  0x88B8
#define GLD3D_WRITE_ONLY_ARB                 0x88B9
#define GLD3D_READ_WRITE_ARB                 0x88BA
#define GLD3D_BUFFER_ACCESS_ARB              0x88BB
#define GLD3D_BUFFER_MAPPED_ARB              0x88BC
#define GLD3D_BUFFER_MAP_POINTER_ARB         0x88BD
#define GLD3D_STREAM_DRAW_ARB                0x88E0
#define GLD3D_STREAM_READ_ARB                0x88E1
#define GLD3D_STREAM_COPY_ARB                0x88E2
#define GLD3D_STATIC_DRAW_ARB                0x88E4
#define GLD3D_STATIC_READ_ARB                0x88E5
#define GLD3D_STATIC_COPY_ARB                0x88E6
#define GLD3D_DYNAMIC_DRAW_ARB               0x88E8
#define GLD3D_DYNAMIC_READ_ARB               0x88E9
#define GLD3D_DYNAMIC_COPY_ARB               0x88EA

#define GLD3D_QUERY_COUNTER_BITS_ARB         0x8864
#define GLD3D_CURRENT_QUERY_ARB              0x8865
#define GLD3D_QUERY_RESULT_ARB               0x8866
#define GLD3D_QUERY_RESULT_AVAILABLE_ARB     0x8867
#define GLD3D_SAMPLES_PASSED_ARB             0x8914

#define GLD3D_PROGRAM_OBJECT_ARB             0x8B40
#define GLD3D_SHADER_OBJECT_ARB              0x8B48
#define GLD3D_OBJECT_TYPE_ARB                0x8B4E
#define GLD3D_OBJECT_SUBTYPE_ARB             0x8B4F
#define GLD3D_FLOAT_VEC2_ARB                 0x8B50
#define GLD3D_FLOAT_VEC3_ARB                 0x8B51
#define GLD3D_FLOAT_VEC4_ARB                 0x8B52
#define GLD3D_INT_VEC2_ARB                   0x8B53
#define GLD3D_INT_VEC3_ARB                   0x8B54
#define GLD3D_INT_VEC4_ARB                   0x8B55
#define GLD3D_BOOL_ARB                       0x8B56
#define GLD3D_BOOL_VEC2_ARB                  0x8B57
#define GLD3D_BOOL_VEC3_ARB                  0x8B58
#define GLD3D_BOOL_VEC4_ARB                  0x8B59
#define GLD3D_FLOAT_MAT2_ARB                 0x8B5A
#define GLD3D_FLOAT_MAT3_ARB                 0x8B5B
#define GLD3D_FLOAT_MAT4_ARB                 0x8B5C
#define GLD3D_SAMPLER_1D_ARB                 0x8B5D
#define GLD3D_SAMPLER_2D_ARB                 0x8B5E
#define GLD3D_SAMPLER_3D_ARB                 0x8B5F
#define GLD3D_SAMPLER_CUBE_ARB               0x8B60
#define GLD3D_SAMPLER_1D_SHADOW_ARB          0x8B61
#define GLD3D_SAMPLER_2D_SHADOW_ARB          0x8B62
#define GLD3D_SAMPLER_2D_RECT_ARB            0x8B63
#define GLD3D_SAMPLER_2D_RECT_SHADOW_ARB     0x8B64
#define GLD3D_OBJECT_DELETE_STATUS_ARB       0x8B80
#define GLD3D_OBJECT_COMPILE_STATUS_ARB      0x8B81
#define GLD3D_OBJECT_LINK_STATUS_ARB         0x8B82
#define GLD3D_OBJECT_VALIDATE_STATUS_ARB     0x8B83
#define GLD3D_OBJECT_INFO_LOG_LENGTH_ARB     0x8B84
#define GLD3D_OBJECT_ATTACHED_OBJECTS_ARB    0x8B85
#define GLD3D_OBJECT_ACTIVE_UNIFORMS_ARB     0x8B86
#define GLD3D_OBJECT_ACTIVE_UNIFORM_MAX_LENGTH_ARB 0x8B87
#define GLD3D_OBJECT_SHADER_SOURCE_LENGTH_ARB 0x8B88

#define GLD3D_VERTEX_SHADER_ARB              0x8B31
#define GLD3D_MAX_VERTEX_UNIFORM_COMPONENTS_ARB 0x8B4A
#define GLD3D_MAX_VARYING_FLOATS_ARB         0x8B4B
#define GLD3D_MAX_VERTEX_TEXTURE_IMAGE_UNITS_ARB 0x8B4C
#define GLD3D_MAX_COMBINED_TEXTURE_IMAGE_UNITS_ARB 0x8B4D
#define GLD3D_OBJECT_ACTIVE_ATTRIBUTES_ARB   0x8B89
#define GLD3D_OBJECT_ACTIVE_ATTRIBUTE_MAX_LENGTH_ARB 0x8B8A

#define GLD3D_FRAGMENT_SHADER_ARB            0x8B30
#define GLD3D_MAX_FRAGMENT_UNIFORM_COMPONENTS_ARB 0x8B49
#define GLD3D_FRAGMENT_SHADER_DERIVATIVE_HINT_ARB 0x8B8B

#define GLD3D_SHADING_LANGUAGE_VERSION_ARB   0x8B8C

#define GLD3D_POINT_SPRITE_ARB               0x8861
#define GLD3D_COORD_REPLACE_ARB              0x8862

#define GLD3D_MAX_DRAW_BUFFERS_ARB           0x8824
#define GLD3D_DRAW_BUFFER0_ARB               0x8825
#define GLD3D_DRAW_BUFFER1_ARB               0x8826
#define GLD3D_DRAW_BUFFER2_ARB               0x8827
#define GLD3D_DRAW_BUFFER3_ARB               0x8828
#define GLD3D_DRAW_BUFFER4_ARB               0x8829
#define GLD3D_DRAW_BUFFER5_ARB               0x882A
#define GLD3D_DRAW_BUFFER6_ARB               0x882B
#define GLD3D_DRAW_BUFFER7_ARB               0x882C
#define GLD3D_DRAW_BUFFER8_ARB               0x882D
#define GLD3D_DRAW_BUFFER9_ARB               0x882E
#define GLD3D_DRAW_BUFFER10_ARB              0x882F
#define GLD3D_DRAW_BUFFER11_ARB              0x8830
#define GLD3D_DRAW_BUFFER12_ARB              0x8831
#define GLD3D_DRAW_BUFFER13_ARB              0x8832
#define GLD3D_DRAW_BUFFER14_ARB              0x8833
#define GLD3D_DRAW_BUFFER15_ARB              0x8834

#define GLD3D_TEXTURE_RECTANGLE_ARB          0x84F5
#define GLD3D_TEXTURE_BINDING_RECTANGLE_ARB  0x84F6
#define GLD3D_PROXY_TEXTURE_RECTANGLE_ARB    0x84F7
#define GLD3D_MAX_RECTANGLE_TEXTURE_SIZE_ARB 0x84F8

#define GLD3D_RGBA_FLOAT_MODE_ARB            0x8820
#define GLD3D_CLAMP_VERTEX_COLOR_ARB         0x891A
#define GLD3D_CLAMP_FRAGMENT_COLOR_ARB       0x891B
#define GLD3D_CLAMP_READ_COLOR_ARB           0x891C
#define GLD3D_FIXED_ONLY_ARB                 0x891D

#define GLD3D_HALF_FLOAT_ARB                 0x140B

#define GLD3D_TEXTURE_RED_TYPE_ARB           0x8C10
#define GLD3D_TEXTURE_GREEN_TYPE_ARB         0x8C11
#define GLD3D_TEXTURE_BLUE_TYPE_ARB          0x8C12
#define GLD3D_TEXTURE_ALPHA_TYPE_ARB         0x8C13
#define GLD3D_TEXTURE_LUMINANCE_TYPE_ARB     0x8C14
#define GLD3D_TEXTURE_INTENSITY_TYPE_ARB     0x8C15
#define GLD3D_TEXTURE_DEPTH_TYPE_ARB         0x8C16
#define GLD3D_UNSIGNED_NORMALIZED_ARB        0x8C17
#define GLD3D_RGBA32F_ARB                    0x8814
#define GLD3D_RGB32F_ARB                     0x8815
#define GLD3D_ALPHA32F_ARB                   0x8816
#define GLD3D_INTENSITY32F_ARB               0x8817
#define GLD3D_LUMINANCE32F_ARB               0x8818
#define GLD3D_LUMINANCE_ALPHA32F_ARB         0x8819
#define GLD3D_RGBA16F_ARB                    0x881A
#define GLD3D_RGB16F_ARB                     0x881B
#define GLD3D_ALPHA16F_ARB                   0x881C
#define GLD3D_INTENSITY16F_ARB               0x881D
#define GLD3D_LUMINANCE16F_ARB               0x881E
#define GLD3D_LUMINANCE_ALPHA16F_ARB         0x881F

#define GLD3D_PIXEL_PACK_BUFFER_ARB          0x88EB
#define GLD3D_PIXEL_UNPACK_BUFFER_ARB        0x88EC
#define GLD3D_PIXEL_PACK_BUFFER_BINDING_ARB  0x88ED
#define GLD3D_PIXEL_UNPACK_BUFFER_BINDING_ARB 0x88EF

#define GLD3D_ABGR_EXT                       0x8000

#define GLD3D_CONSTANT_COLOR_EXT             0x8001
#define GLD3D_ONE_MINUS_CONSTANT_COLOR_EXT   0x8002
#define GLD3D_CONSTANT_ALPHA_EXT             0x8003
#define GLD3D_ONE_MINUS_CONSTANT_ALPHA_EXT   0x8004
#define GLD3D_BLEND_COLOR_EXT                0x8005

#define GLD3D_POLYGON_OFFSET_EXT             0x8037
#define GLD3D_POLYGON_OFFSET_FACTOR_EXT      0x8038
#define GLD3D_POLYGON_OFFSET_BIAS_EXT        0x8039

#define GLD3D_ALPHA4_EXT                     0x803B
#define GLD3D_ALPHA8_EXT                     0x803C
#define GLD3D_ALPHA12_EXT                    0x803D
#define GLD3D_ALPHA16_EXT                    0x803E
#define GLD3D_LUMINANCE4_EXT                 0x803F
#define GLD3D_LUMINANCE8_EXT                 0x8040
#define GLD3D_LUMINANCE12_EXT                0x8041
#define GLD3D_LUMINANCE16_EXT                0x8042
#define GLD3D_LUMINANCE4_ALPHA4_EXT          0x8043
#define GLD3D_LUMINANCE6_ALPHA2_EXT          0x8044
#define GLD3D_LUMINANCE8_ALPHA8_EXT          0x8045
#define GLD3D_LUMINANCE12_ALPHA4_EXT         0x8046
#define GLD3D_LUMINANCE12_ALPHA12_EXT        0x8047
#define GLD3D_LUMINANCE16_ALPHA16_EXT        0x8048
#define GLD3D_INTENSITY_EXT                  0x8049
#define GLD3D_INTENSITY4_EXT                 0x804A
#define GLD3D_INTENSITY8_EXT                 0x804B
#define GLD3D_INTENSITY12_EXT                0x804C
#define GLD3D_INTENSITY16_EXT                0x804D
#define GLD3D_RGB2_EXT                       0x804E
#define GLD3D_RGB4_EXT                       0x804F
#define GLD3D_RGB5_EXT                       0x8050
#define GLD3D_RGB8_EXT                       0x8051
#define GLD3D_RGB10_EXT                      0x8052
#define GLD3D_RGB12_EXT                      0x8053
#define GLD3D_RGB16_EXT                      0x8054
#define GLD3D_RGBA2_EXT                      0x8055
#define GLD3D_RGBA4_EXT                      0x8056
#define GLD3D_RGB5_A1_EXT                    0x8057
#define GLD3D_RGBA8_EXT                      0x8058
#define GLD3D_RGB10_A2_EXT                   0x8059
#define GLD3D_RGBA12_EXT                     0x805A
#define GLD3D_RGBA16_EXT                     0x805B
#define GLD3D_TEXTURE_RED_SIZE_EXT           0x805C
#define GLD3D_TEXTURE_GREEN_SIZE_EXT         0x805D
#define GLD3D_TEXTURE_BLUE_SIZE_EXT          0x805E
#define GLD3D_TEXTURE_ALPHA_SIZE_EXT         0x805F
#define GLD3D_TEXTURE_LUMINANCE_SIZE_EXT     0x8060
#define GLD3D_TEXTURE_INTENSITY_SIZE_EXT     0x8061
#define GLD3D_REPLACE_EXT                    0x8062
#define GLD3D_PROXY_TEXTURE_1D_EXT           0x8063
#define GLD3D_PROXY_TEXTURE_2D_EXT           0x8064
#define GLD3D_TEXTURE_TOO_LARGE_EXT          0x8065

#define GLD3D_PACK_SKIP_IMAGES_EXT           0x806B
#define GLD3D_PACK_IMAGE_HEIGHT_EXT          0x806C
#define GLD3D_UNPACK_SKIP_IMAGES_EXT         0x806D
#define GLD3D_UNPACK_IMAGE_HEIGHT_EXT        0x806E
#define GLD3D_TEXTURE_3D_EXT                 0x806F
#define GLD3D_PROXY_TEXTURE_3D_EXT           0x8070
#define GLD3D_TEXTURE_DEPTH_EXT              0x8071
#define GLD3D_TEXTURE_WRAP_R_EXT             0x8072
#define GLD3D_MAX_3D_TEXTURE_SIZE_EXT        0x8073

#define GLD3D_FILTER4_SGIS                   0x8146
#define GLD3D_TEXTURE_FILTER4_SIZE_SGIS      0x8147

#define GLD3D_TEXTURE_PRIORITY_EXT           0x8066
#define GLD3D_TEXTURE_RESIDENT_EXT           0x8067
#define GLD3D_TEXTURE_1D_BINDING_EXT         0x8068
#define GLD3D_TEXTURE_2D_BINDING_EXT         0x8069
#define GLD3D_TEXTURE_3D_BINDING_EXT         0x806A

#define GLD3D_RESCALE_NORMAL_EXT             0x803A

#define GLD3D_VERTEX_ARRAY_EXT               0x8074
#define GLD3D_NORMAL_ARRAY_EXT               0x8075
#define GLD3D_COLOR_ARRAY_EXT                0x8076
#define GLD3D_INDEX_ARRAY_EXT                0x8077
#define GLD3D_TEXTURE_COORD_ARRAY_EXT        0x8078
#define GLD3D_EDGE_FLAG_ARRAY_EXT            0x8079
#define GLD3D_VERTEX_ARRAY_SIZE_EXT          0x807A
#define GLD3D_VERTEX_ARRAY_TYPE_EXT          0x807B
#define GLD3D_VERTEX_ARRAY_STRIDE_EXT        0x807C
#define GLD3D_VERTEX_ARRAY_COUNT_EXT         0x807D
#define GLD3D_NORMAL_ARRAY_TYPE_EXT          0x807E
#define GLD3D_NORMAL_ARRAY_STRIDE_EXT        0x807F
#define GLD3D_NORMAL_ARRAY_COUNT_EXT         0x8080
#define GLD3D_COLOR_ARRAY_SIZE_EXT           0x8081
#define GLD3D_COLOR_ARRAY_TYPE_EXT           0x8082
#define GLD3D_COLOR_ARRAY_STRIDE_EXT         0x8083
#define GLD3D_COLOR_ARRAY_COUNT_EXT          0x8084
#define GLD3D_INDEX_ARRAY_TYPE_EXT           0x8085
#define GLD3D_INDEX_ARRAY_STRIDE_EXT         0x8086
#define GLD3D_INDEX_ARRAY_COUNT_EXT          0x8087
#define GLD3D_TEXTURE_COORD_ARRAY_SIZE_EXT   0x8088
#define GLD3D_TEXTURE_COORD_ARRAY_TYPE_EXT   0x8089
#define GLD3D_TEXTURE_COORD_ARRAY_STRIDE_EXT 0x808A
#define GLD3D_TEXTURE_COORD_ARRAY_COUNT_EXT  0x808B
#define GLD3D_EDGE_FLAG_ARRAY_STRIDE_EXT     0x808C
#define GLD3D_EDGE_FLAG_ARRAY_COUNT_EXT      0x808D
#define GLD3D_VERTEX_ARRAY_POINTER_EXT       0x808E
#define GLD3D_NORMAL_ARRAY_POINTER_EXT       0x808F
#define GLD3D_COLOR_ARRAY_POINTER_EXT        0x8090
#define GLD3D_INDEX_ARRAY_POINTER_EXT        0x8091
#define GLD3D_TEXTURE_COORD_ARRAY_POINTER_EXT 0x8092
#define GLD3D_EDGE_FLAG_ARRAY_POINTER_EXT    0x8093

#define GLD3D_TEXTURE_MAX_ANISOTROPY_EXT     0x84FE
#define GLD3D_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
