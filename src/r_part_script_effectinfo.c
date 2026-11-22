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

// Lifetime of a particle that DarkPlaces keeps until it has faded out, and
// light time of a light that fades until its radius is gone.
#define EI_FOREVER EI_MAXVALUE

// A sprite's corners lie this many times its scale from its centre: a
// quarter of it along view axes 1.5 units long.
#define EI_SPRITECORNER 0.375f

// Units between the points of a beam along a trail state. The beam's head
// lags its projectile by up to this much, which a grenade's model hides; a
// grenade at 600 units per second spends 150 segments a second.
#define EI_BEAMSPACING 4

// Texture coordinates as a type stores them: t1 is the bottom edge.
typedef struct ei_slot_s {
	float s1, s2, t1, t2;
	char texname[MAX_QPATH];
} ei_slot_t;

typedef struct ei_block_s {
	int type;             // type being filled, P_INVALID while skipping a block
	qbool dieset;         // the block gave the lifetime
	qbool blooddecal;     // splats into a decal where it collides
	qbool entityparticle; // lives for one frame whatever its time
	qbool rain;           // removed where it hits a surface
	float rotate[4];      // start angle and spin ranges in degrees
	float lighttime;      // how long a fading light lasts
} ei_block_t;

// DarkPlaces' particle types. Its alpha and additive blends both become
// premultiplied blending, which blends with premul 1 and adds with premul
// 2, so either draws with one blend function. A beam's blend is converted
// to straight alpha when its block is finished.
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
	slot->t1 = 1;
	slot->t2 = 0;
	strlcpy(slot->texname, texname, sizeof(slot->texname));
}

