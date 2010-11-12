/*-------------------------------------------------------------------------*/
/* selection.c --- xcircuit routines handling element selection etc.	   */
/* Copyright (c) 2002  Tim Edwards, Johns Hopkins University       	   */
/*-------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------*/
/*      written by Tim Edwards, 8/13/93    				   */
/*-------------------------------------------------------------------------*/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

#ifdef TCL_WRAPPER 
#include <tk.h>
#endif

#include "context.h"
#include "matrix.h"
#include "xcircuit.h"
#include "colors.h"
#include "prototypes.h"
#include "xcqt.h"

/*----------------------------------------------------------------------*/
/* Exported Variable definitions					*/
/*----------------------------------------------------------------------*/

extern Cursor	appcursors[NUM_CURSORS];

#ifdef TCL_WRAPPER 
extern Tcl_Interp *xcinterp;
#endif


selection::~selection()
{
   free(selectlist);
   selection *sel = next;
   while (sel) {
      selection *next = sel->next;
      sel->next = NULL; // avoid recursion
      delete sel;
      sel = next;
   }
}

/*----------------------------------------------------------------------*/
/* Prevent a list of elements from being selected.			*/
/*----------------------------------------------------------------------*/

void disable_selects(objectptr thisobject, short *selectlist, int selects)
{
   genericptr genptr;
   short *i;

   for (i = selectlist; i < selectlist + selects; i++) {
      genptr = thisobject->at(*i);
      /// \todo select_hide
      //genptr->select_hide = 1;
   }
}

/*----------------------------------------------------------------------*/
/* Allow a list of elements to be selected, if they were disabled using	*/
/* the disable_selects() routine.					*/
/*----------------------------------------------------------------------*/

void enable_selects(objectptr thisobject, short *selectlist, int selects)
{
   genericptr genptr;
   short *i;

   for (i = selectlist; i < selectlist + selects; i++) {
      genptr = thisobject->at(*i);
      /// \todo select_hide
      //genptr->select_hide = 0;
   }
}

/*----------------------------------------------------------------------*/
/* Change filter to determine what types can be selected		*/
/*----------------------------------------------------------------------*/

void selectfilter(QAction * a, pointertype value, void* calldata)
{
   short bitwise = (int)value;
   bool bval = areawin->filter & bitwise;

   if (bval)
      areawin->filter &= ~bitwise;
   else
      areawin->filter |= bitwise;

#ifndef TCL_WRAPPER
   toggle(a, (pointertype)(-1), &bval);
#endif
}

/*----------------------------------------------------------------------*/
/* Look at select stack to see if there are any selects; call select,	*/
/* if not.  If draw_selected is True, then the selected items are drawn	*/
/* in the select color.	 Otherwise, they are not redrawn.		*/
/*----------------------------------------------------------------------*/

bool checkselect(short value, bool draw_selected)
{
   short *check;
   editmode savemode;
 
   value &= areawin->filter;	/* apply the selection filter */

   if (areawin->selects == 0) {
      savemode = eventmode;
      if (!draw_selected) eventmode = PENDING_MODE;
      select_element(value);
      eventmode = savemode;
   }
   if (areawin->selects == 0) return false;
   for (check = areawin->selectlist; check < areawin->selectlist +
	areawin->selects; check++)
      if (SELECTTYPE(check) & value) break;
   if (check == areawin->selectlist + areawin->selects) return false;
   else return true;
}

/*--------------------------------------------------------------*/
/* Select list numbering revision when an element is deleted	*/
/* from an object. 						*/
/*--------------------------------------------------------------*/

void reviseselect(short *slist, int selects, short *removed)
{
   short *chkselect;

   for (chkselect = slist; chkselect < slist + selects; chkselect++)
      if (*chkselect > *removed) (*chkselect)--;
}

/*----------------------*/
/* Draw a selected item */
/*----------------------*/

void geneasydraw(Context* ctx, short instance, int mode, objectptr curobj, objinstptr curinst)
{
   genericptr elementptr = curobj->at(instance);

   switch (ELEMENTTYPE(elementptr)) {
      case ARC:
      case POLYGON:
      case SPLINE:
      case PATH:
      case GRAPHIC:
         elementptr->draw(ctx);
	 break;
      case LABEL:
         UDrawString(ctx, (labelptr)elementptr, mode, curinst);
	 break;
      case OBJINST:
         UDrawObject(ctx, (objinstptr)elementptr, SINGLE, mode, NULL);
         break;
   }
}

/*-------------------------------------------------*/
/* Draw a selected item, including selection color */
/*-------------------------------------------------*/

void gendrawselected(Context* ctx, short *newselect, objectptr curobj, objinstptr curinst)
{
   /* Don't draw selection color when selecting for edit */
   if (eventmode == PENDING_MODE) return;

   SetFunction(ctx->gc(), GXcopy);
   ctx->gctype = GXcopy;
   SetForeground(ctx->gc(), BACKGROUND);
   ctx->gccolor = BACKGROUND;
   geneasydraw(ctx, *newselect, DOFORALL, curobj, curinst);
   curobj->at(*newselect)->indicateparams(ctx);

   SetFunction(ctx->gc(), GXxor);
   ctx->gctype = GXxor;
   SetForeground(ctx->gc(), SELECTCOLOR ^ BACKGROUND);
   ctx->gccolor = SELECTCOLOR ^ BACKGROUND;
   geneasydraw(ctx, *newselect, DOFORALL, curobj, curinst);

   SetForeground(ctx->gc(), AUXCOLOR ^ BACKGROUND);
   curobj->at(*newselect)->indicateparams(ctx);

   SetForeground(ctx->gc(), ctx->gccolor);
   SetFunction(ctx->gc(), ctx->gctype);
}

/*---------------------------------------------------*/
/* Allocate or reallocate memory for a new selection */
/*---------------------------------------------------*/

short *allocselect()
{
   short *newselect;

   if (areawin->selects == 0)
      areawin->selectlist = (short *) malloc(sizeof(short));
   else
      areawin->selectlist = (short *) realloc(areawin->selectlist,
	 (areawin->selects + 1) * sizeof(short));

   newselect = areawin->selectlist + areawin->selects;
   areawin->selects++;

   return newselect;
}

