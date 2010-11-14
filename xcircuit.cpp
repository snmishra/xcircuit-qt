/*----------------------------------------------------------------------*/
/* xcircuit.c --- An X-windows program for drawing circuit diagrams	*/
/* Copyright (c) 2002  R. Timothy Edwards				*/
/*									*/
/* This program is free software; you can redistribute it and/or modify */
/* it under the terms of the GNU General Public License as published by */
/* the Free Software Foundation; either version 2 of the License, or	*/
/* (at your option) any later version.					*/
/*									*/
/* This program is distributed in the hope that it will be useful,	*/
/* but WITHOUT ANY WARRANTY; without even the implied warranty of	*/
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the	*/
/* GNU General Public License for more details.				*/
/*									*/
/* You should have received a copy of the GNU General Public License	*/
/* along with this program; if not, write to:				*/
/*	Free Software Foundation, Inc.					*/
/*	59 Temple Place, Suite 330					*/
/*	Boston, MA  02111-1307  USA					*/
/*									*/
/* See file ./README for contact information				*/
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*      Xcircuit written by Tim Edwards beginning 8/13/93 		*/
/*----------------------------------------------------------------------*/

#include <QAction>
#include <QMenu>
#include <QFile>

#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <cerrno>
#include <climits>
#include <clocale>
#include <cctype>
#include <stdint.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef TCL_WRAPPER
#include <tk.h>
#endif

#ifndef P_tmpdir
#define P_tmpdir TMPDIR
#endif

#include "xcircuit.h"
#include "matrix.h"
#include "cursors.h"
#include "prototypes.h"
#include "xcqt.h"
#include "colors.h"

/*----------------------------------------------------------------------*/
/* Global Variable definitions						*/
/*----------------------------------------------------------------------*/

char	 _STR[150];   /* Generic multipurpose string */
short	 popups;      /* total number of popup widgets on the screen */
int	 pressmode;   /* Whether we are in a press & hold state */
Colormap cmap;
Cursor	appcursors[NUM_CURSORS];
ApplicationData appdata;
XCWindowData *areawin;
Globaldata xobjs;
short menusize;
XtIntervalId printtime_id;
short beeper;
short fontcount;
fontinfo *fonts;

/*----------------------------------------------------------------------*/
/* Externally defined variables						*/
/*----------------------------------------------------------------------*/

extern aliasptr aliastop;
extern float version;

#ifdef TCL_WRAPPER
extern Tcl_Interp *xcinterp;
#endif

//
//

/*---------------*/
/* Quit xcircuit */
/*---------------*/

void quit(QAction* a, void*, void*)
{
   /* exit ghostscript if background rendering was enabled */

   exit_gs();

#ifdef TCL_WRAPPER
   /* exit ngspice if the simulator is still running */

   exit_spice();
#endif

   /* remove any temporary files created for background rendering */

   for (int i = 0; i < xobjs.pages; i++) {
      if (xobjs.pagelist[i].pageinst != NULL)
         if (xobjs.pagelist[i].background.name != (char *)NULL)
            if (xobjs.pagelist[i].background.name[0] == '@')
                QFile::remove(xobjs.pagelist[i].background.name.mid(1));
   }

   /* remove the temporary file and free the filename memory	*/
   /* if w = NULL, quit() was reached from Ctrl-C.  Don't 	*/
   /* remove the temporary file;  instead, notify user of the 	*/
   /* filename.							*/

   if (xobjs.tempfile != NULL) {
      if (a != NULL) {
         if (!QFile::remove(xobjs.tempfile))
            Fprintf(stderr, "Error deleting file \"%s\"\n", xobjs.tempfile.toLocal8Bit().data());
      }
      else
	 Fprintf(stderr, "Ctrl-C exit:  reload workspace from \"%s\"\n",
                xobjs.tempfile.toLocal8Bit().data());
   }

#if defined(HAVE_PYTHON)
   /* exit by exiting the Python interpreter, if enabled */
   exit_interpreter();
#elif defined(TCL_WRAPPER)
   exit(0);	/* For now, exit.  Later, clear all pages and return to interp. */
#else
   qApp->exit(0);
#endif

}

