/*
 * real jconfig.h is created when compiling the libjpeg library during ./configure phase
 * and is different (and incompatible) accross platforms (this occurs since update to version 7)
 * so we've created this meta-jconfig.h to make the whole thing virtually cross-platform
 */

#ifdef _WIN32
#include "jconfig.win.h"
#endif

#ifdef linux
#include "jconfig.linux.h"
#endif

#ifdef __APPLE__
#include "jconfig.mac.h"
#endif

#ifdef __FreeBSD__
#include "jconfig.bsd.h"
#endif
