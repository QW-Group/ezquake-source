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

// Imports DarkPlaces effectinfo.txt files. Every "effect" block becomes a
// scripted particle type in the namespace of the set that named the file,
// its DarkPlaces settings converted to their r_part equivalents.

#include "quakedef.h"
#include "r_part_script_local.h"

// Texture slots "tex" can address; DarkPlaces' particle font has 256.
#define EI_NUMSLOTS 256
// The slot DarkPlaces draws an effect with when the block has no "tex".
#define EI_DEFAULTSLOT 63
// DarkPlaces loads this slot, its beam texture, from an image of its own.
#define EI_BEAMSLOT 60

// Repeats of one effect name each chain one more type.
#define EI_MAXREPEATS 64
// Length of the "<n>+" prefix that names a repeat, n < EI_MAXREPEATS.
#define EI_REPEATPREFIX 3

// Each block can create a type, and creating one scans and grows the type
// array, so the work grows with the square of the block count.
#define EI_MAXEFFECTS  4096
#define EI_MAXFILESIZE (8 << 20)

// Bound for every decimal number read, small enough that the products and
// sums the particle code forms from them stay finite in single precision.
#define EI_MAXVALUE 1e6

// Closest trail spacing accepted; sixteen particles per unit already overlap
// completely.
#define EI_MINSPACING (1.0f / 16)

// Texture coordinates as a type stores them: t1 is the bottom edge.
typedef struct ei_slot_s {
	float s1, s2, t1, t2;
	char texname[MAX_QPATH];
} ei_slot_t;

typedef struct ei_block_s {
	int type;         // type being filled, P_INVALID while skipping a block
	qbool dieset;     // the block gave the lifetime
	qbool blooddecal; // splats into a decal where it collides
} ei_block_t;

// DarkPlaces' particle types. Its alpha and additive blends both become
// premultiplied blending, which blends with premul 1 and adds with premul
// 2, so either draws with one blend function.
static const struct {
	const char *name;
	pscript_looktype_t look;
	pscript_blendmode_t blendmode;
	int premul;
} ei_types[] = {
	{ "alphastatic", PT_NORMAL, BM_PREMUL, 1 },
	{ "static", PT_NORMAL, BM_PREMUL, 2 },
	{ "spark", PT_TEXTUREDSPARK, BM_PREMUL, 2 },
	{ "beam", PT_BEAM, BM_PREMUL, 2 },
	{ "rain", PT_TEXTUREDSPARK, BM_PREMUL, 2 },
	{ "raindecal", PT_UDECAL, BM_PREMUL, 2 },
	{ "snow", PT_NORMAL, BM_PREMUL, 2 },
	{ "bubble", PT_NORMAL, BM_PREMUL, 2 },
	{ "blood", PT_NORMAL, BM_INVMODC, 2 },
	{ "smoke", PT_NORMAL, BM_PREMUL, 2 },
	{ "decal", PT_CDECAL, BM_INVMODC, 2 },
	{ "cdecal", PT_CDECAL, BM_INVMODC, 2 },
	{ "udecal", PT_UDECAL, BM_INVMODC, 2 },
	{ "entityparticle", PT_NORMAL, BM_PREMUL, 1 },
};

static const struct {
	const char *name;
	pscript_blendmode_t blendmode;
	int premul;
} ei_blends[] = {
	{ "alpha", BM_PREMUL, 1 },
	{ "add", BM_PREMUL, 2 },
	{ "invmod", BM_INVMODC, 2 },
};

// DarkPlaces keywords for features these particles lack.
static const char *const ei_ignored[] = {
	"liquidfriction",
	"lightshadow",
	"lightcubemapnum",
	"lightcorona",
	"staincolor",
	"stainalpha",
	"stainsize",
	"staintex",
	"stainless",
	"relativeoriginoffset",
	"relativevelocityoffset",
};

