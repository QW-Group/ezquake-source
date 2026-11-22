/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2016      Spike

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

// Scripted particles: the r_part config parser and effect type defaults.

#include <limits.h>
#include "quakedef.h"
#include "r_part_script_local.h"

// The longest config statement, and the size of the tokenizer's argument
// buffer: a longer line could not be tokenized whole anyway.
#define PSCRIPT_MAX_LINE 1024

// Config numbers are clamped to this magnitude. It is far beyond any
// meaningful particle parameter and keeps every value convertible to int.
#define PSCRIPT_MAX_NUMBER 1e9

// Palette indices above 255 select index & 255 at half alpha, so a larger
// colorindex is reduced to 256 plus its low byte. colorrand is bounded to keep
// every colorindex + colorrand sum, with the tracer offset, within int.
#define PSCRIPT_MAX_COLORINDEX 0x1ff
#define PSCRIPT_MAX_COLORRAND (INT_MAX - 0x400)

// How many "r_part +name" blocks can extend one effect.
#define PSCRIPT_MAX_CHAIN 64

// Effect blocks read per file. Each looks its name up among all types, so a
// huge file would stall loading even once the type table is full.
#define PSCRIPT_MAX_BLOCKS 4096

// Only stops hostile files from growing the ramp without bound.
#define PSCRIPT_MAX_RAMP 256

// One sound is picked by weight each time the effect spawns.
#define PSCRIPT_MAX_SOUNDS 16

// Cells per atlas axis, and the bound for texture coordinates and strides in
// texture widths; keeps cell indices from overflow.
#define PSCRIPT_MAX_ATLAS 1024

// Random texture slots: every cell of the largest atlas.
#define PSCRIPT_MAX_SLOTS (PSCRIPT_MAX_ATLAS * PSCRIPT_MAX_ATLAS)

typedef struct pscript_reader_s {
	const char *config; // set name, for messages
	const char *data;
	size_t size;
	size_t offset;
	char *rest; // unread part of line, NULL when it is used up
	char line[PSCRIPT_MAX_LINE];
} pscript_reader_t;

// State of the r_part block being parsed. The type is held by index because
// creating a type reallocates pscript_types.
typedef struct pscript_blockparse_s {
	pscript_reader_t *reader;
	const char *config;
	int pnum;
	int rampcapacity;
	int soundcapacity;
	qbool settype;
	qbool setalphadelta;
	qbool setbeamlen;
	qbool shaderbody;
} pscript_blockparse_t;

typedef struct pscript_name_s {
	const char *name;
	int value;
} pscript_name_t;

static const pscript_name_t pscript_looktypes[] = {
	{ "beam", PT_BEAM },
	{ "vbeam", PT_BEAM }, // beam variant for vertical textures
	{ "spark", PT_SPARK },
	{ "linespark", PT_SPARK },
	{ "sparkfan", PT_SPARKFAN },
	{ "trianglefan", PT_SPARKFAN },
	{ "texturedspark", PT_TEXTUREDSPARK },
	{ "decal", PT_CDECAL },
	{ "cdecal", PT_CDECAL },
	{ "udecal", PT_UDECAL },
	{ "normal", PT_NORMAL },
};

static const pscript_name_t pscript_spawnmodes[] = {
	{ "box", SM_BOX },
	{ "circle", SM_CIRCLE },
	{ "ball", SM_BALL },
	{ "spiral", SM_SPIRAL },
	{ "tracer", SM_TRACER },
	{ "telebox", SM_TELEBOX },
	{ "lavasplash", SM_LAVASPLASH },
	{ "uniformcircle", SM_UNICIRCLE },
	{ "syncfield", SM_FIELD },
	{ "distball", SM_DISTBALL },
};

static const pscript_name_t pscript_rampmodes[] = {
	{ "none", RAMP_NONE },
	{ "absolute", RAMP_NEAREST },
	{ "nearest", RAMP_NEAREST },
	{ "lerp", RAMP_LERP },
	{ "delta", RAMP_DELTA },
};

static const struct {
	const char *name;
	pscript_blendmode_t blendmode;
	int premul;
} pscript_blends[] = {
	{ "adda", BM_ADDA, 0 },
	{ "add", BM_ADDA, 0 },
	{ "addc", BM_ADDC, 0 },
	{ "subtract", BM_SUBTRACT, 0 },
	{ "invmoda", BM_INVMODA, 0 },
	{ "invmod", BM_INVMODA, 0 },
	{ "invmodc", BM_INVMODC, 0 },
	{ "blendcolour", BM_BLENDCOLOUR, 0 },
	{ "blendcolor", BM_BLENDCOLOUR, 0 },
	{ "blendalpha", BM_BLEND, 0 },
	{ "blend", BM_BLEND, 0 },
	{ "premul_subtract", BM_INVMODC, 1 },
	{ "premul_add", BM_PREMUL, 2 },
	{ "premul_blend", BM_PREMUL, 1 },
	// rtsmoke is meant to be lit by realtime lights; unlit, it is premul_blend.
	{ "rtsmoke", BM_PREMUL, 1 },
};

// Keywords for features ezQuake lacks, accepted so configs that use them load
// without a warning per line.
static const char *pscript_unsupportedfields[] = {
	"model",
	"viewspace",
	"spawnstain",
	"stains",
	"lightcorona",
	"lightscales",
	"lightcubemap",
	"lightshadows",
	"lightstyle",
	"placeholder",
};

static float PScript_ClampNumber(double v)
{
	if (!isfinite(v)) {
		return 0;
	}
	return bound(-PSCRIPT_MAX_NUMBER, v, PSCRIPT_MAX_NUMBER);
}

static float PScript_ParseNumber(const char *s)
{
	return PScript_ClampNumber(atof(s));
}

// 0 when den is 0.
static float PScript_Divide(float num, float den)
{
	if (den == 0) {
		return 0;
	}
	return PScript_ClampNumber((double)num / den);
}

static int PScript_ParseInt(const char *s, int base)
{
	long v = strtol(s, NULL, base);

	return (int)bound(-INT_MAX, v, INT_MAX);
}

static float PScript_ArgFloat(tokenizecontext_t *ctx, int arg)
{
	return PScript_ParseNumber(Cmd_ArgvEx(ctx, arg));
}

// -1 if name isn't there.
static int PScript_FindName(const pscript_name_t *names, int count, const char *name)
{
	int i;

	for (i = 0; i < count; i++) {
		if (!strcmp(names[i].name, name)) {
			return names[i].value;
		}
	}
	return -1;
}

// Grows array to hold one element more than count. Callers bound count.
static void *PScript_GrowArray(void *array, int count, int *capacity, size_t elementsize)
{
	if (count < *capacity) {
		return array;
	}
	*capacity = (*capacity ? *capacity * 2 : 4);
	return Q_realloc(array, *capacity * elementsize);
}

// Copies the next line, with its '\n', into buffer, truncated to fit, and
// moves *offset past the whole line. Returns NULL at the end of the data.
char *PScript_ReadLine(char *buffer, size_t buffersize, const char *filedata, size_t filesize, size_t *offset)
{
	const char *start, *end, *f;
	size_t length;

	if (*offset >= filesize) {
		return NULL;
	}

	start = f = filedata + *offset;
	end = filedata + filesize;
	while (f < end) {
		if (*f++ == '\n') {
			break;
		}
	}
	*offset = f - filedata;

	length = min((size_t)(f - start), buffersize - 1);
	memcpy(buffer, start, length);
	buffer[length] = 0;
	return buffer;
}

