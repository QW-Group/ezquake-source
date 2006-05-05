#======================================================================
# ezQuake Makefile
# based on: Fuhquake Makefile && ZQuake Makefile && JoeQuake Makefile
#======================================================================


# compilation tool and detection of targets/achitecture
_E			=
CC			=gcc
CC_BASEVERSION		=$(shell $(CC) -dumpversion | sed -e 's/\..*//g')
MACHINE			=$(shell $(CC) -dumpmachine)
ARCH			=$(shell echo $(MACHINE) | sed -e 's/.*mingw32.*/mingw32/g' -e 's/\-.*//g' -e 's/i.86/x86/g')
STRIP			=strip --strip-unneeded --remove-section=.comment

# Mac OSX Tiger : powerpc -> ppc
ifeq ($(MACHINE),powerpc-apple-darwin8) # MacOS-10.4/ppc
	ARCH		=ppc
endif

# Mac OSX Tiger : i686 -> macx86
ifeq ($(MACHINE),i686-apple-darwin8) # MacOS-10.4/x86
	ARCH		=macx86
endif

# add special architecture based flags
ifeq ($(ARCH),x86)	# Linux/x86
	DEST_ARCH	=x86
	ARCH_CFLAGS	=-march=$(shell echo $(MACHINE) | sed -e 's/\-.*//g')
endif
ifeq ($(ARCH),mingw32)	# Win32/x86 in MingW environment
	DEST_ARCH	=x86
	ARCH_CFLAGS	=-mwin32 -mno-cygwin
endif
ifeq ($(ARCH),ppc)	# MacOS-X/ppc
	DEST_ARCH	=ppc
	ARCH_CFLAGS	=-arch ppc -faltivec -maltivec -mcpu=7450 -mtune=7450 -mpowerpc -mpowerpc-gfxopt
	ifeq ($(CC_BASEVERSION),4) # auto vectorize if we're using gcc4.0+
	    ARCH_CFLAGS += -ftree-vectorize
	endif
endif


################

default_target: glx

all: glx x11 svga

################################
# Directories for object files #
################################

GLX_DIR		:= release-$(ARCH)/glx
X11_DIR		:= release-$(ARCH)/x11
SVGA_DIR	:= release-$(ARCH)/svga
MAC_DIR		:= release-$(ARCH)/mac

################
# Binary files #
################

GLX_TARGET	:= release-$(ARCH)/ezquake-gl.glx
X11_TARGET	:= release-$(ARCH)/ezquake.x11
SVGA_TARGET	:= release-$(ARCH)/ezquake.svga
MAC_TARGET	:= release-$(ARCH)/ezquake-gl.mac

################

C_BUILD = $(_E)$(CC) -c -o $@ $(_CFLAGS) $<
S_BUILD = $(_E)$(CC) -c -o $@ $(_CFLAGS) -DELF -x assembler-with-cpp $<
BUILD = $(_E)$(CC) -o $@ $(_OBJS) $(_LDFLAGS)
MKDIR = $(_E)mkdir -p $@
RM_FILES = $(_E)rm -f $(_FILES)
RM_DIRS = $(_E)rm -fr $(_DIRS)

################

$(GLX_DIR) $(X11_DIR) $(SVGA_DIR) $(MAC_DIR):
	$(MKDIR)

# compiler flags
PRJ_CFLAGS =-DWITH_ZLIB -DWITH_PNG -DEMBED_TCL
XMMS_CFLAGS =-DWITH_XMMS `glib-config --cflags`
BASE_CFLAGS =-Wall -Wsign-compare $(PRJ_CFLAGS) $(ARCH_CFLAGS)
BASE_RELEASE_CFLAGS =-ffast-math -O2 -fomit-frame-pointer -fexpensive-optimizations -funsigned-char -fno-strict-aliasing -pipe
ifneq ($(CC_BASEVERSION),4) # if we're not auto-vectorizing then we can unroll the loops (mdfour ahoy)
	BASE_RELEASE_CFLAGS  +=-funroll-loops
endif

ifeq ($(ARCH),ppc)
	BASE_RELEASE_CFLAGS +=-falign-loops=16 -falign-jumps=16 -falign-functions=16
else
	BASE_RELEASE_CFLAGS +=-falign-loops=2 -falign-jumps=2 -falign-functions=2
