/* 
 * Copyright (C) 2024 Paul Hill
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. 
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <clib/alib_protos.h>
#include <clib/exec_protos.h>
#include <devices/scsidisk.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/expansion.h>
#include <proto/utility.h>
#include <stdlib.h>

#define BLUESCSI_TOOLBOX_COUNT_FILES 0xD2
#define BLUESCSI_TOOLBOX_LIST_FILES 0xD0
#define BLUESCSI_TOOLBOX_GETFILE 0xD1
#define BLUESCSI_TOOLBOX_SEND_FILE_PREP 0xD3
#define BLUESCSI_TOOLBOX_SEND_FILE_10 0xD4
#define BLUESCSI_TOOLBOX_SEND_FILE_END 0xD5
#define BLUESCSI_TOOLBOX_TOGGLE_DEBUG 0xD6
#define BLUESCSI_TOOLBOX_LIST_CDS 0xD7
#define BLUESCSI_TOOLBOX_SET_NEXT_CD 0xD8
#define BLUESCSI_TOOLBOX_LIST_DEVICES 0xD9
#define BLUESCSI_TOOLBOX_COUNT_CDS 0xDA

#define SCSI_CMD_INQ 0x12

// from BlueSCSI_Toolbox.cpp
#define MAX_MAC_PATH 32

int Toolbox_List_Files(int cdrom);
int Toolbox_List_Devices();
int Toolbox_Count_Files(int cdrom);
int Toolbox_GetFileByName(char *destination, char *source);
int Toolbox_PutFileByName(char *destination, char *source);
int Toolbox_List_CDs();
void Toolbox_Show_files();
void Toolbox_Next_CD(int index);
void Toolbox_Debug(int debugon);

void dump(char *msg, UBYTE *d, int len);
void DiskChange();
void bstrcpy(char *dest,UBYTE *src);
int DoScsiCmd(UBYTE *data, int datasize, UBYTE *cmd, int cmdsize, UBYTE flags);
int BlueSCSI_InitDevice();

struct IOStdReq *io_ptr;
struct MsgPort *mp_ptr;
struct SCSICmd *scsi_cmd;
UBYTE *scsi_sense;
UBYTE *scsi_data = NULL;
struct Library *UtilityBase = NULL;

LONG scsi_removable;
LONG scsi_isCD;
LONG scsi_isBlueSCSI;

UBYTE scsi_dev[1024];
LONG scsi_id = 0;

#define SENSE_LEN 252
#define MAX_DATA_LEN 4096

struct FileEntry
{
   int Index;
   unsigned int Size;
   int Type;
   char Name[32 + 1];
};

struct FileEntry *files = NULL;
int filecount = 0;

// ReadArgs template
char *template = "DEVICE/K,UNIT/K/N,DIR=LIST/S,SEND/K,RECEIVE/K,LISTDEVICES/S,LISTCDS/S,SETCD/K/N,SETDEBUG/K/N";

enum ToolboxCommand
{
   TOOLBOX_NONE,
   TOOLBOX_DIR,
   TOOLBOX_SEND,
   TOOLBOX_RECEIVE,
   TOOLBOX_LISTDEVICES,
   TOOLBOX_LISTCDS,
   TOOLBOX_SETCD,
   TOOLBOX_SETDEBUG
};

enum ToolboxParams
{
   DEVICE,
   UNIT,
   DIR,
   SEND,
   RECEIVE,
   LISTDEVICES,
   LISTCDS,
   SETCD,
   SETDEBUG
};

int main(int argc, char* argv[])
{
   struct RDArgs *rd;
   enum ToolboxCommand toolboxCommand = TOOLBOX_NONE;
   char filename[256];
   LONG params[] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
   LONG nextcd;
   LONG debugon;
   
   if ((UtilityBase = OpenLibrary("utility.library", 37L)) == NULL)
   {
      goto exit;
   }

   rd = ReadArgs(template, params, NULL);
   if (rd)
   {
      if (params[DEVICE])
      {
         Strncpy(scsi_dev, (char *)params[0], sizeof(scsi_dev));
      }
      if (params[UNIT])
      {
         scsi_id = (*((LONG *)params[1]));
      }
      if (params[DIR])
      {
         toolboxCommand = TOOLBOX_DIR;
      }
      if (params[SEND])
      {
         toolboxCommand = TOOLBOX_SEND;
         Strncpy(filename, (char *)params[SEND], 256);
      }
      if (params[RECEIVE])
      {
         toolboxCommand = TOOLBOX_RECEIVE;
         Strncpy(filename, (char *)params[RECEIVE], 256);
      }
      if (params[LISTDEVICES])
      {
         toolboxCommand = TOOLBOX_LISTDEVICES;
      }
      if (params[LISTCDS])
      {
         toolboxCommand = TOOLBOX_LISTCDS;
      }
      if (params[SETCD])
      {
         toolboxCommand = TOOLBOX_SETCD;
         nextcd = (*((LONG *)params[SETCD]));
      }
      if (params[SETDEBUG])
      {
         toolboxCommand = TOOLBOX_SETDEBUG;
         debugon = (*((LONG *)params[SETDEBUG]));
      }
      FreeArgs(rd);
   }
   else
   {
      SetIoErr(ERROR_REQUIRED_ARG_MISSING);
      PrintFault(IoErr(), argv[0]);
      return 5;
   }

   if (toolboxCommand == TOOLBOX_NONE)
   {
      SetIoErr(ERROR_REQUIRED_ARG_MISSING);
      PrintFault(IoErr(), argv[0]);
      return 5;
   }

   if ((mp_ptr = (struct MsgPort *)CreateMsgPort()) == NULL)
   {
      PutStr("CreatePort failed!\n");
      goto exit;
   }
   if ((io_ptr = (struct IOStdReq *)CreateIORequest(mp_ptr, sizeof(struct IOStdReq))) == NULL)
   {
      PutStr("CreateIORequest failed!\n");
      goto exit;
   }
   if (OpenDevice(scsi_dev, scsi_id, (struct IORequest *)io_ptr, 0) != 0)
   {
      Printf("Error %ld opening SCSI device %s unit %ld\n", io_ptr->io_Error, scsi_dev, scsi_id);
      goto exit;
   }
   if ((scsi_cmd = (struct SCSICmd *)AllocMem(sizeof(struct SCSICmd), MEMF_CLEAR)) == NULL)
   {
      PutStr("AllocMem scsi_cmd failed\n");
      goto exit;
   }
   if ((scsi_sense = (UBYTE *)AllocMem(SENSE_LEN, MEMF_CLEAR)) == NULL)
   {
      PutStr("AllocMem scsi_sense failed\n");
      goto exit;
   }
   scsi_data = (UBYTE *)AllocMem(MAX_DATA_LEN, MEMF_CLEAR);
   if (scsi_data == NULL)
   {
      PutStr("AllocMem scsi_data failed\n");
      goto exit;
   }
   
   // Init the device and read some flags
   if (BlueSCSI_InitDevice())
   {
      PutStr("Error sending inquiry to device\n");
      goto exit;
   }
   if (!scsi_isBlueSCSI)
   {
      PutStr("Not a BlueSCSI device\n");
      goto exit;
   }

   switch (toolboxCommand)
   {
   case TOOLBOX_DIR:
      Toolbox_List_Files(0);
      Toolbox_Show_files();
      break;
   case TOOLBOX_LISTCDS:
      if (scsi_removable)
      {
         Toolbox_List_Files(1);
         Toolbox_Show_files();
      }
      else
      {
         PutStr("Not a CDROM\n");
      }
      break;
   case TOOLBOX_SEND:
      Toolbox_PutFileByName(FilePart(filename), filename);
      break;
   case TOOLBOX_RECEIVE:
      Toolbox_List_Files(0);
      Toolbox_GetFileByName(filename, FilePart(filename));
      break;
   case TOOLBOX_LISTDEVICES:
      Toolbox_List_Devices();
      break;
   case TOOLBOX_SETCD:
      if (scsi_removable)
      {
         Toolbox_List_Files(1);
         if (nextcd<=0 || nextcd>(filecount))
         {
            SetIoErr(ERROR_BAD_NUMBER);
            PrintFault(IoErr(), NULL);
         }
         else
         {
            Toolbox_Next_CD(nextcd);
            DiskChange();
         }
      }
      else
      {
         PutStr("Not a CDROM\n");
      }
      break;
   case TOOLBOX_SETDEBUG:
      Toolbox_Debug(debugon);
      break;
   }

exit:

   if (UtilityBase) CloseLibrary(UtilityBase);
   if (files) FreeMem(files, sizeof(struct FileEntry) * filecount);
   if (scsi_data) FreeMem(scsi_data, MAX_DATA_LEN);
   if (scsi_sense) FreeMem(scsi_sense, SENSE_LEN);
   if (io_ptr) 
   {
      CloseDevice((struct IORequest *)io_ptr);
      DeleteIORequest(io_ptr);
   }
   if (mp_ptr) DeleteMsgPort(mp_ptr);
}

/* Send a diskchange command to the filesystem that is connected to this device/unit */
void DiskChange()
{
   char drive[256];
	char device[256];
   int found = 0;
   
   struct DosList *dl;
   dl = LockDosList(LDF_DEVICES | LDF_READ);

   while ((dl = NextDosEntry(dl,LDF_DEVICES)))
   {
      struct FileSysStartupMsg *fssm = BADDR(dl->dol_misc.dol_handler.dol_Startup);
      if (TypeOfMem(fssm) && (APTR)fssm > (APTR)1000)
      {
         if (fssm->fssm_Unit == scsi_id)
         {
            bstrcpy(device, BADDR(fssm->fssm_Device));
            if (Stricmp(scsi_dev, device) == 0)
            {
               bstrcpy(drive, BADDR(dl->dol_Name));
               found = 1;
               break;
            }
         }
      }
   }

   UnLockDosList (LDF_DEVICES | LDF_READ);

   if (found)
   {
      Strncat(drive, ":", 256); 
      Inhibit(drive, DOSTRUE);
      Inhibit(drive, DOSFALSE);
   }   
}

