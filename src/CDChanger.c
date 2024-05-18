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
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/icon.h>
#include <proto/layout.h>
#include <proto/label.h>
#include <proto/listbrowser.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/window.h>
#include <proto/expansion.h>
#include <clib/alib_protos.h>
#include <classes/window.h>
#include <gadgets/layout.h>
#include <gadgets/button.h>
#include <gadgets/listbrowser.h>
#include <gadgets/palette.h>
#include <images/label.h>
#include <reaction/reaction_macros.h>
#include <devices/scsidisk.h>
#include <workbench/startup.h>
#include "toolbox.h"

// TODO: Add command line usage
static const char ver[] = "$VER: CDChanger 1.1 (13.5.2024)";

void GetVolumeName();
void DiskChange();
void FreeListBrowserNodes();
BOOL AddListBrowserNode(ULONG num, STRPTR number, STRPTR text);

struct Library *WindowBase, *LayoutBase, *LabelBase, *ListBrowserBase;
struct Library *UtilityBase, *IconBase;
struct IntuitionBase *IntuitionBase;
struct MsgPort *AppPort;

UBYTE scsi_dev[1024];
LONG scsi_unit = -1;

char selectedDrive[256];   // bit of a hack
static char *readArgsTemplate = "DEVICE/K,UNIT/K/N";
static char* appname = "CD Changer";

enum ToolboxParams
{
   DEVICE,
   UNIT,
};

enum Gadgets
{
   GID_NEXT,
   GID_LISTBROWSER,
};

struct List gb_List;
struct ColumnInfo gb_ListbrowserColumn[] =
{
   {  10, "Number", 0 },
   { 170, "Image", 0 },
   {  -1, (STRPTR)~0, (ULONG)-1 }
};

extern UWORD bluescsi_logo_data[];
extern UWORD zuluscsi_logo_data[];
struct Image logo_image =
{
    0, 0,             // LeftEdge, TopEdge
    192, 71, 1,        // Width, Height, Depth
    NULL,        // ImageData
    0x0001, 0x0000,   // PlanePick, PlaneOnOff
    NULL              // NextImage
};

struct Node *selectedNode;
struct Window *mainWindow;

