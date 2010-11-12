/*-------------------------------------------------------------------------*/
/* events.c --- xcircuit routines handling Xevents and Callbacks	   */
/* Copyright (c) 2002  Tim Edwards, Johns Hopkins University        	   */
/*-------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------*/
/*      written by Tim Edwards, 8/13/93    				   */
/*-------------------------------------------------------------------------*/

#include <QPainter>
#include <QAction>
#include <QEvent>
#include <QScrollBar>
#include <QResizeEvent>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cctype>

#ifdef TCL_WRAPPER
#define  XK_MISCELLANY
#define  XK_LATIN1
#endif

#ifdef TCL_WRAPPER 
#include <tk.h>
#endif

#include "xcircuit.h"
#include "matrix.h"
#include "colors.h"
#include "prototypes.h"
#include "xcqt.h"
#include "area.h"
#include "context.h"

/*-------------------------------------------------------------------------*/
/* Global Variable definitions						   */
/*-------------------------------------------------------------------------*/

extern XtAppContext app;
extern Cursor	appcursors[NUM_CURSORS];
extern XCWindowData *areawin;
extern ApplicationData appdata;
extern short popups;
extern int pressmode;
extern char  _STR[150];
extern short beeper;
extern double saveratio;
extern u_char texttype;
extern aliasptr aliastop;

#ifdef TCL_WRAPPER
extern Tcl_Interp *xcinterp;
#else
extern short help_up;
#endif

#ifdef OPENGL
extern GLXContext grXcontext;
#endif

bool was_preselected;

/*----------------------------------------------------------------------------*/
/* Edit Object pushing and popping.					      */
/*----------------------------------------------------------------------------*/

bool recursefind(objectptr parent, objectptr suspect)
{
   if (parent == suspect) return true;

   for (objinstiter shell; parent->values(shell); )
         if (recursefind(shell->thisobject, suspect)) return true;

   return false;
}

/*--------------------------------------------------------------*/
/* Transfer objects in the select list to the current object	*/
/* (but disallow infinitely recursive loops!)			*/
/*--------------------------------------------------------------*/
/* IMPORTANT:  delete_for_xfer() MUST be executed prior to	*/
/* calling transferselects(), so that the deleted elements are	*/
/* in an object saved in areawin->editstack.			*/
/*--------------------------------------------------------------*/

void transferselects()
{
   short locselects;
   objinstptr tobj;
   XPoint newpos;

   if (areawin->editstack->parts == 0) return;

   if (eventmode == MOVE_MODE || eventmode == COPY_MODE || 
		eventmode == UNDO_MODE || eventmode == CATMOVE_MODE) {
      short ps = topobject->parts;

      freeselects();

      locselects = areawin->editstack->parts;
      areawin->selectlist = xc_undelete(areawin->topinstance,
                areawin->editstack, (short *)NULL);
      areawin->selects = locselects;

      /* Move all selected items to the cursor position	*/
      newpos = UGetCursor();
      drag((int)newpos.x, (int)newpos.y);

      /* check to make sure this object is not the current object	*/
      /* or one of its direct ancestors, else an infinite loop results. */

      for (objinstiter tobj; topobject->values(tobj); ) {
	    if (recursefind(tobj->thisobject, topobject)) {
	       Wprintf("Attempt to place object inside of itself");
               delete_noundo();
	       break;
	    }
      }
   }
}

/*-------------------------------------------------------*/
/* set the viewscale variable to the proper address	 */
/*-------------------------------------------------------*/

void setpage(bool killselects)
{
   areawin->vscale = topobject->viewscale;
   areawin->pcorner = topobject->pcorner;

   if (killselects) clearselects();

#ifdef TCL_WRAPPER
   if (xobjs.suspend < 0)
      XcInternalTagCall(xcinterp, 2, "page", "goto");
#endif
}

/*-------------------------------------------------------*/
/* switch to a new page					 */
/*-------------------------------------------------------*/

int changepage(short pagenumber)
{
   short npage;
   objectptr pageobj;
   u_char undo_type;

   /* to add to existing number of top level pages. . . */

   if (pagenumber == 255) {
      if (xobjs.pages == 255) {
	 Wprintf("Out of available pages!");
	 return -1;
      }
      else pagenumber = xobjs.pages;
   }

   if (pagenumber >= xobjs.pages) {

      Q_ASSERT(pagenumber == xobjs.pages);
      xobjs.pagelist.append(Pagelist());
      xobjs.pagelist[pagenumber].filename.clear();
      xobjs.pagelist[pagenumber].background.name.clear();
      xobjs.pagelist[pagenumber].pageinst = NULL;

      makepagebutton();
   }

   if (eventmode == MOVE_MODE || eventmode == COPY_MODE || eventmode == UNDO_MODE) {
      delete_for_xfer(areawin->selectlist, areawin->selects);
      undo_type = UNDO_MORE;
   }
   else {
      clearselects();
      undo_type = UNDO_DONE;
   }
   if (areawin->page != pagenumber)
      register_for_undo(XCF_Page, undo_type, areawin->topinstance,
	  areawin->page, pagenumber);

   if (eventmode != ASSOC_MODE) {
      areawin->page = pagenumber;
      free_stack(&areawin->stack);
   }
   if (xobjs.pagelist[pagenumber].pageinst == NULL) {

      /* initialize a new page */

      pageobj = new object;
      sprintf(pageobj->name, "Page %d", pagenumber + 1);

      xobjs.pagelist[pagenumber].pageinst = new objinst(pageobj);
      xobjs.pagelist[pagenumber].filename.clear();
      xobjs.pagelist[pagenumber].background.name.clear();

      pagereset(pagenumber);
   }

   /* Write back the current view parameters */
   if (areawin->topinstance != NULL) {
      topobject->viewscale = areawin->vscale;
      topobject->pcorner = areawin->pcorner;
   }

   areawin->topinstance = xobjs.pagelist[pagenumber].pageinst;

   setpage(true);

   return 0;
}

/*-------------------------------------------------------*/
/* switch to a new page and redisplay			 */
/*-------------------------------------------------------*/

void newpage(short pagenumber)
{
   switch (eventmode) {
      case CATALOG_MODE:
         eventmode = NORMAL_MODE;
	 catreturn();
	 break;

      case NORMAL_MODE: case COPY_MODE: case MOVE_MODE: case UNDO_MODE:
	 if (changepage(pagenumber) >= 0) {
	    transferselects();
	    renderbackground();
	    refresh(NULL, NULL, NULL);

            togglegrid((u_short)xobjs.pagelist[areawin->page].coordstyle);
	    setsymschem();
	 }
	 break;

      default:
         Wprintf("Cannot switch pages from this mode");
	 break;
   }
}

/*---------------------------------------*/
/* Stack structure push and pop routines */
/*---------------------------------------*/

void push_stack(pushlistptr *stackroot, objinstptr thisinst)
{
   pushlistptr newpush;

   newpush = new pushlist;
   newpush->next = *stackroot;
   newpush->thisinst = thisinst;
   *stackroot = newpush;
}

/*----------------------------------------------------------*/

void pop_stack(pushlistptr *stackroot)
{
   pushlistptr lastpush;

   if (!(*stackroot)) {
      Fprintf(stderr, "pop_genstack() Error: NULL instance stack!\n");
      return;
   }

   lastpush = (*stackroot)->next;
   free(*stackroot);
   *stackroot = lastpush;
}

/*----------------------------------------------------------*/

void free_stack(pushlistptr *stackroot)
{
    if (stackroot)
        while (*stackroot)
            pop_stack(stackroot);
}

/*------------------------------------------*/
/* Push object onto hierarchy stack to edit */
/*------------------------------------------*/

void pushobject(objinstptr thisinst)
{
  short *selectobj, *savelist;
   int saves;
   u_char undo_type = UNDO_DONE;
   objinstptr pushinst = thisinst;

   savelist = NULL;
   saves = 0;
   if (eventmode == MOVE_MODE || eventmode == COPY_MODE) {
      savelist = areawin->selectlist;
      saves = areawin->selects;
      areawin->selectlist = NULL;
      areawin->selects = 0;
      undo_type = UNDO_MORE;
   }

   if (pushinst == NULL) {
      selectobj = areawin->selectlist;
      if (areawin->selects == 0) {
         disable_selects(topobject, savelist, saves);
	 selectobj = select_element(OBJINST);
         enable_selects(topobject, savelist, saves);
      }
      if (areawin->selects == 0) {
         Wprintf("No objects selected.");
         return;
      }
      else if (areawin->selects > 1) {
         Wprintf("Choose only one object.");
         return;
      }
      else if (SELECTTYPE(selectobj) != OBJINST) {
         Wprintf("Element to push must be an object.");
         return;
      }
      else pushinst = SELTOOBJINST(selectobj);
   }

   if (savelist != NULL) {
      delete_for_xfer(savelist, saves);
      free(savelist);
   }

   register_for_undo(XCF_Push, undo_type, areawin->topinstance, pushinst);

   /* save the address of the current object to the push stack */

   push_stack(&areawin->stack, areawin->topinstance);

   topobject->viewscale = areawin->vscale;
   topobject->pcorner = areawin->pcorner;
   areawin->topinstance = pushinst;

   /* move selected items to the new object */

   setpage(true);
   transferselects();
   refresh(NULL, NULL, NULL);
   setsymschem();
}

/*--------------------------*/
/* Pop edit hierarchy stack */
/*--------------------------*/

void popobject(QAction* w, pointertype no_undo, void* calldata)
{
   u_char undo_type = UNDO_DONE;

   if (areawin->stack == NULL || (eventmode != NORMAL_MODE && eventmode != MOVE_MODE
	&& eventmode != COPY_MODE && eventmode != FONTCAT_MODE &&
	eventmode != ASSOC_MODE && eventmode != UNDO_MODE &&
	eventmode != EFONTCAT_MODE)) return;

   if ((eventmode == MOVE_MODE || eventmode == COPY_MODE || eventmode == UNDO_MODE)
	&& ((areawin->stack->thisinst == xobjs.libtop[LIBRARY]) ||
       (areawin->stack->thisinst == xobjs.libtop[USERLIB]))) return;

   /* remove any selected items from the current object */

   if (eventmode == MOVE_MODE || eventmode == COPY_MODE || eventmode == UNDO_MODE) {
      undo_type = UNDO_MORE;
      delete_for_xfer(areawin->selectlist, areawin->selects);
   }
   else if (eventmode != FONTCAT_MODE && eventmode != EFONTCAT_MODE)
      unselect_all();

   /* If coming from the library, don't register an undo action, because */
   /* it has already been registered as type XCF_Library_Pop.		 */

   if (no_undo == (pointertype)0)
      register_for_undo(XCF_Pop, undo_type, areawin->topinstance);

   topobject->viewscale = areawin->vscale;
   topobject->pcorner = areawin->pcorner;
   areawin->topinstance = areawin->stack->thisinst;
   pop_stack(&areawin->stack);

   /* if new object is a library or PAGELIB, put back into CATALOG_MODE */

   if (is_library(topobject) >= 0) eventmode = CATALOG_MODE;

   /* move selected items to the new object */

   if (eventmode == FONTCAT_MODE || eventmode == EFONTCAT_MODE)
      setpage(false);
   else {
      setpage(true);
      setsymschem();
      if (eventmode != ASSOC_MODE)
         transferselects();
   }
   refresh(NULL, NULL, NULL);
}

/*-------------------------------------------------------------------------*/
/* Destructive reset of entire object		 			   */
/*-------------------------------------------------------------------------*/

void resetbutton(QAction* button, pointertype pageno_, void* calldata)
{
   short page;
   unsigned int pageno = (unsigned int)pageno_;
   objectptr pageobj;
   objinstptr pageinst;

   if (eventmode != NORMAL_MODE) return;
 
   page = (pageno == 0) ? areawin->page : (short)(pageno - 1);

   pageinst = xobjs.pagelist[page].pageinst;

   if (pageinst == NULL) return; /* page already cleared */

   pageobj = pageinst->thisobject;

   /* Make sure this is a real top-level page */

   if (is_page(topobject) < 0) {
      if (pageno == 0) {
	 Wprintf("Can only clear top-level pages!");
	 return;
      }
      else {
	 /* Make sure that we're not in the hierarchy of the page being deleted */
	 pushlistptr slist;
	 for (slist = areawin->stack; slist != NULL; slist = slist->next)
	    if (slist->thisinst->thisobject == pageobj) {
	       Wprintf("Can't delete the page while you're in its hierarchy!");
	       return;
	    }
      }
   }

   /* Watch for pages which are linked by schematic/symbol. */
   
   if (pageobj->symschem != NULL) {
      Wprintf("Schematic association to object %s", pageobj->symschem->name);
      return;
   }

   sprintf(pageobj->name, "Page %d", page + 1);
   xobjs.pagelist[page].filename = QString::fromLocal8Bit(pageobj->name);
   pageobj->clear();
   flush_undo_stack();

   if (page == areawin->page) {
      areawin->update();
      printname(pageobj);
      renamepage(page);
      Wprintf("Page cleared.");
   }
}

/*------------------------------------------------------*/
/* Pan the screen to follow the cursor position 	*/
/*------------------------------------------------------*/

void trackpan(int x, int y)
{
   long newpx, newpy;
   XPoint newpos;
   short savey = areawin->pcorner.y;
   short savex = areawin->pcorner.x;

   newpos.x = areawin->origin.x - x;
   newpos.y = y - areawin->origin.y;

   areawin->pcorner.x += newpos.x / areawin->vscale;
   areawin->pcorner.y += newpos.y / areawin->vscale;

   /// \todo
#if 0
   areawin->scrollbarh->update();
   areawin->scrollbarv->update();
#endif

#if 0
   areawin->pcorner.x = savex;
   areawin->pcorner.y = savey;
#endif
}

/*--------------------------------------------------------------------*/
/* Zoom functions-- zoom box, zoom in, zoom out, and pan.	      */
/*--------------------------------------------------------------------*/

void postzoom()
{
   W3printf(" ");
   areawin->lastbackground = NULL;
   renderbackground();
}

/*--------------------------------------------------------------------*/

void zoominbox(int x, int y)
{
   float savescale;
   float delxscale, delyscale;
   XPoint savell;

   savescale = areawin->vscale;
   savell = areawin->pcorner;

   /* zoom-box function: corners are in areawin->save and areawin->origin */
   /* select box has lower-left corner in .origin, upper-right in .save */
   /* ignore if zoom box is size zero */

   if (areawin->save.x == areawin->origin.x || areawin->save.y == areawin->origin.y) {
      Wprintf("Zoom box of size zero: Ignoring.");
      eventmode = NORMAL_MODE;
      return;
   }

   /* determine whether x or y is limiting factor in zoom */
   delxscale = (areawin->width / areawin->vscale) /
	   abs(areawin->save.x - areawin->origin.x);
   delyscale = (areawin->height / areawin->vscale) /
	   abs(areawin->save.y - areawin->origin.y);
   areawin->vscale *= qMin(delxscale, delyscale);

   areawin->pcorner.x = qMin(areawin->origin.x, areawin->save.x) -
	 (areawin->width / areawin->vscale - 
	 abs(areawin->save.x - areawin->origin.x)) / 2;
   areawin->pcorner.y = qMin(areawin->origin.y, areawin->save.y) -
	 (areawin->height / areawin->vscale - 
	 abs(areawin->save.y - areawin->origin.y)) / 2;
   eventmode = NORMAL_MODE;

   /* check for minimum scale */

   Context ctx(NULL);
   if (checkbounds(&ctx) == -1) {
      areawin->pcorner = savell;
      areawin->vscale = savescale;
      Wprintf("At minimum scale: cannot scale further");

      /* this is a rare case where an object gets out-of-bounds */

      Context ctx(NULL);
      if (checkbounds(&ctx) == -1) {
         if (beeper) XBell(100);
	 Wprintf("Unable to scale: Delete out-of-bounds object!");
      }
      return;
   }
   postzoom();
}

/*--------------------------------------------------------------------*/

void zoomin_call(QAction*, void*, void*)
{
    areawin->area->zoomin();
}

void zoominrefresh(int x, int y)
{
    if (eventmode == SELAREA_MODE) {
        zoominbox(x, y);
        refresh(NULL, NULL, NULL);
    } else {
        areawin->area->zoomin();
    }
}

/*--------------------------------------------------------------------*/

void zoomoutbox(int x, int y)
{
   float savescale;
   float delxscale, delyscale, scalefac;
   XPoint savell;
   XlPoint newll;

   savescale = areawin->vscale;
   savell = areawin->pcorner;

   /* zoom-box function, analogous to that for zoom-in */
   /* ignore if zoom box is size zero */

   if (areawin->save.x == areawin->origin.x || areawin->save.y == areawin->origin.y) {
      Wprintf("Zoom box of size zero: Ignoring.");
      eventmode = NORMAL_MODE;
      return;
   }

   /* determine whether x or y is limiting factor in zoom */
   delxscale = abs(areawin->save.x - areawin->origin.x) /
	   (areawin->width / areawin->vscale);
   delyscale = abs(areawin->save.y - areawin->origin.y) /
	   (areawin->height / areawin->vscale);
   scalefac = qMin(delxscale, delyscale);
   areawin->vscale *= scalefac;

   /* compute lower-left corner of (reshaped) select box */
   if (delxscale < delyscale) {
      newll.y = qMin(areawin->save.y, areawin->origin.y);
      newll.x = (areawin->save.x + areawin->origin.x
		- (abs(areawin->save.y - areawin->origin.y) *
		areawin->width / areawin->height)) / 2;
   }
   else {
      newll.x = qMin(areawin->save.x, areawin->origin.x);
      newll.y = (areawin->save.y + areawin->origin.y
		- (abs(areawin->save.x - areawin->origin.x) *
		areawin->height / areawin->width)) / 2;
   }

   /* extrapolate to find new lower-left corner of screen */
   newll.x = areawin->pcorner.x - (int)((float)(newll.x -
		areawin->pcorner.x) / scalefac);
   newll.y = areawin->pcorner.y - (int)((float)(newll.y -
		areawin->pcorner.y) / scalefac);

   eventmode = NORMAL_MODE;
   areawin->pcorner = newll;

   Context ctx(NULL);
   if ((newll.x << 1) != (long)(areawin->pcorner.x << 1) || (newll.y << 1)
         != (long)(areawin->pcorner.y << 1) || checkbounds(&ctx) == -1) {
      areawin->vscale = savescale; 
      areawin->pcorner = savell;
      Wprintf("At maximum scale: cannot scale further.");
      return;
   }
   postzoom();
}

/*--------------------------------------------------------------------*/

void zoomout_call(QAction*, void*, void*)
{
    areawin->area->zoomout();
}

void zoomoutrefresh(int x, int y)
{
    if (eventmode == SELAREA_MODE) {
        zoomoutbox(x, y);
        refresh(NULL, NULL, NULL);
    } else {
        areawin->area->zoomout();
    }
}

void warppointer(int x, int y)
{
    QCursor::setPos(areawin->viewport->mapToGlobal(QPoint(x,y)));
}

/*--------------------------------------------------------------*/
/* ButtonPress handler during center pan 			*/
/* x and y are cursor coordinates.				*/
/* If ptype is 1-4 (directional), then "value" is a fraction of	*/
/* 	the screen to scroll.					*/
/*--------------------------------------------------------------*/

void panbutton(u_int ptype, int x, int y, float value)
{
   int  xpos, ypos, newllx, newlly;
   XPoint savell;
   Dimension hwidth = areawin->width >> 1, hheight = areawin->height >> 1;

   savell = areawin->pcorner;

   switch(ptype) {
      case 1:
         xpos = hwidth - (hwidth * 2 * value);
         ypos = hheight;
	 break;
      case 2:
         xpos = hwidth + (hwidth * 2 * value);
         ypos = hheight;
	 break;
      case 3:
         xpos = hwidth;
         ypos = hheight - (hheight * 2 * value);
	 break;
      case 4:
         xpos = hwidth;
         ypos = hheight + (hheight * 2 * value);
	 break;
      case 5:
         xpos = x;
         ypos = y;
	 break;
      case 6:	/* "pan follow" */
	 if (eventmode == PAN_MODE)
	    finish_op(XCF_Finish, x, y);
	 else if (eventmode == NORMAL_MODE) {
	    eventmode = PAN_MODE;
	    areawin->save.x = x;
	    areawin->save.y = y;
            u2u_snap(areawin->save);
	    areawin->origin = areawin->save;
            xcAddEventHandler(areawin->viewport, PointerMotionMask, false,
			(XtEventHandler)xlib_drag, NULL);
	 }
	 return;
	 break;
      default:	/* "pan here" */
	 xpos = x;
	 ypos = y;
         warppointer(hwidth, hheight);
	 break;
   }

   xpos -= hwidth;
   ypos = hheight - ypos;

   newllx = (int)areawin->pcorner.x + (int)((float)xpos / areawin->vscale);
   newlly = (int)areawin->pcorner.y + (int)((float)ypos / areawin->vscale);

   areawin->pcorner.x = (short) newllx;
   areawin->pcorner.y = (short) newlly;

   Context ctx(NULL);
   if ((newllx << 1) != (long)(areawin->pcorner.x << 1) || (newlly << 1)
           != (long)(areawin->pcorner.y << 1) || checkbounds(&ctx) == -1) {
      areawin->pcorner = savell;
      Wprintf("Reached bounds:  cannot pan further.");
      return;
   }
   else if (eventmode == MOVE_MODE || eventmode == COPY_MODE ||
		eventmode == CATMOVE_MODE)
      drag(x, y);

   postzoom();
}

