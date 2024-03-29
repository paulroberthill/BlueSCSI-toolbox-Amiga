OBJS	 = BlueSCSIToolbox.o CDChanger.o BlueSCSIlogo.o Test.o scsi.o SDTransfer.o
CC	     = vc
CFLAGS   = +aos68k "-ID:\Amiga\Include_H" -O2 -sc -c99 -cpu=68020 -DDEBUG
LFLAGS	 = -lamiga -ldebug

all: BlueSCSIToolbox CDChanger SDTransfer

BlueSCSIToolbox: BlueSCSIToolbox.o
	$(CC) +aos68k BlueSCSIToolbox.o -o BlueSCSIToolbox $(LFLAGS)
	copy /Y BlueSCSIToolbox z:

BlueSCSIToolbox.o: BlueSCSIToolbox.c
	$(CC) $(CFLAGS) -c BlueSCSIToolbox.c

CDChanger: CDChanger.o BlueSCSIlogo.o scsi.o common.o
	$(CC) +aos68k CDChanger.o -o CDChanger $(LFLAGS) BlueSCSIlogo.o scsi.o common.o
	copy /Y CDChanger z:
	copy /Y CDChanger.info z:

CDChanger.o: CDChanger.c BlueSCSIlogo.c toolbox.h
	$(CC) $(CFLAGS) -c CDChanger.c


SDTransfer: SDTransfer.o BlueSCSIlogo.o scsi.o common.o
	$(CC) +aos68k SDTransfer.o -o SDTransfer $(LFLAGS) BlueSCSIlogo.o scsi.o common.o
	copy /Y SDTransfer z:
	copy /Y SDTransfer.info z:

SDTransfer.o: SDTransfer.c BlueSCSIlogo.c toolbox.h
	$(CC) $(CFLAGS) -c SDTransfer.c

scsi.o: scsi.c toolbox.h
	$(CC) $(CFLAGS) -c scsi.c

common.o: common.c toolbox.h
	$(CC) $(CFLAGS) -c common.c

BlueSCSIlogo.o: BlueSCSIlogo.c
	$(CC) $(CFLAGS) -c BlueSCSIlogo.c

Test: Test.o BlueSCSIlogo.o
	$(CC) +aos68k Test.o -o Test $(LFLAGS) BlueSCSIlogo.o
	copy /Y Test z:
	copy /Y Test.info z:

Test.o: Test.c BlueSCSIlogo.c
	$(CC) $(CFLAGS) -c Test.c

clean:
	del -f $(OBJS) BlueSCSIToolbox CDChanger
