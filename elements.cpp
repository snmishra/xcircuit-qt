/*----------------------------------------------------------------------*/
/* elements.c --- xcircuit routines for creating basic elements		*/
/* Copyright (c) 2002  Tim Edwards, Johns Hopkins University            */
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*      written by Tim Edwards, 8/13/93                                 */
/*----------------------------------------------------------------------*/

#include <QPainter>
#include <QChar>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

#ifdef TCL_WRAPPER 
#include <tk.h>
#endif

#include "area.h"
#include "xcircuit.h"
#include "prototypes.h"
#include "xcqt.h"
#include "colors.h"
#include "area.h"

/*----------------------------------------------------------------------*/
/* Global Variable definitions                                          */
/*----------------------------------------------------------------------*/

extern Cursor   appcursors[NUM_CURSORS];
extern XCWindowData *areawin;
extern Widget   top;
extern fontinfo *fonts;
extern short fontcount;

extern double atan2();

/*------------------------------------------------------------------------*/
/* Declarations of global variables                                       */
/*------------------------------------------------------------------------*/

char extchar[20];
double saveratio;
u_char texttype;

/*--------------------------------------*/
/* Draw a dot at the current point.     */
/*--------------------------------------*/

void drawdot(int xpos, int ypos)
{
   arcptr *newarc;
   objinstptr *newdot;
   objectptr dotobj;
   
   /* Find the object "dot" in the builtin library, or else use an arc */
   
   if ((dotobj = finddot()) != (objectptr)NULL) {
      newdot = topobject->append(new objinst(dotobj, xpos, ypos));
      register_for_undo(XCF_Dot, UNDO_DONE, areawin->topinstance, *newdot);
   }
   else {
      newarc = topobject->append(new arc(xpos, ypos));
      (*newarc)->radius = 6;
      (*newarc)->yaxis = 6;
      (*newarc)->width = 1.0;
      (*newarc)->style = FILLED | FILLSOLID | NOBORDER;
      (*newarc)->calc();
      register_for_undo(XCF_Arc, UNDO_DONE, areawin->topinstance, *newarc);
   }
   incr_changes(topobject);
}

/*--------------------------------------*/
/* Button handler when creating a label */
/*--------------------------------------*/

void textbutton(u_char dopin, int x, int y)
{
   labelptr *newlabel;
   XPoint userpt;
   short tmpheight, *newselect;

   XDefineCursor(areawin->viewport, TEXTPTR);
   W3printf("Click to end or cancel.");

   if (fontcount == 0)
      Wprintf("Warning:  No fonts available!");

   unselect_all();
   snap(x, y, &userpt);
   newlabel = topobject->temp_append(new label(dopin, userpt));
   newselect = allocselect();
   *newselect = topobject->parts;

   tmpheight = (short)(TEXTHEIGHT * (*newlabel)->scale);
   userpt.y -= ((*newlabel)->justify & NOTBOTTOM) ?
	(((*newlabel)->justify & TOP) ? tmpheight : tmpheight / 2) : 0;
   areawin->update();
   areawin->origin = userpt;
   areawin->textpos = 1;  /* Text position is *after* the font declaration */
}

/*----------------------------------------------------------------------*/
/* Report on characters surrounding the current text position		*/
/*----------------------------------------------------------------------*/

#define MAXCHARS 10

void charreport(labelptr curlabel)
{
   int i, locpos;
   stringpart *strptr;

   QString rep;
   for (i = areawin->textpos - MAXCHARS; i <= areawin->textpos + MAXCHARS - 1; i++) {
      if (i < 0) continue; 
      strptr = findstringpart(i, &locpos, curlabel->string, areawin->topinstance);
      if (i == areawin->textpos) {
         rep += "| ";
      }
      if (strptr == NULL) break;
      rep += charprint(strptr, locpos);
      rep += " ";
   }
   W3printf("%ls", rep.utf16());
}

/*----------------------------------------------------------------------*/
/* See if a (pin) label has a copy (at least one) in this drawing.	*/
/*----------------------------------------------------------------------*/

labelptr findlabelcopy(labelptr curlabel, stringpart *curstring)
{
    for (labeliter tlab; topobject->values(tlab); ) {
        if (tlab->pin != LOCAL) continue;
        else if (tlab == curlabel) continue;  /* Don't count self! */
        else if (!stringcomp(tlab->string, curstring)) return tlab;
    }
    return NULL;
}

/*--------------------------------------------------------------*/
/* Interpret string and add to current label.			*/
/* 	keypressed is a KeySym					*/
/*	clientdata can pass information for label controls	*/
/*								*/
/* Return true if labeltext handled the character, false if the	*/
/* character was not recognized.				*/
/*--------------------------------------------------------------*/

