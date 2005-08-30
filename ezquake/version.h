// version.h

#define	QW_VERSION		2.40

#ifdef _WIN32
#define QW_PLATFORM	"Win32"
#endif
#ifdef __APPLE__
#define QW_PLATFORM "MacOSX"
#endif
#ifndef QW_PLATFORM
#define QW_PLATFORM	"Linux"
#endif

#ifdef GLQUAKE
#define QW_RENDERER	"GL"
#else
#define QW_RENDERER "Soft"
#endif

int build_number (void);
void CL_Version_f (void);
char *VersionString (void);