/*--------------------------------------------------------------*/
/* Check for changes in an object and all of its descendents	*/
/*--------------------------------------------------------------*/

u_short getchanges(objectptr thisobj)
{
   u_short changes = thisobj->changes;

   for (objinstiter pinst; thisobj->values(pinst); ) {
         changes += getchanges(pinst->thisobject);
   }
   return changes;
}

/*--------------------------------------------------------------*/
/* Check to see if any objects in xcircuit have been modified	*/
/* without saving.						*/
/*--------------------------------------------------------------*/

u_short countchanges(char **promptstr)
{
  int slen = 1, i;
   u_short locchanges, changes = 0, words = 1;
   objectptr thisobj;
   char *fname;
   TechPtr ns;

   if (promptstr != NULL) slen += strlen(*promptstr);

   for (i = 0; i < xobjs.pages; i++) {
      if (xobjs.pagelist[i].pageinst != NULL) {
         thisobj = xobjs.pagelist[i].pageinst->thisobject;
	 locchanges = getchanges(thisobj);
         if (locchanges > 0) {
	    if (promptstr != NULL) {
	       slen += strlen(thisobj->name) + 2;
	       *promptstr = (char *)realloc(*promptstr, slen);
	       if ((words % 8) == 0) strcat(*promptstr, ",\n");
	       else if (changes > 0) strcat(*promptstr, ", ");
	       strcat(*promptstr, thisobj->name);
	       words++;
	    }
	    changes += locchanges;
	 }
      }
   }

   /* Check all library objects for unsaved changes */

   for (ns = xobjs.technologies; ns != NULL; ns = ns->next) {
      tech_set_changes(ns);
      if ((ns->flags & LIBRARY_CHANGED) != 0) {
	 changes++;
	 if (promptstr != NULL) {
	    fname = ns->filename;
	    if (fname != NULL) {
	       slen += strlen(fname) + 2;
	       *promptstr = (char *)realloc(*promptstr, slen);
	       if ((words % 8) == 0) strcat(*promptstr, ",\n");
	       else if (changes > 0) strcat(*promptstr, ", ");
	       strcat(*promptstr, fname);
	       words++;
	    }
	 }
      }
   }
   return changes;
}

/*----------------------------------------------*/
/* Check for conditions to approve program exit */
/*----------------------------------------------*/

#ifdef TCL_WRAPPER

void quitcheck(Widget w, caddr_t clientdata, caddr_t calldata)
{
   char *promptstr;
   bool doprompt = false;

   /* enable default interrupt signal handler during this time, so that */
   /* a double Control-C will ALWAYS exit.				*/

   signal(SIGINT, SIG_DFL);

   promptstr = (char *)malloc(60);
   strcpy(promptstr, ".query.title.field configure -text \"Unsaved changes in: ");

   /* Check all page objects for unsaved changes */

   doprompt = countchanges(&promptstr) > 0;

   /* If any changes have not been saved, generate a prompt */

   if (doprompt) {
      promptstr = (char *)realloc(promptstr, strlen(promptstr) + 15);
      strcat(promptstr, "\nQuit anyway?");

      strcat(promptstr, "\"");
      Tcl_Eval(xcinterp, promptstr);
      Tcl_Eval(xcinterp, ".query.bbar.okay configure -command {quitnocheck}");
      Tcl_Eval(xcinterp, "wm deiconify .query");
      Tcl_Eval(xcinterp, "raise .query");
      free(promptstr);
   }
   else {
      free(promptstr);
      quit(w, NULL);
   }
}

#endif

/*--------------------------------------*/
/* A gentle Ctrl-C shutdown		*/
/*--------------------------------------*/

void dointr(int)
{
   quitcheck(NULL, NULL, NULL);
}

/*--------------*/
/* Null routine */
/*--------------*/

void DoNothing(QAction*, void*, void*)
{
   /* Nothing here! */
}

/*-----------------------------------------------------------------------*/
/* Write page scale (overall scale, and X and Y dimensions) into strings */
/*-----------------------------------------------------------------------*/

