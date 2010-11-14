/*-------------------------------------------------------------------------*/
/* functions.c 								   */
/* Copyright (c) 2002  Tim Edwards, Johns Hopkins University        	   */
/*-------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------*/
/*      written by Tim Edwards, 8/13/93    				   */
/*-------------------------------------------------------------------------*/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <climits>

#ifdef TCL_WRAPPER 
#include <tk.h>
#endif

#include "matrix.h"
#include "context.h"
#include "xcircuit.h"
#include "prototypes.h"
#include "xcqt.h"
#include "colors.h"
#include "area.h"

#include <QPainter>

/*------------------------------------------------------------------------*/
/* find the squared length of a wire (or distance between two points in   */
/* user space).								  */
/*------------------------------------------------------------------------*/

long sqwirelen(const XPoint *userpt1, const XPoint *userpt2)
{
  long xdist, ydist;

  xdist = (long)userpt2->x - (long)userpt1->x;
  ydist = (long)userpt2->y - (long)userpt1->y;
  return (xdist * xdist + ydist * ydist);
}

/*------------------------------------------------------------------------*/
/* floating-point version of the above					  */
/*------------------------------------------------------------------------*/

float fsqwirelen(XfPoint *userpt1, XfPoint *userpt2)
{
  float xdist, ydist;

  xdist = userpt2->x - userpt1->x;
  ydist = userpt2->y - userpt1->y;
  return (xdist * xdist + ydist * ydist);
}

/*------------------------------------------------------------------------*/
/* Find absolute distance between two points in user space		  */
/*------------------------------------------------------------------------*/

int wirelength(const XPoint *userpt1, const XPoint *userpt2)
{
  u_long xdist, ydist;

  xdist = (long)(userpt2->x) - (long)(userpt1->x);
  ydist = (long)(userpt2->y) - (long)(userpt1->y);
  return (int)sqrt((double)(xdist * xdist + ydist * ydist));
}

/*------------------------------------------------------------------------*/
/* Find the closest (squared) distance from a point to a line		  */
/*------------------------------------------------------------------------*/

long finddist(const XPoint *linept1, const XPoint *linept2, const XPoint *userpt)
{
   long a, b, c, frac;
   float protod;

   c = sqwirelen(linept1, linept2);
   a = sqwirelen(linept1, userpt);
   b = sqwirelen(linept2, userpt);
   frac = a - b;
   if (frac >= c) return b;	  /* "=" is important if c = 0 ! */
   else if (-frac >= c) return a;
   else {
      protod = (float)(c + a - b);
      return (a - (long)((protod * protod) / (float)(c << 2)));
   }
}

/*------------------------------------------------------------------------*/
/* Compute spline coefficients						  */
/*------------------------------------------------------------------------*/

void computecoeffs(splineptr thespline, float *ax, float *bx, float *cx,
	float *ay, float *by, float *cy)
{
   *cx = 3.0 * (float)(thespline->ctrl[1].x - thespline->ctrl[0].x);
   *bx = 3.0 * (float)(thespline->ctrl[2].x - thespline->ctrl[1].x) - *cx;
   *ax = (float)(thespline->ctrl[3].x - thespline->ctrl[0].x) - *cx - *bx;

   *cy = 3.0 * (float)(thespline->ctrl[1].y - thespline->ctrl[0].y);
   *by = 3.0 * (float)(thespline->ctrl[2].y - thespline->ctrl[1].y) - *cy;
   *ay = (float)(thespline->ctrl[3].y - thespline->ctrl[0].y) - *cy - *by;   
}

/*------------------------------------------------------------------------*/
/* Find the (x,y) position and tangent rotation of a point on a spline    */
/*------------------------------------------------------------------------*/

void findsplinepos(splineptr thespline, float t, XPoint *retpoint, int *retrot)
{
   float ax, bx, cx, ay, by, cy;
   float tsq = t * t;
   float tcb = tsq * t;
   double dxdt, dydt;

   computecoeffs(thespline, &ax, &bx, &cx, &ay, &by, &cy);
   retpoint->x = (short)(ax * tcb + bx * tsq + cx * t + (float)thespline->ctrl[0].x);
   retpoint->y = (short)(ay * tcb + by * tsq + cy * t + (float)thespline->ctrl[0].y);

   if (retrot != NULL) {
      dxdt = (double)(3 * ax * tsq + 2 * bx * t + cx);
      dydt = (double)(3 * ay * tsq + 2 * by * t + cy);
      *retrot = (int)(INVRFAC * atan2(dxdt, dydt));  /* reversed y, x */
      if (*retrot < 0) *retrot += 360;
   }
}

/*------------------------------------------------------------------------*/
/* floating-point version of the above					  */
/*------------------------------------------------------------------------*/

void ffindsplinepos(splineptr thespline, float t, XfPoint *retpoint)
{
   float ax, bx, cx, ay, by, cy;
   float tsq = t * t;
   float tcb = tsq * t;

   computecoeffs(thespline, &ax, &bx, &cx, &ay, &by, &cy);
   retpoint->x = ax * tcb + bx * tsq + cx * t + (float)thespline->ctrl[0].x;
   retpoint->y = ay * tcb + by * tsq + cy * t + (float)thespline->ctrl[0].y;
}

/*------------------------------------------------------------------------*/
/* Find the closest distance between a point and a spline and return the  */
/* fractional distance along the spline of this point.			  */
/*------------------------------------------------------------------------*/

float findsplinemin(splineptr thespline, XPoint *upoint)
{
   XfPoint 	*spt, flpt, newspt;
   float	minval = 1000000, tval, hval, ndist;
   short	j, ival;

   flpt.x = (float)(upoint->x);
   flpt.y = (float)(upoint->y);

   /* get estimate from precalculated spline points */

   for (spt = thespline->points; spt < thespline->points + INTSEGS;
	spt++) {
      ndist = fsqwirelen(spt, &flpt);
      if (ndist < minval) {
	 minval = ndist;
	 ival = (short)(spt - thespline->points);
      }
   }
   tval = (float)(ival + 1) / (INTSEGS + 1);
   hval = 0.5 / (INTSEGS + 1);

   /* short fixed iterative loop to converge on minimum t */

   for (j = 0; j < 5; j++) {
      tval += hval;
      ffindsplinepos(thespline, tval, &newspt);
      ndist = fsqwirelen(&newspt, &flpt);
      if (ndist < minval) minval = ndist;
      else {
         tval -= hval * 2;
         ffindsplinepos(thespline, tval, &newspt);
         ndist = fsqwirelen(&newspt, &flpt);
         if (ndist < minval) minval = ndist;
	 else tval += hval;
      }
      hval /= 2;
   }

   if (tval < 0.1) {
      if ((float)sqwirelen(&(thespline->ctrl[0]), upoint) < minval) tval = 0;
   }
   else if (tval > 0.9) {
      if ((float)sqwirelen(&(thespline->ctrl[3]), upoint) < minval) tval = 1;
   }
   return tval;
}

/*----------------------------------------------------------------------------*/
/* Find closest point of a polygon to the cursor			      */
/*----------------------------------------------------------------------------*/

