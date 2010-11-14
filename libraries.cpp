/*-------------------------------------------------------------------------*/
/* libraries.c --- xcircuit routines for the builtin and user libraries    */
/* Copyright (c) 2002  Tim Edwards, Johns Hopkins University        	   */
/*-------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------*/
/*      written by Tim Edwards, 8/13/93    				   */
/*-------------------------------------------------------------------------*/

#include <QApplication>
#include <QWidget>

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

#ifdef TCL_WRAPPER 
#include <tk.h>
#endif

#include "colors.h"
#include "xcircuit.h"
#include "prototypes.h"
#include "xcqt.h"

/*-------------------------------------------------------------------------*/
/* Global Variable definitions						   */
/*-------------------------------------------------------------------------*/

extern Cursor	appcursors[NUM_CURSORS];
extern short fontcount;
extern fontinfo *fonts;
extern bool was_preselected;

/*---------------------------------------------------------*/
/* Find the Helvetica font for use in labeling the objects */
/*---------------------------------------------------------*/

short findhelvetica()
{
   short fval;

   if (fontcount == 0) loadfontfile("Helvetica");

   for (fval = 0; fval < fontcount; fval++)
      if (!strcmp(fonts[fval].psname, "Helvetica"))
	 break; 

   /* If not there, use the first Helvetica font */

   if (fval == fontcount) {
      for (fval = 0; fval < fontcount; fval++)
         if (!strcmp(fonts[fval].family, "Helvetica"))
	    break; 
   }

   /* If still not there, use the first non-Symbol font */
   /* If this doesn't work, then the libraries are probably misplaced. . .*/

   if (fval == fontcount) {
      for (fval = 0; fval < fontcount; fval++)
         if (strcmp(fonts[fval].family, "Symbol"))
	    break; 
   }

   return fval;
}

/*-------------------------------------------*/
/* Return to drawing window from the library */
/*-------------------------------------------*/

void catreturn()
{
   /* Pop the object being edited from the push stack. */

   popobject(NULL, Number(1), NULL);
}

/*------------------------------------------------------*/
/* Find page number from cursor position		*/
/* Mode = 0:  Look for exact corresponding page number  */
/*   and return -1 if out-of-bounds			*/
/* Mode = 1:  Look for position between pages, return	*/
/*   page number of page to the right.  		*/
/*------------------------------------------------------*/

int pageposition(short libmode, int x, int y, int mode)
{
   int xin, yin, bpage, pages;
   int gxsize, gysize, xdel, ydel;

   pages = (libmode == PAGELIB) ? xobjs.pages : xobjs.numlibs;
   computespacing(libmode, &gxsize, &gysize, &xdel, &ydel);
   window_to_user(x, y, &areawin->save);

   if (mode == 0) {	/* On-page */
      if (areawin->save.x >= 0 && areawin->save.y <= 0) {
         xin = areawin->save.x / xdel;
         yin = areawin->save.y / ydel; 
         if (xin < gxsize && yin > -gysize) {
            bpage = (xin % gxsize) - (yin * gxsize);
            if (bpage < pages)
	       return bpage;
         }
      }
      return -1;
   }
   else {		/* Between-pages */
      xin = (areawin->save.x + (xdel >> 1)) / xdel;
      if (xin > gxsize) xin = gxsize;
      if (xin < 0) xin = 0;
      yin = areawin->save.y  / ydel; 
      if (yin > 0) yin = 0;
      if (yin < -gysize) yin = -gysize;
      bpage = (xin % (gxsize + 1)) + 1 - (yin * gxsize);
      if (bpage > pages + 1) bpage = pages + 1;
      return bpage;
   }
}

/*------------------------------------------------------*/
/* Find the number of other pages linked to the		*/
/* indicated page (having the same filename, and	*/
/* ignoring empty pages).  result is the total number	*/
/* of pages in the output file.				*/
/*------------------------------------------------------*/

short pagelinks(int page)
{
   int i;
   short count = 0;

   for (i = 0; i < xobjs.pages; i++)
      if (xobjs.pagelist[i].pageinst != NULL)
         if (xobjs.pagelist[i].pageinst->thisobject->parts > 0)
            if ((i == page) || (!filecmp(xobjs.pagelist[i].filename,
                        xobjs.pagelist[page].filename)))
	       count++;

   return count;
}

/*------------------------------------------------------*/
/* This is an expanded version of pagelinks() (above),	*/
/* to deal with the separate issues of independent top-	*/
/* level schematics and subcircuits.  For the indicated	*/
/* page, return a list of pages depending on the mode:	*/
/*							*/
/* mode = INDEPENDENT: independent top-level pages	*/
/* mode = DEPENDENT: dependent pages (subcircuits)	*/
/* mode = PAGE_DEPEND: subcircuits of the current page,	*/
/* mode = LINKED_PAGES: subcircuits of the current	*/
/*	page, including parameter links			*/
/* mode = TOTAL_PAGES: independent pages + subcircuits  */
/* mode = ALL_PAGES: all pages in xcircuit		*/
/*							*/
/* The list is the size of the number of pages, and	*/
/* entries corresponding to the requested mode are set	*/
/* nonzero (the actual number indicates the number of	*/
/* references to the page, which may or may not be	*/
/* useful to know).					*/
/*							*/
/* It is the responsibility of the calling routine to	*/
/* free the memory allocated for the returned list.	*/
/*------------------------------------------------------*/

short *pagetotals(int page, short mode)
{
   int i;
   short *counts, *icount;

   if (xobjs.pagelist[page].pageinst == NULL) return NULL;

   counts = (short *)malloc(xobjs.pages * sizeof(short));
   icount = (short *)malloc(xobjs.pages * sizeof(short));
   for (i = 0; i < xobjs.pages; i++) {
      *(counts + i) = 0;
      *(icount + i) = 0;
   }

   /* Find all the subcircuits of this page */

   if (mode != ALL_PAGES)
      findsubschems(page, xobjs.pagelist[page].pageinst->thisobject,
		0, counts, (mode == LINKED_PAGES) ? true : false);

   /* Check independent entries (top-level pages which are not	*/
   /* subcircuits of another page, but have the same filename	*/
   /* as the page we started from).  Set the counts entry to -1	*/
   /* to mark each independent page.				*/

   if (mode != PAGE_DEPEND)
      for (i = 0; i < xobjs.pages; i++)
         if (xobjs.pagelist[i].pageinst != NULL)
            if (xobjs.pagelist[i].pageinst->thisobject->parts > 0)
	    {
	       if (mode == ALL_PAGES)
		  (*(counts + i)) = 1;
	       else
	       {
                  if ((i == page) || (!filecmp(xobjs.pagelist[i].filename,
                        xobjs.pagelist[page].filename)))
		     if ((mode == INDEPENDENT) || (*(counts + i) == 0))
		        (*(icount + i))++;
	       }
	    }

   /* Check other dependent entries (top-level pages which are 	*/
   /* subcircuits of any independent page).			*/

   if ((mode == DEPENDENT) || (mode == TOTAL_PAGES) || (mode == LINKED_PAGES))
   {
      for (i = 0; i < xobjs.pages; i++)
	 if ((i != page) && (*(icount + i) > 0))
            findsubschems(i, xobjs.pagelist[i].pageinst->thisobject,
		0, counts, (mode == LINKED_PAGES) ? true : false);
   }

   if (mode == INDEPENDENT)
   {
      free((char *)counts);
      return icount;
   }
   else
   {
      if ((mode == TOTAL_PAGES) || (mode == LINKED_PAGES)) {
	 /* merge dependent and independent */
	 for (i = 0; i < xobjs.pages; i++)
	    if (*(icount + i) > 0)
	       (*(counts + i))++;
      }
      free((char *)icount);
      return counts;
   }
}