/* Send a SCSI inquiry command to the device to gather some info */
int BlueSCSI_InitDevice()
{
   UBYTE command[] = {SCSI_CMD_INQ, 0, 0, 0, 252, 0};
   int err;

   if ((err = DoScsiCmd((UBYTE *)scsi_data, MAX_DATA_LEN,
                        (UBYTE *)&command, sizeof(command),
                        (SCSIF_READ | SCSIF_AUTOSENSE))) != 0)
   {
      Printf("SCSI error %ld\n", err);
      return -1;
   }

   if (scsi_cmd->scsi_Actual)
   {
      scsi_removable = (scsi_data[1] & 0x80) ? 1 : 0;
      scsi_isCD = (scsi_data[0] & 0x1F) ? 0x05 : 0x00;
      scsi_isBlueSCSI = Strnicmp("BlueSCSI", &scsi_data[8], 8) == 0;
   }
   return 0;
}

/* Copy a BCPL string to a C string */
void bstrcpy(char *dest, UBYTE *src)
{
   int len = *src++;
   Strncpy(dest, src, len + 1);
   dest[len] = 0;
}

/* Show all the files array */
void Toolbox_Show_files()
{
   struct FileEntry *file = files;
   for (int i = 0; i < filecount; i++)
   {
      Printf("%2ld: %-32s", i+1, file->Name);
      if (file->Type == 1)
         Printf("%10lu\n", file->Size);
      else
         PutStr("       Dir\n");
      file++;
   }
}