// A line too long for the buffer is skipped: cut short, its last statement
// could mean something else.
static qbool PScript_NextLine(pscript_reader_t *r)
{
	size_t start, length;

	for (;;) {
		start = r->offset;
		if (!PScript_ReadLine(r->line, sizeof(r->line), r->data, r->size, &r->offset)) {
			return false;
		}

		length = r->offset - start;
		if (r->data[r->offset - 1] == '\n') {
			length--;
		}
		if (length < sizeof(r->line)) {
			r->rest = r->line;
			return true;
		}
		Con_DPrintf("%s: skipped a line longer than %d characters\n", r->config, (int)sizeof(r->line) - 1);
	}
}

// Returns the next non-empty statement, or NULL at the end of the data.
// Statements end at a line break or, unless wholeline is set, at a ';' or
// "//" outside quotes, where "/*...*/" within the line is also skipped. Both
// comments only start a word. Valid until the next call; assigning it to
// r->rest gives it back to be read again.
static char *PScript_NextStatement(pscript_reader_t *r, qbool wholeline)
{
	char *s, *p;
	qbool quoted;

	for (;;) {
		if (!r->rest && !PScript_NextLine(r)) {
			return NULL;
		}

		s = r->rest;
		r->rest = NULL;
		while (*s && (unsigned char)*s <= ' ') {
			s++;
		}

		if (!wholeline) {
			quoted = false;
			for (p = s; *p; p++) {
				if (*p == '"') {
					quoted = !quoted;
				}
				else if (quoted) {
					continue;
				}
				else if (*p == ';') {
					*p = 0;
					r->rest = p + 1;
					break;
				}
				else if (p[0] == '/' && p[1] == '/' && (p == s || (unsigned char)p[-1] <= ' ')) {
					*p = 0;
					break;
				}
				else if (p[0] == '/' && p[1] == '*' && (p == s || (unsigned char)p[-1] <= ' ')) {
					char *end = strstr(p + 2, "*/");

					// Unclosed, it runs to the end of the line.
					if (!end) {
						*p = 0;
						break;
					}
					memset(p, ' ', end + 2 - p);
					p = end + 1;
				}
			}
			while (*s && (unsigned char)*s <= ' ') {
				s++;
			}
		}

		if (*s) {
			return s;
		}
	}
}

// The block's '{' was read.
static void PScript_SkipBlock(pscript_reader_t *r)
{
	int depth = 1;
	char *s;

	while (depth > 0 && (s = PScript_NextStatement(r, false))) {
		if (*s == '{') {
			depth++;
		}
		else if (*s == '}') {
			depth--;
		}
	}
}

// Skips the inline shader body that may follow a shader line, reading whole
// lines because shader text uses ';' itself. Returns whether there was one.
static qbool PScript_SkipShaderBody(pscript_reader_t *r, const char *effectname)
{
	int depth = 1;
	char *s;

	s = PScript_NextStatement(r, true);
	if (!s) {
		return false;
	}
	if (*s != '{') {
		r->rest = s;
		return false;
	}

	while (depth > 0) {
		s = PScript_NextStatement(r, true);
		if (!s) {
			Con_Printf("Unexpected end of buffer with effect %s\n", effectname);
			break;
		}
		if (*s == '}') {
			depth--;
		}
		else if (*s == '{') {
			depth++;
		}
	}
	return true;
}

// Returns the index of type name in config, creating it if needed, or
// P_INVALID if linking from to it would form an assoc loop or the type table
// is full. May reallocate pscript_types.
static int CheckAssociation(const char *config, const char *name, int from)
{
	int to, orig, steps;

	orig = to = PScript_AllocateParticleType(config, name);

	// Every node reachable through assoc is visited within pscript_numtypes
	// steps, even when other files have left a loop elsewhere.
	for (steps = 0; PART_VALID(to) && steps < pscript_numtypes; steps++) {
		if (to == from) {
			Con_Printf("Association of %s would cause an infinite loop\n", name);
			return P_INVALID;
		}
		to = pscript_types[to].assoc;
	}
	return orig;
}

// Reads "min [max]" starting at arg, scaled by scale, as min and the random
// extra max - min, which is 0 without max.
static void PScript_ParseRange(tokenizecontext_t *ctx, int arg, float scale, float *minvalue, float *extra)
{
	*minvalue = PScript_ArgFloat(ctx, arg) * scale;
	*extra = (Cmd_ArgcEx(ctx) > arg + 1 ? PScript_ArgFloat(ctx, arg + 1) * scale - *minvalue : 0);
}

// "<all>" or "<r> <g> <b>".
static void PScript_ParseColour(tokenizecontext_t *ctx, vec3_t out, float divisor)
{
	out[0] = out[1] = out[2] = PScript_ArgFloat(ctx, 1) / divisor;
	if (Cmd_ArgcEx(ctx) > 3) {
		out[1] = PScript_ArgFloat(ctx, 2) / divisor;
		out[2] = PScript_ArgFloat(ctx, 3) / divisor;
	}
}

static void PScript_ParseVector(tokenizecontext_t *ctx, vec3_t out)
{
	out[0] = PScript_ArgFloat(ctx, 1);
	out[1] = PScript_ArgFloat(ctx, 2);
	out[2] = PScript_ArgFloat(ctx, 3);
}

// Colour deltas apply for the particle's whole life unless rgbdeltatime said
// otherwise.
static void PScript_DefaultChangeTime(pscript_type_t *ptype)
{
	if (!ptype->rgbchangetime) {
		ptype->rgbchangetime = ptype->die;
	}
}

// underwater and notunderwater: the contents the type spawns in or avoids.
static void PScript_ParseFluids(pscript_type_t *ptype, tokenizecontext_t *ctx, unsigned int flag)
{
	const char *name;
	int i;

	ptype->flags |= flag;
	if ((ptype->flags & (PT_TRUNDERWATER | PT_TROVERWATER)) == (PT_TRUNDERWATER | PT_TROVERWATER)) {
		ptype->flags &= ~PT_TRUNDERWATER;
		Con_Printf("%s.%s: both over and under water\n", ptype->config, ptype->name);
	}

	if (Cmd_ArgcEx(ctx) == 1) {
		ptype->fluidmask = PSCRIPT_CONTENTS_FLUID;
		return;
	}

	ptype->fluidmask = 0;
	for (i = 1; i < Cmd_ArgcEx(ctx); i++) {
		name = Cmd_ArgvEx(ctx, i);
		if (!strcmp(name, "water")) {
			ptype->fluidmask |= PSCRIPT_CONTENTS_WATER;
		}
		else if (!strcmp(name, "slime")) {
			ptype->fluidmask |= PSCRIPT_CONTENTS_SLIME;
		}
		else if (!strcmp(name, "lava")) {
			ptype->fluidmask |= PSCRIPT_CONTENTS_LAVA;
		}
		else if (!strcmp(name, "sky")) {
			ptype->fluidmask |= PSCRIPT_CONTENTS_SKY;
		}
		else if (!strcmp(name, "fluid")) {
			ptype->fluidmask |= PSCRIPT_CONTENTS_FLUID;
		}
		else if (!strcmp(name, "solid")) {
			ptype->fluidmask |= PSCRIPT_CONTENTS_SOLID;
		}
		else if (!strcmp(name, "playerclip")) {
			ptype->fluidmask |= PSCRIPT_CONTENTS_PLAYERCLIP;
		}
		else if (strcmp(name, "none")) {
			Con_Printf("%s.%s: unknown contents: %s\n", ptype->config, ptype->name, name);
		}
	}
}