/*-------------------------------------------------*/
/* Set Options menu according to 1st selection	   */
/*-------------------------------------------------*/

void setoptionmenu()
{
   short      *mselect;
   labelptr   mlabel;

   if (areawin->selects == 0) {
      setallstylemarks(areawin->style);
      setcolormark(areawin->color);
      setdefaultfontmarks();
      setparammarks(NULL);
      return;
   }

   for (mselect = areawin->selectlist; mselect < areawin->selectlist +
	areawin->selects; mselect++) {
      setcolormark(SELTOCOLOR(mselect));
      setparammarks(SELTOGENERIC(mselect));
      switch(SELECTTYPE(mselect)) {
	 case ARC:
	    setallstylemarks(SELTOARC(mselect)->style);
	    return;
	 case POLYGON:
	    setallstylemarks(SELTOPOLY(mselect)->style);
	    return;
	 case SPLINE:
            setallstylemarks(SELTOSPLINE(mselect)->style);
	    return;
	 case PATH:
            setallstylemarks(SELTOPATH(mselect)->style);
	    return;
	 case LABEL:
	    mlabel = SELTOLABEL(mselect);
	    setfontmarks(mlabel->string->data.font, mlabel->justify);
	    return;
      }
   }
}

/*-------------------------------------*/
/* Test of point being inside of a box */
/*-------------------------------------*/

int test_insideness(int tx, int ty, const XPoint *boxpoints)
{
   int i, stval = 0; 
   const XPoint *pt1, *pt2;
   int stdir;   

   for (i = 0; i < 4; i++) {
      pt1 = boxpoints + i;
      pt2 = boxpoints + ((i + 1) % 4);
      stdir = (pt2->x - pt1->x) * (ty - pt1->y)
		- (pt2->y - pt1->y) * (tx - pt1->x);
      stval += sign(stdir);
   }
   return (abs(stval) == 4) ? 1 : 0;
}

/*--------------------------------------------*/
/* Search for selection among path components */
/*--------------------------------------------*/

#define RANGE_NARROW 11.5
#define RANGE_WIDE 50

bool pathselect(genericptr *curgen, short class_, float range)
{
   /*----------------------------------------------------------------------*/
   /* wirelim is the distance, in user-space units, at which an element is */
   /* considered for selection.						   */
   /*									   */
   /* wirelim = A + B / (scale + C)					   */
   /*									   */
   /* where A = minimum possible distance (expands range at close scale)   */
   /*       C = minimum possible scale    (contract range at far scale)	   */
   /*	    B   makes wirelim approx. 25 at default scale of 0.5, which	   */
   /*		is an empirical result.					   */
   /*----------------------------------------------------------------------*/

   float	wirelim = 2 + range / (areawin->vscale + 0.05);
   long		sqrwirelim = (int)(wirelim * wirelim);

   long		newdist;
   bool	selected = false;

   class_ &= areawin->filter;	/* apply the selection filter */

   if ((*curgen)->type == (class_ & ARC)) {

      /* look among the arcs */

      fpointlist currentpt;
      XPoint nearpt[3];

      nearpt[2] = nearpt[0] = TOARC(curgen)->points[0];
      for (currentpt = TOARC(curgen)->points + 1; currentpt < TOARC(curgen)->points
              + TOARC(curgen)->number; currentpt++) {
         nearpt[1] = nearpt[0];
         nearpt[0] = *currentpt;
	 newdist = finddist(&nearpt[0], &nearpt[1], &areawin->save);
         if (newdist <= sqrwirelim) break;
      }
      if ((!(TOARC(curgen)->style & UNCLOSED)) && (newdist > sqrwirelim))
	 newdist = finddist(&nearpt[0], &nearpt[2], &areawin->save);

      if (newdist <= sqrwirelim) selected = true;
   }

   else if ((*curgen)->type == (class_ & SPLINE)) {

      /* look among the splines --- look at polygon representation */

      fpointlist currentpt;
      XPoint nearpt[2];

      nearpt[0] = TOSPLINE(curgen)->points[0];
      newdist = finddist(&(TOSPLINE(curgen)->ctrl[0]), &(nearpt[0]),
		   &areawin->save);
      if (newdist > sqrwirelim) {
         for (currentpt = TOSPLINE(curgen)->points; currentpt <
		  TOSPLINE(curgen)->points + INTSEGS; currentpt++) {
            nearpt[1] = nearpt[0];
            nearpt[0] = *currentpt;
	    newdist = finddist(&nearpt[0], &nearpt[1], &areawin->save);
            if (newdist <= sqrwirelim) break;
	 }
	 if (newdist > sqrwirelim) {
	    newdist = finddist(&nearpt[0], &(TOSPLINE(curgen)->ctrl[3]),
			&areawin->save);
            if ((!(TOSPLINE(curgen)->style & UNCLOSED)) && (newdist > sqrwirelim))
	       newdist = finddist(&(TOSPLINE(curgen)->ctrl[0]),
		     &(TOSPLINE(curgen)->ctrl[3]), &areawin->save);
	 }
      }

      if (newdist <= sqrwirelim) selected = true;
   }

   else if ((*curgen)->type == (class_ & POLYGON)) {

      /* finally, look among the polygons */

      XPoint* currentpt;

      for (currentpt = TOPOLY(curgen)->points.begin(); currentpt < TOPOLY(curgen)
                ->points.end() - 1; currentpt++) {
	 newdist = finddist(currentpt, currentpt + 1, &areawin->save);
	 if (newdist <= sqrwirelim) break;
      }
      if ((!(TOPOLY(curgen)->style & UNCLOSED)) && (newdist > sqrwirelim))
         newdist = finddist(currentpt, TOPOLY(curgen)->points.begin(),
		&areawin->save);

      if (newdist <= sqrwirelim) selected = true;
   }
   return selected;
}

/*------------------------------------------------------*/
/* Check to see if any selection has registered cycles	*/
/*------------------------------------------------------*/