void writescalevalues(char *scdest, char *xdest, char *ydest)
{
   float oscale, psscale;
   int width, height;
   Pagedata    *curpage;

   curpage = &xobjs.pagelist[areawin->page];
   oscale = curpage->outscale;
   psscale = getpsscale(oscale, areawin->page);

   width = toplevelwidth(curpage->pageinst, NULL);
   height = toplevelheight(curpage->pageinst, NULL);

   sprintf(scdest, "%6.5f", oscale);
   if (curpage->coordstyle == CM) {
      sprintf(xdest, "%6.5f", (width * psscale) / IN_CM_CONVERT);
      sprintf(ydest, "%6.5f", (height * psscale) / IN_CM_CONVERT);
   }
   else {
      sprintf(xdest, "%6.5f", (width * psscale) / 72.0);
      sprintf(ydest, "%6.5f", (height * psscale) / 72.0);
   }
}

/*------------------------------------------------------------------------------*/
/* diagnostic tool for translating the event mode into a string and printing	*/
/* to stderr (for debugging only).						*/
/*------------------------------------------------------------------------------*/

void printeventmode() {
   Fprintf(stderr, "eventmode is \'");
   switch(eventmode) {
      case NORMAL_MODE:
	 Fprintf(stderr, "NORMAL");
	 break;
      case MOVE_MODE:
	 Fprintf(stderr, "MOVE");
	 break;
      case COPY_MODE:
	 Fprintf(stderr, "COPY");
	 break;
      case SELAREA_MODE:
	 Fprintf(stderr, "SELAREA");
	 break;
      case CATALOG_MODE:
	 Fprintf(stderr, "CATALOG");
	 break;
      case CATTEXT_MODE:
	 Fprintf(stderr, "CATTEXT");
	 break;
      case CATMOVE_MODE:
	 Fprintf(stderr, "CATMOVE");
	 break;
      case FONTCAT_MODE:
	 Fprintf(stderr, "FONTCAT");
	 break;
      case EFONTCAT_MODE:
	 Fprintf(stderr, "EFONTCAT");
	 break;
      case TEXT_MODE:
	 Fprintf(stderr, "TEXT");
	 break;
      case ETEXT_MODE:
	 Fprintf(stderr, "ETEXT");
	 break;
      case WIRE_MODE:
	 Fprintf(stderr, "WIRE");
	 break;
      case BOX_MODE:
	 Fprintf(stderr, "BOX");
	 break;
      case EPOLY_MODE:
	 Fprintf(stderr, "EPOLY");
	 break;
      case ARC_MODE:
	 Fprintf(stderr, "ARC");
	 break;
      case EARC_MODE:
	 Fprintf(stderr, "EARC");
	 break;
      case SPLINE_MODE:
	 Fprintf(stderr, "SPLINE");
	 break;
      case ESPLINE_MODE:
	 Fprintf(stderr, "ESPLINE");
	 break;
      case EPATH_MODE:
	 Fprintf(stderr, "EPATH");
	 break;
      case EINST_MODE:
	 Fprintf(stderr, "EINST");
	 break;
      case ASSOC_MODE:
	 Fprintf(stderr, "ASSOC");
	 break;
      case RESCALE_MODE:
	 Fprintf(stderr, "RESCALE");
	 break;
      case PAN_MODE:
	 Fprintf(stderr, "PAN");
	 break;
      default:
	 Fprintf(stderr, "(unknown)");
	 break;
   }
   Fprintf(stderr, "_MODE\'\n");
}

/*------------------------------------------------------------------------------*/

#ifdef TCL_WRAPPER

/*------------------------------------------------------------------------------*/
/* "docommand" de-iconifies the TCL console.					*/
/*------------------------------------------------------------------------------*/

void docommand()
{
   Tcl_Eval(xcinterp, "catch xcircuit::raiseconsole");
}

/*------------------------------------------------------------------------------*/
/* When all else fails, install your own colormap			 	*/
/*------------------------------------------------------------------------------*/