short closepointdistance(polyptr curpoly, XPoint *cursloc, short *mindist)
{
   short curdist;
   XPoint *curpt, *savept; 

   curpt = savept = curpoly->points.begin();
   *mindist = wirelength(curpt, cursloc);
   while (++curpt < curpoly->points.end()) {
      curdist = wirelength(curpt, cursloc);
      if (curdist < *mindist) {
         *mindist = curdist;
         savept = curpt;
      }
   }
   return (short)(savept - curpoly->points.begin());
}

/*----------------------------------------------------------------------------*/
/* Find closest point of a polygon to the cursor			      */
/*----------------------------------------------------------------------------*/

short closepoint(polyptr curpoly, XPoint *cursloc)
{
   short mindist;
   return closepointdistance(curpoly, cursloc, &mindist);
}

/*----------------------------------------------------------------------------*/
/* Find the distance to the closest point of a polygon to the cursor	      */
/*----------------------------------------------------------------------------*/

short closedistance(polyptr curpoly, XPoint *cursloc)
{
   short mindist;
   closepointdistance(curpoly, cursloc, &mindist);
   return mindist;
}

/*----------------------------------------------------------------------------*/
/* Coordinate system transformations 					      */
/*----------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------*/
/*  Check screen bounds:  minimum, maximum scale and translation is determined	*/
/*  by values which fit in an X11 type XPoint (short int).  If the window	*/
/*  extremes exceed type short when mapped to user space, or if the page 	*/
/*  bounds exceed type short when mapped to X11 window space, return error.	*/ 
/*------------------------------------------------------------------------------*/

short checkbounds(Context* ctx)
{
   XPoint testpt;
   long lval;

   /* check window-to-user space */

   lval = 2 * (long)((float) (areawin->width) / areawin->vscale) +
	(long)areawin->pcorner.x;
   if (lval != (long)((short)lval)) return -1;
   lval = 2 * (long)((float) (areawin->height) / areawin->vscale) +
	(long)areawin->pcorner.y;
   if (lval != (long)((short)lval)) return -1;

   /* check user-to-window space */

   lval = (long)((float)(topobject->bbox.lowerleft.x - areawin->pcorner.x) *
	areawin->vscale);
   if (lval != (long)((short)lval)) return -1;
   lval = (long)areawin->height - (long)((float)(topobject->bbox.lowerleft.y -
	areawin->pcorner.y) * areawin->vscale); 
   if (lval != (long)((short)lval)) return -1;
   ctx->CTM().transform(&(topobject->bbox.lowerleft), &testpt, 1);

   lval = (long)((float)(topobject->bbox.lowerleft.x + topobject->bbox.width -
	areawin->pcorner.x) * areawin->vscale);
   if (lval != (long)((short)lval)) return -1;
   lval = (long)areawin->height - (long)((float)(topobject->bbox.lowerleft.y +
	topobject->bbox.height - areawin->pcorner.y) * areawin->vscale); 
   if (lval != (long)((short)lval)) return -1;

   return 0;
}

/*------------------------------------------------------------------------*/
/* Transform X-window coordinate to xcircuit coordinate system		  */
/*------------------------------------------------------------------------*/

void window_to_user(short xw, short yw, XPoint *upt)
{
  float tmpx, tmpy;

  tmpx = (float)xw / areawin->vscale + (float)areawin->pcorner.x;
  tmpy = (float)(areawin->height - yw) / areawin->vscale + 
	(float)areawin->pcorner.y;

  tmpx += (tmpx > 0) ? 0.5 : -0.5;
  tmpy += (tmpy > 0) ? 0.5 : -0.5;

  upt->x = (short)tmpx;
  upt->y = (short)tmpy;
}

/*------------------------------------------------------------------------*/
/* Transform xcircuit coordinate back to X-window coordinate system       */
/*------------------------------------------------------------------------*/

void user_to_window(XPoint upt, XPoint *wpt)
{
  float tmpx, tmpy;

  tmpx = (float)(upt.x - areawin->pcorner.x) * areawin->vscale;
  tmpy = (float)areawin->height - (float)(upt.y - areawin->pcorner.y)
	* areawin->vscale; 

  tmpx += (tmpx > 0) ? 0.5 : -0.5;
  tmpy += (tmpy > 0) ? 0.5 : -0.5;

  wpt->x = (short)tmpx;
  wpt->y = (short)tmpy;
}

/*----------------------------------------------------------------------*/
/* Transformations in the object hierarchy				*/
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/* Get the cursor position						*/
/*----------------------------------------------------------------------*/

XPoint UGetCursor()
{
    return areawin->viewport->mapFromGlobal(QCursor::pos());
}

/*----------------------------------------------------------------------*/
/* Get the cursor position and translate to user coordinates		*/
/*----------------------------------------------------------------------*/

XPoint UGetCursorPos()
{
   XPoint winpos, userpos;
 
   winpos = UGetCursor();

   window_to_user(winpos.x, winpos.y, &userpos);  

   return userpos;
}

/*----------------------------------------------------------------------*/
/* Translate a point to the nearest snap-to grid point			*/
/*----------------------------------------------------------------------*/
/* user coordinates to user coordinates version 			*/

XPoint u2u_getSnapped(XPoint f)
{
    u2u_snap(f);
    return f;
}

void u2u_snap(XPoint &uvalue)
{
   float tmpx, tmpy;
   float tmpix, tmpiy;

   if (areawin->snapto) {
      tmpx = (float)uvalue.x / xobjs.pagelist[areawin->page].snapspace;
      if (tmpx > 0)
	 tmpix = (float)((int)(tmpx + 0.5));
      else
         tmpix = (float)((int)(tmpx - 0.5));

      tmpy = (float)uvalue.y / xobjs.pagelist[areawin->page].snapspace;
      if (tmpy > 0)
         tmpiy = (float)((int)(tmpy + 0.5));
      else
         tmpiy = (float)((int)(tmpy - 0.5));

      tmpix *= xobjs.pagelist[areawin->page].snapspace;
      tmpix += (tmpix > 0) ? 0.5 : -0.5;
      tmpiy *= xobjs.pagelist[areawin->page].snapspace;
      tmpiy += (tmpiy > 0) ? 0.5 : -0.5;

      uvalue.x = (int)tmpix;
      uvalue.y = (int)tmpiy;
   }
}

/*------------------------------------------------------------------------*/
/* window coordinates to user coordinates version 			  */
/*------------------------------------------------------------------------*/

void snap(short valuex, short valuey, XPoint *returnpt)
{
   window_to_user(valuex, valuey, returnpt);  
   u2u_snap(*returnpt);
}

/*----------------------------------------------------------------------*/
/* Adjust wire coords to force a wire to a horizontal or vertical	*/
/* position.								*/
/* "pospt" is the target position for the point of interest.		*/
/* "cycle" is the point number in the polygon of the point of interest.	*/
/* cycle == -1 is equivalent to the last point of the polygon.		*/
/* If "strict" is true then single-segment wires are forced manhattan	*/
/* even if that means that the endpoint drifts from the target point.	*/
/* If "strict" is false then single-segment wires will become non-	*/
/* manhattan so that the target point is reached.			*/
/* NOTE:  It might be preferable to add a segment to maintain a		*/
/* manhattan layout, except that we want to avoid merging nets		*/
/* together. . .							*/
/*----------------------------------------------------------------------*/