bool checkforcycles(short *selectlist, int selects)
{
   genericptr pgen;
   pointselect *cycptr;
   short *ssel;

   for (ssel = selectlist; ssel < selectlist + selects; ssel++) {
      pgen = SELTOGENERIC(ssel);
      switch(pgen->type) {
         case POLYGON:
            cycptr = ((polyptr)pgen)->cycle;
            break;
         case ARC:
            cycptr = ((arcptr)pgen)->cycle;
            break;
         case SPLINE:
            cycptr = ((splineptr)pgen)->cycle;
            break;
         case LABEL:
            cycptr = ((labelptr)pgen)->cycle;
            break;
      }
      if (cycptr != NULL)
         if (cycptr->number != -1)
            return true;
   }
   return false;
}

/*--------------------------------------------------------------*/
/* Copy a cycle selection list from one element to another	*/
/*--------------------------------------------------------------*/

void copycycles(pointselect **newl, pointselect * const*old)
{
   pointselect *pptr;
   short cycles = 0;

   if (*old == NULL) {
      *newl = NULL;
      return;
   }

   for (pptr = *old; !(pptr->flags & LASTENTRY); pptr++, cycles++);
   cycles += 2;
   *newl = (pointselect *)malloc(cycles * sizeof(pointselect));
   memcpy(*newl, *old, cycles * sizeof(pointselect));
}

/*--------------------------------------------------------------*/
/* Create a selection record of selected points in an element.	*/
/* If a record already exists, and "cycle" is not already in	*/
/* the list, add it.						*/
/* "flags" may be set to EDITX or EDITY.  If "flags" is zero,	*/
/* then flags = EDITX | EDITY is assumed.			*/
/*--------------------------------------------------------------*/

pointselect *addcycle(genericptr *pgen, short cycle, u_char flags)
{
   polyptr ppoly;
   arcptr parc;
   splineptr pspline;
   labelptr plabel;
   pointselect *pptr, **cycptr;
   short cycles = 0;

   switch((*pgen)->type) {
      case POLYGON:
         ppoly = TOPOLY(pgen);
         cycptr = &ppoly->cycle;
         break;
      case ARC:
         parc = TOARC(pgen);
         cycptr = &parc->cycle;
         break;
      case SPLINE:
         pspline = TOSPLINE(pgen);
         cycptr = &pspline->cycle;
         break;
      case LABEL:
         plabel = TOLABEL(pgen);
         cycptr = &plabel->cycle;
         break;
   }

   switch((*pgen)->type) {
      case POLYGON:
      case ARC:
      case SPLINE:
      case LABEL:		// To-do:  Handle labels separately

         if (*cycptr == NULL) {
            *cycptr = (pointselect *)malloc(sizeof(pointselect));
            pptr = *cycptr;
            pptr->number = cycle;
            pptr->flags = (flags == 0) ? EDITX | EDITY : flags;
            pptr->flags |= LASTENTRY;
         }
         else {
            for (pptr = *cycptr; !(pptr->flags & LASTENTRY); pptr++, cycles++) {
               if (pptr->number == cycle)
                  break;
               pptr->flags &= ~LASTENTRY;
            }
            if (pptr->number != cycle) {
               pptr->flags &= ~LASTENTRY;
               *cycptr = (pointselect *)realloc(*cycptr,
                        (cycles + 2) * sizeof(pointselect));
               pptr = *cycptr + cycles + 1;
               pptr->number = cycle;
               pptr->flags = (flags == 0) ? EDITX | EDITY : flags;
               pptr->flags |= LASTENTRY;
            }
            else {
               pptr->flags |= (flags == 0) ? EDITX | EDITY : flags;
            }
         }
         break;
   }
   return pptr;
}

/*--------------------------------------------------------------*/
/* Find the cycle numbered "cycle", and mark it as the		*/
/* reference point.						*/
/*--------------------------------------------------------------*/

void makerefcycle(pointselect *cycptr, short cycle)
{
   pointselect *pptr, *sptr;

   for (pptr = cycptr;; pptr++) {
      if (pptr->flags & REFERENCE) {
         pptr->flags &= ~REFERENCE;
         break;
      }
      if (pptr->flags & LASTENTRY) break;
   }

   for (sptr = cycptr;; sptr++) {
      if (sptr->number == cycle) {
         sptr->flags |= REFERENCE;
         break;
      }
      if (sptr->flags & LASTENTRY) break;
   }

   /* If something went wrong, revert to the original reference */

   if (!(sptr->flags & REFERENCE)) {
      pptr->flags |= REFERENCE;
   }
}

/* Original routine, used 1st entry as reference (deprecated) */

void makefirstcycle(pointselect *cycptr, short cycle)
{
   pointselect *pptr, tmpp;

   for (pptr = cycptr;; pptr++) {
      if (pptr->number == cycle) {
         /* swap with first entry */
         tmpp = *cycptr;
         *cycptr = *pptr;
         *pptr = tmpp;
         if (cycptr->flags & LASTENTRY) {
            cycptr->flags &= ~LASTENTRY;
            pptr->flags |= LASTENTRY;
         }
         return;
      }
      if (pptr->flags & LASTENTRY) break;
   }
}

/*------------------------------------------------------------------------------*/
/* Advance a cycle (point) selection from value "cycle" to value "newvalue"	*/
/* If "newvalue" is < 0 then remove the cycle.					*/
/*										*/
/* If there is only one cycle point on the element, then advance its point	*/
/* number.  If there are multiple points on the element, then change the	*/
/* reference point by moving the last item in the list to the front.		*/
/*------------------------------------------------------------------------------*/

