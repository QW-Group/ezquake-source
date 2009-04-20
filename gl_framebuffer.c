#if defined(FRAMEBUFFERS) && defined(GLQUAKE)

// Loads and does all the framebuffer stuff
#include "quakedef.h"
#include "gl_framebuffer.h"

cvar_t	framebuffer		= {"framebuffer", "0"};
/*
cvar_t	fb_width		= {"fb_width","0"};
cvar_t	fb_height		= {"fb_height","0"};
cvar_t	fb_ratio_w		= {"fb_ratio_w","0"};
cvar_t	fb_ratio_h		= {"fb_ratio_h","0"};
cvar_t	fb_x			= {"fb_x","0"};
cvar_t	fb_y			= {"fb_y","0"};
*/

fb_t	main_fb; // The main framebuffer that can be used generally to draw things offscreen.

qbool	use_framebuffer = false;

//
// EXT_framebuffer_object - http://oss.sgi.com/projects/ogl-sample/registry/EXT/framebuffer_object.txt
//
extern PFNGLISRENDERBUFFEREXTPROC						glIsRenderbufferEXT							= NULL; // boolean IsRenderbufferEXT(uint renderbuffer);
extern PFNGLBINDRENDERBUFFEREXTPROC						glBindRenderbufferEXT						= NULL; // void BindRenderbufferEXT(enum target, uint renderbuffer);
extern PFNGLDELETERENDERBUFFERSEXTPROC					glDeleteRenderbuffersEXT					= NULL; // void DeleteRenderbuffersEXT(sizei n, const uint *renderbuffers);
extern PFNGLGENRENDERBUFFERSEXTPROC						glGenRenderbuffersEXT						= NULL; // void GenRenderbuffersEXT(sizei n, uint *renderbuffers);
extern PFNGLRENDERBUFFERSTORAGEEXTPROC					glRenderbufferStorageEXT					= NULL; // void RenderbufferStorageEXT(enum target, enum internalformat, sizei width, sizei height);
extern PFNGLGETRENDERBUFFERPARAMETERIVEXTPROC			glGetRenderbufferParameterivEXT				= NULL; // void GetRenderbufferParameterivEXT(enum target, enum pname, int *params);
extern PFNGLISFRAMEBUFFEREXTPROC						glIsFramebufferEXT							= NULL; // boolean IsFramebufferEXT(uint framebuffer);
extern PFNGLBINDFRAMEBUFFEREXTPROC						glBindFramebufferEXT						= NULL; // void BindFramebufferEXT(enum target, uint framebuffer);
extern PFNGLDELETEFRAMEBUFFERSEXTPROC					glDeleteFramebuffersEXT						= NULL; // void DeleteFramebuffersEXT(sizei n, const uint *framebuffers);
extern PFNGLGENFRAMEBUFFERSEXTPROC						glGenFramebuffersEXT						= NULL; // void GenFramebuffersEXT(sizei n, uint *framebuffers)
extern PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC				glCheckFramebufferStatusEXT					= NULL; // enum CheckFramebufferStatusEXT(enum target);
extern PFNGLFRAMEBUFFERTEXTURE1DEXTPROC					glFramebufferTexture1DEXT					= NULL; // void FramebufferTexture1DEXT(enum target, enum attachment, enum textarget, uint texture, int level);
extern PFNGLFRAMEBUFFERTEXTURE2DEXTPROC					glFramebufferTexture2DEXT					= NULL; // void FramebufferTexture2DEXT(enum target, enum attachment, enum textarget, uint texture, int level);
extern PFNGLFRAMEBUFFERTEXTURE3DEXTPROC					glFramebufferTexture3DEXT					= NULL; // void FramebufferTexture3DEXT(enum target, enum attachment, enum textarget, uint texture, int level, int zoffset);
extern PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC				glFramebufferRenderbufferEXT				= NULL; // void FramebufferRenderbufferEXT(enum target, enum attachment, enum renderbuffertarget, uint renderbuffer);
extern PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC	glGetFramebufferAttachmentParameterivEXT	= NULL; // void GetFramebufferAttachmentParameterivEXT(enum target, enum attachment, enum pname, int *params);
extern PFNGLGENERATEMIPMAPEXTPROC						glGenerateMipmapEXT							= NULL; // void GenerateMipmapEXT(enum target);

// GL_ARB_multitexture.
extern PFNGLACTIVETEXTUREARBPROC       glActiveTextureEXT       = NULL;
extern PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTextureEXT = NULL;
extern PFNGLMULTITEXCOORD2FARBPROC     glMultiTexCoord2fEXT     = NULL;