int installowncmap()
{
   Colormap newcmap;

   Fprintf(stdout, "Installing my own colormap\n");

   /* allocate a new colormap */

   newcmap = XCopyColormapAndFree(cmap);
   if (newcmap == (Colormap)NULL) return (-1);
   cmap = newcmap;
   return(1);
}

#endif /* TCL_WRAPPER */

/*-------------------------------------------------------------------------*/
/* Make the cursors from the cursor bit data				   */
/*-------------------------------------------------------------------------*/

void makecursors()
{
   QRgb fgcolor, bgcolor;

   bgcolor = BACKGROUND;
   fgcolor = FOREGROUND;

   ARROW = XCreatePixmapCursor(CreateBitmapFromData(arrow_bits,
        arrow_width, arrow_height), CreateBitmapFromData(arrowmask_bits,
        arrow_width, arrow_height), fgcolor, bgcolor, arrow_x_hot, arrow_y_hot);
   CROSS = XCreatePixmapCursor(CreateBitmapFromData(cross_bits,
        cross_width, cross_height), CreateBitmapFromData(crossmask_bits,
        cross_width, cross_height), fgcolor, bgcolor, cross_x_hot, cross_y_hot);
   SCISSORS = XCreatePixmapCursor(CreateBitmapFromData(scissors_bits,
        scissors_width, scissors_height), CreateBitmapFromData(
        scissorsmask_bits, scissors_width, scissors_height), fgcolor,
        bgcolor, scissors_x_hot, scissors_y_hot);
   EDCURSOR = XCreatePixmapCursor(CreateBitmapFromData(exx_bits,
        exx_width, exx_height), CreateBitmapFromData(exxmask_bits,
        exx_width, exx_height), fgcolor, bgcolor, exx_x_hot, exx_y_hot);
   COPYCURSOR = XCreatePixmapCursor(CreateBitmapFromData(copy_bits,
        copy_width, copy_height), CreateBitmapFromData(copymask_bits,
        copy_width, copy_height), fgcolor, bgcolor, copy_x_hot, copy_y_hot);
   ROTATECURSOR = XCreatePixmapCursor(CreateBitmapFromData(rot_bits,
        rot_width, rot_height), CreateBitmapFromData(rotmask_bits,
        rot_width, rot_height), fgcolor, bgcolor, circle_x_hot, circle_y_hot);
   QUESTION = XCreatePixmapCursor(CreateBitmapFromData(question_bits,
        question_width, question_height), CreateBitmapFromData(
	questionmask_bits, question_width, question_height),
        fgcolor, bgcolor, question_x_hot, question_y_hot);
   CIRCLE = XCreatePixmapCursor(CreateBitmapFromData(circle_bits,
        circle_width, circle_height), CreateBitmapFromData(circlemask_bits,
        circle_width, circle_height), fgcolor, bgcolor, circle_x_hot, circle_y_hot);
   HAND = XCreatePixmapCursor(CreateBitmapFromData(hand_bits,
        hand_width, hand_height), CreateBitmapFromData(handmask_bits,
        hand_width, hand_height), fgcolor, bgcolor, hand_x_hot, hand_y_hot);

   TEXTPTR = new QCursor(Qt::IBeamCursor);
   WAITFOR = new QCursor(Qt::WaitCursor);
}

/*----------------------------------------------------------------------*/
/* Remove a window structure and deallocate all memory used by it.	*/
/* If it is the last window, this is equivalent to calling "quit".	*/
/*----------------------------------------------------------------------*/

void delete_window(XCWindowData *window)
{
   XCWindowData *searchwin, *lastwin = NULL;

   if (xobjs.windowlist->next == NULL) {
      quitcheck(NULL, NULL, NULL);
      return;
   }

   for (searchwin = xobjs.windowlist; searchwin != NULL; searchwin =
		searchwin->next) {
      if (searchwin == window) {

	 /* Free any select list */
	 if (searchwin->selects > 0) free(searchwin->selectlist);

	 /* Free the matrix and pushlist stacks */

	 free_stack(&searchwin->hierstack);
	 free_stack(&searchwin->stack);

	 if (lastwin != NULL)
	    lastwin->next = searchwin->next;
	 else
	    xobjs.windowlist = searchwin->next;
	 break;
      }
      lastwin = searchwin;
   }

   if (searchwin == NULL) {
      Wprintf("No such window in list!\n");
   }
   else {
      if (areawin == searchwin) areawin = xobjs.windowlist;
      free(searchwin);
   }
}