void advancecycle(genericptr *pgen, short newvalue)
{
   polyptr ppoly;
   arcptr parc;
   splineptr pspline;
   labelptr plabel;
   pointselect *pptr, *endptr, *fcycle, **cycptr, tmpcyc;

   if (newvalue < 0) {
      removecycle(*pgen);
      return;
   }

   switch((*pgen)->type) {
      case POLYGON:
         ppoly = TOPOLY(pgen);
         cycptr = &ppoly->cycle;
         break;
      case ARC:
         parc = TOARC(pgen);
         cycptr = &parc->cycle;
         break;
      case SPLINE:
         pspline = TOSPLINE(pgen);
         cycptr = &pspline->cycle;
         break;
      case LABEL:
         plabel = TOLABEL(pgen);
         cycptr = &plabel->cycle;
         break;
   }
   if (*cycptr == NULL) return;

   /* Remove any cycles that have only X or Y flags set.	*/
   /* "Remove" them by shuffling them to the end of the list,	*/
   /* and marking the one in front as the last entry.		*/

   for (endptr = *cycptr; !(endptr->flags & LASTENTRY); endptr++);
   pptr = *cycptr;
   while (pptr < endptr) {
      if ((pptr->flags & (EDITX | EDITY)) != (EDITX | EDITY)) {
         tmpcyc = *endptr;
         *endptr = *pptr;
         *pptr = tmpcyc;
         pptr->flags &= ~LASTENTRY;
         pptr->number = -1;
         endptr--;
         endptr->flags |= LASTENTRY;
      }
      else
         pptr++;
   }

   if (pptr->flags & LASTENTRY) {
      if ((pptr->flags & (EDITX | EDITY)) != (EDITX | EDITY)) {
         pptr->flags &= ~LASTENTRY;
         pptr->number = -1;
         endptr--;
         endptr->flags |= LASTENTRY;
      }
   }

   /* Now advance the cycle */

   pptr = *cycptr;
   if (pptr->flags & LASTENTRY) {
      pptr->number = newvalue;
   }
   else {
      fcycle = *cycptr;
      for (pptr = fcycle + 1;; pptr++) {
         if (pptr->flags & (EDITX | EDITY))
            fcycle = pptr;
         if (pptr->flags & LASTENTRY) break;
      }
      makerefcycle(*cycptr, fcycle->number);
   }
}

/*--------------------------------------*/
/* Remove a cycle (point) selection	*/
/*--------------------------------------*/

void removecycle(genericptr pgen)
{
   polyptr ppoly;
   pathptr ppath;
   arcptr parc;
   splineptr pspline;
   labelptr plabel;
   pointselect **cycptr = NULL;

   switch(pgen->type) {
      case POLYGON:
         ppoly = TOPOLY(&pgen);
         cycptr = &ppoly->cycle;
         break;
      case ARC:
         parc = TOARC(&pgen);
         cycptr = &parc->cycle;
         break;
      case SPLINE:
         pspline = TOSPLINE(&pgen);
         cycptr = &pspline->cycle;
         break;
      case LABEL:
         plabel = TOLABEL(&pgen);
         cycptr = &plabel->cycle;
         break;
      case PATH:
         ppath = TOPATH(&pgen);
         std::for_each(ppath->begin(), ppath->end(), removecycle);
         break;
   }
   if (cycptr == NULL) return;
   if (*cycptr == NULL) return;
   free(*cycptr);
   *cycptr = NULL;
}

/*--------------------------------------*/
/* Select one of the elements on-screen */
/*--------------------------------------*/

selection *genselectelement(short class_, u_char mode, objectptr selobj,
		objinstptr selinst)
{
   selection	*rselect = NULL;
   genericptr	*curgen;
   XPoint 	newboxpts[4];
   bool	selected;
   float	range = RANGE_NARROW;

   if (mode == MODE_RECURSE_WIDE)
      range = RANGE_WIDE;

   /* Loop through all elements found underneath the cursor */

   for (curgen = selobj->begin(); curgen != selobj->end(); curgen++) {

      selected = false;

      /* Check among polygons, arcs, and curves */

      if (((*curgen)->type == (class_ & POLYGON)) ||
                ((*curgen)->type == (class_ & ARC)) ||
                ((*curgen)->type == (class_ & SPLINE))) {
          selected = pathselect(curgen, class_, range);
      }

      else if ((*curgen)->type == (class_ & LABEL)) {

         /* Look among the labels */

	 labelptr curlab = TOLABEL(curgen);

	 /* Don't select temporary labels from schematic capture system */
	 if (curlab->string->type != FONT_NAME) continue;

         curlab->getBbox(newboxpts, selinst);

	 /* Need to check for zero-size boxes or test_insideness()	*/
	 /* fails.  Zero-size boxes happen when labels are parameters	*/
	 /* set to a null string.					*/

	 if ((newboxpts[0].x != newboxpts[1].x) || (newboxpts[0].y !=
		newboxpts[1].y)) {
	
            /* check for point inside bounding box, as for objects */

	    selected = test_insideness(areawin->save.x, areawin->save.y,
		newboxpts);
	    if (selected) areawin->textpos = areawin->textend = 0;
	 }
      }

      else if ((*curgen)->type == (class_ & GRAPHIC)) {

         /* Look among the graphic images */

	 graphicptr curg = TOGRAPHIC(curgen);
         curg->getBbox(newboxpts);

         /* check for point inside bounding box, as for objects */
	 selected = test_insideness(areawin->save.x, areawin->save.y,
		newboxpts);
      }

      else if ((*curgen)->type == (class_ & PATH)) {

         /* Check among the paths */

	 genericptr *pathp;

         /* Accept path if any subcomponent of the path is accepted */

         for (pathp = TOPATH(curgen)->begin(); pathp != TOPATH(curgen)->end(); pathp++)
	    if (pathselect(pathp, SPLINE|ARC|POLYGON, range)) {
               selected = true;
	       break;
	    }
      }

      else if ((*curgen)->type == (class_ & OBJINST)) {

         TOOBJINST(curgen)->getBbox(newboxpts, (int)range);

         /* Look for an intersect of the boundingbox and pointer position. */
         /* This is a simple matter of rotating the pointer position with  */
         /*  respect to the origin of the bounding box segment, as if the  */
         /*  segment were rotated to 0 degrees.  The sign of the resulting */
         /*  point's y-position is the same for all bbox segments if the   */
         /*  pointer position is inside the bounding box.		   */

	 selected = test_insideness(areawin->save.x, areawin->save.y,
		newboxpts);
      }

      /* Add this object to the list of things found under the cursor */

      if (selected) {
         if (rselect == NULL) {
            rselect = new selection;
	    rselect->selectlist = (short *)malloc(sizeof(short));
	    rselect->selects = 0;
	    rselect->thisinst = selinst;
	    rselect->next = NULL;
	 }
         else {
            rselect->selectlist = (short *)realloc(rselect->selectlist,
			(rselect->selects + 1) * sizeof(short));
	 }
	 *(rselect->selectlist + rselect->selects) = (short)(curgen -
                selobj->begin());
	 rselect->selects++;
      }
   }
   return rselect;
}

/*----------------------------------------------------------------*/
/* select arc, curve, and polygon objects from a defined box area */
/*----------------------------------------------------------------*/