bool labeltext(int keypressed, char *clientdata)
{
   labelptr curlabel;
   stringpart *curpos, *labelbuf;
   int locpos;
   bool r = true, do_redraw = false;
   short tmplength, tmpheight, cfont;
   TextExtents tmpext;

   curlabel = TOLABEL(EDITPART);

   if (curlabel == NULL || curlabel->type != LABEL || areawin->textpos <= 0) {
      Wprintf("Error:  Bad label string");
      eventmode = NORMAL_MODE;
      return false;
   }

   /* find text segment of the current position */
   curpos = findstringpart(areawin->textpos, &locpos, curlabel->string,
		areawin->topinstance);

   if (clientdata != NULL && keypressed == TEXT_DELETE) {
      if (areawin->textpos > 1) {
         int curloc, strpos;
         stringpart *strptr;

         if (areawin->textend == 0) areawin->textend = areawin->textpos - 1;

         for (strpos = areawin->textpos - 1; strpos >= areawin->textend; strpos--) {
	    strptr = findstringpart(strpos, &curloc, curlabel->string,
			 areawin->topinstance);
	    if (curloc >= 0) {
               size_t len = strlen(strptr->data.string + curloc + 1) + 1;
               memmove(strptr->data.string + curloc,
                        strptr->data.string + curloc + 1, len);
	       if (strlen(strptr->data.string) == 0)
	          deletestring(strptr, &curlabel->string, areawin->topinstance);
	    }

            /* Don't delete any parameter boundaries---must use	*/
            /* "unparameterize" command for that.		*/

	    else if (strptr != NULL) {
	       if ((strptr->type != PARAM_START) && (strptr->type != PARAM_END))
	          deletestring(strptr, &curlabel->string, areawin->topinstance);
	       else
	          areawin->textpos++;
	    }
	    else
	       Fprintf(stdout, "Error:  Unexpected NULL string part\n");
            areawin->textpos--;
         }
         areawin->textend = 0;
         do_redraw = true;
      }
   }
   else if (clientdata != NULL && keypressed == TEXT_DEL_PARAM) {
      if (areawin->textpos > 1) {
         int curloc, strpos;
         stringpart *strptr;

         strptr = findstringpart(areawin->textpos - 1, &curloc, curlabel->string,
			 areawin->topinstance);
         if ((curloc < 0) && (strptr != NULL) && (strptr->type == PARAM_END)) {
	    while (strptr->type != PARAM_START)
	       strptr = findstringpart(--strpos, &curloc, curlabel->string,
			areawin->topinstance);
	    unmakeparam(curlabel, strptr);
            do_redraw = true;
	 }
      }
   }
   else if (clientdata != NULL && keypressed == TEXT_RETURN) {
      bool hasstuff = false;   /* Check for null string */
      stringpart *tmppos;

      for (tmppos = curlabel->string; tmppos != NULL; tmppos = tmppos->nextpart) {
	 if (tmppos->type == PARAM_START) hasstuff = true;
	 else if (tmppos->type == TEXT_STRING) hasstuff = true;
      }
      XDefineCursor(areawin->viewport, DEFAULTCURSOR);

      W3printf("");

      if (hasstuff && (eventmode != ETEXT_MODE && eventmode != CATTEXT_MODE)) {
	 topobject->parts++;
         register_for_undo(XCF_Text, UNDO_MORE, areawin->topinstance,
                    curlabel);

	 incr_changes(topobject);
	 invalidate_netlist(topobject);

      }
      else if (!hasstuff && (eventmode == ETEXT_MODE)) {
         if (*(areawin->selectlist) < topobject->parts) {
	    /* Force the "delete" undo record to be a continuation of	*/
	    /* the undo series containing the edit.  That way, "undo"	*/
	    /* does not generate a label with null text.		*/

	    xobjs.undostack->idx = -xobjs.undostack->idx;
            standard_element_delete();
	 }
	 else {
            /* Label had just been created; just delete it w/o undo */
            delete curlabel;
	    topobject->parts--;
            unselect_all();
	 }
      }

      if ((!hasstuff) && (eventmode == CATTEXT_MODE)) {  /* can't have null labels! */ 
          undo_action();
	  Wprintf("Object must have a name!");
	  eventmode = CATALOG_MODE;
      }
      else if (!hasstuff) {
	  eventmode = NORMAL_MODE;
      }
      else if (eventmode == CATTEXT_MODE) {
	 objectptr libobj;
	 stringpart *oldname;
	 int page, libnum;

	 /* Get the library object whose name matches the original string */
	 oldname = get_original_string(curlabel);
         if ((libobj = NameToObject(oldname->nextpart->data.string, NULL, false))
			!= NULL) {

            /* Set name of object to new string.  Don't overwrite the	*/
	    /* object's technology *unless* the new string has a	*/
	    /* namespace, in which case the object's technology gets	*/
	    /* changed.							*/

	    char *techptr, *libobjname = libobj->name;
	    if ((techptr = strstr(libobjname, "::")) != NULL &&
			(strstr(curlabel->string->nextpart->data.string, "::")
			== NULL))
	       libobjname = techptr + 2;
	    strcpy(libobjname, curlabel->string->nextpart->data.string);

	    /* If checkname() alters the name, it has to be copied back to */
	    /* the catalog label for the object.			   */

            if (checkname(libobj)) {
	       curlabel->string->nextpart->data.string = (char *)realloc(
			curlabel->string->nextpart->data.string,
		  	(strlen(libobj->name) + 1) * sizeof(char));
               strcpy(curlabel->string->nextpart->data.string, libobj->name);
	    }
	    AddObjectTechnology(libobj);
	 }

	 /* Check if we altered a page name */
	 else if ((libobj = NameToPageObject(oldname->nextpart->data.string,
		NULL, &page)) != NULL) {
	    strcpy(libobj->name, curlabel->string->nextpart->data.string);
	    renamepage(page);
	 }

	 /* Check if we altered a library name */
	 else if ((libnum = NameToLibrary(oldname->nextpart->data.string)) != -1) {
	    libobj = xobjs.libtop[libnum + LIBRARY]->thisobject;
	    strcpy(libobj->name, curlabel->string->nextpart->data.string);
	 }
 	 else {
	    Wprintf("Error:  Cannot match name to any object, page, or library!");
	    refresh(NULL, NULL, NULL);
         }

         eventmode = CATALOG_MODE;
      }
      else {	/* (hasstuff && eventmode != CATTEXT_MODE) */
	 eventmode = NORMAL_MODE;
	 incr_changes(topobject);
	 if (curlabel->pin != false) invalidate_netlist(topobject);
      }

      setdefaultfontmarks();
      setcolormark(areawin->color);
      if ((labelbuf = get_original_string(curlabel)) != NULL) {

	 /* If the original label (before modification) is a pin in a	*/
	 /* schematic/symbol with a matching symbol/schematic, and the	*/
	 /* name is unique, change every pin in the matching symbol/	*/
	 /* schematic to match the new text.				*/

	 if ((curlabel->pin == LOCAL) && (topobject->symschem != NULL) &&
			(topobject->symschem->schemtype != PRIMARY)) {
	    if ((findlabelcopy(curlabel, labelbuf) == NULL)
			&& (findlabelcopy(curlabel, curlabel->string) == NULL)) {
	       if (changeotherpins(curlabel, labelbuf) > 0) {
	          if (topobject->schemtype == PRIMARY ||
				topobject->schemtype == SECONDARY)
	             Wprintf("Changed corresponding pin in associated symbol");
	          else
	             Wprintf("Changed corresponding pin in associated schematic");
	          incr_changes(topobject->symschem);
	          invalidate_netlist(topobject->symschem);
	       }
	    }
	 }
	
	 resolveparams(areawin->topinstance);
         updateinstparam(topobject);
	 setobjecttype(topobject);
      }
      else
         calcbbox(areawin->topinstance);

      unselect_all();
      return r;
   }
   else if (clientdata != NULL && keypressed == TEXT_RIGHT) {
      if (curpos != NULL) areawin->textpos++;
   }
   else if (clientdata != NULL && keypressed == TEXT_LEFT) {
      if (areawin->textpos > 1) areawin->textpos--;
   }
   else if (clientdata != NULL && keypressed == TEXT_DOWN) {
      while (curpos != NULL) {
	 areawin->textpos++;
	 curpos = findstringpart(areawin->textpos, &locpos, curlabel->string,
			areawin->topinstance);
	 if (curpos != NULL)
	    if (curpos->type == RETURN)
	       break;
      }
   }
   else if (clientdata != NULL && keypressed == TEXT_UP) {
      while (areawin->textpos > 1) {
	 areawin->textpos--;
	 curpos = findstringpart(areawin->textpos, &locpos, curlabel->string,
	 		areawin->topinstance);
	 if (curpos->type == RETURN) {
	    if (areawin->textpos > 1) areawin->textpos--;
	    break;
	 }
      }
   }
   else if (clientdata != NULL && keypressed == TEXT_HOME)
      areawin->textpos = 1;
   else if (clientdata != NULL && keypressed == TEXT_END)
      areawin->textpos = stringlength(curlabel->string, true, areawin->topinstance);
   else if (clientdata != NULL && keypressed == TEXT_SPLIT) {
      labelptr *newlabel;
      XPoint points[4], points1[4], points2[4];

      /* Everything after the cursor gets dumped into a new label */

      if ((areawin->textpos > 1) && (curpos != NULL)) {
         curlabel->getBbox(points, areawin->topinstance);
         newlabel = topobject->append(new label(curlabel->pin, curlabel->position));
         if (locpos > 0)
            curpos = splitstring(areawin->textpos, &curlabel->string,
			areawin->topinstance);
	 /* move back one position to find end of top part of string */
         curpos = splitstring(areawin->textpos - 1, &curlabel->string,
			areawin->topinstance);
	 if (curpos->nextpart->type == FONT_NAME) {
	    freelabel((*newlabel)->string);
	    (*newlabel)->string = curpos->nextpart;
	 }
	 else {
	    (*newlabel)->string->data.font = curlabel->string->data.font;
	    (*newlabel)->string->nextpart = curpos->nextpart;
	 }
         curpos->nextpart = NULL;

	 /* Adjust position of both labels to retain their original	*/
	 /* relative positions.						*/

         curlabel->getBbox(points1, areawin->topinstance);
         (*newlabel)->getBbox(points2, areawin->topinstance);
         curlabel->position += points[1] - points1[1];
         (*newlabel)->position += points[3] - points2[3];

         do_redraw = true;
      }
   }

   /* Write a font designator or other control into the string */

   else if (clientdata != NULL) {
      oparamptr ops;
      stringpart *newpart;
      bool errcond = false;

      /* erase first before redrawing unless the string is empty */
      if (locpos > 0) {
         curpos = splitstring(areawin->textpos, &curlabel->string,
			areawin->topinstance);
	 curpos = curpos->nextpart;
      }
      newpart = makesegment(&curlabel->string, curpos);
      newpart->type = keypressed;
      switch (keypressed) {
	 case FONT_SCALE:
	    newpart->data.scale = *((float *)clientdata);
	    break;
	 case KERN:
	    newpart->data.kern[0] = *((short *)clientdata);
	    newpart->data.kern[1] = *((short *)clientdata + 1);
	    break;
	 case FONT_COLOR:
	    newpart->data.color = *((int *)clientdata);
            if (newpart->data.color >= colorlist.count()) errcond = true;
	    break;
	 case FONT_NAME:
	    newpart->data.font = *((int *)clientdata);
	    if (newpart->data.font >= fontcount) errcond = true;
	    break;
	 case PARAM_START:
	    newpart->data.string = (char *)malloc(1 + strlen(clientdata));
	    strcpy(newpart->data.string, clientdata);
	    ops = match_param(topobject, clientdata);
	    if (ops == NULL) errcond = true;
	    else {
	       /* Forward edit cursor position to the end of the parameter */
	       do {
	          areawin->textpos++;
	          curpos = findstringpart(areawin->textpos, &locpos, curlabel->string,
	 		areawin->topinstance);
	       } while (curpos->type != PARAM_END);
	    }
	    break;
      }
      if (errcond) {
	 Wprintf("Error in insertion.  Ignoring.");
	 deletestring(newpart, &curlabel->string, areawin->topinstance);
         r = false;
      }
      else {
         areawin->textpos++;
      }
      do_redraw = true;
   }

   /* Append the character to the string.  If the current label segment is	*/
   /* not text, then create a text segment for it.				*/

   else if (keypressed >= 32 && keypressed <= 0xFFFF) {
      keypressed = QChar((ushort)keypressed).toLatin1();
      qDebug("appending %c", (char)keypressed);
      Q_ASSERT(keypressed != 0);
      stringpart *lastpos;

      /* Current position is not in a text string */
      if (locpos < 0) {

         /* Find part of string which is immediately in front of areawin->textpos */
         lastpos = findstringpart(areawin->textpos - 1, &locpos, curlabel->string,
		areawin->topinstance);

	 /* No text on either side to attach to: make a new text segment */
	 if (locpos < 0) {
	    curpos = makesegment(&curlabel->string, curpos);
	    curpos->type = TEXT_STRING;
            curpos->data.string = (char*)malloc(2);
	    curpos->data.string[0] = keypressed;
	    curpos->data.string[1] = '\0';
	 }
	 else {		/* append to end of lastpos text string */
	    int slen = strlen(lastpos->data.string);
            lastpos->data.string = (char*)realloc(lastpos->data.string,
		2 +  slen);
	    *(lastpos->data.string + slen) = keypressed;
	    *(lastpos->data.string + slen + 1) = '\0';
	 }
      }
      else {	/* prepend to end of curpos text string */
         curpos->data.string = (char*)realloc(curpos->data.string,
	     2 + strlen(curpos->data.string));
	 memmove(curpos->data.string + locpos + 1, curpos->data.string + locpos,
		strlen(curpos->data.string + locpos) + 1);
         *(curpos->data.string + locpos) = keypressed;
      }
      areawin->textpos++;	/* move forward to next text position */
      do_redraw = true;
      r = true;
   }

   areawin->update();

   if (r || do_redraw) {

      /* Report on characters at the cursor position in the message window */

      charreport(curlabel);

      /* find current font and adjust menubuttons as necessary */

      cfont = findcurfont(areawin->textpos, curlabel->string, areawin->topinstance);
      if (cfont < 0) {
         Wprintf("Error:  Illegal label string");
         return r;
      }
      else
         setfontmarks(cfont, -1);

      areawin->textend = 0;
   }
   return r;
}

