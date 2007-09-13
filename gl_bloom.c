/*
Copyright (C) 1997-2001 Id Software, Inc.

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

/**
	
	GL Bloom

	Ported by Cokeman, June 2007
	last edit:
	$Id: gl_bloom.c,v 1.5 2007-09-13 14:49:30 disconn3ct Exp $

*/


// gl_bloom.c: 2D lighting post process effect

//#include "r_local.h"
#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"

/*
==============================================================================
 
                        LIGHT BLOOMS
 
==============================================================================
*/

static float Diamond8x[8][8] = { 
	{0.0f, 0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f, 0.0f}, 
	{0.0f, 0.0f, 0.2f, 0.3f, 0.3f, 0.2f, 0.0f, 0.0f}, 
	{0.0f, 0.2f, 0.4f, 0.6f, 0.6f, 0.4f, 0.2f, 0.0f}, 
	{0.1f, 0.3f, 0.6f, 0.9f, 0.9f, 0.6f, 0.3f, 0.1f}, 
	{0.1f, 0.3f, 0.6f, 0.9f, 0.9f, 0.6f, 0.3f, 0.1f}, 
	{0.0f, 0.2f, 0.4f, 0.6f, 0.6f, 0.4f, 0.2f, 0.0f}, 
	{0.0f, 0.0f, 0.2f, 0.3f, 0.3f, 0.2f, 0.0f, 0.0f}, 
	{0.0f, 0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f, 0.0f} };

static float Diamond6x[6][6] = { 
	{0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f}, 
	{0.0f, 0.3f, 0.5f, 0.5f, 0.3f, 0.0f},  
	{0.1f, 0.5f, 0.9f, 0.9f, 0.5f, 0.1f}, 
	{0.1f, 0.5f, 0.9f, 0.9f, 0.5f, 0.1f}, 
	{0.0f, 0.3f, 0.5f, 0.5f, 0.3f, 0.0f}, 
	{0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f} };

static float Diamond4x[4][4] = {  
	{0.3f, 0.4f, 0.4f, 0.3f},  
	{0.4f, 0.9f, 0.9f, 0.4f}, 
	{0.4f, 0.9f, 0.9f, 0.4f}, 
	{0.3f, 0.4f, 0.4f, 0.3f} };

static int BLOOM_SIZE;

cvar_t      r_bloom = {"r_bloom", "0", true};
cvar_t      r_bloom_alpha = {"r_bloom_alpha", "0.5", true};
cvar_t      r_bloom_diamond_size = {"r_bloom_diamond_size", "8", true};
cvar_t      r_bloom_intensity = {"r_bloom_intensity", "1", true};
cvar_t      r_bloom_darken = {"r_bloom_darken", "3", true};
cvar_t      r_bloom_sample_size = {"r_bloom_sample_size", "256", true};
cvar_t      r_bloom_fast_sample = {"r_bloom_fast_sample", "0", true};

int r_bloomscreentexture;
int r_bloomeffecttexture;
int r_bloombackuptexture;
int r_bloomdownsamplingtexture;

static int      r_screendownsamplingtexture_size;
static int      screen_texture_width, screen_texture_height;
static int      r_screenbackuptexture_size;

// Current refdef size:
static int  curView_x;
static int  curView_y;
static int  curView_width;
static int  curView_height;

// Texture coordinates of screen data inside screentexture.
static float screenText_tcw;
static float screenText_tch;

static int  sample_width;
static int  sample_height;

// Texture coordinates of adjusted textures.
static float sampleText_tcw;
static float sampleText_tch;

// This macro is in sample size workspace coordinates.
#define R_Bloom_SamplePass( xpos, ypos )                           \
	glBegin(GL_QUADS);                                             \
	glTexCoord2f(  0,                      sampleText_tch);        \
	glVertex2f(    xpos,                   ypos);                  \
	glTexCoord2f(  0,                      0);                     \
	glVertex2f(    xpos,                   ypos+sample_height);    \
	glTexCoord2f(  sampleText_tcw,         0);                     \
	glVertex2f(    xpos+sample_width,      ypos+sample_height);    \
	glTexCoord2f(  sampleText_tcw,         sampleText_tch);        \
	glVertex2f(    xpos+sample_width,      ypos);                  \
	glEnd();

