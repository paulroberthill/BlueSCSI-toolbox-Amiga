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
#include <stdio.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include <devices/scsidisk.h>
#include "toolbox.h"

#define SENSE_LEN 252
#define MAX_DATA_LEN 4096

struct MsgPort *scsiPort;
struct IOStdReq *io_ptr;
struct SCSICmd *scsi_cmd;
UBYTE *scsi_sense;
UBYTE *scsi_data;

int scsi_isCD;
int scsi_isBlueSCSI;
int scsi_isRemovable;

struct FileEntry *files = NULL; 
int filecount = 0;

int Toolbox_InitDevice();

/* Setup the SCSI device */
int scsi_setup(char *scsi_dev, int scsi_unit)
{
   if ((scsiPort = CreateMsgPort()) == NULL)
   {
      return -1;
   }
   if ((io_ptr = (struct IOStdReq *)CreateIORequest(scsiPort, sizeof(struct IOStdReq))) == NULL)
   {
      return -1;
   }
   if (OpenDevice(scsi_dev, scsi_unit, (struct IORequest *)io_ptr, 0) != 0)
   {
      MessageBox("scsi_setup", "Error opening SCSI device");
      return -1;
   }
   if ((scsi_cmd = (struct SCSICmd *)AllocMem(sizeof(struct SCSICmd), MEMF_CLEAR)) == NULL)
   {
      return -1;
   }
   if ((scsi_sense = (UBYTE *)AllocMem(SENSE_LEN, MEMF_CLEAR)) == NULL)
   {
      return -1;
   }
   if ((scsi_data = (UBYTE *)AllocMem(MAX_DATA_LEN, MEMF_CLEAR)) == NULL)
   {
      return -1;
   }

   // Init the device and check some flags
   if (Toolbox_InitDevice() < 0)
   {
      MessageBox("scsi_setup", "Error sending inquiry to device\n");
      return -1;
   }
   return 0;
}

void scsi_cleanup()
{
   if (files) FreeMem(files, sizeof(struct FileEntry) * (filecount + 1));
   if (scsi_data) FreeMem(scsi_data, MAX_DATA_LEN);
   if (scsi_sense) FreeMem(scsi_sense, SENSE_LEN);
   if (scsi_cmd) FreeMem(scsi_cmd, sizeof(struct SCSICmd));

   if (io_ptr) 
   {
      CloseDevice((struct IORequest *)io_ptr);
      DeleteIORequest(io_ptr);
   }
   if (scsiPort) DeleteMsgPort(scsiPort);
}

/* Send a SCSI command */
int DoScsiCmd(UBYTE *data, int datasize, UBYTE *cmd, int cmdsize, UBYTE flags)
{
   int i;
   io_ptr->io_Length = sizeof(struct SCSICmd);
   io_ptr->io_Data = scsi_cmd;
   io_ptr->io_Command = HD_SCSICMD;
   scsi_cmd->scsi_Data = (UWORD *)data;
   scsi_cmd->scsi_Length = datasize;
   scsi_cmd->scsi_SenseActual = 0;
   scsi_cmd->scsi_SenseLength = SENSE_LEN;
   scsi_cmd->scsi_SenseData = scsi_sense;
   scsi_cmd->scsi_Command = cmd;
   scsi_cmd->scsi_CmdLength = cmdsize;
   scsi_cmd->scsi_Flags = flags;
   DoIO((struct IORequest *)io_ptr);
   return (io_ptr->io_Error);
}

/* Send a SCSI inquiry command to the device to gather some info */
int Toolbox_InitDevice()
{
   int err = 0;
#ifdef TESTMODE
   scsi_isCD = 1;
   scsi_isBlueSCSI = 1;
   scsi_isRemovable = 1;
#else
   UBYTE command[] = {SCSI_CMD_INQ, 0, 0, 0, 252, 0};

   if ((err = DoScsiCmd((UBYTE *)scsi_data, MAX_DATA_LEN,
                        (UBYTE *)&command, sizeof(command),
                        (SCSIF_READ | SCSIF_AUTOSENSE))) != 0)
   {
      return -1;
   }

   if (scsi_cmd->scsi_Actual)
   {
      scsi_isCD = (scsi_data[0] & 0x1F) ? 0x05 : 0x00;
      scsi_isRemovable = (scsi_data[1] & 0x80) ? 1 : 0;
      scsi_isBlueSCSI = (scsi_isRemovable == 1) && Strnicmp("BlueSCSI", &scsi_data[8], 8) == 0;
   }
#endif
   return err;
}

