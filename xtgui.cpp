/*----------------------------------------------------------------------*/
/* xtgui.c --- 								*/
/* XCircuit's graphical user interface using the Xw widget set		*/
/* (see directory Xw) (non-Tcl/Tk GUI)					*/
/* Copyright (c) 2002  R. Timothy Edwards				*/
/*----------------------------------------------------------------------*/

#include <QStyle>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QLayout>
#include <QMenu>
#include <QToolButton>
#include <QFileInfo>

#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <climits>
#include <clocale>

#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "xcircuit.h"
#include "menus.h"
#include "prototypes.h"
#include "pixmaps.h"
#include "xcqt.h"
#include "colors.h"

/*----------------------------------------------------------------------*/
/* Global Variable definitions						*/
/*----------------------------------------------------------------------*/

extern short	 popups;      /* total number of popup widgets on the screen */

Atom wprot, wmprop[2];

extern Colormap cmap;
extern Cursor appcursors[NUM_CURSORS];
extern ApplicationData appdata;
extern short menusize;
extern XtIntervalId printtime_id;
extern short beeper;
extern short fontcount;
extern fontinfo *fonts;
extern short help_up;
extern Pixmap helppix;
extern aliasptr aliastop;
extern float version;

/*----------------------------------------------*/
/* Check for conditions to approve program exit */
/*----------------------------------------------*/

void quitcheck(QAction* a, void*, void*)
{
   char *promptstr;
   bool doprompt = false;

   /* enable default interrupt signal handler during this time, so that */
   /* a double Control-C will ALWAYS exit.				*/

   signal(SIGINT, SIG_DFL);
   promptstr = (char *)malloc(22);
   strcpy(promptstr, "Unsaved changes in: ");

   /* Check all page objects for unsaved changes */

   doprompt = countchanges(&promptstr) > 0;

   /* If any changes have not been saved, generate a prompt */

   if (doprompt) {
      promptstr = (char *)realloc(promptstr, strlen(promptstr) + 15);
      strcat(promptstr, "\nQuit anyway?");
      popupYesNo(a, promptstr, quit, NULL);
      free(promptstr);
   }
   else {
      free(promptstr);
      quit(a, NULL, NULL);
   }
}

/*-------------------------------------------------------------------------*/
/* Popup dialog box routines						   */
/*-------------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/* Toggle button showing number of pages of output.			*/
/* Erase the filename and set everything accordingly.			*/
/*----------------------------------------------------------------------*/

void linkset(Widget button, propstruct *callstruct, caddr_t calldata)
{
   Arg wargs[1];

   xobjs.pagelist[areawin->page].filename.clear();

   XwTextClearBuffer((XwTextEditWidget)callstruct->textw);
   getproptext(button, callstruct, calldata);

   /* Change the select button back to "Apply" */
   XtSetArg(wargs[0], XtNlabel, "Apply");
   XtSetValues(callstruct->buttonw, wargs, 1);

   /* Pop down the toggle button */
   XtUnmanageChild(button);
}

/*-------------------------------------------------------------------------*/
/* Grab text from the "output properties" window			   */
/*-------------------------------------------------------------------------*/