/*---------------------------------------------------------*/
/* Test whether a library instance is a "virtual" instance */
/*---------------------------------------------------------*/

bool is_virtual(objinstptr thisinst) {
   int libno;
   liblistptr ilist;

   libno = libfindobject(thisinst->thisobject, NULL);

   for (ilist = xobjs.userlibs[libno].instlist; ilist != NULL; ilist = ilist->next)
      if ((ilist->thisinst == thisinst) && (ilist->isvirtual == true))
	 return true;

   return false;
}

/*------------------------------------------------------*/
/* Test whether an object is a page, and return the	*/
/* page number if it is.  Otherwise, return -1.		*/
/*------------------------------------------------------*/

int is_page(objectptr thisobj)
{
   int i;
   
   for (i = 0; i < xobjs.pages; i++)
      if (xobjs.pagelist[i].pageinst != NULL)
         if (xobjs.pagelist[i].pageinst->thisobject == thisobj) return i;

   return -1;
}

/*------------------------------------------------------*/
/* Test whether an object is a library, and return the	*/
/* library number if it is.  Otherwise, return -1.	*/
/*------------------------------------------------------*/

int is_library(objectptr thisobj)
{
   int i;
   
   for (i = 0; i < xobjs.numlibs; i++)
      if (xobjs.libtop[i + LIBRARY]->thisobject == thisobj) return i;

   return -1;
}

/*------------------------------------------------------*/
/* Check for library name (string).  Because XCircuit   */
/* generates the text "Library: <filename>" for		*/
/* library names, we also check against <filename>	*/
/* only in these names (this library name syntax is	*/
/* deprecated, but the check is retained for backwards	*/
/* compatibility).					*/
/*							*/
/* If no library matches the given name, return -1.	*/
/*------------------------------------------------------*/

int NameToLibrary(char *libname)
{
   char *slib;
   int i;

   for (i = 0; i < xobjs.numlibs; i++) {
      slib = xobjs.libtop[i + LIBRARY]->thisobject->name;
      if (!strcmp(libname, slib)) {
	 return i;
      }
      else if (!strncmp(slib, "Library: ", 9)) {
	 if (!strcmp(libname, slib + 9)) {
	    return i;
         }
      }
   }
   return -1;
}

/*------------------------------------------------------*/
/* Move an object and all of its virtual instances from	*/
/* one library to another.				*/
/*------------------------------------------------------*/

int libmoveobject(objectptr thisobject, int libtarget)
{
   int j, libsource;
   liblistptr spec, slast, srch;

   libsource = libfindobject(thisobject, &j);

   if (libsource == libtarget) return libsource; /* nothing to do */
   if (libsource < 0) return libsource;		 /* object not in the library */

   /* Move the object from library "libsource" to library "libtarget" */

   xobjs.userlibs[libtarget].library =
                (objectptr *)realloc(xobjs.userlibs[libtarget].library,
		(xobjs.userlibs[libtarget].number + 1) * sizeof(objectptr));

   *(xobjs.userlibs[libtarget].library + xobjs.userlibs[libtarget].number) = thisobject;
   xobjs.userlibs[libtarget].number++;

   for (; j < xobjs.userlibs[libsource].number; j++)
      *(xobjs.userlibs[libsource].library + j) =
		*(xobjs.userlibs[libsource].library + j + 1);
      xobjs.userlibs[libsource].number--;

   /* Move all instances from library "libsource" to library "libtarget" */

   slast = NULL;
   for (spec = xobjs.userlibs[libsource].instlist; spec != NULL;) {
      if (spec->thisinst->thisobject == thisobject) {

	 /* Add to end of spec list in target */
	 srch = xobjs.userlibs[libtarget].instlist;
	 if (srch == NULL)
	    xobjs.userlibs[libtarget].instlist = spec;
	 else {
	    for (; srch->next != NULL; srch = srch->next);
	    spec->next = srch->next;
	    srch->next = spec;
	 }
	 
	 if (slast != NULL) {
	    slast->next = spec->next;
	    spec = slast->next;
	 }
	 else {
	    xobjs.userlibs[libsource].instlist = spec->next;
	    spec = xobjs.userlibs[libsource].instlist;
	 }
      }
      else {
	 slast = spec;
	 spec = spec->next;
      }
   }

   return libsource;
}

/*------------------------------------------------------*/
/* Determine which library contains the specified	*/
/* object.  If found, return the library number, or	*/
/* else return -1 if the object was not found in any	*/
/* library.  If "partidx" is non-null, fill with the	*/
/* integer offset of the object from the beginning of	*/
/* the library.						*/
/*------------------------------------------------------*/

int libfindobject(objectptr thisobject, int *partidx)
{
   int i, j;
   objectptr libobj;

   for (i = 0; i < xobjs.numlibs; i++) {
      for (j = 0; j < xobjs.userlibs[i].number; j++) {
         libobj = *(xobjs.userlibs[i].library + j);
	 if (libobj == thisobject) {
	    if (partidx != NULL) *partidx = j;
	    return i;
	 }
      }
   }
   return -1;
}

/*------------------------------------------------------*/
/* ButtonPress handler during page catalog viewing mode */
/*------------------------------------------------------*/