// sound <name> [vol [atten [pitch [delay [weight]]]]] or with key=value
// arguments. The sound isn't loaded here: the effect looks it up by name when
// it plays, so a config cannot fill the engine's sound table by itself.
static void PScript_ParseSound(pscript_blockparse_t *bp, pscript_type_t *ptype, tokenizecontext_t *ctx)
{
	const char *name = Cmd_ArgvEx(ctx, 1);
	const char *arg;
	char *end;
	pscript_sound_t *sound;
	int i;

	// An empty name is kept: with its weight it is a chance of silence.
	if (strlen(name) >= sizeof(sound->name)) {
		Con_DPrintf("%s.%s: bad sound name \"%s\"\n", ptype->config, ptype->name, name);
		return;
	}
	if (ptype->numsounds >= PSCRIPT_MAX_SOUNDS) {
		Con_DPrintf("%s.%s: more than %d sounds, %s ignored\n", ptype->config, ptype->name, PSCRIPT_MAX_SOUNDS, name);
		return;
	}

	ptype->sounds = PScript_GrowArray(ptype->sounds, ptype->numsounds, &bp->soundcapacity, sizeof(*ptype->sounds));
	sound = &ptype->sounds[ptype->numsounds++];
	strlcpy(sound->name, name, sizeof(sound->name));
	sound->vol = 1;
	sound->atten = 1;
	sound->weight = 0;

	// Positional only when the second argument is missing or an integer
	// throughout, so a fractional volume such as "0.5" selects the
	// named-argument path and the positional values are ignored.
	strtoul(Cmd_ArgvEx(ctx, 2), &end, 0);
	while (*end == ' ' || *end == '\t') {
		end++;
	}

	if (*end) {
		for (i = 2; i < Cmd_ArgcEx(ctx); i++) {
			arg = Cmd_ArgvEx(ctx, i);
			if (!strncasecmp(arg, "vol=", 4) || !strncasecmp(arg, "volume=", 7)) {
				sound->vol = PScript_ParseNumber(strchr(arg, '=') + 1);
			}
			else if (!strncasecmp(arg, "attn=", 5) || !strncasecmp(arg, "atten=", 6) || !strncasecmp(arg, "attenuation=", 12)) {
				arg = strchr(arg, '=') + 1;
				if (!strcmp(arg, "none")) {
					sound->atten = 0;
				}
				else if (!strcmp(arg, "normal")) {
					sound->atten = 1;
				}
				else {
					sound->atten = PScript_ParseNumber(arg);
				}
			}
			else if (!strncasecmp(arg, "weight=", 7)) {
				sound->weight = PScript_ParseNumber(strchr(arg, '=') + 1);
			}
			else if (strncasecmp(arg, "pitch=", 6) && strncasecmp(arg, "delay=", 6)) {
				Con_DPrintf("%s.%s: bad named argument: %s\n", ptype->config, ptype->name, arg);
			}
		}
	}
	else {
		// Pitch and delay, arguments 4 and 5, have no equivalent in ezQuake.
		sound->vol = PScript_ArgFloat(ctx, 2);
		if (!sound->vol) {
			sound->vol = 1;
		}
		sound->atten = PScript_ArgFloat(ctx, 3);
		if (!sound->atten) {
			sound->atten = 1;
		}
		sound->weight = PScript_ArgFloat(ctx, 6);
	}
	if (!sound->weight) {
		sound->weight = 1;
	}
}

// NULL when the ramp is full.
static pscript_ramp_t *PScript_AddRampEntry(pscript_blockparse_t *bp, pscript_type_t *ptype)
{
	pscript_ramp_t *ramp;

	if (ptype->rampindexes >= PSCRIPT_MAX_RAMP) {
		Con_DPrintf("%s.%s: ramp has more than %d entries\n", ptype->config, ptype->name, PSCRIPT_MAX_RAMP);
		return NULL;
	}

	ptype->ramp = PScript_GrowArray(ptype->ramp, ptype->rampindexes, &bp->rampcapacity, sizeof(*ptype->ramp));
	ramp = &ptype->ramp[ptype->rampindexes++];
	memset(ramp, 0, sizeof(*ramp));
	return ramp;
}

// Palette indices above 255 select the colour of index & 255 at half alpha.
static void PScript_SetRampPalette(pscript_ramp_t *ramp, int colorindex)
{
	const byte *rgba = (const byte *)&d_8to24table[(unsigned int)colorindex & 0xff];

	ramp->alpha = (colorindex > 255 ? 0.5 : 1);
	ramp->rgb[0] = rgba[0] * (1 / 255.0f);
	ramp->rgb[1] = rgba[1] * (1 / 255.0f);
	ramp->rgb[2] = rgba[2] * (1 / 255.0f);
}

// Alpha blending for unknown names.
static void PScript_ParseBlend(pscript_type_t *ptype, const char *value)
{
	int i;

	for (i = 0; i < (int)ARRAY_LEN(pscript_blends); i++) {
		if (!strcmp(pscript_blends[i].name, value)) {
			ptype->looks.blendmode = pscript_blends[i].blendmode;
			ptype->looks.premul = pscript_blends[i].premul;
			return;
		}
	}

	Con_DPrintf("%s.%s: uses unknown blend type '%s', assuming legacy 'blendalpha'\n", ptype->config, ptype->name, value);
	ptype->looks.blendmode = BM_BLEND;
	ptype->looks.premul = 0;
}

// spawnmode <mode> [param1 [param2]], box for unknown modes.
static void PScript_ParseSpawnMode(pscript_type_t *ptype, tokenizecontext_t *ctx)
{
	const char *value = Cmd_ArgvEx(ctx, 1);
	int mode = PScript_FindName(pscript_spawnmodes, (int)ARRAY_LEN(pscript_spawnmodes), value);

	if (mode < 0) {
		Con_DPrintf("%s.%s: uses unknown spawn type '%s', assuming 'box'\n", ptype->config, ptype->name, value);
		mode = SM_BOX;
	}
	ptype->spawnmode = (pscript_spawnmode_t)mode;

	// Legacy syncfield parameters, for configs that don't give them.
	if (ptype->spawnmode == SM_FIELD) {
		ptype->spawnparam1 = 16;
		ptype->spawnparam2 = 0;
	}

	if (Cmd_ArgcEx(ctx) > 2) {
		if (Cmd_ArgcEx(ctx) > 3) {
			ptype->spawnparam2 = PScript_ArgFloat(ctx, 3);
		}
		ptype->spawnparam1 = PScript_ArgFloat(ctx, 2);
	}
}

// Delta for unknown names.
static void PScript_ParseRampMode(pscript_type_t *ptype, const char *value)
{
	int mode = PScript_FindName(pscript_rampmodes, (int)ARRAY_LEN(pscript_rampmodes), value);

	if (mode < 0) {
		Con_DPrintf("%s.%s: uses unknown ramp mode '%s', assuming 'delta'\n", ptype->config, ptype->name, value);
		mode = RAMP_DELTA;
	}
	else if (!strcmp(value, "absolute")) {
		Con_DPrintf("%s.%s: 'rampmode absolute' is deprecated, use 'rampmode nearest'\n", ptype->config, ptype->name);
	}
	ptype->rampmode = (pscript_rampmode_t)mode;
}

static qbool PScript_IsUnsupportedField(const char *var)
{
	int i;

	for (i = 0; i < (int)ARRAY_LEN(pscript_unsupportedfields); i++) {
		if (!strcmp(pscript_unsupportedfields[i], var)) {
			return true;
		}
	}
	return false;
}

