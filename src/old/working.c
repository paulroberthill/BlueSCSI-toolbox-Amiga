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
#include "bluescsi.h"
#define TEST 1

int Toolbox_Count_Files();
int Toolbox_List_Files();
int DoScsiCmd(UBYTE *data, int datasize, UBYTE *cmd, int cmdsize, UBYTE flags);
void GetVolumeName();
void DiskChange();
void MessageBox(char *body);
void FreeListBrowserNodes();
void Toolbox_Next_CD(UBYTE index);

struct Library *WindowBase, *LayoutBase, *LabelBase, *ListBrowserBase;
struct Library *UtilityBase;
struct Library *IconBase;
struct MsgPort *AppPort;
struct IntuitionBase *IntuitionBase;

UBYTE scsi_dev[1024];
UBYTE errmsg[80];
LONG scsi_unit = -1;

struct IOStdReq *io_ptr;
struct SCSICmd *scsi_cmd;
UBYTE *scsi_sense;
UBYTE *scsi_data = NULL;

// bit of a hack
char selectedDrive[256];

#define SENSE_LEN 252
#define MAX_DATA_LEN 4096

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

extern UWORD logo_data[];
struct Image logo_image =
{
    0, 0,             // LeftEdge, TopEdge
    192, 71, 1,        // Width, Height, Depth
    logo_data,        // ImageData
    0x0001, 0x0000,   // PlanePick, PlaneOnOff
    NULL              // NextImage
};

struct Node *selectedNode;
struct Window *mainWindow;

struct FileEntry
{
   int Index;
   unsigned int Size;
   int Type;
   char Name[32 + 1];
   char Number[5 + 1];
};

struct FileEntry *files = NULL;
int filecount = 0;

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
      MessageBox("Could not open listbrowser.gadget");
      goto exit;
   }
   if ((LayoutBase = OpenLibrary("gadgets/layout.gadget", 44)) == NULL)
   {
      MessageBox("Could not open layout.gadget");
      goto exit;
   }
   if ((LabelBase = OpenLibrary("images/label.image", 44)) == NULL)
   {
      MessageBox("Could not open label.image");
      goto exit;
   }
   if ((AppPort = CreateMsgPort()) == NULL)     // fixme: 
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
            MessageBox("Missing SCSI device or unit");
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

   if ((io_ptr = (struct IOStdReq *)CreateIORequest(AppPort, sizeof(struct IOStdReq))) == NULL)
   {
      PutStr("CreateIORequest failed!\n");
      goto exit;
   }
   if (OpenDevice(scsi_dev, scsi_unit, (struct IORequest *)io_ptr, 0) != 0)
   {
      MessageBox("Error opening SCSI device");
      goto exit;
   }
   if ((scsi_cmd = (struct SCSICmd *)AllocMem(sizeof(struct SCSICmd), MEMF_CLEAR)) == NULL)
   {
      goto exit;
   }
   if ((scsi_sense = (UBYTE *)AllocMem(SENSE_LEN, MEMF_CLEAR)) == NULL)
   {
      goto exit;
   }
   if ((scsi_data = (UBYTE *)AllocMem(MAX_DATA_LEN, MEMF_CLEAR)) == NULL)
   {
      goto exit;
   }


   NewList(&gb_List);

   // Try to f the volume linked to this scsi CDROM device
   GetVolumeName();

   filecount = Toolbox_List_Files();

   char scsi_msg[100];
   sprintf(scsi_msg, "Device:%s Unit:%ld", scsi_dev, scsi_unit);