void pagecat_op(int op, int x, int y)
{
   int bpage;
   short mode;

   for (mode = 0; mode < LIBRARY; mode++) {
      if (areawin->topinstance == xobjs.libtop[mode]) break;
   }
   if (mode == LIBRARY) return;  /* Something went wrong if this happens */

   if (op != XCF_Cancel) {
      if ((bpage = pageposition(mode, x, y, 0)) >= 0) {
		  
	 if (eventmode == ASSOC_MODE) {
	    if (mode == PAGELIB) {
	       /* using changepage() allows use of new page for schematic */
	       changepage(bpage);
	       /* associate the new schematic */
	       schemassoc(topobject, areawin->stack->thisinst->thisobject);
	       /* pop back to calling (symbol) page */
	       catreturn();
	       eventmode = NORMAL_MODE;
	    }
	    else {
	       areawin->lastlibrary = bpage;
               startcatalog(NULL, Number(LIBRARY + bpage), NULL);
	    }
	    return;
         }
	 else if (op == XCF_Select) {
	    if (mode == PAGELIB)    /* No such method for LIBLIB is defined. */
	       select_add_element(OBJINST);
	 }
	 else if ((op == XCF_Library_Pop) || (op == XCF_Finish)) {

	    /* like catreturn(), but don't actually go to the popped page */
	    unselect_all();
	    eventmode = NORMAL_MODE;
	    if (mode == PAGELIB) {
	       newpage(bpage);
	    }
	    else {
               startcatalog(NULL, Number(LIBRARY + bpage), NULL);
	    }
	    return;
	 }
      }
   }
   else {
      eventmode = NORMAL_MODE;
      catreturn();
   }
}

/*------------------------------------------------------------------------------*/
/* Subroutine to find the correct scale and position of the object instance	*/
/* representing an entire page in the page directory.				*/
/*------------------------------------------------------------------------------*/

void pageinstpos(short mode, short tpage, objinstptr drawinst, int gxsize,
	int gysize, int xdel, int ydel)
{
   Q_UNUSED(mode);
   Q_UNUSED(gysize);
   objectptr libobj = drawinst->thisobject;
   float scalex, scaley;

   drawinst->position.x = (tpage % gxsize) * xdel;
   drawinst->position.y = -(tpage / gxsize + 1) * ydel;

   /* center the object on its page bounding box */

   if (drawinst->bbox.width == 0 || drawinst->bbox.height == 0) {
      drawinst->scale = 0.45 * libobj->viewscale;
      drawinst->position.x += 0.05 * xdel - libobj->pcorner.x * drawinst->scale;
      drawinst->position.y += 0.05 * ydel - libobj->pcorner.y * drawinst->scale;
   }
   else {
      scalex = (0.9 * xdel) / drawinst->bbox.width;
      scaley = (0.9 * ydel) / drawinst->bbox.height;
      if (scalex > scaley) {
         drawinst->scale = scaley;
	 drawinst->position.x -= (drawinst->bbox.lowerleft.x * scaley);
         drawinst->position.x += (xdel - (drawinst->bbox.width * scaley)) / 2;
         drawinst->position.y += 0.05 * ydel - drawinst->bbox.lowerleft.y
			* drawinst->scale;
      }
      else {
         drawinst->scale = scalex;
	 drawinst->position.y -= (drawinst->bbox.lowerleft.y * scalex);
         drawinst->position.y += (ydel - (drawinst->bbox.height * scalex)) / 2;
         drawinst->position.x += 0.05 * xdel - drawinst->bbox.lowerleft.x
			* drawinst->scale;
      }
   }
}

/*-----------------------------------------------------------*/
/* Find spacing of objects for pages in the page directories */
/*-----------------------------------------------------------*/

void computespacing(short mode, int *gxsize, int *gysize, int *xdel, int *ydel)
{
   int pages = (mode == PAGELIB) ? xobjs.pages : xobjs.numlibs;

   *gxsize = (int)sqrt((double)pages) + 1;
   *gysize = 1 + pages / (*gxsize);

   /* 0.5 is the default vscale;  g#size is the number of pages per line */

   *xdel = areawin->width() / (0.5 * (*gxsize));
   *ydel = areawin->height() / (0.5 * (*gysize));
}

/*-------------------------------------------------------------------*/
/* Draw the catalog of page ordering or the library master directory */
/*-------------------------------------------------------------------*/

void composepagelib(short mode)
{
   objinstptr drawinst;
   objectptr libobj, directory = xobjs.libtop[mode]->thisobject;
   short i;
   polyptr *drawbox;
   labelptr *pagelabel;
   stringpart *strptr;
   XPoint* pointptr;
   int margin, xdel, ydel, gxsize, gysize;
   int pages = (mode == PAGELIB) ? xobjs.pages : xobjs.numlibs;
   short fval = findhelvetica();

   /* Like the normal libraries, instances come from a special list, so	 */
   /* they should not be destroyed, but will be null'd out and retrieved */
   /* from the list.							 */

   for (objinstiter optr; directory->values(optr); )
      optr.clear();

   directory->clear();

   /* generate the list of object instances */

   computespacing(mode, &gxsize, &gysize, &xdel, &ydel);
   margin = xdel / 40;	/* margin between pages */

   for (i = 0; i < pages; i++) {
      drawinst = (mode == PAGELIB) ? xobjs.pagelist[i].pageinst :
                xobjs.libtop[i + LIBRARY];
      if (drawinst != NULL) {
	 libobj = drawinst->thisobject;

	 /* This is a stop-gap measure. . . should be recalculating the bounds of */
	 /* the instance on every action, not just before arranging the library.  */
	 drawinst->bbox.lowerleft.x = libobj->bbox.lowerleft.x;
	 drawinst->bbox.lowerleft.y = libobj->bbox.lowerleft.y;
	 drawinst->bbox.width = libobj->bbox.width;
	 drawinst->bbox.height = libobj->bbox.height;
	 /* End stop-gap measure */

         directory->append(drawinst);
         pageinstpos(mode, i, drawinst, gxsize, gysize, xdel, ydel);
      }

      /* separate pages (including empty ones) with bounding boxes */

      drawbox = directory->append(new polygon);
      (*drawbox)->color = LOCALPINCOLOR;   /* default red */
      (*drawbox)->style = NORMAL;      	   /* CLOSED */
      (*drawbox)->width = 1.0;
      (*drawbox)->points.resize(4);
      pointptr = (*drawbox)->points.begin();
      pointptr[0].x = (i % gxsize) * xdel + margin;
      pointptr[0].y = -(i / gxsize) * ydel - margin;
      pointptr[1].x = ((i % gxsize) + 1) * xdel - margin;
      pointptr[1].y = -(i / gxsize) * ydel - margin;
      pointptr[2].x = ((i % gxsize) + 1) * xdel - margin;
      pointptr[2].y = -((i / gxsize) + 1) * ydel + margin;
      pointptr[3].x = (i % gxsize) * xdel + margin;
      pointptr[3].y = -((i / gxsize) + 1) * ydel + margin;

      /* each page gets its name printed at the bottom */

      if (drawinst != NULL) {
         pagelabel = directory->append(new label(false, XPoint((pointptr->x + (pointptr-1)->x) / 2,
                        pointptr->y - 5)));
	 (*pagelabel)->color = DEFAULTCOLOR;
	 (*pagelabel)->scale = 0.75;
         (*pagelabel)->string->data.font = fval;
	 strptr = makesegment(&((*pagelabel)->string), NULL);
	 strptr->type = TEXT_STRING;
	 strptr->data.string = (char *) malloc(1 + strlen(libobj->name));
	 strcpy(strptr->data.string, libobj->name);
         (*pagelabel)->justify = TOP | NOTBOTTOM | NOTLEFT;
      }
   }

   /* calculate a bounding box for this display */
   /* and center it in its window */

   calcbbox(xobjs.libtop[mode]);
   centerview(xobjs.libtop[mode]);
}