static void PScript_ParseField(pscript_blockparse_t *bp, tokenizecontext_t *ctx)
{
	pscript_type_t *ptype = &pscript_types[bp->pnum];
	const char *var = Cmd_ArgvEx(ctx, 0);
	const char *value = Cmd_ArgvEx(ctx, 1);
	int argc = Cmd_ArgcEx(ctx);
	pscript_ramp_t *ramp;
	int target, mode, i;

	if (!strcmp(var, "shader")) {
		// Without a shader system, the image named like the shader is the
		// closest look; an inline body is a look that can't be reproduced.
		strlcpy(ptype->texname, *value ? value : ptype->name, sizeof(ptype->texname));
		bp->shaderbody = PScript_SkipShaderBody(bp->reader, ptype->name);
	}
	else if (!strcmp(var, "texture") || !strcmp(var, "linear_texture") || !strcmp(var, "nearest_texture") || !strcmp(var, "nearesttexture")) {
		strlcpy(ptype->texname, value, sizeof(ptype->texname));
		ptype->looks.nearest = !strncmp(var, "nearest", 7);
		bp->shaderbody = false;
	}
	else if (!strcmp(var, "tcoords")) {
		float tscale = PScript_ArgFloat(ctx, 5);

		if (tscale <= 0) {
			tscale = 1;
		}
		ptype->s1 = PScript_Divide(PScript_ArgFloat(ctx, 1), tscale);
		ptype->t1 = PScript_Divide(PScript_ArgFloat(ctx, 2), tscale);
		ptype->s2 = PScript_Divide(PScript_ArgFloat(ctx, 3), tscale);
		ptype->t2 = PScript_Divide(PScript_ArgFloat(ctx, 4), tscale);

		// The stride is in tscale units like the coordinates. Left undivided,
		// an explicit stride would step whole textures, so every random slot
		// would show the first image.
		ptype->randsmax = PScript_ParseInt(Cmd_ArgvEx(ctx, 6), 10);
		ptype->texsstride = PScript_Divide(argc > 7 ? PScript_ArgFloat(ctx, 7) : 1, tscale);
		if (ptype->randsmax < 1 || ptype->texsstride == 0) {
			ptype->randsmax = 1;
		}
	}
	else if (!strcmp(var, "atlas")) {
		// atlas <cells per axis> <first> [last], cells numbered along rows.
		float cells = PScript_ArgFloat(ctx, 1);
		int dims, first, last;

		dims = (cells < 1 ? 1 : (cells > PSCRIPT_MAX_ATLAS ? PSCRIPT_MAX_ATLAS : (int)cells));
		first = PScript_ParseInt(Cmd_ArgvEx(ctx, 2), 10);
		first = bound(0, first, dims * dims - 1);
		last = PScript_ParseInt(Cmd_ArgvEx(ctx, 3), 10);
		last = bound(first, last, first + PSCRIPT_MAX_SLOTS - 1);

		ptype->s1 = (first % dims) / (float)dims;
		ptype->s2 = (first % dims + 1) / (float)dims;
		ptype->t1 = (first / dims) / (float)dims;
		ptype->t2 = (first / dims + 1) / (float)dims;
		ptype->randsmax = last - first + 1;
		ptype->texsstride = ptype->s2 - ptype->s1;
	}
	else if (!strcmp(var, "rotation")) {
		PScript_ParseRange(ctx, 1, M_PI / 180, &ptype->rotationstartmin, &ptype->rotationstartrand);
		PScript_ParseRange(ctx, 3, M_PI / 180, &ptype->rotationmin, &ptype->rotationrand);
	}
	else if (!strcmp(var, "rotationstart")) {
		PScript_ParseRange(ctx, 1, M_PI / 180, &ptype->rotationstartmin, &ptype->rotationstartrand);
	}
	else if (!strcmp(var, "rotationspeed")) {
		PScript_ParseRange(ctx, 1, M_PI / 180, &ptype->rotationmin, &ptype->rotationrand);
	}
	else if (!strcmp(var, "beamtexstep")) {
		// Beams keep their texture step in rotationstartmin; a step of 0,
		// which would be infinite, keeps the default.
		float step = PScript_ArgFloat(ctx, 1);

		if (step != 0) {
			ptype->rotationstartmin = PScript_Divide(1, step);
			ptype->rotationstartrand = 0;
			bp->setbeamlen = true;
		}
	}
	else if (!strcmp(var, "beamtexspeed")) {
		ptype->rotationmin = PScript_ArgFloat(ctx, 1);
	}
	else if (!strcmp(var, "scale")) {
		ptype->scale = PScript_ArgFloat(ctx, 1);
		if (argc > 2) {
			ptype->scalerand = PScript_ArgFloat(ctx, 2) - ptype->scale;
		}
	}
	else if (!strcmp(var, "scalerand")) {
		ptype->scalerand = PScript_ArgFloat(ctx, 1);
	}
	else if (!strcmp(var, "scalefactor")) {
		ptype->looks.scalefactor = PScript_ArgFloat(ctx, 1);
	}
	else if (!strcmp(var, "scaledelta")) {
		ptype->scaledelta = PScript_ArgFloat(ctx, 1);
	}
	else if (!strcmp(var, "stretchfactor")) {
		ptype->looks.stretch = PScript_ArgFloat(ctx, 1);
		ptype->looks.minstretch = (argc > 2 ? PScript_ArgFloat(ctx, 2) : 0);
	}
	else if (!strcmp(var, "step")) {
		// Distance between trail particles. A step of 0 would make the count
		// infinite; without a positive step no spacing applies.
		float spacing = PScript_ArgFloat(ctx, 1);

		ptype->countspacing = (spacing > 0 ? spacing : 0);
		ptype->count = (spacing > 0 ? PScript_Divide(1, spacing) : 0);
		if (argc > 2) {
			ptype->countrand = PScript_Divide(1, PScript_ArgFloat(ctx, 2));
		}
		if (argc > 3) {
			ptype->countextra = PScript_ArgFloat(ctx, 3);
		}
	}
	else if (!strcmp(var, "count")) {
		ptype->countspacing = 0;
		ptype->count = PScript_ArgFloat(ctx, 1);
		if (argc > 2) {
			ptype->countrand = PScript_ArgFloat(ctx, 2);
		}
		if (argc > 3) {
			ptype->countextra = PScript_ArgFloat(ctx, 3);
		}
	}
	else if (!strcmp(var, "rainfrequency")) {
		ptype->rainfrequency = PScript_ArgFloat(ctx, 1);
	}
	else if (!strcmp(var, "alpha")) {
		ptype->alpha = PScript_ArgFloat(ctx, 1);
		if (argc > 2) {
			ptype->alpharand = PScript_ArgFloat(ctx, 2) - ptype->alpha;
		}
		if (argc > 3) {
			ptype->alphachange = PScript_ArgFloat(ctx, 3);
			bp->setalphadelta = true;
		}
	}
	else if (!strcmp(var, "alpharand")) {
		ptype->alpharand = PScript_ArgFloat(ctx, 1);
	}
	else if (!strcmp(var, "alphachange")) {
		Con_DPrintf("%s.%s: alphachange is deprecated, use alphadelta\n", ptype->config, ptype->name);
		ptype->alphachange = PScript_ArgFloat(ctx, 1);
	}
	else if (!strcmp(var, "alphadelta")) {
		ptype->alphachange = PScript_ArgFloat(ctx, 1);
		bp->setalphadelta = true;
	}
	else if (!strcmp(var, "die")) {
		ptype->die = PScript_ArgFloat(ctx, 1);
		if (argc > 2) {
			float mn = ptype->die, mx = PScript_ArgFloat(ctx, 2);

			if (mn > mx) {
				mn = mx;
				mx = ptype->die;
			}
			ptype->die = mx;
			ptype->randdie = mx - mn;
		}
	}
	else if (!strcmp(var, "diesubrand")) {
		Con_DPrintf("%s.%s: diesubrand is deprecated, use die with two arguments\n", ptype->config, ptype->name);
		ptype->randdie = PScript_ArgFloat(ctx, 1);
	}
	else if (!strcmp(var, "randomvel")) {
		ptype->velbias[0] = ptype->velbias[1] = 0;
		ptype->velwrand[0] = ptype->velwrand[1] = PScript_ArgFloat(ctx, 1);
		if (argc > 3) {
			// "randomvel <horiz> <vertmin> <vertmax>": the random part is
			// +-1, so take half the range around its middle.
			ptype->velbias[2] = PScript_ArgFloat(ctx, 2);
			ptype->velwrand[2] = (PScript_ArgFloat(ctx, 3) - ptype->velbias[2]) / 2;
			ptype->velbias[2] += ptype->velwrand[2];
		}
		else if (argc > 2) {
			ptype->velwrand[2] = PScript_ArgFloat(ctx, 2);
			ptype->velbias[2] = 0;
		}
		else {
			ptype->velwrand[2] = ptype->velwrand[0];
			ptype->velbias[2] = 0;
		}
	}
	else if (!strcmp(var, "veladd")) {
		PScript_ParseRange(ctx, 1, 1, &ptype->veladd, &ptype->randomveladd);
	}
	else if (!strcmp(var, "orgadd")) {
		PScript_ParseRange(ctx, 1, 1, &ptype->orgadd, &ptype->randomorgadd);
	}
	else if (!strcmp(var, "orgbias")) {
		PScript_ParseVector(ctx, ptype->orgbias);
	}
	else if (!strcmp(var, "velbias")) {
		PScript_ParseVector(ctx, ptype->velbias);
	}
	else if (!strcmp(var, "orgwrand")) {
		PScript_ParseVector(ctx, ptype->orgwrand);
	}
	else if (!strcmp(var, "velwrand")) {
		PScript_ParseVector(ctx, ptype->velwrand);
	}
	else if (!strcmp(var, "friction")) {
		ptype->friction[0] = ptype->friction[1] = ptype->friction[2] = PScript_ArgFloat(ctx, 1);
		if (argc > 3) {
			ptype->friction[1] = PScript_ArgFloat(ctx, 2);
			ptype->friction[2] = PScript_ArgFloat(ctx, 3);
		}
		else if (argc > 2) {
			ptype->friction[2] = PScript_ArgFloat(ctx, 2);
		}
	}
	else if (!strcmp(var, "gravity")) {
		ptype->gravity = PScript_ArgFloat(ctx, 1);
	}
	else if (!strcmp(var, "flurry")) {
		ptype->flurry = PScript_ArgFloat(ctx, 1);
	}
	else if (!strcmp(var, "assoc")) {
		target = CheckAssociation(bp->config, value, bp->pnum);
		pscript_types[bp->pnum].assoc = target;
	}
	else if (!strcmp(var, "inwater")) {
		// The underwater switch happens once per spawn, so checking it like
		// an assoc keeps the replacement from leading back here.
		target = CheckAssociation(bp->config, value, bp->pnum);
		pscript_types[bp->pnum].inwater = target;
	}
	else if (!strcmp(var, "underwater")) {
		PScript_ParseFluids(ptype, ctx, PT_TRUNDERWATER);
	}
	else if (!strcmp(var, "notunderwater")) {
		PScript_ParseFluids(ptype, ctx, PT_TROVERWATER);
	}
	else if (!strcmp(var, "sound")) {
		PScript_ParseSound(bp, ptype, ctx);
	}
	else if (!strcmp(var, "colorindex")) {
		// Read as 32-bit unsigned stored in int: an index past INT_MAX is
		// negative and selects rgb, any other keeps its low byte.
		if (argc > 2) {
			ptype->colorrand = (int)(unsigned int)strtoul(Cmd_ArgvEx(ctx, 2), NULL, 0);
		}
		ptype->colorindex = (int)(unsigned int)strtoul(value, NULL, 0);
	}
	else if (!strcmp(var, "colorrand")) {
		ptype->colorrand = PScript_ParseInt(value, 10);
	}
	else if (!strcmp(var, "citracer")) {
		ptype->flags |= PT_CITRACER;
	}
	else if (!strcmp(var, "red")) {
		ptype->rgb[0] = PScript_ArgFloat(ctx, 1) / 255;
	}
	else if (!strcmp(var, "green")) {
		ptype->rgb[1] = PScript_ArgFloat(ctx, 1) / 255;
	}
	else if (!strcmp(var, "blue")) {
		ptype->rgb[2] = PScript_ArgFloat(ctx, 1) / 255;
	}
	else if (!strcmp(var, "rgb")) {
		PScript_ParseColour(ctx, ptype->rgb, 255);
	}
	else if (!strcmp(var, "rgbf")) {
		PScript_ParseColour(ctx, ptype->rgb, 1);
	}
	else if (!strcmp(var, "reddelta")) {
		ptype->rgbchange[0] = PScript_ArgFloat(ctx, 1) / 255;
		PScript_DefaultChangeTime(ptype);
	}
	else if (!strcmp(var, "greendelta")) {
		ptype->rgbchange[1] = PScript_ArgFloat(ctx, 1) / 255;
		PScript_DefaultChangeTime(ptype);
	}
	else if (!strcmp(var, "bluedelta")) {
		ptype->rgbchange[2] = PScript_ArgFloat(ctx, 1) / 255;
		PScript_DefaultChangeTime(ptype);
	}
	else if (!strcmp(var, "rgbdelta")) {
		PScript_ParseColour(ctx, ptype->rgbchange, 255);
		PScript_DefaultChangeTime(ptype);
	}
	else if (!strcmp(var, "rgbdeltaf")) {
		PScript_ParseColour(ctx, ptype->rgbchange, 1);
		PScript_DefaultChangeTime(ptype);
	}
	else if (!strcmp(var, "rgbdeltatime")) {
		ptype->rgbchangetime = PScript_ArgFloat(ctx, 1);
	}
	else if (!strcmp(var, "redrand")) {
		ptype->rgbrand[0] = PScript_ArgFloat(ctx, 1) / 255;
	}
	else if (!strcmp(var, "greenrand")) {
		ptype->rgbrand[1] = PScript_ArgFloat(ctx, 1) / 255;
	}
	else if (!strcmp(var, "bluerand")) {
		ptype->rgbrand[2] = PScript_ArgFloat(ctx, 1) / 255;
	}
	else if (!strcmp(var, "rgbrand")) {
		PScript_ParseColour(ctx, ptype->rgbrand, 255);
	}
	else if (!strcmp(var, "rgbrandf")) {
		PScript_ParseColour(ctx, ptype->rgbrand, 1);
	}
	else if (!strcmp(var, "rgbrandsync")) {
		PScript_ParseColour(ctx, ptype->rgbrandsync, 1);
	}
	else if (!strcmp(var, "redrandsync")) {
		ptype->rgbrandsync[0] = PScript_ArgFloat(ctx, 1);
	}
	else if (!strcmp(var, "greenrandsync")) {
		ptype->rgbrandsync[1] = PScript_ArgFloat(ctx, 1);
	}
	else if (!strcmp(var, "bluerandsync")) {
		ptype->rgbrandsync[2] = PScript_ArgFloat(ctx, 1);
	}
	else if (!strcmp(var, "blend")) {
		PScript_ParseBlend(ptype, value);
	}
	else if (!strcmp(var, "spawnmode")) {
		PScript_ParseSpawnMode(ptype, ctx);
	}
	else if (!strcmp(var, "type")) {
		mode = PScript_FindName(pscript_looktypes, (int)ARRAY_LEN(pscript_looktypes), value);
		if (mode < 0) {
			Con_DPrintf("%s.%s: uses unknown render type '%s', assuming 'normal'\n", ptype->config, ptype->name, value);
			mode = PT_NORMAL;
		}
		ptype->looks.type = (pscript_looktype_t)mode;
		bp->settype = true;
	}
	else if (!strcmp(var, "clippeddecal")) {
		// Decals only land where (surface flags & mask) == match.
		if (argc >= 2) {
			ptype->surfflagmatch = ptype->surfflagmask = (unsigned int)strtoul(value, NULL, 0);
			if (argc >= 3) {
				ptype->surfflagmatch = (unsigned int)strtoul(Cmd_ArgvEx(ctx, 2), NULL, 0);
			}
		}
		ptype->looks.type = PT_CDECAL;
		bp->settype = true;
	}
	else if (!strcmp(var, "isbeam")) {
		Con_DPrintf("%s.%s: isbeam is deprecated, use type beam\n", ptype->config, ptype->name);
		ptype->looks.type = PT_BEAM;
		bp->settype = true;
	}
	else if (!strcmp(var, "spawntime")) {
		ptype->spawntime = PScript_ArgFloat(ctx, 1);
	}
	else if (!strcmp(var, "spawnchance")) {
		ptype->spawnchance = PScript_ArgFloat(ctx, 1);
	}
	else if (!strcmp(var, "cliptype")) {
		target = PScript_AllocateParticleType(bp->config, value);
		pscript_types[bp->pnum].cliptype = target;
	}
	else if (!strcmp(var, "clipcount")) {
		ptype->clipcount = PScript_ArgFloat(ctx, 1);
	}
	else if (!strcmp(var, "clipbounce")) {
		ptype->clipbounce = PScript_ArgFloat(ctx, 1);
		if (ptype->clipbounce < 0 && ptype->cliptype == P_INVALID) {
			ptype->cliptype = bp->pnum;
		}
	}
	else if (!strcmp(var, "bounce")) {
		ptype->cliptype = bp->pnum;
		ptype->clipbounce = PScript_ArgFloat(ctx, 1);
	}
	else if (!strcmp(var, "emit")) {
		target = PScript_AllocateParticleType(bp->config, value);
		pscript_types[bp->pnum].emit = target;
	}
	else if (!strcmp(var, "emitinterval")) {
		ptype->emittime = PScript_ArgFloat(ctx, 1);
	}
	else if (!strcmp(var, "emitintervalrand")) {
		ptype->emitrand = PScript_ArgFloat(ctx, 1);
	}
	else if (!strcmp(var, "emitstart")) {
		ptype->emitstart = PScript_ArgFloat(ctx, 1);
	}
	else if (!strcmp(var, "areaspread")) {
		Con_DPrintf("%s.%s: areaspread is deprecated, use spawnorg\n", ptype->config, ptype->name);
		ptype->areaspread = PScript_ArgFloat(ctx, 1);
	}
	else if (!strcmp(var, "areaspreadvert")) {
		Con_DPrintf("%s.%s: areaspreadvert is deprecated, use spawnorg\n", ptype->config, ptype->name);
		ptype->areaspreadvert = PScript_ArgFloat(ctx, 1);
	}
	else if (!strcmp(var, "offsetspread")) {
		Con_DPrintf("%s.%s: offsetspread is deprecated, use spawnvel\n", ptype->config, ptype->name);
		ptype->spawnvel = PScript_ArgFloat(ctx, 1);
	}
	else if (!strcmp(var, "offsetspreadvert")) {
		Con_DPrintf("%s.%s: offsetspreadvert is deprecated, use spawnvel\n", ptype->config, ptype->name);
		ptype->spawnvelvert = PScript_ArgFloat(ctx, 1);
	}
	else if (!strcmp(var, "spawnorg")) {
		ptype->areaspreadvert = ptype->areaspread = PScript_ArgFloat(ctx, 1);
		if (argc > 2) {
			ptype->areaspreadvert = PScript_ArgFloat(ctx, 2);
		}
	}
	else if (!strcmp(var, "spawnvel")) {
		ptype->spawnvelvert = ptype->spawnvel = PScript_ArgFloat(ctx, 1);
		if (argc > 2) {
			ptype->spawnvelvert = PScript_ArgFloat(ctx, 2);
		}
	}
	else if (!strcmp(var, "spawnparam1")) {
		ptype->spawnparam1 = PScript_ArgFloat(ctx, 1);
		Con_DPrintf("%s.%s: 'spawnparam1' is deprecated, use 'spawnmode foo X'\n", ptype->config, ptype->name);
	}
	else if (!strcmp(var, "spawnparam2")) {
		ptype->spawnparam2 = PScript_ArgFloat(ctx, 1);
		Con_DPrintf("%s.%s: 'spawnparam2' is deprecated, use 'spawnmode foo X Y'\n", ptype->config, ptype->name);
	}
	else if (!strcmp(var, "up")) {
		ptype->orgbias[2] = PScript_ArgFloat(ctx, 1);
	}
	else if (!strcmp(var, "rampmode")) {
		PScript_ParseRampMode(ptype, value);
	}
	else if (!strcmp(var, "rampindexlist")) {
		for (i = 1; i < argc; i++) {
			ramp = PScript_AddRampEntry(bp, ptype);
			if (!ramp) {
				break;
			}
			PScript_SetRampPalette(ramp, PScript_ParseInt(Cmd_ArgvEx(ctx, i), 10));
			ramp->scale = ptype->scale;
		}
	}
	else if (!strcmp(var, "rampindex")) {
		ramp = PScript_AddRampEntry(bp, ptype);
		if (ramp) {
			PScript_SetRampPalette(ramp, PScript_ParseInt(value, 10));
			if (argc > 2) {
				ramp->alpha *= PScript_ArgFloat(ctx, 2);
			}
			ramp->scale = (argc > 3 ? PScript_ArgFloat(ctx, 3) : ptype->scale);
		}
	}
	else if (!strcmp(var, "ramp")) {
		ramp = PScript_AddRampEntry(bp, ptype);
		if (ramp) {
			ramp->rgb[0] = PScript_ArgFloat(ctx, 1) / 255;
			if (argc > 3) {
				ramp->rgb[1] = PScript_ArgFloat(ctx, 2) / 255;
				ramp->rgb[2] = PScript_ArgFloat(ctx, 3) / 255;
				ramp->alpha = (argc > 4 ? PScript_ArgFloat(ctx, 4) : ptype->alpha);
				ramp->scale = (argc > 5 ? PScript_ArgFloat(ctx, 5) : ptype->scaledelta);
			}
			else {
				ramp->rgb[1] = ramp->rgb[2] = ramp->rgb[0];
				ramp->alpha = ptype->alpha;
				ramp->scale = ptype->scaledelta;
			}
		}
	}
	else if (!strcmp(var, "perframe")) {
		ptype->flags |= PT_INVFRAMETIME;
	}
	else if (!strcmp(var, "averageout")) {
		ptype->flags |= PT_AVERAGETRAIL;
	}
	else if (!strcmp(var, "nostate")) {
		ptype->flags |= PT_NOSTATE;
	}
	else if (!strcmp(var, "nospreadfirst")) {
		ptype->flags |= PT_NOSPREADFIRST;
	}
	else if (!strcmp(var, "nospreadlast")) {
		ptype->flags |= PT_NOSPREADLAST;
	}
	else if (!strcmp(var, "lightradius")) {
		ptype->dl_radius[0] = ptype->dl_radius[1] = PScript_ArgFloat(ctx, 1);
		if (argc > 2) {
			ptype->dl_radius[1] = PScript_ArgFloat(ctx, 2);
		}
		ptype->dl_radius[1] -= ptype->dl_radius[0];
	}
	else if (!strcmp(var, "lightradiusfade")) {
		ptype->dl_decay = PScript_ArgFloat(ctx, 1);
	}
	else if (!strcmp(var, "lightrgb")) {
		PScript_ParseVector(ctx, ptype->dl_rgb);
	}
	else if (!strcmp(var, "lightrgbfade")) {
		PScript_ParseVector(ctx, ptype->dl_rgbfade);
	}
	else if (!strcmp(var, "lighttime")) {
		ptype->dl_time = PScript_ArgFloat(ctx, 1);
	}
	else if (PScript_IsUnsupportedField(var)) {
		Con_DPrintf("%s.%s: %s is not supported\n", ptype->config, ptype->name, var);
	}
	else {
		Con_DPrintf("%s.%s: %s is not a recognised particle type field\n", ptype->config, ptype->name, var);
	}
}