// LEAK
   APTR windowObj = WindowObject,
      WA_Title, appname,
      WA_Activate, TRUE,
      WA_DepthGadget, TRUE,
      WA_DragBar, TRUE,
      WA_CloseGadget, TRUE,
      WA_SizeGadget, TRUE,
      WA_Width, 200+50,
      WA_Height, 150+150,
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
               BUTTON_BevelStyle, BVS_NONE,
            End,
            CHILD_MaxWidth, 200,
         End,
         CHILD_MaxHeight, 71+10,
           
         // Current SCSI device
         LAYOUT_AddImage,
            LabelObject,
               LABEL_Text, scsi_msg,
            End,
               
         // ListBrowser with the ISOs
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
                  // Printf("WMHI_RAWKEY code=%lx result=%lx\n", code, result & WMHI_KEYMASK);
                  if ((result & WMHI_KEYMASK) == RAWKEY_ESC)
                  {
                     done = TRUE;
                  }
                  break;
               case WMHI_CHANGEWINDOW:
                  Printf("WMHI_CHANGEWINDOW\n");
                  break;
               case WMHI_CLOSEWINDOW:
                  Printf("WMHI_CLOSEWINDOW\n");
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
                           ULONG userdata;
                           GetListBrowserNodeAttrs(node, LBNA_Column, 0, LBNA_UserData, (ULONG *)&userdata, TAG_DONE);

                           Toolbox_Next_CD((UBYTE) userdata);
                           DiskChange();
                        }
                        break;
                  }
                  break;
               case WMHI_ACTIVE:
                  Printf("WMHI_ACTIVE\n");
                  break;
               case WMHI_INACTIVE:
                  Printf("WMHI_INACTIVE\n");
                  break;
               case WMHI_ICONIFY:
                  Printf("WMHI_ICONIFY\n");
                  RA_Iconify(windowObj);
                  mainWindow = NULL;
                  break;
               case WMHI_UNICONIFY:
                  Printf("WMHI_UNICONIFY\n");
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
                  Printf("result=%lx\n", result & WMHI_CLASSMASK);
                  break;
            }
         }
      }
      
      
      DisposeObject( windowObj );

      //RA_CloseWindow((Object *)windowObj);
      FreeListBrowserNodes();
   }
   else
   {
      PutStr("Could not create msgport\n");
   }
   
exit:

   if (scsi_data) FreeMem(scsi_data, MAX_DATA_LEN);
   if (scsi_sense) FreeMem(scsi_sense, SENSE_LEN);
   if (scsi_cmd) FreeMem(scsi_cmd, sizeof(struct SCSICmd));

   if (io_ptr) 
   {
      CloseDevice((struct IORequest *)io_ptr);
      DeleteIORequest(io_ptr);
   }

   if (files) FreeMem(files, sizeof(struct FileEntry) *filecount);

   if (AppPort) DeleteMsgPort(AppPort);
   
   if (IconBase) CloseLibrary(IconBase);
   if (LabelBase) CloseLibrary(LabelBase);
   if (LayoutBase) CloseLibrary(LayoutBase);
   if (ListBrowserBase) CloseLibrary(ListBrowserBase);
   if (WindowBase) CloseLibrary(WindowBase);
   if (UtilityBase) CloseLibrary(UtilityBase);
   if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
}

/* Add a ISO filename to the browser */
BOOL AddListBrowserNode(ULONG num, STRPTR number, STRPTR text)
{
  struct Node *node;
  if((node = AllocListBrowserNode(2,
        LBNA_Column,   0,
                LBNCA_Text,          number,
                LBNCA_Justification, LCJ_LEFT,
                LBNCA_Editable,      FALSE,
                LBNCA_CopyText,      TRUE,
                LBNCA_MaxChars,      4,
                LBNA_UserData,       num,
        LBNA_Column,   1,
                LBNCA_Text,          text,
                LBNCA_Justification, LCJ_LEFT,
                LBNCA_Editable,      FALSE,
                LBNCA_CopyText,      TRUE,
                LBNCA_MaxChars,      100,
                LBNA_UserData,       num,
     TAG_END)))
  {
    node->ln_Pri = num;
    AddTail(&gb_List,node);
    
    if (Strnicmp(text, selectedDrive, strlen(text)-4) == 0)
    {
       selectedNode = node;
    }
  }
  return( node ? TRUE : FALSE );
}

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
#ifndef TEST
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