//
// Initialize framebuffer stuff, Loads procadresses and such.
//
void Framebuffer_Init (void)
{
	char *ext = (char*)qglGetString( GL_EXTENSIONS );
//	int temp;

	// FIXME: alredy initialized, what to do?
	//        doubt this will work on vid_restart...
	if (use_framebuffer)
	{
		return;
	}

/* wtf, and a bit later set to use_framebuffer = true;
	if ((temp = COM_CheckParm("-framebuffer")) && temp + 1 < COM_Argc())
	{
		use_framebuffer = Q_atoi(COM_Argv(temp+1));
	}
*/
	
	// Check if the driver supports framebuffers.
	if( strstr( ext, "EXT_framebuffer_object" ) == NULL || !strstr(ext, "GL_ARB_multitexture"))
	{
		Com_Printf("Framebuffer extension is not supported by your driver/graphics card.\n");
		use_framebuffer = false;
	}
	else
	{
		//
		// Get the procedure adresses for the frame buffer functions.
		// 
		use_framebuffer = true;
		Com_Printf("Framebuffer extension do exist \n");
		glIsRenderbufferEXT				= (PFNGLISRENDERBUFFEREXTPROC)wglGetProcAddress("glIsRenderbufferEXT");
		glBindRenderbufferEXT			= (PFNGLBINDRENDERBUFFEREXTPROC)wglGetProcAddress("glBindRenderbufferEXT");
		glDeleteRenderbuffersEXT		= (PFNGLDELETERENDERBUFFERSEXTPROC)wglGetProcAddress("glDeleteRenderbuffersEXT");
		glGenRenderbuffersEXT			= (PFNGLGENRENDERBUFFERSEXTPROC)wglGetProcAddress("glGenRenderbuffersEXT");
		glRenderbufferStorageEXT		= (PFNGLRENDERBUFFERSTORAGEEXTPROC)wglGetProcAddress("glRenderbufferStorageEXT");
		glGetRenderbufferParameterivEXT = (PFNGLGETRENDERBUFFERPARAMETERIVEXTPROC)wglGetProcAddress("glGetRenderbufferParameterivEXT");
		glIsFramebufferEXT				= (PFNGLISFRAMEBUFFEREXTPROC)wglGetProcAddress("glIsFramebufferEXT");
		glBindFramebufferEXT			= (PFNGLBINDFRAMEBUFFEREXTPROC)wglGetProcAddress("glBindFramebufferEXT");
		glDeleteFramebuffersEXT			= (PFNGLDELETEFRAMEBUFFERSEXTPROC)wglGetProcAddress("glDeleteFramebuffersEXT");
		glGenFramebuffersEXT			= (PFNGLGENFRAMEBUFFERSEXTPROC)wglGetProcAddress("glGenFramebuffersEXT");
		glCheckFramebufferStatusEXT		= (PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC)wglGetProcAddress("glCheckFramebufferStatusEXT");
		glFramebufferTexture1DEXT		= (PFNGLFRAMEBUFFERTEXTURE1DEXTPROC)wglGetProcAddress("glFramebufferTexture1DEXT");
		glFramebufferTexture2DEXT		= (PFNGLFRAMEBUFFERTEXTURE2DEXTPROC)wglGetProcAddress("glFramebufferTexture2DEXT");
		glFramebufferTexture3DEXT		= (PFNGLFRAMEBUFFERTEXTURE3DEXTPROC)wglGetProcAddress("glFramebufferTexture3DEXT");
		glFramebufferRenderbufferEXT	= (PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC)wglGetProcAddress("glFramebufferRenderbufferEXT");
		glGetFramebufferAttachmentParameterivEXT = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC)wglGetProcAddress("glGetFramebufferAttachmentParameterivEXT");
		glGenerateMipmapEXT				= (PFNGLGENERATEMIPMAPEXTPROC)wglGetProcAddress("glGenerateMipmapEXT");
		glActiveTextureEXT				= (PFNGLACTIVETEXTUREARBPROC)wglGetProcAddress("glActiveTextureARB");
		glClientActiveTextureEXT		= (PFNGLCLIENTACTIVETEXTUREARBPROC)wglGetProcAddress("glClientActiveTextureARB");
		glMultiTexCoord2fEXT			= (PFNGLMULTITEXCOORD2FARBPROC)wglGetProcAddress("glMultiTexCoord2fARB");
	
		//
		// Make sure we find them all.
		//
		if( !glIsRenderbufferEXT || !glBindRenderbufferEXT || !glDeleteRenderbuffersEXT || 
			!glGenRenderbuffersEXT || !glRenderbufferStorageEXT || !glGetRenderbufferParameterivEXT || 
			!glIsFramebufferEXT || !glBindFramebufferEXT || !glDeleteFramebuffersEXT || 
			!glGenFramebuffersEXT || !glCheckFramebufferStatusEXT || !glFramebufferTexture1DEXT || 
			!glFramebufferTexture2DEXT || !glFramebufferTexture3DEXT || !glFramebufferRenderbufferEXT||  
			!glGetFramebufferAttachmentParameterivEXT || !glGenerateMipmapEXT )
		{
			Com_Printf("Couldnt get the function adresses for framebuffer support.\n");
			use_framebuffer = false;
			return;
		}

		/*
		Cmd_AddCommand("framebuffer_load", Framebuffer_Main_Init);

		Cvar_Register(&fb_width);
		Cvar_Register(&fb_height);
		Cvar_Register(&fb_ratio_w);
		Cvar_Register(&fb_ratio_h);
		Cvar_Register(&fb_x);
		Cvar_Register(&fb_y);
		*/

		// FIXME: which group we belong?
		Cvar_Register(&framebuffer);
	}
}

