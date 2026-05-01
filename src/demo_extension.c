/*
 * Copyright (C) 2026 Oscar Linderholm <osm@recv.se>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "quakedef.h"
#include "demo_extension.h"
#include "demo_extension_voice.h"
#include "fs.h"

#define DEMO_EXTENSION_AUTOEXEC "autoexec.cfg"

#ifdef WITH_ZIP
static unzFile demo_extension_zip;
#endif

void DemoExtension_Init(void)
{
	DemoExtensionVoice_Init();
}

void DemoExtension_UpdateFrame(void)
{
	DemoExtensionVoice_UpdateFrame();
}

static void DemoExtension_Unload(void)
{
	DemoExtensionVoice_Unload();
}

void DemoExtension_StartDemo(void)
{
	DemoExtensionVoice_StartDemo();
}

void DemoExtension_StopDemo(void)
{
	DemoExtensionVoice_StopDemo();
}

static qbool DemoExtension_IsSafePath(const char *path)
{
	const char *segment;

	if (!path[0] || path[0] == '/' || path[0] == '\\' || strchr(path, ':')) {
		return false;
	}

	segment = path;
	while (*segment) {
		size_t len;
		const char *slash = strpbrk(segment, "/\\");

		len = slash ? (size_t)(slash - segment) : strlen(segment);
		if (!len || (len == 1 && segment[0] == '.') ||
			(len == 2 && segment[0] == '.' && segment[1] == '.')) {
			return false;
		}

		if (!slash) {
			break;
		}
		segment = slash + 1;
	}

	return true;
}

qbool DemoExtension_LoadFile(const char *path, byte **data, int *len)
{
#ifdef WITH_ZIP
	size_t unpacked_len;
	void *loaded;

	if (!demo_extension_zip || !DemoExtension_IsSafePath(path)) {
		return false;
	}

	loaded = FS_ZipUnpackOneFileToMemory(
		demo_extension_zip, path, false, false, NULL, &unpacked_len);
	if (!loaded || unpacked_len > INT_MAX) {
		if (loaded) {
			Q_free(loaded);
		}
		return false;
	}

	*data = loaded;
	*len = unpacked_len;
	return true;
#else
	return false;
#endif
}

#ifdef WITH_ZIP
static void *DemoExtension_ZipOpenFile(void *file, const char *filename, int mode)
{
	(void)filename;

	if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER) != ZLIB_FILEFUNC_MODE_READ) {
		return NULL;
	}

	return file;
}

static unsigned long DemoExtension_ZipReadFile(void *file, void *stream, void *buf,
	unsigned long size)
{
	(void)file;
	return VFS_READ((vfsfile_t *)stream, buf, size, NULL);
}

static unsigned long DemoExtension_ZipWriteFile(void *file, void *stream,
	const void *buf, unsigned long size)
{
	(void)file;
	(void)stream;
	(void)buf;
	(void)size;
	return 0;
}

static long DemoExtension_ZipTellFile(void *file, void *stream)
{
	(void)file;
	return VFS_TELL((vfsfile_t *)stream);
}

static long DemoExtension_ZipSeekFile(void *file, void *stream, unsigned long offset,
	int origin)
{
	int whence;

	(void)file;
	switch (origin) {
	case ZLIB_FILEFUNC_SEEK_CUR:
		whence = SEEK_CUR;
		break;
	case ZLIB_FILEFUNC_SEEK_END:
		whence = SEEK_END;
		break;
	case ZLIB_FILEFUNC_SEEK_SET:
		whence = SEEK_SET;
		break;
	default:
		return -1;
	}

	return VFS_SEEK((vfsfile_t *)stream, offset, whence);
}

static int DemoExtension_ZipCloseFile(void *file, void *stream)
{
	(void)file;
	(void)stream;
	return 0;
}

static int DemoExtension_ZipErrorFile(void *file, void *stream)
{
	(void)file;
	(void)stream;
	return 0;
}

static unzFile DemoExtension_OpenZip(vfsfile_t *file, const char *name)
{
	zlib_filefunc_def funcs;

	funcs.zopen_file = DemoExtension_ZipOpenFile;
	funcs.zread_file = DemoExtension_ZipReadFile;
	funcs.zwrite_file = DemoExtension_ZipWriteFile;
	funcs.ztell_file = DemoExtension_ZipTellFile;
	funcs.zseek_file = DemoExtension_ZipSeekFile;
	funcs.zclose_file = DemoExtension_ZipCloseFile;
	funcs.zerror_file = DemoExtension_ZipErrorFile;
	funcs.opaque = file;

	return unzOpen2(name, &funcs);
}
#endif

static qbool DemoExtension_ExecConfig(vfsfile_t *file, const char *name,
	const char *config)
{
#ifdef WITH_ZIP
	int cfg_len;
	byte *cfg;
	char *text;
	qbool loaded = false;
	unzFile zip;

	if (!file) {
		return false;
	}

	zip = DemoExtension_OpenZip(file, name);
	if (!zip) {
		return false;
	}

	demo_extension_zip = zip;
	if (DemoExtension_LoadFile(config, &cfg, &cfg_len)) {
		text = Q_malloc_named(cfg_len + 2, config);
		if (!text) {
			Q_free(cfg);
			demo_extension_zip = NULL;
			FS_ZipUnpackCloseFile(zip);
			return false;
		}
		memcpy(text, cfg, cfg_len);
		text[cfg_len] = '\n';
		text[cfg_len + 1] = 0;
		Q_free(cfg);

		Cbuf_AddTextEx(&cbuf_demo_extension, text);
		Cbuf_ExecuteEx(&cbuf_demo_extension);
		Q_free(text);
		loaded = true;
		Com_Printf("demo_extension: loaded %s from %s\n",
			config, COM_SkipPath(name));
	}
	demo_extension_zip = NULL;

	FS_ZipUnpackCloseFile(zip);
	return loaded;
#else
	return false;
#endif
}

qbool DemoExtension_Load(vfsfile_t *file, const char *name)
{
	qbool loaded;
	unsigned long pos;

	if (!file) {
		return false;
	}

	DemoExtension_Unload();
	pos = VFS_TELL(file);
	loaded = DemoExtension_ExecConfig(file, name, DEMO_EXTENSION_AUTOEXEC);
	VFS_SEEK(file, pos, SEEK_SET);
	return loaded;
}