/*-------------------------------------*/
/* Initiate return from text edit mode */
/*-------------------------------------*/

void textreturn()
{
   labeltext(TEXT_RETURN, (char *)1);
}

/*-------------------------------------*/
/* Change the justification of a label */
/*-------------------------------------*/

void rejustify(short mode)
{
   labelptr curlabel = NULL;
   short    *tsel;
   short jsave;
   bool preselected = false, changed = false;
   static short transjust[] = {15, 13, 12, 7, 5, 4, 3, 1, 0};

   if (eventmode == TEXT_MODE || eventmode == ETEXT_MODE) {
      curlabel = TOLABEL(EDITPART);
      jsave = curlabel->justify;
      curlabel->justify = transjust[mode] |
		(curlabel->justify & NONJUSTFIELD);
      if (jsave != curlabel->justify) {
	 register_for_undo(XCF_Justify, UNDO_MORE, areawin->topinstance,
                        curlabel, (int)jsave);
	 changed = true;
      }

      setfontmarks(-1, curlabel->justify);
   }
   else {
      if (areawin->selects == 0) {
	 if (!checkselect(LABEL))
	    return;
      }
      else preselected = true;

      for (tsel = areawin->selectlist; tsel < areawin->selectlist +
	      areawin->selects; tsel++) {
	 if (SELECTTYPE(tsel) == LABEL) {
	    curlabel = SELTOLABEL(tsel);
            jsave = curlabel->justify;
      	    curlabel->justify = transjust[mode] |
		(curlabel->justify & NONJUSTFIELD);
            if (jsave != curlabel->justify) {
	       register_for_undo(XCF_Justify, UNDO_MORE, areawin->topinstance,
                        curlabel, (int)jsave);
	       changed = true;
	    }
	 }
      }
      if (preselected == false && eventmode != MOVE_MODE && eventmode != COPY_MODE)
	 unselect_all();
#if 0
      else
	 draw_all_selected();
      /// \todo side effects of draw_all_selected?
#endif
   }
   if (curlabel == NULL)
      Wprintf("No labels chosen to rejustify");
   else if (changed) {
      pwriteback(areawin->topinstance);
      calcbbox(areawin->topinstance);
      incr_changes(topobject);
   }
}