void getproptext(Widget button, void *callstruct_, void*)
{
   propstruct *callstruct = (propstruct*)callstruct_;
   /* xobjs.pagelist[areawin->page]->filename can be realloc'd by the */
   /* call to *(callstruct->setvalue), so callstruct->dataptr may no    */
   /* longer be pointing to the data.					*/

   Arg wargs[1];
   short file_yes = (callstruct->setvalue == setfilename);

   QString str = XwTextCopyBuffer((XwTextEditWidget)callstruct->textw);
   (*(callstruct->setvalue))((QAction*)button, str, callstruct->dataptr);

   /* special stuff for filename changes */

   if (file_yes) {
      char blabel[1024];
      short num_linked;
      Widget wrbutton, ltoggle;
      struct stat statbuf;

      /* get updated file information */

      if (xobjs.pagelist[areawin->page].filename.contains("."))
         sprintf(blabel, "%s.ps", xobjs.pagelist[areawin->page].filename.toLocal8Bit().data());
      else sprintf(blabel, "%s", xobjs.pagelist[areawin->page].filename.toLocal8Bit().data());
      if (stat(blabel, &statbuf) == 0) {
         sprintf(blabel, " Overwrite File ");
         if (beeper) XBell(100);
         Wprintf("    Warning:  File exists");
      }
      else {
         sprintf(blabel, " Write File ");
         if (errno == ENOTDIR)
            Wprintf("Error:  Incorrect pathname");
         else if (errno == EACCES)
            Wprintf("Error:  Path not readable");
	 W3printf("  ");
      }

      wrbutton = XtNameToWidget(XtParent(button), "Write File");
      XtSetArg(wargs[0], XtNlabel, blabel);
      XtSetValues(wrbutton, wargs, 1);

      num_linked = pagelinks(areawin->page);
      if (num_linked > 1) {
         ltoggle = XtNameToWidget(XtParent(button), "LToggle");
	 sprintf(blabel, "%d Pages", num_linked);
	 XtSetArg(wargs[0], XtNlabel, blabel);
	 XtSetValues(ltoggle, wargs, 1);
	 XtManageChild(ltoggle);
      }
   }

   /* topobject->name is not malloc'd, so is not changed by call to */
   /* *(callstruct->setvalue).					   */

   else if (callstruct->dataptr == topobject->name) {
      printname(topobject);
      renamepage(areawin->page);
   }

   /* Button title changes from "Apply" to "Okay" */

   XtSetArg(wargs[0], XtNlabel, "Okay");
   XtSetValues(callstruct->buttonw, wargs, 1);
}

/*----------------------------------------------------------------------*/
/* Update scale, width, and height in response to change of one of them	*/
/*----------------------------------------------------------------------*/

void updatetext(QObject*, void* callstruct_, void*)
{
   WidgetList callstruct = (WidgetList)callstruct_;
   char  edit[3][50];
   short i, n, posit;
   char  *pdptr;
   Arg	 wargs[2];

   /* auto-fit may override any changes to the scale */

   autoscale(areawin->page);
   writescalevalues(edit[0], edit[1], edit[2]);
   for (i = 0; i < 3; i++) {
      n = 0;
      XtnSetArg(XtNstring, edit[i]);
      pdptr = strchr(edit[i], '.');
      posit = (pdptr != NULL) ? (short)(pdptr - edit[i]) : strlen(edit[i]);
      XtnSetArg(XtNinsertPosition, posit);
      XtSetValues(callstruct[i + 2], wargs, n);
   }
}

/*-------------------------------------------------------------------------*/
/* Update the object name in response to a change in filename		   */
/*-------------------------------------------------------------------------*/

void updatename(Widget, void* callstruct_, void*)
{
   WidgetList callstruct = (WidgetList)callstruct_;
   short n, posit;
   Arg   wargs[2]; 
      
   if (strstr(topobject->name, "Page ") != NULL || strstr(topobject->name,
	"Page_") != NULL || topobject->name[0] == '\0') {

      QFileInfo fi(xobjs.pagelist[areawin->page].filename);
      sprintf(topobject->name, "%.79s", fi.fileName().toLocal8Bit().data());
  
      n = 0;
      posit = strlen(topobject->name);
      XtnSetArg(XtNstring, topobject->name);
      XtnSetArg(XtNinsertPosition, posit);
      XtSetValues(callstruct[1], wargs, n);
      printname(topobject);
      renamepage(areawin->page);
   }
}

/*-------------------------------------------------------------------------*/
/* Create a popup window for property changes				   */
/*-------------------------------------------------------------------------*/

#define MAXPROPS 7
#define MARGIN 15

propstruct okstruct[MAXPROPS], fpokstruct;

#ifdef XC_X11