// Blanks out the comments in one line. *incomment carries an open "/*" over
// to the next call.
static void EI_StripComments(char *line, qbool *incomment)
{
	qbool inquote = false;
	char *s;

	for (s = line; *s; s++) {
		if (*incomment) {
			if (s[0] == '*' && s[1] == '/') {
				*incomment = false;
				*s++ = ' ';
			}
			*s = ' ';
		}
		else if (*s == '"') {
			inquote = !inquote;
		}
		else if (!inquote && s[0] == '/' && s[1] == '/') {
			*s = '\0';
			return;
		}
		else if (!inquote && s[0] == '/' && s[1] == '*') {
			*incomment = true;
			*s++ = ' ';
			*s = ' ';
		}
	}
}

// Reads a texture slot number. Values beyond the table are clamped to -1 or
// EI_NUMSLOTS, which stay out of range without overflowing any arithmetic.
static int EI_Slot(const char *s)
{
	long v = strtol(s, NULL, 10);

	return (int)bound(-1, v, EI_NUMSLOTS);
}

// Reads count (at most 4) numbers into out, clamped to +-EI_MAXVALUE. Fails,
// leaving out unchanged, if a number is not finite.
static qbool EI_ParseNumbers(tokenizecontext_t *ctx, int first, int count, float *out)
{
	double v[4];
	int i;

	for (i = 0; i < count; i++) {
		v[i] = atof(Cmd_ArgvEx(ctx, first + i));
		if (!isfinite(v[i])) {
			return false;
		}
	}
	for (i = 0; i < count; i++) {
		out[i] = (float)bound(-EI_MAXVALUE, v[i], EI_MAXVALUE);
	}
	return true;
}

// True when the line is keyword followed by exactly count arguments.
static qbool EI_Keyword(tokenizecontext_t *ctx, const char *keyword, int count)
{
	return !strcmp(Cmd_ArgvEx(ctx, 0), keyword) && Cmd_ArgcEx(ctx) == count + 1;
}

// EI_Keyword, with the count (at most 4) arguments read by EI_ParseNumbers.
static qbool EI_Numbers(tokenizecontext_t *ctx, const char *keyword, int count, float *out)
{
	return EI_Keyword(ctx, keyword, count) && EI_ParseNumbers(ctx, 1, count, out);
}

static void EI_SetWholeTexture(ei_slot_t *slot, const char *texname)
{
	slot->s1 = 0;
	slot->s2 = 1;
	slot->t1 = 0;
	slot->t2 = 1;
	strlcpy(slot->texname, texname, sizeof(slot->texname));
}

// Fills the texture slots "tex" picks from: an 8x8 grid over
// particles/particlefont, unless particles/particlefont.txt moves a slot
// ("<slot> <s1> <t1> <s2> <t2> [<texture>]") or gives it a texture of its
// own ("<slot> <texture>").
static void EI_LoadParticleFont(ei_slot_t *slots)
{
	tokenizecontext_t ctx;
	char line[1024];
	char *data;
	const char *texname;
	size_t offset = 0;
	qbool incomment = false;
	float tc[4];
	int i, size, argc;

	for (i = 0; i < EI_NUMSLOTS; i++) {
		slots[i].s1 = (i & 7) / 8.0f;
		slots[i].s2 = ((i & 7) + 1) / 8.0f;
		slots[i].t1 = ((i >> 3) + 1) / 8.0f;
		slots[i].t2 = (i >> 3) / 8.0f;
		strlcpy(slots[i].texname, "particles/particlefont", sizeof(slots[i].texname));
	}
	EI_SetWholeTexture(&slots[EI_BEAMSLOT], "particles/nexbeam");

	data = (char *)FS_LoadHeapFile("particles/particlefont.txt", &size);
	if (!data) {
		return;
	}
	if (size > EI_MAXFILESIZE) {
		Con_Printf("particles/particlefont.txt is larger than %d bytes, ignored\n", EI_MAXFILESIZE);
		Q_free(data);
		return;
	}

	while (PScript_ReadLine(line, sizeof(line), data, (size_t)size, &offset)) {
		EI_StripComments(line, &incomment);
		Cmd_TokenizeStringEx(&ctx, line);
		argc = Cmd_ArgcEx(&ctx);
		if (!argc) {
			continue;
		}

		i = EI_Slot(Cmd_ArgvEx(&ctx, 0));
		texname = (argc == 2 || argc == 6) ? Cmd_ArgvEx(&ctx, argc - 1) : "particles/particlefont";
		if (i < 0 || i >= EI_NUMSLOTS || strlen(texname) >= sizeof(slots[i].texname)) {
			Con_DPrintf("particles/particlefont.txt: ignored %s\n", Cmd_MakeArgsEx(&ctx, 0));
		}
		else if (argc == 2) {
			EI_SetWholeTexture(&slots[i], texname);
		}
		else if ((argc == 5 || argc == 6) && EI_ParseNumbers(&ctx, 1, 4, tc)) {
			slots[i].s1 = tc[0];
			slots[i].t2 = tc[1];
			slots[i].s2 = tc[2];
			slots[i].t1 = tc[3];
			strlcpy(slots[i].texname, texname, sizeof(slots[i].texname));
		}
		else {
			Con_DPrintf("particles/particlefont.txt: ignored %s\n", Cmd_MakeArgsEx(&ctx, 0));
		}
	}
	Q_free(data);
}