int main(int argc, char **argv)
{
   struct RDArgs *rd;
   LONG params[] = {0, 0};
   Object *listBrowser;

   if ((IntuitionBase = (struct IntuitionBase *) OpenLibrary("intuition.library", 33L)) == NULL)
   {
      PutStr("Could not open intuition.library\n");
      goto exit;
   }
   if ((WindowBase = OpenLibrary("window.class", 44)) == NULL)
   {
      PutStr("Could not open window.class\n");
      goto exit;
   }
   if ((UtilityBase = OpenLibrary("utility.library", 37L)) == NULL)
   {
      PutStr("Could not open utility.library\n");
      goto exit;
   }
   if ((ListBrowserBase = OpenLibrary("gadgets/listbrowser.gadget", 44)) == NULL)
   {
      MessageBox(appname, "Could not open listbrowser.gadget");
      goto exit;
   }
   if ((LayoutBase = OpenLibrary("gadgets/layout.gadget", 44)) == NULL)
   {
      MessageBox(appname, "Could not open layout.gadget");
      goto exit;
   }
   if ((LabelBase = OpenLibrary("images/label.image", 44)) == NULL)
   {
      MessageBox(appname, "Could not open label.image");
      goto exit;
   }
   if ((AppPort = CreateMsgPort()) == NULL)
   {
      goto exit;
   }

   if (argc==0)
   {
      // Started from Workbench. Read tooltypes
      if ((IconBase = OpenLibrary("icon.library", 33)) != NULL)
      {
         struct WBStartup *WBenchMsg = (struct WBStartup *)argv;
         struct WBArg *wbarg;

         wbarg = WBenchMsg->sm_ArgList;
         for(LONG i=0; i < WBenchMsg->sm_NumArgs; i++, wbarg++)
         {
            struct DiskObject *dobj;
            char *s;
            if((*wbarg->wa_Name) && (dobj=GetDiskObject(wbarg->wa_Name)))
            {
               STRPTR *toolarray = (STRPTR *)dobj->do_ToolTypes;

               if(s=(STRPTR)FindToolType(toolarray,"DEVICE"))
               {
                  Strncpy(scsi_dev, s, sizeof(scsi_dev));
               }
               if(s=(STRPTR)FindToolType(toolarray,"UNIT"))
               {
                  StrToLong(s, &scsi_unit);
               }
               FreeDiskObject(dobj);
               break;
            }
         }
      }
      
      if (scsi_dev[0] == '\0' || scsi_unit < 0)
      {
         if (argc==0)
         {
            MessageBox(appname, "Missing SCSI device or unit");
         }
         else
         {
            // CLI
            PrintFault(ERROR_REQUIRED_ARG_MISSING, argv[0]);
         }
         goto exit;
      }
   }
   else
   {
      // Started from CLI
      rd = ReadArgs(readArgsTemplate, params, NULL);
      if (rd)
      {
         if (params[DEVICE])
         {
            Strncpy(scsi_dev, (UBYTE *)params[DEVICE], sizeof(scsi_dev));
         }
         if (params[UNIT])
         {
            scsi_unit = (*((ULONG *)params[UNIT]));
         }
      }
      else
      {
         SetIoErr(ERROR_REQUIRED_ARG_MISSING);
         PrintFault(IoErr(), argv[0]);
         goto exit;
      }
      FreeArgs(rd);
      
      if (scsi_dev[0] == '\0' || scsi_unit < 0)
      {
         SetIoErr(ERROR_REQUIRED_ARG_MISSING);
         PrintFault(IoErr(), argv[0]);
         goto exit;
      }
   }

   if (scsi_setup(scsi_dev, scsi_unit) != 0)
   {
      goto exit;
   }

   if (scsi_isZuluSCSI) {
      logo_image.ImageData = zuluscsi_logo_data;
   } else {
      logo_image.ImageData = bluescsi_logo_data;
   }

   NewList(&gb_List);

   // Try to find olume linked to this scsi CDROM device
   GetVolumeName();

   // Read the CD Images
   struct FileEntry *files = Toolbox_List_Files(1);
   if (files)
   {
      struct FileEntry *file = files;
      while (file->Type >= 0)
      {
         if (file->Type == 1)    // Files only, folders not currently supported
         {
            AddListBrowserNode(file->Index, file->Number, file->Name);
         }
         file++;
      }
   }
   else
   {
      if (argc!=0)
      {
         Printf("no files\n");
      }
      //goto exit;
   }

   char scsi_msg[50];
   sprintf(scsi_msg, "Device:%s Unit:%ld", scsi_dev, scsi_unit);

   APTR windowObj = WindowObject,
      WA_Title, appname,
      WA_Activate, TRUE,
      WA_DepthGadget, TRUE,
      WA_DragBar, TRUE,
      WA_CloseGadget, TRUE,
      WA_SizeGadget, TRUE,
      WA_Width, 250,
      WA_Height, 250,
      WINDOW_IconifyGadget, TRUE,
      WINDOW_IconTitle, appname,
      WINDOW_AppPort, AppPort,
      WINDOW_Position, WPOS_CENTERMOUSE,
      WINDOW_ParentGroup, VLayoutObject,
         LAYOUT_SpaceOuter, TRUE,
         LAYOUT_DeferLayout, TRUE,

         // BlueSCSI Logo
         LAYOUT_AddChild, VLayoutObject,
            LAYOUT_VertAlignment, LALIGN_CENTER,
            LAYOUT_HorizAlignment, LALIGN_CENTER,
            LAYOUT_BevelStyle, BVS_THIN,
            LAYOUT_AddChild,
               ButtonObject,
               GA_Image, &logo_image,
               GA_ReadOnly, TRUE,
               BUTTON_BevelStyle, BVS_NONE,
            End,
            CHILD_MaxWidth, 200,
         End,
         CHILD_MaxHeight, 81,
           
         // Current SCSI device
         LAYOUT_AddImage,
            LabelObject,
               LABEL_Text, scsi_msg,
            End,
               
         // ListBrowser with the CD images
         LAYOUT_AddChild, 
            listBrowser = ListBrowserObject,
               GA_ID, GID_LISTBROWSER,
               GA_RelVerify, TRUE,
               LISTBROWSER_ColumnInfo, &gb_ListbrowserColumn,
               LISTBROWSER_ColumnTitles, TRUE,
               LISTBROWSER_Labels, &gb_List,
               LISTBROWSER_VertSeparators, TRUE,
               LISTBROWSER_Spacing, 1,
               LISTBROWSER_ShowSelected, TRUE,
               LISTBROWSER_AutoFit, TRUE,
               LISTBROWSER_Editable, FALSE,
               LISTBROWSER_MinVisible, 5,
            End,

         // Select button
         LAYOUT_AddChild, 
            ButtonObject,
               GA_ID, GID_NEXT,
               GA_RelVerify, TRUE,
               GA_Text, "_Select",
            End,
            CHILD_MinHeight, 20,
            CHILD_MaxHeight, 20,

      EndGroup, 
   EndWindow;
 
   mainWindow = (struct Window *)RA_OpenWindow(windowObj);
   if (mainWindow)
   {
      ULONG done = FALSE;
      ULONG result;
      ULONG code;
      ULONG signal;
      ULONG attr;
      struct Node *node;
      UBYTE app = (1L << AppPort->mp_SigBit);
         
      if (selectedNode)
      {
         // Hilight the active ISO image
         SetGadgetAttrs((struct Gadget *)listBrowser, mainWindow, NULL, LISTBROWSER_SelectedNode, selectedNode, TAG_END);
      }
      
      /* Obtain the window wait signal mask */
      GetAttr(WINDOW_SigMask, windowObj, &signal);
      ULONG wait = Wait(signal | SIGBREAKF_CTRL_C | app);

      while (!done)
      {
         while ((result = RA_HandleInput(windowObj, &code)) != WMHI_LASTMSG)
         {
            switch (result & WMHI_CLASSMASK)
            {
               case WMHI_RAWKEY:
                  if ((result & WMHI_KEYMASK) == RAWKEY_ESC)
                  {
                     done = TRUE;
                  }
                  break;
               case WMHI_CLOSEWINDOW:
                  done = TRUE;
                  break;
               case WMHI_GADGETUP:
                  switch (result & WMHI_GADGETMASK)
                  {
                     case GID_LISTBROWSER:
                        //Printf("WMHI_GADGETUP GID_LISTBROWSER\n");
                        break;
                     case GID_NEXT:
                        GetAttr(LISTBROWSER_SelectedNode, listBrowser, (ULONG*) &node);
                        if (node)
                        {
                           // userdata contains the CD image index
                           ULONG userdata;
                           GetListBrowserNodeAttrs(node, LBNA_Column, 0, LBNA_UserData, (ULONG *)&userdata, TAG_DONE);

                           Toolbox_Set_Next_CD((UBYTE) userdata);
                           DiskChange();
                           //RA_CloseWindow((Object *)windowObj);
                        }
                        break;
                  }
                  break;
               case WMHI_ICONIFY:
                  RA_Iconify(windowObj);
                  mainWindow = NULL;
                  break;
               case WMHI_UNICONIFY:
                  mainWindow = (struct Window *)RA_OpenWindow(windowObj);
                  if (mainWindow)
                  {
                     GetAttr(WINDOW_SigMask, windowObj, &signal);
                  }
                  else
                  {
                     done = TRUE;
                  }
                  break;
               default:
                  //printf("%lx\n", result & WMHI_CLASSMASK);
                  break;
            }
         }
      }
      
      DisposeObject( windowObj );

      FreeListBrowserNodes();
   }
   else
   {
      MessageBox(appname, "Could not open window\n");
   }

exit:
   scsi_cleanup();

   if (AppPort) DeleteMsgPort(AppPort);
   if (IconBase) CloseLibrary(IconBase);
   if (LabelBase) CloseLibrary(LabelBase);
   if (LayoutBase) CloseLibrary(LayoutBase);
   if (ListBrowserBase) CloseLibrary(ListBrowserBase);
   if (WindowBase) CloseLibrary(WindowBase);
   if (UtilityBase) CloseLibrary(UtilityBase);
   if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
}