/*-------------------------*/
/* Start drawing a spline. */
/*-------------------------*/

void splinebutton(int x, int y)
{
   splineptr *newspline;
   XPoint userpt;
   short *newselect;

   unselect_all();
   snap(x, y, &userpt);
   newspline = topobject->temp_append(new spline(userpt.x, userpt.y));
   newselect = allocselect();
   *newselect = topobject->parts;

   addcycle((genericptr *)newspline, 3, 0);
   makerefcycle((*newspline)->cycle, 3);

   xcAddEventHandler(areawin->viewport, PointerMotionMask, false,
        (XtEventHandler)trackelement, NULL);
   areawin->update();

   eventmode = SPLINE_MODE;
}

/*----------------------------------------------------------------------*/
/* Generate cycles on a path where endpoints meet, so that the path	*/
/* remains connected during an edit.  If the last point on any part	*/
/* of the path is a cycle, then the first point on the next part of	*/
/* the path should also be a cycle, with the same flags.		*/
/*----------------------------------------------------------------------*/

void updatepath(pathptr thepath)
{
   genericptr *ggen, *ngen;
   short locparts, cycle;
   pointselect *cptr;
   polyptr thispoly;
   splineptr thisspline;

   for (polyiter poly; thepath->values(poly); ) {
       findconstrained(poly);
       break;
   }

   locparts = (thepath->style & UNCLOSED) ? thepath->parts - 1 : thepath->parts;
   for (ggen = thepath->begin(); ggen != thepath->begin() + locparts; ggen++) {
      ngen = (ggen == thepath->end() - 1) ? thepath->begin() : ggen + 1;

      switch (ELEMENTTYPE(*ggen)) {
         case POLYGON:
            thispoly = TOPOLY(ggen);
            if (thispoly->cycle == NULL) continue;
            cycle = thispoly->points.count() - 1;
            for (cptr = thispoly->cycle;; cptr++) {
               if (cptr->number == cycle) break;
               if (cptr->flags & LASTENTRY) break;
            }
            if (cptr->number != cycle) continue;
            break;
         case SPLINE:
            thisspline = TOSPLINE(ggen);
            if (thisspline->cycle == NULL) continue;
            cycle = 3;
            for (cptr = thisspline->cycle;; cptr++) {
               if (cptr->number == cycle) break;
               if (cptr->flags & LASTENTRY) break;
            }
            if (cptr->number != cycle) continue;
            break;
      }
      addcycle(ngen, 0, cptr->flags & (EDITX | EDITY));
      switch (ELEMENTTYPE(*ngen)) {
         case POLYGON:
            findconstrained(TOPOLY(ngen));
            break;
      }
   }

   /* Do the same thing in the other direction */
   locparts = (thepath->style & UNCLOSED) ? 1 : 0;
   for (ggen = thepath->end() - 1; ggen >= thepath->begin() + locparts;
                ggen--) {
      ngen = (ggen == thepath->begin()) ? thepath->end() - 1 : ggen - 1;

      switch (ELEMENTTYPE(*ggen)) {
         case POLYGON:
            thispoly = TOPOLY(ggen);
            if (thispoly->cycle == NULL) continue;
            cycle = 0;
            for (cptr = thispoly->cycle;; cptr++) {
               if (cptr->number == cycle) break;
               if (cptr->flags & LASTENTRY) break;
            }
            if (cptr->number != cycle) continue;
            break;
         case SPLINE:
            thisspline = TOSPLINE(ggen);
            if (thisspline->cycle == NULL) continue;
            cycle = 0;
            for (cptr = thisspline->cycle;; cptr++) {
               if (cptr->number == cycle) break;
               if (cptr->flags & LASTENTRY) break;
            }
            if (cptr->number != cycle) continue;
            break;
      }
      switch (ELEMENTTYPE(*ngen)) {
         case POLYGON:
            addcycle(ngen, TOPOLY(ngen)->points.count() - 1, cptr->flags & (EDITX | EDITY));
            break;
         case SPLINE:
            addcycle(ngen, 3, cptr->flags & (EDITX | EDITY));
            break;
      }
   }
}

/*-------------------------------------*/
/* Button handler when creating an arc */
/*-------------------------------------*/

void arcbutton(int x, int y)
{
   arcptr *newarc;
   XPoint userpt;
   short *newselect;

   unselect_all();
   snap(x, y, &userpt);
   newarc = topobject->temp_append(new arc(userpt.x, userpt.y));
   newselect = allocselect();
   *newselect = topobject->parts;
   saveratio = 1.0;
   addcycle((genericptr *)newarc, 0, 0);

   xcAddEventHandler(areawin->viewport, PointerMotionMask, false,
        (XtEventHandler)trackarc, NULL);

   eventmode = ARC_MODE;
   areawin->update();
}

/*----------------------------------*/
/* Track an arc during mouse motion */
/*----------------------------------*/

