OBJS	 = BlueSCSIToolbox.o CDChanger.o BlueSCSIlogo.o ZuluSCSIlogo.o Test.o scsi.o SDTransfer.o
CC	     = vc

DISTDIR  = ../dist

ifeq ($(OS),Windows_NT)
INCLUDES = "-ID:\Amiga\Include_H"
RM       = del
CP       = copy
MKDIR    = if not exist $(DISTDIR) mkdir $(DISTDIR)
PATHSEP  = \\
else
INCLUDES = -I/opt/amiga/m68k-amigaos/ndk-include -I/opt/amiga/m68k-amigaos/vbcc/include
RM       = rm
CP       = cp
MKDIR    = mkdir -p $(DISTDIR)
PATHSEP  = /
endif

CFLAGS   = +aos68k $(INCLUDES) -O2 -sc -c99 -cpu=68000
#CFLAGS  += -DDEBUG
LFLAGS   = -lamiga
#LFLAGS  += -ldebug

all: BlueSCSIToolbox CDChanger SDTransfer

BlueSCSIToolbox: BlueSCSIToolbox.o
	$(CC) +aos68k BlueSCSIToolbox.o -o BlueSCSIToolbox $(LFLAGS)

BlueSCSIToolbox.o: BlueSCSIToolbox.c
	$(CC) $(CFLAGS) -c BlueSCSIToolbox.c

CDChanger: CDChanger.o BlueSCSIlogo.o ZuluSCSIlogo.o scsi.o common.o
	$(CC) +aos68k CDChanger.o -o CDChanger $(LFLAGS) BlueSCSIlogo.o ZuluSCSIlogo.o scsi.o common.o

CDChanger.o: CDChanger.c BlueSCSIlogo.c ZuluSCSIlogo.c toolbox.h
	$(CC) $(CFLAGS) -c CDChanger.c

SDTransfer: SDTransfer.o BlueSCSIlogo.o ZuluSCSIlogo.o scsi.o common.o
	$(CC) +aos68k SDTransfer.o -o SDTransfer $(LFLAGS) BlueSCSIlogo.o ZuluSCSIlogo.o scsi.o common.o

SDTransfer.o: SDTransfer.c BlueSCSIlogo.c ZuluSCSIlogo.c toolbox.h
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
	$(RM) -f $(DISTDIR)$(PATHSEP)*
	$(CP) CDChanger $(DISTDIR)
	$(CP) CDChanger.info $(DISTDIR)
	$(CP) SDTransfer $(DISTDIR)
	$(CP) SDTransfer.info $(DISTDIR)
	$(CP) ..$(PATHSEP)LICENSE.txt $(DISTDIR)
	$(CP) BlueSCSIToolbox $(DISTDIR)