/*--------------------------------------------------------------*/

void panrefresh(u_int ptype, int x, int y, float value)
{
   panbutton(ptype, x, y, value);
   refresh(NULL, NULL, NULL);
}

/*----------------------------------------------------------------*/
/* Check for out-of-bounds before warping pointer, and pan window */
/* if necessary.                                                  */
/*----------------------------------------------------------------*/

void checkwarp(XPoint *userpt)
{
  XPoint wpoint;

  user_to_window(*userpt, &wpoint);

  if (wpoint.x < 0 || wpoint.y < 0 || wpoint.x > areawin->width ||
        wpoint.y > areawin->height) {
     panrefresh(5, wpoint.x, wpoint.y, 0); 
     wpoint.x = areawin->width >> 1;
     wpoint.y = areawin->height >> 1;
     /* snap(wpoint.x, wpoint.y, userpt); */
  }
  warppointer(wpoint.x, wpoint.y);
}

/*--------------------------------------------------------------*/
/* Return a pointer to the element containing a reference point	*/
/*--------------------------------------------------------------*/

genericptr getsubpart(pathptr editpath, int *idx)
{
   pointselect *tmpptr = NULL;
   genericptr *pgen;

   if (idx) *idx = 0;

   for (pgen = editpath->begin(); pgen != editpath->end(); pgen++) {
      switch (ELEMENTTYPE(*pgen)) {
         case POLYGON:
            if (TOPOLY(pgen)->cycle != NULL) {
               for (tmpptr = TOPOLY(pgen)->cycle;; tmpptr++) {
                  if (tmpptr->flags & REFERENCE) break;
                  if (tmpptr->flags & LASTENTRY) break;
               }
               if (tmpptr->flags & REFERENCE) return *pgen;
            }
            break;
         case SPLINE:
            if (TOSPLINE(pgen)->cycle != NULL) {
               for (tmpptr = TOSPLINE(pgen)->cycle;; tmpptr++) {
                  if (tmpptr->flags & REFERENCE) break;
                  if (tmpptr->flags & LASTENTRY) break;
               }
               if (tmpptr->flags & REFERENCE) return *pgen;
            }
            break;
      }
      if (idx) *idx++;
   }
   return NULL;
}

/*--------------------------------------------------------------*/
/* Return a pointer to the current reference point of an	*/
/* edited element (polygon, spline, or path)			*/
/*--------------------------------------------------------------*/

pointselect *getrefpoint(genericptr genptr, XPoint **refpt)
{
   pointselect *tmpptr = NULL;
   genericptr *pgen;

   if (refpt) *refpt = NULL;
   switch (genptr->type) {
      case POLYGON:
         if (((polyptr)genptr)->cycle != NULL) {
            for (tmpptr = ((polyptr)genptr)->cycle;; tmpptr++) {
               if (tmpptr->flags & REFERENCE) break;
               if (tmpptr->flags & LASTENTRY) break;
            }
            if (!(tmpptr->flags & REFERENCE)) tmpptr = NULL;
            else if (refpt) *refpt = ((polyptr)genptr)->points + tmpptr->number;
         }
         break;
      case SPLINE:
         if (((splineptr)genptr)->cycle != NULL) {
            for (tmpptr = ((splineptr)genptr)->cycle;; tmpptr++) {
               if (tmpptr->flags & REFERENCE) break;
               if (tmpptr->flags & LASTENTRY) break;
            }
            if (!(tmpptr->flags & REFERENCE)) tmpptr = NULL;
            else if (refpt) *refpt = &((splineptr)genptr)->ctrl[tmpptr->number];
         }
         break;
      case PATH:
         for (pgen = ((pathptr)genptr)->begin(); pgen != ((pathptr)genptr)->end(); pgen++) {
            if ((tmpptr = getrefpoint(*pgen, refpt)) != NULL)
               return tmpptr;
         }
         break;
      default:
         tmpptr = NULL;
         break;
   }
   return tmpptr;
}

/*--------------------------------------------------------------*/
/* Return next edit point on a polygon, arc, or spline.  Do not	*/
/* update the cycle of the element.				*/
/*--------------------------------------------------------------*/

int checkcycle(genericptr genptr, short dir)
{
   pointselect *tmpptr;
   short tmppt, points;
   genericptr *pgen;

   switch (genptr->type) {
      case POLYGON:
         if (((polyptr)genptr)->cycle == NULL)
            tmpptr = NULL;
         else {
            for (tmpptr = ((polyptr)genptr)->cycle;; tmpptr++) {
               if (tmpptr->flags & REFERENCE) break;
               if (tmpptr->flags & LASTENTRY) break;
            }
            if (!(tmpptr->flags & REFERENCE)) tmpptr = ((polyptr)genptr)->cycle;
         }
         tmppt = (tmpptr == NULL) ? -1 : tmpptr->number;
         points = ((polyptr)genptr)->points.count();
         break;
      case SPLINE:
         if (((splineptr)genptr)->cycle == NULL)
            tmpptr = NULL;
         else {
            for (tmpptr = ((splineptr)genptr)->cycle;; tmpptr++) {
               if (tmpptr->flags & REFERENCE) break;
               if (tmpptr->flags & LASTENTRY) break;
            }
            if (!(tmpptr->flags & REFERENCE)) tmpptr = ((splineptr)genptr)->cycle;
         }
         tmppt = (tmpptr == NULL) ? -1 : tmpptr->number;
         points = 4;
         break;
      case ARC:
         tmpptr = ((arcptr)genptr)->cycle;
         tmppt = (tmpptr == NULL) ? -1 : tmpptr->number;
         points = 4;
         break;
      case PATH:
         for (pgen = ((pathptr)genptr)->begin(); pgen < ((pathptr)genptr)->end(); pgen++) {
            if ((tmppt = checkcycle(*pgen, dir)) >= 0)
               return tmppt;
         }
         break;
      default:
         tmppt = -1;
         break;
   }
   if (tmppt >= 0) {	/* Ignore nonexistent cycles */
      tmppt += dir;
      if (tmppt < 0) tmppt += points;
      tmppt %= points;
   }
   return tmppt;
}

/*--------------------------------------------------------------*/
/* Change to the next part of a path for editing		*/
/* For now, |dir| is treated as 1 regardless of its value.	*/
/*--------------------------------------------------------------*/

void nextpathcycle(pathptr nextpath, short dir)
{
   genericptr ppart = getsubpart(nextpath, NULL);
   genericptr *ggen;
   XPoint *curpt;
   polyptr thispoly;
   splineptr thisspline;
   pointselect *cptr;
   short cycle, newcycle;

   /* Simple cases---don't need to switch elements */

   switch (ELEMENTTYPE(ppart)) {
      case POLYGON:
	 thispoly = (polyptr)ppart;
         cptr = thispoly->cycle;
         if (cptr == NULL) return;
         curpt = thispoly->points + cptr->number;
         newcycle = checkcycle(ppart, dir);
         advancecycle(&ppart, newcycle);
         if (cptr->number < thispoly->points.count() && cptr->number > 0) {
            checkwarp(thispoly->points.begin() + cptr->number);
            updatepath(nextpath);
	    return;
	 }
	 break;
      case SPLINE:
	 thisspline = (splineptr)ppart;
         cptr = ((splineptr)ppart)->cycle;
         if (cptr == NULL) return;
         curpt = &thisspline->ctrl[cptr->number];
         newcycle = checkcycle(ppart, dir);
         advancecycle(&ppart, newcycle);
         if (cptr->number < 4 && cptr->number > 0) {
            checkwarp(&thisspline->ctrl[cptr->number]);
            updatepath(nextpath);
	    return;
	 }
	 break;
   }

   /* If dir < 0, go to the penultimate cycle of the last part	*/
   /* If dir > 0, go to the second cycle of the next part 	*/

   for (ggen = nextpath->begin(); (*ggen != ppart) &&
                (ggen < nextpath->end()); ggen++) ;

   if (ggen == nextpath->end()) return;  /* shouldn't happen! */

   if (dir > 0)
      ggen++;
   else
      ggen--;

   if (ggen < nextpath->begin())
      ggen = nextpath->end() - 1;
   else if (ggen == nextpath->end())
      ggen = nextpath->begin();

   removecycle(nextpath);

   /* The next point to edit is the first point in the next segment	*/
   /* that is not at the same position as the one we were last editing.	*/

   switch (ELEMENTTYPE(*ggen)) {
      case POLYGON:
	 thispoly = TOPOLY(ggen);
         cycle = (dir > 0) ? 0 : thispoly->points.count() - 1;
         addcycle(ggen, cycle, 0);
         makerefcycle(thispoly->cycle, cycle);
         if (thispoly->points[cycle] == *curpt) {
             newcycle = checkcycle(thispoly, 1);
             advancecycle(ggen, newcycle);
             cycle = newcycle;
         }
         checkwarp(thispoly->points + cycle);
	 break;
      case SPLINE:
	 thisspline = TOSPLINE(ggen);
         cycle = (dir > 0) ? 0 : 3;
         addcycle(ggen, cycle, 0);
         makerefcycle(thisspline->cycle, cycle);
         if (thisspline->ctrl[cycle] == *curpt) {
             newcycle = checkcycle(thisspline, 1);
             advancecycle(ggen, newcycle);
             cycle = newcycle;
         }
         checkwarp(&(thisspline->ctrl[cycle]));
	 break;
   }
   updatepath(nextpath);
}

/*--------------------------------------------------------------*/
/* Change to next edit point on a polygon			*/
/*--------------------------------------------------------------*/

void nextpolycycle(polyptr *nextpoly, short dir)
{
    short newcycle;

    newcycle = checkcycle(*nextpoly, dir);
    advancecycle((genericptr *)nextpoly, newcycle);
    findconstrained(*nextpoly);
    printeditbindings();

    newcycle = (*nextpoly)->cycle->number;
    checkwarp((*nextpoly)->points + newcycle);
}

/*--------------------------------------------------------------*/
/* Change to next edit cycle on a spline			*/
/*--------------------------------------------------------------*/

void nextsplinecycle(splineptr *nextspline, short dir)
{
    short newcycle;
    newcycle = checkcycle(*nextspline, dir);
    advancecycle((genericptr *)nextspline, newcycle);

    if (newcycle == 1 || newcycle == 2)
       Wprintf("Adjust control point");
    else
       Wprintf("Adjust endpoint position");

    checkwarp(&(*nextspline)->ctrl[newcycle]);
}

/*--------------------------------------------------------------*/
/* Warp pointer to the edit point on an arc.			*/
/*--------------------------------------------------------------*/

void warparccycle(arcptr nextarc, short cycle)
{
   XPoint curang = nextarc->position;
   double rad;

   switch(cycle) {
      case 0:
         curang.x += abs(nextarc->radius);
	 if (abs(nextarc->radius) != nextarc->yaxis)
	    Wprintf("Adjust ellipse size");
	 else
	    Wprintf("Adjust arc radius");
	 break;
      case 1:
         rad = (double)(nextarc->angle1 * RADFAC);
         curang.x += abs(nextarc->radius) * cos(rad);
         curang.y += nextarc->yaxis * sin(rad);
	 Wprintf("Adjust arc endpoint");
  	 break;
      case 2:
         rad = (double)(nextarc->angle2 * RADFAC);
         curang.x += abs(nextarc->radius) * cos(rad);
         curang.y += nextarc->yaxis * sin(rad);
	 Wprintf("Adjust arc endpoint");
	 break;
      case 3:
         curang.y += nextarc->yaxis;
	 Wprintf("Adjust ellipse minor axis");
	 break;
   }
   checkwarp(&curang);
}

/*--------------------------------------------------------------*/
/* Change to next edit cycle on an arc				*/
/*--------------------------------------------------------------*/

void nextarccycle(arcptr *nextarc, short dir)
{
    short newcycle;

    newcycle = checkcycle(*nextarc, dir);
    advancecycle((genericptr*)nextarc, newcycle);
    warparccycle(*nextarc, newcycle);
}

/*------------------------------------------------------*/
/* Get a numerical response from the keyboard (0-9)     */
/*------------------------------------------------------*/

#ifndef TCL_WRAPPER

short getkeynum()
{
    /// \todo - we have to wait for a key event, may be done by installing a global event handler
#if 0
   QEvent* event;
   XKeyEvent *keyevent = (XKeyEvent *)(&event);
   KeySym keypressed;

   for (;;) {
      if (event.type == QEvent::KeyPress) break;
      else XtDispatchEvent(&event);
   }
   XLookupString(keyevent, _STR, 150, &keypressed, NULL);
   if (keypressed > XK_0 && keypressed <= XK_9)
      return (short)(keypressed - XK_1);
   else
      return -1;
#endif
   return -1;
}

#endif

/*--------------------------*/
/* Register a "press" event */
/*--------------------------*/

#ifdef TCL_WRAPPER
void makepress(ClientData clientdata)
#else
void makepress(XtPointer clientdata, XtIntervalId *id) 
#endif
{
   int keywstate = (int)((pointertype)clientdata);

   /* Button/Key was pressed long enough to make a "press", not a "tap" */
   areawin->time_id = 0;
   pressmode = keywstate;
   eventdispatch(keywstate | HOLD, areawin->save.x, areawin->save.y);
}

/*------------------------------------------------------*/
/* Handle button events as if they were keyboard events */
/* This is only done on the area window                 */
/*------------------------------------------------------*/

XKeyEvent::XKeyEvent(const QKeyEvent & kev) :
        QKeyEvent(kev.type(), kev.key(), kev.modifiers(), kev.text(), kev.isAutoRepeat(), kev.count())
{
}

XKeyEvent::XKeyEvent(const QMouseEvent & mev) :
        QKeyEvent(mev.type(), 0, mev.modifiers())
{
    switch (mev.button()) {
    case Qt::LeftButton:
       k = Key_Button1;
       break;
    case Qt::RightButton:
       k = Key_Button2;
       break;
    case Qt::MidButton:
       k = Key_Button3;
       break;
    }
    if (mev.type() == QEvent::MouseButtonPress) {
        t = QEvent::KeyPress;
    }
    else if (mev.type() == QEvent::MouseButtonRelease) {
        t = QEvent::KeyRelease;
    }
    p = mev.pos();
}

/*--------------------------------------------------------------*/
/* Edit operations specific to polygons (point manipulation)	*/
/*--------------------------------------------------------------*/

void poly_edit_op(int op)
{
   genericptr keygen = *(EDITPART);
   polyptr lwire;
   short cycle;

   if (IS_PATH(keygen))
      keygen = getsubpart((pathptr)keygen, NULL);

   switch(ELEMENTTYPE(keygen)) {
      case POLYGON: {
   	 lwire = (polyptr)keygen;

         /* Remove a point from the polygon */
	 if (op == XCF_Edit_Delete) {
            if (lwire->points.count() < 3) return;
            if (lwire->points.count() == 3 && !(lwire->style & UNCLOSED))
	       lwire->style |= UNCLOSED;
            cycle = checkcycle(lwire, 0);
            lwire->points.remove(cycle);
            areawin->update();
            nextpolycycle(&lwire, -1);
         }

         /* Add a point to the polygon */
         else if (op == XCF_Edit_Insert || op == XCF_Edit_Append) {
            cycle = checkcycle(lwire, 0);
            lwire->points.insert(cycle, lwire->points[cycle-1]);
            areawin->update();
	    if (op == XCF_Edit_Append)
               nextpolycycle(&lwire, 1);
         }

	 /* Parameterize the position of a polygon point */
	 else if (op == XCF_Edit_Param) {
             cycle = checkcycle(lwire, 0);
             makenumericalp(&keygen, P_POSITION_X, NULL, cycle);
             makenumericalp(&keygen, P_POSITION_Y, NULL, cycle);
	 }
      }
   }
}

/*----------------------------------------------------------------------*/
/* Handle attachment of edited elements to nearby elements		*/
/*----------------------------------------------------------------------*/

void attach_to()
{
   /* Conditions: One element is selected, key "A" is pressed.	*/
   /* Then there must exist a spline, polygon, arc, or label	*/
   /* to attach to.						*/

   if (areawin->selects <= 1) {
      short *refsel;

      if (areawin->attachto >= 0) {
	 areawin->attachto = -1;	/* default value---no attachments */
	 Wprintf("Unconstrained moving");
      }
      else {
	 int select_prev;

	 select_prev = areawin->selects;
	 refsel = select_add_element(SPLINE|ARC|POLYGON|LABEL|OBJINST);
	 if ((refsel != NULL) && (areawin->selects > select_prev)) {

	    /* transfer refsel over to attachto */

	    areawin->attachto = *(refsel + areawin->selects - 1);
	    areawin->selects--;
	    if (areawin->selects == 0) freeselects();

	    Wprintf("Constrained attach");

	    /* Starting a new wire? */
	    if (eventmode == NORMAL_MODE) {
	       XPoint newpos, userpt;
	       userpt = UGetCursorPos();
	       findattach(&newpos, NULL, &userpt);
	       startwire(&newpos);
	       eventmode = WIRE_MODE;
	       areawin->attachto = -1;
	    }
	 }
	 else {
	    Wprintf("Nothing found to attach to");
	 }
      }
   }
}

/*--------------------------------------------------------------*/
/* This function returns true if the indicated function is	*/
/* compatible with the current eventmode;  that is, whether	*/
/* the function could ever be called from eventdispatch()	*/
/* given the existing eventmode.				*/
/*								*/
/* Note that this function has to be carefully written or the	*/
/* function dispatch mechanism can put xcircuit into a bad	*/
/* state.							*/
/*--------------------------------------------------------------*/