void outputpopup(Widget button, void* clientdata, void* calldata)
{
   buttonsave  *savebutton;
   Arg         wargs[9];
   Widget      popup, dialog, okbutton, titlearea, wrbutton;
   Widget      fpentertext, fpokay, autobutton, allpages;
   WidgetList  staticarea, entertext, okays;
   XWMHints    *wmhints;	/* for proper input focus */
   short       num_linked;
   Position    xpos, ypos;
   short       n = 0;
   Dimension   height, width, areawidth, areaheight, bwidth, owidth, wwidth;
   Pagedata    *curpage;
   char	       *pdptr;
   short       posit, i;
   popupstruct *donestruct;
   XtCallbackProc function[MAXPROPS];
   XtCallbackProc update[MAXPROPS];
   char	statics[MAXPROPS][50], edit[MAXPROPS][75], request[150];
   char fpedit[75], outname[75], pstr[20];
   void	*data[MAXPROPS];
   struct stat statbuf;
   static char defaultTranslations[] = "<Key>Return:	execute()";

   if (is_page(topobject) == -1) {
      Wprintf("Can only save a top-level page!");
      return;
   }
   if (button == NULL) button = FileWriteXcircuitPSButton;
   savebutton = getgeneric(button, outputpopup, NULL);

   curpage = xobjs.pagelist[areawin->page];

   sprintf(request, "PostScript output properties (Page %d):", 
	areawin->page + 1);
   sprintf(statics[0], "Filename:");
   sprintf(statics[1], "Page label:");
   sprintf(statics[2], "Scale:");
   if (curpage->coordstyle == CM) {
      sprintf(statics[3], "X Size (cm):");
      sprintf(statics[4], "Y Size (cm):");
   }
   else {
      sprintf(statics[3], "X Size (in):");
      sprintf(statics[4], "Y Size (in):");
   }
   sprintf(statics[5], "Orientation:");
   sprintf(statics[6], "Mode:");

   if (curpage->filename)
      sprintf(edit[0], "%s", curpage->filename);
   else
      sprintf(edit[0], "Page %d", areawin->page + 1);
   sprintf(edit[1], "%s", topobject->name);

   /* recompute bounding box and auto-scale, if set */
   calcbbox(areawin->topinstance);
   if (curpage->pmode & 2) autoscale(areawin->page);
   writescalevalues(edit[2], edit[3], edit[4]);
   sprintf(edit[5], "%s", (curpage->orient == 0) ? "Portrait" : "Landscape");
   sprintf(edit[6], "%s", (curpage->pmode & 1)
	? "Full page" : "Embedded (EPS)");
   function[0] = setfilename;
   function[1] = setpagelabel;
   function[2] = setfloat;
   function[3] = setscalex;
   function[4] = setscaley;
   function[5] = setorient;
   function[6] = setpmode;
   update[0] = updatename;
   update[1] = update[6] = NULL;
   update[2] = updatetext;
   update[3] = updatetext;
   update[4] = updatetext;
   update[5] = updatetext;
   data[0] = &(curpage->filename);
   data[1] = topobject->name;
   data[2] = data[3] = data[4] = &(curpage->outscale);
   data[5] = &(curpage->orient);
   data[6] = &(curpage->pmode);

   entertext = (WidgetList) XtMalloc (7 * sizeof (Widget));
   staticarea = (WidgetList) XtMalloc (7 * sizeof (Widget));
   okays = (WidgetList) XtMalloc (6 * sizeof (Widget));

   /* get file information */

   if (strstr(edit[0], ".") == NULL)
      sprintf(outname, "%s.ps", edit[0]);  
   else sprintf(outname, "%s", edit[0]);
   if (stat(outname, &statbuf) == 0) {
      sprintf(outname, "Overwrite File");
      Wprintf("  Warning:  File exists");
   }
   else {
      sprintf(outname, "Write File");
      if (errno == ENOTDIR)
	 Wprintf("Error:  Incorrect pathname");
      else if (errno == EACCES)
	 Wprintf("Error:  Path not readable");
      else
         W3printf("  ");
   }

   height = ROWHEIGHT * 17;  /* 3 + (2 * MAXPROPS) */
   width = XTextWidth(appdata.xcfont, request, strlen(request)) + 20;
   bwidth = XTextWidth(appdata.xcfont, "Close", strlen("Close")) + 50;
   owidth = XTextWidth(appdata.xcfont, "Apply", strlen("Apply")) + 50;
   wwidth = XTextWidth(appdata.xcfont, outname, strlen(outname)) + 80;
   if (width < 500) width = 500;

   XtnSetArg(XtNwidth, &areawidth);
   XtnSetArg(XtNheight, &areaheight);
   XtGetValues(areawin->area, wargs, n); n = 0;
   XtTranslateCoords(areawin->area, (Position) (areawidth / 2 - width 
	/ 2 + popups * 20), (Position) (areaheight / 2 - height / 2 +
	popups * 20), &xpos, &ypos);
   XtnSetArg(XtNx, xpos);
   XtnSetArg(XtNy, ypos);
   popup = XtCreatePopupShell("prompt", transientShellWidgetClass,
        areawin->area, wargs, n); n = 0;
   popups++;

   XtnSetArg(XtNlayout, XwIGNORE);
   XtnSetArg(XtNwidth, width);
   XtnSetArg(XtNheight, height);
   dialog = XtCreateManagedWidget("dialog", XwbulletinWidgetClass,
        popup, wargs, n); n = 0;

   XtnSetArg(XtNx, 20);
   XtnSetArg(XtNy, ROWHEIGHT - 10);
   XtnSetArg(XtNstring, request);
   XtnSetArg(XtNborderWidth, 0);
   XtnSetArg(XtNgravity, WestGravity);
   XtnSetArg(XtNbackground, BARCOLOR);
   XtnSetArg(XtNfont, appdata.xcfont);
   titlearea = XtCreateManagedWidget("title", XwstaticTextWidgetClass,
	dialog, wargs, n); n = 0;

   XtnSetArg(XtNx, 20);
   XtnSetArg(XtNy, height - ROWHEIGHT - 10);
   XtnSetArg(XtNwidth, owidth); 
   XtnSetArg(XtNfont, appdata.xcfont);
   okbutton = XtCreateManagedWidget("Close", XwmenuButtonWidgetClass, 
	dialog, wargs, n); n = 0;

   XtnSetArg(XtNx, width - wwidth - 20);
   XtnSetArg(XtNy, height - ROWHEIGHT - 10);
   XtnSetArg(XtNwidth, wwidth);
   XtnSetArg(XtNfont, appdata.xcfont);
   XtnSetArg(XtNlabel, outname);
   wrbutton = XtCreateManagedWidget("Write File", XwmenuButtonWidgetClass,
	dialog, wargs, n); n = 0;

   for (i = 0; i < MAXPROPS; i++) {
      XtnSetArg(XtNx, 20);
      XtnSetArg(XtNy, ROWHEIGHT + MARGIN + 5 + (i * 2 * ROWHEIGHT));
      XtnSetArg(XtNstring, statics[i]);
      XtnSetArg(XtNborderWidth, 0);
      XtnSetArg(XtNgravity, WestGravity);
      XtnSetArg(XtNfont, appdata.xcfont);
      staticarea[i] = XtCreateManagedWidget("static", XwstaticTextWidgetClass,
	   dialog, wargs, n); n = 0;

      XtnSetArg(XtNx, 150);
      XtnSetArg(XtNy, ROWHEIGHT + MARGIN + (i * 2 * ROWHEIGHT));
      if (i < 5) {
         XtnSetArg(XtNheight, ROWHEIGHT + 5);
         XtnSetArg(XtNstring, edit[i]);
         XtnSetArg(XtNwidth, width - owidth - 190);
         pdptr = strchr(edit[i], '.');
         posit = (pdptr != NULL) ? (short)(pdptr - edit[i]) : strlen(edit[i]);
         XtnSetArg(XtNinsertPosition, posit);
         XtnSetArg(XtNscroll, XwAutoScrollHorizontal);
         XtnSetArg(XtNwrap, XwWrapOff);
         XtnSetArg(XtNfont, appdata.textfont);
         entertext[i] = XtCreateManagedWidget("Edit", XwtextEditWidgetClass,
	     dialog, wargs, n); n = 0; 

         XtnSetArg(XtNx, width - owidth - 20);
         XtnSetArg(XtNy, ROWHEIGHT + MARGIN + (i * 2 * ROWHEIGHT));
         XtnSetArg(XtNwidth, owidth); 
         XtnSetArg(XtNfont, appdata.xcfont);
         okays[i] = XtCreateManagedWidget("Apply", XwmenuButtonWidgetClass,
			dialog, wargs, n); n = 0;

         okstruct[i].textw = entertext[i];
	 okstruct[i].buttonw = okays[i];
         okstruct[i].setvalue = function[i];
         okstruct[i].dataptr = data[i];

         XtAddCallback(okays[i], XtNselect, getproptext, &okstruct[i]);
	 if (update[i] != NULL)
            XtAddCallback(okays[i], XtNselect, (XtCallbackProc)update[i], entertext);
         //XtOverrideTranslations(entertext[i], XtParseTranslationTable
         //     	(defaultTranslations));
         XtAddCallback(entertext[i], XtNexecute, (XtCallbackProc)getproptext,
		&okstruct[i]);
         if (update[i] != NULL) XtAddCallback(entertext[i], XtNexecute,
		(XtCallbackProc)update[i], entertext);

      }
      else {
	 XtnSetArg(XtNlabel, edit[i]);
         XtnSetArg(XtNfont, appdata.xcfont);
         entertext[i] = XtCreateManagedWidget("Toggle", XwpushButtonWidgetClass,
	    dialog, wargs, n); n = 0;
	 XtAddCallback(entertext[i], XtNselect, (XtCallbackProc)function[i], data[i]);
	 if (update[i] != NULL)
            XtAddCallback(entertext[i], XtNselect, (XtCallbackProc)update[i], entertext);
      }
   }

   /* If this filename is linked to other pages (multi-page output), add a button */
   /* which will unlink the page name from the other pages when toggled.	  */

   num_linked = pagelinks(areawin->page);
   XtnSetArg(XtNx, width - wwidth - 20);
   XtnSetArg(XtNy, ROWHEIGHT - 10);
   XtnSetArg(XtNset, true);
   XtnSetArg(XtNsquare, true);
   XtnSetArg(XtNborderWidth, 0);
   XtnSetArg(XtNfont, appdata.xcfont);
   sprintf(pstr, "%d Pages", num_linked);
   XtnSetArg(XtNlabel, pstr);
   allpages = XtCreateWidget("LToggle", XwtoggleWidgetClass, dialog, wargs, n); n = 0;
   XtAddCallback(allpages, XtNrelease, (XtCallbackProc)linkset, &okstruct[0]);

   /* If full-page pmode is chosen, there is an additional text structure.
      Make this text structure always but allow it to be managed and
      unmanaged as necessary. */

   XtnSetArg(XtNx, 240);
   XtnSetArg(XtNy, ROWHEIGHT + MARGIN + (10 * ROWHEIGHT));
   XtnSetArg(XtNset, (curpage->pmode & 2) ? true : false);
   XtnSetArg(XtNsquare, true);
   XtnSetArg(XtNborderWidth, 0);
   XtnSetArg(XtNfont, appdata.xcfont);
   autobutton = XtCreateWidget("Auto-fit", XwtoggleWidgetClass,
       dialog, wargs, n); n = 0;

   if (curpage->coordstyle == CM) {
      sprintf(fpedit, "%3.2f x %3.2f cm",
	 (float)curpage->pagesize.x / IN_CM_CONVERT,
	 (float)curpage->pagesize.y / IN_CM_CONVERT);
   }
   else {
      sprintf(fpedit, "%3.2f x %3.2f in",
	 (float)curpage->pagesize.x / 72.0,
	 (float)curpage->pagesize.y / 72.0);
   }
   XtnSetArg(XtNx, 240);
   XtnSetArg(XtNy, ROWHEIGHT + MARGIN + (12 * ROWHEIGHT));
   XtnSetArg(XtNheight, ROWHEIGHT + 5);
   XtnSetArg(XtNstring, fpedit);
   XtnSetArg(XtNwidth, width - owidth - 280);
   pdptr = strchr(fpedit, '.');
   posit = (pdptr != NULL) ? (short)(pdptr - fpedit) : strlen(fpedit);
   XtnSetArg(XtNscroll, XwAutoScrollHorizontal);
   XtnSetArg(XtNwrap, XwWrapOff);
   XtnSetArg(XtNfont, appdata.textfont);
   XtnSetArg(XtNinsertPosition, posit);
   fpentertext = XtCreateWidget("fpedit", XwtextEditWidgetClass,
       dialog, wargs, n); n = 0;

   XtnSetArg(XtNx, width - owidth - 20);
   XtnSetArg(XtNy, ROWHEIGHT + MARGIN + (12 * ROWHEIGHT));
   XtnSetArg(XtNwidth, owidth);
   XtnSetArg(XtNfont, appdata.xcfont);
   XtnSetArg(XtNlabel, "Apply");
   fpokay = XtCreateWidget("fpokay", XwmenuButtonWidgetClass,
      dialog, wargs, n); n = 0;

   fpokstruct.textw = fpentertext;
   fpokstruct.buttonw = fpokay;
   fpokstruct.setvalue = setpagesize;
   fpokstruct.dataptr = &(curpage->pagesize);

   XtAddCallback(fpokay, XtNselect, (XtCallbackProc)getproptext, &fpokstruct);
   XtAddCallback(fpokay, XtNselect, (XtCallbackProc)updatetext, entertext);
   //XtOverrideTranslations(fpentertext, XtParseTranslationTable
   //     (defaultTranslations));
   XtAddCallback(fpentertext, XtNexecute, (XtCallbackProc)getproptext, &fpokstruct);
   XtAddCallback(fpentertext, XtNexecute, (XtCallbackProc)updatetext, entertext);
   XtAddCallback(autobutton, XtNselect, (XtCallbackProc)autoset, entertext);
   XtAddCallback(autobutton, XtNrelease, (XtCallbackProc)autostop, NULL);

   if (curpage->pmode & 1) {
      XtManageChild(fpentertext);
      XtManageChild(fpokay);
      XtManageChild(autobutton);
   }

   if (num_linked > 1) {
      XtManageChild(allpages);
   }

   /* end of pagesize extra Widget definitions */

   donestruct = (popupstruct *) malloc(sizeof(popupstruct));
   donestruct->popup = popup;
   donestruct->buttonptr = savebutton;
   donestruct->filter = NULL;
   XtAddCallback(okbutton, XtNselect, (XtCallbackProc)destroypopup, donestruct);

   /* Send setfile() the widget entertext[0] in case because user sometimes
      forgets to type "okay" but buffer contains the expected filename */

   XtAddCallback(wrbutton, XtNselect, (XtCallbackProc)setfile, entertext[0]);

   /* Begin Popup */

   XtPopup(popup, XtGrabNone);

   /* set the input focus for the window */

   wmhints = XGetWMHints(popup);
   wmhints->flags |= InputHint;
   wmhints->input = true;
   XSetWMHints(popup, wmhints);
   XSetTransientForHint(popup, top);
   XFree(wmhints);

   for (i = 0; i < 5; i++)
      XDefineCursor(entertext[i], TEXTPTR);
}