/*------------------------------------------------------------*/
/* Update the page or library directory based on new bounding */
/* box information for the page or library passed in "tpage". */
/*------------------------------------------------------------*/

void updatepagelib(short mode, short tpage)
{
   objectptr compobj, libinst = xobjs.libtop[mode]->thisobject;
   int xdel, ydel, gxsize, gysize, lpage;

   /* lpage is the number of the page as found on the directory page */
   lpage = (mode == PAGELIB) ? tpage : tpage - LIBRARY;
   compobj = (mode == PAGELIB) ? xobjs.pagelist[tpage].pageinst->thisobject
		: xobjs.libtop[tpage]->thisobject;

   computespacing(mode, &gxsize, &gysize, &xdel, &ydel);

   bool hit = false;
   for (objinstiter pinst; libinst->values(pinst); ) {
       if (pinst->thisobject == compobj) {
           /* recalculate scale and position of the object instance */
           pageinstpos(mode, lpage, pinst, gxsize, gysize, xdel, ydel);
           hit = true;
           break;
       }
   }

   /* if there is no instance for this page, then recompose the whole library */

   if (!hit) composelib(mode);
}

/*----------------------*/
/* Rearrange pages	*/
/*----------------------*/

void pagecatmove(int x, int y)
{
   int bpage;
   objinstptr exchobj;

   if (areawin->selects == 0) return;
   else if (areawin->selects > 2) {
      Wprintf("Select maximum of two objects.");
      return;
   }

   /* Get the page corresponding to the first selected object */
   exchobj = SELTOOBJINST(areawin->selectlist);
   int page = xobjs.pagelist.indexOf(exchobj);

   /* If two objects are selected, then exchange their order */
   if (areawin->selects == 2) {
      exchobj = SELTOOBJINST(areawin->selectlist + 1);
      int page2 = xobjs.pagelist.indexOf(exchobj);
      std::swap(xobjs.pagelist[page], xobjs.pagelist[page2]);
   }

   /* If one object selected; find place to put from cursor position */
   else if ((bpage = pageposition(PAGELIB, x, y, 1)) >= 0) {

      /* move page (page) to position between current pages */
      /* (bpage - 2) and (bpage - 1) by shifting pointers.   */

      if ((bpage - 1) < page) {
         xobjs.pagelist.insert(bpage-1, xobjs.pagelist[page]);
         xobjs.pagelist.removeAt(page+1);
         for (int i = 0; i < xobjs.pagelist.count(); ++i) {
            renamepage(i);
         }
      }
      else if ((bpage - 2) > page) {
         xobjs.pagelist.insert(bpage-1, xobjs.pagelist[page]);
         xobjs.pagelist.removeAt(page);
         for (int i = 0; i < xobjs.pagelist.count(); ++i) {
             renamepage(i);
         }
      }
   }

   unselect_all();
   composelib(PAGELIB);
   areawin->update();
}

/*-----------------------------------------*/
/* Draw the catalog of predefined elements */
/*-----------------------------------------*/

void composelib(short mode)
{
   objinstptr drawinst;
   labelptr *drawname;
   objectptr libobj, libpage = xobjs.libtop[mode]->thisobject;
   liblistptr spec;
   int xpos = 0, ypos = areawin->height() << 1;
   int nypos = 220, nxpos;
   short fval;
   short llx, lly, width, height;

   int totalarea, targetwidth;
   double scale, savescale;
   XPoint savepos;

   /* Also make composelib() a wrapper for composepagelib() */
   if ((mode > FONTLIB) && (mode < LIBRARY)) {
      composepagelib(mode);
      return;
   }

   /* The instances on the library page come from the library's		*/
   /* "instlist".  So that we don't destroy the actual instance when we	*/
   /* call reset(), we find the pointer to the instance and NULL it.	*/
   
   for (objinstiter pgen; libpage->values(pgen); )
      pgen.clear();

   /* Before resetting, save the position and scale.  We will restore	*/
   /* them at the end.							*/

   savepos = libpage->pcorner;
   savescale = libpage->viewscale;
   libpage->clear();

   /* Return if library defines no objects or virtual instances */

   if (xobjs.userlibs[mode - LIBRARY].instlist == NULL) return;

   /* Find the Helvetica font for use in labeling the objects */

   fval = findhelvetica();

   /* experimental:  attempt to produce a library with the same aspect  */
   /* ratio as the drawing window.                                      */
   const short minWidth = 200; // minimum box width
   const short minHeight = 220; // minimum box height

   totalarea = 0;
   for (spec = xobjs.userlibs[mode - LIBRARY].instlist; spec != NULL;
                spec = spec->next) {
      libobj = spec->thisinst->thisobject;

      /* "Hidden" objects are not drawn */
      if (libobj->hidden) continue;

      drawinst = spec->thisinst;
      drawinst->position.x = 0;
      drawinst->position.y = 0;

      /* Get the bounding box of the instance in the page's coordinate system */
      calcinstbbox((genericptr *)(&drawinst), &llx, &lly, &width, &height);
      width -= llx;	/* convert urx to width */
      height -= lly;	/* convert ury to height */
      width += 30;	/* space padding */
      height += 30;	/* height padding */
      width = qMax(width, minWidth);
      height = qMax(height, minHeight);
      totalarea += (width * height);
   }

   const double screenWidth = qApp->desktop()->width();
   const double screenHeight = qApp->desktop()->height();
   scale = (double)totalarea / (screenWidth * screenHeight);
   targetwidth = (int)(sqrt(scale) * screenWidth);

   /* generate the list of object instances and their labels */

   for (spec = xobjs.userlibs[mode - LIBRARY].instlist; spec != NULL;
		spec = spec->next) {
      libobj = spec->thisinst->thisobject;

      /* "Hidden" objects are not drawn */
      if (libobj->hidden) continue;

      drawinst = spec->thisinst;
      drawinst->position.x = 0;
      drawinst->position.y = 0;

      /* Get the bounding box of the instance in the page's coordinate system */
      calcinstbbox((genericptr *)(&drawinst), &llx, &lly, &width, &height);
      width -= llx;  /* convert urx to width */
      height -= lly; /* convert ury to height */

      /* Determine the area needed on the page to draw the object */

      nxpos = xpos + ((width > 170) ? width + 30 : minWidth);
      /* if ((nxpos > (areawin->width << 1)) && (xpos > 0)) { */
      if ((nxpos > targetwidth) && (xpos > 0)) {
	 nxpos -= xpos; 
	 xpos = 0;
	 ypos -= nypos;
	 nypos = 200;
      }
      /* extra space of 20 is to leave room for the label */

      if (height > (nypos - 50)) nypos = height + 50;

      drawinst->position.x = xpos - llx;
      drawinst->position.y = ypos - (height + lly);
      if (width <= 170) drawinst->position.x += ((170 - width) >> 1);
      if (height <= 170) drawinst->position.y -= ((170 - height) >> 1);
      drawinst->color = DEFAULTCOLOR;

      libpage->append(drawinst);

      if (fval < fontcount) {
         stringpart *strptr;

         drawname = libpage->append(new label(false, XPoint(0,0)));
	 (*drawname)->color = (spec->isvirtual) ?
			OFFBUTTONCOLOR : DEFAULTCOLOR;
         (*drawname)->scale = 0.75;
	 (*drawname)->string->data.font = fval;
	 strptr = makesegment(&((*drawname)->string), NULL);
	 strptr->type = TEXT_STRING;

         strptr->data.string = strdup(libobj->name);
         (*drawname)->justify = TOP | NOTBOTTOM | NOTLEFT;

         if (width > 170)
            (*drawname)->position.x = xpos + (width >> 1);
         else
            (*drawname)->position.x = xpos + 85;

         if (height > 170)
            (*drawname)->position.y = drawinst->position.y + lly - 10;
         else
            (*drawname)->position.y = ypos - 180;
      }
      xpos = nxpos;
   }

   /* Compute the bounding box of the library page */
   calcbbox(xobjs.libtop[mode]);

   /* Update the library directory */
   updatepagelib(LIBLIB, mode);

   /* Restore original view position */
   libpage->pcorner = savepos;
   libpage->viewscale = savescale;
}