/*----------------------------------------------------------------------*/
/* Create a new window structure and initialize it.			*/
/* Return a pointer to the new window.					*/
/*----------------------------------------------------------------------*/

XCWindowData *create_new_window()
{
   XCWindowData *newwindow;

   newwindow = new XCWindowData;

   /* Prepend to linked window list in global data (xobjs) */
   newwindow->next = xobjs.windowlist;
   xobjs.windowlist = newwindow;

   return newwindow;
}

XCWindowData::XCWindowData()
{
    toolbar_on = true;
    viewport = NULL;
    mapped = false;
    psfont = 0;
    justify = FLIPINV;
    page = 0;
    textscale = 1.0;
    linewidth = 1.0;
    zoomfactor = SCALEFAC;
    style = UNCLOSED;
    invert = false;
    antialias = false;
    axeson = true;
    snapto = true;
    gridon = true;
    center = true;
    bboxon = false;
    filter = ALL_TYPES;
    editinplace = true;
    selects = 0;
    selectlist = NULL;
    lastlibrary = 0;
    manhatn = false;
    boxedit = MANHATTAN;
    lastbackground = NULL;
    editstack = new object;
    stack = NULL;   /* at the top of the hierarchy */
    hierstack = NULL;
    pinpointon = false;
    pinattach = false;
    buschar = '(';	/* Vector notation for buses */
    defaultcursor = &CROSS;
    event_mode = NORMAL_MODE;
    attachto = -1;
    color = DEFAULTCOLOR;
    time_id = 0;
    vscale = 1;
    pcorner.x = pcorner.y = 0;
    topinstance = NULL;
    next = NULL;
    updates = 0;
}

/*----------------------------------------------------------------------*/
/* Preparatory initialization (to be run before setting up the GUI)	*/
/*----------------------------------------------------------------------*/

