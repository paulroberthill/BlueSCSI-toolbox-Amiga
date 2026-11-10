OBJS	 = BlueSCSIToolbox.o CDChanger.o BlueSCSIlogo.o ZuluSCSIlogo.o common.o scsi.o SDTransfer.o
CC	     = vc

ifeq ($(OS),Windows_NT)
PATHSEP  = \\
else
PATHSEP  = /
endif

DISTDIR  = ..$(PATHSEP)dist

ifeq ($(OS),Windows_NT)
INCLUDES = "-ID:\Amiga\Include_H"
RM       = del /q
CP       = copy
MKDIR    = if not exist $(DISTDIR) mkdir $(DISTDIR)
MKDIRP   = if not exist $(1) mkdir $(1)
RMTREE   = if exist $(1) rmdir /s /q $(1)
else
INCLUDES = -I/opt/amiga/m68k-amigaos/ndk-include -I/opt/amiga/m68k-amigaos/vbcc/include
RM       = rm -f
CP       = cp
MKDIR    = mkdir -p $(DISTDIR)
MKDIRP   = mkdir -p $(1)
RMTREE   = rm -rf $(1)
endif

TIMEVAL_DEFS = -D__USE_NEW_TIMEVAL__ -Dtimeval=TimeVal -Dtimerequest=TimeRequest -Dtv_sec=tv_secs -Dtv_usec=tv_micro

CFLAGS   = +aos68k $(INCLUDES) $(TIMEVAL_DEFS) -O2 -sc -c99 -cpu=68000
#CFLAGS  += -DDEBUG
LFLAGS   = -lamiga
#LFLAGS  += -ldebug

all: BlueSCSIToolbox CDChanger SDTransfer

BlueSCSIToolbox: BlueSCSIToolbox.o
	$(CC) +aos68k BlueSCSIToolbox.o -o BlueSCSIToolbox $(LFLAGS)

BlueSCSIToolbox.o: BlueSCSIToolbox.c toolbox.h version.h
	$(CC) $(CFLAGS) -c BlueSCSIToolbox.c

CDChanger: CDChanger.o BlueSCSIlogo.o ZuluSCSIlogo.o scsi.o common.o
	$(CC) +aos68k CDChanger.o -o CDChanger $(LFLAGS) BlueSCSIlogo.o ZuluSCSIlogo.o scsi.o common.o

CDChanger.o: CDChanger.c BlueSCSIlogo.c ZuluSCSIlogo.c toolbox.h version.h
	$(CC) $(CFLAGS) -c CDChanger.c

SDTransfer: SDTransfer.o BlueSCSIlogo.o ZuluSCSIlogo.o scsi.o common.o
	$(CC) +aos68k SDTransfer.o -o SDTransfer $(LFLAGS) BlueSCSIlogo.o ZuluSCSIlogo.o scsi.o common.o

SDTransfer.o: SDTransfer.c BlueSCSIlogo.c ZuluSCSIlogo.c toolbox.h version.h
	$(CC) $(CFLAGS) -c SDTransfer.c

scsi.o: scsi.c toolbox.h
	$(CC) $(CFLAGS) -c scsi.c

common.o: common.c toolbox.h
	$(CC) $(CFLAGS) -c common.c

BlueSCSIlogo.o: BlueSCSIlogo.c
	$(CC) $(CFLAGS) -c BlueSCSIlogo.c

ZuluSCSIlogo.o: ZuluSCSIlogo.c
	$(CC) $(CFLAGS) -c ZuluSCSIlogo.c

clean:
	$(RM) -f $(OBJS) BlueSCSIToolbox CDChanger

release:
	$(MKDIR)
	$(RM) $(DISTDIR)$(PATHSEP)*
	$(CP) CDChanger $(DISTDIR)
	$(CP) CDChanger.info $(DISTDIR)
	$(CP) SDTransfer $(DISTDIR)
	$(CP) SDTransfer.info $(DISTDIR)
	$(CP) ..$(PATHSEP)LICENSE.txt $(DISTDIR)
	$(CP) BlueSCSIToolbox $(DISTDIR)

# Aminet release: stages the archive contents in ../aminet/build and packs
# them into ../aminet/build/BlueSCSI-toolbox.lha with LHa for UNIX (lha on
# the PATH). Upload the .lha and the .readme from ../aminet/build.
# AMINETNAME matches the existing Aminet entry disk/misc/BlueSCSI-toolbox,
# so new uploads replace it; the drawer in the archive is ARCNAME.
# Note: the 2005 Windows lha.exe cannot list its own level 2 archives
# ("Unknown level header"), but the archives are fine; check with 7z t.
LHA       = lha
ARCNAME   = BlueSCSI-Toolbox
AMINETNAME = BlueSCSI-toolbox
AMINETDIR = ..$(PATHSEP)aminet
STAGEDIR  = $(AMINETDIR)$(PATHSEP)build
ARCDIR    = $(STAGEDIR)$(PATHSEP)$(ARCNAME)
ARCSRCDIR = $(ARCDIR)$(PATHSEP)src
ARCFILES  = BlueSCSIToolbox CDChanger CDChanger.info SDTransfer SDTransfer.info
SRCFILES  = BlueSCSIToolbox.c CDChanger.c SDTransfer.c scsi.c common.c \
            BlueSCSIlogo.c ZuluSCSIlogo.c toolbox.h version.h \
            CDChanger.info SDTransfer.info Makefile smakefile

aminet: all
	$(call RMTREE,$(STAGEDIR))
	$(call MKDIRP,$(ARCSRCDIR))
	$(foreach f,$(ARCFILES),$(CP) $(f) $(ARCDIR) &&) $(CP) ..$(PATHSEP)LICENSE.txt $(ARCDIR)
	$(CP) $(AMINETDIR)$(PATHSEP)$(ARCNAME).readme $(ARCDIR)
	$(foreach f,$(SRCFILES),$(CP) $(f) $(ARCSRCDIR) &&) echo Source copied
	$(CP) $(AMINETDIR)$(PATHSEP)$(ARCNAME).info $(STAGEDIR)
	cd $(STAGEDIR) && $(LHA) aq $(AMINETNAME).lha $(ARCNAME) $(ARCNAME).info
	$(CP) $(AMINETDIR)$(PATHSEP)$(ARCNAME).readme $(STAGEDIR)$(PATHSEP)$(AMINETNAME).readme

