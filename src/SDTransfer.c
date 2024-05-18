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
#include <proto/asl.h>
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
#include <proto/fuelgauge.h>
#include <images/label.h>
#include <reaction/reaction_macros.h>
#include <devices/scsidisk.h>
#include <workbench/startup.h>
#include "toolbox.h"

static const char ver[] = "$VER: SDTransfer 1.1 (13.5.2024)";

void FreeListBrowserNodes();
BOOL AddListBrowserNode(ULONG index, STRPTR filename);
void progress(int pc);
void getfilename(char *name, char *title);

struct Library *WindowBase, *LayoutBase, *LabelBase, *ListBrowserBase;
struct Library *UtilityBase, *FuelGaugeBase, *IconBase, *AslBase;
struct IntuitionBase *IntuitionBase;
struct MsgPort *AppPort;

UBYTE scsi_dev[1024];
LONG scsi_unit = -1;

static char *readArgsTemplate = "DEVICE/K,UNIT/K/N";
static char* appname = "SD Transfer";

enum ToolboxParams
{
   DEVICE,
   UNIT,
};

enum Gadgets
{
   GID_DOWNLOAD,
   GID_LISTBROWSER,
   GID_FUELGAUGE,
};

struct List gb_List;
struct ColumnInfo gb_ListbrowserColumn[] =
{
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

struct Window *mainWindow;
Object *fuelGauge;

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
   if ((AslBase = OpenLibrary("asl.library", 44L)) == NULL)
   {
      PutStr("Could not open asl.library\n");
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
   if ((FuelGaugeBase = OpenLibrary("gadgets/fuelgauge.gadget", 44)) == NULL)
   {
      MessageBox(appname, "Could not open fuelgauge.gadget");
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

   // Read the SD Card Files
   struct FileEntry *files = Toolbox_List_Files(0);
   if (files)
   {
      struct FileEntry *file = files;
      while (file->Type >= 0)
      {
         if (file->Type == 1)    // Files only, folders not currently supported
         {
            AddListBrowserNode(file->Index, file->Name);
         }
         file++;
      }
   }
   else
   {
      goto exit;
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
               GA_ID, GID_DOWNLOAD,
               GA_RelVerify, TRUE,
               GA_Text, "_Download",
            End,
            CHILD_MinHeight, 20,
            CHILD_MaxHeight, 20,
            
         // Progress
         LAYOUT_AddChild, 
            fuelGauge = FuelGaugeObject,
               GA_ID, GID_FUELGAUGE,
               GA_RelVerify, TRUE,
               GA_Text, "",     // centered?
               FUELGAUGE_Min, 0,
               FUELGAUGE_Max, 100,
               FUELGAUGE_Level, 0,
               FUELGAUGE_Percent, FALSE,
               FUELGAUGE_FillPen, FILLPEN,
               FUELGAUGE_Ticks, 0,
            End,
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
                     case GID_DOWNLOAD:
                        GetAttr(LISTBROWSER_SelectedNode, listBrowser, (ULONG*) &node);
                        if (node)
                        {
                           // Get the filename
                           ULONG userdata;
                           GetListBrowserNodeAttrs(node, LBNA_Column, 0, LBNCA_Text, (ULONG *)&userdata, TAG_DONE);
                           if (userdata)
                           {
                              char *source = (char *) userdata;

                              // Set the fuelGauge to %
                              SetGadgetAttrs((struct Gadget *)fuelGauge, mainWindow, NULL, FUELGAUGE_Percent, TRUE, FUELGAUGE_Level, 0, TAG_END);

                              char destination[MAXPATH];    // static????
                              strncpy(destination, source, MAXPATH);
                              
                              // doesn't work if cancelled!!!!
                              // If multiselect use a folder requester?
                              getfilename(destination, "Save As");
                              if (destination)
                              {
                                 int bytes = Toolbox_Download(source, destination, progress);
                                 if (bytes > 0)
                                 {
                                    // Display the number of bytes
                                    ULONG varargs[] = { (ULONG) bytes, (ULONG) destination };
                                    SetGadgetAttrs((struct Gadget *)fuelGauge, mainWindow, NULL, 
                                                   GA_Text, "%ld bytes saved to %s", 
                                                   FUELGAUGE_VarArgs, varargs, 
                                                   FUELGAUGE_Percent, FALSE,
                                                   TAG_END);
                                 }
                              }
                           }
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
   if (FuelGaugeBase) CloseLibrary(FuelGaugeBase);
   if (LabelBase) CloseLibrary(LabelBase);
   if (LayoutBase) CloseLibrary(LayoutBase);
   if (ListBrowserBase) CloseLibrary(ListBrowserBase);
   if (WindowBase) CloseLibrary(WindowBase);
   if (AslBase) CloseLibrary(AslBase);
   if (UtilityBase) CloseLibrary(UtilityBase);
   if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
}

/* Update progress */
void progress(int pc)
{
   SetGadgetAttrs((struct Gadget *)fuelGauge, mainWindow, NULL, FUELGAUGE_Level, pc, TAG_END);
}

/* Add a filename to the browser */
BOOL AddListBrowserNode(ULONG index, STRPTR filename)
{
  struct Node *node;
  if((node = AllocListBrowserNode(2,
         LBNA_Generation, 2,
         LBNA_Column,   0,
         LBNCA_Text,          filename,
         LBNCA_Justification, LCJ_LEFT,
         LBNCA_CopyText,      TRUE,
         LBNCA_MaxChars,      100,
         LBNA_UserData,       index,
     TAG_END)))
  {
    AddTail(&gb_List,node);
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


/*
** getfilename()
** - ask the user for a file name using the ASL file requester
*/
void getfilename(char *name, char *title)
{
	struct FileRequester *fr;
	char dirname[MAXPATH];
	char *filename;

	if (strlen(name) > 0)
	{
		strncpy(dirname,name,MAXPATH);
		dirname[MAXPATH - 1] = '\0';
		filename = (STRPTR) PathPart(dirname);
		filename[0] = '\0';
		filename = (STRPTR) FilePart(name);
	} else
	{
		dirname[0] = '\0';
		filename = dirname;
	}

	if (fr = (struct FileRequester *) 
			AllocAslRequestTags(	ASL_FileRequest,
										ASLFR_TitleText,		(ULONG) title,
										ASLFR_InitialFile,	(ULONG) filename,
										ASLFR_InitialDrawer,	(ULONG) dirname,
										TAG_DONE))
	{
		if (AslRequest(fr, NULL))
		{
			strcpy(name,fr->rf_Dir);
			AddPart(name,fr->rf_File,MAXPATH);
		}
		FreeAslRequest(fr);
	}
}