/*----------------------------------------------------------------*/
/* Find any dependencies on an object.				  */
/*   Return values:  0 = no dependency, 1 = dependency on page,	  */
/*	2 = dependency in another library object.		  */
/*   Object/Page with dependency (if any) returned in "compobjp". */
/*----------------------------------------------------------------*/

short finddepend(objinstptr libobj, objectptr **compobjp)
{
   short page, i, j;
   objectptr *compobj;
  
   for (i = 0; i < xobjs.numlibs; i++) {
      for (j = 0; j < xobjs.userlibs[i].number; j++) {
	 compobj = xobjs.userlibs[i].library + j;
         *compobjp = compobj;
		     
         for (objinstiter testobj; (*compobj)->values(testobj); ) {
               if (testobj->thisobject == libobj->thisobject) return 2;
	 }
      }
   }

   /* also look in the xobjs.pagelist */

   for (page = 0; page < xobjs.pages; page++) {
      if (xobjs.pagelist[page].pageinst == NULL) continue;
      compobj = &(xobjs.pagelist[page].pageinst->thisobject);
      *compobjp = compobj;
      for (objinstiter testobj; (*compobj)->values(testobj); ) {
            if (testobj->thisobject == libobj->thisobject) return 1;
      }
   }
   return 0;
}

/*--------------------------------------------------------------*/
/* Virtual copy:  Make a separate copy of an object on the same	*/
/* library page as the original, representing an instance of	*/
/* the object with different parameters.  The object must have	*/
/* parameters for this to make sense, so check for parameters	*/
/* before allowing the virtual copy.				*/
/*--------------------------------------------------------------*/

void catvirtualcopy()
{
   short i, *newselect;
   objinstptr libobj, libinst;

   if (areawin->selects == 0) return;
   else if ((i = is_library(topobject)) < 0) return;

   /* Check for existance of parameters in the object for each */
   /* selected instance */

   for (newselect = areawin->selectlist; newselect < areawin->selectlist
	  + areawin->selects; newselect++) {
      libobj = SELTOOBJINST(newselect);
      libinst = addtoinstlist(i, libobj->thisobject, true);
      *libinst = *libobj;
      tech_mark_changed(GetObjectTechnology(libobj->thisobject));
   }

   clearselects();
   composelib(LIBRARY + i);
   areawin->update();
}

/*----------------------------------------------------------------*/
/* "Hide" an object (must have a dependency or else it disappears)*/
/*----------------------------------------------------------------*/

void cathide()
{
   int i;
   short *newselect;
   objectptr *compobj;
   objinstptr libobj;

   if (areawin->selects == 0) return;

   /* Can only hide objects which are instances in other objects; */
   /* Otherwise, object would be "lost".			  */

   for (newselect = areawin->selectlist; newselect < areawin->selectlist
	  + areawin->selects; newselect++) {
      libobj = SELTOOBJINST(newselect);

      if (finddepend(libobj, &compobj) == 0) {
	 Wprintf("Cannot hide: no dependencies");
      }
      else { 		/* All's clear to hide. */
	 libobj->thisobject->hidden = true;
      }
   }

   clearselects();

   if ((i = is_library(topobject)) >= 0) composelib(LIBRARY + i);

   areawin->update();
}

/*----------------------------------------------------------------*/
/* Delete an object from the library if there are no dependencies */
/*----------------------------------------------------------------*/

