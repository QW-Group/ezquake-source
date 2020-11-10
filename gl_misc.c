/*
Copyright (C) 1996-1997 Id Software, Inc.

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
// gl_misc.c

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "tr_types.h"
#include "r_renderer.h"
#include "r_lightmaps_internal.h"
#include "gl_texture_internal.h"

typedef struct gl_enum_value_s {
	GLenum value;
	const char* name;
	int benchmark_bpp;
} gl_enum_value_t;

static gl_enum_value_t image_formats[] = {
	{ GL_RED, "RED", 0 },
	{ GL_RG, "RG", 0 },
	{ GL_RGB, "RGB", 0 },
	{ GL_BGR, "BGR", 0 },
	{ GL_RGBA, "RGBA", 4 },
	{ GL_BGRA, "BGRA", 4 },
	{ GL_RED_INTEGER, "RED_INT" },
	{ GL_RG_INTEGER, "RG_INT" },
	{ GL_RGB_INTEGER, "RGB_INT" },
	{ GL_BGR_INTEGER, "BGR_INT" },
	{ GL_RGBA_INTEGER, "RGBA_INT" },
	{ GL_BGRA_INTEGER, "BGRA_INT" },
	{ GL_STENCIL_INDEX, "STENCIL_INDEX" },
	{ GL_DEPTH_COMPONENT, "DEPTH_COMPONENT" },
	{ GL_DEPTH_STENCIL, "DEPTH_STENCIL" }
};
static gl_enum_value_t image_types[] = {
	{ GL_UNSIGNED_BYTE, "UBYTE", 3 },
	{ GL_BYTE, "BYTE", 3 },
	{ GL_UNSIGNED_SHORT, "USHORT", 0 },
	{ GL_SHORT, "SHORT", 0 },
	{ GL_UNSIGNED_INT, "UINT", 4 },
	{ GL_INT, "INT", 4 },
	{ GL_FLOAT, "FLOAT", 0 },
	{ GL_UNSIGNED_BYTE_3_3_2, "UBYTE_332", 0 },
	{ GL_UNSIGNED_BYTE_2_3_3_REV, "UBYTE_233R", 0 },
	{ GL_UNSIGNED_SHORT_5_6_5, "USHORT_565", 0 },
	{ GL_UNSIGNED_SHORT_5_6_5_REV, "USHORT_565R", 0 },
	{ GL_UNSIGNED_SHORT_4_4_4_4, "USHORT_4444", 0 },
	{ GL_UNSIGNED_SHORT_4_4_4_4_REV, "USHORT_4444R", 0 },
	{ GL_UNSIGNED_SHORT_5_5_5_1, "USHORT_5551", 0 },
	{ GL_UNSIGNED_SHORT_1_5_5_5_REV, "USHORT_1555R", 0 },
	{ GL_UNSIGNED_INT_8_8_8_8, "UINT_8888", 4 },
	{ GL_UNSIGNED_INT_8_8_8_8_REV, "UINT_8888R", 4 },
	{ GL_UNSIGNED_INT_10_10_10_2, "UINT_101010_2", 4 },
	{ GL_UNSIGNED_INT_2_10_10_10_REV, "UINT_2_101010R", 4 }
};


void GL_Clear(qbool clear_color)
{
	R_ApplyRenderingState(r_state_default_3d);
	glClear((clear_color ? GL_COLOR_BUFFER_BIT : 0) | GL_DEPTH_BUFFER_BIT);
}

void GL_EnsureFinished(void)
{
	glFinish();
}

#ifdef WITH_RENDERING_TRACE
GLenum GL_ProcessErrors(const char* message)
{
	GLenum error = glGetError();
	GLenum firstError = error;

	while (error != GL_NO_ERROR) {
		if (error == GL_INVALID_ENUM) {
			R_TraceLogAPICall("  ERROR: %s (GL_INVALID_ENUM)\n", message);
		}
		else if (error == GL_INVALID_VALUE) {
			R_TraceLogAPICall("  ERROR: %s (GL_INVALID_VALUE)\n", message);
		}
		else if (error == GL_STACK_OVERFLOW) {
			R_TraceLogAPICall("  ERROR: %s (GL_STACK_OVERFLOW)\n", message);
		}
		else if (error == GL_STACK_UNDERFLOW) {
			R_TraceLogAPICall("  ERROR: %s (GL_STACK_UNDERFLOW)\n", message);
		}
		else if (error == GL_OUT_OF_MEMORY) {
			R_TraceLogAPICall("  ERROR: %s (GL_OUT_OF_MEMORY)\n", message);
		}
		else {
			R_TraceLogAPICall("  ERROR: %s (UNKNOWN_ERROR 0x%X)\n", message, error);
		}
		error = glGetError();
	}
	return firstError;
}
#endif // WITH_RENDERING_TRACE

static void GL_PrintInfoLine(const char* label, int labelsize, const char* fmt, ...)
{
	va_list argptr;
	char msg[128];

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	Com_Printf_State(PRINT_ALL, "  %-*s ", labelsize, label);
	con_margin = labelsize + 3;
	Com_Printf_State(PRINT_ALL, "%s", msg);
	con_margin = 0;
	Com_Printf_State(PRINT_ALL, "\n");
}

void GL_PrintGfxInfo(void)
{
	SDL_DisplayMode current;
	GLint num_extensions;
	int i;

	Com_Printf_State(PRINT_ALL, "\nOpenGL (%s)\n", R_UseImmediateOpenGL() ? "classic" : "glsl");
	GL_PrintInfoLine("Vendor:", 9, "%s", (const char*)glConfig.vendor_string);
	GL_PrintInfoLine("Renderer:", 9, "%s", (const char*)glConfig.renderer_string);
	GL_PrintInfoLine("Version:", 9, "%s", (const char*)glConfig.version_string);
	if (R_UseModernOpenGL()) {
		GL_PrintInfoLine("GLSL:", 9, "%s", (const char*)glConfig.version_string);
	}

	if (r_showextensions.integer) {
		Com_Printf_State(PRINT_ALL, "GL_EXTENSIONS: ");
		if (GL_VersionAtLeast(3, 0)) {
			typedef const GLubyte* (APIENTRY *glGetStringi_t)(GLenum name, GLuint index);
			glGetStringi_t glGetStringi = (glGetStringi_t)SDL_GL_GetProcAddress("glGetStringi");
			if (glGetStringi) {
				glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);
				for (i = 0; i < num_extensions; ++i) {
					Com_Printf_State(PRINT_ALL, "%s%s", i ? " " : "", glGetStringi(GL_EXTENSIONS, i));
				}
				Com_Printf_State(PRINT_ALL, "\n");
			}
			else {
				Com_Printf_State(PRINT_ALL, "(list unavailable)\n");
			}
		}
		else {
			Com_Printf_State(PRINT_ALL, "%s\n", (const char*)glGetString(GL_EXTENSIONS));
		}
	}

	Com_Printf_State(PRINT_ALL, "Textures:\n");
	GL_PrintInfoLine("Units:", 14, "%d", glConfig.texture_units);
	GL_PrintInfoLine("Size:", 14, "%d", glConfig.gl_max_size_default);
	if (R_UseModernOpenGL()) {
		GL_PrintInfoLine("3D Sizes:", 14, "%dx%dx%d\n", glConfig.max_3d_texture_size, glConfig.max_3d_texture_size, glConfig.max_texture_depth);
	}
	if (glConfig.preferred_format || glConfig.preferred_type) {
		Com_Printf_State(PRINT_ALL, "Preferences\n");
		const char* format = "?";
		const char* type = "?";

		for (i = 0; i < sizeof(image_formats) / sizeof(image_formats[0]); ++i) {
			if (image_formats[i].value == glConfig.preferred_format) {
				format = image_formats[i].name;
				break;
			}
		}
		for (i = 0; i < sizeof(image_types) / sizeof(image_types[0]); ++i) {
			if (image_types[i].value == glConfig.preferred_type) {
				type = image_types[i].name;
				break;
			}
		}

		GL_PrintInfoLine("Image Format:", 14, "0x%x (%s)", glConfig.preferred_format, format);
		GL_PrintInfoLine("Image Type:", 14, "0x%x (%s)", glConfig.preferred_type, type);
	}
	GL_PrintInfoLine("Lightmaps:", 14, "%s/%s", GL_Supported(R_SUPPORT_BGRA_LIGHTMAPS) ? "BGRA" : "RGBA", GL_Supported(R_SUPPORT_INT8888R_LIGHTMAPS) ? "UINT8888R" : "UBYTE");

	Com_Printf_State(PRINT_ALL, "Supported features:\n");
	GL_PrintInfoLine("Shaders:", 15, "%s", GL_Supported(R_SUPPORT_RENDERING_SHADERS) ? "&c0f0available&r" : "&cf00unsupported&r");
	GL_PrintInfoLine("Compute:", 15, "%s", GL_Supported(R_SUPPORT_COMPUTE_SHADERS) ? "&c0f0available&r" : "&cf00unsupported&r");
	GL_PrintInfoLine("Framebuffers:", 15, "%s", GL_Supported(R_SUPPORT_FRAMEBUFFERS) ? "&c0f0available&r" : "&cf00unsupported&r");
	GL_PrintInfoLine("Tex arrays:", 15, "%s", GL_Supported(R_SUPPORT_TEXTURE_ARRAYS) ? "&c0f0available&r" : "&cf00unsupported&r");
	GL_PrintInfoLine("Tex samplers:", 15, "%s", GL_Supported(R_SUPPORT_TEXTURE_SAMPLERS) ? "&c0f0available&r" : "&cf00unsupported&r");
	GL_PrintInfoLine("HW lighting:", 15, "%s", GL_Supported(R_SUPPORT_FEATURE_HW_LIGHTING) ? "&c0f0available&r" : "&cf00unsupported&r");

	if (SDL_GetCurrentDisplayMode(VID_DisplayNumber(r_fullscreen.value), &current) != 0) {
		current.refresh_rate = 0; // print 0Hz if we run into problem fetching data
	}

	Com_Printf_State(PRINT_ALL, "Video\n");
	GL_PrintInfoLine("Resolution:", 12, "%dx%d@%dhz [%s]", current.w, current.h, current.refresh_rate, r_fullscreen.integer ? "fullscreen" : "windowed");
	GL_PrintInfoLine("Format:", 12, "%2d-bit color\n%2d-bit z-buffer\n%2d-bit stencil", glConfig.colorBits, glConfig.depthBits, glConfig.stencilBits);
	if (GL_FramebufferEnabled3D()) {
		GL_PrintInfoLine("Framebuffer:", 12, "%dx%d,%s\n", GL_FrameBufferWidth(framebuffer_std), GL_FrameBufferHeight(framebuffer_std), GL_FramebufferZBufferString(framebuffer_std));
	}
	if (vid.aspect) {
		GL_PrintInfoLine("Aspect:", 12, "%f%%", vid.aspect);
	}
	if (r_conwidth.integer || r_conheight.integer) {
		GL_PrintInfoLine("Console Res:", 12,"%d x %d", r_conwidth.integer, r_conheight.integer);
	}
}

void GL_Viewport(int x, int y, int width, int height)
{
	glViewport(x, y, width, height);
}

const char* GL_DescriptiveString(void)
{
	return (const char*)glGetString(GL_RENDERER);
}

typedef struct {
	int format;
	int type;
	float result;
} lightmap_benchmark_t;

int LightmapBenchmarkComparison(const void* lhs_, const void* rhs_)
{
	lightmap_benchmark_t* lhs = (lightmap_benchmark_t*)lhs_;
	lightmap_benchmark_t* rhs = (lightmap_benchmark_t*)rhs_;

	if (lhs->result < rhs->result) {
		return -1;
	}
	if (lhs->result > rhs->result) {
		return 1;
	}
	return (uintptr_t)lhs < (uintptr_t)rhs ? -1 : 1;
}

void GL_BenchmarkLightmapFormats(void)
{
	texture_ref texture;
	int format;
	int type;
	byte data[LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * 16];
	lightmap_benchmark_t results[(sizeof(image_formats) / sizeof(image_formats[0])) * (sizeof(image_types) / sizeof(image_types[0]))];
	int count = 0, i;

	for (format = 0; format < sizeof(image_formats) / sizeof(image_formats[0]); ++format) {
		for (type = 0; type < sizeof(image_types) / sizeof(image_types[0]); ++type) {
			double started;
			double finished;

			if (!image_formats[format].benchmark_bpp || !image_types[type].benchmark_bpp || image_types[type].benchmark_bpp > image_formats[format].benchmark_bpp) {
				continue;
			}

			memset(data, 255, sizeof(data));

			GL_ProcessErrors("Pre-init");

			GL_CreateTexturesWithIdentifier(texture_type_2d, 1, &texture, "lightmap-benchmark");
			renderer.TextureUnitBind(0, texture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, 0, image_formats[format].value, image_types[type].value, data);
			if (glGetError() != GL_NO_ERROR) {
				Con_Printf("%s/%s: error\n", image_formats[format].name, image_types[type].name);
				continue;
			}
			renderer.TextureSetFiltering(texture, texture_minification_linear, texture_magnification_linear);
			renderer.TextureWrapModeClamp(texture);
			glFinish();

			memset(data, 0, sizeof(data));
			started = Sys_DoubleTime();
			for (i = 0; i < 1000; ++i) {
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, image_formats[format].value, image_types[type].value, data);
			}
			glFinish();
			finished = Sys_DoubleTime();

			GL_ProcessErrors("Post-test");
			renderer.TextureDelete(texture);

			results[count].format = format;
			results[count].type = type;
			results[count].result = (finished - started) * 1000.0f;
			++count;
		}
	}

	qsort(results, count, sizeof(results[0]), LightmapBenchmarkComparison);

	Con_Printf("Results:\n");
	for (i = 0; i < count; ++i) {
		qbool preferred;
		qbool current;
		char label[20];

		format = results[i].format;
		type = results[i].type;

		strlcpy(label, image_formats[format].name, sizeof(label));
		strlcat(label, "/", sizeof(label));
		strlcat(label, image_types[type].name, sizeof(label));

		preferred = (image_formats[format].value == glConfig.preferred_format && image_types[type].value == glConfig.preferred_type);
		current = (GL_Supported(R_SUPPORT_BGRA_LIGHTMAPS) ? GL_BGRA : GL_RGBA) == image_formats[format].value &&
		          (GL_Supported(R_SUPPORT_INT8888R_LIGHTMAPS) ? GL_UNSIGNED_INT_8_8_8_8_REV : GL_UNSIGNED_BYTE) == image_types[type].value;

		Con_Printf("%s %02d %-20s: %8.3fms%s&r\n", current ? "&c0f0>>>" : "   ", i + 1, label, results[i].result, current && preferred ? " <<< preferred" : preferred ? " &cff0<<< preferred" : "");
	}
}