bool compatible_function(int function)
{
   int r = false;
   QString funcname;

   switch(function) {
      case XCF_Text_Left: case XCF_Text_Right:
      case XCF_Text_Home: case XCF_Text_End:
      case XCF_Text_Return: case XCF_Text_Delete:
	 r = (eventmode == CATTEXT_MODE || eventmode == TEXT_MODE ||
		eventmode == ETEXT_MODE) ?
		true : false;
	 break;

      case XCF_Linebreak: case XCF_Halfspace:
      case XCF_Quarterspace: case XCF_TabStop:
      case XCF_TabForward: case XCF_TabBackward:
      case XCF_Superscript: case XCF_Subscript:
      case XCF_Normalscript: case XCF_Underline:
      case XCF_Overline: case XCF_Font:
      case XCF_Boldfont: case XCF_Italicfont:
      case XCF_Normalfont: case XCF_ISO_Encoding:
      case XCF_Special: case XCF_Text_Split:
      case XCF_Text_Up: case XCF_Text_Down:
      case XCF_Parameter:
	 r = (eventmode == TEXT_MODE || eventmode == ETEXT_MODE) ?
		true : false;
	 break;

      case XCF_Justify:
	 r = (eventmode == TEXT_MODE || eventmode == ETEXT_MODE ||
		eventmode == MOVE_MODE || eventmode == COPY_MODE ||
		eventmode == NORMAL_MODE) ?
		true : false;
	 break;

      case XCF_Edit_Delete: case XCF_Edit_Insert: case XCF_Edit_Append:
      case XCF_Edit_Param:
	 r = (eventmode == EPOLY_MODE || eventmode == EPATH_MODE) ?
		true : false;
	 break;

      case XCF_Edit_Next:
	 r = (eventmode == EPOLY_MODE || eventmode == EPATH_MODE ||
		eventmode == EINST_MODE || eventmode == EARC_MODE ||
		eventmode == ESPLINE_MODE) ?
		true : false;
	 break;

      case XCF_Attach:
	 r = (eventmode == EPOLY_MODE || eventmode == EPATH_MODE ||
		eventmode == MOVE_MODE || eventmode == COPY_MODE ||
		eventmode == WIRE_MODE || eventmode == NORMAL_MODE) ?
		true : false;
	 break;

      case XCF_Rotate: case XCF_Flip_X:
      case XCF_Flip_Y:
	 r = (eventmode == MOVE_MODE || eventmode == COPY_MODE ||
		eventmode == NORMAL_MODE || eventmode == CATALOG_MODE) ?
		true : false;
	 break;

      case XCF_Snap: case XCF_Swap:
	 r = (eventmode == MOVE_MODE || eventmode == COPY_MODE ||
		eventmode == NORMAL_MODE) ?
		true : false;
	 break;

      case XCF_Double_Snap: case XCF_Halve_Snap:
      case XCF_SnapTo:
	 r = (eventmode == CATALOG_MODE || eventmode == CATTEXT_MODE ||
		eventmode == ASSOC_MODE || eventmode == CATMOVE_MODE) ?
		false : true;
	 break;

      case XCF_Library_Pop:
	 r = (eventmode == CATALOG_MODE || eventmode == ASSOC_MODE) ?
		true : false;
	 break;

      case XCF_Library_Edit: case XCF_Library_Delete:
      case XCF_Library_Duplicate: case XCF_Library_Hide:
      case XCF_Library_Virtual: case XCF_Library_Move:
      case XCF_Library_Copy:
	 r = (eventmode == CATALOG_MODE) ?
		true : false;
	 break;

      case XCF_Library_Directory:
	 r = (eventmode == CATALOG_MODE || eventmode == NORMAL_MODE ||
		eventmode == ASSOC_MODE) ?
		true : false;
	 break;

      case XCF_Next_Library:
	 r = (eventmode == CATALOG_MODE || eventmode == NORMAL_MODE ||
		eventmode == ASSOC_MODE || eventmode == CATMOVE_MODE) ?
		true : false;
	 break;

      case XCF_Select: case XCF_Exit:
	 r = (eventmode == CATALOG_MODE || eventmode == NORMAL_MODE) ?
		true : false;
	 break;

      case XCF_Pop:
	 r = (eventmode == MOVE_MODE || eventmode == COPY_MODE ||
		eventmode == CATALOG_MODE || eventmode == NORMAL_MODE ||
		eventmode == ASSOC_MODE) ?
		true : false;
	 break;

      case XCF_Push:
	 r = (eventmode == MOVE_MODE || eventmode == COPY_MODE ||
		eventmode == CATALOG_MODE || eventmode == NORMAL_MODE) ?
		true : false;
	 break;

      case XCF_SelectBox: case XCF_Wire:
      case XCF_Delete: case XCF_Rescale:
      case XCF_Pin_Label: case XCF_Pin_Global:
      case XCF_Info_Label: case XCF_Connectivity:
      case XCF_Box: case XCF_Arc:
      case XCF_Text: case XCF_Exchange:
      case XCF_Copy: case XCF_Virtual:
      case XCF_Page_Directory: case XCF_Join:
      case XCF_Unjoin: case XCF_Spline:
      case XCF_Edit: case XCF_Undo:
      case XCF_Redo: case XCF_Select_Save:
      case XCF_Unselect: case XCF_Dashed:
      case XCF_Dotted: case XCF_Solid:
      case XCF_Dot: case XCF_Write:
      case XCF_Netlist: case XCF_Sim:
      case XCF_SPICE: case XCF_SPICEflat:
      case XCF_PCB: case XCF_Move:
	 r = (eventmode == NORMAL_MODE) ?
		true : false;
	 break;

      case XCF_Nothing: case XCF_View:
      case XCF_Redraw: case XCF_Zoom_In:
      case XCF_Zoom_Out: case XCF_Pan:
      case XCF_Page: case XCF_Help:
      case XCF_Cancel: case XCF_Prompt:
	 r = true;
	 break;

      case XCF_Continue_Copy:
      case XCF_Finish_Copy:
	 r = (eventmode == COPY_MODE) ?
		true : false;
	 break;

      case XCF_Continue_Element:
      case XCF_Finish_Element:
	 r = (eventmode == WIRE_MODE || eventmode == BOX_MODE ||
		eventmode == ARC_MODE || eventmode == SPLINE_MODE ||
		eventmode == EPATH_MODE || eventmode == EPOLY_MODE ||
		eventmode == EARC_MODE || eventmode == ESPLINE_MODE ||
		eventmode == MOVE_MODE || eventmode == CATMOVE_MODE ||
		eventmode == EINST_MODE || eventmode == RESCALE_MODE) ?
		true : false;
	 break;

      case XCF_Cancel_Last:
	 r = (eventmode == WIRE_MODE || eventmode == ARC_MODE ||
		eventmode == SPLINE_MODE || eventmode == EPATH_MODE || 
		eventmode == EPOLY_MODE || eventmode == EARC_MODE ||
		eventmode == EINST_MODE || eventmode == ESPLINE_MODE) ?
		true : false;
	 break;

      case XCF_Finish:
	 r = (eventmode == FONTCAT_MODE || eventmode == EFONTCAT_MODE ||
		eventmode == ASSOC_MODE || eventmode == CATALOG_MODE ||
		eventmode == CATTEXT_MODE || eventmode == MOVE_MODE ||
		eventmode == RESCALE_MODE || eventmode == SELAREA_MODE ||
		eventmode == PAN_MODE || eventmode == NORMAL_MODE ||
		eventmode == CATMOVE_MODE) ?
		true : false;
	 break;

      default:	/* Function type was not handled. */
         funcname = func_to_string(function);
         if (funcname.isEmpty())
            Wprintf("Error:  (%d) is not a known function!", function);
	 else
            Wprintf("Error:  Function type \"%ls\" (%d) not handled by "
                "compatible_function()", funcname.utf16(),
		function);
	 break;
   }
   return r;
}

/*----------------------------------------------------------------------*/
/* Main event dispatch routine.  Call one of the known routines based	*/
/* on the key binding.  Some handling is done by secondary dispatch	*/
/* routines in other files;  when this is done, the key binding is	*/
/* determined here and the bound operation type passed to the secondary	*/
/* dispatch routine.							*/
/*									*/
/* Return value:  0 if event was handled, -1 if not.			*/
/*----------------------------------------------------------------------*/

int eventdispatch(int keywstate, int x, int y)
{
   short value;		/* For return values from boundfunction() */
   int function;	/* What function should be invoked	  */

   /* Invalid key state returned from getkeysignature(); usually this	*/
   /* means a modifier key pressed by itself.				*/

   if (keywstate == -1) return -1;
   function = boundfunction(areawin->viewport, keywstate, &value);

   /* Check for printable characters in keywstate while in	*/
   /* a text-entry state.  Only the function XCF_Special is allowed in	*/
   /* text-entry mode, because XCF_Special can be used to enter the	*/
   /* character bound to the XCF_Special function.			*/

   if (keywstate >= 32 && keywstate <= 0xFFFF) {
      if (eventmode == CATTEXT_MODE || eventmode == TEXT_MODE ||
		eventmode == ETEXT_MODE) {
	 if (function != XCF_Special)
            return labeltext(keywstate, NULL);
	 else if (eventmode != CATTEXT_MODE) {
	    labelptr elabel = TOLABEL(EDITPART);
	    if (elabel->justify & LATEXLABEL)
               return labeltext(keywstate, NULL);
	 }
      }
   }

   if (function > -1)
      return functiondispatch(function, value, x, y);
   else {
      QString keystring = key_to_string(keywstate);
#ifdef HAVE_PYTHON
      if (python_key_command(keywstate) < 0)
#endif
      Wprintf("Key \'%ls\' is not bound to a macro", keystring.utf16());
   }
   return -1;
}

/*----------------------------------------------------------------------*/
/* Dispatch actions by function number.  Note that the structure of	*/
/* this function is closely tied to the routine compatible_function().	*/
/*----------------------------------------------------------------------*/

int functiondispatch(int function, short value, int x, int y)
{
   int result = 0;

   switch (eventmode) {
      case MOVE_MODE:
      case COPY_MODE:
         snap(x, y, &areawin->save);
	 break;
      case NORMAL_MODE:
	 window_to_user(x, y, &areawin->save);
	 break;
   }

   switch(function) {
      case XCF_Page:
	 if (value < 0 || value > xobjs.pages)
	    Wprintf("Page %d out of range.", (int)value);
	 else
	    newpage(value - 1);
	 break;
      case XCF_Justify:
	 rejustify(value);
	 break;
      case XCF_Superscript:
	 labeltext(SUPERSCRIPT, (char *)1);
	 break;
      case XCF_Subscript:
	 labeltext(SUBSCRIPT, (char *)1);
	 break;
      case XCF_Normalscript:
	 labeltext(NORMALSCRIPT, (char *)1);
	 break;
      case XCF_Font:
         setfont(NULL, (void*)1000, NULL);
	 break;
      case XCF_Boldfont:
         fontstyle(NULL, (void*)1, NULL);
	 break;
      case XCF_Italicfont:
         fontstyle(NULL, (void*)2, NULL);
	 break;
      case XCF_Normalfont:
         fontstyle(NULL, (void*)0, NULL);
	 break;
      case XCF_Underline:
	 labeltext(UNDERLINE, (char *)1);
	 break;
      case XCF_Overline:
	 labeltext(OVERLINE, (char *)1);
	 break;
      case XCF_ISO_Encoding:
         fontencoding(NULL, (void*)2, NULL);
	 break;
      case XCF_Halfspace:
	 labeltext(HALFSPACE, (char *)1);
	 break;
      case XCF_Quarterspace:
	 labeltext(QTRSPACE, (char *)1);
	 break;
      case XCF_Special:
	 result = dospecial();
	 break;
#ifndef TCL_WRAPPER
      case XCF_Parameter:
	 insertparam();
	 break;
#endif
      case XCF_TabStop:
	 labeltext(TABSTOP, (char *)1);
	 break;
      case XCF_TabForward:
	 labeltext(TABFORWARD, (char *)1);
	 break;
      case XCF_TabBackward:
	 labeltext(TABBACKWARD, (char *)1);
	 break;
      case XCF_Text_Return:
	 labeltext(TEXT_RETURN, (char *)1);
	 break;
      case XCF_Text_Delete:
	 labeltext(TEXT_DELETE, (char *)1);
	 break;
      case XCF_Text_Right:
	 labeltext(TEXT_RIGHT, (char *)1);
	 break;
      case XCF_Text_Left:
	 labeltext(TEXT_LEFT, (char *)1);
	 break;
      case XCF_Text_Up:
	 labeltext(TEXT_UP, (char *)1);
	 break;
      case XCF_Text_Down:
	 labeltext(TEXT_DOWN, (char *)1);
	 break;
      case XCF_Text_Split:
	 labeltext(TEXT_SPLIT, (char *)1);
	 break;
      case XCF_Text_Home:
	 labeltext(TEXT_HOME, (char *)1);
	 break;
      case XCF_Text_End:
	 labeltext(TEXT_END, (char *)1);
	 break;
      case XCF_Linebreak:
	 labeltext(RETURN, (char *)1);
	 break;
      case XCF_Edit_Param:
      case XCF_Edit_Delete:
      case XCF_Edit_Insert:
      case XCF_Edit_Append:
	 poly_edit_op(function);
	 break;
      case XCF_Edit_Next:
	 path_op(*(EDITPART), XCF_Continue_Element, x, y);
	 break;
      case XCF_Attach:
	 attach_to();
	 break;
      case XCF_Next_Library:
	 changecat();
	 break;
      case XCF_Library_Directory:
         startcatalog(NULL, (void*)LIBLIB, NULL);
	 break;
      case XCF_Library_Edit:
	 window_to_user(x, y, &areawin->save);
	 unselect_all();
	 select_element(LABEL);
	 if (areawin->selects == 1)
	    edit(x, y);
	 break;
      case XCF_Library_Delete:
	 catalog_op(XCF_Select, x, y);
	 catdelete();
	 break;
      case XCF_Library_Duplicate:
	 catalog_op(XCF_Select, x, y);
	 copycat();
	 break;
      case XCF_Library_Hide:
	 catalog_op(XCF_Select, x, y);
	 cathide();
	 break;
      case XCF_Library_Virtual:
	 catalog_op(XCF_Select, x, y);
	 catvirtualcopy();
	 break;
      case XCF_Page_Directory:
         startcatalog(NULL, (void*)PAGELIB, NULL);
	 break;
      case XCF_Library_Copy:
      case XCF_Library_Pop:
	 catalog_op(function, x, y);
	 break;
      case XCF_Virtual:
	 copyvirtual();
	 break;
      case XCF_Help:
	 starthelp(NULL, NULL, NULL);
	 break;
      case XCF_Redraw:
         areawin->update();
	 break;
      case XCF_View:
	 zoomview(NULL, NULL, NULL);
	 break;
      case XCF_Zoom_In:
	 zoominrefresh(x, y);
	 break;
      case XCF_Zoom_Out:
	 zoomoutrefresh(x, y);
	 break;
      case XCF_Pan:
	 panrefresh(value, x, y, 0.3);
	 break;
      case XCF_Double_Snap:
	 setsnap(1);
	 break;
      case XCF_Halve_Snap:
	 setsnap(-1);
	 break;
      case XCF_Write:
#ifdef TCL_WRAPPER
	 Tcl_Eval(xcinterp, "xcircuit::promptsavepage");
#else
	 outputpopup(NULL, NULL, NULL);
#endif
	 break;
      case XCF_Rotate:
	 elementrotate(value, &areawin->save);
	 break;
      case XCF_Flip_X:
	 elementflip(&areawin->save);
	 break;
      case XCF_Flip_Y:
	 elementvflip(&areawin->save);
	 break;
      case XCF_Snap:
	 snapelement();
	 break;
      case XCF_SnapTo:
	 if (areawin->snapto) {
	    areawin->snapto = false;
	    Wprintf("Snap-to off");
	 }
	 else {
	    areawin->snapto = true;
	    Wprintf("Snap-to on");
	 }
	 break;
      case XCF_Pop:
	 if (eventmode == CATALOG_MODE || eventmode == ASSOC_MODE) {
	    eventmode = NORMAL_MODE;
	    catreturn();
	 }
	 else
	    popobject(NULL, 0, NULL);
	 break;
      case XCF_Push:
	 if (eventmode == CATALOG_MODE) {
	    /* Don't allow push from library directory */
	    if ((areawin->topinstance != xobjs.libtop[LIBLIB])
                	&& (areawin->topinstance != xobjs.libtop[PAGELIB])) {
	       window_to_user(x, y, &areawin->save);
	       eventmode = NORMAL_MODE;
	       pushobject(NULL);
	    }
	 }
	 else
	    pushobject(NULL);
	 break;
      case XCF_Delete:
	 deletebutton(x, y);
	 break;
      case XCF_Select:
	 if (eventmode == CATALOG_MODE)
	    catalog_op(function, x, y);
	 else
	    select_add_element(ALL_TYPES);
	 break;
      case XCF_Box:
	 boxbutton(x, y);
	 break;
      case XCF_Arc:
	 arcbutton(x, y);
	 break;
      case XCF_Text:
	 eventmode = TEXT_MODE;
         areawin->viewport->setFocus();
	 textbutton(NORMAL, x, y);
	 break;
      case XCF_Exchange:
	 exchange();
	 break;
      case XCF_Library_Move:
	 /* Don't allow from library directory.  Then fall through to XCF_Move */
	 if (areawin->topinstance == xobjs.libtop[LIBLIB]) break;
      case XCF_Move:
	 if (areawin->selects == 0) {
	    was_preselected = false;
	    if (eventmode == CATALOG_MODE)
	       catalog_op(XCF_Select, x, y);
	    else
	       select_element(ALL_TYPES);
	 }
	 else was_preselected = true;

	 if (areawin->selects > 0) {
	    eventmode = (eventmode == CATALOG_MODE) ? CATMOVE_MODE : MOVE_MODE;
            u2u_snap(areawin->save);
	    areawin->origin = areawin->save;
            reset_cycles();
	    select_connected_pins();
            XDefineCursor(areawin->viewport, ARROW);
#ifdef TCL_WRAPPER
	    Tk_CreateEventHandler(areawin->area, ButtonMotionMask,
			(Tk_EventProc *)xctk_drag, NULL);
#endif
	 }
	 break;
      case XCF_Join:
	 join();
	 break;
      case XCF_Unjoin:
	 unjoin();
	 break;
      case XCF_Spline:
	 splinebutton(x, y);
	 break;
      case XCF_Edit:
	 edit(x, y);
	 break;
      case XCF_Undo:
	 undo_action();
	 break;
      case XCF_Redo:
	 redo_action();
	 break;
      case XCF_Select_Save:
#ifdef TCL_WRAPPER
	 Tcl_Eval(xcinterp, "xcircuit::promptmakeobject");
#else
	 selectsave(NULL, NULL, NULL);
#endif
	 break;
      case XCF_Unselect:
	 select_add_element(-ALL_TYPES);
	 break;
      case XCF_Dashed:
	 setelementstyle(NULL, DASHED, NOBORDER | DOTTED | DASHED);
	 break;
      case XCF_Dotted:
	 setelementstyle(NULL, DOTTED, NOBORDER | DOTTED | DASHED);
	 break;
      case XCF_Solid:
	 setelementstyle(NULL, NORMAL, NOBORDER | DOTTED | DASHED);
	 break;
      case XCF_Prompt:
	 docommand();
	 break;
      case XCF_Dot:
	 snap(x, y, &areawin->save);
	 drawdot(areawin->save.x, areawin->save.y);
         areawin->update();
	 break;
      case XCF_Wire:
         u2u_snap(areawin->save);
	 startwire(&areawin->save);
	 eventmode = WIRE_MODE;
	 break;
      case XCF_Nothing:
	 DoNothing(NULL, NULL, NULL);
	 break;
      case XCF_Exit:
         quitcheck((QAction*)1, NULL, NULL);
	 break;
      case XCF_Netlist:
	 callwritenet(NULL, 0, NULL);
	 break;
      case XCF_Swap:
	 swapschem(0, -1, NULL);
	 break;
      case XCF_Pin_Label:
	 eventmode = TEXT_MODE;
	 textbutton(LOCAL, x, y);
	 break;
      case XCF_Pin_Global:
	 eventmode = TEXT_MODE;
	 textbutton(GLOBAL, x, y);
	 break;
      case XCF_Info_Label:
	 eventmode = TEXT_MODE;
	 textbutton(INFO, x, y);
	 break;
      case XCF_Rescale:
	 if (checkselect(LABEL | OBJINST | GRAPHIC) == true) {
	    eventmode = RESCALE_MODE;
            areawin->update();
            xcAddEventHandler(areawin->viewport, ButtonMotionMask, false,
		(XtEventHandler)xlib_drag, NULL);
	 }
	 break;
      case XCF_SelectBox:
	 startselect();
	 break;
      case XCF_Connectivity:
	 connectivity(NULL, NULL, NULL);
	 break;
      case XCF_Copy:
      case XCF_Continue_Copy:
      case XCF_Finish_Copy:
	 copy_op(function, x, y);
	 break;
      case XCF_Continue_Element:
	 if (eventmode == CATMOVE_MODE || eventmode == MOVE_MODE ||
		eventmode == RESCALE_MODE)
	    finish_op(XCF_Finish, x, y);
	 else
	    continue_op(function, x, y);
	 break;
      case XCF_Finish_Element:
      case XCF_Cancel_Last:
      case XCF_Cancel:
	 finish_op(function, x, y);
	 break;
      case XCF_Finish:
	 if (eventmode == CATALOG_MODE || eventmode == ASSOC_MODE)
	    catalog_op(XCF_Library_Pop, x, y);
	 else
	    finish_op(function, x, y);
	 break;
      case XCF_Sim:
	 writenet(topobject, "flatsim", "sim");
	 break;
      case XCF_SPICE:
	 writenet(topobject, "spice", "spc");
	 break;
      case XCF_PCB:
	 writenet(topobject, "pcb", "pcbnet");
	 break;
      case XCF_SPICEflat:
	 writenet(topobject, "flatspice", "fspc");
	 break;
      case XCF_Graphic:
	 Wprintf("Action not handled");
	 result = -1;
	 break;
      case XCF_Text_Delete_Param:
	 Wprintf("Action not handled");
	 result = -1;
	 break;
      case XCF_ChangeStyle:
	 Wprintf("Action not handled");
	 result = -1;
	 break;
   }

   /* Ensure that we do not get stuck in suspend mode	*/
   /* by removing the suspend state whenever a key is	*/
   /* pressed.						*/

   if (xobjs.suspend == 1) {
      xobjs.suspend = -1;
      refresh(NULL, NULL, NULL);
   }
   else if (xobjs.suspend != 2)
      xobjs.suspend = -1;

   return result;
}

/*------------------------------------------------------*/
/* Get a canonical signature for a button/key event	  */
/* This is essentially Qt::Key | Qt::KeyboardModifer    */
/*------------------------------------------------------*/

int getkeysignature(XKeyEvent *event)
{
    int keywstate = event->key();

    /* Ignore Shift, Control, Caps Lock, and Meta (Alt) keys 	*/
    /* when pressed alone.					*/

    if (keywstate == Qt::Key_Control || keywstate == Qt::Key_Shift ||
        keywstate == Qt::Key_Alt || keywstate == Qt::Key_AltGr ||
        keywstate == Qt::Key_CapsLock || keywstate == Qt::Key_NumLock)
      return -1;

    /* Only keep key state information pertaining to Shift, Caps Lock,	*/
    /* Control, and Alt (Meta)						*/

    int modifiers = event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);

    /* Printable keys with no modifiers or shift modifier are replaced by a case-correct version  */
    /* Otherwise, modifiers are merged with the key.                                              */
    if ((modifiers == 0 || modifiers == Qt::ShiftModifier) && keywstate >= 32 && keywstate <= 0xFFFF) {
        keywstate = event->text()[0].unicode();
    } else {
        keywstate |= modifiers;
    }

    /* Mouse button and key events are treated the same, the XKeyEvent is a QKeyEvent that stores mouse coordinates.
       Since this is a hack of sorts, we're treating mouse buttons like individual keyboard keys, not like modifiers
       -- presses of multiple mouse buttons at the same time are ignored, only the last-pressed button is taken
       into account. It'd be a long shot to expect various Qt mechanisms not to misbehave when presented with modifier bits
       below Qt::ShiftModifier. */

    return keywstate;
}

/*------------------------*/
/* Handle keyboard inputs */
/*------------------------*/