endif

BASE_DEBUG_CFLAGS               =-ggdb -D_DEBUG

ifeq ($(ARCH),x86)              # Linux/x86
	BASE_CFLAGS             +=-Did386 $(XMMS_CFLAGS)
endif
ifeq ($(ARCH),mingw32)          # Win32/x86 in MingW environment
	# use define for special assembly routines:
	BASE_CFLAGS             +=-Did386 -DMINGW32
endif
ifeq ($(ARCH),ppc)              # MacOS-X/ppc
	BASE_CFLAGS             +=-D__BIG_ENDIAN__ -Ddarwin
endif
ifeq ($(ARCH),macx86)           # MacOS-X/x86
	BASE_CFLAGS             +=-Ddarwin
endif
ifeq ($(ARCH),powerpc)          # Linux/PPC
	BASE_CFLAGS             +=-D__BIG_ENDIAN__
endif

RELEASE_CFLAGS                  =$(BASE_CFLAGS) $(BASE_RELEASE_CFLAGS) -DNDEBUG
DEBUG_CFLAGS                    =$(BASE_CFLAGS) $(BASE_DEBUG_CFLAGS)

# opengl builds
ifeq ($(ARCH),x86)              # Linux/x86
	ARCH_GLCFLAGS           =-I/usr/include -DWITH_VMODE -DWITH_DGA -DWITH_EVDEV
endif
ifeq ($(ARCH),mingw32)          # Win32/x86 in MingW environment
	ARCH_GLCFLAGS           =-mwindows -I/opt/xmingw/include/
endif
ifeq ($(ARCH),ppc)              # MacOS-X/ppc
	ARCH_GLCFLAGS           =-I/opt/local/include/ -I/Developer/Headers/FlatCarbon -I/sw/include -FOpenGL -FAGL
endif
ifeq ($(ARCH),macx86)           # MacOS-X/x86
	ARCH_GLCFLAGS           =-I/opt/local/include/ -I/Developer/Headers/FlatCarbon -I/sw/include -FOpenGL -FAGL
endif

GLCFLAGS=$(ARCH_GLCFLAGS) $(BASE_GLCFLAGS)

CFLAGS += $(RELEASE_CFLAGS)
LDFLAGS := -lm -ldl -lpthread

COMMON_OBJS := libs/libz.a libs/libpng.a libs/libpcre.a libs/libexpat.a libs/libtcl8.4.a libs/libglib.a libs/libjpeg.a

#######
# GLX #
#######

GLX_C_FILES := \
	Ctrl		Ctrl_EditBox	Ctrl_PageViewer		Ctrl_Tab \
	vid_glx		vid_common_gl	EX_FileList		EX_FunNames \
	EX_browser	EX_browser_net	EX_browser_ping		EX_browser_sources \
	EX_misc		auth		cd_linux		cl_cam \
	cl_cmd		cl_collision	cl_demo			cl_ents \
	cl_input	cl_main		cl_parse		cl_pred \
	cl_screen	cl_slist	cl_tcl			cl_tent \
	cl_view		cmd		collision		com_msg \
	common		common_draw	config_manager		console \
	crc		cvar		document_rendering	fchecks \
	fmod		fragstats	gl_draw			gl_md3 \
	gl_mesh		gl_model	gl_ngraph		gl_refrag \
	gl_rlight	gl_rmain	gl_rmisc		gl_rpart \
	gl_rsurf	gl_texture	gl_warp			help \
	help_browser	help_files	host			hud \
	hud_common	ignore		image			keymap_x11 \
	keys		localtime_linux	logging			match_tools \
	mathlib		mdfour		menu			modules \
	movie		mp3_player	mvd_utils		net_chan \
	net_udp		parser		pmove			pmovetst \
	pr_cmds		pr_edict	pr_exec			qtv \
	r_part		rulesets	sbar			sha1 \
	skin		snd_alsa	snd_dma			snd_linux\
	snd_mem		snd_mix		snd_oss			sv_ccmds \
	sv_ents		sv_init		sv_main			sv_master \
	sv_move		sv_nchan	sv_phys			sv_save	\
	sv_send		sv_user		sv_world		sys_linux \
	teamplay	utils		version			vx_camera \
	vx_coronas	vx_motiontrail	vx_stuff		vx_tracker \
	vx_vertexlights	wad		xml_test		xsd \
	xsd_command	xsd_document	xsd_variable		zone