/* Execute BLUESCSI_TOOLBOX_LIST_CDS and populate the browser */
int Toolbox_List_Files()
{
#ifdef TEST
   // AddListBrowserNode(0,"1","MacOS8_0.iso");
   // AddListBrowserNode(1,"2","Debian.iso");
   // AddListBrowserNode(2,"3","Mac OS 7.6.1.iso");
   // AddListBrowserNode(3,"4","AmigaOS3.1.iso");
   // AddListBrowserNode(4,"5","AmigaOS3.2.iso");
   // AddListBrowserNode(5,"6","Star Wars - Dark Forces (1994)(LucasArts)[!][CDD6287].iso");
   // AddListBrowserNode(6,"7","NetBSD-9.3-amiga.iso");
   return 7;
#else

   // Update the file count
   int count = Toolbox_Count_Files();
   if (count > 0)
   {
      UBYTE command[] = {BLUESCSI_TOOLBOX_LIST_CDS, 0, 0, 0, 0, 0};
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
            sprintf(file->Number, "%d", f+1);
            Printf("[%s]\n", file->Number);

            c += MAX_MAC_PATH + 2;
            file->Size = c[0] << 24 | c[1] << 16 | c[2] << 8 | c[3];
            c += 4;

            // file->Size = size;
            
            AddListBrowserNode(f, file->Number, file->Name);

            file++;
         }
      }
   }
   

   return count;
#endif
}

/* Execute BLUESCSI_TOOLBOX_COUNT_CDS */
int Toolbox_Count_Files()
{
   UBYTE command[] = {BLUESCSI_TOOLBOX_COUNT_CDS, 0, 0, 0, 0, 0};
   int err;
   int count = 0;

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
         files = (struct FileEntry *)AllocMem(sizeof(struct FileEntry) * count, MEMF_CLEAR);
      }
   }
   else
   {
      MessageBox("Could not find any any ISO images");
   }
   
   return count;
}

/* Show a message */
void MessageBox(char *body)
{
   struct EasyStruct general_es =
   {
      sizeof(struct EasyStruct),
      0,
      appname,
      body,
      "OK"
   };
   EasyRequest(mainWindow, &general_es, NULL);
}

/* Select CD image 'n' */
void Toolbox_Next_CD(UBYTE index)
{
   UBYTE command[] = {BLUESCSI_TOOLBOX_SET_NEXT_CD, 0, 0, 0, 0, 0};
   command[1] = (UBYTE) index;
   int err;
   if ((err = DoScsiCmd((UBYTE *)scsi_data, MAX_DATA_LEN,
                        (UBYTE *)&command, sizeof(command),
                        (SCSIF_READ | SCSIF_AUTOSENSE))) != 0)
   {
      sprintf(errmsg, "SCSI error %d\n", err);
      MessageBox(errmsg);
   }
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
// Printf("nCalling DoIO\n");
   DoIO((struct IORequest *)io_ptr);
   return (io_ptr->io_Error);
}




   // Gadgets (dir)
   // button.gadget          checkbox.gadget        chooser.gadget         clicktab.gadget
   // colorwheel.gadget      datebrowser.gadget     fuelgauge.gadget       getcolor.gadget
   // getfile.gadget         getfont.gadget         getscreenmode.gadget   gradientslider.gadget
   // integer.gadget         layout.gadget          listbrowser.gadget     listview.gadget
   // page.gadget            palette.gadget         progress.gadget        radiobutton.gadget
   // scroller.gadget        sketchboard.gadget     slider.gadget          space.gadget
   // speedbar.gadget        string.gadget          tabs.gadget            tapedeck.gadget
   // texteditor.gadget      textfield.gadget       virtual.gadget
   //
   // Images (dir)
   // bevel.image        bitmap.image       boingball.image    drawlist.image     glyph.image
   // label.image        led.image          penmap.image       smartbitmap.image
   // arexx.class      requester.class  window.class