void trackarc(Widget w, caddr_t clientdata, caddr_t calldata)
{
   XPoint newpos;
   arcptr newarc;
   double adjrat;
   short cycle;

   newarc = TOARC(EDITPART);

   newpos = UGetCursorPos();
   u2u_snap(newpos);
   if (areawin->save == newpos) return;

   cycle = (newarc->cycle == NULL) ? -1 : newarc->cycle->number;
   if (cycle == 1 || cycle == 2) {
      float *angleptr, tmpang;

      adjrat = (newarc->yaxis == 0) ? 1 :
		(double)(abs(newarc->radius)) / (double)newarc->yaxis;
      angleptr = (cycle == 1) ? &newarc->angle1 : &newarc->angle2;
      tmpang = (float)(atan2((double)(newpos.y - newarc->position.y) * adjrat,
	   (double)(newpos.x - newarc->position.x)) / RADFAC);
      if (cycle == 1) {
	 if (tmpang > newarc->angle2) tmpang -= 360;
	 else if (newarc->angle2 - tmpang > 360) newarc->angle2 -= 360;
      }
      else {
         if (tmpang < newarc->angle1) tmpang += 360;
	 else if (tmpang - newarc->angle1 > 360) newarc->angle1 += 360;
      }
      *angleptr = tmpang;

      if (newarc->angle2 <= 0) {
	 newarc->angle2 += 360;
	 newarc->angle1 += 360;
      }
      if (newarc->angle2 <= newarc->angle1)
	 newarc->angle1 -= 360;
   }
   else if (cycle == 0) {
      short direc = (newarc->radius < 0);
      newarc->radius = wirelength(&newpos, &(newarc->position));
      newarc->yaxis = (short)((double)newarc->radius * saveratio);
      if (direc) newarc->radius = -newarc->radius;
   }
   else {
      newarc->yaxis = wirelength(&newpos, &(newarc->position));
      saveratio = (double)newarc->yaxis / (double)newarc->radius;
   }

   newarc->calc();
   printpos(newpos);

   areawin->save = newpos;
   areawin->update();
}

/*------------------------------------*/
/* Button handler when creating a box */
/*------------------------------------*/

void boxbutton(int x, int y)
{
   polyptr *newbox;
   XPoint userpt;
   short *newselect;

   unselect_all();
   snap(x, y, &userpt);
   newbox = topobject->temp_append(new polygon(4, userpt.x, userpt.y));
   newselect = allocselect();
   *newselect = topobject->parts;

   xcAddEventHandler(areawin->viewport, PointerMotionMask, false,
        (XtEventHandler)trackbox, NULL);

   eventmode = BOX_MODE;
   areawin->update();
}

/*---------------------------------*/
/* Track a box during mouse motion */
/*---------------------------------*/

void trackbox(Widget, caddr_t, caddr_t)
{
   XPoint 	newpos;
   polyptr      newbox;

   newbox = TOPOLY(EDITPART);
   newpos = UGetCursorPos();
   u2u_snap(newpos);

   if (areawin->save == newpos) return;

   newbox->points[1].y = newpos.y;
   newbox->points[2] = newpos;
   newbox->points[3].x = newpos.x;

   printpos(newpos);

   areawin->save = newpos;
   areawin->update();
}

/*----------------------------------------------------------------------*/
/* Track a wire during mouse motion					*/
/* Note:  The manhattanize algorithm will change the effective cursor	*/
/* position to keep the wire manhattan if the wire is only 1 segment.	*/
/* It will change the previous point's position if the wire has more	*/
/* than one segment.  They are called at different times to ensure the	*/
/* wire redraw is correct.						*/
/*----------------------------------------------------------------------*/

void trackwire(Widget w, caddr_t clientdata, caddr_t calldata)
{
   XPoint newpos, upos, *tpoint;
   polyptr	newwire;

   newwire = TOPOLY(EDITPART);

   if (areawin->attachto >= 0) {
      upos = UGetCursorPos();
      findattach(&newpos, NULL, &upos); 
   }
   else {
      newpos = UGetCursorPos();
      u2u_snap(newpos);
      if (areawin->manhatn && (newwire->points.count() == 2))
         manhattanize(&newpos, newwire, -1, true);
   }

   if (areawin->save != newpos) {
      tpoint = newwire->points.end() - 1;
      if (areawin->manhatn && (newwire->points.count() > 2))
         manhattanize(&newpos, newwire, -1, true);
      *tpoint = newpos;
      areawin->save = newpos;
      printpos(newpos);
      areawin->update();
   }
}

/*--------------------------*/
/* Start drawing a polygon. */
/*--------------------------*/

void startwire(XPoint *userpt)
{
   polyptr *newwire;
   short *newselect;

   unselect_all();
   newwire = topobject->temp_append(new polygon);
   newselect = allocselect();
   *newselect = topobject->parts;

   /* always start unfilled, unclosed; can fix on next button-push. */

   (*newwire)->style = UNCLOSED | (areawin->style & (DASHED | DOTTED));
   (*newwire)->color = areawin->color;
   (*newwire)->width = areawin->linewidth;
   (*newwire)->points.resize(2);
   (*newwire)->points[0] = (*newwire)->points[1] = areawin->save = *userpt;

   xcAddEventHandler(areawin->viewport, PointerMotionMask, false,
          (XtEventHandler)trackwire, NULL);
   areawin->update();
}

/*--------------------------------------------------------------*/
/* Find which points should track along with the edit point in	*/
/* in polygon RHOMBOID or MANHATTAN edit modes.		    	*/
/* (point number is stored in lastpoly->cycle)	    	    	*/
/*								*/
/* NOTE:  This routine assumes that either the points have just	*/
/* been selected, or that advancecycle() has been called to	*/
/* remove all previously recorded tracking points.		*/
/*--------------------------------------------------------------*/

void findconstrained(polyptr lastpoly)
{
   XPoint *savept, *npt, *lpt;
   short cycle;
   short lflags, nflags;
   short lcyc, ncyc;
   pointselect *cptr, *nptr;

   if (areawin->boxedit == NORMAL) return;

   if (lastpoly->cycle == NULL) return;

   /* Set "process" flags on all original points */
   for (cptr = lastpoly->cycle;; cptr++) {
      cptr->flags |= PROCESS;
      if (cptr->flags & LASTENTRY) break;
   }

   cptr = lastpoly->cycle;
   while (1) {
      if (cptr->flags & PROCESS) {
         cptr->flags &= ~PROCESS;
         cycle = cptr->number;
         savept = lastpoly->points + cycle;

         /* find points before and after the edit point */

         lcyc = (cycle == 0) ? ((lastpoly->style & UNCLOSED) ?
                -1 : lastpoly->points.count() - 1) : cycle - 1;
         ncyc = (cycle == lastpoly->points.count() - 1) ?
                ((lastpoly->style & UNCLOSED) ? -1 : 0) : cycle + 1;

         lpt = (lcyc == -1) ? NULL : lastpoly->points + lcyc;
         npt = (ncyc == -1) ? NULL : lastpoly->points + ncyc;

         lflags = nflags = NONE;

         /* two-point polygons (lines) are a degenerate case in RHOMBOID edit mode */

         if (areawin->boxedit != MANHATTAN && lastpoly->points.count() <= 2) return;

         /* This is complicated but logical:  in MANHATTAN mode, boxes maintain */
         /* box shape.  In RHOMBOID modes, parallelagrams maintain shape.  The  */
         /* "savedir" variable determines which coordinate(s) of which point(s) */
         /* should track along with the edit point.			     */

         if (areawin->boxedit != RHOMBOIDY) {
            if (lpt != NULL) {
               if (lpt->y == savept->y) {
                  lflags |= EDITY;
                  if (areawin->boxedit == RHOMBOIDX && lpt->x != savept->x)
                     lflags |= EDITX;
                  else if (areawin->boxedit == RHOMBOIDA && npt != NULL) {
                     if (npt->y != savept->y) nflags |= EDITX;
                  }
               }
            }
            if (npt != NULL) {
               if (npt->y == savept->y) {
                  nflags |= EDITY;
                  if (areawin->boxedit == RHOMBOIDX && npt->x != savept->x)
                     nflags |= EDITX;
                  else if (areawin->boxedit == RHOMBOIDA && lpt != NULL) {
                     if (lpt->y != savept->y) lflags |= EDITX;
                  }
               }
            }
         }
         if (areawin->boxedit != RHOMBOIDX) {
            if (lpt != NULL) {
               if (lpt->x == savept->x) {
                  lflags |= EDITX;
                  if (areawin->boxedit == RHOMBOIDY && lpt->y != savept->y)
                     lflags |= EDITY;
                  else if (areawin->boxedit == RHOMBOIDA && npt != NULL) {
                     if (npt->x != savept->x) nflags |= EDITY;
                  }
               }
            }
            if (npt != NULL) {
               if (npt->x == savept->x) {
                  nflags |= EDITX;
                  if (areawin->boxedit == RHOMBOIDY && npt->y != savept->y)
                     nflags |= EDITY;
                  else if (areawin->boxedit == RHOMBOIDA && lpt != NULL) {
                     if (lpt->x != savept->x) lflags |= EDITY;
                  }
               }
            }
         }
         nptr = cptr + 1;
         if (lpt != NULL && lflags != 0) {
            addcycle((genericptr *)(&lastpoly), lcyc, lflags);
            cptr = nptr = lastpoly->cycle;
         }
         if (npt != NULL && nflags != 0) {
            addcycle((genericptr *)(&lastpoly), ncyc, nflags);
            cptr = nptr = lastpoly->cycle;
         }
      }
      else
         nptr = cptr + 1;
      if (cptr->flags & LASTENTRY) break;
      cptr = nptr;
   }
}