void keyhandler(Widget w, caddr_t clientdata, XKeyEvent *event)
{
   int keywstate;	/* KeySym with prepended state information	*/
   int j, func;

#ifdef TCL_WRAPPER
   if (popups > 0) return;
#else
   if (popups > 0 && help_up == 0) return;
#endif

   bool newEvent = false;
   if (!dynamic_cast<XKeyEvent*>(event)) {
       event = new XKeyEvent(*dynamic_cast<QKeyEvent*>(event));
       newEvent = true;
   }
   //qDebug("key event: tf=%d vf=%d key=%x type=%d", top->hasFocus(), areawin->viewport->hasFocus(), event->key(), event->type());

   if (event->type() == QEvent::KeyRelease) {

      /* Register a "tap" event if a key or button was released	*/
      /* while a timeout event is pending.			*/

      if (areawin->time_id != 0) {
         xcRemoveTimeout(areawin->time_id);
         areawin->time_id = 0;
         keywstate = getkeysignature(event);
         eventdispatch(keywstate, areawin->save.x, areawin->save.y);
      }
      else {
         keywstate = getkeysignature(event);
	 if ((pressmode != 0) && (keywstate == pressmode)) {
            /* Events that require hold & drag (namely, MOVE_MODE)	*/
	    /* must be resolved here.  Call finish_op() to ensure	*/
	    /* that we restore xcircuit to	a state of sanity.	*/

            finish_op(XCF_Finish, event->x(), event->y());
	    pressmode = 0;
         }
	 return;	/* Ignore all other release events */
      }
   }

   /* Check if any bindings match key/button "hold".  If so, then start	*/
   /* the timer and wait for key release or timeout.			*/

   else {
      keywstate = getkeysignature(event);
      if ((keywstate != -1) && (xobjs.hold == true)) {

	 /* Establish whether a HOLD modifier binding would apply in	*/
	 /* the current eventmode.  If so, set the HOLD timer.		*/

	 j = 0;
         func = boundfunction(areawin->viewport, keywstate | HOLD, NULL);
	 if (func != -1) {
            areawin->save.x = event->x();
            areawin->save.y = event->y();
            areawin->time_id = xcAddTimeout(app, PRESSTIME,
			makepress, (ClientData)((pointertype)keywstate));
            return;
	 }

      }
      eventdispatch(keywstate, event->x(), event->y());
   }

   if (newEvent) delete event;
}

/*--------------------------------*/
/* Set snap spacing from keyboard */
/*--------------------------------*/

void setsnap(short direction)
{
   float oldsnap = xobjs.pagelist[areawin->page].snapspace;
   char buffer[50];

   if (direction > 0) xobjs.pagelist[areawin->page].snapspace *= 2;
   else {
      if (oldsnap >= 2.0)
         xobjs.pagelist[areawin->page].snapspace /= 2;
      else {
         measurestr(xobjs.pagelist[areawin->page].snapspace, buffer);
	 Wprintf("Snap space at minimum value of %s", buffer);
      }
   }
   if (xobjs.pagelist[areawin->page].snapspace != oldsnap) {
      measurestr(xobjs.pagelist[areawin->page].snapspace, buffer);
      Wprintf("Snap spacing set to %s", buffer);
      areawin->update();
  }
}

/*-----------------------------------------*/
/* Reposition an object onto the snap grid */
/*-----------------------------------------*/

void snapelement()
{
   short *selectobj;
   bool preselected;

   preselected = (areawin->selects > 0) ? true : false;
   if (!checkselect(ALL_TYPES)) return;
   for (selectobj = areawin->selectlist; selectobj < areawin->selectlist
        + areawin->selects; selectobj++) {
      switch(SELECTTYPE(selectobj)) {
         case OBJINST: {
	    objinstptr snapobj = SELTOOBJINST(selectobj);

            u2u_snap(snapobj->position);
	    } break;
         case GRAPHIC: {
	    graphicptr snapg = SELTOGRAPHIC(selectobj);

            u2u_snap(snapg->position);
	    } break;
         case LABEL: {
	    labelptr snaplabel = SELTOLABEL(selectobj);

            u2u_snap(snaplabel->position);
	    } break;
         case POLYGON: {
	    polyptr snappoly = SELTOPOLY(selectobj);
            std::transform(snappoly->points.begin(), snappoly->points.end(),
                           snappoly->points.begin(), u2u_getSnapped);
            } break;
         case ARC: {
	    arcptr snaparc = SELTOARC(selectobj);

            u2u_snap(snaparc->position);
	    if (areawin->snapto) {
	       snaparc->radius = (snaparc->radius /
                    xobjs.pagelist[areawin->page].snapspace) *
                    xobjs.pagelist[areawin->page].snapspace;
	       snaparc->yaxis = (snaparc->yaxis /
                    xobjs.pagelist[areawin->page].snapspace) *
                    xobjs.pagelist[areawin->page].snapspace;
	    }
            snaparc->calc();
	    } break;
	 case SPLINE: {
	    splineptr snapspline = SELTOSPLINE(selectobj);
	    short i;

	    for (i = 0; i < 4; i++)
               u2u_snap(snapspline->ctrl[i]);
            snapspline->calc();
	    } break;
      }
   }
   select_invalidate_netlist();
   if (eventmode == NORMAL_MODE)
      if (!preselected)
         unselect_all();
}

/*----------------------------------------------*/
/* Routines to print the cursor position	*/
/*----------------------------------------------*/

/*----------------------------------------------*/
/* fast integer power-of-10 routine 		*/
/*----------------------------------------------*/

int ipow10(int a)
{
   int i = 1;
   while (a--) { i *= 10; }
   return i;
}

/*--------------------------------------------------*/
/* find greatest common factor between two integers */
/*--------------------------------------------------*/

int calcgcf(int a, int b)
{
   register int mod;

   if ((mod = a % b) == 0) return (b);
   else return (calcgcf(b, mod));
}

/*--------------------------------------------------------------*/
/* generate a fraction from a float, if possible 		*/
/* fraction returned as a string (must be allocated beforehand)	*/
/*--------------------------------------------------------------*/

void fraccalc(float xyval, char *fstr)
{
   short i, t, rept;
   int ip, mant, divisor, denom, numer, rpart;
   double fp;
   char num[10], *nptr = &num[2], *sptr;
   
   ip = (int)xyval;
   fp = fabs(xyval - ip);

   /* write fractional part and grab mantissa as integer */

   sprintf(num, "%1.7f", fp);
   num[8] = '\0';		/* no rounding up! */
   sscanf(nptr, "%d", &mant);

   if (mant != 0) {    /* search for repeating substrings */
      for (i = 1; i <= 3; i++) {
         rept = 1;
	 nptr = &num[8] - i;
	 while ((sptr = nptr - rept * i) >= &num[2]) {
            for (t = 0; t < i; t++)
	       if (*(sptr + t) != *(nptr + t)) break;
	    if (t != i) break;
	    else rept++;
	 }
	 if (rept > 1) break;
      }
      nptr = &num[8] - i;
      sscanf(nptr, "%d", &rpart);  /* rpart is repeating part of mantissa */
      if (i > 3 || rpart == 0) { /* no repeat */ 
	 divisor = calcgcf(1000000, mant);
	 denom = 1000000 / divisor;
      }
      else { /* repeat */
	 int z, p, fd;

	 *nptr = '\0';
	 sscanf(&num[2], "%d", &z);
	 p = ipow10(i) - 1;
	 mant = z * p + rpart;
	 fd = ipow10(nptr - &num[2]) * p;

	 divisor = calcgcf(fd, mant);
	 denom = fd / divisor;
      }
      numer = mant / divisor;
      if (denom > 1024)
	 sprintf(fstr, "%5.3f", xyval); 
      else if (ip == 0)
         sprintf(fstr, "%hd/%hd", (xyval > 0) ? numer : -numer, denom);
      else
         sprintf(fstr, "%hd %hd/%hd", ip, numer, denom);
   }
   else sprintf(fstr, "%hd", ip);
}

/*------------------------------------------------------------------------------*/
/* Print the position of the cursor in the upper right-hand message window	*/
/*------------------------------------------------------------------------------*/

void printpos(const XPoint & pt)
{
   QString str;
   float f1, f2;
   float oscale, iscale = (float)xobjs.pagelist[areawin->page].drawingscale.y /
        (float)xobjs.pagelist[areawin->page].drawingscale.x;
   int llen, lwid;
   u_char wlflag = 0;
   XPoint *tpoint, *npoint;
   short cycle;

   /* For polygons, print the length (last line of a wire or polygon) or  */
   /* length and width (box only)					  */

   if (eventmode == BOX_MODE || eventmode == EPOLY_MODE || eventmode == WIRE_MODE) {
      polyptr lwire = (eventmode == BOX_MODE) ?  TOPOLY(ENDPART) : TOPOLY(EDITPART);
      if ((eventmode == EPOLY_MODE) && (lwire->points.count() > 2)) {
	 /* sanity check on edit cycle */
         cycle = lwire->cycle->number;
         if (cycle < 0 || cycle >= lwire->points.count()) {
            advancecycle((genericptr *)(&lwire), 0);
            cycle = 0;
         }
         tpoint = lwire->points.begin() + cycle;
         npoint = lwire->points.begin() + checkcycle(lwire, 1);
         llen = wirelength(tpoint, npoint);
         npoint = lwire->points.begin() + checkcycle(lwire, -1);
         lwid = wirelength(tpoint, npoint);
         wlflag = 3;
	 if (lwire->style & UNCLOSED) {   /* unclosed polys */
            if (cycle == 0)
	       wlflag = 1;
            else if (cycle == lwire->points.count() - 1) {
	       wlflag = 1;
	       llen = lwid;
	    }
	 }
	 if ((npoint->y - tpoint->y) == 0) {	/* swap width and length */
	    int tmp = lwid;
	    lwid = llen;
	    llen = tmp;
	 }
      }
      else if (eventmode == BOX_MODE) {
         tpoint = lwire->points.begin();
         npoint = lwire->points.begin() + 1;
         llen = wirelength(tpoint, npoint);
         npoint = lwire->points.begin() + 3;
         lwid = wirelength(tpoint, npoint);
	 if ((npoint->y - tpoint->y) == 0) {	/* swap width and length */
	    int tmp = lwid;
	    lwid = llen;
	    llen = tmp;
	 }
	 wlflag = 3;
      }
      else {
         tpoint = lwire->points.end() - 1;
         llen = wirelength(tpoint - 1, tpoint);
         wlflag = 1;
      }
   }
   else if (eventmode == ARC_MODE || eventmode == EARC_MODE) {
      arcptr larc = (eventmode == ARC_MODE) ?  TOARC(ENDPART) : TOARC(EDITPART);
      llen = larc->radius;
      if (abs(larc->radius) != larc->yaxis) {
	 lwid = larc->yaxis;
	 wlflag = 3;
      }
      else
         wlflag = 1;
   }

   switch (xobjs.pagelist[areawin->page].coordstyle) {
      case INTERNAL:
         str.sprintf("%g, %g", pt.x * iscale, pt.y * iscale);
	 if (wlflag) {
	    if (wlflag & 2)
               str.sprintf("%ls (%g x %g)", str.utf16(), llen * iscale, lwid * iscale);
	    else
               str.sprintf("%ls (length %g)", str.utf16(), llen * iscale);
	 }
	 break;
      case DEC_INCH:
         oscale = xobjs.pagelist[areawin->page].outscale * INCHSCALE;
         f1 = ((float)(pt.x) * iscale * oscale) / 72.0;
         f2 = ((float)(pt.y) * iscale * oscale) / 72.0;
         str.sprintf("%5.3f, %5.3f in", f1, f2);
	 if (wlflag) {
            f1 = ((float)(llen) * iscale * oscale) / 72.0;
	    if (wlflag & 2) {
               f2 = ((float)(lwid) * iscale * oscale) / 72.0;
               str.sprintf("%ls (%5.3f x %5.3f in)", str.utf16(), f1, f2);
	    }
	    else
               str.sprintf("%ls (length %5.3f in)", str.utf16(), f1);
	 }
	 break;
      case FRAC_INCH: {
	 char fstr1[30], fstr2[30];
	 
         oscale = xobjs.pagelist[areawin->page].outscale * INCHSCALE;
         fraccalc((((float)(pt.x) * iscale * oscale) / 72.0), fstr1);
         fraccalc((((float)(pt.y) * iscale * oscale) / 72.0), fstr2);
         str.sprintf("%s, %s in", fstr1, fstr2);
	 if (wlflag) {
	    fraccalc((((float)(llen) * iscale * oscale) / 72.0), fstr1);
	    if (wlflag & 2) {
	       fraccalc((((float)(lwid) * iscale * oscale) / 72.0), fstr2);
               str.sprintf("%ls (%s x %s in)", str.utf16(), fstr1, fstr2);
	    }
	    else
               str.sprintf("%ls (length %s in)", str.utf16(), fstr1);
	 }
	 } break;
      case CM:
         oscale = xobjs.pagelist[areawin->page].outscale * CMSCALE;
         f1 = ((float)(pt.x) * iscale * oscale) / IN_CM_CONVERT;
         f2 = ((float)(pt.y) * iscale * oscale) / IN_CM_CONVERT;
         str.sprintf("%5.3f, %5.3f cm", f1, f2);
	 if (wlflag) {
            f1 = ((float)(llen) * iscale * oscale) / IN_CM_CONVERT;
	    if (wlflag & 2) {
               f2 = ((float)(lwid) * iscale * oscale) / IN_CM_CONVERT;
               str.sprintf("%ls (%5.3f x %5.3f cm)", str.utf16(), f1, f2);
	    }
   	    else
               str.sprintf("%ls (length %5.3f cm)", str.utf16(), f1);
	 }
	 break;
   }
   W1printf("%ls", str.utf16());
}

/*---------------------------------------------------*/
/* Find nearest point of intersection of the cursor  */
/* position to a wire and move there.		     */
/*---------------------------------------------------*/

void findwirex(XPoint *endpt1, XPoint *endpt2, XPoint *userpt,
		XPoint *newpos, int *rot)
{
   long xsq, ysq, zsq;
   float frac;

   xsq = sqwirelen(endpt1, endpt2);
   ysq = sqwirelen(endpt1, userpt);
   zsq = sqwirelen(endpt2, userpt);
   frac = 0.5 + (float)(ysq - zsq) / (float)(xsq << 1);
   if (frac > 1) frac = 1;
   else if (frac < 0) frac = 0;
   newpos->x = endpt1->x + (int)((endpt2->x - endpt1->x) * frac);
   newpos->y = endpt1->y + (int)((endpt2->y - endpt1->y) * frac);

   *rot = 180 + (int)(INVRFAC * atan2((double)(endpt1->x -
	  endpt2->x), (double)(endpt1->y - endpt2->y)));

   /* make adjustment for nearest-integer calculation 	 */
   /* ((*rot)++, (*rot)-- works if (360 / RSTEPS >= 2))  */  /* ? */

   if (*rot > 0)
      (*rot)++;
   else if (*rot < 0)
      (*rot)--;
}

/*----------------------------------------------------------------*/
/* Find the closest point of attachment from the pointer position */
/* to the "attachto" element.					  */
/*----------------------------------------------------------------*/

void findattach(XPoint *newpos, int *rot, XPoint *userpt)
{
   XPoint *endpt1, *endpt2;
   float frac;
   double tmpang;
   int locrot = 0;

   if (rot) locrot = *rot;

   /* find point of intersection and slope */

   if (SELECTTYPE(&areawin->attachto) == ARC) {
      arcptr aarc = SELTOARC(&areawin->attachto);
      float tmpdeg;
      tmpang = atan2((double)(userpt->y - aarc->position.y) * (double)
		(abs(aarc->radius)), (double)(userpt->x - aarc->position.x) *
		(double)aarc->yaxis);

      /* don't follow the arc beyond its endpoints */

      tmpdeg = (float)(tmpang * INVRFAC);
      if (tmpdeg < 0) tmpdeg += 360;
      if (((aarc->angle2 > 360) && (tmpdeg > aarc->angle2 - 360) &&
		(tmpdeg < aarc->angle1)) ||
	  	((aarc->angle1 < 0) && (tmpdeg > aarc->angle2) &&
		(tmpdeg < aarc->angle1 + 360)) ||
	  	((aarc->angle1 >= 0) && (aarc->angle2 <= 360) && ((tmpdeg
		> aarc->angle2) || (tmpdeg < aarc->angle1)))) {
	 float testd1 = aarc->angle1 - tmpdeg;
	 float testd2 = tmpdeg - aarc->angle2;
	 if (testd1 < 0) testd1 += 360;
	 if (testd2 < 0) testd2 += 360;

	 /* if arc is closed, attach to the line between the endpoints */

	 if (!(aarc->style & UNCLOSED)) {
	    XPoint end1, end2;
	    tmpang = (double) aarc->angle1 / INVRFAC;
	    end1.x = aarc->position.x + abs(aarc->radius) * cos(tmpang);
	    end1.y = aarc->position.y + aarc->yaxis  * sin(tmpang);
	    tmpang = (double) aarc->angle2 / INVRFAC;
	    end2.x = aarc->position.x + abs(aarc->radius) * cos(tmpang);
	    end2.y = aarc->position.y + aarc->yaxis  * sin(tmpang);
            findwirex(&end1, &end2, userpt, newpos, &locrot);
	    if (rot) *rot = locrot;
	    return;
	 }
	 else
	    tmpang = (double)((testd1 < testd2) ? aarc->angle1 : aarc->angle2)
			/ INVRFAC;
      }

      /* get position in user coordinates nearest to the intersect pt */

      newpos->x = aarc->position.x + abs(aarc->radius) * cos(tmpang);
      newpos->y = aarc->position.y + aarc->yaxis  * sin(tmpang);

      /* rotation of object is normal to the curve of the ellipse */

      if (rot) {
         *rot = 90 - (int)(INVRFAC * tmpang);
         if (*rot < 0) *rot += 360;
      }
   }
   else if (SELECTTYPE(&areawin->attachto) == SPLINE) {
       splineptr aspline = SELTOSPLINE(&areawin->attachto);
       frac = findsplinemin(aspline, userpt);
       findsplinepos(aspline, frac, newpos, &locrot);
       if (rot) *rot = locrot;
   }
   else if (SELECTTYPE(&areawin->attachto) == OBJINST) {

      objinstptr ainst = SELTOOBJINST(&areawin->attachto);
      objectptr aobj = ainst->thisobject;
      long testdist, mindist = 1e8;
      XPoint mdpoint;

      /* In case instance has no pin labels, we will attach to	*/
      /* the instance's own origin.				*/

      mdpoint.x = mdpoint.y = 0;
      ReferencePosition(ainst, &mdpoint, newpos);

      /* Find the nearest pin label in the object instance and attach to it */
      for (labeliter alab; aobj->values(alab); ) {
        if (alab->pin == LOCAL || alab->pin == GLOBAL) {
           ReferencePosition(ainst, &alab->position, &mdpoint);
           testdist = sqwirelen(&mdpoint, userpt);
           if (testdist < mindist) {
              mindist = testdist;
              *newpos = mdpoint;
           }
        }
      }
   }
   else if (SELECTTYPE(&areawin->attachto) == LABEL) {
      /* Only one choice:  Attach to the label position */
      labelptr alabel = SELTOLABEL(&areawin->attachto);
      *newpos = alabel->position;
   }
   else if (SELECTTYPE(&areawin->attachto) == POLYGON) {
       polyptr apoly = SELTOPOLY(&areawin->attachto);
       XPoint *testpt, *minpt, *nxtpt;
       long mindist = 1e8, testdist;
       minpt = nxtpt = apoly->points.begin(); 	/* so variables aren't uninitialized */
       for (testpt = apoly->points.begin(); testpt < apoly->points.end() - 1; testpt++) {
	 testdist =  finddist(testpt, testpt + 1, userpt);
	 if (testdist < mindist) {
	    mindist = testdist;
	    minpt = testpt;
	    nxtpt = testpt + 1;
	 }
      }
      if (!(apoly->style & UNCLOSED)) {
         testdist = finddist(testpt, apoly->points.begin(), userpt);
	 if (testdist < mindist) {
	    mindist = testdist;
	    minpt = testpt;
            nxtpt = apoly->points.begin();
	 }
      }
      endpt1 = minpt;
      endpt2 = nxtpt;
      findwirex(endpt1, endpt2, userpt, newpos, &locrot);
      if (rot) *rot = locrot;
   }
}

/*--------------------------------------------*/
/* Find closest point in a path to the cursor */
/*--------------------------------------------*/

XPoint *pathclosepoint(pathptr dragpath, XPoint *newpos)
{
   XPoint *rpoint;
   genericptr *cpoint;
   short mpoint;
   int mdist = 1000000, tdist;

   for (cpoint = dragpath->begin(); cpoint != dragpath->end(); cpoint++) {
      switch(ELEMENTTYPE(*cpoint)) {
	 case ARC:
	   tdist = wirelength(&(TOARC(cpoint)->position), newpos);
	    if (tdist < mdist) {
	       mdist = tdist;
	       rpoint = &(TOARC(cpoint)->position);
	    }
	    break;
	 case POLYGON:
	    mpoint = closepoint(TOPOLY(cpoint), newpos);
	    tdist = wirelength(TOPOLY(cpoint)->points + mpoint, newpos);
	    if (tdist < mdist) {
	       mdist = tdist;
	       rpoint = TOPOLY(cpoint)->points + mpoint;
	    }
	    break;
	 case SPLINE:
	    tdist = wirelength(&(TOSPLINE(cpoint)->ctrl[0]), newpos);
	    if (tdist < mdist) {
	       mdist = tdist;
	       rpoint = &(TOSPLINE(cpoint)->ctrl[0]);
	    }
	    tdist = wirelength(&(TOSPLINE(cpoint)->ctrl[3]), newpos);
	    if (tdist < mdist) {
	       mdist = tdist;
	       rpoint = &(TOSPLINE(cpoint)->ctrl[3]);
	    }
	    break;
      }
   }
   return rpoint;
}