static void EI_ApplySlot(pscript_type_t *ptype, const ei_slot_t *slot)
{
	strlcpy(ptype->texname, slot->texname, sizeof(ptype->texname));
	ptype->s1 = slot->s1;
	ptype->s2 = slot->s2;
	ptype->t1 = slot->t1;
	ptype->t2 = slot->t2;
}

// "tex <first> <end>": each particle takes a random slot from first up to,
// not including, end. Fails for a first slot out of range.
static qbool EI_SetTex(pscript_type_t *ptype, const ei_slot_t *slots, const char *first, const char *end)
{
	int lo = EI_Slot(first);
	int hi = EI_Slot(end);

	if (lo < 0 || lo >= EI_NUMSLOTS) {
		return false;
	}
	EI_ApplySlot(ptype, &slots[lo]);
	// The random slot steps along s, so a range must lie in one row of one
	// texture.
	ptype->texsstride = slots[(lo + 1) % EI_NUMSLOTS].s1 - slots[lo].s1;
	ptype->randsmax = bound(1, hi - lo, EI_NUMSLOTS - lo);
	return true;
}

// "color <0xRRGGBB> <0xRRGGBB>": each particle takes one point on the line
// between the two colours.
static void EI_SetColor(pscript_type_t *ptype, const char *first, const char *second)
{
	unsigned long c1 = strtoul(first, NULL, 0);
	unsigned long c2 = strtoul(second, NULL, 0);
	int i, b1, b2;

	for (i = 0; i < 3; i++) {
		b1 = (int)((c1 >> (16 - 8 * i)) & 0xff);
		b2 = (int)((c2 >> (16 - 8 * i)) & 0xff);
		ptype->rgb[i] = b1 / 255.0f;
		ptype->rgbrand[i] = (b2 - b1) / 255.0f;
		ptype->rgbrandsync[i] = 1;
	}
}

// Applies a DarkPlaces particle type. Fails for an unknown type.
static qbool EI_SetType(ei_block_t *block, const char *name)
{
	pscript_type_t *ptype = &pscript_types[block->type];
	size_t i;

	for (i = 0; i < sizeof(ei_types) / sizeof(ei_types[0]); i++) {
		if (!strcmp(name, ei_types[i].name)) {
			break;
		}
	}
	if (i == sizeof(ei_types) / sizeof(ei_types[0])) {
		return false;
	}

	ptype->looks.type = ei_types[i].look;
	ptype->looks.blendmode = ei_types[i].blendmode;
	ptype->looks.premul = ei_types[i].premul;
	if (!strcmp(name, "blood")) {
		// Falls at full gravity and splats where it collides.
		ptype->gravity = 800;
		block->blooddecal = true;
	}
	else if (!strcmp(name, "snow")) {
		// Drifts sideways as it falls.
		ptype->flurry = 32;
	}
	else if (!strcmp(name, "entityparticle")) {
		// Respawned by its entity every frame, so it lives for one.
		ptype->die = 0;
		block->dieset = true;
	}
	return true;
}