// Fills the texture slots "tex" picks from: an 8x8 grid over
// particles/particlefont, repeated for slots past 63, unless
// particles/particlefont.txt moves a slot ("<slot> <s1> <t1> <s2> <t2>
// [<texture>]") or gives it a texture of its own ("<slot> <texture>").
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
		slots[i].t1 = (((i >> 3) & 7) + 1) / 8.0f;
		slots[i].t2 = ((i >> 3) & 7) / 8.0f;
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
		block->entityparticle = true;
	}
	else if (!strcmp(name, "rain")) {
		block->rain = true;
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
	if (strcmp(name, "billboard") && strcmp(name, "spark") && strcmp(name, "oriented") && strcmp(name, "beam")) {
		return false;
	}
	if (ptype->looks.type == PT_CDECAL) {
		// A decal splats onto the surface whatever its orientation.
		return true;
	}

	if (!strcmp(name, "billboard")) {
		ptype->looks.type = PT_NORMAL;
	}
	else if (!strcmp(name, "spark")) {
		ptype->looks.type = PT_TEXTUREDSPARK;
	}
	else if (!strcmp(name, "oriented")) {
		ptype->looks.type = PT_UDECAL;
	}
	else {
		ptype->looks.type = PT_BEAM;
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
	// than the largest pool holds. A negative count takes from the other.
	if (EI_Numbers(ctx, "count", 1, v)) {
		ptype->count = bound(-PSCRIPT_MAX_PARTICLES, v[0], PSCRIPT_MAX_PARTICLES);
		return true;
	}
	if (EI_Numbers(ctx, "countabsolute", 1, v)) {
		ptype->countextra = bound(-PSCRIPT_MAX_PARTICLES, v[0], PSCRIPT_MAX_PARTICLES);
		return true;
	}
	if (EI_Numbers(ctx, "trailspacing", 1, v)) {
		// Zero or less turns spacing off. The trail loop is bounded by the
		// pool, and so is the count a small spacing gives.
		if (v[0] > 0) {
			ptype->countspacing = v[0];
			ptype->count = min(1 / v[0], PSCRIPT_MAX_PARTICLES);
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
		// Either order; each particle lives a random time between the two
		// (die less up to randdie), so one with a negative time expires
		// before it is drawn. Two zeros mean until it has faded out.
		if (v[0] || v[1]) {
			ptype->die = max(v[0], v[1]);
			ptype->randdie = ptype->die - min(v[0], v[1]);
			block->dieset = true;
		}
		else {
			block->dieset = false;
		}
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
		// Any bounce but 0 collides: a negative one removes the particle
		// on impact, a positive one takes that many times its speed into
		// the surface off its velocity.
		ptype->clipbounce = v[0];
		ptype->cliptype = v[0] ? block->type : P_INVALID;
		return true;
	}
	if (EI_Numbers(ctx, "rotate", 4, v)) {
		memcpy(block->rotate, v, sizeof(block->rotate));
		// Start angle range in degrees and spin range in degrees per second.
		// At angle 0 a particle's corners lie on the view axes, so the start
		// is turned by 45 degrees to stand it upright as DarkPlaces draws it.
		ptype->rotationstartmin = (float)(DEG2RAD(v[0]) + M_PI / 4);
		ptype->rotationstartrand = (float)DEG2RAD(v[1] - v[0]);
		ptype->rotationmin = (float)DEG2RAD(v[2]);
		ptype->rotationrand = (float)DEG2RAD(v[3] - v[2]);
		return true;
	}
	// A radius of 0 or less spawns no light.
	if (EI_Numbers(ctx, "lightradius", 1, v)) {
		ptype->dl_radius[0] = v[0];
		ptype->dl_radius[1] = 0;
		return true;
	}
	if (EI_Numbers(ctx, "lightradiusfade", 1, v)) {
		ptype->dl_decay = v[0];
		return true;
	}
	if (EI_Numbers(ctx, "lightcolor", 3, v)) {
		VectorCopy(v, ptype->dl_rgb);
		return true;
	}
	if (EI_Numbers(ctx, "lighttime", 1, v)) {
		block->lighttime = v[0];
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

// How many times its size DarkPlaces draws a billboard or oriented particle
// with stretchfactor s. Turned by an angle a it draws a square with half side
// size * sqrt(s^2 cos^2 a + sin^2 a); unturned, a rectangle s times as wide
// as it is tall. A square has one size, so it gets the one with the same
// area, averaged over the angles when they vary.
static float EI_StretchScale(const ei_block_t *block, float s)
{
	const float *r = block->rotate;
	float a;

	if (r[0] == r[1] && r[2] == 0 && r[3] == 0) {
		if (r[0] == 0) {
			return sqrtf(fabsf(s));
		}
		a = (float)DEG2RAD(r[0]);
		return sqrtf(s * s * cosf(a) * cosf(a) + sinf(a) * sinf(a));
	}
	return sqrtf((s * s + 1) / 2);
}

// Converts the block's DarkPlaces sizes, fades, lifetime and light, then
// finishes the type like a parsed one. Finishing the type doubles scale and
// scalerand for the scale factor, but not scaledelta.
static void EI_FinishBlock(const ei_block_t *block)
{
	pscript_type_t *ptype = &pscript_types[block->type];
	qbool dieset = block->dieset;
	float die, f;

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
			// DarkPlaces draws a square with half side size, its corners
			// size * sqrt(2) from its centre. An unclipped decal has its
			// corners at its scale from its centre.
			f = EI_StretchScale(block, ptype->looks.stretch) * 0.70710678f;
			ptype->scale *= f;
			ptype->scalerand *= f;
			ptype->scaledelta *= 2 * f;
			ptype->looks.stretch = 1;
			break;
		case PT_NORMAL:
			// The same square as an oriented particle, facing the view. A
			// sprite has its corners EI_SPRITECORNER times its scale from its
			// centre.
			f = EI_StretchScale(block, ptype->looks.stretch) * 1.41421356f / (2 * EI_SPRITECORNER);
			ptype->scale *= f;
			ptype->scalerand *= f;
			ptype->scaledelta *= 2 * f;
			ptype->looks.stretch = 1;
			break;
		case PT_BEAM:
			// DarkPlaces draws a beam effect as one unmoving quad from the
			// start of the trail to its end, size to either side, its
			// texture repeating every 64 units over stretchfactor, and
			// nothing for a point effect. Here one segment from start to end
			// for a call without a trail state, and a point every
			// EI_BEAMSPACING units along one, so that a trail run every
			// frame costs the same at any frame rate; as wide as its scale
			// to either side, and its texture stepping by the angle per
			// unit.
			ptype->flags |= PT_SPANBEAM;
			ptype->count = 0;
			ptype->countextra = 0;
			ptype->countrand = 0;
			ptype->countspacing = EI_BEAMSPACING;
			ptype->scale *= 0.5f;
			ptype->scalerand *= 0.5f;
			ptype->rotationstartmin = ptype->looks.stretch / 64;
			ptype->rotationstartrand = 0;
			ptype->rotationmin = 0;
			ptype->rotationrand = 0;
			VectorClear(ptype->orgbias);
			VectorClear(ptype->orgwrand);
			VectorClear(ptype->velbias);
			VectorClear(ptype->velwrand);
			VectorClear(ptype->friction);
			ptype->veladd = 0;
			ptype->gravity = 0;
			ptype->flurry = 0;
			ptype->cliptype = P_INVALID;
			// A beam takes its particles' colours as they are, so it gets the
			// straight alpha blend with the same result: colour and texture
			// scaled by alpha, and added or blended over. Inverse modulation
			// has no such blend; it darkens by the full colour whatever the
			// alpha.
			if (ptype->looks.blendmode == BM_PREMUL) {
				ptype->looks.blendmode = ptype->looks.premul == 2 ? BM_ADDA : BM_BLEND;
				ptype->looks.premul = 0;
			}
			// A slot as tall as its texture runs it along the beam in t.
			ptype->looks.vbeam = ptype->t1 == 1 && ptype->t2 == 0;
			break;
		case PT_TEXTUREDSPARK:
			// DarkPlaces draws a spark size to either side and 0.04 times its
			// velocity times stretchfactor, but at least half its size, to
			// either end. A spark here is as wide as its scale, which
			// finishing doubles, and as long as its stretch times its speed,
			// or a quarter of its scale, to either end. A stretch of 0 or
			// less draws the shortest spark, as a tiny positive one does here.
			ptype->scaledelta *= 2;
			ptype->looks.stretch *= 0.04f;
			if (!(ptype->looks.stretch > 0)) {
				ptype->looks.stretch = 1e-30f;
			}
			break;
		default:
			break;
	}

	if (block->blooddecal) {
		// Splats into a decal instead of bouncing, where it collides.
		ptype->clipbounce = -2;
	}
	if (block->rain) {
		// Removed where it hits a surface, whatever its bounce.
		ptype->clipbounce = -1;
		ptype->cliptype = block->type;
	}

	if (block->entityparticle) {
		ptype->die = 0;
		ptype->randdie = 0;
	}
	else if (!dieset) {
		// Lives until it has faded out, which one that does not fade
		// never does.
		if (ptype->alphachange < 0) {
			die = (ptype->alpha + ptype->alpharand) / -ptype->alphachange;
			ptype->die = bound(0, die, EI_FOREVER);
		}
		else {
			ptype->die = EI_FOREVER;
		}
	}

	// A light fades only with both a fade and a time, until its radius is
	// gone or the time is up; any other light lasts one frame.
	if (ptype->dl_radius[0] > 0 && ptype->dl_decay > 0 && block->lighttime > 0) {
		ptype->dl_time = min(block->lighttime, ptype->dl_radius[0] / ptype->dl_decay);
	}
	else {
		ptype->dl_decay = 0;
		ptype->dl_time = 0;
	}

	ptype->looks.minstretch = 0.5f;
	PScript_FinishParticleType(ptype);
}

// Starts a block with DarkPlaces' defaults for what the type does not hold.
static void EI_ClearBlock(ei_block_t *block)
{
	block->type = P_INVALID;
	block->dieset = false;
	block->blooddecal = false;
	block->entityparticle = false;
	block->rain = false;
	block->rotate[0] = 0;
	block->rotate[1] = 360;
	block->rotate[2] = 0;
	block->rotate[3] = 0;
	block->lighttime = EI_FOREVER;
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

	EI_ClearBlock(&block);
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
			EI_ClearBlock(&block);
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