/*-------------------------------------------*/
/* Drag a selected element around the screen */
/*-------------------------------------------*/

void drag(int x, int y)
{
    // TODO: this should use event compression, or at least approximate it
   XPoint userpt;
   XPoint delta;
   int locx, locy;

   locx = x;
   locy = y;

   /* Determine if this event is supposed to be handled by 	*/
   /* trackselarea(), or whether we should not be here at all	*/
   /* (button press and mouse movement in an unsupported mode)	*/

   if (eventmode == SELAREA_MODE) {
      trackselarea();
      return;
   }
   else if (eventmode == RESCALE_MODE) {
      trackrescale();
      return;
   }
   else if (eventmode == PAN_MODE) {
      trackpan(x, y);
      return;
   }
   else if (eventmode != CATMOVE_MODE && eventmode != MOVE_MODE
		&& eventmode != COPY_MODE)
      return;

   snap(locx, locy, &userpt);
   delta = userpt - areawin->save;
   if (delta.x == 0 && delta.y == 0) return;

   areawin->save = userpt;

   placeselects(delta, &userpt);

   /* print the position and other useful measurements */

   printpos(userpt);
}

/*------------------------------------------------------*/
/* Wrapper for drag() for xlib callback compatibility.	*/
/*------------------------------------------------------*/

void xlib_drag(Widget w, caddr_t clientdata, QEvent* *event)
{
   XButtonEvent *bevent = (XButtonEvent *)event;
   drag(bevent->x(), bevent->y());
}

/*----------------------------------------------*/
/* Rotate an element of a path			*/
/*----------------------------------------------*/

void elemrotate(genericptr genobj, short direction, XPoint *position)
{
   XPoint negpt;

   negpt.x = -position->x;
   negpt.y = -position->y;

   switch(ELEMENTTYPE(genobj)) {
      case(ARC):{
         arcptr rotatearc = TOARC(&genobj);
	 rotatearc->angle1 -= (float)(direction);
	 rotatearc->angle2 -= (float)(direction);
         if (rotatearc->angle1 >= 360) {
            rotatearc->angle1 -= 360;
            rotatearc->angle2 -= 360;
         }
         else if (rotatearc->angle2 <= 0) {
            rotatearc->angle1 += 360;
            rotatearc->angle2 += 360;
         } 
         XPoint newpt;
         UTransformPoints(&rotatearc->position, &newpt, 1, negpt, 1.0, 0);
         UTransformPoints(&newpt, &rotatearc->position, 1, *position,
			1.0, direction);
         rotatearc->calc();
	 }break;

      case(SPLINE):{
         splineptr rotatespline = TOSPLINE(&genobj);
         XPoint newpts[4];
	 UTransformPoints(rotatespline->ctrl, newpts, 4, negpt, 1.0, 0);
	 UTransformPoints(newpts, rotatespline->ctrl, 4, *position,
			1.0, direction);
         rotatespline->calc();
	 }break;

      case(POLYGON):{
         polyptr rotatepoly = TOPOLY(&genobj);
         pointlist newpts(rotatepoly->points.count());
         UTransformPoints(rotatepoly->points.begin(), newpts.begin(), rotatepoly->points.count(),
		   negpt, 1.0, 0);
         UTransformPoints(newpts.begin(), rotatepoly->points.begin(), rotatepoly->points.count(),
		   *position, 1.0, direction);
	 }break;
   }
}

/*------------------------------------------------------*/
/* Rotate an element or group of elements		*/
/* Objects and labels, if selected singly, rotate	*/
/* about their position point.  All other elements,	*/
/* and groups, rotate about the cursor point.		*/
/*------------------------------------------------------*/

void elementrotate(short direction, XPoint *position)
{
  short    *selectobj;
   bool  single = false;
   bool  need_refresh = false;
   bool  preselected;
   XPoint   newpt, negpt;

   preselected = (areawin->selects > 0) ? true : false;
   if (!checkselect(ALL_TYPES)) return;
   if (areawin->selects == 1) single = true;

   negpt.x = -position->x;
   negpt.y = -position->y;

   for (selectobj = areawin->selectlist; selectobj < areawin->selectlist
        + areawin->selects; selectobj++) {

      switch(SELECTTYPE(selectobj)) {

	 case(OBJINST):{
            objinstptr rotateobj = SELTOOBJINST(selectobj);

	    if (is_library(topobject) >= 0 && !is_virtual(rotateobj)) break;
            rotateobj->rotation += direction;
	    while (rotateobj->rotation >= 360) rotateobj->rotation -= 360;
	    while (rotateobj->rotation <= 0) rotateobj->rotation += 360;
	    if (!single) {
	       UTransformPoints(&rotateobj->position, &newpt, 1, negpt, 1.0, 0);
	       UTransformPoints(&newpt, &rotateobj->position, 1, *position,
			1.0, direction);
	    }
	    }break;

	 case(LABEL):{
            labelptr rotatetext = SELTOLABEL(selectobj);

            rotatetext->rotation += direction;
	    while (rotatetext->rotation >= 360) rotatetext->rotation -= 360;
	    while (rotatetext->rotation <= 0) rotatetext->rotation += 360;
	    if (!single) {
	       UTransformPoints(&rotatetext->position, &newpt, 1, negpt, 1.0, 0);
	       UTransformPoints(&newpt, &rotatetext->position, 1, *position,
			1.0, direction);
	    }
	    }break;

	 case(GRAPHIC):{
            graphicptr rotateg = SELTOGRAPHIC(selectobj);

            rotateg->rotation += direction;
	    while (rotateg->rotation >= 360) rotateg->rotation -= 360;
	    while (rotateg->rotation <= 0) rotateg->rotation += 360;
	    rotateg->valid = false;
	    if (!single) {
	       UTransformPoints(&rotateg->position, &newpt, 1, negpt, 1.0, 0);
	       UTransformPoints(&newpt, &rotateg->position, 1, *position,
			1.0, direction);
	    }
	    need_refresh = true;
	    }break;

	 case POLYGON: case ARC: case SPLINE:{
            genericptr genpart = topobject->at(*selectobj);
	    register_for_undo(XCF_Edit, UNDO_MORE, areawin->topinstance,
                        genpart);
            elemrotate(genpart, direction, position);
	    }break;

	 case PATH:{
	    genericptr *genpart;
	    pathptr rotatepath = SELTOPATH(selectobj);

	    register_for_undo(XCF_Edit, UNDO_MORE, areawin->topinstance,
			rotatepath);
            for (genpart = rotatepath->begin(); genpart < rotatepath->end(); genpart++)
               elemrotate(*genpart, direction, position);
	    }break;
      }

      /* redisplay the element */
      if (preselected || ((eventmode != NORMAL_MODE) && !need_refresh)) {
          need_refresh = true;
      }
   }

   /* This takes care of all selected instances and labels in one go,	*/
   /* because we only need to know the origin and amount of rotation.	*/

   if (eventmode != COPY_MODE)
      register_for_undo(XCF_Rotate, UNDO_MORE, areawin->topinstance,
		(eventmode == MOVE_MODE) ? &areawin->origin : position,
		(int)direction);

   /* New rule (6/15/07) to be applied generally:  If objects were	*/
   /* selected prior to calling elementrotate() and similar functions,	*/
   /* leave them selected upon exit.  Otherwise, deselect them.		*/

   if (eventmode == NORMAL_MODE || eventmode == CATALOG_MODE)
      if (!preselected)
         unselect_all();

   if (eventmode == CATALOG_MODE) {
      int libnum;
      if ((libnum = is_library(topobject)) >= 0) {
	 composelib(libnum + LIBRARY);
	 need_refresh = true;
      }
   }
   else {
      pwriteback(areawin->topinstance);
      calcbbox(areawin->topinstance);
   }

   if (need_refresh) areawin->update();
}

/*----------------------------------------------*/
/* Rescale the current edit element to the	*/
/* dimensions of the rescale box.		*/
/*----------------------------------------------*/

void elementrescale(float newscale)
{
   short *selectobj;
   labelptr sclab;
   objinstptr scinst;
   graphicptr scgraph;
   float oldsize;

   for (selectobj = areawin->selectlist; selectobj < areawin->selectlist
		+ areawin->selects; selectobj++) {
      switch (SELECTTYPE(selectobj)) {
	 case LABEL:
	    sclab = SELTOLABEL(selectobj);
	    oldsize = sclab->scale;
	    sclab->scale = newscale;
	    break;
	 case OBJINST:
	    scinst = SELTOOBJINST(selectobj);
	    oldsize = scinst->scale;
	    scinst->scale = newscale;
	    break;
	 case GRAPHIC:
	    scgraph = SELTOGRAPHIC(selectobj);
	    oldsize = scgraph->scale;
	    scgraph->scale = newscale;
	    break;
      }
      register_for_undo(XCF_Rescale, UNDO_MORE, areawin->topinstance,
             SELTOGENERIC(selectobj), (double)oldsize);
   }
}

/*-------------------------------------------------*/
/* Edit an element in an element-dependent fashion */
/*-------------------------------------------------*/

void edit(int x, int y)
{
   short *selectobj, saveselects;
   bool preselected = false;

   if (areawin->selects == 0) {
       editmode savemode = eventmode;
       eventmode = PENDING_MODE;
       selectobj = select_element(ALL_TYPES);
       eventmode = savemode;
   }
   else {
       preselected = true;
       selectobj = areawin->selectlist;
   }
   if (areawin->selects == 0)
      return;
   else if (areawin->selects != 1) { /* Multiple object edit */
      int seltype, selnum;
      short *selectlist, selrefno;

      /* Find the closest part to use as a reference */
      selnum = areawin->selects;
      selectlist = areawin->selectlist;
      areawin->selects = 0;
      areawin->selectlist = NULL;
      selectobj = select_element(ALL_TYPES);
      if (selectobj != NULL)
         selrefno = *selectobj;
      else
	 selrefno = -1;
      freeselects();
      areawin->selects = selnum;
      areawin->selectlist = selectlist;
      for (selectobj = areawin->selectlist; selectobj < areawin->selectlist
		+ areawin->selects; selectobj++) {
	 if (*selectobj == selrefno) break;
      }
      if (selectobj == areawin->selectlist + areawin->selects) {
         Wprintf("Put cursor close to the reference element.");
	 return;
      }
      /* Shuffle the reference element to the beginning of the select list */
      *selectobj = *(areawin->selectlist);
      *(areawin->selectlist) = selrefno;
      selectobj = areawin->selectlist;
   }

   XDefineCursor (areawin->viewport, EDCURSOR);
   switch(SELECTTYPE(selectobj)) {
       case LABEL: {
	 labelptr *lastlabel = (labelptr *)EDITPART;
	 short curfont;
	 XPoint tmppt;
	 TextExtents tmpext;
	
	 /* save the old string, including parameters */
	 register_for_undo(XCF_Edit, UNDO_MORE, areawin->topinstance,
                        *lastlabel);

	 /* fill any NULL instance parameters with the default value */
	 copyparams(areawin->topinstance, areawin->topinstance);

	 /* place text cursor line at point nearest the cursor */
	 /* unless textend is set. . .			       */
 
	 if (areawin->textend == 0) {
	    window_to_user(x, y, &areawin->save);
	    InvTransformPoints(&areawin->save, &tmppt, 1, (*lastlabel)->position,
		(*lastlabel)->scale, (*lastlabel)->rotation);
            tmpext = ULength(NULL, *lastlabel, areawin->topinstance, 0.0, 0, NULL);
	    tmppt.x += ((*lastlabel)->justify & NOTLEFT ?
		((*lastlabel)->justify & RIGHT ? tmpext.width : tmpext.width >> 1) : 0);
	    tmppt.y += ((*lastlabel)->justify & NOTBOTTOM ?
		((*lastlabel)->justify & TOP ? tmpext.ascent :
		(tmpext.ascent + tmpext.base) >> 1) : tmpext.base);
	    if ((*lastlabel)->pin)
	       pinadjust((*lastlabel)->justify, &tmppt.x, NULL, -1);
            tmpext = ULength(NULL, *lastlabel, areawin->topinstance, 0.0, 0, &tmppt);
	    areawin->textpos = tmpext.width;
	 }

	 /* find current font */

	 curfont = findcurfont(areawin->textpos, (*lastlabel)->string,
			areawin->topinstance);

	 /* change menu buttons accordingly */

	 setfontmarks(curfont, (*lastlabel)->justify);

         Context ctx(NULL);
         tmpext = ULength(&ctx, *lastlabel, areawin->topinstance,
			(*lastlabel)->scale, 0, NULL);

         areawin->origin.x = (*lastlabel)->position.x + ((*lastlabel)->
	    justify & NOTLEFT ? ((*lastlabel)->justify & RIGHT ? 0 : tmpext.width
	    / 2) : tmpext.width);
         areawin->origin.y = (*lastlabel)->position.y + ((*lastlabel)->
	    justify & NOTBOTTOM ? ((*lastlabel)->justify & TOP ? -tmpext.ascent :
	    -(tmpext.ascent + tmpext.base) / 2) : -tmpext.base);
	 if ((*lastlabel)->pin)
	    pinadjust((*lastlabel)->justify, &(areawin->origin.x),
		&(areawin->origin.y), 1);
	
	 if (eventmode == CATALOG_MODE) {
            /* CATTEXT_MODE may show an otherwise hidden library namespace */
            eventmode = CATTEXT_MODE;
	 }
	 else eventmode = ETEXT_MODE;

         XDefineCursor(areawin->viewport, TEXTPTR);

	 /* write the text at the bottom */

	 charreport(*lastlabel);

      } break;

      case POLYGON: case ARC: case SPLINE: case PATH:
	 window_to_user(x, y, &areawin->save);
         /* Redraw without drawing selections */
         saveselects = areawin->selects;
         selectobj = areawin->selectlist;
         areawin->selects = 0;
         areawin->selectlist = NULL;
         areawin->selectlist = selectobj;
         areawin->selects = saveselects;

         pathedit(*(EDITPART));
	 break;
      case OBJINST: case GRAPHIC:
         if (areawin->selects == 1)
            unselect_all();
         break;
   }
   areawin->update();
}

/*----------------------------------------------------------------------*/
/* edit() routine for path-type elements (polygons, splines, arcs, and	*/
/* paths)								*/
/*----------------------------------------------------------------------*/

void pathedit(genericptr editpart)
{
   splineptr lastspline = NULL;
   pathptr lastpath;
   polyptr lastpoly = NULL;
   arcptr lastarc;
   XPoint *savept;
   short *eselect;
   int cycle;
   bool havecycle;

   /* Find and set constrained edit points on all elements.  Register	*/
   /* each element with the undo mechanism.				*/

   for (eselect = areawin->selectlist; eselect < areawin->selectlist +
                        areawin->selects; eselect++) {
      switch (SELECTTYPE(eselect)) {
         case POLYGON:
            findconstrained(SELTOPOLY(eselect));
            /* fall through */
         default:
            register_for_undo(XCF_Edit, UNDO_MORE, areawin->topinstance,
                        SELTOGENERIC(eselect));
            break;
      }
   }

   switch(ELEMENTTYPE(editpart)) {
      case PATH: {
	 genericptr *ggen, *savegen = NULL;
	 int mincycle, dist, mindist = 1e6;

         lastpath = (pathptr)editpart;
         havecycle = checkcycle(editpart, 0) >= 0;

	 /* determine which point of the path is closest to the cursor */
         for (ggen = lastpath->begin(); ggen < lastpath->end();
		ggen++) {
	    switch (ELEMENTTYPE(*ggen)) {
	       case POLYGON:
		  lastpoly = TOPOLY(ggen);
		  cycle = closepoint(lastpoly, &areawin->save);
		  dist = wirelength(lastpoly->points + cycle, &areawin->save);
		  break;
	       case SPLINE:
		  lastspline = TOSPLINE(ggen);
		  cycle =  (wirelength(&lastspline->ctrl[0],
			&areawin->save) < wirelength(&lastspline->ctrl[3],
			&areawin->save)) ? 0 : 3;
		  dist = wirelength(&lastspline->ctrl[cycle], &areawin->save);
		  break;
	    }
	    if (dist < mindist) {
	       mindist = dist;
	       mincycle = cycle;
	       savegen = ggen;
	    }
	 }
	 if (savegen == NULL) return;	/* something went terribly wrong */
	 switch (ELEMENTTYPE(*savegen)) {
	    case POLYGON:
	       lastpoly = TOPOLY(savegen);
               addcycle(savegen, mincycle, 0);
               savept = lastpoly->points + mincycle;
               makerefcycle(lastpoly->cycle, mincycle);
               findconstrained(lastpoly);
	       break;
	    case SPLINE:
	       lastspline = TOSPLINE(savegen);
               addcycle(savegen, mincycle, 0);
               savept = &lastspline->ctrl[mincycle];
               makerefcycle(lastspline->cycle, mincycle);
	       break;
	 }
         updatepath(lastpath);
         if (!havecycle)
            register_for_undo(XCF_Edit, UNDO_MORE, areawin->topinstance,
                        lastpath);
	 patheditpush(lastpath);
	 areawin->origin = areawin->save;
         checkwarp(savept);

         xcAddEventHandler(areawin->viewport, PointerMotionMask, false,
            (XtEventHandler)trackelement, NULL);
	 eventmode = EPATH_MODE;
         printpos(*savept);

      } break;
      case POLYGON: {

         lastpoly = (polyptr)editpart;

         /* Determine which point of polygon is closest to cursor */
         cycle = closepoint(lastpoly, &areawin->save);
         havecycle = lastpoly->cycle != NULL;
         addcycle(&editpart, cycle, 0);
         savept = lastpoly->points + cycle;
         makerefcycle(lastpoly->cycle, cycle);
         if (!havecycle) {
            findconstrained(lastpoly);
            register_for_undo(XCF_Edit, UNDO_MORE, areawin->topinstance,
                        lastpoly);
        }

	 /* Push onto the editstack */
	 polyeditpush(lastpoly);

	 /* remember our postion for pointer restore */
	 areawin->origin = areawin->save;

	 checkwarp(savept);

         xcAddEventHandler(areawin->viewport, PointerMotionMask, false,
            (XtEventHandler)trackelement, NULL);
	 eventmode = EPOLY_MODE;
	 printeditbindings();
         printpos(*savept);
      } break;
      case SPLINE: {
	 XPoint *curpt;

         lastspline = (splineptr)editpart;

	 /* find which point is closest to the cursor */

         cycle =  (wirelength(&lastspline->ctrl[0],
              &areawin->save) < wirelength(&lastspline->ctrl[3],
              &areawin->save)) ? 0 : 3;
         havecycle = lastspline->cycle != NULL;
         addcycle(&editpart, cycle, 0);
         makerefcycle(lastspline->cycle, cycle);
         curpt = &lastspline->ctrl[cycle];
         if (!havecycle)
            register_for_undo(XCF_Edit, UNDO_MORE, areawin->topinstance,
                        lastspline);

	 /* Push onto the editstack */
	 splineeditpush(lastspline);

	 /* remember our postion for pointer restore */
	 areawin->origin = areawin->save;

         checkwarp(curpt);

         areawin->update();
         xcAddEventHandler(areawin->viewport, PointerMotionMask, false,
               (XtEventHandler)trackelement, NULL);
	 eventmode = ESPLINE_MODE;
      } break;
      case ARC: {
	 XPoint curpt;
	 float tmpratio, tlen;

         lastarc = (arcptr)editpart;

	 /* find a part of the arc close to the pointer */

	 tlen = (float)wirelength(&areawin->save, &(lastarc->position));
	 tmpratio = (float)(abs(lastarc->radius)) / tlen;
	 curpt.x = lastarc->position.x + tmpratio * (areawin->save.x
	     - lastarc->position.x);
	 tmpratio = (float)lastarc->yaxis / tlen;
	 curpt.y = lastarc->position.y + tmpratio * (areawin->save.y
	     - lastarc->position.y);
         addcycle(&editpart, 0, 0);
	 saveratio = (double)(lastarc->yaxis) / (double)(abs(lastarc->radius));
	 
	 /* Push onto the editstack */
	 arceditpush(lastarc);
         areawin->origin = areawin->save;

	 checkwarp(&curpt);

         areawin->update();
         areawin->save = curpt;	/* for redrawing dotted edit line */
         xcAddEventHandler(areawin->viewport, PointerMotionMask, false,
	    (XtEventHandler)trackarc, NULL);
	 eventmode = EARC_MODE;
         printpos(curpt);
      } break;
   }
}

/*-------------------------------------------------*/
/* Raise/Lower an object			   */
/*-------------------------------------------------*/

void xc_raise(short *selectno)
{
   genericptr *raiseobj, *genobj, temp;

   raiseobj = topobject->begin() + *selectno;
   temp = *raiseobj;
   for (genobj = topobject->begin() + *selectno; genobj <
                topobject->end() - 1; genobj++)
      *genobj = *(genobj + 1);
   *(topobject->end() - 1) = temp;
   *selectno = topobject->parts - 1;
}

/*-------------------------------------------------*/

void xc_lower(short *selectno)
{
   genericptr *lowerobj, *genobj, temp;

   lowerobj = topobject->begin() + *selectno;
   temp = *lowerobj;
   for (genobj = topobject->begin() + *selectno;
                genobj > topobject->begin(); genobj--)
      *genobj = *(genobj - 1);
   *genobj = temp;
   *selectno = 0;
}