#endif

/*-------------------------------------------------*/
/* Print a string to the message widget. 	   */ 
/* Note: Widget message must be a global variable. */
/* For formatted strings, format first into _STR   */
/*-------------------------------------------------*/

void clrmessage(XtPointer, XtIntervalId *)
{
   char buf1[50], buf2[50];

   /* Don't write over the report of the edit string contents,	*/
   /* if we're in one of the label edit modes 			*/

   if (eventmode == TEXT_MODE || eventmode == ETEXT_MODE)
      charreport(TOLABEL(EDITPART));
   else {
      measurestr(xobjs.pagelist[areawin->page].gridspace, buf1);
      measurestr(xobjs.pagelist[areawin->page].snapspace, buf2);
      Wprintf("Grid %.50s : Snap %.50s", buf1, buf2);
   }
}

/*----------------------------------------------------------------------*/
/* This is the non-Tcl version of tcl_vprintf()				*/
/*----------------------------------------------------------------------*/

void xc_vprintf(Widget widget, const char *fmt, va_list args_in)
{
   va_list args;
   QString s;

   va_copy(args, args_in);
   s.vsprintf(fmt, args);
   va_end(args);

   if (QLabel * lbl = qobject_cast<QLabel*>(widget)) lbl->setText(s);
   else if (QPushButton * btn = qobject_cast<QPushButton*>(widget)) btn->setText(s);

   if (widget == message3) {
      if (printtime_id != 0) {
         xcRemoveTimeout(printtime_id);
         printtime_id = 0;
      }
   }

   /* 10 second timeout */
   if (widget == message3) {
      printtime_id = xcAddTimeout(10000, clrmessage, NULL);
   }
}