bool areaelement(genericptr *curgen, bool is_path, short level)
{
   bool selected;
   XPoint* currentpt;
   short cycle;

   switch(ELEMENTTYPE(*curgen)) {

      case(ARC):
	   /* check center of arcs */

      	   selected = (TOARC(curgen)->position.x < areawin->save.x) &&
	  	(TOARC(curgen)->position.x > areawin->origin.x) &&
		(TOARC(curgen)->position.y < areawin->save.y) &&
		(TOARC(curgen)->position.y > areawin->origin.y);
	   break;

      case(POLYGON):
	   /* check each point of the polygons */

	   selected = false;
           cycle = 0;
           for (currentpt = TOPOLY(curgen)->points.begin(); currentpt <
                TOPOLY(curgen)->points.end();
                currentpt++, cycle++) {
              if ((currentpt->x < areawin->save.x) && (currentpt->x >
	           areawin->origin.x) && (currentpt->y < areawin->save.y) &&
	           (currentpt->y > areawin->origin.y)) {
	         selected = true;
                 if (level == 0) addcycle(curgen, cycle, 0);
	      }
           }
	   break;

      case(SPLINE):
	   /* check each control point of the spline */
	
           selected = false;
           if ((TOSPLINE(curgen)->ctrl[0].x < areawin->save.x) &&
	   	(TOSPLINE(curgen)->ctrl[0].x > areawin->origin.x) &&
		(TOSPLINE(curgen)->ctrl[0].y < areawin->save.y) &&
                (TOSPLINE(curgen)->ctrl[0].y > areawin->origin.y)) {
               selected = true;
               if (level == 0) addcycle(curgen, 0, 0);
           }
           if ((TOSPLINE(curgen)->ctrl[3].x < areawin->save.x) &&
		(TOSPLINE(curgen)->ctrl[3].x > areawin->origin.x) &&
		(TOSPLINE(curgen)->ctrl[3].y < areawin->save.y) &&
                (TOSPLINE(curgen)->ctrl[3].y > areawin->origin.y)) {
               selected = true;
               if (level == 0) addcycle(curgen, 3, 0);
           }

	   break;
   }
   return selected;
}

/*--------------------------------------------*/
/* select all objects from a defined box area */
/*--------------------------------------------*/

bool selectarea(objectptr selobj, short level)
{
   short	*newselect;
   genericptr   *curgen, *pathgen;
   bool	selected;
   stringpart	*strptr;
   int		locpos;
   objinstptr	curinst;
   XPoint	saveorig, savesave, tmppt;

   /* put points of area bounding box into proper order */

   if (areawin->origin.y > areawin->save.y) {
      short tmp;
      tmp = areawin->origin.y;
      areawin->origin.y = areawin->save.y;
      areawin->save.y = tmp;
   }
   if (areawin->origin.x > areawin->save.x) {
      std::swap(areawin->origin.x, areawin->save.x);
   }
   if (selobj == topobject) {
      areawin->textpos = areawin->textend = 0;
   } 

   for (curgen = selobj->begin(); curgen != selobj->end(); curgen++) {

      /* apply the selection filter */
      if (!((*curgen)->type & areawin->filter)) continue;

      switch(ELEMENTTYPE(*curgen)) {
	case(OBJINST):
	    curinst = TOOBJINST(curgen);

	    /* An object instance is selected if any part of it is	*/
	    /* selected on a recursive area search.			*/
            savesave = areawin->save;
            InvTransformPoints(&areawin->save, &tmppt, 1, curinst->position,
			curinst->scale, curinst->rotation);
            areawin->save = tmppt;

            saveorig = areawin->origin;
            InvTransformPoints(&areawin->origin, &tmppt, 1, curinst->position,
			curinst->scale, curinst->rotation);
            areawin->origin = tmppt;

            {
                Matrix ctm;
                /// \todo this may be wrong
                //UPushCTM();
                ctm.preMult(curinst->position, curinst->scale,
                            curinst->rotation);
                selected = selectarea(curinst->thisobject, level + 1);
                //UPopCTM();
            }
            areawin->save = savesave;
            areawin->origin = saveorig;
	    break;

	case(GRAPHIC):
           /* check for graphic image center point inside area box */
           selected = (TOGRAPHIC(curgen)->position.x < areawin->save.x) &&
		(TOGRAPHIC(curgen)->position.x > areawin->origin.x) &&
		(TOGRAPHIC(curgen)->position.y < areawin->save.y) &&
		(TOGRAPHIC(curgen)->position.y > areawin->origin.y);
	   break;

        case(LABEL): {
	   XPoint bboxpts[4], newboxpts[4], adj;
	   labelptr slab = TOLABEL(curgen);
	   short j, state, isect, tmpl1, tmpl2;
	   TextExtents tmpext;

	   selected = false;

	   /* Ignore temporary labels created by the netlist generator */
	   if (slab->string->type != FONT_NAME) break;

	   /* Ignore info and pin labels that are not on the top level */
	   if ((selobj != topobject) && (slab->pin != false)) break;

	   /* create a 4-point polygon out of the select box information */
	   bboxpts[0].x = bboxpts[3].x = areawin->origin.x;
	   bboxpts[0].y = bboxpts[1].y = areawin->origin.y;
	   bboxpts[1].x = bboxpts[2].x = areawin->save.x;
	   bboxpts[2].y = bboxpts[3].y = areawin->save.y;

   	   /* translate select box into the coordinate system of the label */
	   InvTransformPoints(bboxpts, newboxpts, 4, slab->position,
		  slab->scale, slab->rotation);

	   if (slab->pin) {
	      for (j = 0; j < 4; j++) 
		 pinadjust(slab->justify, &(newboxpts[j].x),
			&(newboxpts[j].y), -1);
	   }

           tmpext = ULength(NULL, slab, areawin->topinstance, 0.0, 0, NULL);
	   adj.x = (slab->justify & NOTLEFT ? (slab->justify & RIGHT ? 
			tmpext.width : tmpext.width >> 1) : 0);
	   adj.y = (slab->justify & NOTBOTTOM ? (slab->justify & TOP ? 
			tmpext.ascent : (tmpext.ascent + tmpext.base) >> 1)
			: tmpext.base);
	   
	   /* Label selection:  For each character in the label string, */
	   /* do an insideness test with the select box.		*/

	   state = tmpl2 = 0;
	   for (j = 0; j < stringlength(slab->string, true, areawin->topinstance); j++) {
	      strptr = findstringpart(j, &locpos, slab->string, areawin->topinstance);
	      if (locpos < 0) continue;	  /* only look at printable characters */
	      if (strptr->type == RETURN) tmpl2 = 0;
	      tmpl1 = tmpl2;
              tmpext = ULength(NULL, slab, areawin->topinstance, 0.0, j + 1, NULL);
	      tmpl2 = tmpext.width;
	      isect = test_insideness(((tmpl1 + tmpl2) >> 1) - adj.x,
			(tmpext.base + (BASELINE >> 1)) - adj.y, newboxpts);

	      /* tiny state machine */
	      if (state == 0) {
		 if (isect) {
		    state = 1;
		    selected = true;
		    areawin->textend = j;
		    if ((areawin->textend > 1) && strptr->type != TEXT_STRING)
		       areawin->textend--;
		 }
	      }
	      else {
		 if (!isect) {
		    areawin->textpos = j;
		    state = 2;
		    break;
		 }
	      }
	   }
	   if (state == 1) areawin->textpos = j;   /* selection goes to end of string */
	   } break;
	   
	case(PATH):
	   /* check position point of each subpart of the path */

	   selected = false;
           for (pathgen = TOPATH(curgen)->begin(); pathgen != TOPATH(curgen)->end(); pathgen++) {
              if (areaelement(pathgen, true, level)) selected = true;
	   }
	   break;

	default:
           selected = areaelement(curgen, false, level);
	   break;
      }

      /* on recursive searches, return as soon as we find something */

      if ((selobj != topobject) && selected) return true;

      /* check if this part has already been selected */

      if (selected)
         for (newselect = areawin->selectlist; newselect <
              areawin->selectlist + areawin->selects; newselect++)
            if (*newselect == (short)(curgen - topobject->begin()))
                  selected = false;

      /* add to list of selections */

      if (selected) {
         newselect = allocselect();
         *newselect = (short)(curgen - topobject->begin());
      }
   }
   if (selobj != topobject) return false;
   setoptionmenu();

   /* if none or > 1 label has been selected, cancel any textpos placement */

   if (!checkselect(LABEL) || areawin->selects != 1 ||
	(areawin->selects == 1 && SELECTTYPE(areawin->selectlist) != LABEL)) {
      areawin->textpos = areawin->textend = 0;
   }

   /* Register the selection as an undo event */
   register_for_undo(XCF_Select, UNDO_DONE, areawin->topinstance,
		areawin->selectlist, areawin->selects);

   /* Drawing of selected objects will take place when drawarea() is */
   /* executed after the button release.			     */

#ifdef TCL_WRAPPER
   if (xobjs.suspend < 0)
      XcInternalTagCall(xcinterp, 2, "select", "here");
#endif

   return selected;
}