/*------------------------------------------------------*/
/* Generate a virtual copy of an object instance in the	*/ 
/* user library.  This is like the library virtual copy	*/
/* except that it allows the user to generate a library	*/
/* copy of an existing instance, without having to make	*/
/* a copy of the master library instance and edit it.	*/
/* copyvirtual() also allows the library virtual	*/
/* instance to take on a specific rotation or flip	*/
/* value, which cannot be done with the library virtual	*/
/* copy function.					*/
/*------------------------------------------------------*/

void copyvirtual()
{
   short *selectno, created = 0;
   objinstptr vcpobj, libinst;

   for (selectno = areawin->selectlist; selectno < areawin->selectlist +
		areawin->selects; selectno++) {
      if (SELECTTYPE(selectno) == OBJINST) {
	 vcpobj = SELTOOBJINST(selectno);
	 libinst = addtoinstlist(USERLIB - LIBRARY, vcpobj->thisobject, true);
         *libinst = *vcpobj;
	 created++;
      }
   }
   if (created == 0) {
      Wprintf("No object instances selected for virtual copy!");
   }
   else {
      unselect_all();
      composelib(USERLIB);
   }
}

/*------------------------------------------------------*/
/* Exchange the list position (drawing order) of two	*/
/* elements, or move the position of one element to the	*/
/* top or bottom.					*/
/*------------------------------------------------------*/

void exchange()
{
   short *selectno;
   genericptr *exchobj, *exchobj2, temp;
   bool preselected;

   preselected = (areawin->selects > 0) ? true : false;
   if (!checkselect(ALL_TYPES)) {
      Wprintf("Select 1 or 2 objects");
      return;
   }

   selectno = areawin->selectlist;
   if (areawin->selects == 1) {  /* lower if on top; raise otherwise */
      if (*selectno == topobject->parts - 1)
	 xc_lower(selectno);
      else
	 xc_raise(selectno);
   }
   else {  /* exchange the two objects */
      exchobj = topobject->begin() + *selectno;
      exchobj2 = topobject->begin() + *(selectno + 1);

      temp = *exchobj;
      *exchobj = *exchobj2;
      *exchobj2 = temp;
   }

   incr_changes(topobject);
   if (!preselected)
      clearselects();
   areawin->update();
}

/*--------------------------------------------------------*/
/* Flip an element horizontally (POLYGON, ARC, or SPLINE) */
/*--------------------------------------------------------*/

void elhflip(genericptr *genobj, short x)
{
   switch(ELEMENTTYPE(*genobj)) {
      case POLYGON:{
	 polyptr flippoly = TOPOLY(genobj);
         XPoint* ppoint;
         for (ppoint = flippoly->points.begin(); ppoint < flippoly->points.end(); ppoint++)
	    ppoint->x = (x << 1) - ppoint->x;	
	 }break;

      case ARC:{
	 arcptr fliparc = TOARC(genobj);
	 float tmpang = 180 - fliparc->angle1;
	 fliparc->angle1 = 180 - fliparc->angle2;
	 fliparc->angle2 = tmpang;
	 if (fliparc->angle2 < 0) {
	    fliparc->angle1 += 360;
	    fliparc->angle2 += 360;
	 }
	 fliparc->radius = -fliparc->radius;
	 fliparc->position.x = (x << 1) - fliparc->position.x;
         fliparc->calc();
	 }break;

      case SPLINE:{
	 splineptr flipspline = TOSPLINE(genobj);
	 int i;
	 for (i = 0; i < 4; i++)
	    flipspline->ctrl[i].x = (x << 1) - flipspline->ctrl[i].x;
         flipspline->calc();
	 }break;
   }
}

/*--------------------------------------------------------*/
/* Flip an element vertically (POLYGON, ARC, or SPLINE)   */
/*--------------------------------------------------------*/

void elvflip(genericptr *genobj, short y)
{
   switch(ELEMENTTYPE(*genobj)) {

      case POLYGON:{
	 polyptr flippoly = TOPOLY(genobj);
         XPoint* ppoint;
         for (ppoint = flippoly->points.begin(); ppoint < flippoly->points.end(); ppoint++)
            ppoint->y = (y << 1) - ppoint->y;
	 }break;

      case ARC:{
	 arcptr fliparc = TOARC(genobj);
	 float tmpang = 360 - fliparc->angle1;
	 fliparc->angle1 = 360 - fliparc->angle2;
	 fliparc->angle2 = tmpang;
	 if (fliparc->angle1 >= 360) {
	    fliparc->angle1 -= 360;
	    fliparc->angle2 -= 360;
	 }
	 fliparc->radius = -fliparc->radius;
	 fliparc->position.y = (y << 1) - fliparc->position.y;
         fliparc->calc();
	 }break;

      case SPLINE:{
	 splineptr flipspline = TOSPLINE(genobj);
	 int i;
	 for (i = 0; i < 4; i++)
	    flipspline->ctrl[i].y = (y << 1) - flipspline->ctrl[i].y;
         flipspline->calc();
	 }break;
   }
}

/*------------------------------------------------------*/
/* Horizontally flip an element				*/
/*------------------------------------------------------*/

void elementflip(XPoint *position)
{
   short *selectobj;
   bool single = false;
   bool preselected;

   preselected = (areawin->selects > 0) ? true : false;
   if (!checkselect(ALL_TYPES)) return;
   if (areawin->selects == 1) single = true;

   if (eventmode != COPY_MODE)
      register_for_undo(XCF_Flip_X, UNDO_MORE, areawin->topinstance,
		(eventmode == MOVE_MODE) ? &areawin->origin : position);

   for (selectobj = areawin->selectlist; selectobj < areawin->selectlist
	+ areawin->selects; selectobj++) {

      switch(SELECTTYPE(selectobj)) {
	 case LABEL:{
	    labelptr fliplab = SELTOLABEL(selectobj);
	    if ((fliplab->justify & (RIGHT | NOTLEFT)) != NOTLEFT)
	       fliplab->justify ^= (RIGHT | NOTLEFT);
	    if (!single)
	       fliplab->position.x = (position->x << 1) - fliplab->position.x;
	    }break;
	 case GRAPHIC:{
	    graphicptr flipg = SELTOGRAPHIC(selectobj);
	    flipg->scale = -flipg->scale;
	    flipg->valid = false;
	    if (!single)
	       flipg->position.x = (position->x << 1) - flipg->position.x;
	    }break;
	 case OBJINST:{
            objinstptr flipobj = SELTOOBJINST(selectobj);
	    if (is_library(topobject) >= 0 && !is_virtual(flipobj)) break;
   	    flipobj->scale = -flipobj->scale;
	    if (!single)
	       flipobj->position.x = (position->x << 1) - flipobj->position.x;
	    }break;
	 case POLYGON: case ARC: case SPLINE:
            elhflip(topobject->begin() + *selectobj, position->x);
	    break;
	 case PATH:{
	    genericptr *genpart;
	    pathptr flippath = SELTOPATH(selectobj);

            for (genpart = 0; flippath->values(genpart); )
	       elhflip(genpart, position->x);
	    }break;
      }
   }
   select_invalidate_netlist();
   if (eventmode == NORMAL_MODE || eventmode == CATALOG_MODE)
      if (!preselected)
         unselect_all();

   if (eventmode == NORMAL_MODE)
      incr_changes(topobject);
   if (eventmode == CATALOG_MODE) {
      int libnum;
      if ((libnum = is_library(topobject)) >= 0) {
	 composelib(libnum + LIBRARY);
         areawin->update();
      }
   }
   else {
      pwriteback(areawin->topinstance);
      calcbbox(areawin->topinstance);
   }
}

/*----------------------------------------------*/
/* Vertically flip an element			*/
/*----------------------------------------------*/

void elementvflip(XPoint *position)
{
   short *selectobj;
   bool preselected;
   bool single = false;

   preselected = (areawin->selects > 0) ? true : false;
   if (!checkselect(ALL_TYPES)) return;
   if (areawin->selects == 1) single = true;

   if (eventmode != COPY_MODE)
      register_for_undo(XCF_Flip_Y, UNDO_MORE, areawin->topinstance,
		(eventmode == MOVE_MODE) ? &areawin->origin : position);

   for (selectobj = areawin->selectlist; selectobj < areawin->selectlist
	+ areawin->selects; selectobj++) {

      switch(SELECTTYPE(selectobj)) {
	 case(LABEL):{
	    labelptr fliplab = SELTOLABEL(selectobj);
	    if ((fliplab->justify & (TOP | NOTBOTTOM)) != NOTBOTTOM)
	       fliplab->justify ^= (TOP | NOTBOTTOM);
	    if (!single)
	       fliplab->position.y = (position->y << 1) - fliplab->position.y;
	    } break;
	 case(OBJINST):{
            objinstptr flipobj = SELTOOBJINST(selectobj);

	    if (is_library(topobject) >= 0 && !is_virtual(flipobj)) break;
	    flipobj->scale = -(flipobj->scale);
	    flipobj->rotation += 180;
	    while (flipobj->rotation >= 360) flipobj->rotation -= 360;
	    if (!single)
	       flipobj->position.y = (position->y << 1) - flipobj->position.y;
	    }break;
	 case(GRAPHIC):{
            graphicptr flipg = SELTOGRAPHIC(selectobj);

	    flipg->scale = -(flipg->scale);
	    flipg->rotation += 180;
	    while (flipg->rotation >= 360) flipg->rotation -= 360;
	    if (!single)
	       flipg->position.y = (position->y << 1) - flipg->position.y;
	    }break;
	 case POLYGON: case ARC: case SPLINE:
            elvflip(topobject->begin() + *selectobj, position->y);
	    break;
	 case PATH:{
	    genericptr *genpart;
	    pathptr flippath = SELTOPATH(selectobj);

            for (genpart = 0; flippath->values(genpart); )
	       elvflip(genpart, position->y);
	    }break;
      }
   }
   select_invalidate_netlist();
   if (eventmode == NORMAL_MODE || eventmode == CATALOG_MODE)
      if (!preselected)
         unselect_all();
   if (eventmode == NORMAL_MODE) {
      incr_changes(topobject);
   }
   if (eventmode == CATALOG_MODE) {
      int libnum;
      if ((libnum = is_library(topobject)) >= 0) {
         composelib(libnum + LIBRARY);
      }
   }
   else {
      pwriteback(areawin->topinstance);
      calcbbox(areawin->topinstance);
   }
   areawin->update();
}

/*----------------------------------------*/
/* ButtonPress handler during delete mode */
/*----------------------------------------*/

void deletebutton(int x, int y)
{
   if (checkselect(ALL_TYPES)) {
      standard_element_delete();
      calcbbox(areawin->topinstance);
   }
   setoptionmenu();	/* Return GUI check/radio boxes to default */
}

/*----------------------------------------------------------------------*/
/* Process of element deletion.  Remove one element from the indicated	*/
/* object.								*/
/*----------------------------------------------------------------------*/

void delete_one_element(objinstptr thisinstance, genericptr thiselement)
{
   objectptr thisobject;
   genericptr *genobj;
   bool pinchange = false;

   thisobject = thisinstance->thisobject;

   /* The netlist contains pointers to elements which no longer		*/
   /* exist on the page, so we should remove them from the netlist.	*/

   if (RemoveFromNetlist(thisobject, thiselement)) pinchange = true;
   for (genobj = 0; thisobject->values(genobj); )
      if (*genobj == thiselement)
	 break;

   if (genobj == thisobject->end()) return;

   for (genobj; thisobject->values(genobj); )
      *(genobj - 1) = *genobj;
   thisobject->take_last();

   if (pinchange) setobjecttype(thisobject);
   incr_changes(thisobject);
   calcbbox(thisinstance);
   invalidate_netlist(thisobject);
   /* freenetlist(thisobject); */
}
  
/*----------------------------------------------------------------------*/
/* Process of element deletion.  Remove everything in the selection	*/
/* list from the indicated object, and return a new object containing	*/
/* only the deleted elements.						*/
/*									*/
/* Note that if "slist" is areawin->selectlist, it is freed by this	*/
/* routine (calls freeselects()), but not otherwise.			*/
/*----------------------------------------------------------------------*/

objectptr delete_element(objinstptr thisinstance, short *slist, int selects)
{
   short *selectobj;
   objectptr delobj, thisobject;
   genericptr *genobj;
   bool pinchange = false;

   if (slist == NULL || selects == 0) return NULL;

   thisobject = thisinstance->thisobject;

   delobj = new object;

   for (selectobj = slist; selectobj < slist + selects; selectobj++) {
      genobj = thisobject->begin() + *selectobj;
      delobj->append(*genobj);

       /* The netlist contains pointers to elements which no longer	*/
       /* exist on the page, so we should remove them from the netlist.	*/

      if (RemoveFromNetlist(thisobject, *genobj)) pinchange = true;
      for (genobj; thisobject->values(genobj); )
	 *(genobj - 1) = *genobj;
      thisobject->take_last();
      reviseselect(slist, selects, selectobj);
   }
   if (pinchange) setobjecttype(thisobject);

   if (slist == areawin->selectlist)
      freeselects();

   calcbbox(thisinstance);
   /* freenetlist(thisobject); */

   /// \todo this may be gratuituous, it used to depend on drawmode
   areawin->update();
   return delobj;
}
  
/*----------------------------------------------------------------------*/
/* Wrapper for delete_element().  Remember this deletion for the undo	*/
/* function.								*/
/*----------------------------------------------------------------------*/

void standard_element_delete()
{
   objectptr delobj;

#if 0
   // 3.7.8
   register_for_undo(XCF_Select, UNDO_MORE, areawin->topinstance,
		areawin->selectlist, areawin->selects);
#endif
   select_invalidate_netlist();
   delobj = delete_element(areawin->topinstance, areawin->selectlist,
        areawin->selects);
   register_for_undo(XCF_Delete, UNDO_DONE, areawin->topinstance,
                delobj);
   incr_changes(topobject);  /* Treat as one change */
}

/*----------------------------------------------------------------------*/
/* Another wrapper for delete_element(), in which we do not save the	*/
/* deletion as an undo event.  However, the returned object is saved	*/
/* on areawin->editstack, so that the objects can be grabbed.  This	*/
/* allows objects to be carried across pages and through the hierarchy.	*/
/*----------------------------------------------------------------------*/

void delete_for_xfer(short *slist, int selects)
{
   if (selects > 0) {
      delete areawin->editstack;
      areawin->editstack = delete_element(areawin->topinstance,
                slist, selects);
   }
}

/*----------------------------------------------------------------------*/
/* Yet another wrapper for delete_element(), in which we destroy the	*/
/* object returned and free all associated memory.			*/
/*----------------------------------------------------------------------*/

void delete_noundo()
{
   objectptr delobj;

   select_invalidate_netlist();
   delobj = delete_element(areawin->topinstance, areawin->selectlist,
        areawin->selects);

   delete delobj;
}

/*----------------------------------------------------------------------*/
/* Undelete last deleted elements and return a selectlist of the	*/
/* elements.  If "olist" is non-NULL, then the undeleted elements are	*/
/* placed into the object of thisinstance in the order given by olist,	*/
/* and a copy of olist is returned.  If "olist" is NULL, then the	*/
/* undeleted elements are placed at the end of thisinstance->thisobject	*/
/* ->plist, and a new selection list is returned.  If "olist" is non-	*/
/* NULL, then the size of olist had better match the number of objects	*/
/* in delobj!  It is up to the calling routine to check this.		*/
/*----------------------------------------------------------------------*/

short *xc_undelete(objinstptr thisinstance, objectptr delobj, 	short *olist)
{
   objectptr  thisobject;
   genericptr *regen;
   short      *slist, count, i;

   thisobject = thisinstance->thisobject;
   slist = (short *)malloc(delobj->parts * sizeof(short));
   count = 0;

   for (regen = 0; delobj->values(regen); ) {
      thisobject->temp_append();
      if (olist == NULL) {
         *(slist + count) = thisobject->parts;
         *(ENDPART) = *regen;
      }
      else {
         *(slist + count) = *(olist + count);
	 for (i = thisobject->parts; i > *(olist + count); i--)
            *(thisobject->begin() + i) = *(thisobject->begin() + i - 1);
         *(thisobject->begin() + i) = *regen;
      }
      thisobject->parts++;
      count++;

      /* If the element has passed parameters (eparam), then we have to */
      /* check if the key exists in the new parent object.  If not,	*/
      /* delete the parameter.						*/

      if ((*regen)->passed) {
	 eparamptr nextepp, epp = (*regen)->passed;
	 while (epp != NULL) {
	    nextepp = epp->next;
	    if (!match_param(thisobject, epp->key)) {
	       if (epp == (*regen)->passed) (*regen)->passed = nextepp;
	       free_element_param(*regen, epp);
	    }
	    epp = nextepp;
	 }
      }

      /* Likewise, string parameters must be checked in labels because	*/
      /* they act like element parameters.				*/

      if (IS_LABEL(*regen)) {
	 labelptr glab = TOLABEL(regen);
	 stringpart *gstr, *lastpart = NULL;
	 for (gstr = glab->string; gstr != NULL; gstr = lastpart->nextpart) {
	    if (gstr->type == PARAM_START) {
	       if (!match_param(thisobject, gstr->data.string)) {
		  free(gstr->data.string);
		  if (lastpart)
		     lastpart->nextpart = gstr->nextpart;
		  else
		     glab->string = gstr->nextpart;
		  free(gstr);
		  gstr = (lastpart) ? lastpart : glab->string;
	       }
	    }
	    lastpart = gstr;
	 }
      }
   }
   incr_changes(thisobject);	/* treat as one change */
   calcbbox(thisinstance);

   /* flush the delete buffer but don't delete the elements */
   delobj->clear_nodelete();

   if (delobj != areawin->editstack) delete delobj;

   return slist;
}

/*----------------------------*/
/* select save object handler */
/*----------------------------*/

void printname(objectptr curobject)
{
#if 0
   char editstr[10], pagestr[10];
   short ispage;

#ifndef TCL_WRAPPER
   Arg	wargs[1];
   Dimension swidth, swidth2, sarea;
   char tmpname[256];
   char *sptr = tmpname;
#endif
   
   /* print full string to make message widget proper size */

   strcpy(editstr, ((ispage = is_page(curobject)) >= 0) ? "Editing: " : "");
   strcpy(editstr, (is_library(curobject) >= 0) ? "Library: " : "");
   if (strstr(curobject->name, "Page") == NULL && (ispage >= 0))
      sprintf(pagestr, " (p. %d)", areawin->page + 1);
   else
      pagestr[0] = '\0';
   W2printf("%s%s%s", editstr, curobject->name, pagestr); 


   XtSetArg(wargs[0], XtNwidth, &sarea);
   XtGetValues(message2, wargs, 1); 
   
   /* in the remote case that the string is longer than message widget,    */
   /* truncate the string and denote the truncation with an ellipsis (...) */

   strcpy(tmpname, curobject->name);
   swidth2 = XTextWidth(appdata.xcfont, editstr, strlen(editstr));
   swidth = XTextWidth(appdata.xcfont, tmpname, strlen(tmpname));

   if (sptr && (swidth + swidth2) > sarea) {
      char *ip;
      while ((swidth + swidth2) > sarea) {
         sptr++;
         swidth = XTextWidth(appdata.xcfont, sptr, strlen(sptr));
      }
      for(ip = sptr; ip < sptr + 3 && *ip != '\0'; ip++) *ip = '.';

      W2printf("Editing: %s", sptr); 
   }
#endif
}

/*--------------------------------------------------------------*/
/* Make sure that a string does not conflict with postscript	*/
/* names (commands and definitions found in xcircps2.pro).	*/
/*								*/
/* Return value is NULL if no change was made.  Otherwise, the	*/
/* return value is an allocated string.				*/
/*--------------------------------------------------------------*/

char *checkvalidname(char *teststring, objectptr newobj)
{
   int i, j;
   short dupl;  /* flag a duplicate string */
   objectptr *libobj;
   char *sptr, *pptr;
   aliasptr aref;
   slistptr sref;

   /* Try not to allocate memory unless necessary */

   sptr = teststring;
   pptr = sptr;

   do {
      dupl = 0;
      if (newobj != NULL) {
         for (i = 0; i < xobjs.numlibs; i++) {
	    for (j = 0; j < xobjs.userlibs[i].number; j++) {
	       libobj = xobjs.userlibs[i].library + j;

	       if (*libobj == newobj) continue;
               if (!strcmp(pptr, (*libobj)->name)) { 

		  /* Instead of the old method of prepending an		*/
		  /* underscore to the name, we now change the		*/
		  /* technology, not the name, to avoid a conflict.  	*/
		  /* If there's no technology, then we add one called	*/
		  /* "unref".  Otherwise, the technology gets the	*/
		  /* leading underscore.				*/

		  if (strstr(pptr, "::") == NULL) {
                     pptr = (char *)malloc(strlen((*libobj)->name) + 2);
		     sprintf(pptr, "unref::%s", (*libobj)->name);
		  }
		  else {
		     if (pptr == sptr)
                        pptr = (char *)malloc(strlen((*libobj)->name) + 2);
		     else
                        pptr = (char *)realloc(pptr, strlen((*libobj)->name) + 2);
	             sprintf(pptr, "_%s", (*libobj)->name);
		  }
	          dupl = 1;
	       }
	    }
	 }

         /* If we're in the middle of a file load, the name cannot be	*/
         /* the same as an alias, either.				*/
	
         if (aliastop != NULL) {
	    for (aref = aliastop; aref != NULL; aref = aref->next) {
	       for (sref = aref->aliases; sref != NULL; sref = sref->next) {
	          if (!strcmp(pptr, sref->alias)) {
		     if (pptr == sptr)
                        pptr = (char *)malloc(strlen(sref->alias) + 2);
		     else
                        pptr = (char *)realloc(pptr, strlen(sref->alias) + 2);
	             sprintf(pptr, "_%s", sref->alias);
	             dupl = 1;
		  }
	       }
	    }
         }
      }

   } while (dupl == 1);

   return (pptr == sptr) ? NULL : pptr;
}