#define R_Bloom_Quad( x, y, width, height, textwidth, textheight ) \
	glBegin(GL_QUADS);                                             \
	glTexCoord2f(  0,          textheight);                        \
	glVertex2f(    x,          y);                                 \
	glTexCoord2f(  0,          0);                                 \
	glVertex2f(    x,          y+height);                          \
	glTexCoord2f(  textwidth,  0);                                 \
	glVertex2f(    x+width,    y+height);                          \
	glTexCoord2f(  textwidth,  textheight);                        \
	glVertex2f(    x+width,    y);                                 \
	glEnd();

//=================
// R_Bloom_InitBackUpTexture
// =================
void R_Bloom_InitBackUpTexture( int width, int height )
{
	unsigned char *data;
	
	data = (unsigned char *) Q_calloc (width * height, sizeof (int));

	r_screenbackuptexture_size = width;

	r_bloombackuptexture = GL_LoadTexture ("***r_bloombackuptexture***", width, height, data, 0, 4);

	Q_free (data);
}

// =================
// R_Bloom_InitEffectTexture
// =================
void R_Bloom_InitEffectTexture (void)
{
	unsigned char *data;
	float bloomsizecheck;

	if (r_bloom_sample_size.value < 32)
		Cvar_SetValue (&r_bloom_sample_size, 32);

	// Make sure bloom size is a power of 2.
	BLOOM_SIZE = r_bloom_sample_size.value;
	bloomsizecheck = (float) BLOOM_SIZE;

	while (bloomsizecheck > 1.0f)
		bloomsizecheck /= 2.0f;

	if (bloomsizecheck != 1.0f)
	{
		BLOOM_SIZE = 32;

		while (BLOOM_SIZE < r_bloom_sample_size.value)
			BLOOM_SIZE *= 2;
	}

	// Make sure bloom size doesn't have stupid values.
	if (BLOOM_SIZE > screen_texture_width || BLOOM_SIZE > screen_texture_height)
		BLOOM_SIZE = min( screen_texture_width, screen_texture_height );

	if (BLOOM_SIZE != r_bloom_sample_size.value)
		Cvar_SetValue (&r_bloom_sample_size, BLOOM_SIZE);

	data = (unsigned char *) Q_calloc (BLOOM_SIZE * BLOOM_SIZE, sizeof (int));

	r_bloomeffecttexture = GL_LoadTexture ("***r_bloomeffecttexture***", BLOOM_SIZE, BLOOM_SIZE, data, 0, 4);

	Q_free (data);
}