GLX_S_FILES := \
	cl_math		math		snd_mixa		sys_x86

GLX_C_OBJS := $(addprefix $(GLX_DIR)/, $(addsuffix .o, $(GLX_C_FILES)))
GLX_S_OBJS := $(addprefix $(GLX_DIR)/, $(addsuffix .o, $(GLX_S_FILES)))

GLX_CFLAGS := $(CFLAGS) $(GLCFLAGS) -DGLQUAKE -DWITH_JPEG -I/usr/X11R6/include

GLX_LDFLAGS := $(LDFLAGS) -lGL -L/usr/X11R6/lib -lX11 -lXext # -lXxf86dga -lXxf86vm

glx: _DIR := $(GLX_DIR)
glx: _OBJS := $(GLX_C_OBJS) $(GLX_S_OBJS) $(COMMON_OBJS) /usr/lib/libXxf86vm.a /usr/lib/libXxf86dga.a
glx: _LDFLAGS := $(GLX_LDFLAGS)
glx: _CFLAGS := $(GLX_CFLAGS)
glx: $(GLX_TARGET)

$(GLX_TARGET): $(GLX_DIR) $(GLX_C_OBJS) $(GLX_S_OBJS)
	$(BUILD)
	$(STRIP) $(GLX_TARGET)

$(GLX_C_OBJS): $(GLX_DIR)/%.o: %.c
	$(C_BUILD)

$(GLX_S_OBJS): $(GLX_DIR)/%.o: %.s
	$(S_BUILD)

glxclean: _FILES += $(GLX_C_OBJS) $(GLX_S_OBJS)
glxclean: _DIRS := $(GLX_DIR)
glxclean:
	$(RM_FILES)
	$(RM_DIRS)

glxclobber: _FILES := $(GLX_TARGET)
glxclobber: glxclean

#######
# X11 #
#######

X11_C_FILES := \
	Ctrl		Ctrl_EditBox	Ctrl_PageViewer		Ctrl_Tab \
	vid_x11 	EX_FileList	EX_FunNames		EX_browser \
	EX_browser_net	EX_browser_ping	EX_browser_sources	EX_misc \
	auth		cd_linux	cl_cam			cl_cmd \
	cl_demo		cl_ents		cl_input		cl_main \
	cl_parse	cl_pred		cl_screen		cl_slist \
	cl_tcl		cl_tent		cl_view			cmd \
	com_msg		common		common_draw		config_manager \
	console		crc		cvar			d_edge \
	d_fill		d_init		d_modech		d_polyse \
	d_scan		d_sky		d_sprite		d_surf \
	d_vars		d_zpoint	document_rendering	fchecks \
	fmod		fragstats	help			help_browser \
	help_files	host		hud			hud_common \
	ignore		image		keymap_x11		keys \
	localtime_linux	logging		match_tools		\
	mathlib		mdfour		menu			modules	\
	movie		mp3_player	mvd_utils		net_chan \
	net_udp		parser		pmove			pmovetst \
	pr_cmds		pr_edict	pr_exec			qtv \
	r_aclip		r_alias		r_bsp			r_draw \
	r_edge		r_efrag		r_light			r_main \
	r_misc		r_model		r_part			r_rast \
	r_sky		r_sprite	r_surf			r_vars \
	rulesets	sbar		sha1			skin \
	snd_alsa	snd_dma		snd_linux		snd_mem \
	snd_mix		snd_oss		sv_ccmds		sv_ents \
	sv_init		sv_main		sv_master		sv_move \
	sv_nchan	sv_phys		sv_save			sv_send \
	sv_user		sv_world	sys_linux		teamplay \
	utils		version		wad			xml_test \
	xsd		xsd_command	xsd_document		xsd_variable \
	zone

X11_S_FILES := \
	cl_math		d_draw		d_draw16	d_parta \
	d_polysa	d_scana		d_spr8		d_varsa \
	math		r_aclipa	r_aliasa	r_drawa \
	r_edgea		r_varsa		snd_mixa	surf8 \
	sys_x86