/*------------------------------------------------------------------------------*/
/* W3printf is the same as Wprintf because the non-Tcl based version does not	*/
/* duplicate output to stdout/stderr.						*/
/*------------------------------------------------------------------------------*/

void W3printf(const char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  xc_vprintf(message3, format, ap);
  va_end(ap);
}

/*------------------------------------------------------------------------------*/

void Wprintf(const char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  xc_vprintf(message3, format, ap);
  va_end(ap);
}

/*------------------------------------------------------------------------------*/

void W1printf(const char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  xc_vprintf(message1, format, ap);
  va_end(ap);
}   

/*------------------------------------------------------------------------------*/

void W2printf(const char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  xc_vprintf(message2, format, ap);
  va_end(ap);
}   

/*------------------------------------------------------------------------------*/

void getcommand(Widget cmdw, void*, void*)
{
    QLineEdit * le = qobject_cast<QLineEdit*>(cmdw);
    QByteArray str = le->text().toLocal8Bit();
#ifdef HAVE_PYTHON
    execcommand(0, str + 4);
#else
    execcommand(0, str.data() + 2);
#endif
    cmdw->hide();
    message3->show();
}

/*------------------------------------------------------------------------------*/
/* "docommand" overlays the message3 widget temporarily with a TextEdit widget. */
/*------------------------------------------------------------------------------*/