// =================
// R_Bloom_InitTextures
// =================
extern int texture_extension_number;
void R_Bloom_InitTextures( void )
{
	unsigned char *data;
	int maxtexsize;
	size_t size;

	// Find closer power of 2 to screen size.
	for (screen_texture_width = 1;  screen_texture_width  < glwidth;  screen_texture_width *= 2);
	for (screen_texture_height = 1; screen_texture_height < glheight; screen_texture_height *= 2);

    // Disable blooms if we can't handle a texture of that size.
	glGetIntegerv (GL_MAX_TEXTURE_SIZE, &maxtexsize);
	if (screen_texture_width > maxtexsize || screen_texture_height > maxtexsize)
	{
		screen_texture_width = screen_texture_height = 0;
		Cvar_SetValue (&r_bloom, 0);
		Com_Printf ("WARNING: 'R_InitBloomScreenTexture' too high resolution for Light Bloom. Effect disabled\n");
		return;
	}

	// Init the screen texture.
	size = screen_texture_width * screen_texture_height * sizeof (int);
	data = Q_malloc (size);
	memset (data, 255, size);
	//r_bloomscreentexture = GL_LoadTexture ( "***r_screenbackuptexture***", screen_texture_width, screen_texture_height, data, 0, 4); // false, false, 4);
	
	if (!r_bloomscreentexture)
		r_bloomscreentexture = texture_extension_number++;

	GL_Bind (r_bloomscreentexture);
	glTexImage2D (GL_TEXTURE_2D, 0, gl_solid_format, screen_texture_width, screen_texture_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	Q_free (data);

	// Validate bloom size and init the bloom effect texture.
	R_Bloom_InitEffectTexture();

	// If screensize is more than 2x the bloom effect texture, set up for stepped downsampling.
	r_bloomdownsamplingtexture = 0;
	r_screendownsamplingtexture_size = 0;
	if( glwidth > (BLOOM_SIZE * 2) && !r_bloom_fast_sample.value )
	{
		r_screendownsamplingtexture_size = (int)(BLOOM_SIZE * 2);
		data = Q_calloc (r_screendownsamplingtexture_size * r_screendownsamplingtexture_size, sizeof (int));
		r_bloomdownsamplingtexture = GL_LoadTexture ( "***r_bloomdownsamplingtexture***", r_screendownsamplingtexture_size, r_screendownsamplingtexture_size, data, 0, 4);
		Q_free (data);
	}

	// Init the screen backup texture.
	if (r_screendownsamplingtexture_size)
		R_Bloom_InitBackUpTexture (r_screendownsamplingtexture_size, r_screendownsamplingtexture_size);
	else
		R_Bloom_InitBackUpTexture (BLOOM_SIZE, BLOOM_SIZE);
}

// =================
// R_InitBloomTextures
// =================
void R_InitBloomTextures( void )
{
	BLOOM_SIZE = 0;
	if( !r_bloom.value )
	{
		return;
	}

	r_bloomscreentexture = 0;	// This came from a vid_restart, where none of the textures are valid any more.
	R_Bloom_InitTextures ();
}

// =================
// R_Bloom_DrawEffect
// =================
void R_Bloom_DrawEffect( void )
{
	GL_Bind(r_bloomeffecttexture);

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glColor4f(r_bloom_alpha.value, r_bloom_alpha.value, r_bloom_alpha.value, 1.0f);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBegin(GL_QUADS);                         
	glTexCoord2f(  0,                          sampleText_tch  ); 
	glVertex2f(    curView_x,                  curView_y   );             
	glTexCoord2f(  0,                          0   );             
	glVertex2f(    curView_x,                  curView_y + curView_height  ); 
	glTexCoord2f(  sampleText_tcw,             0   );             
	glVertex2f(    curView_x + curView_width,  curView_y + curView_height  ); 
	glTexCoord2f(  sampleText_tcw,             sampleText_tch  ); 
	glVertex2f(    curView_x + curView_width,  curView_y   );             
	glEnd();

	glDisable(GL_BLEND);
}

#if 0
// =================
// R_Bloom_GeneratexCross - alternative bluring method
// =================
void R_Bloom_GeneratexCross( void )
{
	int         i;
	static int      BLOOM_BLUR_RADIUS = 8;
	//static float  BLOOM_BLUR_INTENSITY = 2.5f;
	float   BLOOM_BLUR_INTENSITY;
	static float intensity;
	static float range;

	// Setup sample size workspace.
	glViewport( 0, 0, sample_width, sample_height );
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity ();
	glOrtho(0, sample_width, sample_height, 0, -10, 100);
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity ();

	// Copy small scene into r_bloomeffecttexture.
	GL_Bind(r_bloomeffecttexture);
	//glBindTexture(GL_TEXTURE_2D, r_bloomeffecttexture);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height);

	// Start modifying the small scene corner.
	glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	glEnable(GL_BLEND);

	// Darkening passes.
	if( r_bloom_darken.value )
	{
		glBlendFunc(GL_DST_COLOR, GL_ZERO);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		for(i=0; i<r_bloom_darken.integer ;i++) 
		{
			R_Bloom_SamplePass( 0, 0 );
		}
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height);
	}

    // Bluring passes.
	if( BLOOM_BLUR_RADIUS ) 
	{
		glBlendFunc(GL_ONE, GL_ONE);

		range = (float)BLOOM_BLUR_RADIUS;

		BLOOM_BLUR_INTENSITY = r_bloom_intensity.value;
		//diagonal-cross draw 4 passes to add initial smooth
		glColor4f( 0.5f, 0.5f, 0.5f, 1.0);
		R_Bloom_SamplePass( 1, 1 );
		R_Bloom_SamplePass( -1, 1 );
		R_Bloom_SamplePass( -1, -1 );
		R_Bloom_SamplePass( 1, -1 );
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height);

		for(i =- (BLOOM_BLUR_RADIUS + 1); i <BLOOM_BLUR_RADIUS; i++) 
		{
			intensity = BLOOM_BLUR_INTENSITY/(range*2+1)*(1 - fabs(i*i)/(float)(range*range));
			if( intensity < 0.05f )
			{
				continue;
			}
			glColor4f( intensity, intensity, intensity, 1.0f);
			R_Bloom_SamplePass( i, 0 );
			//R_Bloom_SamplePass( -i, 0 );
		}

		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height);

		//for(i = 0; i < BLOOM_BLUR_RADIUS; i++)
		for(i =- (BLOOM_BLUR_RADIUS + 1); i < BLOOM_BLUR_RADIUS; i++) 
		{
			intensity = BLOOM_BLUR_INTENSITY / (range*2+1) * (1 - fabs(i * i) / (float)(range * range));
			if( intensity < 0.05f )
			{
				continue;
			}
			glColor4f( intensity, intensity, intensity, 1.0f);
			R_Bloom_SamplePass( 0, i );
			//R_Bloom_SamplePass( 0, -i );
		}

		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height);
	}

	// Restore full screen workspace.
	glViewport( 0, 0, glwidth, glheight );
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity ();
	glOrtho(0, glwidth, glheight, 0, -10, 100);
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity ();
}
#endif