/*------------------------*/
/* start deselection mode */
/*------------------------*/

void startdesel(QAction* w, void* clientdata, void* calldata)
{
   if (eventmode == NORMAL_MODE) {
      if (areawin->selects == 0)
	 Wprintf("Nothing to deselect!");
      else if (areawin->selects == 1)
	 unselect_all();
   }
}

/*------------------------------------------------------*/
/* Redraw all the selected objects in the select color.	*/
/*------------------------------------------------------*/

void draw_all_selected(Context* ctx)
{
   int j;

   if (areawin->hierstack != NULL) return;

   for (j = 0; j < areawin->selects; j++)
      gendrawselected(ctx, areawin->selectlist + j, topobject, areawin->topinstance);
}

/*--------------------------------------------------------------*/
/* Free memory from the previous selection list, copy the	*/
/* current selection list to the previous selection list, and	*/
/* zero out the current selection list.				*/
/* Normally one would use clearselects();  use freeselects()	*/
/* only if the menu/toolbars are going to be updated later in	*/
/* the call.							*/
/*--------------------------------------------------------------*/

void freeselects()
{
   if (areawin->selects > 0)
      free(areawin->selectlist);
   areawin->selects = 0;
   free_stack(&areawin->hierstack);
}

/*--------------------------------------------------------------*/
/* Free memory from the selection list and set menu/toolbar	*/
/* items back to default values.				*/
/*--------------------------------------------------------------*/

void clearselects_noundo()
{
   if (areawin->selects > 0) {
      reset_cycles();
      freeselects();
      if (xobjs.suspend < 0) {
         setallstylemarks(areawin->style);
         setcolormark(areawin->color);
         setdefaultfontmarks();
	 setparammarks(NULL);
      }

#ifdef TCL_WRAPPER
      if (xobjs.suspend < 0)
         XcInternalTagCall(xcinterp, 2, "unselect", "all");
#endif
   }
}

/*--------------------------------------------------------------*/
/* Same as above, but registers an undo event.			*/
/*--------------------------------------------------------------*/

void clearselects()
{
   if (areawin->selects > 0) {
      register_for_undo(XCF_Select, UNDO_DONE, areawin->topinstance,
		NULL, 0);
      clearselects_noundo();
   }
}

/*--------------------------------------------------------------*/
/* Unselect all the selected elements and free memory from the	*/
/* selection list.						*/
/*--------------------------------------------------------------*/

void unselect_all()
{
   clearselects();
}

/*----------------------------------------------------------------------*/
/* Select the nearest element, searching the hierarchy if necessary.	*/
/* Return an pushlist pointer to a linked list containing the 		*/
/* hierarchy of objects, with the topmost pushlist also containing a	*/
/* pointer to the polygon found.					*/
/* Allocates memory for the returned linked list which must be freed by	*/
/* the calling routine.							*/
/*----------------------------------------------------------------------*/