/* Add a filename to the browser */
BOOL AddListBrowserNode(ULONG index, STRPTR number, STRPTR text)
{
  struct Node *node;
  if((node = AllocListBrowserNode(2,
      LBNA_Generation, 2,
      LBNA_Column,   0,
         LBNCA_Text,          number,
         LBNCA_Justification, LCJ_LEFT,
         LBNCA_CopyText,      TRUE,
         LBNCA_MaxChars,      4,
         LBNA_UserData,       index,
      LBNA_Column,   1,
         LBNCA_Text,          text,
         LBNCA_Justification, LCJ_LEFT,
         LBNCA_CopyText,      TRUE,
         LBNCA_MaxChars,      100,
         LBNA_UserData,       index,
     TAG_END)))
  {
    AddTail(&gb_List,node);
    
    if (Strnicmp(text, selectedDrive, strlen(text)-4) == 0)
    {
       selectedNode = node;
    }
  }
  return( node ? TRUE : FALSE );
}

/* Free the browser nodes */
void FreeListBrowserNodes()
{
  struct Node *node, *nextnode;
  node = gb_List.lh_Head;
 
  while((nextnode = node->ln_Succ))
  {
    Remove(node);
    FreeListBrowserNode(node);
    node = nextnode;
  }
}

/* Copy a BCPL string to a C string */
void bstrcpy(char *dest, UBYTE *src)
{
   int len = *src++;
   Strncpy(dest, src, len + 1);
   dest[len] = 0;
}

