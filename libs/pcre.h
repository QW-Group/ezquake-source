/*
	I got tired to merge new pcre.h with "disconnect's PCRE_STATIC fix"
	each time I update the lib, and since build/config process started to be made with CMAKE
	there is this an option to just copy CMAKE-generated PCRE's config.h overwriting old one.
	
	Just rename config.h to pcre.config.<arch>.h and copy it here.

	-AAS	
*/


#ifdef _WIN32
#include "pcre.config.win.h"
#endif

#ifdef linux
#include "pcre.config.linux.h"
#endif

#ifdef __APPLE__
#include "pcre.config.mac.h"
#endif

#ifdef __FreeBSD__
#include "pcre.config.freebsd.h"
#endif

/* an old header under new name */
#include "pcre.header.h"