selection *recurselect(short class_, u_char mode, pushlistptr *seltop)
{
   selection *rselect, *rcheck, *lastselect;
   genericptr rgen;
   short i;
   objectptr selobj;
   objinstptr selinst;
   XPoint savesave, tmppt;
   pushlistptr selnew;
   short j, unselects;
   u_char locmode = (mode == MODE_CONNECT) ? UNDO_DONE : mode;
   u_char recmode = (mode != MODE_CONNECT) ? MODE_RECURSE_WIDE : MODE_RECURSE_NARROW;

   if (*seltop == NULL) {
      Fprintf(stderr, "Error: recurselect called with NULL pushlist pointer\n");
      return NULL;
   }

   selinst = (*seltop)->thisinst;
   selobj = selinst->thisobject;

   class_ &= areawin->filter;		/* apply the selection filter */

   unselects = 0;
   rselect = genselectelement(class_, locmode, selobj, selinst);
   if (rselect == NULL) return NULL;

   for (i = 0; i < rselect->selects; i++) {
      rgen = selobj->at(rselect->selectlist[i]);
      if (rgen->type == OBJINST) {
         selinst = TOOBJINST(selobj->begin() + rselect->selectlist[i]);

         /* Link hierarchy information to the pushlist linked list */
         selnew = new pushlist;
         selnew->thisinst = selinst;
         selnew->next = NULL;
         (*seltop)->next = selnew;
	 
         /* Translate areawin->save into object's own coordinate system */
         savesave = areawin->save;
         InvTransformPoints(&areawin->save, &tmppt, 1, selinst->position,
			selinst->scale, selinst->rotation);
         areawin->save = tmppt;
         /* Fprintf(stdout, "objinst %s found in object %s; searching recursively\n",
			selinst->thisobject->name, selobj->name); */
         /* Fprintf(stdout, "cursor position originally (%d, %d); "
			"in new object is (%d, %d)\n",
			savesave.x, savesave.y,
			areawin->save.x, areawin->save.y); */
         Matrix ctm;
         /// \todo this may be wrong
         //UPushCTM();
         ctm.preMult(selinst->position, selinst->scale, selinst->rotation);
         rcheck = recurselect(ALL_TYPES, recmode, &selnew);
         //UPopCTM();
         areawin->save = savesave;

	 /* If rgen is NULL, remove selected object from the list, and	*/
	 /* remove the last entry from the pushlist stack.		*/

	 if (rcheck == NULL) {
	    *(rselect->selectlist + i) = -1;
	    unselects++;
	    (*seltop)->next = NULL;
	    if (selnew->next != NULL)
	       Fprintf(stderr, "Error: pushstack was freed, but was not empty!\n");
	    free(selnew);
	 }
	 else {
	    for (lastselect = rselect; lastselect->next != NULL; lastselect =
			lastselect->next);
	    lastselect->next = rcheck;
	 }
      }
   }

   /* Modify the selection list */

   for (i = 0, j = 0; i < rselect->selects; i++) {
      if (*(rselect->selectlist + i) >= 0) {
	 if (i != j)
	    *(rselect->selectlist + j) = *(rselect->selectlist + i);
	 j++;
      }
   }
   rselect->selects -= unselects;
   if (rselect->selects == 0) {
      delete rselect;
      rselect = NULL;
   }
   return rselect;
}

/*----------------------------------*/
/* Start drawing a select area box. */
/*----------------------------------*/

void startselect()
{
   eventmode = SELAREA_MODE;
   areawin->origin = areawin->save;
   areawin->update();

#ifdef TCL_WRAPPER
   Tk_CreateEventHandler(areawin->viewport, ButtonMotionMask,
		(Tk_EventProc *)xctk_drag, NULL);
#else
   xcAddEventHandler(areawin->viewport, ButtonMotionMask, false,
		(XtEventHandler)xlib_drag, NULL);
#endif

}

/*-------------------------*/
/* Track a select area box */
/*-------------------------*/

void trackselarea()
{
   XPoint newpos;

   newpos = UGetCursorPos();
   if (newpos == areawin->save) return;

   areawin->update();
   areawin->save = newpos;
}

/*----------------------*/
/* Track a rescale box	*/
/*----------------------*/

void trackrescale()
{
   XPoint newpos;

   newpos = UGetCursorPos();
   if (newpos == areawin->save) return;

   areawin->update();
   areawin->save = newpos;
}

/*----------------------------------------------------------------------*/
/* Polygon distance comparison function for qsort			*/
/*----------------------------------------------------------------------*/

int dcompare(const void *a, const void *b)
{
   XPoint cpt;
   genericptr agen, bgen;
   short j, k, adist, bdist;

   cpt = areawin->save;

   j = *((short *)a);
   k = *((short *)b);

   agen = topobject->at(j);
   bgen = topobject->at(k);

   if (agen->type != POLYGON || bgen->type != POLYGON) return 0;

   adist = closedistance((polyptr)agen, &cpt);
   bdist = closedistance((polyptr)bgen, &cpt);

   if (adist == bdist) return 0;
   return (adist < bdist) ? 1 : -1;
}

/*----------------------------------------------------------------------*/
/* Compare two selection linked lists					*/
/*----------------------------------------------------------------------*/

bool compareselection(selection *sa, selection *sb)
{
   int i, j, match;
   short n1, n2;

   if ((sa == NULL) || (sb == NULL)) return false;
   if (sa->selects != sb->selects) return false;
   match = 0;
   for (i = 0; i < sa->selects; i++) {
      n1 = *(sa->selectlist + i);
      for (j = 0; j < sb->selects; j++) {
         n2 = *(sb->selectlist + j);
	 if (n1 == n2) {
	    match++;
	    break;
	 }
      }
   }
   return match == sa->selects;
}

/*----------------------------------------------------------------------*/
/* Add pin cycles connected to selected labels				*/
/*----------------------------------------------------------------------*/

void label_connect_cycles(labelptr thislab)
{
   genericptr *pgen;
   bool is_selected;
   XPoint *testpt;
   polyptr cpoly;
   short *stest, cycle;

   if (thislab->pin == LOCAL || thislab->pin == GLOBAL) {
      for (pgen = topobject->begin(); pgen != topobject->end(); pgen++) {
	 /* Ignore any wires that are already selected */
         is_selected = false;
	 for (stest = areawin->selectlist; stest < areawin->selectlist +
			areawin->selects; stest++) {
	    if (SELTOGENERIC(stest) == *pgen) {
               is_selected = true;
	       break;
	    }
	 }
	 if (ELEMENTTYPE(*pgen) == POLYGON) {
	    cpoly = TOPOLY(pgen);
	    if (!is_selected) {
	       cycle = 0;
               for (testpt = cpoly->points.begin(); testpt < cpoly->points.end(); testpt++) {
                  if (*testpt == thislab->position) {
                     addcycle(pgen, cycle, 0);
		     break;
		  }
		  else
		     cycle++;
	       }
	    }
	    else {
	       /* Make sure that this polygon's cycle is not set! */
               removecycle(*pgen);
	    }
	 }
      }
   }
}