void pre_initialize()
{
   short i, page;

   /*-------------------------------------------------------------*/
   /* Force LC_NUMERIC locale to en_US for decimal point = period */
   /* notation.  The environment variable LC_NUMERIC overrides if */
   /* it is set explicitly, so it has to be unset first to allow  */
   /* setlocale() to work.					  */
   /*-------------------------------------------------------------*/

#ifdef HAVE_PUTENV
   putenv("LC_ALL=en_US");
   putenv("LC_NUMERIC=en_US");
   putenv("LANG=POSIX");
#else
   unsetenv("LC_ALL");
   unsetenv("LC_NUMERIC");
   setenv("LANG", "POSIX", 1);
#endif
   setlocale(LC_ALL, "en_US");

   /*---------------------------*/
   /* initialize user variables */
   /*---------------------------*/

   version = PROG_VERSION;
   aliastop = NULL;
   for (page = 0; page < PAGES; page++) {
      xobjs.pagelist.append(Pagedata());
      xobjs.pagelist[page].pageinst = NULL;
   }
   /* Set values for the first page */
   xobjs.pagelist[0].wirewidth = 2.0;
   xobjs.pagelist[0].outscale = 1.0;
   xobjs.pagelist[0].background.name = (char *)NULL;
   xobjs.pagelist[0].pmode = 0;
   xobjs.pagelist[0].orient = 0;
   xobjs.pagelist[0].gridspace = DEFAULTGRIDSPACE;
   xobjs.pagelist[0].snapspace = DEFAULTSNAPSPACE;
   xobjs.pagelist[0].drawingscale.x = xobjs.pagelist[0].drawingscale.y = 1;
   xobjs.pagelist[0].coordstyle = INTERNAL;
   xobjs.pagelist[0].pagesize.x = 612;
   xobjs.pagelist[0].pagesize.y = 792;
   xobjs.pagelist[0].margins.x = 72;
   xobjs.pagelist[0].margins.y = 72;

   xobjs.hold = true;
   xobjs.showtech = false;
   xobjs.suspend = (char)0;	/* Suspend graphics until finished with startup */
   xobjs.new_changes = 0;
   xobjs.filefilter = true;
   xobjs.retain_backup = false;	/* default: remove backup after file write */
   signal(SIGINT, dointr);
   printtime_id = 0;

   xobjs.technologies = NULL;
   xobjs.undostack = NULL;
   xobjs.redostack = NULL;

   /* Set the temporary directory name as compiled, unless overridden by */
   /* environment variable "TMPDIR".					 */

   xobjs.tempdir = getenv("TMPDIR");
   if (xobjs.tempdir == NULL) xobjs.tempdir = strdup(TEMP_DIR);

   xobjs.windowlist = (XCWindowDataPtr)NULL;
   areawin = NULL;

   xobjs.numlibs = LIBS - LIBRARY - 1;
   xobjs.fontlib.number = 0;
   xobjs.userlibs = new Library[xobjs.numlibs];
   for (i = 0; i < xobjs.numlibs; i++) {
      xobjs.userlibs[i].library = (objectptr *) malloc(sizeof(objectptr));
      xobjs.userlibs[i].instlist = NULL;
      xobjs.userlibs[i].number = 0;
   }
   xobjs.imagelist = NULL;
   xobjs.images = 0;

   fontcount = 0;
   fonts = new fontinfo;
   fonts[0].encoding = NULL;	/* To prevent segfaults */
   fonts[0].psname = NULL;
   fonts[0].family = NULL;

   /* Initialization of objects requires values for the window width and height, */
   /* so set up the widgets and realize them first.				 */

   popups = 0;        /* no popup windows yet */
   beeper = 1;        /* Ring bell on certain warnings or errors */
   pressmode = 0;	/* not in a button press & hold mode yet */
   initsplines();	/* create lookup table of spline parameters */
}

#ifdef TCL_WRAPPER

/*----------------------------------------------------------------------*/
/* Create a new Handle object in Tcl */
/*----------------------------------------------------------------------*/

static void UpdateStringOfHandle _ANSI_ARGS_((Tcl_Obj *objPtr));
static int SetHandleFromAny _ANSI_ARGS_((Tcl_Interp *interp, Tcl_Obj *objPtr));

static Tcl_ObjType tclHandleType = {
    "handle",				/* name */
    (Tcl_FreeInternalRepProc *) NULL,	/* freeIntRepProc */
    (Tcl_DupInternalRepProc *) NULL,	/* dupIntRepProc */
    UpdateStringOfHandle,		/* updateStringProc */
    SetHandleFromAny			/* setFromAnyProc */
};

/*----------------------------------------------------------------------*/

static void
UpdateStringOfHandle(objPtr)
    Tcl_Obj *objPtr;   /* Int object whose string rep to update. */
{
    char buffer[TCL_INTEGER_SPACE];
    int len;

    sprintf(buffer, "H%08lX", objPtr->internalRep.longValue);
    len = strlen(buffer);

    objPtr->bytes = Tcl_Alloc((u_int)len + 1);
    strcpy(objPtr->bytes, buffer);
    objPtr->length = len;
}

/*----------------------------------------------------------------------*/