void catdelete()
{
   short *newselect, *libpobjs;
   int i;
   objinstptr libobj;
   liblistptr ilist, llist;
   objectptr *libpage, *compobj, *tlib, *slib;

   if (areawin->selects == 0) return;

   if ((i = is_library(topobject)) >= 0) {
      libpage = xobjs.userlibs[i].library;
      libpobjs = &xobjs.userlibs[i].number;
   }
   else
      return;  /* To-do: Should have a mechanism here for deleting pages! */

   for (newselect = areawin->selectlist; newselect < areawin->selectlist
	  + areawin->selects; newselect++) {
      libobj = SELTOOBJINST(newselect);

      /* If this is just a "virtual copy", simply remove it from the list */

      llist = NULL;
      for (ilist = xobjs.userlibs[i].instlist; ilist != NULL;
		llist = ilist, ilist = ilist->next) {
	 if ((ilist->thisinst == libobj) && (ilist->isvirtual == true)) {
	    if (llist == NULL)
	       xobjs.userlibs[i].instlist = ilist->next;
	    else
	       llist->next = ilist->next;
	    break;
	 }
      }
      if (ilist != NULL) {
         delete ilist;
	 continue;
      }

      /* Cannot delete an object if another object uses an instance of it, */
      /* or if the object is used on a page.				   */

      if (finddepend(libobj, &compobj)) {
	 Wprintf("Cannot delete: dependency in \"%s\"", (*compobj)->name);
      }
      else { 		/* All's clear to delete.		     	   */

         /* Clear the undo stack so that any references to this object	*/
	 /* won't screw up the database (this could be kinder & gentler	*/
	 /* by first checking if there are any references to the object	*/
	 /* in the undo stack. . .					*/

	 flush_undo_stack();

	 /* Next, remove the object from the library page. */

	 for (tlib = libpage; tlib < libpage + *libpobjs; tlib++)
	    if ((*tlib) == libobj->thisobject) {
	       for (slib = tlib; slib < libpage + *libpobjs - 1; slib++)
		  (*slib) = (*(slib + 1));
	       (*libpobjs)--;
	       break;
	    }
	 
	 /* Next, remove all instances of the object on	the library page. */
	  
         llist = NULL;
         for (ilist = xobjs.userlibs[i].instlist; ilist != NULL;
		llist = ilist, ilist = ilist->next) {
	    if (ilist->thisinst->thisobject == libobj->thisobject) {
	       if (llist == NULL) {
	          xobjs.userlibs[i].instlist = ilist->next;
                  delete ilist;
	          if (!(ilist = xobjs.userlibs[i].instlist)) break;
	       }
	       else {
	          llist->next = ilist->next;
                  delete ilist;
	          if (!(ilist = llist)) break;
	       }
	    }
	 }

	 /* Finally, delete the object (permanent---no undoing this!) */
         tech_mark_changed(GetObjectTechnology(libobj->thisobject));
         delete libobj->thisobject;
      }
   }

   clearselects();

   if ((i = is_library(topobject)) >= 0) {
      composelib(LIBRARY + i);
   }

   areawin->update();
}

/*------------------------------------------------------*/
/* Linked list rearrangement routines			*/
/*------------------------------------------------------*/

void linkedlistswap(liblistptr *spec, int o1, int o2)
{
   liblistptr s1, s1m, s2, s2m, stmp;
   int j;

   if (o1 == o2) return;

   s1m = NULL;
   s1 = *spec;
   for (j = 0; j < o1; j++) {
      s1m = s1;
      s1 = s1->next;
   }

   s2m = NULL;
   s2 = *spec;
   for (j = 0; j < o2; j++) {
      s2m = s2;
      s2 = s2->next;
   }

   if (s2m)
      s2m->next = s1;
   else
      *spec = s1;

   if (s1m)
      s1m->next = s2;
   else
      *spec = s2;

   stmp = s1->next;
   s1->next = s2->next;
   s2->next = stmp;
}

/*------------------------------------------------------*/

void linkedlistinsertafter(liblistptr *spec, int o1, int o2)
{
  liblistptr s1, s1m, s2;
   int j;

   if ((o1 == o2) || (o1 == (o2 + 1))) return;

   s1m = NULL;
   s1 = *spec;
   for (j = 0; j < o1; j++) {
      s1m = s1;
      s1 = s1->next;
   }

   s2 = *spec;
   for (j = 0; j < o2; j++)
      s2 = s2->next;

   if (s1m)
      s1m->next = s1->next;
   else
      *spec = s1->next;

   if (o2 == -1) {   /* move s1 to front */
      s1->next = *spec;
      *spec = s1;
   }
   else {
      s1->next = s2->next;
      s2->next = s1;
   }
}

/*------------------------------------------------------*/
/* Set the "changed" flag in a library if any object	*/
/* in that library has changed.				*/
/*							*/
/* If "technology" is NULL, check all objects,		*/
/* otherwise only check objects with a matching		*/
/* technology.						*/
/*------------------------------------------------------*/

void tech_set_changes(TechPtr refns)
{
   TechPtr ns;
   int i, j;
   objectptr thisobj;

   for (i = 0; i < xobjs.numlibs; i++) {
      for (j = 0; j < xobjs.userlibs[i].number; j++) {
         thisobj = *(xobjs.userlibs[i].library + j);
         if (getchanges(thisobj) > 0) {
            ns = GetObjectTechnology(thisobj);
	    if ((refns == NULL) || (refns == ns)) 
	       ns->flags |= LIBRARY_CHANGED;
	 }
      }
   }
}

/*------------------------------------------------------*/
/* Mark a technology as having been modified.		*/
/*------------------------------------------------------*/

void tech_mark_changed(TechPtr ns)
{
   if (ns != NULL) ns->flags |= LIBRARY_CHANGED;
}

/*------------------------------------------------------*/
/* Rearrange objects in the library			*/
/*------------------------------------------------------*/

void catmove(int x, int y)
{
  int i, j, k, s1, s2, ocentx, ocenty, rangey, l;
   liblistptr spec;
   objinstptr exchobj, lobj;

   /* make catmove() a wrapper for pagecatmove() */

   if ((i = is_library(topobject)) < 0) {
      pagecatmove(x, y);
      return;
   }

   if (areawin->selects == 0) return;

   /* Add selected object or objects at the cursor position */

   window_to_user(x, y, &areawin->save);

   s2 = -1;
   for (j = 0, spec = xobjs.userlibs[i].instlist; spec != NULL;
		spec = spec->next, j++) {
      lobj = spec->thisinst;
      for (k = 0; k < areawin->selects; k++) {
	 exchobj = SELTOOBJINST(areawin->selectlist + k);
	 if (lobj == exchobj) break;
      }
      if (k < areawin->selects) continue;	/* ignore items being moved */

      ocentx = lobj->position.x + lobj->bbox.lowerleft.x
	        + (lobj->bbox.width >> 1);
      ocenty = lobj->position.y + lobj->bbox.lowerleft.y
	        + (lobj->bbox.height >> 1);
      rangey = (lobj->bbox.height > 200) ? 
	        (lobj->bbox.height >> 1) : 100;

      if ((areawin->save.y < ocenty + rangey) && (areawin->save.y 
	         > ocenty - rangey)) {
	 s2 = j - 1;
	 if (areawin->save.x < ocentx) break;
	 else s2 = j;
      }
   }
   if ((s2 == -1) && (spec == NULL)) {
      if (areawin->save.y <
		xobjs.libtop[i + LIBRARY]->thisobject->bbox.lowerleft.y)
	 s2 = j - 1;
      else if (areawin->save.y <=
		xobjs.libtop[i + LIBRARY]->thisobject->bbox.lowerleft.y +
		xobjs.libtop[i + LIBRARY]->thisobject->bbox.height) {
	 unselect_all();	
	 Wprintf("Could not find appropriate place to insert object");
	 return;
      }
   }

   /* Find object number s2 (because s2 value may change during insertion) */
   if (s2 > -1) {
      for (k = 0, spec = xobjs.userlibs[i].instlist; k < s2; spec = spec->next, k++);
      lobj = spec->thisinst;
   }
   else lobj = NULL;

   /* Move items; insert them after item s2 in order selected */

   j = i;
   for (k = 0; k < areawin->selects; k++) {

      /* Find number of lobj (may have changed) */

      if (lobj == NULL)
	 s2 = -1;
      else {
         for (s2 = 0, spec = xobjs.userlibs[i].instlist; spec != NULL;
		spec = spec->next, s2++)
	    if (spec->thisinst == lobj)
	       break;
      }

      exchobj = SELTOOBJINST(areawin->selectlist + k);
      for (s1 = 0, spec = xobjs.userlibs[i].instlist; spec != NULL;
		spec = spec->next, s1++)
         if (spec->thisinst == exchobj)
	    break;

      if (spec == NULL) {
	 /* Object came from another library */
         if ((l = libmoveobject(exchobj->thisobject, i)) >= 0) j = l;
      }
      else {
         linkedlistinsertafter(&(xobjs.userlibs[i].instlist), s1, s2);
      }
   }

   unselect_all();
   composelib(LIBRARY + i);
   if (j != i) {
      composelib(LIBRARY + j);
      centerview(xobjs.libtop[LIBRARY + j]);
   }

   areawin->update();
}