// Parses the block of "r_part <name>", whose '{' was read. "+name" adds a
// type to name's assoc chain, a plain name drops the chain it had. May
// reallocate pscript_types.
static void PScript_ParseEffect(pscript_reader_t *r, tokenizecontext_t *ctx, const char *config, qbool weak)
{
	const char *name = Cmd_ArgvEx(ctx, 1);
	qbool chained = (*name == '+');
	char linkname[MAX_QPATH + 16];
	pscript_blockparse_t bp;
	pscript_type_t *ptype;
	int parent, assoc, i;
	char *s;

	ptype = PScript_GetParticleType(config, chained ? name + 1 : name);
	if (!ptype) {
		PScript_SkipBlock(r);
		return;
	}

	// Weak may replace weak, which is how a +chain extends a weak effect.
	if (weak && ptype->loaded == 2) {
		PScript_SkipBlock(r);
		return;
	}

	if (chained && ptype->loaded) {
		for (i = 0; i < PSCRIPT_MAX_CHAIN; i++) {
			parent = ptype - pscript_types;
			// Cut to the length a type name keeps, so the next definition of
			// the chain finds the same type.
			snprintf(linkname, sizeof(linkname), "+%i%s", i, ptype->name);
			linkname[sizeof(ptype->name) - 1] = 0;
			ptype = PScript_GetParticleType(config, linkname);
			if (!ptype) {
				PScript_SkipBlock(r);
				return;
			}
			if (!ptype->loaded) {
				if (pscript_types[parent].assoc != P_INVALID) {
					Con_Printf("warning: assoc on particle chain \"%s.%s\" overridden\n", pscript_types[parent].config, pscript_types[parent].name);
				}
				pscript_types[parent].assoc = ptype - pscript_types;
				break;
			}
		}
		if (i == PSCRIPT_MAX_CHAIN) {
			Con_Printf("Too many duplicate names, gave up\n");
			PScript_SkipBlock(r);
			return;
		}
	}
	else if (!chained && ptype->loaded) {
		// The chain's types keep their indices, so references to them stay
		// valid; they are unloaded until a +block defines them again.
		assoc = ptype->assoc;
		for (i = 0; PART_VALID(assoc) && i < pscript_numtypes && pscript_types[assoc].name[0] == '+'; i++) {
			pscript_types[assoc].loaded = 0;
			assoc = pscript_types[assoc].assoc;
		}
	}

	memset(&bp, 0, sizeof(bp));
	bp.reader = r;
	bp.config = config;
	bp.pnum = ptype - pscript_types;
	PScript_ResetToDefaults(ptype);

	for (;;) {
		s = PScript_NextStatement(r, false);
		if (!s) {
			Con_Printf("Unexpected end of buffer with effect %s\n", pscript_types[bp.pnum].name);
			return;
		}
		if (*s == '}') {
			break;
		}

		Cmd_TokenizeStringEx(ctx, s);
		if (Cmd_ArgcEx(ctx)) {
			PScript_ParseField(&bp, ctx);
		}
	}

	ptype = &pscript_types[bp.pnum];
	ptype->loaded = (weak ? 1 : 2);
	if (ptype->clipcount < 1) {
		ptype->clipcount = 1;
	}

	if (!bp.settype) {
		if (ptype->looks.type == PT_NORMAL && !*ptype->texname) {
			ptype->looks.type = (ptype->scale ? PT_SPARKFAN : PT_SPARK);
			if (ptype->countextra || ptype->count || ptype->countrand) {
				Con_DPrintf("%s.%s: effect lacks a texture. assuming type %s.\n", ptype->config, ptype->name, ptype->scale ? "sparkfan" : "spark");
			}
		}
		else if (ptype->looks.type == PT_SPARK) {
			if (*ptype->texname) {
				ptype->looks.type = PT_TEXTUREDSPARK;
			}
			else if (ptype->scale) {
				ptype->looks.type = PT_SPARKFAN;
			}
		}
	}

	// Without alphadelta, alphachange is the fraction of alpha lost over the
	// particle's life.
	if (!bp.setalphadelta) {
		ptype->alphachange = PScript_Divide(-ptype->alphachange, ptype->die) * ptype->alpha;
	}

	// An inline shader needs a shader program; the fallback texture would be
	// a large blob. Its lights, sounds and assoc chain still run.
	if (bp.shaderbody) {
		Con_DPrintf("%s.%s: inline shaders are not supported, the effect draws nothing\n", ptype->config, ptype->name);
		ptype->looks.type = PT_INVISIBLE;
	}

	PScript_FinishParticleType(ptype);

	if (ptype->looks.type == PT_BEAM && !bp.setbeamlen) {
		ptype->rotationstartmin = 1 / 128.0f;
	}
}