//
// Disable drawing to a framebuffer.
//
void Framebuffer_Disable (fb_t *fbs)
{
	if (!framebuffer.value)
	{
		return;
	}

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}

//
// Enable drawing to a framebuffer.
//
void Framebuffer_Enable (fb_t *fbs)
{	
	if (!framebuffer.value)
	{
		return;
	}

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbs->frame_buffer);
}

//
// Create a framebuffer object.
//
void Framebuffer_Create (fb_t *fbs)
{
	GLenum status;
	extern RECT window_rect;
	
	// Frame buffers not supported.
	if (!use_framebuffer)
	{
		return;
	}

	// Set the texture specs.
	if (!fbs->texture)
	{
		extern int texture_extension_number;

		// Make sure the texture resolution is a power of 2.
		Q_ROUND_POWER2(window_rect.right - window_rect.left, fbs->width);
		Q_ROUND_POWER2(window_rect.bottom - window_rect.top, fbs->height);
		
		if (fbs->width < main_fb.height)
		{
			fbs->width = main_fb.height;
		}
		else
		{
			main_fb.height = main_fb.width;
		}

		fbs->texture	= texture_extension_number++;
		fbs->depthtex	= texture_extension_number++;
		fbs->realwidth	= (int)(window_rect.right - window_rect.left);
		fbs->realheight = (int)(window_rect.bottom - window_rect.top);
		fbs->ratio_h	= (float) fbs->realheight / fbs->height;
		fbs->ratio_w	= (float) fbs->realwidth  / fbs->width;
		fbs->x			= 0;
		fbs->y			= 0;
	}

	// Generate framebuffer.
	glGenFramebuffersEXT(1, &fbs->frame_buffer);
	
	// Generate depthbuffer.
	glGenRenderbuffersEXT(1, &fbs->depth_buffer);

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbs->frame_buffer);
	
	// Initializing the texture.
	glBindTexture(GL_TEXTURE_2D, fbs->texture);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, fbs->width, fbs->height, 0, GL_RGB, GL_FLOAT, NULL);
	glTexImage2D(GL_TEXTURE_2D, 0, 4, fbs->width, fbs->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, fbs->texture, 0);
	
	// Initializing the depthbuffer.
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, fbs->depth_buffer);
	glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, fbs->width, fbs->height);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, fbs->depth_buffer);

	status = glCheckFramebufferStatusEXT (GL_FRAMEBUFFER_EXT);
	switch (status) 
	{
		case GL_FRAMEBUFFER_COMPLETE_EXT:
			Com_Printf("Frame buffer configuration supported\n");
			break;
		case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
			Com_Printf("Frame buffer configuration unsupported\n");
			return ;
		default:
			Com_Printf("Error\n");
			return ;
	}

	// Unbind the frame buffer so that we dont draw to it accidentally.
	glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, 0);
}

//
// Initializes the main frame buffer object, which can be reused
// in several places. Use Framebuffer_Create(...) and create a new
// buffer for specialized use.
//
void Framebuffer_Main_Init (void)
{
	if (!use_framebuffer)
	{
		return;
	}

	if (main_fb.texture != 0)
	{
		return;
	}

	Framebuffer_Create(&main_fb);
}

//
// Draws the specified frame buffer object onto a polygon
// with the coordinates / bounds specified in it.
//
void Framebuffer_Draw (fb_t *fbs)
{
	if (!framebuffer.value)
	{
		return;
	}

	GL_Bind(fbs->texture);

	glBegin (GL_QUADS);

	// Top left corner.
	glTexCoord2f (0, 0);
	glVertex2f (fbs->x, fbs->y + fbs->realheight);

	// Upper right corner.
	glTexCoord2f (fbs->ratio_w, 0);
	glVertex2f (fbs->x + fbs->realwidth, fbs->y + fbs->realheight);

	// Bottom right corner.
	glTexCoord2f (fbs->ratio_w, fbs->ratio_h);
	glVertex2f (fbs->x + fbs->realwidth, fbs->y);

	// Bottom left corner.
	glTexCoord2f (0, fbs->ratio_h);
	glVertex2f (fbs->x, fbs->y);
	
	glEnd();
}
#endif // FRAMEBUFFERS & GLQUAKE