/*------------------------------------------------------*/
/* Make a duplicate of an object, put in the User	*/
/* Library or the current library (if we're in one).	*/
/*------------------------------------------------------*/

/// \todo most of this should go into the object operator=
void copycat()
{
   short *newselect;
   objectptr *newobj, *curlib, oldobj;
   objinstptr libobj;
   oparamptr ops, newops;
   int libnum;

   libnum = is_library(topobject);
   if (libnum < 0) libnum = USERLIB - LIBRARY;  /* default */

   for (newselect = areawin->selectlist; newselect < areawin->selectlist
	  + areawin->selects; newselect++) {

      libobj = SELTOOBJINST(newselect);
      oldobj = libobj->thisobject;

      /* generate new entry in user library */

      curlib = (objectptr *) realloc(xobjs.userlibs[libnum].library,
	 (xobjs.userlibs[libnum].number + 1) * sizeof(objectptr));
      xobjs.userlibs[libnum].library = curlib;
      newobj = xobjs.userlibs[libnum].library + xobjs.userlibs[libnum].number;
      *newobj = new object;
      xobjs.userlibs[libnum].number++;

      /* give the new object a unique name */

      sprintf((*newobj)->name, "_%s", oldobj->name);
      checkname(*newobj);

      /* copy other object properties */

      (*newobj)->bbox = oldobj->bbox;
      (*newobj)->pcorner = oldobj->pcorner;
      (*newobj)->viewscale = oldobj->viewscale;
      /* don't attach the same schematic. . . */
      (*newobj)->symschem = NULL;
      /* don't copy highlights */
      (*newobj)->highlight.netlist = NULL;
      (*newobj)->highlight.thisinst = NULL;

      /* Copy the parameter structure */
      (*newobj)->params = NULL;
      for (ops = oldobj->params; ops != NULL; ops = ops->next) {
         newops = new oparam;
	 newops->next = (*newobj)->params;
	 newops->key = strdup(ops->key);
	 (*newobj)->params = newops;
	 newops->type = ops->type;
	 newops->which = ops->which;
	 switch (ops->type) {
	    case XC_INT:
	       newops->parameter.ivalue = ops->parameter.ivalue;
	       break;
	    case XC_FLOAT:
	       newops->parameter.fvalue = ops->parameter.fvalue;
	       break;
	    case XC_STRING:
	       newops->parameter.string = stringcopy(ops->parameter.string);
	       break;
	    case XC_EXPR:
	       newops->parameter.expr = strdup(ops->parameter.expr);
	       break;
	 }
      }

      (*newobj)->schemtype = oldobj->schemtype;
      (*newobj)->netnames = NULL;
      (*newobj)->ports = NULL;
      (*newobj)->calls = NULL;
      (*newobj)->polygons = NULL;
      (*newobj)->labels = NULL;
      (*newobj)->valid = false;
      (*newobj)->traversed = false;
      (*newobj)->hidden = false;

      /* copy over all the elements of the original object */

      *static_cast<Plist*>(*newobj) = *static_cast<Plist*>(oldobj);
   }

   /* make instance for library and measure its bounding box */
   addtoinstlist(USERLIB - LIBRARY, *newobj, false);

   composelib(USERLIB);
   unselect_all();

   if (areawin->topinstance == xobjs.libtop[USERLIB])
      areawin->update();
   else startcatalog(NULL, Number(USERLIB), NULL);
}

/*--------------------------------------------------------*/
/* ButtonPress handler during normal catalog viewing mode */
/*--------------------------------------------------------*/