static int
SetHandleFromAny(interp, objPtr)
    Tcl_Interp *interp;         /* Used for error reporting if not NULL. */
    Tcl_Obj *objPtr;   /* The object to convert. */
{  
    Tcl_ObjType *oldTypePtr = objPtr->typePtr;
    char *string, *end;
    int length;
    char *p;
    long newLong;
    pushlistptr newstack = NULL;

    string = Tcl_GetStringFromObj(objPtr, &length);
    errno = 0;
    for (p = string;  isspace((u_char)(*p));  p++);

nexthier:

    if (*p++ != 'H') {
	if (interp != NULL) {
            Tcl_ResetResult(interp);
            Tcl_AppendToObj(Tcl_GetObjResult(interp), 
		"handle is identified by leading H and hexidecimal value only", -1);
        }
        free_stack(&newstack);
        return TCL_ERROR;
    } else {
        newLong = strtoul(p, &end, 16);
    }
    if (end == p) {
        badHandle:
        if (interp != NULL) {
            /*
             * Must copy string before resetting the result in case a caller
             * is trying to convert the interpreter's result to an int.
             */

            char buf[100];
            sprintf(buf, "expected handle but got \"%.50s\"", string);
            Tcl_ResetResult(interp);
            Tcl_AppendToObj(Tcl_GetObjResult(interp), buf, -1);
        }
        free_stack(&newstack);
        return TCL_ERROR;
    }
    if (errno == ERANGE) {
        if (interp != NULL) {
            char *s = "handle value too large to represent";
            Tcl_ResetResult(interp);
            Tcl_AppendToObj(Tcl_GetObjResult(interp), s, -1);
            Tcl_SetErrorCode(interp, "ARITH", "IOVERFLOW", s, (char *) NULL);
        }
        free_stack(&newstack);
        return TCL_ERROR;
    }
    /*
     * Make sure that the string has no garbage after the end of the handle.
     */
   
    while ((end < (string+length)) && isspace((u_char)(*end))) end++;
    if (end != (string+length)) {
       /* Check for handles separated by slashes.  If present,	*/      
       /* then generate a hierstack.				*/

	if ((end != NULL) && (*end == '/')) {
	   objinstptr refinst, chkinst;
	   genericptr *rgen;

	   *end = '\0';
           newLong = strtoul(p, &end, 16);
	   p = end + 1;
	   *end = '/';
	   refinst = (newstack == NULL) ? areawin->topinstance : newstack->thisinst;
           chkinst = (objinstptr)((intptr_t)(newLong));
	   /* Ensure that chkinst is in the plist of			*/
	   /* refinst->thisobject, and that it is type objinst.	*/
	   for (rgen = refinst->thisobject->plist; rgen < refinst->thisobject->plist
			+ refinst->thisobject->parts; rgen++) {
	      if ((objinstptr)(*rgen) == chkinst) {
		 if (ELEMENTTYPE(*rgen) != OBJINST) {
		    free_stack(&newstack);
		    Tcl_SetResult(interp, "Hierarchical element handle "
				"component is not an object instance.", NULL);
		    return TCL_ERROR;
		 }
		 break;
	      }
	   }
	   if (rgen == refinst->thisobject->plist + refinst->thisobject->parts) {
               Tcl_SetResult(interp, "Bad component in hierarchical "
			"element handle.", NULL);
	       free_stack(&newstack);
	       return TCL_ERROR;
	   }
	   push_stack(&newstack, chkinst);
	   goto nexthier;
        }
	else
	   goto badHandle;
    }
   
    /* Note that this check won't prevent a hierarchical selection from	*/
    /* being added to a non-hierarchical selection.			*/

    if (areawin->hierstack != NULL) {
       if ((newstack == NULL) || (newstack->thisinst !=
		areawin->hierstack->thisinst)) {
	  Tcl_SetResult(interp, "Attempt to select components in different "
			"objects.", NULL);
          free_stack(&newstack);
	  return TCL_ERROR;
       }
    }
    free_stack(&areawin->hierstack);
    areawin->hierstack = newstack;

    /*
     * The conversion to handle succeeded. Free the old internalRep before
     * setting the new one. We do this as late as possible to allow the
     * conversion code, in particular Tcl_GetStringFromObj, to use that old
     * internalRep.
     */
   
    if ((oldTypePtr != NULL) && (oldTypePtr->freeIntRepProc != NULL)) {
        oldTypePtr->freeIntRepProc(objPtr);
    }
   
    objPtr->internalRep.longValue = newLong;
    objPtr->typePtr = &tclHandleType;
    return TCL_OK;
}  

/*----------------------------------------------------------------------*/

Tcl_Obj *
Tcl_NewHandleObj(optr)
    void *optr;      /* Int used to initialize the new object. */
{
    Tcl_Obj *objPtr;

    objPtr = Tcl_NewObj();
    objPtr->bytes = NULL;

    objPtr->internalRep.longValue = (long)(optr);
    objPtr->typePtr = &tclHandleType;
    return objPtr;
}

