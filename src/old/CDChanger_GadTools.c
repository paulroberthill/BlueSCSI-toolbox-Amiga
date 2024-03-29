#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/icon.h>
#include <proto/alib.h>
#include <exec/types.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include <workbench/startup.h>
#include <stdio.h>
#include <string.h>
 
#define WINDOW_WIDTH 200
#define WINDOW_HEIGHT 220+3

#define MYGAD_BUTTON    (0)
#define MYGAD_LISTVIEW    (1)
#define MYGAD_MAX (MYGAD_LISTVIEW+1)

struct IntuitionBase *IntuitionBase;
struct Library  *IconBase;
struct Library *GadToolsBase;

struct Gadget *gadgets[MYGAD_MAX];
struct List files;
struct TextAttr Topaz80 = { "topaz.font", 8, 0, 0, };
struct Node *SelectedNode;
char StatusMessage[100];
char SCSIMessage[100];

char *template = "DEVICE/K/M,UNIT/K/N";
 
VOID process_window_events(struct Window *);
VOID gadtoolsWindow(VOID);
UBYTE scsi_dev[1024];
ULONG scsi_id = -1;




// FILE *conwin = NULL;
// UBYTE *conwinname   = "CON:10/10/620/180/iconexample/close";

// BOOL showToolTypes(struct WBArg *wbarg)
    // {
    // struct DiskObject *dobj;
    // STRPTR *toolarray;
    // char *s;
    // BOOL success = FALSE;

    // kprintf("\nWBArg Lock=0x%lx, Name=%s\n",
                           // wbarg->wa_Lock,wbarg->wa_Name);

    // if((*wbarg->wa_Name) && (dobj=GetDiskObject(wbarg->wa_Name)))
    // {
        // kprintf("  We have read the DiskObject (icon) for this arg\n");
        // toolarray = (STRPTR *)dobj->do_ToolTypes;

        // if(s=(STRPTR)FindToolType(toolarray,"DEVICE"))
        // {
            // kprintf("    Found tooltype DEVICE with value %s\n",s);
        // }
        // if(s=(STRPTR)FindToolType(toolarray,"UNIT"))
        // {
            // kprintf("    Found tooltype UNIT with value %s\n",s);
        // }
        // FreeDiskObject(dobj);
        // success = TRUE;
    // }
    // else if(!(*wbarg->wa_Name))
        // kprintf("  Must be a disk or drawer icon\n");
    // else
        // kprintf("  Can't find any DiskObject (icon) for this WBArg\n");
    // return(success);
    // }



int main(int argc, char **argv)
{
   struct RDArgs *rd;
   LONG params[] = {0, 0};
   scsi_dev[0] = '\0';

   if ((IntuitionBase = (struct IntuitionBase *) OpenLibrary("intuition.library", 33)) != NULL)
   {
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
               /* if there's a directory lock for this wbarg, CD there */
               // olddir = -1;
               // if((wbarg->wa_Lock)&&(*wbarg->wa_Name)) olddir = CurrentDir(wbarg->wa_Lock);

               struct DiskObject *dobj;
               char *s;
               if((*wbarg->wa_Name) && (dobj=GetDiskObject(wbarg->wa_Name)))
               {
                  STRPTR *toolarray = (STRPTR *)dobj->do_ToolTypes;

                  if(s=(STRPTR)FindToolType(toolarray,"DEVICE"))
                  {
                     strncpy(scsi_dev, s, sizeof(scsi_dev));
                  }
                  if(s=(STRPTR)FindToolType(toolarray,"UNIT"))
                  {
                     StrToLong(s, &scsi_id);
                  }
                  FreeDiskObject(dobj);
                  break;
               }

               // if((i>0)&&(*wbarg->wa_Name)) kprintf("In Main. We could open the %s file here\n",wbarg->wa_Name);
               // if(olddir != -1)  CurrentDir(olddir); /* CD back where we were */
            }

            CloseLibrary((struct Library *)IconBase);
         }
      }
      else
      {
         // Started from CLI
         rd = ReadArgs(template, params, NULL);
         if (rd)
         {
            if (params[0])
            {
               strncpy(scsi_dev, (char *)params[0], sizeof(scsi_dev));
               // CopyMem((char *)params[0], scsi_dev, sizeof(scsi_dev));
            }
            if (params[1])
            {
               scsi_id = (*((LONG *)params[1]));
            }
         }
      }
      
      if (scsi_dev[0] == '\0' || scsi_id == -1)
      {
         if (argc==0)
         {
            struct EasyStruct YesNoReply = {
               sizeof(struct EasyStruct),
               0,
               "Error",
               "Missing SCSI device/unit",
               "OK"
            };
            EasyRequest(NULL, &YesNoReply, NULL);
         }
         else
         {
            // CLI
            PrintFault(ERROR_REQUIRED_ARG_MISSING, argv[0]);
         }
         return 5;
      }

      if ((GadToolsBase = OpenLibrary("gadtools.library", 33)) != NULL)
      {
         gadtoolsWindow();
         CloseLibrary(GadToolsBase);
      }
      CloseLibrary((struct Library *)IntuitionBase);
  }
  return 0;
}