/*--------------------------------------------------------------*/
/* Make sure that name for new object does not conflict with	*/
/* existing object definitions					*/
/*								*/
/* Return:  true if name required change, false otherwise	*/
/*--------------------------------------------------------------*/

bool checkname(objectptr newobj)
{
   char *pptr;

   /* Check for empty string */
   if (strlen(newobj->name) == 0) {
      Wprintf("Blank object name changed to default");
      sprintf(newobj->name, "user_object");
   }

   pptr = checkvalidname(newobj->name, newobj);

   /* Change name if necessary to avoid naming conflicts */
   if (pptr == NULL) {
      Wprintf("Created new object %s", newobj->name);
      return false;
   }
   else {
      Wprintf("Changed name from %s to %s to avoid conflict with "
			"existing object", newobj->name, pptr);
      strncpy(newobj->name, pptr, 79);
      free(pptr);
   }
   return true;
}

/*------------------------------------------------------------*/
/* Find the object "dot" in the builtin library, if it exists */
/*------------------------------------------------------------*/

objectptr finddot()
{
   objectptr dotobj;
   short i, j;
   char *name, *pptr;

   for (i = 0; i < xobjs.numlibs; i++) {
      for (j = 0; j < xobjs.userlibs[i].number; j++) {
	 dotobj = *(xobjs.userlibs[i].library + j);
	 name = dotobj->name;
         if ((pptr = strstr(name, "::")) != NULL) name = pptr + 2;
         if (!strcmp(name, "dot")) {
            return dotobj;
         }
      }
   }
   return (objectptr)NULL;
}

/*--------------------------------------*/
/* Add value origin to all points	*/
/*--------------------------------------*/

void movepoints(genericptr *ssgen, const XPoint & delta)
{
   switch(ELEMENTTYPE(*ssgen)) {
         case ARC:{
            fpointlist sspoints;
            TOARC(ssgen)->position += delta;
            for (sspoints = TOARC(ssgen)->points; sspoints < TOARC(ssgen)->points +
	          TOARC(ssgen)->number; sspoints++) {
               *sspoints += delta;
            }
	    }break;

         case POLYGON:
            TOPOLY(ssgen)->points += delta;
            break;

         case SPLINE:{
            fpointlist sspoints;
            short j;
            for (sspoints = TOSPLINE(ssgen)->points; sspoints <
		  TOSPLINE(ssgen)->points + INTSEGS; sspoints++) {
               *sspoints += delta;
            }
            for (j = 0; j < 4; j++) {
               TOSPLINE(ssgen)->ctrl[j] += delta;
            }
            }break;
       case OBJINST:
          TOOBJINST(ssgen)->position += delta;
          break;
       case GRAPHIC:
          TOGRAPHIC(ssgen)->position += delta;
          break;
       case LABEL:
          TOLABEL(ssgen)->position += delta;
          break;
   }
}

/*----------------------------------------------------------------------*/
/* Add value origin to all edited points, according to edit flags	*/
/*----------------------------------------------------------------------*/

void editpoints(genericptr *ssgen, const XPoint & delta)
{
   pathptr editpath;
   polyptr editpoly;
   splineptr editspline;
   short cycle, cpoint;
   pointselect *cptr;
   XPoint *curpt;
   genericptr *ggen;

   switch(ELEMENTTYPE(*ssgen)) {
      case POLYGON:
         editpoly = TOPOLY(ssgen);
         if (editpoly->cycle == NULL)
            movepoints(ssgen, delta);
         else {
            for (cptr = editpoly->cycle;; cptr++) {
               cycle = cptr->number;
               curpt = editpoly->points + cycle;
               if (cptr->flags & EDITX) curpt->x += delta.x;
               if (cptr->flags & EDITY) curpt->y += delta.y;
               if (cptr->flags & LASTENTRY) break;
            }
         }
         exprsub(*ssgen);
         break;

      case SPLINE:
         editspline = TOSPLINE(ssgen);
         if (editspline->cycle == NULL)
            movepoints(ssgen, delta);
         else {
            for (cptr = editspline->cycle;; cptr++) {
               cycle = cptr->number;
               if (cycle == 0 || cycle == 3) {
                  cpoint = (cycle == 0) ? 1 : 2;
                  if (cptr->flags & EDITX) editspline->ctrl[cpoint].x += delta.x;
                  if (cptr->flags & EDITY) editspline->ctrl[cpoint].y += delta.y;
               }
               if (cptr->flags & EDITX) editspline->ctrl[cycle].x += delta.x;
               if (cptr->flags & EDITY) editspline->ctrl[cycle].y += delta.y;
               if (cptr->flags & LASTENTRY) break;
            }
         }
         exprsub(*ssgen);
         editspline->calc();
         break;

      case PATH:
         editpath = TOPATH(ssgen);
         if (checkcycle(*ssgen, 0) < 0) {
            for (ggen = 0; editpath->values(ggen); )
               movepoints(ggen, delta);
         }
         else {
             for (ggen = 0; editpath->values(ggen); ) {
               if (checkcycle(*ggen, 0) >= 0)
                  editpoints(ggen, delta);
            }
         }
         break;

      default:
         movepoints(ssgen, delta);
         exprsub(*ssgen);
         break;
   }
}

#ifndef TCL_WRAPPER

void xlib_makeobject(QAction*, const QString & str, void*)
{
   domakeobject(-1, str.toLocal8Bit().data(), false);
}

#endif /* !TCL_WRAPPER */

/*--------------------------------------------------------------*/
/* Set the name for a new user-defined object and make the	*/
/* object.  If "forceempty" is true, we allow creation of a new	*/
/* object with no elements (normally would be used only from a	*/
/* script, where an object is being constructed automatically).	*/
/*--------------------------------------------------------------*/

objinstptr domakeobject(int libnum, char *name, bool forceempty)
{
   objectptr *newobj;
   objinstptr *newinst;
   genericptr *ssgen;
   oparamptr ops, newop;
   eparamptr epp, newepp;
   stringpart *sptr;
   XPoint origin;
   short loclibnum = libnum;

   if (libnum == -1) loclibnum = USERLIB - LIBRARY;

   /* make room for new entry in library list */

   xobjs.userlibs[loclibnum].library =
        (objectptr *)realloc(xobjs.userlibs[loclibnum].library,
	(xobjs.userlibs[loclibnum].number + 1) * sizeof(objectptr));

   newobj = xobjs.userlibs[loclibnum].library + xobjs.userlibs[loclibnum].number;

   *newobj = delete_element(areawin->topinstance, areawin->selectlist,
                        areawin->selects);

   if (*newobj == NULL) {
      objectptr initobj;

      if (!forceempty) return NULL;

      /* Create a new (empty) object */

      initobj = new object;
      *newobj = initobj;
   }

   invalidate_netlist(topobject);
   xobjs.userlibs[loclibnum].number++;

   /* Create the instance of this object so we can compute a bounding box */

   newinst = topobject->append(new objinst(*newobj, 0, 0));
   calcbbox(*newinst);

   /* find closest snap point to bbox center and make this the obj. center */

   if (areawin->center) {
      origin.x = (*newobj)->bbox.lowerleft.x + (*newobj)->bbox.width / 2;
      origin.y = (*newobj)->bbox.lowerleft.y + (*newobj)->bbox.height / 2;
   }
   else {
      origin.x = origin.y = 0;
   }
   u2u_snap(origin);
   **newinst = objinst(*newobj, origin.x, origin.y);

   for (ssgen = 0; (*newobj)->values(ssgen); ) {
      switch(ELEMENTTYPE(*ssgen)) {

	 case(OBJINST):
            TOOBJINST(ssgen)->position.x -= origin.x;
            TOOBJINST(ssgen)->position.y -= origin.y;
	    break;

	 case(GRAPHIC):
            TOGRAPHIC(ssgen)->position.x -= origin.x;
            TOGRAPHIC(ssgen)->position.y -= origin.y;
	    break;

	 case(LABEL):
            TOLABEL(ssgen)->position.x -= origin.x;
            TOLABEL(ssgen)->position.y -= origin.y;
	    break;

	 case(PATH):{
	    genericptr *pathlist;
            for (pathlist = 0; TOPATH(ssgen)->values(pathlist); ) {
               movepoints(pathlist, -origin);
	    }
	    }break;

	 default:
            movepoints(ssgen, -origin);
	    break;
      }
   }

   for (ssgen = 0; (*newobj)->values(ssgen); ) {
      for (epp = (*ssgen)->passed; epp != NULL; epp = epp->next) {
	 ops = match_param(topobject, epp->key);
	 newop = copyparameter(ops);
	 newop->next = (*newobj)->params;
	 (*newobj)->params = newop;

	 /* Generate an indirect parameter reference from child to parent */
	 newepp = make_new_eparam(epp->key);
	 newepp->flags |= P_INDIRECT;
	 newepp->pdata.refkey = strdup(epp->key);
	 newepp->next = (*newinst)->passed;
	 (*newinst)->passed = newepp;
      }
      if (IS_LABEL(*ssgen)) {
	 /* Also need to check for substring parameters in labels */
	 for (sptr = TOLABEL(ssgen)->string; sptr != NULL; sptr = sptr->nextpart) {
	    if (sptr->type == PARAM_START) {
	       ops = match_param(topobject, sptr->data.string);
	       newop = copyparameter(ops);
	       newop->next = (*newobj)->params;
	       (*newobj)->params = newop;

	       /* Generate an indirect parameter reference from child to parent */
	       newepp = make_new_eparam(sptr->data.string);
	       newepp->flags |= P_INDIRECT;
	       newepp->pdata.refkey = strdup(sptr->data.string);
	       newepp->next = (*newinst)->passed;
	       (*newinst)->passed = newepp;
	    }
	 }
      }
   }

   /* any parameters in the top-level object that used by the selected	*/
   /* elements must be copied into the new object.			*/

   /* put new object back into place */

   (*newobj)->hidden = false;
   (*newobj)->schemtype = SYMBOL;

   calcbbox(*newinst);
   incr_changes(*newobj);

   /* (netlist invalidation was taken care of by delete_element() */
   /* Also, incr_changes(topobject) was done by delete_element())	*/

   areawin->update();

   /* Copy name into object and check for conflicts */

   strcpy((*newobj)->name, name);
   checkname(*newobj);

   /* register the technology and mark the technology as not saved */
   AddObjectTechnology(*newobj);

   /* generate library instance for this object (bounding box	*/
   /* should be default, so don't do calcbbox() on it)		*/

   addtoinstlist(loclibnum, *newobj, false);

   /* recompile the user catalog and reset view bounds */

   composelib(loclibnum + LIBRARY);
   centerview(xobjs.libtop[loclibnum + LIBRARY]);

   return *newinst;
}

#ifndef TCL_WRAPPER

/*-------------------------------------------*/
/* Make a user object from selected elements */
/*-------------------------------------------*/

void selectsave(QAction* a, void*, void*)
{
   if (areawin->selects == 0) return;  /* nothing was selected */

   /* Get a name for the new object */
   popupQuestion(a, "Enter name for new object:", "\0", xlib_makeobject);
}

#endif

/*-----------------------------*/
/* Edit-stack support routines */
/*-----------------------------*/

void arceditpush(arcptr lastarc)
{
   arcptr *newarc;

   newarc = areawin->editstack->append(new arc(*lastarc));
}

/*--------------------------------------*/

void splineeditpush(splineptr lastspline)
{
   splineptr *newspline;

   newspline = areawin->editstack->append(new spline(*lastspline));
}

/*--------------------------------------*/

void polyeditpush(polyptr lastpoly)
{
   polyptr *newpoly;

   newpoly = areawin->editstack->append(new polygon(*lastpoly));
}

/*--------------------------------------*/

void patheditpush(pathptr lastpath)
{
   pathptr *newpath = areawin->editstack->append(new path(*lastpath));
}

/*--------------------------------------------------------------*/
/* Removes one or more objects, with undo.                      */
/* Call undo_finish_series() when you're done calling this      */
/* function.                                                    */
/*--------------------------------------------------------------*/

void delete_more(objinstptr thisinst, genericptr gen)
{
    objectptr thisobject, delobj;
    short stmp;
    bool deleted = false;

    for (stmp = 0; stmp < thisobject->parts; stmp++) {
        if (thisobject->at(stmp) != gen) continue;
        deleted = true;
        delobj = delete_element(thisinst, &stmp, 1);
        register_for_undo(XCF_Delete, UNDO_MORE, thisinst, delobj, 0);

        /* If we destroy elements in the current window, we need to	   */
        /* make sure that the selection list is updated appropriately. */
        if (thisobject == topobject && areawin->selects != 0) {
           for (short * sobj = areawin->selectlist; sobj < areawin->selectlist +
                    areawin->selects; sobj++)
              if (*sobj > stmp) (*sobj)--;
        }

        /* Also ensure that this element is not referenced in any	*/
        /* netlist.   If it is, remove it and mark the netlist as	*/
        /* invalid.							*/
        remove_netlist_element(thisobject, gen);
    }
    Q_ASSERT(deleted);
}

/*-----------------------------------------------------------------*/
/* For copying:  Check if an object is about to be placed directly */
/* on top of the same object.  If so, delete the one underneath.   */
/*-----------------------------------------------------------------*/

void checkoverlap()
{
   short *sobj, *cobj;
   genericptr *sgen, *pgen;

   QList<genericptr> tagged;

   /* Work through the select list */

   for (sobj = areawin->selectlist; sobj < areawin->selectlist +
	areawin->selects; sobj++) {
      sgen = topobject->begin() + (*sobj);

      /* For each object being copied, compare it against every object	*/
      /* on the current page (except self).  Flag if it's the same.	*/

      for (pgen = 0; topobject->values(pgen); ) {
	 if (pgen == sgen) continue;
         if (**sgen == **pgen) {
	    /* Make sure that this object is not part of the selection, */
	    /* else chaos will reign.					*/
	    for (cobj = areawin->selectlist; cobj < areawin->selectlist +
			areawin->selects; cobj++) {
               if (pgen == topobject->begin() + (*cobj)) break;
	    }
	    /* Tag it for future deletion and prevent further compares */
	    if (cobj == areawin->selectlist + areawin->selects) {
               tagged += *pgen;
	   }
	 }	 
      }
   }
   if (tagged.count()) {
      /* Delete the tagged elements */
      Wprintf("Duplicate object deleted");
      foreach (genericptr gen, tagged) {
          delete_more(areawin->topinstance, gen);
      }
      undo_finish_series();
      incr_changes(topobject);
   }
}

/*--------------------------------------------------------------*/
/* Direct placement of elements.  Assumes that the selectlist	*/
/* contains all the elements to be positioned.  "dlt" is the	*/
/* relative position to move the elements by */
/*--------------------------------------------------------------*/

void placeselects(const XPoint & dlt, XPoint *userpt)
{
   short *dragselect;
   XPoint delta = dlt, newpos, *ppt;
   int rot;
   short closest;
   bool doattach;

   doattach = ! ((userpt == NULL) || (areawin->attachto < 0));

   /* under attachto condition, keep element attached to */
   /* the attachto element.				 */

   if (doattach) findattach(&newpos, &rot, userpt);

   for (dragselect = areawin->selectlist; dragselect < areawin->selectlist
      + areawin->selects; dragselect++) {

      switch(SELECTTYPE(dragselect)) {
         case OBJINST: {
	    objinstptr draginst = SELTOOBJINST(dragselect);

	    if (doattach) {
               draginst->position = newpos;
	       while (rot >= 360) rot -= 360;
	       while (rot < 0) rot += 360;
	       draginst->rotation = rot;
	    }
	    else {
               draginst->position += delta;
	    }
            areawin->update();

	 } break;
         case GRAPHIC: {
	    graphicptr dragg = SELTOGRAPHIC(dragselect);
            dragg->position += delta;
            areawin->update();
	 } break;
	 case LABEL: {
	    labelptr draglabel = SELTOLABEL(dragselect);
	    if (doattach) {
               draglabel->position = newpos;
	       draglabel->rotation = rot;
	    }
	    else {
               draglabel->position += delta;
	    }
            areawin->update();
	 } break;
	 case PATH: {
	    pathptr dragpath = SELTOPATH(dragselect);
	    genericptr *pathlist;
	    
	    if (doattach) {
	       XPoint *pdelta = pathclosepoint(dragpath, &newpos);
               delta = newpos - *pdelta;
            }
            for (pathlist = 0; dragpath->values(pathlist); ) {
               movepoints(pathlist, delta);
	    }
            areawin->update();
	 } break;
	 case POLYGON: {
            polyptr dragpoly = SELTOPOLY(dragselect);

            // if (dragpoly->cycle != NULL) continue;
	    if (doattach) {
	       closest = closepoint(dragpoly, &newpos);
               delta = newpos - dragpoly->points[closest];
	    }
            dragpoly->points += delta;
            areawin->update();
	 } break;   
	 case SPLINE: {
	    splineptr dragspline = SELTOSPLINE(dragselect);
	    short j;
	    fpointlist dragpoints;

            // if (dragspline->cycle != NULL) continue;
	    if (doattach) {
	       closest = (wirelength(&dragspline->ctrl[0], &newpos)
		  > wirelength(&dragspline->ctrl[3], &newpos)) ? 3 : 0;
               delta = newpos - dragspline->ctrl[closest];
	    }
	    for (dragpoints = dragspline->points; dragpoints < dragspline->
		   points + INTSEGS; dragpoints++) {
               *dragpoints += delta;
	    }
	    for (j = 0; j < 4; j++) {
               dragspline->ctrl[j] += delta;
	    }
            areawin->update();
	 } break;
	 case ARC: {
	    arcptr dragarc = SELTOARC(dragselect);
	    fpointlist dragpoints;

	    if (doattach) {
               delta = newpos - dragarc->position;
	    }
            dragarc->position += delta;
	    for (dragpoints = dragarc->points; dragpoints < dragarc->
		 points + dragarc->number; dragpoints++) {
               *dragpoints += delta;
	    }
            areawin->update();
         } break;
      }
   }

   if (areawin->pinattach) {
       for (polyiter cpoly; topobject->values(cpoly); ) {
        if (cpoly->cycle != NULL) {
           ppt = cpoly->points + cpoly->cycle->number;
           newpos = *ppt + delta;
           if (areawin->manhatn)
              manhattanize(&newpos, cpoly, cpoly->cycle->number, false);
           *ppt = newpos;
           areawin->update();
        }
      }	
   }
}

/*----------------------------------------------------------------------*/
/* Copy handler.  Assumes that the selectlist contains the elements	*/
/* to be copied, and that the initial position of the copy is held	*/
/* in areawin->save.							*/
/*----------------------------------------------------------------------*/

void createcopies()
{
   short *selectobj;

   if (!checkselect(ALL_TYPES, true)) return;
   u2u_snap(areawin->save);
   for (selectobj = areawin->selectlist; selectobj < areawin->selectlist
		+ areawin->selects; selectobj++) {
      topobject->append((*SELTOGENERICPTR(selectobj))->copy());

      /* change selection from the old to the new object */
      *selectobj = topobject->parts - 1;
   }
}

/*--------------------------------------------------------------*/
/* Function which initiates interactive placement of copied	*/
/* elements.							*/
/*--------------------------------------------------------------*/

void copydrag()
{
   if (areawin->selects > 0) {
      if (eventmode == NORMAL_MODE) {
         XDefineCursor(areawin->viewport, COPYCURSOR);
	 eventmode = COPY_MODE;
         xcAddEventHandler(areawin->viewport, PointerMotionMask, false,
		(XtEventHandler)xlib_drag, NULL);
      }
      select_invalidate_netlist();
   }
   areawin->update();
}

/*-----------------------------------------------------------*/
/* Copy handler for copying from a button push or key event. */
/*-----------------------------------------------------------*/

void copy_op(int op, int x, int y)
{
   short *csel;

   if (op == XCF_Copy) {
      window_to_user(x, y, &areawin->save);
      createcopies();	/* This function does all the hard work */
      copydrag();		/* Start interactive placement */
   }
   else {
      eventmode = NORMAL_MODE;
      areawin->attachto = -1;
      W3printf("");
#ifdef TCL_WRAPPER
      xcRemoveEventHandler(areawin->viewport, PointerMotionMask, false,
	    (XtEventHandler)xctk_drag, NULL);
#else
      xcRemoveEventHandler(areawin->viewport, PointerMotionMask, false,
	    (XtEventHandler)xlib_drag, NULL);
#endif
      XDefineCursor(areawin->viewport, DEFAULTCURSOR);
      u2u_snap(areawin->save);
      if (op == XCF_Cancel) {
         delete_noundo();

	 /* Redraw the screen so that an object directly under */
	 /* the delete object won't get painted black */


      }
      else if (op == XCF_Finish_Copy) {
	 /* If selected objects are the only ones on the page, */
	 /* then do a full bbox calculation.			  */
	 if (topobject->parts == areawin->selects)
	    calcbbox(areawin->topinstance);
	 else	
	    calcbboxselect();
	 checkoverlap();
	 register_for_undo(XCF_Copy, UNDO_MORE, areawin->topinstance,
			areawin->selectlist, areawin->selects);
	 unselect_all();
         incr_changes(topobject);
      }
      else {	 /* XCF_Continue_Copy */
	 if (topobject->parts == areawin->selects)
	    calcbbox(areawin->topinstance);
	 else
	    calcbboxselect();
	 checkoverlap();
	 register_for_undo(XCF_Copy, UNDO_DONE, areawin->topinstance,
			areawin->selectlist, areawin->selects);
         createcopies();
         copydrag();		/* Start interactive placement again */
         incr_changes(topobject);
      }
   }
    areawin->update();
}