static qbool EI_SetBlend(pscript_type_t *ptype, const char *name)
{
	size_t i;

	for (i = 0; i < sizeof(ei_blends) / sizeof(ei_blends[0]); i++) {
		if (!strcmp(name, ei_blends[i].name)) {
			ptype->looks.blendmode = ei_blends[i].blendmode;
			ptype->looks.premul = ei_blends[i].premul;
			return true;
		}
	}
	return false;
}

static qbool EI_SetOrientation(pscript_type_t *ptype, const char *name)
{
	if (!strcmp(name, "billboard")) {
		ptype->looks.type = PT_NORMAL;
	}
	else if (!strcmp(name, "spark")) {
		ptype->looks.type = PT_TEXTUREDSPARK;
	}
	else if (!strcmp(name, "oriented")) {
		// A clipped decal is oriented to the surface it lands on already.
		if (ptype->looks.type != PT_CDECAL) {
			ptype->looks.type = PT_UDECAL;
		}
	}
	else if (!strcmp(name, "beam")) {
		ptype->looks.type = PT_BEAM;
	}
	else {
		return false;
	}
	return true;
}

// Applies one line of an effect block to its type. Returns false, leaving
// the type unchanged, for a line it does not understand.
static qbool EI_ParseLine(ei_block_t *block, tokenizecontext_t *ctx, const ei_slot_t *slots)
{
	pscript_type_t *ptype = &pscript_types[block->type];
	const char *keyword = Cmd_ArgvEx(ctx, 0);
	float v[4];
	size_t i;

	for (i = 0; i < sizeof(ei_ignored) / sizeof(ei_ignored[0]); i++) {
		if (!strcmp(keyword, ei_ignored[i])) {
			Con_DPrintf("%s.%s: %s is not supported\n", ptype->config, ptype->name, keyword);
			return true;
		}
	}

	if (EI_Keyword(ctx, "type", 1)) {
		return EI_SetType(block, Cmd_ArgvEx(ctx, 1));
	}
	if (EI_Keyword(ctx, "blend", 1)) {
		return EI_SetBlend(ptype, Cmd_ArgvEx(ctx, 1));
	}
	if (EI_Keyword(ctx, "orientation", 1)) {
		return EI_SetOrientation(ptype, Cmd_ArgvEx(ctx, 1));
	}
	if (EI_Keyword(ctx, "tex", 2)) {
		return EI_SetTex(ptype, slots, Cmd_ArgvEx(ctx, 1), Cmd_ArgvEx(ctx, 2));
	}
	if (EI_Keyword(ctx, "color", 2)) {
		EI_SetColor(ptype, Cmd_ArgvEx(ctx, 1), Cmd_ArgvEx(ctx, 2));
		return true;
	}
	if (EI_Keyword(ctx, "underwater", 0)) {
		ptype->flags |= PT_TRUNDERWATER;
		return true;
	}
	if (EI_Keyword(ctx, "notunderwater", 0)) {
		ptype->flags |= PT_TROVERWATER;
		return true;
	}

	// Counts bound spawn loops; no single effect can use more particles
	// than the largest pool holds.
	if (EI_Numbers(ctx, "count", 1, v)) {
		ptype->count = bound(0, v[0], PSCRIPT_MAX_PARTICLES);
		return true;
	}
	if (EI_Numbers(ctx, "countabsolute", 1, v)) {
		ptype->countextra = bound(0, v[0], PSCRIPT_MAX_PARTICLES);
		return true;
	}
	if (EI_Numbers(ctx, "trailspacing", 1, v)) {
		// Zero or less turns spacing off.
		if (v[0] > 0) {
			ptype->countspacing = max(v[0], EI_MINSPACING);
			ptype->count = 1 / ptype->countspacing;
		}
		else {
			ptype->countspacing = 0;
		}
		return true;
	}
	if (EI_Numbers(ctx, "size", 2, v)) {
		ptype->scale = v[0];
		ptype->scalerand = v[1] - v[0];
		return true;
	}
	if (EI_Numbers(ctx, "sizeincrease", 1, v)) {
		ptype->scaledelta = v[0];
		return true;
	}
	if (EI_Numbers(ctx, "stretchfactor", 1, v)) {
		ptype->looks.stretch = v[0];
		return true;
	}
	if (EI_Numbers(ctx, "alpha", 3, v)) {
		// Either order of minimum and maximum, then fade per second, all on
		// a 0..256 scale.
		ptype->alpha = min(v[0], v[1]) / 256;
		ptype->alpharand = (max(v[0], v[1]) - min(v[0], v[1])) / 256;
		ptype->alphachange = -v[2] / 256;
		return true;
	}
	if (EI_Numbers(ctx, "time", 2, v)) {
		// Either order. A negative lifetime would expire particles before
		// they are drawn.
		ptype->die = max(0, min(v[0], v[1]));
		ptype->randdie = max(0, max(v[0], v[1]) - ptype->die);
		block->dieset = true;
		return true;
	}
	if (EI_Numbers(ctx, "originoffset", 3, v)) {
		VectorCopy(v, ptype->orgbias);
		return true;
	}
	if (EI_Numbers(ctx, "originjitter", 3, v)) {
		VectorCopy(v, ptype->orgwrand);
		return true;
	}
	if (EI_Numbers(ctx, "velocityoffset", 3, v)) {
		VectorCopy(v, ptype->velbias);
		return true;
	}
	if (EI_Numbers(ctx, "velocityjitter", 3, v)) {
		VectorCopy(v, ptype->velwrand);
		return true;
	}
	if (EI_Numbers(ctx, "velocitymultiplier", 1, v)) {
		ptype->veladd = v[0];
		return true;
	}
	if (EI_Numbers(ctx, "gravity", 1, v)) {
		// A fraction of DarkPlaces' default sv_gravity.
		ptype->gravity = 800 * v[0];
		return true;
	}
	if (EI_Numbers(ctx, "airfriction", 1, v)) {
		VectorSet(ptype->friction, v[0], v[0], v[0]);
		return true;
	}
	if (EI_Numbers(ctx, "bounce", 1, v)) {
		// Only a negative bounce collides, which removes the particle on
		// impact; a positive one is stored but has no effect.
		ptype->clipbounce = v[0];
		if (v[0] < 0) {
			ptype->cliptype = block->type;
		}
		return true;
	}
	if (EI_Numbers(ctx, "rotate", 4, v)) {
		// Start angle range in degrees and spin range in degrees per second.
		// At angle 0 a particle's corners lie on the view axes, so the start
		// is turned by 45 degrees to stand it upright as DarkPlaces draws it.
		ptype->rotationstartmin = (float)(DEG2RAD(v[0]) + M_PI / 4);
		ptype->rotationstartrand = (float)DEG2RAD(v[1] - v[0]);
		ptype->rotationmin = (float)DEG2RAD(v[2]);
		ptype->rotationrand = (float)DEG2RAD(v[3] - v[2]);
		return true;
	}
	// Light values are not negative, so neither is the light lifetime
	// derived from them.
	if (EI_Numbers(ctx, "lightradius", 1, v)) {
		ptype->dl_radius[0] = max(0, v[0]);
		ptype->dl_radius[1] = 0;
		return true;
	}
	if (EI_Numbers(ctx, "lightradiusfade", 1, v)) {
		ptype->dl_decay = max(0, v[0]);
		return true;
	}
	if (EI_Numbers(ctx, "lightcolor", 3, v)) {
		VectorCopy(v, ptype->dl_rgb);
		return true;
	}
	if (EI_Numbers(ctx, "lighttime", 1, v)) {
		ptype->dl_time = max(0, v[0]);
		return true;
	}
	return false;
}