void docommand()
{
   if (overlay == NULL) {
       overlay = new QLineEdit();
       overlay->setObjectName("Command");
       message3->layout()->addWidget(overlay);
       message3->hide();
       XtAddCallback(overlay, XtNexecute, getcommand, NULL);
   }
   else {
       overlay->show();
       message3->hide();
   }
   overlay->setFocus();
   QLineEdit * le = qobject_cast<QLineEdit*>(overlay);

#ifdef HAVE_PYTHON
   le->setText(">>> ");
#else
   le->setText("? ");
#endif
}

/*----------------------------------------------------------------------*/
/* Event handler for input focus					*/
/*----------------------------------------------------------------------*/

#ifdef INPUT_FOCUS

void mappinghandler(Widget w, caddr_t clientdata, QEvent* *event)
{
   switch(event->type) {
      case MapNotify:
	 /* Fprintf(stderr, "Window top was mapped.  Setting input focus\n"); */
	 areawin->mapped = true;
         XSetInputFocus(w, RevertToPointerRoot, CurrentTime);
	 break;
      case UnmapNotify:
	 /* Fprintf(stderr, "Window top was unmapped\n"); */
	 areawin->mapped = false;
	 break;
   }
}

#endif

/*----------------------------------------------------------------------*/

void clientmessagehandler(QWidget* w, caddr_t, QEvent* *event)
{
    Q_UNUSED(w);
    Q_UNUSED(event);
#if 0
   if (event->type == ClientMessage) {
      if (render_client(event))
	 return;

#ifdef INPUT_FOCUS
      if (areawin->mapped) {
         /* Fprintf(stderr, "Forcing input focus\n"); */
         XSetInputFocus(w, RevertToPointerRoot, CurrentTime);
      }
#endif
   }
#endif
}

/*----------------------------------------------------------------------*/
/* Event handler for WM_DELETE_WINDOW message.                          */
/*----------------------------------------------------------------------*/

void delwin(Widget w, popupstruct *, void *)
{
    Q_ASSERT(w == top);
    quitcheck((QAction*)1, NULL, NULL);
    /* in the case of "quitcheck", we want to make sure that the signal	*/
    /* handler is reset to default behavior if the quit command is	*/
    /* canceled from inside the popup prompt window.			*/
    signal(SIGINT, dointr);
}

/*----------------------------------------------------------------------*/