/* Send a diskchange command to the filesystem that is connected to this device/unit */
void DiskChange()
{
#ifndef TESTMODE
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
         if (fssm->fssm_Unit == scsi_unit)
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
#endif
}

/* Attempts to find the volume label of the CDROM attached to the SCSI unit/device 
   This may not work 100% */
void GetVolumeName()
{
	char device[256];
   struct MsgPort *dol_Task = NULL;
   struct DosList *dl;
   
   // Find the dol_Task linked to the device/unit in the LDF_DEVICES DosList
   dl = LockDosList(LDF_DEVICES | LDF_READ);
   while ((dl = NextDosEntry(dl,LDF_DEVICES)))
   {
      struct FileSysStartupMsg *fssm = BADDR(dl->dol_misc.dol_handler.dol_Startup);
      if (TypeOfMem(fssm) && (APTR)fssm > (APTR)1000)
      {
         if (fssm->fssm_Unit == scsi_unit)
         {
            bstrcpy(device, BADDR(fssm->fssm_Device));
            if (Stricmp(scsi_dev, device) == 0)
            {
               dol_Task = dl->dol_Task;
               break;
            }
         }
      }
   }
   UnLockDosList (LDF_DEVICES | LDF_READ);

   // Find the matching dol_Task linked to the device/unit in the LDF_VOLUMES DosList
   // This contains the name
   if (dol_Task)
   {
      dl = LockDosList(LDF_VOLUMES | LDF_READ);
      while ((dl = NextDosEntry(dl,LDF_VOLUMES)))
      {
         if (dl->dol_Task == dol_Task)
         {
            bstrcpy(selectedDrive, BADDR(dl->dol_Name));
            break;
         }
      }
      UnLockDosList (LDF_VOLUMES | LDF_READ);
   }
}
