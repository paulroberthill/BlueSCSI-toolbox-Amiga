/** 
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
**/
#ifndef TOOLBOX_H
#define TOOLBOX_H

#include <exec/types.h>

#define BLUESCSI_TOOLBOX_COUNT_FILES 0xD2
#define BLUESCSI_TOOLBOX_LIST_FILES 0xD0
#define BLUESCSI_TOOLBOX_GET_FILE 0xD1
#define BLUESCSI_TOOLBOX_SEND_FILE_PREP 0xD3
#define BLUESCSI_TOOLBOX_SEND_FILE_10 0xD4
#define BLUESCSI_TOOLBOX_SEND_FILE_END 0xD5
#define BLUESCSI_TOOLBOX_TOGGLE_DEBUG 0xD6
#define BLUESCSI_TOOLBOX_LIST_CDS 0xD7
#define BLUESCSI_TOOLBOX_SET_NEXT_CD 0xD8
#define BLUESCSI_TOOLBOX_LIST_DEVICES 0xD9
#define BLUESCSI_TOOLBOX_COUNT_CDS 0xDA

// from BlueSCSI_Toolbox.cpp
#define MAX_MAC_PATH 32

#define SCSI_CMD_INQ 0x12

// #define TESTMODE 1
#define MAXPATH 1024

#define BLUESCSI_FILE 1
#define BLUESCSI_DIR 0

// scsi.c
struct FileEntry *Toolbox_List_Files(int cdrom);
void Toolbox_Set_Next_CD(UBYTE index);
void scsi_cleanup();
int Toolbox_Download(char *source, char *destination, void (*callback)(int));
int scsi_setup(char *scsi_dev, int scsi_unit);

// toolbox.c
void MessageBox(char *title, char *body);

struct FileEntry
{
   int Index;
   unsigned int Size;
   int Type;
   char Name[32 + 1];
   char Number[5 + 1];
};

#endif