// Starts the type for "effect <name>" with DarkPlaces' defaults, or returns
// P_INVALID to skip the block. A repeated name gets one more type,
// "<n>+<name>", chained by assoc, so spawning the name spawns every block.
static int EI_BeginBlock(const char *config, const char *name, const ei_slot_t *slots, const char *path, int linenum)
{
	char repeatname[MAX_QPATH];
	pscript_type_t *ptype;
	int first, last, typenum, n;

	// A dot would name another set's namespace, and a name that does not
	// fit with its repeat prefix would be truncated into another name.
	if (!*name || strchr(name, '.') || strlen(name) + EI_REPEATPREFIX >= sizeof(repeatname)) {
		Con_DPrintf("%s:%d: unusable effect name \"%s\"\n", path, linenum, name);
		return P_INVALID;
	}

	first = PScript_AllocateParticleType(config, name);
	if (first == P_INVALID) {
		return P_INVALID;
	}
	typenum = first;
	if (pscript_types[first].loaded) {
		// Following the chain keeps a name repeated n times from probing
		// n names, so many repeats stay linear.
		last = first;
		for (n = 0; n < EI_MAXREPEATS && PART_VALID(pscript_types[last].assoc); n++) {
			last = pscript_types[last].assoc;
		}
		if (n == EI_MAXREPEATS) {
			Con_DPrintf("%s:%d: effect %s repeated more than %d times\n", path, linenum, name, EI_MAXREPEATS);
			return P_INVALID;
		}
		snprintf(repeatname, sizeof(repeatname), "%d+%s", n, name);
		typenum = PScript_AllocateParticleType(config, repeatname);
		if (typenum == P_INVALID) {
			return P_INVALID;
		}
		if (pscript_types[typenum].loaded) {
			Con_DPrintf("%s:%d: effect %s.%s is already loaded\n", path, linenum, config, repeatname);
			return P_INVALID;
		}
		pscript_types[last].assoc = typenum;
	}

	ptype = &pscript_types[typenum];
	PScript_ResetToDefaults(ptype);
	ptype->loaded = 2;
	ptype->scale = 1;
	ptype->alpha = 0;
	ptype->alpharand = 1;
	ptype->alphachange = -1;
	VectorSet(ptype->rgb, 1, 1, 1);
	ptype->looks.type = PT_NORMAL;
	ptype->looks.blendmode = BM_PREMUL;
	ptype->looks.premul = 1;
	ptype->looks.scalefactor = 2;
	ptype->looks.invscalefactor = 0;
	ptype->looks.stretch = 1;
	EI_ApplySlot(ptype, &slots[EI_DEFAULTSLOT]);
	ptype->texsstride = 0;
	ptype->randsmax = 1;
	return typenum;
}