X11_C_OBJS := $(addprefix $(X11_DIR)/, $(addsuffix .o, $(X11_C_FILES)))
X11_S_OBJS := $(addprefix $(X11_DIR)/, $(addsuffix .o, $(X11_S_FILES)))

X11_CFLAGS := $(CFLAGS) -D_Soft_X11

X11_LDFLAGS := $(LDFLAGS) -L/usr/X11R6/lib -lX11 -lXext 

x11: _DIR := $(X11_DIR)
x11: _OBJS := $(X11_C_OBJS) $(X11_S_OBJS) $(COMMON_OBJS)
x11: _LDFLAGS := $(X11_LDFLAGS)
x11: _CFLAGS := $(X11_CFLAGS)
x11: $(X11_TARGET)

$(X11_TARGET): $(X11_DIR) $(X11_C_OBJS) $(X11_S_OBJS)
	$(BUILD)
	$(STRIP) $(X11_TARGET)

$(X11_C_OBJS): $(X11_DIR)/%.o: %.c quakedef.h
	$(C_BUILD)

$(X11_S_OBJS): $(X11_DIR)/%.o: %.s
	$(S_BUILD)

x11clean: _FILES += $(X11_C_OBJS) $(X11_S_OBJS)
x11clean: _DIRS := $(X11_DIR)
x11clean:
	$(RM_FILES)
	$(RM_DIRS)

x11clobber: _FILES := $(X11_TARGET)
x11clobber: x11clean

########
# SVGA #
########

SVGA_C_FILES := \
	Ctrl		Ctrl_EditBox	Ctrl_PageViewer		Ctrl_Tab \
	vid_svgalib	EX_FileList	EX_FunNames		EX_browser \
	EX_browser_net	EX_browser_ping	EX_browser_sources	EX_misc \
	auth		cd_linux	cl_cam			cl_cmd \
	cl_demo		cl_ents		cl_input		cl_main \
	cl_parse	cl_pred		cl_screen		cl_slist \
	cl_tcl		cl_tent		cl_view			cmd \
	com_msg		common		common_draw		config_manager \
	console		crc		cvar			d_edge \
	d_fill		d_init		d_modech		d_polyse \
	d_scan		d_sky		d_sprite		d_surf \
	d_vars		d_zpoint	document_rendering	fchecks \
	fmod		fragstats	help			help_browser \
	help_files	host		hud			hud_common \
	ignore		image		keymap_x11		keys \
	localtime_linux	logging		match_tools		\
	mathlib		mdfour		menu			modules \
	movie		mp3_player	mvd_utils		net_chan \
	net_udp		parser		pmove			pmovetst \
	pr_cmds		pr_edict	pr_exec			qtv \
	r_aclip		r_alias		r_bsp			r_draw \
	r_edge		r_efrag		r_light			r_main \
	r_misc		r_model		r_part			r_rast \
	r_sky		r_sprite	r_surf			r_vars \
	rulesets	sbar		sha1			skin \
	snd_alsa	snd_dma		snd_linux		snd_mem \
	snd_mix		snd_oss		sv_ccmds		sv_ents \
	sv_init		sv_main		sv_master		sv_move \
	sv_nchan	sv_phys		sv_save			sv_send \
	sv_user		sv_world	sys_linux		teamplay \
	utils		version		wad			xml_test \
	xsd		xsd_command	xsd_document		xsd_variable \
	zone

SVGA_S_FILES := \
	cl_math		d_draw		d_draw16	d_parta \
	d_polysa	d_scana		d_spr8		d_varsa \
	math		r_aclipa	r_aliasa	r_drawa \
	r_edgea		r_varsa		snd_mixa	surf8 \
	sys_x86		d_copy

SVGA_C_OBJS := $(addprefix $(SVGA_DIR)/, $(addsuffix .o, $(SVGA_C_FILES)))
SVGA_S_OBJS := $(addprefix $(SVGA_DIR)/, $(addsuffix .o, $(SVGA_S_FILES)))

SVGA_CFLAGS := $(CFLAGS) -D_Soft_SVGA

SVGA_LDFLAGS := $(LDFLAGS) -lvga