void manhattanize(XPoint *pospt, polyptr newpoly, short cycle, bool strict)
{
   XPoint *curpt, *bpt, *bbpt, *fpt, *ffpt;

   if (newpoly->points.count() == 1) return;	/* sanity check */

   if (cycle == -1 || cycle == newpoly->points.count() - 1) {
      curpt = newpoly->points.end() - 1;
      bpt = newpoly->points.end() - 2;
      fpt = NULL;
      ffpt = NULL;
      if (newpoly->points.count() > 2)
         bbpt = newpoly->points.end() - 3;
      else
	 bbpt = NULL;
   } 
   else if (cycle == 0) {
      curpt = newpoly->points.begin();
      fpt = newpoly->points.begin() + 1;
      bpt = NULL;
      bbpt = NULL;
      if (newpoly->points.count() > 2)
         ffpt = newpoly->points.begin() + 2;
      else
	 ffpt = NULL;
   }
   else {
      curpt = newpoly->points.begin() + cycle;
      fpt = newpoly->points.begin() + cycle + 1;
      bpt = newpoly->points.begin() + cycle - 1;
      if (cycle > 1)
         bbpt = newpoly->points.begin() + cycle - 2;
      else
	 bbpt = NULL;

      if (cycle < newpoly->points.count() - 2)
         ffpt = newpoly->points.begin() + cycle + 2;
      else
	 ffpt = NULL;
   }

   /* enforce constraints on point behind cycle position */

   if (bpt != NULL) {
      if (bbpt != NULL) {
         if (bpt->x == bbpt->x) bpt->y = pospt->y;
         if (bpt->y == bbpt->y) bpt->x = pospt->x;
      }
      else if (strict) {
         XPoint delta = abs(*bpt - *pospt);

         /* Only one segment---just make sure it's horizontal or vertical */
         if (delta.y > delta.x) pospt->x = bpt->x;
         else pospt->y = bpt->y;
      }
   }

   /* enforce constraints on point forward of cycle position */

   if (fpt != NULL) {
      if (ffpt != NULL) {
         if (fpt->x == ffpt->x) fpt->y = pospt->y;
         if (fpt->y == ffpt->y) fpt->x = pospt->x;
      }
      else if (strict) {
         XPoint delta = abs(*fpt - *pospt);

         /* Only one segment---just make sure it's horizontal or vertical */
         if (delta.y > delta.x) pospt->x = fpt->x;
         else pospt->y = fpt->y;
      }
   }
}

/*----------------------------------------------------------------------*/
/* Bounding box calculation routines					*/
/*----------------------------------------------------------------------*/

void bboxcalc(short testval, short *lowerval, short *upperval)
{
   if (testval < *lowerval) *lowerval = testval;
   if (testval > *upperval) *upperval = testval;
}

/*----------------------------------------------------------------------*/
/* Bounding box calculation for elements which can be part of a path	*/
/*----------------------------------------------------------------------*/

void calcextents(genericptr *bboxgen, short *llx, short *lly, 
	short *urx, short *ury)
{
   switch (ELEMENTTYPE(*bboxgen)) {
      case(POLYGON): {
         foreach (XPoint p, TOPOLY(bboxgen)->points) {
            bboxcalc(p.x, llx, urx);
            bboxcalc(p.y, lly, ury);
         }
         } break;

      case(SPLINE): {
         fpointlist bboxpts;
         bboxcalc(TOSPLINE(bboxgen)->ctrl[0].x, llx, urx);
         bboxcalc(TOSPLINE(bboxgen)->ctrl[0].y, lly, ury);
         bboxcalc(TOSPLINE(bboxgen)->ctrl[3].x, llx, urx);
         bboxcalc(TOSPLINE(bboxgen)->ctrl[3].y, lly, ury);
         for (bboxpts = TOSPLINE(bboxgen)->points; bboxpts < 
		 TOSPLINE(bboxgen)->points + INTSEGS; bboxpts++) {
	    bboxcalc((short)(bboxpts->x), llx, urx);
	    bboxcalc((short)(bboxpts->y), lly, ury);
         }
         } break;

      case (ARC): {
         fpointlist bboxpts;
         for (bboxpts = TOARC(bboxgen)->points; bboxpts < TOARC(bboxgen)->points +
	         TOARC(bboxgen)->number; bboxpts++) {
            bboxcalc((short)(bboxpts->x), llx, urx);
	    bboxcalc((short)(bboxpts->y), lly, ury);
         }
         } break;
   }
}

/*--------------------------------------------------------------*/
/* Wrapper for single call to calcbboxsingle() in the netlister */
/*--------------------------------------------------------------*/

void calcinstbbox(genericptr *bboxgen, short *llx, short *lly, short *urx,
                short *ury)
{
   *llx = *lly = 32767;
   *urx = *ury = -32768;

   calcbboxsingle(bboxgen, areawin->topinstance, llx, lly, urx, ury);
}

/*----------------------------------------------------------------------*/
/* Bounding box calculation for a single generic element		*/
/*----------------------------------------------------------------------*/

void calcbboxsingle(genericptr *bboxgen, objinstptr thisinst, 
		short *llx, short *lly, short *urx, short *ury)
{
   XPoint npoints[4];
   short j;

   /* For each screen element, compute the extents and revise bounding	*/
   /* box points, if necessary. 					*/

   switch(ELEMENTTYPE(*bboxgen)) {

      case(OBJINST):
         TOOBJINST(bboxgen)->getBbox(npoints, (int)0);

         for (j = 0; j < 4; j++) {
            bboxcalc(npoints[j].x, llx, urx);
            bboxcalc(npoints[j].y, lly, ury);
         }
	 break;

      case(LABEL):
	 /* because a pin is offset from its position point, include */
	 /* that point in the bounding box.				*/

	 if (TOLABEL(bboxgen)->pin) {
            bboxcalc(TOLABEL(bboxgen)->position.x, llx, urx);
            bboxcalc(TOLABEL(bboxgen)->position.y, lly, ury);
	 }
         TOLABEL(bboxgen)->getBbox(npoints, thisinst);

         for (j = 0; j < 4; j++) {
            bboxcalc(npoints[j].x, llx, urx);
            bboxcalc(npoints[j].y, lly, ury);
         }
         break;

      case(GRAPHIC):
         TOGRAPHIC(bboxgen)->getBbox(npoints);
         for (j = 0; j < 4; j++) {
            bboxcalc(npoints[j].x, llx, urx);
            bboxcalc(npoints[j].y, lly, ury);
         }
	 break;

      case(PATH): {
	 genericptr *pathc;
         for (pathc = TOPATH(bboxgen)->begin(); pathc < TOPATH(bboxgen)->end(); pathc++)
	       calcextents(pathc, llx, lly, urx, ury);
	 } break;

      default:
	 calcextents(bboxgen, llx, lly, urx, ury);
   }
}

/*------------------------------------------------------*/
/* Find if an object is in the specified library	*/
/*------------------------------------------------------*/

bool object_in_library(short libnum, objectptr thisobject)
{
   short i;

   for (i = 0; i < xobjs.userlibs[libnum].number; i++) {
      if (*(xobjs.userlibs[libnum].library + i) == thisobject)
	 return true;
   }
   return false;
}