UWORD bla[] = 
{
   -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
   -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
   -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
};

/*
** Prepare for using GadTools, set up gadgets and open window.
** Clean up and when done or on error.
*/
VOID gadtoolsWindow(VOID)
{
   struct Screen    *mysc;
   struct Window    *mywin;
   struct Gadget    *glist, *gad;
   struct NewGadget lblLogo;
   struct NewGadget lblPath;
   struct NewGadget lvFiles;
   struct NewGadget btnNext;
   void             *vi;
    
   glist = NULL;

   struct Node top;
   top.ln_Succ = NULL;
   top.ln_Pred = NULL;
   top.ln_Name = "MacOS8_0.iso";

   struct Node top2;
   top2.ln_Succ = NULL;
   top2.ln_Pred = NULL;
   top2.ln_Name = "Debian.iso";

   struct Node top3;
   top3.ln_Succ = NULL;
   top3.ln_Pred = NULL;
   top3.ln_Name = "Mac OS 7.6.1.iso";

   struct Node top4;
   top4.ln_Succ = NULL;
   top4.ln_Pred = NULL;
   top4.ln_Name = "AmigaOS3.2.iso";

   files.lh_Tail = NULL;
   files.lh_TailPred = NULL;
   NewList(&files);

   AddTail(&files,&top);
   AddTail(&files,&top2);
   AddTail(&files,&top3);
   AddTail(&files,&top4);

   if ((mysc = LockPubScreen(NULL)) != NULL)
   {
      if ( (vi = GetVisualInfo(mysc, TAG_END)) != NULL )
      {
         gad = CreateContext(&glist);
/*

         // Logo
         lblLogo.ng_TextAttr   = &Topaz80;
         lblLogo.ng_VisualInfo = vi;
         lblLogo.ng_LeftEdge   = mysc->WBorLeft;
         lblLogo.ng_TopEdge    = mysc->WBorTop + (mysc->Font->ta_YSize + 1);
         lblLogo.ng_Width      = WINDOW_WIDTH - (mysc->WBorLeft + mysc->WBorRight);
         lblLogo.ng_Height     = 50;
         lblLogo.ng_GadgetText = NULL;
         // lblLogo.ng_Flags      = GFLG_GADGIMAGE;
         // lblLogo.GadgetRender      = NULL;
         
         gad = CreateGadget(TEXT_KIND, gad, &lblLogo, 
                            GTTX_Text, "SCSI SD Transfer",
                            GTTX_FrontPen, 2, 
                            GTTX_Justification, 
                            GTJ_CENTER, 
                            TAG_END);
*/

         // Logo
         lblLogo.ng_TextAttr   = &Topaz80;
         lblLogo.ng_VisualInfo = vi;
         lblLogo.ng_LeftEdge   = mysc->WBorLeft;
         lblLogo.ng_TopEdge    = mysc->WBorTop + (mysc->Font->ta_YSize + 1);
         lblLogo.ng_Width      = WINDOW_WIDTH - (mysc->WBorLeft + mysc->WBorRight);
         lblLogo.ng_Height     = 50;
         lblLogo.ng_GadgetText = NULL;
         lblLogo.ng_Flags      = GFLG_GADGIMAGE;
         // lblLogo.GadgetRender      = NULL;
         
         gad = CreateGadget(GENERIC_KIND, gad, &lblLogo, 
                            TAG_END);

// A pointer to the Image or Border structure containing the graphics
// imagery of this gadget.  If this field is set to NULL, no rendering
// will be done.

struct Image i;
i.LeftEdge = 0;
i.TopEdge = 0;
i.Width = 198;
i.Height = 77;
i.Depth = 2;
i.ImageData = bla;
i.PlanePick = 1;
i.PlaneOnOff = 0;


         gad->GadgetRender = &i;
         
         
         


// #define GFLG_GADGIMAGE 	  0x0004  /* set if GadgetRender and SelectRender
				   // * point to an Image structure, clear
				   // * if they point to Border structures
				   // */


// Bitmap or custom images are used as imagery for a gadget by setting the
// GFLG_GADGIMAGE flag in the Flags field of the Gadget structure.  An Image
// structure must be set up to manage the bitmap data.  The address of the
// Image structure is placed into the gadget's GadgetRender field.  The
// bitmap image will be positioned relative to the gadget's select box.  For
// more information about creating Intuition images, see the chapter
// "Intuition Images, Line Drawing, and Text."  For a listing of the Gadget
// structure and all its flags see the "Gadget Structure" section later in
// this chapter.




//	DATATYPE_KIND, NULL, 
// GA_Text, NULL, 
// GA_Left, 119, 
// GA_Top, 21, 
// GA_Width, 86, 
// GA_Height, 45, 
// GA_UserData, (ULONG) FN_sexy_Clicked, 
// TAG_DONE,


         // Device/Unit
         sprintf(SCSIMessage, "%2s unit %lu", scsi_dev, scsi_id);
         
         lblPath.ng_TextAttr   = &Topaz80;
         lblPath.ng_VisualInfo = vi;
         lblPath.ng_LeftEdge   = mysc->WBorLeft;
         lblPath.ng_TopEdge    = lblLogo.ng_TopEdge + lblLogo.ng_Height;
         lblPath.ng_Width      = lblLogo.ng_Width;
         lblPath.ng_Height     = 18;
         lblPath.ng_GadgetText = NULL;
         gad = CreateGadget(TEXT_KIND, gad, &lblPath, 
                            GTTX_Border, TRUE, 
                            GTTX_Text, SCSIMessage, 
                            TAG_END);

         // Listview
         lvFiles.ng_TextAttr   = &Topaz80;
         lvFiles.ng_VisualInfo = vi;
         lvFiles.ng_LeftEdge   = mysc->WBorLeft;
         lvFiles.ng_TopEdge    = lblPath.ng_TopEdge + lblPath.ng_Height;
         lvFiles.ng_Width      = lblLogo.ng_Width;
         lvFiles.ng_Height     = WINDOW_HEIGHT - (lblPath.ng_Height + lblLogo.ng_Height + 20);
         lvFiles.ng_GadgetText = NULL,
         lvFiles.ng_GadgetID   = MYGAD_LISTVIEW;
         lvFiles.ng_Flags      = 0;
         gad = CreateGadget(LISTVIEW_KIND, gad, &lvFiles, 
                            GTLV_Labels, &files,
                            GTLV_ShowSelected, NULL,
                            GTLV_Selected, 0,
                            TAG_END);
         gadgets[MYGAD_LISTVIEW] = gad;

         // Next button
         btnNext.ng_TextAttr   = &Topaz80;
         btnNext.ng_VisualInfo = vi;
         btnNext.ng_LeftEdge   = lvFiles.ng_LeftEdge;
         btnNext.ng_TopEdge    = lvFiles.ng_TopEdge + lvFiles.ng_Height;
         btnNext.ng_Width      = lvFiles.ng_Width;
         btnNext.ng_Height     = 20;
         btnNext.ng_GadgetText = "_Next";
         btnNext.ng_GadgetID   = MYGAD_BUTTON;
         btnNext.ng_Flags      = 0;
         gad = CreateGadget(BUTTON_KIND, gad, &btnNext,
                            GT_Underscore, '_',
                            TAG_END);
         gadgets[MYGAD_BUTTON] = gad;

         if (gad != NULL)
         {
            if ((mywin = OpenWindowTags(NULL,
                                        WA_Title,        "CD Changer",
                                        WA_Gadgets,      glist,
                                        WA_AutoAdjust,   TRUE,
                                        WA_Width,        WINDOW_WIDTH,
                                        WA_InnerHeight,  WINDOW_HEIGHT,
                                        WA_DragBar,      TRUE,
                                        WA_DepthGadget,  TRUE,
                                        WA_Activate,     TRUE,   
                                        WA_CloseGadget,  TRUE,
                                        WA_IDCMP,        IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | BUTTONIDCMP,
                                        WA_PubScreen,    mysc,
                                        TAG_END)) != NULL)
            {


// struct DiskObject *dobj;
// dobj=GetDiskObject("CDSwitcher.info");
// if (dobj)
// {
// AddGadget(mywin, &dobj->do_Gadget, NULL);
// }


               GT_RefreshWindow(mywin, NULL);
               process_window_events(mywin);
               CloseWindow(mywin);
            }
         }
         FreeGadgets(glist);
         FreeVisualInfo(vi);
      }
      UnlockPubScreen(NULL, mysc);
   }
}
 