// Converts the block's DarkPlaces sizes, fades and lifetime, then finishes
// the type like a parsed one.
static void EI_FinishBlock(const ei_block_t *block)
{
	pscript_type_t *ptype = &pscript_types[block->type];
	qbool dieset = block->dieset;
	float die;

	switch (ptype->looks.type) {
		case PT_CDECAL:
			if (!dieset) {
				ptype->die = 20;
				dieset = true;
			}
			// Fades out over its whole life.
			ptype->alphachange = ptype->die > 0 ? -ptype->alpha / ptype->die : 0;
			break;
		case PT_UDECAL:
			// An unclipped decal has its corners at its scale from its
			// centre. Divided by the square root of two here and doubled by
			// the scale factor, half its side is DarkPlaces' size times
			// stretchfactor.
			ptype->looks.stretch *= 0.70710678f;
			ptype->scale *= ptype->looks.stretch;
			ptype->scalerand *= ptype->looks.stretch;
			ptype->scaledelta *= ptype->looks.stretch;
			ptype->looks.stretch = 1;
			break;
		case PT_NORMAL:
			// A textured particle has its corners at a quarter of its scale
			// from its centre. Doubled here and by the scale factor, that is
			// DarkPlaces' size. Finishing the type does not apply the scale
			// factor to the growth rate, so it gets both doublings here.
			// Billboards ignore stretchfactor.
			ptype->looks.stretch = 1;
			ptype->scale *= 2;
			ptype->scalerand *= 2;
			ptype->scaledelta *= 4;
			break;
		case PT_BEAM:
			// DarkPlaces draws a beam effect as one quad from start to end:
			// one segment at each end of the trail and none between.
			ptype->count = 0;
			ptype->countextra = 2;
			ptype->countrand = 0;
			ptype->countspacing = 10000;
			ptype->scale *= 0.5f;
			break;
		case PT_TEXTUREDSPARK:
			// DarkPlaces draws a spark 0.04 times its velocity times
			// stretchfactor long.
			ptype->scale *= 2;
			ptype->scalerand *= 2;
			ptype->scaledelta *= 2;
			ptype->looks.stretch *= 0.04f;
			break;
		default:
			break;
	}

	if (block->blooddecal) {
		// Splats into a decal instead of bouncing, where it collides.
		ptype->clipbounce = -2;
	}

	if (!dieset) {
		// Lives until it has faded out; one that never fades lives 15
		// seconds.
		if (ptype->alphachange < 0) {
			die = (ptype->alpha + ptype->alpharand) / -ptype->alphachange;
			ptype->die = bound(0, die, EI_MAXVALUE);
		}
		else {
			ptype->die = 15;
		}
	}
	ptype->looks.minstretch = 0.5f;
	PScript_FinishParticleType(ptype);
}