/*----------------------------------------------------------------------*/
/* Add pin cycles connected to selected instances			*/
/*----------------------------------------------------------------------*/

void inst_connect_cycles(objinstptr thisinst)
{
   genericptr *pgen;
   bool is_selected;
   XPoint refpoint;
   polyptr cpoly;
   short *stest, cycle;
   objectptr thisobj = thisinst->thisobject;

   for (labeliter clab; thisobj->values(clab); ) {
	 if (clab->pin == LOCAL || clab->pin == GLOBAL) {
	    ReferencePosition(thisinst, &clab->position, &refpoint);
            for (pgen = topobject->begin(); pgen != topobject->end(); pgen++) {
	       /* Ignore any wires that are already selected */
               is_selected = false;
	       for (stest = areawin->selectlist; stest < areawin->selectlist +
			areawin->selects; stest++) {
		  if (SELTOGENERIC(stest) == *pgen) {
                     is_selected = true;
		     break;
		  }
	       }
	       if (ELEMENTTYPE(*pgen) == POLYGON) {
		  cpoly = TOPOLY(pgen);
	          if (!is_selected) {
		     cycle = 0;
                     foreach (XPoint testpt, cpoly->points) {
                         if (testpt == refpoint) {
                             addcycle(pgen, cycle, 0);
                             break;
                         }
                         else
                             cycle++;
                     }
		  }
	          else {
		     /* Make sure that this polygon's cycle is not set! */
                     removecycle(*pgen);
		  }
	       }
	    }
	 }
   }
}

/*----------------------------------------------------------------------*/
/* Select connected pins on all selected object instances and labels	*/
/*----------------------------------------------------------------------*/

void select_connected_pins()
{
   short *selptr;
   objinstptr selinst;
   labelptr sellab;

   if (!areawin->pinattach) return;

   for (selptr = areawin->selectlist; selptr < areawin->selectlist +
		areawin->selects; selptr++) {
      switch (SELECTTYPE(selptr)) {
	 case LABEL:
            sellab = SELTOLABEL(selptr);
            label_connect_cycles(sellab);
	    break;
	 case OBJINST:
            selinst = SELTOOBJINST(selptr);
            inst_connect_cycles(selinst);
	    break;
      }
   }
}

/*----------------------------------------------------------------------*/
/* Reset all polygon cycles flagged during a move (polygon wires	*/
/* connected to pins of an object instance).				*/
/*----------------------------------------------------------------------*/

void reset_cycles()
{
    std::for_each(topobject->begin(), topobject->end(), removecycle);
}

/*----------------------------------------------------------------------*/
/* Recursive selection mechanism					*/
/*----------------------------------------------------------------------*/

short *recurse_select_element(short class_, u_char mode) {
   pushlistptr seltop, nextptr;
   selection *rselect;
   short *newselect, localpick;
   static short pick = 0;
   static selection *saveselect = NULL;
   int i, j, k, ilast, jlast;
   bool unselect = false;

   seltop = new pushlist;
   seltop->thisinst = areawin->topinstance;
   seltop->next = NULL;

   /* Definition for unselecting an element */

   if (class_ < 0) {
      unselect = true;
      class_ = -class_;
   }
   rselect = recurselect(class_, mode, &seltop);

   if (rselect) {
      /* Order polygons according to nearest point distance. */
      qsort((void *)rselect->selectlist, (size_t)rselect->selects,
		sizeof(short), dcompare);

      if (compareselection(rselect, saveselect))
	 pick++;
      else
	 pick = 0;

      localpick = pick % rselect->selects;
   }

   /* Mechanism for unselecting elements under the cursor	*/
   /* (Unselect all picked objects)				*/

   if (rselect && unselect) {

      ilast = -1;
      k = 0;
      for (i = 0; i < rselect->selects; i++) {
	 for (j = 0; j < areawin->selects; j++) {
	    if (*(areawin->selectlist + j) == *(rselect->selectlist + i)) {
	       jlast = j;
	       ilast = i;
	       if (++k == localpick)
	          break;
	    }
	 }
	 if (j < areawin->selects) break;
      }
      if (ilast >= 0) {
         newselect = rselect->selectlist + ilast;
         areawin->selects--;
         for (k = jlast; k < areawin->selects; k++)
	    *(areawin->selectlist + k) = *(areawin->selectlist + k + 1);

         if (areawin->selects == 0) freeselects();

         /* Register the selection as an undo event */
         register_for_undo(XCF_Select, mode, areawin->topinstance,
		areawin->selectlist, areawin->selects);
      }
   }

   else if (rselect) {

      /* Mechanism for selecting objects:			*/
      /* Count all elements from rselect that are part of	*/
      /* the current selection.  Pick the "pick"th item (modulo	*/
      /* total number of items).				*/

      ilast = -1;
      k = 0;
      for (i = 0; i < rselect->selects; i++) {
	 for (j = 0; j < areawin->selects; j++) {
	    if (*(areawin->selectlist + j) == *(rselect->selectlist + i))
	       break;
	 }
	 if (j == areawin->selects) {
	    ilast = i;
	    if (++k == localpick)
	       break;
	 }
      }

      if (ilast >= 0) {
         newselect = allocselect();
         *newselect = *(rselect->selectlist + ilast);
         setoptionmenu();
         u2u_snap(areawin->save);

         /* Register the selection as an undo event	*/
	 /* (only if selection changed)			*/

         register_for_undo(XCF_Select, mode, areawin->topinstance,
		areawin->selectlist, areawin->selects);
      }
   }

   /* Cleanup */

   while (seltop != NULL) {
      nextptr = seltop->next;
      free(seltop);
      seltop = nextptr;
   }

   delete saveselect;
   saveselect = rselect;

#ifdef TCL_WRAPPER
   if (xobjs.suspend < 0)
      XcInternalTagCall(xcinterp, 2, "select", "here");
#endif

   return areawin->selectlist;
}