/*------------------------------------------------------*/
/* Find all pages and libraries containing this object	*/
/* and update accordingly.  If this object is a page,	*/
/* just update the page directory.			*/
/*------------------------------------------------------*/

void updatepagebounds(objectptr thisobject)
{
   short i, j;
   objectptr pageobj;

   if ((i = is_page(thisobject)) >= 0) {
      if (! xobjs.pagelist[i].background.name.isEmpty())
         backgroundbbox(i);
      updatepagelib(PAGELIB, i);
   }
   else {
      for (i = 0; i < xobjs.pages; i++) {
         if (xobjs.pagelist[i].pageinst != NULL) {
            pageobj = xobjs.pagelist[i].pageinst->thisobject;
            if ((j = pageobj->find(thisobject)) >= 0) {
               calcbboxvalues(xobjs.pagelist[i].pageinst, pageobj->begin() + j);
	       updatepagelib(PAGELIB, i);
	    }
         }
      }
      for (i = 0; i < xobjs.numlibs; i++)
         if (object_in_library(i, thisobject))
	    composelib(i + LIBRARY);
   }
}

/*--------------------------------------------------------------*/
/* Calculate the bounding box for an object instance.  Use the	*/
/* existing bbox and finish calculation on all the elements	*/
/* which have parameters not taking default values.		*/
/* This finishes the calculation partially done by		*/
/* calcbboxvalues().						*/
/*--------------------------------------------------------------*/

void calcbboxinst(objinstptr thisinst)
{
   objectptr thisobj;
   genericptr *gelem;
   short llx, lly, urx, ury;

   short pllx, plly, purx, pury;
   bool hasschembbox = false;
   bool didparamsubs = false;

   if (thisinst == NULL) return;

   thisobj = thisinst->thisobject;

   llx = thisobj->bbox.lowerleft.x;
   lly = thisobj->bbox.lowerleft.y;
   urx = llx + thisobj->bbox.width;
   ury = lly + thisobj->bbox.height;

   pllx = plly = 32767;
   purx = pury = -32768;

   for (gelem = thisobj->begin(); gelem != thisobj->end(); gelem++) {
      /* pins which do not appear outside of the object	*/
      /* contribute to the objects "schembbox".		*/

      if (IS_LABEL(*gelem)) {
	 labelptr btext = TOLABEL(gelem);
	 if (btext->pin && !(btext->justify & PINVISIBLE)) {
	    hasschembbox = true;
	    calcbboxsingle(gelem, thisinst, &pllx, &plly, &purx, &pury);
	    continue;
	 }
      }

      if (has_param(*gelem)) {
	 if (didparamsubs == false) {
	    psubstitute(thisinst);
	    didparamsubs = true;
	 }
	 calcbboxsingle(gelem, thisinst, &llx, &lly, &urx, &ury);
      }
   }

   thisinst->bbox.lowerleft.x = llx;
   thisinst->bbox.lowerleft.y = lly;
   thisinst->bbox.width = urx - llx;
   thisinst->bbox.height = ury - lly;

   if (hasschembbox) {
      if (thisinst->schembbox == NULL)
         thisinst->schembbox = new BBox;

      thisinst->schembbox->lowerleft.x = pllx;
      thisinst->schembbox->lowerleft.y = plly;
      thisinst->schembbox->width = purx - pllx;
      thisinst->schembbox->height = pury - plly;
   }
   else {
      delete thisinst->schembbox;
      thisinst->schembbox = NULL;
   }
}

/*--------------------------------------------------------------*/
/* Update things based on a changed instance bounding box.	*/
/* If the parameter was a single-instance			*/
/* substitution, only the page should be updated.  If the	*/
/* parameter was a default value, the library should be updated */
/* and any pages containing the object where the parameter	*/
/* takes the default value.					*/
/*--------------------------------------------------------------*/

void updateinstparam(objectptr bobj)
{
   short i, j;
   objectptr pageobj;

   /* change bounds on pagelib and all pages			*/
   /* containing this *object* if and only if the object	*/
   /* instance takes the default value.  Also update the	*/
   /* library page.						*/

   for (i = 0; i < xobjs.pages; i++)
      if (xobjs.pagelist[i].pageinst != NULL) {
         pageobj = xobjs.pagelist[i].pageinst->thisobject;
         if ((j = pageobj->find(topobject)) >= 0) {

	    /* Really, we'd like to recalculate the bounding box only if the */
	    /* parameter value is the default value which was just changed.	*/
	    /* However, then any non-default values may contain the wrong	*/
	    /* substitutions.						*/

            objinstptr cinst = TOOBJINST(pageobj->begin() + j);
	    if (cinst->thisobject->params == NULL) {
               calcbboxvalues(xobjs.pagelist[i].pageinst, pageobj->begin() + j);
	       updatepagelib(PAGELIB, i);
	    }
	 }
      }

   for (i = 0; i < xobjs.numlibs; i++)
      if (object_in_library(i, bobj))
	 composelib(i + LIBRARY);
}

/*--------------------------------------------------------------*/
/* Calculate bbox on all elements of the given object		*/
/*--------------------------------------------------------------*/

void calcbbox(objinstptr binst)
{
   calcbboxvalues(binst, (genericptr *)NULL);
   if (binst == areawin->topinstance) {
      updatepagebounds(topobject);
   }
}

/*--------------------------------------------------------------*/
/* Calculate bbox on the given element of the specified object.	*/
/* This is a wrapper for calcbboxvalues() assuming that we're	*/
/* on the top-level, and that page bounds need to be updated.	*/
/*--------------------------------------------------------------*/

void singlebbox(genericptr *gelem)
{
   calcbboxvalues(areawin->topinstance, (genericptr *)gelem);
   updatepagebounds(topobject);
}

/*----------------------------------------------------------------------*/
/* Extend bounding box based on selected elements only			*/
/*----------------------------------------------------------------------*/

void calcbboxselect()
{
   short *bsel;
   for (bsel = areawin->selectlist; bsel < areawin->selectlist +
		areawin->selects; bsel++)
      calcbboxvalues(areawin->topinstance, topobject->begin() + *bsel);
  
   updatepagebounds(topobject);
}

/*--------------------------------------------------------------*/
/* Update Bounding box for an object.				*/
/* If newelement == NULL, calculate bounding box from scratch.	*/
/* Otherwise, expand bounding box to enclose newelement.	*/
/*--------------------------------------------------------------*/