VOID process_window_events(struct Window *mywin)
{
   struct IntuiMessage *imsg;
   struct Gadget *gad;
   BOOL  terminated = FALSE;

   while (!terminated)
   {
      Wait (1 << mywin->UserPort->mp_SigBit);
      while ((!terminated) && (imsg = GT_GetIMsg(mywin->UserPort)))
      {
         switch (imsg->Class)
         {
            case IDCMP_GADGETUP:       /* Buttons only report GADGETUP */
               gad = (struct Gadget *)imsg->IAddress;
               if (gad->GadgetID == MYGAD_BUTTON)
               {
                  if (SelectedNode != NULL)
                  {
                     printf("Selecting %s", SelectedNode->ln_Name);
                     // GT_SetGadgetAttrs(gadgets[MYGAD_PROGRESS], mywin, NULL,
                             // GTTX_Text, StatusMessage,
                             // TAG_END);
                  }
               }
               if (gad->GadgetID == MYGAD_LISTVIEW)
               {
                  SelectedNode = NULL;
                  
                  int i=0;
                  for (struct Node *node = files.lh_Head; node->ln_Succ; node = node->ln_Succ)
                  {
                     if (i++ == imsg->Code)
                     {                       
                        SelectedNode = node;
                        break;
                     }
                  }
                  
                  if (SelectedNode != NULL)
                  {
                     // GT_SetGadgetAttrs(gadgets[MYGAD_PROGRESS], mywin, NULL,
                                       // GTTX_Text, SelectedNode->ln_Name,
                                       // TAG_END);
                  }
               }
               break;
            case IDCMP_CLOSEWINDOW:
               terminated = TRUE;
               break;
            case IDCMP_REFRESHWINDOW:
               GT_BeginRefresh(mywin);
               GT_EndRefresh(mywin, TRUE);
               break;
         }
         GT_ReplyIMsg(imsg);
      }
   }
}