// Parses an r_part config as text, not as console commands: only r_part,
// r_partredirect, r_effect, r_trail and cl_expsprite lines are read and every
// other top-level line is ignored. Weak types leave types loaded by strong
// sets alone. May reallocate pscript_types.
void PScript_ParseParticleEffectFile(const char *config, qbool weak, const char *data, size_t size)
{
	char effectnamespace[MAX_QPATH];
	tokenizecontext_t ctx;
	pscript_reader_t r;
	const char *cmd;
	char *s;
	int numblocks = 0;

	strlcpy(effectnamespace, config, sizeof(effectnamespace));
	memset(&r, 0, sizeof(r));
	r.config = config;
	r.data = data;
	r.size = size;

	s = PScript_NextStatement(&r, false);
	while (s) {
		Cmd_TokenizeStringEx(&ctx, s);
		cmd = Cmd_ArgvEx(&ctx, 0);

		if (!strcmp(cmd, "r_part")) {
			if (Cmd_ArgcEx(&ctx) == 2) {
				s = PScript_NextStatement(&r, false);
				if (!s) {
					return;
				}
				if (*s != '{') {
					// What follows is read as an ordinary line.
					Con_DPrintf("%s: r_part %s has no { block\n", config, Cmd_ArgvEx(&ctx, 1));
					continue;
				}
				if (++numblocks > PSCRIPT_MAX_BLOCKS) {
					Con_Printf("%s: more than %d effects, ignored the rest\n", config, PSCRIPT_MAX_BLOCKS);
					return;
				}
				PScript_ParseEffect(&r, &ctx, effectnamespace, weak);
			}
			else if (!strcmp(Cmd_ArgvEx(&ctx, 1), "namespace")) {
				strlcpy(effectnamespace, Cmd_ArgvEx(&ctx, 2), sizeof(effectnamespace));
				if (Cmd_ArgcEx(&ctx) >= 4) {
					weak = (PScript_ParseInt(Cmd_ArgvEx(&ctx, 3), 10) != 0);
				}
			}
			else {
				Con_DPrintf("%s: no name for particle effect\n", config);
			}
		}
		else if (!strcmp(cmd, "cl_expsprite")) {
			// Read like a cvar assignment; without an argument, a cvar query,
			// it changes nothing.
			if (Cmd_ArgcEx(&ctx) >= 2) {
				pscript_expsprite = (PScript_ParseInt(Cmd_ArgvEx(&ctx, 1), 10) != 0);
			}
		}
		else if (!strcmp(cmd, "r_partredirect")) {
			PScript_PartRedirectCmd(&ctx, true);
		}
		else if (!strcmp(cmd, "r_effect") || !strcmp(cmd, "r_trail")) {
			PScript_AssociateEffectCmd(&ctx, effectnamespace);
		}
		else if (*cmd) {
			Con_DPrintf("%s: ignored %s\n", config, cmd);
		}

		s = PScript_NextStatement(&r, false);
	}
}