/* Select CD image 'n'
   First CD is 1 */
void Toolbox_Next_CD(int index)
{
   UBYTE command[] = {BLUESCSI_TOOLBOX_SET_NEXT_CD, 0, 0, 0, 0, 0};
   command[1] = index - 1;
   int err;
   if ((err = DoScsiCmd((UBYTE *)scsi_data, MAX_DATA_LEN,
                        (UBYTE *)&command, sizeof(command),
                        (SCSIF_READ | SCSIF_AUTOSENSE))) != 0)
   {
      Printf("SCSI error %ld\n", err);
      return;
   }
}

/* Count the number of files/CD rom images */
int Toolbox_Count_Files(int cdrom)
{
   UBYTE command[] = {BLUESCSI_TOOLBOX_COUNT_FILES, 0, 0, 0, 0, 0};
   if (cdrom)
   {
      command[0] = BLUESCSI_TOOLBOX_COUNT_CDS;
   }

   int err;
   int count = 0;

   // Causes an Unknown MsgID error if sent to a HD!?
   if ((err = DoScsiCmd((UBYTE *)scsi_data, MAX_DATA_LEN,
                        (UBYTE *)&command, sizeof(command),
                        (SCSIF_READ | SCSIF_AUTOSENSE))) != 0)
   {
      Printf("SCSI error %ld\n", err);
      return -1;
   }

   if (scsi_cmd->scsi_Actual)
   {
      count = (int)scsi_data[0];
      if (count > 0)
      {
         filecount = count;
         files = (struct FileEntry *)AllocMem(sizeof(struct FileEntry) * count, MEMF_CLEAR);
      }
   }
   return count;
}

/* Devices that are active on this SCSI device.
   Does not seem to return anything? */