// =================
// R_Bloom_GeneratexDiamonds
//=================
void R_Bloom_GeneratexDiamonds( void )
{
	int         i, j;
	static float intensity;

	// Setup sample size workspace
	glViewport( 0, 0, sample_width, sample_height );
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity ();
	glOrtho(0, sample_width, sample_height, 0, -10, 100);
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity ();

	// Copy small scene into r_bloomeffecttexture.
	GL_Bind(r_bloomeffecttexture);
	//glBindTexture(GL_TEXTURE_2D, r_bloomeffecttexture);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height);

	// Start modifying the small scene corner.
	glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	glEnable(GL_BLEND);

    // Darkening passes
	if( r_bloom_darken.value )
	{
		glBlendFunc(GL_DST_COLOR, GL_ZERO);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		for(i=0; i < r_bloom_darken.integer ;i++) 
		{
			R_Bloom_SamplePass( 0, 0 );
		}
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height);
	}

    // Bluring passes.
    //glBlendFunc(GL_ONE, GL_ONE);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
   
    if( r_bloom_diamond_size.value > 7 || r_bloom_diamond_size.value <= 3)
	{
		if( r_bloom_diamond_size.integer != 8 ) 
		{
			Cvar_SetValue( &r_bloom_diamond_size, 8 );
		}

		for(i = 0; i < r_bloom_diamond_size.integer; i++) 
		{
			for(j = 0; j < r_bloom_diamond_size.integer; j++) 
			{
				intensity = r_bloom_intensity.value * 0.3 * Diamond8x[i][j];
				if( intensity < 0.01f )
				{
					continue;
				}
				glColor4f( intensity, intensity, intensity, 1.0);
				R_Bloom_SamplePass( i - 4, j - 4 );
			}
		}
	} 
	else if( r_bloom_diamond_size.integer > 5 ) 
	{
		if( r_bloom_diamond_size.integer != 6 ) 
		{
			Cvar_SetValue( &r_bloom_diamond_size, 6 );
		}

		for(i = 0; i < r_bloom_diamond_size.integer; i++) 
		{
			for(j = 0; j < r_bloom_diamond_size.integer; j++) 
			{
				intensity = r_bloom_intensity.value * 0.5 * Diamond6x[i][j];
				if( intensity < 0.01f ) 
				{
					continue;
				}
				glColor4f( intensity, intensity, intensity, 1.0);
				R_Bloom_SamplePass( i - 3, j - 3 );
			}
		}
	} 
	else if( r_bloom_diamond_size.value > 3 ) 
	{
		if( r_bloom_diamond_size.integer != 4 )
		{
			Cvar_SetValue( &r_bloom_diamond_size, 4 );
		}

		for(i = 0; i < r_bloom_diamond_size.integer; i++) 
		{
			for(j = 0; j < r_bloom_diamond_size.integer; j++) 
			{
				intensity = r_bloom_intensity.value * 0.8f * Diamond4x[i][j];
				if( intensity < 0.01f ) continue;
				glColor4f( intensity, intensity, intensity, 1.0);
				R_Bloom_SamplePass( i-2, j-2 );
			}
		}
	}
   
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height);

	// Restore full screen workspace.
	glViewport( 0, 0, glwidth, glheight );
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity ();
	glOrtho(0, glwidth, glheight, 0, -10, 100);
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity ();
}                                           