// Imports <config>.txt as the particle set config; config may be com_token.
// Returns false when config is too long or the file is missing or too large,
// true even when blocks or lines were skipped.
qbool PScript_ImportEffectInfo(const char *config)
{
	char setname[MAX_QPATH];
	char path[MAX_QPATH + 4];
	char line[1024];
	ei_slot_t slots[EI_NUMSLOTS];
	tokenizecontext_t ctx;
	ei_block_t block;
	char *data;
	const char *keyword;
	size_t offset = 0;
	qbool incomment = false, started = false;
	int size, linenum = 0, numeffects = 0, numskipped = 0, numignored = 0;

	// Namespaces in effect names are shorter than MAX_QPATH - 1.
	if (strlcpy(setname, config, sizeof(setname)) >= sizeof(setname) - 1) {
		Con_Printf("Particle set name too long: %s\n", setname);
		return false;
	}
	snprintf(path, sizeof(path), "%s.txt", setname);
	data = (char *)FS_LoadHeapFile(path, &size);
	if (!data) {
		Con_Printf("Couldn't find %s\n", path);
		return false;
	}
	if (size > EI_MAXFILESIZE) {
		Con_Printf("%s is larger than %d bytes, ignored\n", path, EI_MAXFILESIZE);
		Q_free(data);
		return false;
	}

	EI_LoadParticleFont(slots);

	block.type = P_INVALID;
	block.dieset = false;
	block.blooddecal = false;
	while (PScript_ReadLine(line, sizeof(line), data, (size_t)size, &offset)) {
		linenum++;
		EI_StripComments(line, &incomment);
		Cmd_TokenizeStringEx(&ctx, line);
		if (!Cmd_ArgcEx(&ctx)) {
			continue;
		}

		keyword = Cmd_ArgvEx(&ctx, 0);
		if (!strcmp(keyword, "effect")) {
			if (block.type != P_INVALID) {
				EI_FinishBlock(&block);
			}
			block.type = P_INVALID;
			block.dieset = false;
			block.blooddecal = false;
			started = true;
			if (++numeffects > EI_MAXEFFECTS) {
				Con_Printf("%s:%d: more than %d effects, ignored the rest\n", path, linenum, EI_MAXEFFECTS);
				break;
			}
			if (Cmd_ArgcEx(&ctx) != 2) {
				Con_DPrintf("%s:%d: effect needs one name\n", path, linenum);
			}
			else {
				block.type = EI_BeginBlock(setname, Cmd_ArgvEx(&ctx, 1), slots, path, linenum);
			}
			if (block.type == P_INVALID) {
				numskipped++;
			}
		}
		else if (!started) {
			Con_Printf("%s:%d: %s before the first effect, ignored the rest\n", path, linenum, keyword);
			break;
		}
		else if (block.type != P_INVALID && !EI_ParseLine(&block, &ctx, slots)) {
			Con_DPrintf("%s:%d: ignored %s\n", path, linenum, Cmd_MakeArgsEx(&ctx, 0));
			numignored++;
		}
	}
	if (block.type != P_INVALID) {
		EI_FinishBlock(&block);
	}
	Q_free(data);

	// Per-line reasons go to the developer log only, so a broken or hostile
	// file costs one console line.
	if (numskipped || numignored) {
		Con_Printf("%s: skipped %d effects and %d other lines, set developer 1 for details\n", path, numskipped, numignored);
	}
	pscript_effects_dirty = true;
	return true;
}