int Toolbox_List_Devices()
{
   // 0xD9 BLUESCSI_TOOLBOX_LIST_DEVICES
   UBYTE command[] = {BLUESCSI_TOOLBOX_LIST_DEVICES, 0, 0, 0, 0, 0};
   int err;
   
   if ((err = DoScsiCmd((UBYTE *)scsi_data, MAX_DATA_LEN,
                        (UBYTE *)&command, sizeof(command),
                        (SCSIF_READ | SCSIF_AUTOSENSE))) != 0)
   {
      Printf("SCSI error %ld\n", err);
      return -1;
   }

#if DEBUG
   if (scsi_cmd->scsi_Actual)
   {
      dump("Toolbox_List_Devices", scsi_data, scsi_cmd->scsi_Actual);
   }
#endif

   return 0;
}

/* Used for sorting dirs/files */
int FileCompare(const void *s1, const void *s2)
{
   struct FileEntry *e1 = (struct FileEntry *)s1;
   struct FileEntry *e2 = (struct FileEntry *)s2;
   if (e1->Type == e2->Type)
      return Stricmp(e1->Name, e2->Name);
   else
      return e1->Type - e2->Type;
}

/* List the files in the shared folder / CD images */
int Toolbox_List_Files(int cdrom)
{
   // Update the file count
   Toolbox_Count_Files(cdrom);

   UBYTE command[] = {BLUESCSI_TOOLBOX_LIST_FILES, 0, 0, 0, 0, 0};
   if (cdrom)
   {
      command[0] = BLUESCSI_TOOLBOX_LIST_CDS;
   }
   int err;

   if ((err = DoScsiCmd((UBYTE *)scsi_data, MAX_DATA_LEN,
                        (UBYTE *)&command, sizeof(command),
                        (SCSIF_READ | SCSIF_AUTOSENSE))) != 0)
   {
      Printf("SCSI error %ld\n", err);
      return -1;
   }

   if (scsi_cmd->scsi_Actual)
   {
      UBYTE *c = scsi_data;
      struct FileEntry *file = files;
      for (int f = 0; f < filecount; f++)
      {
         file->Index = (int)*c++;
         file->Type = (int)*c++; // 1=file 0=dir

         Strncpy(file->Name, c, 32);

         c += MAX_MAC_PATH + 2;
         unsigned int size = c[0] << 24 | c[1] << 16 | c[2] << 8 | c[3];
         c += 4;

         file->Size = size;
         file++;
      }

      qsort(files, filecount, sizeof(struct FileEntry), FileCompare);
   }
   return 0;
}

/* Copy a file from the shared folder to a destination */
int Toolbox_GetFileByName(char *destination, char *source)
{
   int count = 0;
   int index = -1;
   struct FileEntry *file = files;
   for (int i = 0; i < filecount; i++)
   {
      if (Stricmp(file->Name, source) == 0)
      {
         index = file->Index;
         break;
      }
      file++;
   }

   if (index >= 0)
   {
      int offset = 0; // offset in 4096 size pages
      UBYTE command[] = {BLUESCSI_TOOLBOX_GETFILE, 0, 0, 0, 0, 0};
      command[1] = index;

      BPTR fh = Open(destination, MODE_NEWFILE);
      if (!fh)
      {
         SetIoErr(ERROR_OBJECT_NOT_FOUND);
         PrintFault(IoErr(), destination);
         return 0;
      }

      while (1)
      {
         command[2] = (offset & 0xFF000000) >> 24;
         command[3] = (offset & 0x00FF0000) >> 16;
         command[4] = (offset & 0x0000FF00) >> 8;
         command[5] = (offset & 0x000000FF);
         int err;

         if ((err = DoScsiCmd((UBYTE *)scsi_data, MAX_DATA_LEN,
                              (UBYTE *)&command, sizeof(command),
                              (SCSIF_READ | SCSIF_AUTOSENSE))) != 0)
         {
            Printf("SCSI error %ld\n", err);
            break;
         }

         if (scsi_cmd->scsi_Actual)
         {
            count += scsi_cmd->scsi_Actual;
            offset++;
            Write(fh, scsi_data, scsi_cmd->scsi_Actual);
         }
         else
         {
            break;
         }
      }
      Close(fh);
      Printf("%s. %ld bytes received\n", destination, count);
   }
   else
   {
      SetIoErr(ERROR_OBJECT_NOT_FOUND);
      PrintFault(IoErr(), destination);
   }
   
   return count;
}