void calcbboxvalues(objinstptr thisinst, genericptr *newelement)
{
   genericptr *bboxgen;
   short llx, lly, urx, ury;
   objectptr thisobj = thisinst->thisobject;

   /* no action if there are no elements */
   if (thisobj->parts == 0) return;

   /* If this object has parameters, then we will do a separate		*/
   /* bounding box calculation on parameterized parts.  This		*/
   /* calculation ignores them, and the result is a base that the	*/
   /* instance bounding-box computation can use as a starting point.	*/

   /* set starting bounds as maximum bounds of screen */
   llx = lly = 32767;
   urx = ury = -32768;

   for (bboxgen = thisobj->begin(); bboxgen != thisobj->end(); bboxgen++) {

      /* override the "for" loop if we're doing a single element */
      if (newelement != NULL) bboxgen = newelement;

      if ((thisobj->params == NULL) || (!has_param(*bboxgen))) {
	 /* pins which do not appear outside of the object 	*/
	 /* are ignored now---will be computed per instance.	*/

	 if (IS_LABEL(*bboxgen)) {
	    labelptr btext = TOLABEL(bboxgen);
	    if (btext->pin && !(btext->justify & PINVISIBLE)) {
	       goto nextgen;
	    }
	 }
	 calcbboxsingle(bboxgen, thisinst, &llx, &lly, &urx, &ury);
      }
nextgen:
      if (newelement != NULL) break;
   }

   /* if this is a single-element calculation and its bounding box	*/
   /* turned out to be smaller than the object's, then we need to	*/
   /* recompute the entire object's bounding box in case it got		*/
   /* smaller.  This is not recursive, in spite of looks.		*/

   if (newelement != NULL) {
      if (llx > thisobj->bbox.lowerleft.x &&
		lly > thisobj->bbox.lowerleft.y &&
		urx < (thisobj->bbox.lowerleft.x + thisobj->bbox.width) &&
		ury < (thisobj->bbox.lowerleft.y + thisobj->bbox.height)) {
	 calcbboxvalues(thisinst, NULL);
	 return;
      }
      else {
	 bboxcalc(thisobj->bbox.lowerleft.x, &llx, &urx);
	 bboxcalc(thisobj->bbox.lowerleft.y, &lly, &ury);
	 bboxcalc(thisobj->bbox.lowerleft.x + thisobj->bbox.width, &llx, &urx);
	 bboxcalc(thisobj->bbox.lowerleft.y + thisobj->bbox.height, &lly, &ury);
      }
   }

   /* Set the new bounding box.  In pathological cases, such as a page	*/
   /* with only pin labels, the bounds may not have been changed from	*/
   /* their initial values.  If so, then don't touch the bounding box.	*/

   if ((llx <= urx) && (lly <= ury)) {
      thisobj->bbox.lowerleft.x = llx;
      thisobj->bbox.lowerleft.y = lly;
      thisobj->bbox.width = urx - llx;
      thisobj->bbox.height = ury - lly;
   }

   /* calculate instance-specific values */
   calcbboxinst(thisinst);
}

/*------------------------------------------------------*/
/* Center an object in the viewing window		*/
/*------------------------------------------------------*/

void centerview(objinstptr tinst)
{
   XPoint origin, corner;
   Dimension width, height;
   float fitwidth, fitheight;
   objectptr tobj = tinst->thisobject;

   origin = tinst->bbox.lowerleft;
   corner.x = origin.x + tinst->bbox.width;
   corner.y = origin.y + tinst->bbox.height;

   extendschembbox(tinst, &origin, &corner);

   width = corner.x - origin.x;
   height = corner.y - origin.y;

   fitwidth = (float)areawin->width / ((float)width + 2 * DEFAULTGRIDSPACE);
   fitheight = (float)areawin->height / ((float)height + 2 * DEFAULTGRIDSPACE);

   tobj->viewscale = (fitwidth < fitheight) ?
                 qMin(MINAUTOSCALE, fitwidth) : qMin(MINAUTOSCALE, fitheight);

   tobj->pcorner.x = origin.x - (areawin->width
	       / tobj->viewscale - width) / 2;
   tobj->pcorner.y = origin.y - (areawin->height
	       / tobj->viewscale - height) / 2;

   /* Copy new position values to the current window */

   if (tobj == topobject) {
      areawin->pcorner = tobj->pcorner;
      areawin->vscale = tobj->viewscale;
   }
}

/*-----------------------------------------------------------*/
/* Refresh the window and scrollbars and write the page name */
/*-----------------------------------------------------------*/

void refresh(QAction*, void*, void*)
{
    areawin->area->refresh();
}

/*------------------------------------------------------*/
/* Center the current page in the viewing window	*/
/*------------------------------------------------------*/

void zoomview(QAction*, void*, void*)
{
   if (eventmode == NORMAL_MODE || eventmode == COPY_MODE ||
	 eventmode == MOVE_MODE || eventmode == CATALOG_MODE ||
	eventmode == FONTCAT_MODE || eventmode == EFONTCAT_MODE ||
	eventmode == CATMOVE_MODE) {

      centerview(areawin->topinstance);
      areawin->lastbackground = NULL;
      renderbackground();
      refresh(NULL, NULL, NULL);
   }
}

/*---------------------------------------------------------*/
/* Basic X Graphics Routines in the User coordinate system */
/*---------------------------------------------------------*/

void UDrawSimpleLine(Context* ctx, const XPoint *pt1, const XPoint *pt2)
{
   XPoint newpt1, newpt2;

   ctx->CTM().transform(pt1, &newpt1, 1);
   ctx->CTM().transform(pt2, &newpt2, 1);

   ctx->gc()->drawLine(newpt1, newpt2);
} 

/*-------------------------------------------------------------------------*/

void UDrawLine(Context* ctx, const XPoint *pt1, const XPoint *pt2)
{
   float tmpwidth = ctx->UTopTransScale(xobjs.pagelist[areawin->page].wirewidth);

   SetLineAttributes(ctx->gc(), tmpwidth, LineSolid, CapRound, JoinBevel);
   UDrawSimpleLine(ctx, pt1, pt2);
} 

/*----------------------------------------------------------------------*/
/* Add circle at given point to indicate that the point is a parameter.	*/
/* The circle is divided into quarters.  For parameterized y-coordinate	*/
/* the top and bottom quarters are drawn.  For parameterized x-		*/
/* coordinate, the left and right quarters are drawn.  A full circle	*/
/* indicates either both x- and y-coordinates are parameterized, or	*/
/* else any other kind of parameterization (presently, not used).	*/
/*									*/
/* (note that the two angles in DrawArc() are 1) the start angle,	*/
/* measured in absolute 64th degrees from 0 (3 o'clock), and 2) the	*/
/* path length, in relative 64th degrees (positive = counterclockwise,	*/
/* negative = clockwise)).						*/
/*----------------------------------------------------------------------*/

void UDrawCircle(Context* ctx, const XPoint * upt, u_char which)
{
   XPoint wpt;

   user_to_window(*upt, &wpt);
   SetThinLineAttributes(ctx->gc(), 0, LineSolid, CapButt, JoinMiter);

   switch(which) {
      case P_POSITION_X:
         DrawArc(ctx->gc(), wpt.x - 4,
		wpt.y - 4, 8, 8, -(45 * 64), (90 * 64));
         DrawArc(ctx->gc(), wpt.x - 4,
		wpt.y - 4, 8, 8, (135 * 64), (90 * 64));
	 break;
      case P_POSITION_Y:
         DrawArc(ctx->gc(), wpt.x - 4,
		wpt.y - 4, 8, 8, (45 * 64), (90 * 64));
         DrawArc(ctx->gc(), wpt.x - 4,
		wpt.y - 4, 8, 8, (225 * 64), (90 * 64));
	 break;
      default:
         DrawArc(ctx->gc(), wpt.x - 4,
		wpt.y - 4, 8, 8, 0, (360 * 64));
	 break;
   }
}