// =================
// R_Bloom_DownsampleView
// =================
void R_Bloom_DownsampleView( void )
{
	glDisable( GL_BLEND );
	glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );

	// Stepped downsample.
	if( r_screendownsamplingtexture_size )
	{
		int     midsample_width = r_screendownsamplingtexture_size * sampleText_tcw;
		int     midsample_height = r_screendownsamplingtexture_size * sampleText_tch;

		// Copy the screen and draw resized.
		GL_Bind(r_bloomscreentexture);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, curView_x, glheight - (curView_y + curView_height), curView_width, curView_height);
		R_Bloom_Quad( 0,  glheight - midsample_height, midsample_width, midsample_height, screenText_tcw, screenText_tch  );

		// Now copy into Downsampling (mid-sized) texture.
		GL_Bind(r_bloomdownsamplingtexture);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, midsample_width, midsample_height);

		// Now draw again in bloom size.
		glColor4f( 0.5f, 0.5f, 0.5f, 1.0f );
		R_Bloom_Quad( 0,  glheight - sample_height, sample_width, sample_height, sampleText_tcw, sampleText_tch );

		// Now blend the big screen texture into the bloom generation space (hoping it adds some blur).
		glEnable( GL_BLEND );
		glBlendFunc(GL_ONE, GL_ONE);
		glColor4f( 0.5f, 0.5f, 0.5f, 1.0f );
		GL_Bind(r_bloomscreentexture);
		R_Bloom_Quad( 0,  glheight - sample_height, sample_width, sample_height, screenText_tcw, screenText_tch );
		glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
		glDisable( GL_BLEND );
	} 
	else
	{    
		// Downsample simple.
		GL_Bind(r_bloomscreentexture);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, curView_x, glheight - (curView_y + curView_height), curView_width, curView_height);
		R_Bloom_Quad( 0, glheight - sample_height, sample_width, sample_height, screenText_tcw, screenText_tch );
	}
}

// =================
// R_BloomBlend
// =================
void R_BloomBlend (void)
{
	extern vrect_t	scr_vrect;

	if( !r_bloom.value )
	{
		return;
	}

	if( !BLOOM_SIZE || screen_texture_width < glwidth || screen_texture_height < glheight)
	{
		R_Bloom_InitTextures();
	}

	if( screen_texture_width < BLOOM_SIZE ||
		screen_texture_height < BLOOM_SIZE )
	{
		return;
	}

	// Set up full screen workspace.
	glViewport( 0, 0, glwidth, glheight );
	glDisable( GL_DEPTH_TEST );
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity ();
	glOrtho(0, glwidth, glheight, 0, -10, 100);
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity ();
	glDisable(GL_CULL_FACE);

	glDisable( GL_BLEND );
	glEnable( GL_TEXTURE_2D );

	glColor4f( 1, 1, 1, 1 );

	// Setup current sizes
	curView_x = scr_vrect.x * ((float)glwidth / vid.width);
	curView_y = scr_vrect.y * ((float)glheight / vid.height);
	curView_width = scr_vrect.width * ((float)glwidth / vid.width);
	curView_height = scr_vrect.height * ((float)glheight / vid.height);
	screenText_tcw = ((float)curView_width / (float)screen_texture_width);
	screenText_tch = ((float)curView_height / (float)screen_texture_height);
	
	if( scr_vrect.height > scr_vrect.width ) 
	{
		sampleText_tcw = ((float)scr_vrect.width / (float)scr_vrect.height);
		sampleText_tch = 1.0f;
	} 
	else 
	{
		sampleText_tcw = 1.0f;
		sampleText_tch = ((float)scr_vrect.height / (float)scr_vrect.width);
	}

	sample_width = BLOOM_SIZE * sampleText_tcw;
	sample_height = BLOOM_SIZE * sampleText_tch;

	// Copy the screen space we'll use to work into the backup texture.
	GL_Bind(r_bloombackuptexture);
	//glBindTexture(GL_TEXTURE_2D, r_bloombackuptexture);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, r_screenbackuptexture_size * sampleText_tcw, r_screenbackuptexture_size * sampleText_tch);

	// Create the bloom image.
	R_Bloom_DownsampleView();
	R_Bloom_GeneratexDiamonds();
	//R_Bloom_GeneratexCross();

	// Restore the screen-backup to the screen.
	glDisable(GL_BLEND);
	GL_Bind(r_bloombackuptexture);
	glColor4f( 1, 1, 1, 1 );
	R_Bloom_Quad( 0,
		glheight - (r_screenbackuptexture_size * sampleText_tch),
		r_screenbackuptexture_size * sampleText_tcw,
		r_screenbackuptexture_size * sampleText_tch,
		sampleText_tcw,
		sampleText_tch );

	R_Bloom_DrawEffect();

	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}