/* Execute BLUESCSI_TOOLBOX_COUNT_CDS / BLUESCSI_TOOLBOX_COUNT_FILES */
int Toolbox_Count_Files(int cdrom)
{
   UBYTE command[] = {BLUESCSI_TOOLBOX_COUNT_FILES, 0, 0, 0, 0, 0};
   int err;
   int count = 0;
   if (cdrom)
   {
      command[0] = BLUESCSI_TOOLBOX_COUNT_CDS;
   }

   if ((err = DoScsiCmd((UBYTE *)scsi_data, MAX_DATA_LEN,
                        (UBYTE *)&command, sizeof(command),
                        (SCSIF_READ | SCSIF_AUTOSENSE))) != 0)
   {
      MessageBox("Toolbox_Count_Files", "SCSI error\n");
      return -1;
   }

   if (scsi_cmd->scsi_Actual)
   {
      count = (int)scsi_data[0];
   }
   return count;
}

#ifdef TESTMODE
STRPTR TestData[] = 
{
   "MacOS8_0.iso",
   "Debian.iso",
   "Mac OS 7.6.1.iso",
   "AmigaOS3.1.iso",
   "AmigaOS3.2.iso",
   "Star Wars - Dark Forces (1994)(LucasArts)[!][CDD6287].iso",
   "NetBSD-9.3-amiga.iso"
};
#endif

/* Execute BLUESCSI_TOOLBOX_LIST_CDS and create the files array */
struct FileEntry *Toolbox_List_Files(int cdrom)
{
#ifdef TESTMODE
   filecount = sizeof(TestData) / sizeof(char*);
   files = (struct FileEntry *)AllocMem(sizeof(struct FileEntry) * (filecount + 1), MEMF_CLEAR);
   
   struct FileEntry *file = files;
   for (int f = 0; f < filecount; f++)
   {
      file->Index = f;
      file->Type = BLUESCSI_FILE;
      file->Size = 4096*20000;
      sprintf(file->Number, "%d", f+1);
      Strncpy(file->Name, TestData[f], 32);
      file++;
   }
   file->Type = -1;  // EOF
#else
   
   if (cdrom)
   {
      if (!scsi_isRemovable)
      {
         MessageBox("Toolbox_List_Files", "Not a removable device!\n");
         return NULL;
      }
   }

   // Get the file count
   filecount = Toolbox_Count_Files(cdrom);
   if (filecount > 0)
   {
      files = (struct FileEntry *)AllocMem(sizeof(struct FileEntry) * (filecount + 1), MEMF_CLEAR);
      struct FileEntry *file = files;
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
         MessageBox("Toolbox_List_Files", "SCSI error\n");
         return NULL;
      }

      if (scsi_cmd->scsi_Actual)
      {
         UBYTE *c = scsi_data;
         for (int f = 0; f < filecount; f++)
         {
            file->Index = (int)*c++;
            file->Type = (int)*c++;    // 0=dir 1=file

            sprintf(file->Number, "%d", f+1);
            Strncpy(file->Name, c, 32);

            c += MAX_MAC_PATH + 2;
            file->Size = c[0] << 24 | c[1] << 16 | c[2] << 8 | c[3];
            c += 4;
            file++;
         }
         file->Type = -1;  // EOF
      }
   }
#endif
   return files;
}

/* Select a CD image */
void Toolbox_Set_Next_CD(UBYTE index)
{
   UBYTE command[] = {BLUESCSI_TOOLBOX_SET_NEXT_CD, 0, 0, 0, 0, 0};
   command[1] = (UBYTE) index;
   int err;
   if ((err = DoScsiCmd((UBYTE *)scsi_data, MAX_DATA_LEN,
                        (UBYTE *)&command, sizeof(command),
                        (SCSIF_READ | SCSIF_AUTOSENSE))) != 0)
   {
      UBYTE errmsg[80];
      sprintf(errmsg, "SCSI error %d\n", err);
      MessageBox("Toolbox_Set_Next_CD", errmsg);
   }
}

/* Download a file from the SD card */
int Toolbox_Download(char *source, char *destination, void (*callback)(int))
{
   int result = 0;
   if (files)
   {
      struct FileEntry *file = files;
      int count = 0;
      int index = -1;
      for (int i = 0; i < filecount; i++)
      {
         if (Stricmp(file->Name, source) == 0)
         {
            index = file->Index;
            break;
         }
         file++;
      }

      if (index >= 0 && file->Size > 0)
      {
         int offset = 0; // offset in 4096 size pages
         int size = file->Size;

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
               MessageBox("Toolbox_Download", "SCSI error");
               break;
            }

#ifdef TESTMODE
            offset++;
            count += 4096;
            if (offset * 4096 > size)
            {
               break;
            }
#else
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
#endif

            if (callback && (offset % 16 == 0))
            {
               // Update progress every 64k
               int pc = (offset*100)/(size / 4096);
               callback(pc);
            }
         }
         Close(fh);
         if (callback) callback(100);
         result = count;
      }
      else
      {
         result = -1;
      }
   }
   
   return result;
}