/*----------------------------------------------------------------------*/
/* Add "X" at string origin						*/
/*----------------------------------------------------------------------*/

static void UDrawXAt(Context* ctx, XPoint *wpt)
{
   SetThinLineAttributes(ctx->gc(), 0, LineSolid, CapButt, JoinMiter);
   ctx->gc()->drawLine(wpt->x - 3,
		wpt->y - 3, wpt->x + 3, wpt->y + 3);
   ctx->gc()->drawLine(wpt->x + 3,
		wpt->y - 3, wpt->x - 3, wpt->y + 3);
}

/*----------------------------------------------------------------------*/
/* Draw "X" on current level						*/
/*----------------------------------------------------------------------*/

void UDrawX(Context* ctx, labelptr curlabel)
{
   XPoint wpt;

   user_to_window(curlabel->position, &wpt);
   UDrawXAt(ctx, &wpt);
}

/*----------------------------------------------------------------------*/
/* Draw "X" on top level (only for LOCAL and GLOBAL pin labels)		*/
/*----------------------------------------------------------------------*/

void UDrawXDown(Context* ctx, labelptr curlabel)
{
   XPoint wpt;

   ctx->CTM().transform(&curlabel->position, &wpt, 1);
   UDrawXAt(ctx, &wpt);
}

/*----------------------------------------------------------------------*/
/* Find the "real" width, height, and origin of an object including pin	*/
/* labels and so forth that only show up on a schematic when it is the	*/
/* top-level object.							*/
/*----------------------------------------------------------------------*/

int toplevelwidth(objinstptr bbinst, short *rllx)
{
   short llx, urx;
   short origin, corner;

   if (bbinst->schembbox == NULL) {
      if (rllx) *rllx = bbinst->bbox.lowerleft.x;
      return bbinst->bbox.width;
   }

   origin = bbinst->bbox.lowerleft.x;
   corner = origin + bbinst->bbox.width;

   llx = bbinst->schembbox->lowerleft.x;
   urx = llx + bbinst->schembbox->width;

   bboxcalc(llx, &origin, &corner);
   bboxcalc(urx, &origin, &corner);

   if (rllx) *rllx = origin;
   return(corner - origin);
}

/*----------------------------------------------------------------------*/

int toplevelheight(objinstptr bbinst, short *rlly)
{
   short lly, ury;
   short origin, corner;

   if (bbinst->schembbox == NULL) {
      if (rlly) *rlly = bbinst->bbox.lowerleft.y;
      return bbinst->bbox.height;
   }

   origin = bbinst->bbox.lowerleft.y;
   corner = origin + bbinst->bbox.height;

   lly = bbinst->schembbox->lowerleft.y;
   ury = lly + bbinst->schembbox->height;

   bboxcalc(lly, &origin, &corner);
   bboxcalc(ury, &origin, &corner);

   if (rlly) *rlly = origin;
   return(corner - origin);
}

/*----------------------------------------------------------------------*/
/* Add dimensions of schematic pins to an object's bounding box		*/
/*----------------------------------------------------------------------*/

void extendschembbox(objinstptr bbinst, XPoint *origin, XPoint *corner)
{
   short llx, lly, urx, ury;

   if ((bbinst == NULL) || (bbinst->schembbox == NULL)) return;

   llx = bbinst->schembbox->lowerleft.x;
   lly = bbinst->schembbox->lowerleft.y;
   urx = llx + bbinst->schembbox->width;
   ury = lly + bbinst->schembbox->height;

   bboxcalc(llx, &(origin->x), &(corner->x));
   bboxcalc(lly, &(origin->y), &(corner->y));
   bboxcalc(urx, &(origin->x), &(corner->x));
   bboxcalc(ury, &(origin->y), &(corner->y));
}

/*----------------------------------------------------------------------*/
/* Adjust a pinlabel position to account for pad spacing		*/
/*----------------------------------------------------------------------*/

void pinadjust (short justify, short *xpoint, short *ypoint, short dir)
{
   int delx, dely;

   dely = (justify & NOTBOTTOM) ?
            ((justify & TOP) ? -PADSPACE : 0) : PADSPACE;
   delx = (justify & NOTLEFT) ?
            ((justify & RIGHT) ? -PADSPACE : 0) : PADSPACE;

   if (xpoint != NULL) *xpoint += (dir > 0) ? delx : -delx;
   if (ypoint != NULL) *ypoint += (dir > 0) ? dely : -dely;
}

/*----------------------------------------------------------------------*/
/* Draw line for editing text (position of cursor in string is given by */
/*   tpos (2nd parameter)						*/
/*----------------------------------------------------------------------*/

void UDrawTextLine(Context* ctx, labelptr curlabel, short tpos)
{
   XPoint  points[2]; /* top and bottom of text cursor line */
   short   xdist, xbase, tmpjust;
   TextExtents tmpext;

   /* correct for position, rotation, scale, and flip invariance of text */

   ctx->UPushCTM();
   ctx->CTM().preMult(curlabel->position, curlabel->scale, curlabel->rotation);
   tmpjust = ctx->flipadjust(curlabel->justify);

   SetFunction(ctx->gc(), GXxor);
   SetForeground(ctx->gc(), AUXCOLOR ^ BACKGROUND);

   tmpext = ULength(curlabel, areawin->topinstance, tpos, NULL);
   xdist = tmpext.width;
   xbase = tmpext.base;
   tmpext = ULength(curlabel, areawin->topinstance, 0, NULL);

   points[0].x = (tmpjust & NOTLEFT ?
        (tmpjust & RIGHT ? -tmpext.width : -tmpext.width >> 1) : 0)
	+ xdist;
   points[0].y = (tmpjust & NOTBOTTOM ?
        (tmpjust & TOP ? -tmpext.ascent : -(tmpext.ascent + tmpext.base) / 2)
	: -tmpext.base) + xbase - 3;
   points[1].x = points[0].x;
   points[1].y = points[0].y + TEXTHEIGHT + 6;

   if (curlabel->pin) {
      pinadjust(tmpjust, &(points[0].x), &(points[0].y), 1);
      pinadjust(tmpjust, &(points[1].x), &(points[1].y), 1);
   }

   /* draw the line */

   UDrawLine(ctx, &points[0], &points[1]);
   ctx->UPopCTM();

   UDrawX(ctx, curlabel);
}

/*-----------------------------------------------------------------*/
/* Draw lines for editing text when multiple characters are chosen */
/*-----------------------------------------------------------------*/

void UDrawTLine(Context* ctx, labelptr curlabel)
{
   UDrawTextLine(ctx, curlabel, areawin->textpos);
   if ((areawin->textend > 0) && (areawin->textend < areawin->textpos)) {
      UDrawTextLine(ctx, curlabel, areawin->textend);
   }
}

/*----------------------*/
/* Draw an X		*/
/*----------------------*/