/*------------------------------------------------------*/
/* Track movement of poly, spline, or polygon segments	*/
/* during edit mode					*/
/*------------------------------------------------------*/

void trackelement(Widget, caddr_t, caddr_t)
{
   XPoint newpos, origpt, *curpt;
   short	*selobj;
   pointselect	*cptr;
   XPoint delta;

   newpos = UGetCursorPos();
   u2u_snap(newpos);

   /* force attachment if required */
   if (areawin->attachto >= 0) {
      XPoint apos;
      findattach(&apos, NULL, &newpos);
      newpos = apos;
   }

   if (areawin->save.x == newpos.x && areawin->save.y == newpos.y) return;

   /* Find the reference point */

   cptr = getrefpoint(TOGENERIC(EDITPART), &curpt);
   switch(ELEMENTTYPE(TOGENERIC(EDITPART))) {
      case POLYGON:
         if (cptr == NULL)
            curpt = TOPOLY(EDITPART)->points.begin();
         break;
      case SPLINE:
         if (cptr == NULL)
            curpt = &(TOSPLINE(EDITPART)->ctrl[0]);
         break;
      case ARC:
         curpt = &(TOARC(EDITPART)->position);
         break;
      case OBJINST:
         curpt = &(TOOBJINST(EDITPART)->position);
         break;
      case GRAPHIC:
         curpt = &(TOGRAPHIC(EDITPART)->position);
         break;
   }
   origpt = *curpt;
   delta = newpos - *curpt;

   /* Now adjust all edited elements relative to the reference point */

   for (selobj = areawin->selectlist; selobj < areawin->selectlist +
                areawin->selects; selobj++)
   {
      editpoints(SELTOGENERICPTR(selobj), delta);
   }

   printpos(newpos);
   areawin->save = newpos;
   areawin->update();
}

/*-------------------------------------------------*/
/* Determine values of endpoints of an element	   */
/*-------------------------------------------------*/

void setendpoint(short *scnt, short direc, XPoint **endpoint, XPoint *arcpoint)
{
   genericptr *sptr = topobject->begin() + (*scnt);

   switch(ELEMENTTYPE(*sptr)) {
      case POLYGON:
	 if (direc)
            *endpoint = TOPOLY(sptr)->points.end() - 1;
	 else
            *endpoint = TOPOLY(sptr)->points.begin();
	 break;
      case SPLINE:
	 if (direc)
	    *endpoint = &(TOSPLINE(sptr)->ctrl[3]);
	 else
	    *endpoint = &(TOSPLINE(sptr)->ctrl[0]);
	 break;
      case ARC:
	 if (direc) {
	    arcpoint->x = (short)(TOARC(sptr)->points[TOARC(sptr)->number - 1].x
		+ 0.5);
	    arcpoint->y = (short)(TOARC(sptr)->points[TOARC(sptr)->number - 1].y
		+ 0.5);
	 }
	 else {
	    arcpoint->x = (short)(TOARC(sptr)->points[0].x + 0.5);
	    arcpoint->y = (short)(TOARC(sptr)->points[0].y + 0.5);
	 }
	 *endpoint = arcpoint;
	 break;
   }
}

/*------------------------------------------------------------*/
/* Same as above for floating-point positions		      */
/*------------------------------------------------------------*/

void reversefpoints(XfPoint *plist, short number)
{
   XfPoint *ppt;
   XfPoint *pend = plist + number - 1;
   short hnum = number >> 1;

   for (ppt = plist; ppt < plist + hnum; ppt++, pend--) {
      std::swap(*ppt, *pend);
   }
}

/*--------------------------------------------------------------*/
/* Permanently remove an element from the topobject plist	*/
/*	add = 1 if plist has (parts + 1) elements		*/
/*--------------------------------------------------------------*/

static void freepathparts(short *selectobj, short add)
{
   delete topobject->at(*selectobj);
   removep(selectobj, add);
}

/*--------------------------------------------------------------*/
/* Remove a part from an object					*/
/* 	add = 1 if plist has (parts + 1) elements		*/
/*--------------------------------------------------------------*/

void removep(short *selectobj, short add)
{
   genericptr *oldelem = topobject->begin() + (*selectobj);

   for (++oldelem; oldelem < topobject->end() + add; oldelem++)
	    *(oldelem - 1) = *oldelem;

   topobject->parts--;
}

/*-------------------------------------------------*/
/* Break a path into its constituent components	   */
/*-------------------------------------------------*/