void catalog_op(int op, int x, int y)
{
   short *newselect;
   objinstptr *newobject;
   objectptr libpage = topobject;
   short ocentx, ocenty, rangex, rangey, xdiff, ydiff, flag = 0;
   XPoint oldpos;

   /* Make catalog_op() a wrapper for pagecat_op() */

   if (is_library(topobject) < 0) {
      pagecat_op(op, x, y);
      return;
   }

   /* If XCF_Cancel was invoked, return without a selection. */

   if (op == XCF_Cancel) {
      eventmode = NORMAL_MODE;
      catreturn();
   }
   else {

      window_to_user(x, y, &areawin->save);
      for (objinstiter libobj; topobject->values(libobj); ) {
            ocentx = libobj->position.x + libobj->bbox.lowerleft.x
                + (libobj->bbox.width >> 1);
            ocenty = libobj->position.y + libobj->bbox.lowerleft.y
                + (libobj->bbox.height >> 1);

            rangex = (libobj->bbox.width > 200) ?
                (libobj->bbox.width >> 1) : 100;
            rangey = (libobj->bbox.height > 200) ?
                (libobj->bbox.height >> 1) : 100;

	    if (areawin->save.x > ocentx - rangex && areawin->save.x <
		   ocentx + rangex && areawin->save.y < ocenty + rangey
		   && areawin->save.y > ocenty - rangey) {

	       /* setup to move object around and draw the selected object */

	       if (eventmode == ASSOC_MODE) {

	          /* revert to old page */
		  topobject->viewscale = areawin->vscale;
		  topobject->pcorner = areawin->pcorner;
	          areawin->topinstance = (areawin->stack == NULL) ?
                           xobjs.pagelist[areawin->page].pageinst
			   : areawin->stack->thisinst;
		  /* associate the new object */
                  schemassoc(topobject, libobj->thisobject);
   	          setpage(true);
		  catreturn();
		  eventmode = NORMAL_MODE;
	       }
	       else if ((op == XCF_Library_Pop) || (op == XCF_Library_Copy)) {
		  int saveselects;

	          /* revert to old page */

		  topobject->viewscale = areawin->vscale;
		  topobject->pcorner = areawin->pcorner;
	          areawin->topinstance = (areawin->stack == NULL) ?
                           xobjs.pagelist[areawin->page].pageinst
			   : areawin->stack->thisinst;

	          /* retrieve drawing window state and set position of object to
	             be correct in reference to that window */

	          snap(x, y, &oldpos);

		  saveselects = areawin->selects;
		  areawin->selects = 0;
   	          setpage(false);
		  areawin->selects = saveselects;
	    
	          snap(x, y, &areawin->save);
	          xdiff = areawin->save.x - oldpos.x;
	          ydiff = areawin->save.y - oldpos.y;

                  /* collect all of the selected items */

	          for (newselect = areawin->selectlist; newselect <
		     areawin->selectlist + areawin->selects; newselect++) {
                     newobject = topobject->append(new objinst(*TOOBJINST(libpage->begin() + *newselect)));
		     /* color should be recast as current color */
		     (*newobject)->color = areawin->color;
		     /* position modified by (xdiff, ydiff) */
		     (*newobject)->position.x += xdiff;
		     (*newobject)->position.y += ydiff;

                     u2u_snap((*newobject)->position);
                     *newselect = (short)(newobject - (objinstptr *)topobject->begin());
                     if ((*newobject)->thisobject == libobj->thisobject)
		        flag = 1;
	          }

                  /* add final object to the list of object instances */

	          if (!flag) {
                     newobject = topobject->append(new objinst(**libobj));
		     (*newobject)->color = areawin->color;
                     (*newobject)->position = areawin->save;

	             /* add this object to the list of selected items */

	             newselect = allocselect();
                     *newselect = (short)(newobject - (objinstptr *)topobject->begin());

	          }
	          if (op == XCF_Library_Copy) {

		     /* Key "c" pressed for "copy" (default binding) */

                     XDefineCursor(areawin->viewport, COPYCURSOR);
                     eventmode = COPY_MODE;
#ifndef TCL_WRAPPER
                     xcAddEventHandler(areawin->viewport, PointerMotionMask, false,
			  (XtEventHandler)xlib_drag, NULL);
#endif
	          }
	          else {
                     eventmode = MOVE_MODE;
		     was_preselected = false;
                     register_for_undo(XCF_Library_Pop, UNDO_MORE, areawin->topinstance,
                                                     areawin->selectlist, areawin->selects);
	          }
#ifdef TCL_WRAPPER
		  /* fprintf(stderr, "Creating event handler for xctk_drag: "); */
		  /* printeventmode();		*/
                  Tk_CreateEventHandler(areawin->area, PointerMotionMask,
			(Tk_EventProc *)xctk_drag, NULL);
#endif
                  catreturn();
               }

               /* Select the closest element and stay in the catalog.	   */
	       /* Could just do "select_element" here, but would not cover */
	       /* the entire area in the directory surrounding the object. */

	       else if (op == XCF_Select) {
                  short newinst = (short)(libobj - topobject->begin());
		  /* (ignore this object if it is already in the list of selects) */
		  for (newselect = areawin->selectlist; newselect <
			areawin->selectlist + areawin->selects; newselect++)
		     if (*newselect == newinst) break;
		  if (newselect == areawin->selectlist + areawin->selects) {
		     newselect = allocselect();
                     *newselect = newinst;
                     areawin->update();
		  }
	       }
	       break;
	    }
	 }
   }
}

/*------------------------------*/
/* Switch to the next catalog	*/
/*------------------------------*/

void changecat()
{
   int i, j;

   if ((i = is_library(topobject)) < 0) {
      if (areawin->lastlibrary >= xobjs.numlibs) areawin->lastlibrary = 0;
      j = areawin->lastlibrary;
      eventmode = CATALOG_MODE;
   }
   else {
      j = (i + 1) % xobjs.numlibs;
      if (j == i) {
	 Wprintf("This is the only library.");
	 return;
      }
      areawin->lastlibrary = j;
   }

   if (eventmode == CATMOVE_MODE)
      delete_for_xfer(areawin->selectlist, areawin->selects);

   startcatalog(NULL, Number(j + LIBRARY), NULL);
}

void changecat_call(QAction*, void*, void*)
{
    changecat();
}

/*--------------------------------------*/
/* Begin catalog viewing mode		*/
/*--------------------------------------*/

void startcatalog(QAction*, void* libmod_, void*)
{
   if (xobjs.libtop == NULL) return;	/* No libraries defined */

   unsigned int libmod = (unsigned int)libmod_;

   if ((xobjs.libtop[libmod]->thisobject == NULL) ||
                (areawin->topinstance == xobjs.libtop[libmod])) return;

   if (libmod == FONTLIB) {
      XDefineCursor (areawin->viewport, DEFAULTCURSOR);
      if (eventmode == TEXT_MODE)
         eventmode = FONTCAT_MODE;
      else
         eventmode = EFONTCAT_MODE;
   }
   else if (eventmode == ASSOC_MODE) {
      XDefineCursor (areawin->viewport, DEFAULTCURSOR);
   }
   else if (libmod == PAGELIB || libmod == LIBLIB) {
      XDefineCursor (areawin->viewport, DEFAULTCURSOR);
      eventmode = CATALOG_MODE;
   }
   else if (eventmode != CATMOVE_MODE) {
       /* Don't know why I put this here---causes xcircuit to redraw	*/
       /* the schematic view when switching between library pages.	*/
       // finish_op(XCF_Cancel, 0, 0);
      eventmode = CATALOG_MODE;
   }

   /* Push the current page onto the push stack, unless	we're going */
   /* to a library from the library directory or vice versa, or	    */
   /* library to library.					    */

   if (!(((is_library(topobject) >= 0)
		|| (areawin->topinstance == xobjs.libtop[LIBLIB])
		|| (areawin->topinstance == xobjs.libtop[PAGELIB]))
		&& libmod >= PAGELIB)) {
      push_stack(&areawin->stack, areawin->topinstance);
   }

   /* set library as new object */

   topobject->viewscale = areawin->vscale;
   topobject->pcorner = areawin->pcorner;
   areawin->topinstance = xobjs.libtop[libmod];

   if (libmod == FONTLIB)
      setpage(false);
   else {
      setpage(true);
      transferselects();
   }

   /* draw the new screen */

   refresh(NULL, NULL, NULL);
}

/*-------------------------------------------------------------------------*/