void UDrawXLine(Context* ctx, XPoint opt, XPoint cpt)
{
   XPoint upt, vpt;

   SetForeground(ctx->gc(), AUXCOLOR);
   SetFunction(ctx->gc(), GXxor);

   user_to_window(cpt, &upt);
   user_to_window(opt, &vpt);

   SetThinLineAttributes(ctx->gc(), 0, LineOnOffDash, CapButt, JoinMiter);
   ctx->gc()->drawLine(vpt, upt);

   SetThinLineAttributes(ctx->gc(), 0, LineSolid, CapButt, JoinMiter);
   ctx->gc()->drawLine(upt.x - 3, upt.y - 3,
		upt.x + 3, upt.y + 3);
   ctx->gc()->drawLine(upt.x + 3, upt.y - 3,
		upt.x - 3, upt.y + 3);

   SetFunction(ctx->gc(), ctx->gctype);
   SetForeground(ctx->gc(), ctx->gccolor);
}

/*-------------------------------------------------------------------------*/

void UDrawBox(Context* ctx, XPoint origin, XPoint corner)
{
   XPoint	worig, wcorn;

   user_to_window(origin, &worig);
   user_to_window(corner, &wcorn);

   SetFunction(ctx->gc(), GXxor);
   SetForeground(ctx->gc(), AUXCOLOR ^ BACKGROUND);
   SetThinLineAttributes(ctx->gc(), 0, LineSolid, CapRound, JoinBevel);

   ctx->gc()->drawLine(worig.x, worig.y,
		worig.x, wcorn.y);
   ctx->gc()->drawLine(worig.x, wcorn.y,
		wcorn.x, wcorn.y);
   ctx->gc()->drawLine(wcorn.x, wcorn.y,
		wcorn.x, worig.y);
   ctx->gc()->drawLine(wcorn.x, worig.y,
		worig.x, worig.y);
}

/*----------------------------------------------------------------------*/
/* Draw a box indicating the dimensions of the edit element that most	*/
/* closely reach the position "corner".					*/
/*----------------------------------------------------------------------*/

float UDrawRescaleBox(Context* ctx, const XPoint & corner)
{
   XPoint 	origpoints[5], newpoints[5];
   genericptr	rgen;
   float	newscale;

   if (areawin->selects == 0) return 0.0;

   SetFunction(ctx->gc(), GXxor);
   SetForeground(ctx->gc(), AUXCOLOR ^ BACKGROUND);
   SetThinLineAttributes(ctx->gc(), 0, LineSolid, CapRound, JoinBevel);
   
   /* Use only the 1st selection as a reference to set the scale */

   rgen = SELTOGENERIC(areawin->selectlist);
   newscale = rgen->rescaleBox(corner, newpoints);
   ctx->CTM().transform(newpoints, origpoints, 4);
   strokepath(ctx, origpoints, 4, 0, 1);
   return newscale;
}

/*-------------------------------------------------------------------------*/
void UDrawBBox(Context* ctx)
{
   XPoint	origin;
   XPoint	worig, wcorn, corner;
   objinstptr	bbinst = areawin->topinstance;

   if ((!areawin->bboxon) || (checkforbbox(topobject) != NULL)) return;

   origin = bbinst->bbox.lowerleft;
   corner.x = origin.x + bbinst->bbox.width;
   corner.y = origin.y + bbinst->bbox.height;

   /* Include any schematic labels in the bounding box.	*/
   extendschembbox(bbinst, &origin, &corner);

   user_to_window(origin, &worig);
   user_to_window(corner, &wcorn);

   SetForeground(ctx->gc(), BBOXCOLOR);
   ctx->gc()->drawLine(worig.x, worig.y,
		worig.x, wcorn.y);
   ctx->gc()->drawLine(worig.x, wcorn.y,
		wcorn.x, wcorn.y);
   ctx->gc()->drawLine(wcorn.x, wcorn.y,
		wcorn.x, worig.y);
   ctx->gc()->drawLine(wcorn.x, worig.y,
		worig.x, worig.y);
}

/*-------------------------------------------------------------------------*/
/* Fill and/or draw a border around the stroking path			   */
/*-------------------------------------------------------------------------*/

void strokepath(Context* ctx, XPoint *pathlist, short number, short style, float width)
{
   float        tmpwidth;
   short	minwidth;

   tmpwidth = ctx->UTopTransScale(xobjs.pagelist[areawin->page].wirewidth * width);
   minwidth = qMax(1.0F, tmpwidth);

   QPainter * const p = ctx->gc();

   if (style & FILLED || (!(style & FILLED) && style & OPAQUE)) {
      if ((style & FILLSOLID) == FILLSOLID) {
         p->setBrush(p->pen().color());
      } else if (!(style & FILLED)) {
         SetStipple(p, 7, true);
      }
      else {
         SetStipple(p, ((style & FILLSOLID) >> 5), style & OPAQUE);
      }
      FillPolygon(p, pathlist, number);
      /* return to original state */
      p->setBrush(p->pen().color());
   }
   if (!(style & NOBORDER)) {
      /* set up dots or dashes */
      QVector<qreal> dashes;
      if (style & DASHED) dashes << 4 * minwidth;
      else if (style & DOTTED) dashes << minwidth;
      dashes << 4*minwidth;
      if (style & (DASHED | DOTTED)) {
         QPen pen(p->pen());
         pen.setDashPattern(dashes);
         SetLineAttributes(p, tmpwidth, LineOnOffDash,
		CapButt, (style & SQUARECAP) ? JoinMiter : JoinBevel);
      }
      else
         SetLineAttributes(p, tmpwidth, LineSolid,
		(style & SQUARECAP) ? CapProjecting : CapRound,
		(style & SQUARECAP) ? JoinMiter : JoinBevel);

      /* draw the spline and close off if so specified */
      DrawLines(p, pathlist, number);
      if (!(style & UNCLOSED))
         p->drawLine(pathlist[0].x,
		pathlist[0].y, pathlist[number - 1].x, pathlist[number - 1].y);
   }
}

/*-------------------------------------------------------------------------*/

void makesplinepath(Context* ctx, const spline * thespline, XPoint *pathlist)
{
   XPoint *tmpptr = pathlist;

   ctx->CTM().transform(&(thespline->ctrl[0]), tmpptr, 1);
   ctx->CTM().transform(thespline->points, ++tmpptr, INTSEGS);
   ctx->CTM().transform(&(thespline->ctrl[3]), tmpptr + INTSEGS, 1);
}

/*----------------------------------------------------------------------*/
/* Recursively run through the current page and find any labels which	*/
/* are declared to be style LATEX.  If "checkonly" is present, we set	*/
/* it to true or false depending on whether or not LATEX labels have	*/
/* been encountered.  If NULL, then we write LATEX output appropriately	*/
/* to a file named with the page filename + suffix ".tex".		*/
/*----------------------------------------------------------------------*/