void unjoin()
{
   short *selectobj;
   genericptr *genp, *newg;
   pathptr oldpath;
   polyptr oldpoly, *newpoly;
   bool preselected;
   short i, cycle;

   if (areawin->selects == 0) {
      select_element(PATH | POLYGON);
      preselected = false;
   }
   else preselected = true;

   if (areawin->selects == 0) {
      Wprintf("No objects selected.");
      return;
   }

   /* for each selected path or polygon */

   for (selectobj = areawin->selectlist; selectobj < areawin->selectlist
        + areawin->selects; selectobj++) {
      if (SELECTTYPE(selectobj) == PATH) {
         oldpath = SELTOPATH(selectobj);
      
         /* move components to the top level */

         while (oldpath->parts) {
             topobject->append(oldpath->take_last());
         }

         /* remove the path object and revise the selectlist */

         freepathparts(selectobj, 0);
         reviseselect(areawin->selectlist, areawin->selects, selectobj);
      }
      else if (SELECTTYPE(selectobj) == POLYGON) {
	 /* Method to break a polygon, in lieu of the edit-mode	*/
	 /* polygon break that was removed.			*/
         oldpoly = SELTOPOLY(selectobj);

	 /* Get the point nearest the cursor, and break at that point */
         cycle = closepoint(oldpoly, &areawin->save);
         if (cycle > 0 && cycle < (oldpoly->points.count() - 1)) {
            newpoly = topobject->append(new polygon(*oldpoly));
            oldpoly->points.resize(cycle+1);
            std::copy((*newpoly)->points.begin()+cycle, (*newpoly)->points.end(), (*newpoly)->points.begin());
            (*newpoly)->points.resize((*newpoly)->points.count() - cycle);
	 }
      }
   }
   if (!preselected) clearselects();
   areawin->update();
}

void unjoin_call(QAction*, void*, void*)
{
    unjoin();
}

/*-------------------------------------------------*/
/* Test if two points are near each other	   */
/*-------------------------------------------------*/

bool neartest(XPoint *point1, XPoint *point2)
{
   short diff[2];

   diff[0] = point1->x - point2->x;
   diff[1] = point1->y - point2->y;
   diff[0] = abs(diff[0]);
   diff[1] = abs(diff[1]);

   if (diff[0] <= 2 && diff[1] <= 2) return true;
   else return false;
}


/*-------------------------------------------------*/
/* Join stuff together 			   	   */
/*-------------------------------------------------*/

void join()
{
   short     *selectobj;
   polyptr   *newpoly, nextwire;
   pathptr   *newpath;
   short     *scount, *sptr, *sptr2, *direc, *order;
   short     ordered, startpt = 0;
   short     numpolys, numlabels, numpoints, polytype;
   int	     polycolor;
   float     polywidth;
   XPoint    *testpoint, *testpoint2, *begpoint, *endpoint, arcpoint[4];
   XPoint    *begpoint2, *endpoint2;
   bool   allpolys = true;
   objectptr delobj;

   numpolys = numlabels = 0;
   for (selectobj = areawin->selectlist; selectobj < areawin->selectlist
	+ areawin->selects; selectobj++) {
      if (SELECTTYPE(selectobj) == POLYGON) {
	 /* arbitrary:  keep style of last polygon in selectlist */
	 polytype = SELTOPOLY(selectobj)->style;
	 polywidth = SELTOPOLY(selectobj)->width;
	 polycolor = SELTOPOLY(selectobj)->color;
	 numpolys++;
      }
      else if (SELECTTYPE(selectobj) == SPLINE) {
	 polytype = SELTOSPLINE(selectobj)->style;
	 polywidth = SELTOSPLINE(selectobj)->width;
	 polycolor = SELTOSPLINE(selectobj)->color;
	 numpolys++;
	 allpolys = false;
      }
      else if (SELECTTYPE(selectobj) == ARC) {
	 polytype = SELTOARC(selectobj)->style;
	 polywidth = SELTOARC(selectobj)->width;
	 polycolor = SELTOARC(selectobj)->color;
	 numpolys++;
	 allpolys = false;
      }
      else if (SELECTTYPE(selectobj) == LABEL)
	 numlabels++;
   }
   if ((numpolys == 0) && (numlabels == 0)) {
      Wprintf("No elements selected for joining.");
      return;
   }
   else if ((numpolys == 1) || (numlabels == 1)) {
      Wprintf("Only one element: nothing to join to.");
      return;
   }
   else if ((numpolys > 1) && (numlabels > 1)) {
      Wprintf("Selection mixes labels and line segments.  Ignoring.");
      return;
   }
   else if (numlabels > 0) {
      joinlabels();
      return;
   }

   /* scount is a table of element numbers 				*/
   /* order is an ordered table of end-to-end elements 			*/
   /* direc is an ordered table of path directions (0=same as element,	*/
   /* 	1=reverse from element definition)				*/

   scount = new short[numpolys];
   order  = new short[numpolys];
   direc  = new short[numpolys];
   sptr = scount;
   numpoints = 1;

   /* make a record of the element instances involved */

   for (selectobj = areawin->selectlist; selectobj < areawin->selectlist
	+ areawin->selects; selectobj++) {
      if (SELECTTYPE(selectobj) == POLYGON) {
          numpoints += SELTOPOLY(selectobj)->points.count() - 1;
	  *(sptr++) = *selectobj;
      }
      else if (SELECTTYPE(selectobj) == SPLINE || SELECTTYPE(selectobj) == ARC)
	  *(sptr++) = *selectobj;
   }

   /* Sort the elements by sorting the scount array: 				*/
   /* Loop through each point as starting point in case of strangely connected 	*/
   /* structures. . . for normal structures it should break out on the first   	*/
   /* loop (startpt = 0).							*/

   for (startpt = 0; startpt < numpolys; startpt++) {

      /* set first in ordered list */

      direc[0] = 0;
      order[0] = *(scount + startpt);

      for (ordered = 0; ordered < numpolys - 1; ordered++) {

         setendpoint(order + ordered, (1 ^ direc[ordered]), &endpoint2, &arcpoint[0]);
         setendpoint(order, (0 ^ direc[0]), &begpoint2, &arcpoint[1]);

         for (sptr = scount; sptr < scount + numpolys; sptr++) {

	    /* don't compare with things already in the list */
	    for (sptr2 = order; sptr2 <= order + ordered; sptr2++)
	       if (*sptr == *sptr2) break;
	    if (sptr2 != order + ordered + 1) continue;

            setendpoint(sptr, 0, &begpoint, &arcpoint[2]);
            setendpoint(sptr, 1, &endpoint, &arcpoint[3]);

	    /* four cases of matching endpoint of one element to another */

	    if (neartest(begpoint, endpoint2)) {
	       order[ordered + 1] = *sptr;
	       direc[ordered + 1] = 0;
	       break;
	    }
	    else if (neartest(endpoint, endpoint2)) {
	       order[ordered + 1] = *sptr;
	       direc[ordered + 1] = 1;
	       break;
	    }
	    else if (neartest(begpoint, begpoint2)) {
	       for (sptr2 = order + ordered + 1; sptr2 > order; sptr2--)
	          *sptr2 = *(sptr2 - 1);
	       for (sptr2 = direc + ordered + 1; sptr2 > direc; sptr2--)
	          *sptr2 = *(sptr2 - 1);
	       order[0] = *sptr;
	       direc[0] = 1;
	       break;
	    }
	    else if (neartest(endpoint, begpoint2)) {
	       for (sptr2 = order + ordered + 1; sptr2 > order; sptr2--) 
	          *sptr2 = *(sptr2 - 1);
	       for (sptr2 = direc + ordered + 1; sptr2 > direc; sptr2--)
	          *sptr2 = *(sptr2 - 1);
	       order[0] = *sptr;
	       direc[0] = 0;
	       break;
	    }
         }
	 if (sptr == scount + numpolys) break;
      }
      if (ordered == numpolys - 1) break;
   }

   if (startpt == numpolys) {
      Wprintf("Cannot join: Too many free endpoints");
      delete [] order;
      delete [] direc;
      delete [] scount;
      return;
   }

   /* create the new polygon or path */

   if (allpolys) {
      newpoly = topobject->append(new polygon);

      (*newpoly)->points.resize(numpoints);
      (*newpoly)->width  = polywidth;
      (*newpoly)->style  = polytype;
      (*newpoly)->color  = polycolor;

      /* insert the points into the new polygon */

      testpoint2 = (*newpoly)->points.begin();
      for (sptr = order; sptr < order + numpolys; sptr++) {
         nextwire = SELTOPOLY(sptr);
	 if (*(direc + (short)(sptr - order)) == 0) {
            for (testpoint = nextwire->points.begin(); testpoint < nextwire->points.end() - 1;
                testpoint++) {
               *testpoint2 = *testpoint;
	       testpoint2++;
	    }
         }
         else {
            for (testpoint = nextwire->points.end() - 1; testpoint
                   > nextwire->points.begin(); testpoint--) {
               *testpoint2 = *testpoint;
	       testpoint2++;
	    }
	 }
      }
      /* pick up the last point */
      *testpoint2 = *testpoint;

      /* delete the old elements from the list */

      register_for_undo(XCF_Wire, UNDO_MORE, areawin->topinstance, *newpoly);

      delobj = delete_element(areawin->topinstance, areawin->selectlist,
                areawin->selects);
      register_for_undo(XCF_Delete, UNDO_DONE, areawin->topinstance,
                delobj);

   }
   else {	/* create a path */

      newpath = topobject->append(new path);
      (*newpath)->style = polytype;
      (*newpath)->color = polycolor;
      (*newpath)->width = polywidth;
      /* copy the elements from the top level into the path structure */

      for (sptr = order; sptr < order + numpolys; sptr++) {
         genericptr oldelem = topobject->at(*sptr);
         genericptr newelem = *(*newpath)->append(oldelem->copy());

	 /* reverse point order if necessary */
         if (*(direc + (short)(sptr - order)) == 1) newelem->reverse();

	 /* decompose arcs into bezier curves */
         if (ELEMENTTYPE(newelem) == ARC) (*newpath)->replace_last(new spline(*TOARC(&newelem)));
      }

      /* delete the old elements from the list */

      register_for_undo(XCF_Join, UNDO_MORE, areawin->topinstance, *newpath);

      delobj = delete_element(areawin->topinstance, scount, numpolys);

      register_for_undo(XCF_Delete, UNDO_DONE, areawin->topinstance,
                delobj, NORMAL);

      /* Remove the path parts from the selection list and add the path */
      clearselects();
      selectobj = allocselect();
      for (pathiter pgen; topobject->values(pgen); ) {
         if (pgen == (*newpath)) {
            *selectobj = (short)(pgen - topobject->begin());
	    break;
	 }
      }
   }

   /* clean up */

   incr_changes(topobject);
   /* Do not clear the selection, to be consistent with all the	*/
   /* other actions that clear only when something has not been	*/
   /* preselected before the action.  Elements must be selected	*/
   /* prior to the "join" action, by necessity.			*/
   delete [] scount;
   delete [] order;
   delete [] direc;
}