svga: _DIR := $(SVGA_DIR)
svga: _OBJS := $(SVGA_C_OBJS) $(SVGA_S_OBJS) $(COMMON_OBJS)
svga: _LDFLAGS := $(SVGA_LDFLAGS)
svga: _CFLAGS := $(SVGA_CFLAGS)
svga: $(SVGA_TARGET)

$(SVGA_TARGET): $(SVGA_DIR) $(SVGA_C_OBJS) $(SVGA_S_OBJS)
	$(BUILD)
	$(STRIP) $(SVGA_TARGET)

$(SVGA_C_OBJS): $(SVGA_DIR)/%.o: %.c quakedef.h
	$(C_BUILD)

$(SVGA_S_OBJS): $(SVGA_DIR)/%.o: %.s
	$(S_BUILD)

svgaclean: _FILES += $(SVGA_C_OBJS) $(SVGA_S_OBJS)
svgaclean: _DIRS := $(SVGA_DIR)
svgaclean:
	$(RM_FILES)
	$(RM_DIRS)

svgaclobber: _FILES := $(SVGA_TARGET)
svgaclobber: svgaclean

#######
# MAC #
#######

MAC_C_FILES := \
	CarbonSndPlayDB		Ctrl			Ctrl_EditBox		Ctrl_PageViewer \
	Ctrl_Tab		EX_FileList		EX_FunNames		EX_browser \
	EX_browser_net		EX_browser_ping		EX_browser_sources	EX_misc \
	auth			cd_null			cl_cam			cl_cmd \
	cl_collision		cl_demo			cl_ents			cl_input \
	cl_main			cl_parse		cl_pred			cl_screen \
	cl_slist		cl_tcl			cl_tent			cl_view \
	cmd			collision		com_msg			common \
	common_draw		config_manager		console			crc \
	cvar			document_rendering	fchecks			fmod \
	fragstats		gl_draw			gl_md3			gl_mesh \
	gl_model		gl_ngraph		gl_refrag		gl_rlight \
	gl_rmain		gl_rmisc		gl_rpart		gl_rsurf \
	gl_texture		gl_warp			help			help_browser \
	help_files		host			hud			hud_common \
	ignore			image			in_mac			keys \
	localtime_linux		logging			mac_prefs		match_tools \
	mathlib			mdfour			menu			modules \
	movie			mp3_player		mvd_utils		net_chan \
	net_udp			parser			pmove			pmovetst \
	pr_cmds			pr_edict		pr_exec			qtv \
	r_part			rulesets		sbar			sha1 \
	skin			snd_dma			snd_mac			snd_mem \
	snd_mix			sv_ccmds		sv_ents			sv_init \
	sv_main			sv_master		sv_move			sv_nchan \
	sv_phys			sv_save			sv_send			sv_user \
	sv_world		sys_mac			teamplay		utils \
	version			vid_common_gl		vid_mac			vx_camera \
	vx_coronas		vx_motiontrail		vx_stuff		vx_tracker \
	vx_vertexlights		wad			xml_test		xsd \
	xsd_command		xsd_document		xsd_variable		zone

MAC_C_OBJS := $(addprefix $(MAC_DIR)/, $(addsuffix .o, $(MAC_C_FILES)))

MAC_CFLAGS := $(CFLAGS)

MAC_LDFLAGS := $(LDFLAGS)

mac: _DIR := $(MAC_DIR)
mac: _OBJS := $(MAC_C_OBJS)
mac: _LDFLAGS := $(MAC_LDFLAGS)
mac: _CFLAGS := $(MAC_CFLAGS)
mac: $(MAC_TARGET)

$(MAC_TARGET): $(MAC_DIR) $(MAC_C_OBJS) # FIXME
	$(BUILD)
	$(STRIP) $(MAC_TARGET)

$(MAC_C_OBJS): $(MAC_DIR)/%.o: %.c quakedef.h
	$(C_BUILD)

macclean: _FILES += $(MAC_C_OBJS)
macclean: _DIRS := $(MAC_DIR)
macclean:
	$(RM_FILES)
	$(RM_DIRS)

macclobber: _FILES := $(MAC_TARGET)
macclobber: macclean



#################
clean: glxclean x11clean svgaclean macclean

clobber: _DIRS := $(GLX_DIR) $(X11_DIR) $(SVGA_DIR) $(MAC_DIR)
clobber:
	$(RM_DIRS)