void UDoLatex(objinstptr theinstance, short level, FILE *f,
	float scale, float scale2, int tx, int ty, bool *checkonly)
{
   XPoint	lpos, xlpos;
   XfPoint	xfpos;
   labelptr	thislabel;
   genericptr	*areagen;
   objectptr	theobject = theinstance->thisobject;
   char		*ltext;
   int		lrjust, tbjust;

   Matrix DCTM;
   if (level != 0)
       DCTM.preMult(theinstance->position, theinstance->scale,
			theinstance->rotation);

   /* make parameter substitutions */
   psubstitute(theinstance);

   /* find all of the elements */
   
   for (areagen = theobject->begin(); areagen != theobject->end(); areagen++) {

      switch(ELEMENTTYPE(*areagen)) {
         case(OBJINST):
            UDoLatex(TOOBJINST(areagen), level + 1, f, scale, scale2, tx, ty, checkonly);
	    break;
   
	 case(LABEL): 
	    thislabel = TOLABEL(areagen);
            if (level == 0 || thislabel->pin == 0 ||
			(thislabel->justify & PINVISIBLE))
		if (thislabel->justify & LATEXLABEL) {
		    if (checkonly) {
		       *checkonly = true;
		       return;
		    }
		    else {
                       lpos = thislabel->position;
                       DCTM.transform(&lpos, &xlpos, 1);
		       xlpos.x += tx;
		       xlpos.y += ty;
		       xfpos.x = (float)xlpos.x * scale;
		       xfpos.y = (float)xlpos.y * scale;
		       xfpos.x /= 72.0;
		       xfpos.y /= 72.0;
		       xfpos.x -= 1.0;
		       xfpos.y -= 1.0;
		       xfpos.x += 0.056;
		       xfpos.y += 0.056;
		       xfpos.x /= scale2;
		       xfpos.y /= scale2;
		       ltext = textprint(thislabel->string, theinstance);
		       tbjust = thislabel->justify & (NOTBOTTOM | TOP);
		       lrjust = thislabel->justify & (NOTLEFT | RIGHT);

		       /* The 1.2 factor accounts for the difference between	 */
		       /* Xcircuit's label scale of "1" and LaTeX's "normalsize" */

		       fprintf(f, "   \\putbox{%3.2fin}{%3.2fin}{%3.2f}{",
				xfpos.x, xfpos.y, 1.2 * thislabel->scale);
		       if (thislabel->rotation != 0)
			  fprintf(f, "\\rotatebox{-%d}{", thislabel->rotation);
		       if (lrjust == (NOTLEFT | RIGHT)) fprintf(f, "\\rightbox{");
		       else if (lrjust == NOTLEFT) fprintf(f, "\\centbox{");
		       if (tbjust == (NOTBOTTOM | TOP)) fprintf(f, "\\topbox{");
		       else if (tbjust == NOTBOTTOM) fprintf(f, "\\midbox{");
		       fprintf(f, "%s", ltext);
		       if (lrjust != NORMAL) fprintf(f, "}");
		       if (tbjust != NORMAL) fprintf(f, "}");
		       if (thislabel->rotation != 0) fprintf(f, "}");
		       fprintf(f, "}%%\n");
		       free(ltext);
		    }
		}
	    break;
       }
   }
}

/*----------------------------------------------------------------------*/
/*  Top level routine for writing LATEX output.				*/
/*----------------------------------------------------------------------*/

void TopDoLatex()
{
   FILE *f;
   float psscale, outscale;
   int tx, ty, width, height;
   polyptr framebox;
   XPoint origin;
   bool checklatex = false;
   QString filename, extend;
   int dotidx;

   UDoLatex(areawin->topinstance, 0, NULL, 1.0, 1.0, 0, 0, &checklatex);

   if (checklatex == false) return;	/* No LaTeX labels to write */

   /* Handle cases where the file might have a ".eps" extension.	*/
   /* Thanks to Graham Sheward for pointing this out.			*/

   filename = xobjs.pagelist[areawin->page].filename;
   if (filename.isEmpty()) filename = QString::fromLocal8Bit(xobjs.pagelist[areawin->page].pageinst->thisobject->name);

   if ((dotidx = filename.indexOf(QChar('.'), -3)) == -1) {
       dotidx = filename.length();
       filename.append(".ps");
   }
   extend = filename.mid(dotidx);
   filename.truncate(dotidx);
   filename.append(".tex");

   f = fopen(filename.toLocal8Bit(), "w");

   fprintf(f, "%% XCircuit output \"%s.tex\" for LaTeX input from %s%s\n",
                filename.toLocal8Bit().data(), filename.toLocal8Bit().data(), extend.toLocal8Bit().data());
   fprintf(f, "\\def\\putbox#1#2#3#4{\\makebox[0in][l]{\\makebox[#1][l]{}"
		"\\raisebox{\\baselineskip}[0in][0in]"
		"{\\raisebox{#2}[0in][0in]{\\scalebox{#3}{#4}}}}}\n");
   fprintf(f, "\\def\\rightbox#1{\\makebox[0in][r]{#1}}\n");
   fprintf(f, "\\def\\centbox#1{\\makebox[0in]{#1}}\n");
   fprintf(f, "\\def\\topbox#1{\\raisebox{-0.60\\baselineskip}[0in][0in]{#1}}\n");
   fprintf(f, "\\def\\midbox#1{\\raisebox{-0.20\\baselineskip}[0in][0in]{#1}}\n");

   /* Modified to use \scalebox and \parbox by Alex Tercete, June 2008 */

   fprintf(f, "\\begin{center}\n");

   outscale = xobjs.pagelist[areawin->page].outscale;
   psscale = getpsscale(outscale, areawin->page);

   width = toplevelwidth(areawin->topinstance, &origin.x);
   height = toplevelheight(areawin->topinstance, &origin.y);

   /* Added 10/19/10:  If there is a specified bounding box, let it	*/
   /* determine the figure origin;  otherwise, the labels will be	*/
   /* mismatched to the bounding box.					*/

   if ((framebox = checkforbbox(topobject)) != NULL) {
      int i, maxx, maxy;

      origin.x = maxx = framebox->points[0].x;
      origin.y = maxy = framebox->points[0].y;
      for (i = 1; i < framebox->points.count(); i++) {
         if (framebox->points[i].x < origin.x) origin.x = framebox->points[i].x;
         if (framebox->points[i].x > maxx) maxx = framebox->points[i].x;
         if (framebox->points[i].y < origin.y) origin.y = framebox->points[i].y;
         if (framebox->points[i].y > maxy) maxy = framebox->points[i].y;
      }
      origin.x -= ((width - maxx + origin.x) / 2);
      origin.y -= ((height - maxy + origin.y) / 2);
   }

   tx = (int)(72 / psscale) - origin.x,
   ty = (int)(72 / psscale) - origin.y;

   fprintf(f, "   \\scalebox{%g}{\n", outscale);
   fprintf(f, "   \\normalsize\n");
   fprintf(f, "   \\parbox{%gin}{\n", (((float)width * psscale) / 72.0) / outscale);
   fprintf(f, "   \\includegraphics[scale=%g]{%s%s}\\\\\n", 1.0 / outscale,
                        filename.toLocal8Bit().data(), extend.toLocal8Bit().data());
   fprintf(f, "   %% translate x=%d y=%d scale %3.2f\n", tx, ty, psscale);

   UDoLatex(areawin->topinstance, 0, f, psscale, outscale, tx, ty, NULL);

   fprintf(f, "   } %% close \'parbox\'\n");
   fprintf(f, "   } %% close \'scalebox\'\n");
   fprintf(f, "   \\vspace{-\\baselineskip} %% this is not"
		" necessary, but looks better\n");
   fprintf(f, "\\end{center}\n");
   fclose(f);

   Wprintf("Wrote auxiliary file %ls.tex", filename.utf16());
}