/* Write a file to the shared folder */
int Toolbox_PutFileByName(char *destination, char *source)
{
   int err;
   int count = 0;

   UBYTE command[] = {BLUESCSI_TOOLBOX_SEND_FILE_PREP, 0, 0, 0, 0, 0};

   char *name = destination;
   int i = 0;
   while (*name)
   {
      scsi_data[i++] = *name++;
   }
   scsi_data[i++] = '\0';

   if ((err = DoScsiCmd((UBYTE *)scsi_data, MAX_DATA_LEN,
                        (UBYTE *)&command, sizeof(command),
                        (SCSIF_READ | SCSIF_AUTOSENSE))) != 0)
   {
      Printf("SCSI error %ld\n", err);
      return 0;
   }

   BPTR fh = Open(source, MODE_OLDFILE);
   if (!fh)
   {
      SetIoErr(ERROR_OBJECT_NOT_FOUND);
      PrintFault(IoErr(), source);
      return 0;
   }

   int offset = 0; // offset in 512 byte pages
   while (1)
   {
      LONG len = Read(fh, scsi_data, 512);
      if (len == 0)
      {
         break;
      }

      command[0] = BLUESCSI_TOOLBOX_SEND_FILE_10;
      command[1] = (len & 0xFF00) >> 8;
      command[2] = (len & 0xFF);
      command[3] = (offset & 0xFF0000) >> 16;
      command[4] = (offset & 0xFF00) >> 8;
      command[5] = (offset & 0xFF);
      if ((err = DoScsiCmd((UBYTE *)scsi_data, MAX_DATA_LEN,
                           (UBYTE *)&command, sizeof(command),
                           (SCSIF_READ | SCSIF_AUTOSENSE))) != 0)
      {
         Printf("SCSI error %ld\n", err);
         return -1;
      }

      offset++;
   }

   command[0] = BLUESCSI_TOOLBOX_SEND_FILE_END;
   if ((err = DoScsiCmd((UBYTE *)scsi_data, MAX_DATA_LEN,
                        (UBYTE *)&command, sizeof(command),
                        (SCSIF_READ | SCSIF_AUTOSENSE))) != 0)
   {
      Printf("SCSI error %ld\n", err);
      return 0;
   }
   
   Close(fh);
   return count;
}

/* Enable/Disable BlueSCSI debug */
void Toolbox_Debug(debugon)
{
   UBYTE command[] = {BLUESCSI_TOOLBOX_TOGGLE_DEBUG, 0, 0, 0, 0, 0};
   command[1] = debugon;
   int err;
   int count = 0;

   if ((err = DoScsiCmd((UBYTE *)scsi_data, MAX_DATA_LEN,
                        (UBYTE *)&command, sizeof(command),
                        (SCSIF_READ | SCSIF_AUTOSENSE))) != 0)
   {
      Printf("SCSI error %ld\n", err);
   }
   else
   {
      Printf("Debug set to %ld\n", debugon);
   }
}

/* Send a SCSI command */
int DoScsiCmd(UBYTE *data, int datasize, UBYTE *cmd, int cmdsize, UBYTE flags)
{
   int i;
   io_ptr->io_Length = sizeof(struct SCSICmd);
   io_ptr->io_Data = scsi_cmd;
   io_ptr->io_Command = HD_SCSICMD;

//Printf("io_ptr->io_Flags=%lx\n", io_ptr->io_Flags);
//   io_ptr->io_Flags = 0;

   scsi_cmd->scsi_Data = (UWORD *)data;
   scsi_cmd->scsi_Length = datasize;
   scsi_cmd->scsi_SenseActual = 0;
   scsi_cmd->scsi_SenseLength = SENSE_LEN;
   scsi_cmd->scsi_SenseData = scsi_sense;
   scsi_cmd->scsi_Command = cmd;
   scsi_cmd->scsi_CmdLength = cmdsize;
   scsi_cmd->scsi_Flags = flags;
dump("\nCalling DoIO", cmd, cmdsize);
   DoIO((struct IORequest *)io_ptr);
Printf("io_Error=%lx\n", io_ptr->io_Error);
   return (io_ptr->io_Error);
}

#if DEBUG
void dump(char *msg, UBYTE *d, int len)
{
   Printf("%s (%lu) ", msg, (ULONG)len);
   for (int i = 0; i < len; i++)
   {
      if (i % 16 == 0)
         Printf("\n%04lx   ", (ULONG)i);
      Printf("%02lx ", d[i]);
   }
   Printf("\n");
}
#endif