// Keeps the name, config and the beam segments that trail states still hold.
void PScript_ResetToDefaults(pscript_type_t *ptype)
{
	char name[sizeof(ptype->name)];
	char config[sizeof(ptype->config)];
	pscript_beamseg_t *beams;

	PScript_ReleaseTypeInstances(ptype);
	PScript_UnlinkRunList(ptype);

	beams = ptype->beams;
	strlcpy(name, ptype->name, sizeof(name));
	strlcpy(config, ptype->config, sizeof(config));
	Q_free(ptype->ramp);
	Q_free(ptype->sounds);

	memset(ptype, 0, sizeof(*ptype));

	ptype->beams = beams;
	strlcpy(ptype->name, name, sizeof(ptype->name));
	strlcpy(ptype->config, config, sizeof(ptype->config));
	ptype->rainfrequency = 1;
	ptype->assoc = P_INVALID;
	ptype->inwater = P_INVALID;
	ptype->cliptype = P_INVALID;
	ptype->emit = P_INVALID;
	ptype->fluidmask = PSCRIPT_CONTENTS_FLUID;
	ptype->alpha = 1;
	ptype->alphachange = 1;
	ptype->clipbounce = 0.8f;
	ptype->clipcount = 1;
	ptype->colorindex = -1;
	ptype->rotationstartmin = -M_PI; // with rotationstartrand, any starting angle
	ptype->rotationstartrand = M_PI - ptype->rotationstartmin;
	ptype->spawnchance = 1;
	VectorSet(ptype->dl_rgb, 1, 1, 1);
	ptype->looks.stretch = 0.05f;
	ptype->randsmax = 1;
	ptype->s2 = 1;
	ptype->t2 = 1;
}

