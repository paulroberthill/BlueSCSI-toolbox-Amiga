OBJS	 = BlueSCSIToolbox.o CDChanger.o BlueSCSIlogo.o Test.o scsi.o SDTransfer.o
CC	     = vc
CFLAGS   = +aos68k "-ID:\Amiga\Include_H" -O2 -sc -c99 -cpu=68020
# -DDEBUG
LFLAGS	 = -lamiga -ldebug

all: BlueSCSIToolbox CDChanger SDTransfer

BlueSCSIToolbox: BlueSCSIToolbox.o
	$(CC) +aos68k BlueSCSIToolbox.o -o BlueSCSIToolbox $(LFLAGS)

BlueSCSIToolbox.o: BlueSCSIToolbox.c
	$(CC) $(CFLAGS) -c BlueSCSIToolbox.c

CDChanger: CDChanger.o BlueSCSIlogo.o scsi.o common.o
	$(CC) +aos68k CDChanger.o -o CDChanger $(LFLAGS) BlueSCSIlogo.o scsi.o common.o

CDChanger.o: CDChanger.c BlueSCSIlogo.c toolbox.h
	$(CC) $(CFLAGS) -c CDChanger.c

SDTransfer: SDTransfer.o BlueSCSIlogo.o scsi.o common.o
	$(CC) +aos68k SDTransfer.o -o SDTransfer $(LFLAGS) BlueSCSIlogo.o scsi.o common.o

SDTransfer.o: SDTransfer.c BlueSCSIlogo.c toolbox.h
	$(CC) $(CFLAGS) -c SDTransfer.c

scsi.o: scsi.c toolbox.h
	$(CC) $(CFLAGS) -c scsi.c

common.o: common.c toolbox.h
	$(CC) $(CFLAGS) -c common.c

BlueSCSIlogo.o: BlueSCSIlogo.c
	$(CC) $(CFLAGS) -c BlueSCSIlogo.c

clean:
	del -f $(OBJS) BlueSCSIToolbox CDChanger

release:
	del /q ..\dist\*.*
	copy CDChanger ..\dist
	copy CDChanger.info ..\dist
	copy SDTransfer ..\dist
	copy SDTransfer.info ..\dist
	copy LICENSE.txt ..\dist
	copy BlueSCSIToolbox ..\dist

	