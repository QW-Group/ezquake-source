/* jconfig.vc --- jconfig.h for Microsoft Visual C++ on Windows 95 or NT. */
/* see jconfig.doc for explanations */

#define HAVE_PROTOTYPES
#define HAVE_UNSIGNED_CHAR
#define HAVE_UNSIGNED_SHORT
#ifndef _WIN32
#undef void
#undef const
#else // _WIN32
/* #define void char */
/* #define const */
#endif // _WIN32
#undef CHAR_IS_UNSIGNED
#define HAVE_STDDEF_H
#define HAVE_STDLIB_H
#undef NEED_BSD_STRINGS
#undef NEED_SYS_TYPES_H
#undef NEED_FAR_POINTERS	/* we presume a 32-bit flat memory model */
#undef NEED_SHORT_EXTERNAL_NAMES
/* Define this if you get warnings about undefined structures. */
#undef INCOMPLETE_TYPES_BROKEN

/* Define "boolean" as unsigned char, not int, per Windows custom */
#ifdef _WIN32
#	ifndef __RPCNDR_H__		/* don't conflict if rpcndr.h already read */
typedef unsigned char boolean;
#	endif // __RPCNDR_H__
#else // _WIN32
typedef int boolean;
#endif // _WIN32
#define HAVE_BOOLEAN		/* prevent jmorecfg.h from redefining it */


#ifdef JPEG_INTERNALS

#undef RIGHT_SHIFT_IS_UNSIGNED
#ifndef _WIN32
#define INLINE __inline__
/* These are for configuring the JPEG memory manager. */
#undef DEFAULT_MAX_MEM
#undef NO_MKTEMP
#endif // _WIN32
#endif /* JPEG_INTERNALS */

#ifdef JPEG_CJPEG_DJPEG

#define BMP_SUPPORTED		/* BMP image file format */
#define GIF_SUPPORTED		/* GIF image file format */
#define PPM_SUPPORTED		/* PBMPLUS PPM/PGM image file format */
#undef RLE_SUPPORTED		/* Utah RLE image file format */
#define TARGA_SUPPORTED		/* Targa image file format */

#define TWO_FILE_COMMANDLINE	/* optional */
#ifdef _WIN32
#define USE_SETMODE		/* Microsoft has setmode() */
#endif // _WIN32
#undef NEED_SIGNAL_CATCHER
#undef DONT_USE_B_MODE

/* Define this if you want percent-done progress reports from cjpeg/djpeg. */
#undef PROGRESS_REPORT		/* optional */

#endif /* JPEG_CJPEG_DJPEG */