// Called after a type's definition was read: derives flags, bounds the fields
// used as divisors, moduli and steps, and loads the texture.
void PScript_FinishParticleType(pscript_type_t *ptype)
{
	if (ptype->gravity || ptype->veladd || ptype->spawnvel || ptype->spawnvelvert || DotProduct(ptype->velwrand, ptype->velwrand) || DotProduct(ptype->velbias, ptype->velbias) || ptype->flurry) {
		ptype->flags |= PT_VELOCITY;
	}
	if (DotProduct(ptype->velbias, ptype->velbias) || DotProduct(ptype->velwrand, ptype->velwrand) || DotProduct(ptype->orgwrand, ptype->orgwrand)) {
		ptype->flags |= PT_WORLDSPACERAND;
	}
	if (ptype->friction[0] || ptype->friction[1] || ptype->friction[2]) {
		ptype->flags |= PT_FRICTION;
	}

	// Spawning takes rand() modulo randsmax and colorrand and adds the
	// palette offset to colorindex.
	ptype->randsmax = bound(1, ptype->randsmax, PSCRIPT_MAX_SLOTS);
	if (ptype->colorindex > 255) {
		ptype->colorindex = 256 + (ptype->colorindex & 0xff);
	}
	ptype->colorindex = bound(-1, ptype->colorindex, PSCRIPT_MAX_COLORINDEX);
	ptype->colorrand = bound(0, ptype->colorrand, PSCRIPT_MAX_COLORRAND);
	// A random slot moves s by up to randsmax - 1 strides, and trails move a
	// slot past the right edge to the next row one texture width at a time.
	// The bounds keep those offsets finite.
	ptype->texsstride = bound(-PSCRIPT_MAX_ATLAS, ptype->texsstride, PSCRIPT_MAX_ATLAS);
	ptype->s1 = bound(-PSCRIPT_MAX_ATLAS, ptype->s1, PSCRIPT_MAX_ATLAS);
	ptype->t1 = bound(-PSCRIPT_MAX_ATLAS, ptype->t1, PSCRIPT_MAX_ATLAS);
	ptype->s2 = bound(-PSCRIPT_MAX_ATLAS, ptype->s2, PSCRIPT_MAX_ATLAS);
	ptype->t2 = bound(-PSCRIPT_MAX_ATLAS, ptype->t2, PSCRIPT_MAX_ATLAS);
	// Particles subtract a random part of randdie from their lifetime; a
	// negative one would make them outlive the type's die.
	ptype->randdie = max(ptype->randdie, 0);
	// These modes step through their area by spawnparam2 truncated to int and
	// take rand() modulo it; 0 selects the classic spacing.
	if ((ptype->spawnmode == SM_TELEBOX || ptype->spawnmode == SM_LAVASPLASH) && fabs(ptype->spawnparam2) < 1) {
		ptype->spawnparam2 = 0;
	}

	PScript_LoadTexture(ptype);

	if (ptype->dl_decay && !ptype->dl_time) {
		ptype->dl_time = PScript_Divide(ptype->dl_radius[0], ptype->dl_decay);
	}

	// Drawing blends between a fixed screen size and the world size, so
	// scaling more than the world does is folded into the size; ramp scales
	// are left as they are.
	if (ptype->looks.scalefactor > 1 && !ptype->looks.invscalefactor) {
		ptype->scale *= ptype->looks.scalefactor;
		ptype->scalerand *= ptype->looks.scalefactor;
		ptype->looks.scalefactor = 1;
	}
	ptype->looks.invscalefactor = 1 - ptype->looks.scalefactor;

	if (ptype->looks.type == PT_TEXTUREDSPARK && !ptype->looks.stretch) {
		ptype->looks.stretch = 0.05f;
	}

	if (ptype->looks.type == PT_SPARK && r_part_sparks.integer < 0) {
		ptype->looks.type = PT_INVISIBLE;
	}
	if (ptype->looks.type == PT_TEXTUREDSPARK && !r_part_sparks_textured.integer) {
		ptype->looks.type = PT_SPARK;
	}
	if (ptype->looks.type == PT_SPARKFAN && !r_part_sparks_trifan.integer) {
		ptype->looks.type = PT_SPARK;
	}
	if (ptype->looks.type == PT_SPARK && !r_part_sparks.integer) {
		ptype->looks.type = PT_INVISIBLE;
	}
	if (ptype->looks.type == PT_BEAM && r_part_beams.integer <= 0) {
		ptype->looks.type = PT_INVISIBLE;
	}

	if (ptype->rampmode != RAMP_NONE && (!ptype->ramp || ptype->rampindexes <= 0)) {
		ptype->rampmode = RAMP_NONE;
		Con_Printf("%s.%s: Particle has a ramp mode but no ramp\n", ptype->config, ptype->name);
	}
	else if (ptype->ramp && ptype->rampmode == RAMP_NONE) {
		Con_Printf("%s.%s: Particle has a ramp but no ramp mode\n", ptype->config, ptype->name);
	}

	pscript_effects_dirty = true;
}