void join_call(QAction *, void *, void *)
{
    join();
}

/*-------------------------------------------------*/
/* ButtonPress handler while a wire is being drawn */
/*-------------------------------------------------*/

void wire_op(int op, int x, int y)
{
   XPoint userpt, *tpoint;
   polyptr newwire;

   snap(x, y, &userpt);

   newwire = TOPOLY(EDITPART);

   if (areawin->attachto >= 0) {
      XPoint apos;
      findattach(&apos, NULL, &userpt);
      userpt = apos;
      areawin->attachto = -1;
   }
   else {
      if (areawin->manhatn) manhattanize(&userpt, newwire, -1, true);
   }
 
   tpoint = newwire->points.end() - 1;
   *tpoint = userpt;

   /* cancel wire operation completely */
   if (op == XCF_Cancel) {
      delete newwire;
      newwire = NULL;
      eventmode = NORMAL_MODE;
   }

   /* back up one point; prevent length zero wires */
   else if ((op == XCF_Cancel_Last) || ((tpoint - 1)->x == userpt.x &&
	   (tpoint - 1)->y == userpt.y)) {
      if (newwire->points.count() <= 2) {
         delete newwire;
	 newwire = NULL;
         eventmode = NORMAL_MODE;
      }
      else {
         newwire->points.pop_back();
         if (newwire->points.count() == 2) newwire->style = UNCLOSED |
   		(areawin->style & (DASHED | DOTTED));
      }
   }

   if (newwire && (op == XCF_Wire || op == XCF_Continue_Element)) {
      if (newwire->points.count() == 2)
	 newwire->style = areawin->style;
      newwire->points.append(userpt);
   }
   else if ((newwire == NULL) || op == XCF_Finish_Element || op == XCF_Cancel) {
      xcRemoveEventHandler(areawin->viewport, PointerMotionMask, false,
         (XtEventHandler)trackwire, NULL);
   }

   if (newwire) {
      if (op == XCF_Finish_Element) {

	 /* If the last points are the same, remove all redundant ones.	*/
	 /* This avoids the problem of extra points when people do left	*/
	 /* click followed by middle click to finish (the redundant way	*/
	 /* a lot of drawing programs work).				*/

	 XPoint *t2pt;
         while (newwire->points.count() > 2) {
            tpoint = newwire->points.end() - 1;
            t2pt = newwire->points.end() - 2;
            if (*tpoint != *t2pt)
	       break;
            newwire->points.pop_back();
	 }

	 topobject->parts++;
	 incr_changes(topobject);
	 if (!nonnetwork(newwire)) invalidate_netlist(topobject);
         register_for_undo(XCF_Wire, UNDO_MORE, areawin->topinstance, newwire);
      }
      areawin->update();
      if (op == XCF_Cancel_Last)
         checkwarp(newwire->points.end() - 1);
   }

   if (op == XCF_Finish_Element) {
      eventmode = NORMAL_MODE;
      singlebbox(EDITPART);
   }
}

/*-------------------------------------------------------------------------*/