/*----------------------------------------------------------------------*/
/* Operation continuation---dependent upon the ongoing operation.	*/
/* This operation only pertains to a few event modes for which		*/
/* continuation	of action makes sense---drawing wires (polygons), and	*/
/* editing polygons, arcs, splines, and paths.				*/
/*----------------------------------------------------------------------*/

void continue_op(int op, int x, int y)
{
   XPoint ppos;

   if (eventmode != EARC_MODE && eventmode != ARC_MODE) {
      window_to_user(x, y, &areawin->save);
   }
   snap(x, y, &ppos);
   printpos(ppos);

   switch(eventmode) {
      case(COPY_MODE):
	 copy_op(op, x, y);
	 break;
      case(WIRE_MODE):
	 wire_op(op, x, y);
	 break;
      case(EPATH_MODE): case(EPOLY_MODE): case (ARC_MODE):
      case(EARC_MODE): case(SPLINE_MODE): case(ESPLINE_MODE):
	 path_op(*(EDITPART), op, x, y);
	 break;
      case(EINST_MODE):
	 inst_op(*(EDITPART), op, x, y);
	 break;
      case(BOX_MODE):
	 finish_op(XCF_Finish_Element, x, y);
	 break;
   }
}

/*--------------------------------------------------------------*/
/* Finish or cancel an operation.  This forces a return to	*/
/* "normal" mode, with whatever other side effects are caused	*/
/* by the operation.						*/
/*--------------------------------------------------------------*/

void finish_op(int op, int x, int y)
{
   labelptr curlabel;
   int libnum;
   XPoint snappt;
   float fscale;

   if (eventmode != EARC_MODE && eventmode != ARC_MODE) {
      window_to_user(x, y, &areawin->save);
   }
   switch(eventmode) {
      case(EPATH_MODE): case(BOX_MODE): case(EPOLY_MODE): case (ARC_MODE):
	     case(EARC_MODE): case(SPLINE_MODE): case(ESPLINE_MODE):
	 path_op(*(EDITPART), op, x, y);
	 break;

      case(EINST_MODE):
	 inst_op(*(EDITPART), op, x, y);
	 break;

      case (FONTCAT_MODE):
      case (EFONTCAT_MODE):
	 fontcat_op(op, x, y);
	 eventmode = (eventmode == FONTCAT_MODE) ? TEXT_MODE : ETEXT_MODE;
         XDefineCursor (areawin->viewport, TEXTPTR);
	 break;

      case(ASSOC_MODE):
      case(CATALOG_MODE):
	 catalog_op(op, x, y);
	 break;

      case(WIRE_MODE):
	 wire_op(op, x, y);
	 break;

      case(COPY_MODE):
	 copy_op(op, x, y);
	 break;

      case(TEXT_MODE):
         curlabel = TOLABEL(EDITPART);
         if (op == XCF_Cancel) {
             delete curlabel;
	     curlabel = NULL;
	 }
	 else {
	    topobject->parts++;
	    singlebbox(EDITPART);
	    incr_changes(topobject);
            select_invalidate_netlist();
	 }
	 setdefaultfontmarks();
	 eventmode = NORMAL_MODE;
         break;

      case(ETEXT_MODE): case(CATTEXT_MODE):
         curlabel = TOLABEL(EDITPART);
	 if (op == XCF_Cancel) {
            /* restore the original text */
            undo_finish_series();
	    undo_action();
            if (eventmode == CATTEXT_MODE) eventmode = CATALOG_MODE;
	    W3printf("");
	    setdefaultfontmarks();
	 }
	 else textreturn();  /* Generate "return" key character */
	 areawin->textend = 0;
	 break;

      case(CATMOVE_MODE):
         u2u_snap(areawin->save);
#ifdef TCL_WRAPPER
         Tk_DeleteEventHandler(areawin->viewport, ButtonMotionMask,
                (Tk_EventProc *)xctk_drag, NULL);
#else
         xcRemoveEventHandler(areawin->viewport, ButtonMotionMask, false,
		(XtEventHandler)xlib_drag, NULL);
#endif
	 if (op == XCF_Cancel) {
	    /* Just regenerate the library where we started */
	    if (areawin->selects >= 1) {
	       objinstptr selinst = SELTOOBJINST(areawin->selectlist);
	       libnum = libfindobject(selinst->thisobject, NULL);
	       if (libnum >= 0)
		  composelib(libnum + LIBRARY);
	    }
	 }
	 else {
	    catmove(x, y);
	 }
	 clearselects();
	 eventmode = CATALOG_MODE;
         XDefineCursor(areawin->viewport, DEFAULTCURSOR);
	 break;

      case(MOVE_MODE):
         u2u_snap(areawin->save);
#ifdef TCL_WRAPPER
         Tk_DeleteEventHandler(areawin->viewport, ButtonMotionMask,
                (Tk_EventProc *)xctk_drag, NULL);
#else
         xcRemoveEventHandler(areawin->viewport, ButtonMotionMask, false,
		(XtEventHandler)xlib_drag, NULL);
#endif
	 if (op == XCF_Cancel) {
	    /* If we came from the library with an object instance, in	*/
	    /* MOVE_MODE, then "cancel" should delete the element.	*/
	    /* Otherwise, put the position of the element back to what	*/
	    /* it was before we started the move.  The difference is	*/
	    /* indicated by the value of areawin->editpart.		*/

            if ((areawin->selects > 0) && (*areawin->selectlist == topobject->parts))
               delete_noundo();
	    else 
               placeselects(areawin->origin - areawin->save, NULL);
            clearselects();
	 }
	 else {
	    if (areawin->selects > 0) {
	       register_for_undo(XCF_Move, 
			(was_preselected) ? UNDO_DONE : UNDO_MORE,
			areawin->topinstance,
			(int)(areawin->save.x - areawin->origin.x),
			(int)(areawin->save.y - areawin->origin.y));
	       pwriteback(areawin->topinstance);
	       incr_changes(topobject);
	       select_invalidate_netlist();
	    }
	    W3printf("");
	    /* full calc needed: move may shrink bbox */
	    calcbbox(areawin->topinstance);
	    checkoverlap();
	    if (!was_preselected) clearselects();
	 }
	 areawin->attachto = -1;
	 break;

      case(RESCALE_MODE):

#ifdef TCL_WRAPPER
	 Tk_DeleteEventHandler(areawin->area, ButtonMotionMask,
                (Tk_EventProc *)xctk_drag, NULL);
#endif
         if (op != XCF_Cancel) {
            fscale = SELTOGENERIC(areawin->selectlist)->rescaleBox(areawin->save, NULL);
            if (fscale > 0.0) elementrescale(fscale);
	 }
	 eventmode = NORMAL_MODE;
	 break;

      case(SELAREA_MODE):

#ifdef TCL_WRAPPER
	 Tk_DeleteEventHandler(areawin->area, ButtonMotionMask,
                (Tk_EventProc *)xctk_drag, NULL);
#endif
	 /* Zero-width boxes act like a normal selection.  Otherwise,	*/
 	 /* proceed with the area select.				*/

         if (areawin->origin == areawin->save)
            select_add_element(ALL_TYPES);
	 else
            selectarea(topobject, 0);
	 break;

      case(PAN_MODE):
         u2u_snap(areawin->save);

         xcRemoveEventHandler(areawin->viewport, PointerMotionMask, false,
               (XtEventHandler)xlib_drag, NULL);
	 if (op != XCF_Cancel)
	    panbutton((u_int) 5, (areawin->width >> 1) - (x - areawin->origin.x),
			(areawin->height >> 1) - (y - areawin->origin.y), 0);
	 break;
   }

   /* Remove any selections */
   if ((eventmode == SELAREA_MODE) || (eventmode == PAN_MODE)
		|| (eventmode == MOVE_MODE))
      eventmode = NORMAL_MODE;
   else if (eventmode != MOVE_MODE && eventmode != EPATH_MODE &&
                eventmode != EPOLY_MODE && eventmode != ARC_MODE &&
                eventmode != EARC_MODE && eventmode != SPLINE_MODE &&
                eventmode != ESPLINE_MODE && eventmode != WIRE_MODE &&
                eventmode != ETEXT_MODE && eventmode != TEXT_MODE) {
      unselect_all();
  }

   if (eventmode == NORMAL_MODE) {
      
      /* Return any highlighted networks to normal */
      remove_highlights(areawin->topinstance);

      XDefineCursor(areawin->viewport, DEFAULTCURSOR);
   }

   areawin->update();
   snap(x, y, &snappt);
   printpos(snappt);
}

/*--------------------------------------------------------------*/
/* Edit operations for instances.  This is used to allow	*/
/* numeric parameters to be adjusted from the hierarchical	*/
/* level above, shielding the the unparameterized parts from	*/
/* change.							*/
/*--------------------------------------------------------------*/

void inst_op(genericptr editpart, int op, int x, int y)
{
}

/*--------------------------------------------------------------*/
/* Operations for path components				*/
/*--------------------------------------------------------------*/

void path_op(genericptr editpart, int op, int x, int y)
{
   polyptr newpoly;
   splineptr newspline;
   genericptr *ggen;
   bool donecycles = false;
   XPoint *refpt, *cptr;

   /* Don't allow point cycling in a multi-part edit.	*/
   /* Allowing it is just confusing.  Instead, we treat	*/
   /* button 1 (cycle) like button 2 (finish).		*/
   if (op == XCF_Continue_Element && areawin->selects > 1)
      op = XCF_Finish_Element;

   switch(ELEMENTTYPE(editpart)) {
      case (PATH): {
	 pathptr newpath = (pathptr)editpart;
         short dotrack = true;
         pathptr editpath;

         areawin->attachto = -1;

	 if (op != XCF_Continue_Element) {
	    dotrack = false;
            areawin->update();
	 }
	 if (op == XCF_Continue_Element) {
	    nextpathcycle(newpath, 1);
	    patheditpush(newpath);
	 }
         else if (op == XCF_Finish_Element) {
            areawin->update();
	 }
	 else {		/* restore previous path from edit stack */
	    if (areawin->editstack->parts > 0) {
	       if (op == XCF_Cancel) {
                  editpath = TOPATH(areawin->editstack->begin());
                  *newpath = *editpath;
                  areawin->editstack->clear();
	       }
	       else {
                  editpath = TOPATH(areawin->editstack->end() - 1);
                  *newpath = *editpath;
                  delete editpath;
	          areawin->editstack->parts--;
	       }
	       if (areawin->editstack->parts > 0) {
		  dotrack = true;
                  nextpathcycle(newpath, 1);
	       }
	       else {
	       	  XPoint warppt;

		  user_to_window(areawin->origin, &warppt);
		  warppointer(warppt.x, warppt.y);
	       }
               areawin->update();
	    }
	    else {
	       topobject->parts--;
               delete newpath;
	    }
	 }
	 pwriteback(areawin->topinstance);

	 if (!dotrack) {
	    /* Free the editstack */
            areawin->editstack->clear();
            xcRemoveEventHandler(areawin->viewport, PointerMotionMask, false,
                        (XtEventHandler)trackelement, NULL);
	    eventmode = NORMAL_MODE;
            donecycles = true;
	 }
      } break;

      case (POLYGON): {
	 if (eventmode == BOX_MODE) {
            polyptr   newbox;

            newbox = (polyptr)editpart;
            areawin->update();

            /* prevent length and/or width zero boxes */
            if (newbox->points[0].x != newbox->points[2].x &&
                newbox->points[1].y != newbox->points[3].y) {
               if (op != XCF_Cancel) {
	          topobject->parts++;
                  areawin->update();
	          incr_changes(topobject);
		  if (!nonnetwork(newbox)) invalidate_netlist(topobject);
                  register_for_undo(XCF_Box, UNDO_MORE, areawin->topinstance,
					newbox);
	       }
	       else {
                  delete newbox;
	       }
            }
	    else {
               delete newbox;
	    }

            xcRemoveEventHandler(areawin->viewport, PointerMotionMask, false,
               (XtEventHandler)trackbox, NULL);
            eventmode = NORMAL_MODE;
         }
	 else {   /* EPOLY_MODE */
            polyptr      editpoly;
            short        dotrack = true;
  
            newpoly = (polyptr)editpart;
            areawin->attachto = -1;
   
            if (op != XCF_Continue_Element) {
	       dotrack = false;
               areawin->update();
            }

            if (op == XCF_Continue_Element) {
               nextpolycycle(&newpoly, 1);
	       polyeditpush(newpoly);
	    }
            else if (op == XCF_Finish_Element) {

	       /* Check for degenerate polygons (all points the same). */
	       int i;
               for (i = 1; i < newpoly->points.count(); i++)
                  if (newpoly->points[i] != newpoly->points[i - 1])
		     break;
               if (i == newpoly->points.count()) {
		  /* Remove this polygon with the standard delete	*/
		  /* method (saves polygon on undo stack).		*/
                  delete_more(areawin->topinstance, newpoly);
                  undo_finish_series();
	       }
               else {
                  areawin->update();
                  if (!nonnetwork(newpoly)) invalidate_netlist(topobject);
	       }
	    }
            else {
               XPoint warppt;
	       if (areawin->editstack->parts > 0) {
	          if (op == XCF_Cancel) {
                     editpoly = TOPOLY(areawin->editstack->begin());
                     *newpoly = *editpoly;
                     areawin->editstack->clear();
	          }
		  else {
                     editpoly = TOPOLY(areawin->editstack->end() - 1);
                     *newpoly = *editpoly;
                     delete editpoly;
	             areawin->editstack->parts--;
		  }
	          if (areawin->editstack->parts > 0) {
		     dotrack = true;
                     nextpolycycle(&newpoly, -1);
	          }
                  else {
		     user_to_window(areawin->origin, &warppt);
		     warppointer(warppt.x, warppt.y);
	          }
                  areawin->update();
	       }
	       else {
	          topobject->parts--;
                  delete newpoly;
	       }
	    }
	    pwriteback(areawin->topinstance);

            if (!dotrack) {
               areawin->editstack->clear();

               xcRemoveEventHandler(areawin->viewport, PointerMotionMask, false,
                   (XtEventHandler)trackelement, NULL);
               eventmode = NORMAL_MODE;
               donecycles = true;
            }
 	 }
      } break;

      case (ARC): {
         arcptr   newarc, editarc;
         short    dotrack = true;

	 newarc = (arcptr)editpart;

	 if (op != XCF_Continue_Element) {
            dotrack = false;
            areawin->update();
         }

	 if (op == XCF_Continue_Element) {
            nextarccycle(&newarc, 1);
	    arceditpush(newarc);
	 }

         else if (op == XCF_Finish_Element) {
	    dotrack = false;

            if (newarc->radius != 0 && newarc->yaxis != 0 &&
		   (newarc->angle1 != newarc->angle2)) {
               if (eventmode == ARC_MODE) {
		  topobject->parts++;
	          incr_changes(topobject);
                  register_for_undo(XCF_Arc, UNDO_MORE, areawin->topinstance,
				newarc);
	       }
               areawin->update();
	    }
	    else {

               /* Remove the record if the radius is zero.  If we were	*/
	       /* creating the arc, just delete it;  it's as if it	*/
	       /* never existed.  If we were editing an arc, use the	*/
	       /* standard delete method (saves arc on undo stack).	*/

	       if (eventmode == ARC_MODE) {
                  delete newarc;
	       }
	       else {
                  delete_more(areawin->topinstance, newarc);
                  undo_finish_series();
	       }
	    } 
	 }
         else {	 /* Cancel: restore previous arc from edit stack */
	    if (areawin->editstack->parts > 0) {
	       if (op == XCF_Cancel) {
                  editarc = TOARC(areawin->editstack->begin());
                  *newarc = *editarc;
                  areawin->editstack->clear();
	       }
	       else {
                  editarc = TOARC(areawin->editstack->end() - 1);
                  *newarc = *editarc;
                  delete editarc;
	          areawin->editstack->parts--;
	       }
	       if (areawin->editstack->parts > 0) {
		  dotrack = true;
                  nextarccycle(&newarc, -1);
                  areawin->update();
	       }
               else {
		  if (eventmode != ARC_MODE) {
	       	     XPoint warppt;
		     user_to_window(areawin->origin, &warppt);
		     warppointer(warppt.x, warppt.y);
		  }
	       }
               areawin->update();
	    }
         }
	 pwriteback(areawin->topinstance);

	 if (!dotrack) {
            areawin->editstack->clear();
            xcRemoveEventHandler(areawin->viewport, PointerMotionMask, false,
               (XtEventHandler)trackarc, NULL);
            eventmode = NORMAL_MODE;
	 }
      } break;

      case (SPLINE): {
	 splineptr	editspline;
	 short 		dotrack = true;

	 newspline = (splineptr)editpart;

	 if (op != XCF_Continue_Element) {
            areawin->update();
	    dotrack = false;
	 }

	 if (op == XCF_Continue_Element) {
	    /* Note:  we work backwards through spline control points.	*/
	    /* The reason is that when creating a spline, the sudden	*/
	    /* move from the endpoint to the startpoint	(forward	*/
	    /* direction) is more disorienting than moving from the	*/
	    /* endpoint to the endpoint's control point.		*/

            nextsplinecycle(&newspline, -1);
	    splineeditpush(newspline);
	 }

	 /* unlikely but possible to create zero-length splines */
	 else if (newspline->ctrl[0].x != newspline->ctrl[3].x ||
	      	newspline->ctrl[0].x != newspline->ctrl[1].x ||
	      	newspline->ctrl[0].x != newspline->ctrl[2].x ||
	      	newspline->ctrl[0].y != newspline->ctrl[3].y ||
	      	newspline->ctrl[0].y != newspline->ctrl[1].y ||
	      	newspline->ctrl[0].y != newspline->ctrl[2].y) {
            if (op == XCF_Finish_Element) {
	       if (eventmode == SPLINE_MODE) {
		  topobject->parts++;
		  incr_changes(topobject);
                  register_for_undo(XCF_Spline, UNDO_MORE, areawin->topinstance,
				newspline);
	       }
               areawin->update();
	    }
            else {	/* restore previous spline from edit stack */
	       if (areawin->editstack->parts > 0) {
		  if (op == XCF_Cancel) {
                     editspline = TOSPLINE(areawin->editstack->begin());
                     *newspline = *editspline;
                     areawin->editstack->clear();
		  }
		  else {
                     editspline = TOSPLINE(areawin->editstack->end() - 1);
                     *newspline = *editspline;
                     delete editspline;
	             areawin->editstack->parts--;
		  }
		  if (areawin->editstack->parts > 0) {
		     dotrack = true;
                     nextsplinecycle(&newspline, 1);
                     areawin->update();
		  }
                  else {
		     if (eventmode != SPLINE_MODE) {
	       		XPoint warppt;
		        user_to_window(areawin->origin, &warppt);
		        warppointer(warppt.x, warppt.y);
		     }
                     areawin->update();
		  }
	       }
	    }
	 }
	 else {
	    if (eventmode != SPLINE_MODE) topobject->parts--;
            delete newspline;
	 }
	 pwriteback(areawin->topinstance);

	 if (!dotrack) {
            areawin->editstack->clear();
            xcRemoveEventHandler(areawin->viewport, PointerMotionMask, false,
                    (XtEventHandler)trackelement, NULL);
	    eventmode = NORMAL_MODE;
            donecycles = true;
	 }
      } break;
   }
   calcbbox(areawin->topinstance);

   /* Multiple-element edit:  Some items may have been moved as	*/
   /* opposed to edited, and should be registered here.	 To do	*/
   /* this correctly, we must first unselect the edited items,	*/
   /* then register the move for the remaining items.		*/

   if (donecycles) {
      short *eselect, cycle;
      bool fullmove = false;

      for (eselect = areawin->selectlist; eselect < areawin->selectlist +
                areawin->selects; eselect++) {
         cycle = checkcycle(SELTOGENERIC(eselect), 0);
         if (cycle < 0) {
            fullmove = true;		/* At least 1 object moved */
            break;
         }
      }

      /* Remove all (remaining) cycles */
      for (eselect = areawin->selectlist; eselect < areawin->selectlist +
                areawin->selects; eselect++)
         removecycle(*SELTOGENERICPTR(eselect));

      /* Remove edits from the undo stack when canceling */
      if (op == XCF_Cancel || op == XCF_Cancel_Last) {
         if (xobjs.undostack && (xobjs.undostack->type == XCF_Edit)) {
            undo_finish_series();
            undo_action();
         }
      }
   }
}

/*-------------------------------------------------------------------------*/