/*----------------------------------------------------------------------*/

int
Tcl_GetHandleFromObj(interp, objPtr, handlePtr)
    Tcl_Interp *interp;		/* Used for error reporting if not NULL. */
    Tcl_Obj *objPtr;	/* The object from which to get a int. */
    void **handlePtr;	/* Place to store resulting int. */
{
    long l;
    int result;

    if (objPtr->typePtr != &tclHandleType) {
        result = SetHandleFromAny(interp, objPtr);
        if (result != TCL_OK) {
            return result;
        }
    }
    l = objPtr->internalRep.longValue;
    if (((long)((int)l)) == l) {
        *handlePtr = (void *)objPtr->internalRep.longValue;
        return TCL_OK;
    }
    if (interp != NULL) {
        Tcl_ResetResult(interp);
        Tcl_AppendToObj(Tcl_GetObjResult(interp),
                "value too large to represent as handle", -1);
    }
    return TCL_ERROR;
}


#endif

/*----------------------------------------------------------------------*/
/* Routine to initialize variables after the GUI has been set up	*/
/*----------------------------------------------------------------------*/

void post_initialize()
{
   short i;

   /*--------------------------------------------------*/
   /* Setup the (simple) colormap and make the cursors */
   /*--------------------------------------------------*/

   setcolorscheme(true);
   makecursors();
   param_init();

   /* Now that we have values for the window width and height, we can initialize */
   /* the page objects.								 */

   xobjs.libtop = (objinstptr *)malloc(LIBS * sizeof(objinstptr));
   for (i = 0; i < LIBS; i++) {
      objectptr newlibobj = new object;
      xobjs.libtop[i] = new objinst(newlibobj);
   }

   /* Give names to the five default libraries */
   strcpy(xobjs.libtop[FONTLIB]->thisobject->name, "Font Character List");
   strcpy(xobjs.libtop[PAGELIB]->thisobject->name, "Page Directory");
   strcpy(xobjs.libtop[LIBLIB]->thisobject->name,  "Library Directory");
   strcpy(xobjs.libtop[USERLIB]->thisobject->name, "User Library");
   renamelib(USERLIB);

   changepage(0);

   /* Centering the view is not required here because the default values */
   /* set in object::object() should correctly position the empty page in the	 */
   /* middle of the viewing window.					 */

#ifdef TCL_WRAPPER

   /* Set up fundamentally necessary colors black and white */

   addnewcolorentry(getnamedcolor("Black"));
   addnewcolorentry(getnamedcolor("White"));

   /* Set up new Tcl type "handle" for element handles */

   Tcl_RegisterObjType(&tclHandleType);

#endif

   /*-----------------------------------------------------*/
   /* Set the cursor as a crosshair for the area widget.  */
   /*-----------------------------------------------------*/

   XDefineCursor (areawin->viewport, DEFAULTCURSOR);

   /*---------------------------------------------------*/
   /* Set up a timeout for automatic save to a tempfile */
   /*---------------------------------------------------*/

   xobjs.save_interval = appdata.timeout;
   xobjs.timeout_id = xcAddTimeout(60000 * xobjs.save_interval,
	savetemp, NULL);

   setsymschem();
}

/*----------------------------------------------------------------------*/

QAction* menuAction(const char *m) {
    QAction* a = areawin->menubar->findChild<QAction*>(m);
    if (!a) {
        QMenu* menu = areawin->menubar->findChild<QMenu*>(m);
        a = menu->menuAction();
    }
    Q_ASSERT(a != NULL);
    return a;
}

void XCWindowData::update()
{
    if (! updates) viewport->update();
    updates ++;
}

void XCWindowData::markUpdated()
{
    if (false && updates > 1) qDebug("XCWindowData: update() was called %d times this cycle", updates);
    updates = 0;
}

short  XCWindowData::width() const
{
    return viewport->width();
}

short XCWindowData::height() const
{
    return viewport->height();
}
