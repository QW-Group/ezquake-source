/*
Copyright (C) 2018 ezQuake team

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

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"

void GLM_OrthographicProjection(float left, float right, float top, float bottom, float zNear, float zFar);
void GLM_SetMatrix(float* target, const float* source);

static qbool glc_pause_updates;
static GLfloat projectionMatrix[16];
static GLfloat modelMatrix[16];
//static GLfloat viewMatrix[16];
static GLfloat identityMatrix[16] = {
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 1
};

void GLM_SetIdentityMatrix(float* matrix)
{
	GLM_SetMatrix(matrix, identityMatrix);
}

// 
static const GLfloat* GL_OrthoMatrix(float left, float right, float top, float bottom, float zNear, float zFar)
{
	static GLfloat matrix[16];

	memset(matrix, 0, sizeof(matrix));
	matrix[0] = 2 / (right - left);
	matrix[5] = 2 / (top - bottom);
	matrix[10] = -2 / (zFar - zNear);
	matrix[12] = -(right + left) / (right - left);
	matrix[13] = -(top + bottom) / (top - bottom);
	matrix[14] = -(zFar + zNear) / (zFar - zNear);
	matrix[15] = 1;

	return matrix;
}

void GLM_OrthographicProjection(float left, float right, float top, float bottom, float zNear, float zFar)
{
	// Deliberately inverting top & bottom here...
	GLM_SetMatrix(projectionMatrix, GL_OrthoMatrix(left, right, bottom, top, zNear, zFar));
}

#ifdef WITH_OPENGL_TRACE
static const char* NameForMatrix(GLenum mode)
{
	if (mode == GL_MODELVIEW || mode == GL_MODELVIEW_MATRIX) {
		return "modelview";
	}
	if (mode == GL_PROJECTION || mode == GL_PROJECTION_MATRIX) {
		return "projection";
	}
	return "(other)";
}
#endif

float* GL_MatrixForMode(GLenum type)
{
	static float junk[16] = { 0 };

	if (type == GL_PROJECTION || type == GL_PROJECTION_MATRIX) {
		return projectionMatrix;
	}
	else if (type == GL_MODELVIEW || type == GL_MODELVIEW_MATRIX) {
		return modelMatrix;
	}
	else {
		return junk;
	}
}

void GLM_GetMatrix(GLenum type, float* matrix)
{
	GLM_SetMatrix(matrix, GL_MatrixForMode(type));
}

float* GLM_ModelviewMatrix(void)
{
	return modelMatrix;
}

float* GLM_ProjectionMatrix(void)
{
	return projectionMatrix;
}

void GLM_SetMatrix(float* target, const float* source)
{
	memcpy(target, source, sizeof(float) * 16);
}

void GL_OrthographicProjection(float left, float right, float top, float bottom, float zNear, float zFar)
{
	GLM_OrthographicProjection(left, right, top, bottom, zNear, zFar);
	GLM_Cache2DMatrix();

	if (GL_UseImmediateMode()) {
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(left, right, top, bottom, zNear, zFar);
	}
}

void GLM_RotateMatrix(float* matrix, float angle, float x, float y, float z)
{
	vec3_t vec = { x, y, z };
	double s = sin(angle * M_PI / 180);
	double c = cos(angle * M_PI / 180);
	float rotation[16];
	float result[16];

	VectorNormalize(vec);
	x = vec[0];
	y = vec[1];
	z = vec[2];

	// Taken from https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/glRotate.xml
	rotation[0] = x * x * (1 - c) + c;
	rotation[4] = x * y * (1 - c) - z * s;
	rotation[8] = x * z * (1 - c) + y * s;
	rotation[12] = 0;
	rotation[1] = y * x * (1 - c) + z * s;
	rotation[5] = y * y * (1 - c) + c;
	rotation[9] = y * z * (1 - c) - x * s;
	rotation[13] = 0;
	rotation[2] = x * z * (1 - c) - y * s;
	rotation[6] = y * z * (1 - c) + x * s;
	rotation[10] = z * z * (1 - c) + c;
	rotation[14] = 0;
	rotation[3] = rotation[7] = rotation[11] = 0;
	rotation[15] = 1;

	GLM_MultiplyMatrix(rotation, matrix, result);
	GLM_SetMatrix(matrix, result);
}

void GLM_RotateVector(vec3_t vector, float angle, float x, float y, float z)
{
	vec3_t vec = { x, y, z };
	double s = sin(angle * M_PI / 180);
	double c = cos(angle * M_PI / 180);
	float rotation[16];
	float input[4] = { vector[0], vector[1], vector[2], 1 };
	float output[4];

	VectorNormalize(vec);
	x = vec[0];
	y = vec[1];
	z = vec[2];

	// Taken from https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/glRotate.xml
	rotation[0] = x * x * (1 - c) + c;
	rotation[4] = x * y * (1 - c) - z * s;
	rotation[8] = x * z * (1 - c) + y * s;
	rotation[12] = 0;
	rotation[1] = y * x * (1 - c) + z * s;
	rotation[5] = y * y * (1 - c) + c;
	rotation[9] = y * z * (1 - c) - x * s;
	rotation[13] = 0;
	rotation[2] = x * z * (1 - c) - y * s;
	rotation[6] = y * z * (1 - c) + x * s;
	rotation[10] = z * z * (1 - c) + c;
	rotation[14] = 0;
	rotation[3] = rotation[7] = rotation[11] = 0;
	rotation[15] = 1;

	GLM_MultiplyVector(rotation, input, output);
	VectorCopy(output, vector);
}

void GLM_TransformMatrix(float* matrix, float x, float y, float z)
{
	matrix[12] += matrix[0] * x + matrix[4] * y + matrix[8] * z;
	matrix[13] += matrix[1] * x + matrix[5] * y + matrix[9] * z;
	matrix[14] += matrix[2] * x + matrix[6] * y + matrix[10] * z;
	matrix[15] += matrix[3] * x + matrix[7] * y + matrix[11] * z;
}

void GLM_ScaleMatrix(float* matrix, float x_scale, float y_scale, float z_scale)
{
	matrix[0] *= x_scale;
	matrix[1] *= x_scale;
	matrix[2] *= x_scale;
	matrix[3] *= x_scale;
	matrix[4] *= y_scale;
	matrix[5] *= y_scale;
	matrix[6] *= y_scale;
	matrix[7] *= y_scale;
	matrix[8] *= z_scale;
	matrix[9] *= z_scale;
	matrix[10] *= z_scale;
	matrix[11] *= z_scale;
}

void GLM_MultiplyMatrix(const float* lhs, const float* rhs, float* target)
{
	target[0] = lhs[0] * rhs[0] + lhs[1] * rhs[4] + lhs[2] * rhs[8] + lhs[3] * rhs[12];
	target[1] = lhs[0] * rhs[1] + lhs[1] * rhs[5] + lhs[2] * rhs[9] + lhs[3] * rhs[13];
	target[2] = lhs[0] * rhs[2] + lhs[1] * rhs[6] + lhs[2] * rhs[10] + lhs[3] * rhs[14];
	target[3] = lhs[0] * rhs[3] + lhs[1] * rhs[7] + lhs[2] * rhs[11] + lhs[3] * rhs[15];

	target[4] = lhs[4] * rhs[0] + lhs[5] * rhs[4] + lhs[6] * rhs[8] + lhs[7] * rhs[12];
	target[5] = lhs[4] * rhs[1] + lhs[5] * rhs[5] + lhs[6] * rhs[9] + lhs[7] * rhs[13];
	target[6] = lhs[4] * rhs[2] + lhs[5] * rhs[6] + lhs[6] * rhs[10] + lhs[7] * rhs[14];
	target[7] = lhs[4] * rhs[3] + lhs[5] * rhs[7] + lhs[6] * rhs[11] + lhs[7] * rhs[15];

	target[8] = lhs[8] * rhs[0] + lhs[9] * rhs[4] + lhs[10] * rhs[8] + lhs[11] * rhs[12];
	target[9] = lhs[8] * rhs[1] + lhs[9] * rhs[5] + lhs[10] * rhs[9] + lhs[11] * rhs[13];
	target[10] = lhs[8] * rhs[2] + lhs[9] * rhs[6] + lhs[10] * rhs[10] + lhs[11] * rhs[14];
	target[11] = lhs[8] * rhs[3] + lhs[9] * rhs[7] + lhs[10] * rhs[11] + lhs[11] * rhs[15];

	target[12] = lhs[12] * rhs[0] + lhs[13] * rhs[4] + lhs[14] * rhs[8] + lhs[15] * rhs[12];
	target[13] = lhs[12] * rhs[1] + lhs[13] * rhs[5] + lhs[14] * rhs[9] + lhs[15] * rhs[13];
	target[14] = lhs[12] * rhs[2] + lhs[13] * rhs[6] + lhs[14] * rhs[10] + lhs[15] * rhs[14];
	target[15] = lhs[12] * rhs[3] + lhs[13] * rhs[7] + lhs[14] * rhs[11] + lhs[15] * rhs[15];
}

void GLM_MultiplyVector3f(const float* matrix, float x, float y, float z, float* result)
{
	const float vector[4] = { x, y, z, 1 };

	result[0] = matrix[0] * vector[0] + matrix[4] * vector[1] + matrix[8] * vector[2] + matrix[12] * vector[3];
	result[1] = matrix[1] * vector[0] + matrix[5] * vector[1] + matrix[9] * vector[2] + matrix[13] * vector[3];
	result[2] = matrix[2] * vector[0] + matrix[6] * vector[1] + matrix[10] * vector[2] + matrix[14] * vector[3];
	result[3] = matrix[3] * vector[0] + matrix[7] * vector[1] + matrix[11] * vector[2] + matrix[15] * vector[3];
}

void GLM_MultiplyVector(const float* matrix, const float* vector, float* result)
{
	result[0] = matrix[0] * vector[0] + matrix[4] * vector[1] + matrix[8] * vector[2] + matrix[12] * vector[3];
	result[1] = matrix[1] * vector[0] + matrix[5] * vector[1] + matrix[9] * vector[2] + matrix[13] * vector[3];
	result[2] = matrix[2] * vector[0] + matrix[6] * vector[1] + matrix[10] * vector[2] + matrix[14] * vector[3];
	result[3] = matrix[3] * vector[0] + matrix[7] * vector[1] + matrix[11] * vector[2] + matrix[15] * vector[3];
}

void GLM_MultiplyVector3fv(const float* matrix, const vec3_t vector, float* result)
{
	result[0] = matrix[0] * vector[0] + matrix[4] * vector[1] + matrix[8] * vector[2] + matrix[12];
	result[1] = matrix[1] * vector[0] + matrix[5] * vector[1] + matrix[9] * vector[2] + matrix[13];
	result[2] = matrix[2] * vector[0] + matrix[6] * vector[1] + matrix[10] * vector[2] + matrix[14];
	result[3] = matrix[3] * vector[0] + matrix[7] * vector[1] + matrix[11] * vector[2] + matrix[15];
}

void GL_IdentityModelView(void)
{
	GLM_SetIdentityMatrix(GLM_ModelviewMatrix());

	if (GL_UseImmediateMode() && !glc_pause_updates) {
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		GL_LogAPICall("GL_IdentityModelView(%s)", NameForMatrix(GL_MODELVIEW));
	}
}

void GL_GetMatrix(GLenum mode, GLfloat* matrix)
{
	GLM_GetMatrix(mode, matrix);
}

void GL_Rotate(GLenum matrix, float angle, float x, float y, float z)
{
	GLM_RotateMatrix(GL_MatrixForMode(matrix), angle, x, y, z);

	if (GL_UseImmediateMode() && !glc_pause_updates) {
		glMatrixMode(matrix);
		glRotatef(angle, x, y, z);
		GL_LogAPICall("GL_RotateMatrix(%s)", NameForMatrix(matrix));
	}
}

void GL_Translate(GLenum matrix, float x, float y, float z)
{
	GLM_TransformMatrix(GL_MatrixForMode(matrix), x, y, z);

	if (GL_UseImmediateMode() && !glc_pause_updates) {
		glMatrixMode(matrix);
		glTranslatef(x, y, z);
		GL_LogAPICall("GL_Translate(%s)", NameForMatrix(matrix));
	}
}

void GL_IdentityProjectionView(void)
{
	GLM_SetIdentityMatrix(GLM_ProjectionMatrix());

	if (GL_UseImmediateMode()) {
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		GL_LogAPICall("GL_Identity(%s)", NameForMatrix(GL_PROJECTION));
	}
}

#ifdef GL_PARANOIA
void GL_ProcessErrors(const char* message)
{
	GLenum error = glGetError();
	while (error != GL_NO_ERROR) {
		Con_Printf("%s> = %X\n", message, error);
		error = glGetError();
	}
}
#endif

void GL_PushMatrix(GLenum mode, float* matrix)
{
	memcpy(matrix, GL_MatrixForMode(mode), sizeof(float) * 16);
}

void GL_PopMatrix(GLenum mode, float* matrix)
{
	memcpy(GL_MatrixForMode(mode), matrix, sizeof(float) * 16);

	if (GL_UseImmediateMode()) {
		glMatrixMode(mode);
		glLoadMatrixf(matrix);
		GL_LogAPICall("GL_PopMatrix(%s)", NameForMatrix(mode));
	}
}

void GL_Scale(GLenum matrix, float xScale, float yScale, float zScale)
{
	GLM_ScaleMatrix(GL_MatrixForMode(matrix), xScale, yScale, zScale);

	if (GL_UseImmediateMode() && !glc_pause_updates) {
		glMatrixMode(matrix);
		glScalef(xScale, yScale, zScale);
		GL_LogAPICall("GL_ScaleMatrix(%s)", NameForMatrix(matrix));
	}
}

void GL_Frustum(double left, double right, double bottom, double top, double zNear, double zFar)
{
	float perspective[16] = { 0 };
	float projection[16];
	float new_projection[16];

	perspective[0] = (2 * zNear) / (right - left);
	perspective[8] = (right + left) / (right - left);
	perspective[5] = (2 * zNear) / (top - bottom);
	perspective[9] = (top + bottom) / (top - bottom);
	perspective[10] = -(zFar + zNear) / (zFar - zNear);
	perspective[11] = -1;
	perspective[14] = -2 * (zFar * zNear) / (zFar - zNear);

	GLM_GetMatrix(GL_PROJECTION, projection);
	GLM_MultiplyMatrix(perspective, projection, new_projection);
	GLM_SetMatrix(GL_MatrixForMode(GL_PROJECTION), new_projection);
	
	if (GL_UseImmediateMode()) {
		glFrustum(left, right, bottom, top, zNear, zFar);
		GL_LogAPICall("glFrustum()");
	}
}

void GLM_DebugMatrix(GLenum mode, const char* label)
{
	float matrix[16];
	int i;

	GL_GetMatrix(mode, matrix);

	Con_Printf("%s\n", label);
	for (i = 0; i < 4; ++i) {
		Con_Printf("  [%5.3f %5.3f %5.3f %5.3f]\n", matrix[i], matrix[i + 4], matrix[i + 8], matrix[i + 12]);
	}
}

void GLM_MultiplyMatrixVector(float* matrix, vec3_t vector, float* result)
{
	result[0] = matrix[0] * vector[0] + matrix[4] * vector[1] + matrix[8] * vector[2] + matrix[12] * vector[3];
	result[1] = matrix[1] * vector[0] + matrix[5] * vector[1] + matrix[9] * vector[2] + matrix[13] * vector[3];
	result[2] = matrix[2] * vector[0] + matrix[6] * vector[1] + matrix[10] * vector[2] + matrix[14] * vector[3];
	result[3] = matrix[3] * vector[0] + matrix[7] * vector[1] + matrix[11] * vector[2] + matrix[15] * vector[3];
}

void GLC_PauseMatrixUpdate(void)
{
	glc_pause_updates = true;
}

void GLC_ResumeMatrixUpdate(void)
{
	glc_pause_updates = false;
}

void GLC_LoadMatrix(GLenum matrix)
{
	if (GL_UseImmediateMode()) {
		glMatrixMode(matrix);
		glLoadMatrixf(GL_MatrixForMode(matrix));
		GL_LogAPICall("glLoadMatrixf(%s)", NameForMatrix(matrix));
	}
}

void GLC_BeginCausticsTextureMatrix(void)
{
	GL_SelectTexture(GL_TEXTURE1);
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glScalef(0.5, 0.5, 1);
	glRotatef(r_refdef2.time * 10, 1, 0, 0);
	glRotatef(r_refdef2.time * 10, 0, 1, 0);
	glMatrixMode(GL_MODELVIEW);
}

void GLC_EndCausticsTextureMatrix(void)
{
	GL_SelectTexture(GL_TEXTURE1);
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
}

qbool R_Project3DCoordinates(float objx, float objy, float objz, float* winx, float* winy, float* winz)
{
	float model[16], proj[16];
	float in[4] = { objx, objy, objz, 1.0f };
	float out[4];
	int view[4];
	int i;

	GL_GetMatrix(GL_MODELVIEW_MATRIX, model);
	GL_GetMatrix(GL_PROJECTION_MATRIX, proj);
	GL_GetViewport(view);

	for (i = 0; i < 4; i++) {
		out[i] = in[0] * model[0 * 4 + i] + in[1] * model[1 * 4 + i] + in[2] * model[2 * 4 + i] + in[3] * model[3 * 4 + i];
	}

	for (i = 0; i < 4; i++) {
		in[i] = out[0] * proj[0 * 4 + i] + out[1] * proj[1 * 4 + i] + out[2] * proj[2 * 4 + i] + out[3] * proj[3 * 4 + i];
	}

	if (!in[3]) {
		return false;
	}

	VectorScale(in, 1 / in[3], in);
	*winx = view[0] + (1 + in[0]) * view[2] / 2;
	*winy = view[1] + (1 + in[1]) * view[3] / 2;
	*winz = (1 + in[2]) / 2;

	return true;
}
