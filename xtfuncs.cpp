/*----------------------------------------------------------------------*/
/* xtfuncs.c --- Functions associated with the XCircuit Xt GUI		*/
/*	(no Tcl/Tk interpreter)						*/
/* Copyright (c) 2002  Tim Edwards, Johns Hopkins University        	*/
/*----------------------------------------------------------------------*/

#include <QColorDialog>
#include <QWidget>
#include <QToolBar>
#include <QToolButton>
#include <QMenu>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cerrno>
#include <climits>
#include <stdint.h>

#include <sys/types.h>

#include "xcircuit.h"
#include "prototypes.h"
#include "xcqt.h"
#include "colors.h"

/*----------------------------------------------------------------------*/
/* External Variable definitions					*/
/*----------------------------------------------------------------------*/

extern Widget	 top;
extern ApplicationData appdata;
extern Cursor	  appcursors[NUM_CURSORS];
extern fontinfo *fonts;
extern short fontcount;
extern short	 popups;	     /* total number of popup windows */

/*----------------------------------------------------------------------*/
/* The rest of the local includes depend on the prototype declarations	*/
/* and some of the external global variable declarations.		*/
/*----------------------------------------------------------------------*/

#include "menus.h"

/*----------------------------------------------------------------------*/
/* Local Variable definitions						*/
/*----------------------------------------------------------------------*/

QVector<u_short> fontnumbers;

/*----------------------------------------------*/
/* Set Poly and Arc line styles and fill styles */
/*----------------------------------------------*/

#define BORDERS  (NOBORDER | DOTTED | DASHED)
#define ALLFILLS (FILLSOLID | FILLED)

/*--------------------------------------------------------------*/
/* Toggle a bit in the justification field of a label		*/
/*--------------------------------------------------------------*/

void dojustifybit(QObject* w, labelptr settext, short bitfield)
{
   if (settext != NULL) {
      int oldjust = (int)settext->justify;

      settext->justify ^= bitfield;
      pwriteback(areawin->topinstance);

      register_for_undo(XCF_Justify, UNDO_MORE, areawin->topinstance,
                settext, oldjust);
   }
   else
      areawin->justify ^= bitfield;

   if (w != NULL) {
      bool boolval;
      if (settext)
	 boolval = (settext->justify & bitfield) ? 0 : 1;
      else
	 boolval = (areawin->justify & bitfield) ? 0 : 1;
      toggle(qobject_cast<QAction*>(w), Number(-1), &boolval);
   }
}

/*--------------------------------------------------------------*/
/* Toggle a pin-related bit in the justification field of a	*/
/* label.  This differs from dojustifybit() in that the label	*/
/* must be a pin, and this function cannot change the default	*/
/* behavior set by areawin->justify.				*/
/*--------------------------------------------------------------*/

void dopinjustbit(QObject* w, labelptr settext, short bitfield)
{
   if ((settext != NULL) && settext->pin) {
      int oldjust = (int)settext->justify;

      settext->justify ^= bitfield;
      pwriteback(areawin->topinstance);

      register_for_undo(XCF_Justify, UNDO_MORE, areawin->topinstance,
                settext, oldjust);

      if (w != NULL) {
	 bool boolval = (settext->justify & bitfield) ? 0 : 1;
         toggle(qobject_cast<QAction*>(w), Number(-1), &boolval);
      }
   }
}

/*----------------------------------------------------------------*/
/* Set the justification for the label passed as 3rd parameter	  */
/*----------------------------------------------------------------*/

void setjust(QAction* w, void* value_, labelptr settext, short mode)
{
   short newjust, oldjust;

   unsigned int value = (uintptr_t)value_;

   if (settext != NULL) {

      if (mode == 1)
         newjust = (settext->justify & (NONJUSTFIELD | TBJUSTFIELD))
			| value;
      else
         newjust = (settext->justify & (NONJUSTFIELD | RLJUSTFIELD))
			| value;

      if (settext->justify != newjust) {
          oldjust = (int)settext->justify;
          settext->justify = newjust;
          pwriteback(areawin->topinstance);

          register_for_undo(XCF_Justify, UNDO_MORE, areawin->topinstance,
                settext, oldjust);
      }
   }
   else {
      if (mode == 1)
         newjust = (areawin->justify & (NONJUSTFIELD | TBJUSTFIELD))
			| value;
      else
         newjust = (areawin->justify & (NONJUSTFIELD | RLJUSTFIELD))
			| value;
      areawin->justify = newjust;
   }
   if (w != NULL) toggleexcl(w);
}

/*----------------------------------------------------------------*/
/* Set vertical justification (top, middle, bottom) on all	  */
/* selected labels						  */
/*----------------------------------------------------------------*/

void setvjust(QAction* w, void* value, void*)
{
   short *fselect;
   labelptr settext;
   short labelcount = 0;

   if (eventmode == TEXT_MODE || eventmode == ETEXT_MODE) {
      settext = *((labelptr *)EDITPART);
      setjust(w, value, settext, 2);
   }
   else {
      for (fselect = areawin->selectlist; fselect < areawin->selectlist +
            areawin->selects; fselect++) {
         if (SELECTTYPE(fselect) == LABEL) {
            labelcount++;
            settext = SELTOLABEL(fselect);
            setjust(NULL, value, settext, 2);
         }
      }
      if (labelcount == 0) setjust(w, value, NULL, 2);
      else unselect_all();
   }
}

/*----------------------------------------------------------------*/
/* Set horizontal justification (left, center, right) on all	  */
/* selected labels						  */
/*----------------------------------------------------------------*/

void sethjust(QAction* w, void* value, void*)
{
   short *fselect;
   labelptr settext;
   short labelcount = 0;

   if (eventmode == TEXT_MODE || eventmode == ETEXT_MODE) {
      settext = *((labelptr *)EDITPART);
      setjust(w, value, settext, 1);
   }
   else {
      for (fselect = areawin->selectlist; fselect < areawin->selectlist +
            areawin->selects; fselect++) {
         if (SELECTTYPE(fselect) == LABEL) {
            labelcount++;
            settext = SELTOLABEL(fselect);
            setjust(NULL, value, settext, 1);
         }
      }
      if (labelcount == 0) setjust(w, value, NULL, 1);
      else unselect_all();
   }
}

/*--------------------------------------------------------------*/
/* Set a justify field bit on all selected labels		*/ 
/* (flip invariance, latex mode, etc.)				*/
/*--------------------------------------------------------------*/

void setjustbit(QAction* w, void* value_, void*)
{
   short *fselect;
   labelptr settext;
   short labelcount = 0;
   int value = (intptr_t)value_;

   if (eventmode == TEXT_MODE || eventmode == ETEXT_MODE) {
      settext = *((labelptr *)EDITPART);
      dojustifybit(w, settext, (short)value);
   }
   else {
      for (fselect = areawin->selectlist; fselect < areawin->selectlist +
            areawin->selects; fselect++) {
         if (SELECTTYPE(fselect) == LABEL) {
            labelcount++;
            settext = SELTOLABEL(fselect);
            dojustifybit(NULL, settext, (short)value);
         }
      }
      if (labelcount == 0) dojustifybit(w, NULL, (short)value);
      else unselect_all();
   }
}

/*--------------------------------------------------------------*/
/* Set pin-related bit field of "justify" on all selected pins	*/ 
/* (e.g., pin visibility)					*/
/*--------------------------------------------------------------*/

void setpinjustbit(QAction* w, void* value_, void*)
{
   short *fselect;
   labelptr settext;
   int value = (intptr_t)value_;

   if (eventmode == TEXT_MODE || eventmode == ETEXT_MODE) {
      settext = *((labelptr *)EDITPART);
      if (settext->pin)
	 dopinjustbit(w, settext, (short)value);
   }
   else {
      for (fselect = areawin->selectlist; fselect < areawin->selectlist +
            areawin->selects; fselect++) {
         if (SELECTTYPE(fselect) == LABEL) {
            settext = SELTOLABEL(fselect);
	    if (settext->pin)
               dopinjustbit(NULL, settext, (short)value);
         }
      }
      unselect_all();
   }
}

/*--------------------------------------------------------------*/
/* Enable or Disable the toolbar				*/
/*--------------------------------------------------------------*/

void dotoolbar(QAction*, void*, void*)
{
   if (areawin->toolbar_on) {
      areawin->toolbar_on = false;
      toolbar->hide();
      menuAction("Options_DisableToolbar")->setText("Enable Toolbar");
   }
   else {
      areawin->toolbar_on = true;
      toolbar->show();
      menuAction("Options_DisableToolbar")->setText("Disable Toolbar");
   }
}  

/*--------------------------------------------------------------*/
/* Overwrite the toolbar pixmap for color or stipple entries	*/
/*--------------------------------------------------------------*/

#ifdef XC_X11

void overdrawpixmap(Widget button)
{
   Widget pbutton;
   Arg args[3];
   int ltype, color;
   Pixmap stippix;

   if (button == NULL) return;

   XtSetArg(args[0], XtNlabelType, &ltype);
   XtGetValues(button, args, 1);

   if (ltype != XwRECT && button != ColorInheritColorButton) return;

   XtSetArg(args[0], XtNrectColor, &color);
   XtSetArg(args[1], XtNrectStipple, &stippix);
   XtGetValues(button, args, 2);

   if (stippix == (Pixmap)NULL && button != FillBlackButton)
      pbutton = ColorsToolButton;
   else
      pbutton = FillsToolButton;

   if (button == ColorInheritColorButton) {
      XtSetArg(args[0], XtNlabelType, XwIMAGE);
      XtSetValues(pbutton, args, 1);
   }
   else if (button == FillBlackButton) {
      XtSetArg(args[0], XtNlabelType, XwRECT);
      XtSetArg(args[1], XtNrectColor, color);
      XtSetArg(args[2], XtNrectStipple, (Pixmap)NULL);
      XtSetValues(pbutton, args, 3);
   }
   else if (button != FillOpaqueButton) {
      XtSetArg(args[0], XtNlabelType, XwRECT);
      if (stippix == (Pixmap)NULL)
         XtSetArg(args[1], XtNrectColor, color);
      else
         XtSetArg(args[1], XtNrectStipple, stippix);

      XtSetValues(pbutton, args, 2);
   }
}

#endif

/*--------------------------------------------------------------*/
/* Generate popup dialog for snap space value input		*/
/*--------------------------------------------------------------*/

void getsnapspace(QAction* a, void*, void*)
{
   char buffer[50];
   float *floatptr = &xobjs.pagelist[areawin->page].snapspace;
   measurestr(*floatptr, buffer);
   popupQuestion(a, "Enter value:", buffer, setgrid, floatptr);
}

/*--------------------------------------------------------------*/
/* Generate popup dialog for grid space value input		*/
/*--------------------------------------------------------------*/

void getgridspace(QAction* a, void*, void*)
{
   char buffer[50];
   float *floatptr = &xobjs.pagelist[areawin->page].gridspace;
   measurestr(*floatptr, buffer);
   popupQuestion(a, "Enter value:", buffer, setgrid, floatptr);
}

/*----------------------------------------------------------------*/
/* Generic routine for setting a floating-point value (through a  */
/* (float *) pointer passed as the second parameter)              */
/*----------------------------------------------------------------*/

void setfloat(QAction*, const QString & str, void* dataptr_)
{
   float *dataptr = (float*)dataptr_;
   float oldvalue = *dataptr;
   bool ok;
   *dataptr = str.toFloat(&ok);
   if (!ok || *dataptr <= 0) {
      *dataptr = oldvalue;
      Wprintf("Illegal value");
   }  
   if (oldvalue != *dataptr) areawin->update();
}

/*----------------------------------------------------------------*/
/* Set text scale.                                                */
/*----------------------------------------------------------------*/

void settsize(QAction*, const QString & str, void*)
{
    bool ok;
    float tmpres = str.toFloat(&ok);
    if (!ok || tmpres <= 0) {  /* can't interpret value or bad value */
      Wprintf("Illegal value");
      return;
    }
    changetextscale(tmpres);

    if (areawin->selects > 0) unselect_all();
}

/*--------------------------------------------------------------*/
/* Auto-set:  Enable automatic scaling                          */
/*--------------------------------------------------------------*/

void autoset(QObject* w, WidgetList entertext, caddr_t nulldata)
{
   xobjs.pagelist[areawin->page].pmode |= 2;
   updatetext(w, entertext, nulldata);
}

/*--------------------------------------------------------------*/

void autostop(QWidget*, void*, void*)
{
   xobjs.pagelist[areawin->page].pmode &= 1;
}

/*--------------------------------------------------------------*/
/* Set the denominator value of the drawing scale 		*/
/*--------------------------------------------------------------*/

void setscaley(Widget, const QString & str, void *dataptr_)
{
   float *dataptr = (float*)dataptr_;
   float oldvalue = *dataptr;
   bool ok;
   *dataptr = str.toFloat(&ok);

   if (!ok || *dataptr <= 0 || topobject->bbox.height == 0) {
      *dataptr = oldvalue;
      Wprintf("Illegal value");
   }
   else {
      *dataptr = (*dataptr * 72) / topobject->bbox.height;
      *dataptr /= getpsscale(1.0, areawin->page);
   }
}

/*----------------------------------------------------------------*/
/* Set the numerator value of the drawing scale			  */
/*----------------------------------------------------------------*/

void setscalex(Widget, const QString & str, void *dataptr_)
{
   float *dataptr = (float*)dataptr_;
   float oldvalue = *dataptr;
   bool ok;
   *dataptr = str.toFloat(&ok);

   if (!ok || *dataptr <= 0 || topobject->bbox.width == 0) {
      *dataptr = oldvalue;
      Wprintf("Illegal value");
   }
   else {
      *dataptr = (*dataptr * 72) / topobject->bbox.width;
      *dataptr /= getpsscale(1.0, areawin->page);
   }
}

/*----------------------------------------------------------------*/
/* Set the page orientation (either Landscape or Portrait)	  */
/*----------------------------------------------------------------*/

void setorient(Widget w, void * dataptr_, void*)
{
   short *dataptr = (short*)dataptr_;
   Arg wargs[1];

   if (*dataptr == 0) {
      *dataptr = 90;
      XtSetArg(wargs[0], XtNlabel, "Landscape");
   }
   else {
      *dataptr = 0;
      XtSetArg(wargs[0], XtNlabel, "Portrait");
   }
   XtSetValues(w, wargs, 1);
}

/*----------------------------------------------------------------*/
/* Set the output mode to "Full Page" (unencapsulated PostScript) */
/* or "Embedded" (encapsulated PostScript)			  */
/*----------------------------------------------------------------*/

void setpmode(Widget w, void* dataptr_, void*)
{
   short *dataptr = (short*)dataptr_;
   Arg wargs[1];
   Widget pwidg = XtParent(w);
   Widget autowidg = XtNameToWidget(pwidg, "Auto-fit");

   if (!(*dataptr & 1)) {
      *dataptr = 1;
      XtSetArg(wargs[0], XtNlabel, "Full Page");

      XtManageChild(XtNameToWidget(pwidg, "fpedit"));
      XtManageChild(XtNameToWidget(pwidg, "fpokay"));
      XtManageChild(autowidg);
   }
   else {
      *dataptr = 0;	/* This also turns off auto-fit */
      XtSetArg(wargs[0], XtNset, false);
      XtSetValues(autowidg, wargs, 1);
      XtSetArg(wargs[0], XtNlabel, "Embedded");

      XtUnmanageChild(XtNameToWidget(pwidg, "fpedit"));
      XtUnmanageChild(XtNameToWidget(pwidg, "fpokay"));
      XtUnmanageChild(autowidg);
   }
   XtSetValues(w, wargs, 1);
}

/*--------------------------------------------------------------*/
/* Set the output page size, in the current unit of measure	*/
/*--------------------------------------------------------------*/

void setpagesize(QWidget *w, XPoint * dataptr, const QString & str)
{
   char fpedit[75];
   Arg wargs[1];
   bool is_inches;

   is_inches = setoutputpagesize(dataptr, str);
   sprintf(fpedit, "%3.2f x %3.2f %s",
        (float)xobjs.pagelist[areawin->page].pagesize.x / 72.0,
        (float)xobjs.pagelist[areawin->page].pagesize.y / 72.0,
	(is_inches) ? "in" : "cm");

   XtSetArg(wargs[0], XtNstring, fpedit);
   XtSetValues(XtNameToWidget(XtParent(w), "fpedit"), wargs, 1);
}

/*--------------------------------------------------------------*/
/* Generate popup dialog to get a character kern value, which	*/
/* is two integer numbers in the range -128 to +127 (size char)	*/
/*--------------------------------------------------------------*/

void getkern(QAction* a, void*, void*)
{
   char buffer[50];
   int kx, ky;
   stringpart *strptr, *nextptr;

   strcpy(buffer, "0,0");

   if (eventmode == TEXT_MODE || eventmode == ETEXT_MODE) {
      labelptr curlabel = TOLABEL(EDITPART);
      strptr = findstringpart(areawin->textpos - 1, NULL, curlabel->string,
			areawin->topinstance);
      nextptr = findstringpart(areawin->textpos, NULL, curlabel->string,
			areawin->topinstance);
      if (strptr->type == KERN) {
	 kx = strptr->data.kern[0];
	 ky = strptr->data.kern[1];
         sprintf(buffer, "%d,%d", kx, ky);
      }
      else if (nextptr && nextptr->type == KERN) {
	 strptr = nextptr;
	 kx = strptr->data.kern[0];
	 ky = strptr->data.kern[1];
         sprintf(buffer, "%d,%d", kx, ky);
      }
      else strptr = NULL;
   }

   popupQuestion(a, "Enter Kern X,Y:", buffer, setkern, strptr);
}

/*----------------------------------------------------------------*/
/* Generate popup dialog to get the drawing scale, specified as a */
/* whole-number ratio X:Y					  */
/*----------------------------------------------------------------*/

void getdscale(QAction* a, void*, void*)
{
   char buffer[50];
   XPoint *ptptr = &(xobjs.pagelist[areawin->page].drawingscale);
   sprintf(buffer, "%d:%d", ptptr->x, ptptr->y);
   popupQuestion(a, "Enter Scale:", buffer, setdscale, ptptr);
}

/*--------------------------------------------------------------*/
/* Generate the popup dialog for getting text scale.		*/ 
/*--------------------------------------------------------------*/

void gettsize(QAction* a, void*, void*)
{
   char buffer[50];
   float *floatptr;
   labelptr settext;

   settext = gettextsize(&floatptr);
   sprintf(buffer, "%5.2f", *floatptr);

   if (settext) {
      popupQuestion(a, "Enter text scale:", buffer, settsize, settext);
   }
   else {
      popupQuestion(a, "Enter default text scale:", buffer, setfloat, floatptr);
   }
}

/*----------------------------------------------------------------*/
/* Generate popup dialog for getting object scale		  */
/*----------------------------------------------------------------*/

void getosize(QAction* a, void*, void*)
{
   char buffer[50];
   float flval;
   short *osel = areawin->selectlist;
   short selects = 0;
   objinstptr setobj = NULL;

   for (; osel < areawin->selectlist + areawin->selects; osel++)
      if (SELECTTYPE(osel) == OBJINST) {
	 setobj = SELTOOBJINST(osel);
	 selects++;
	 break;
      }
   if (setobj == NULL) {
      Wprintf("No objects were selected for scaling.");
      return;
   }
   flval = setobj->scale;
   sprintf(buffer, "%4.2f", flval);
   popupQuestion(a, "Enter object scale:", buffer, setosize, setobj);
}

/*----------------------------------------------------------------*/
/* Generate popup prompt for getting global linewidth		  */
/*----------------------------------------------------------------*/

void getwirewidth(QAction* a, void*, void*)
{
   char buffer[50];
   float *widthptr;

   widthptr = &(xobjs.pagelist[areawin->page].wirewidth);
   sprintf(buffer, "%4.2f", *widthptr / 2.0);
   popupQuestion(a, "Enter new global linewidth:", buffer, setwidth, widthptr);
}

/*----------------------------------------------------------------*/
/* Generate popup dialong for getting linewidths of elements	  */ 
/*----------------------------------------------------------------*/

void getwwidth(QAction* a, void*, void*)
{
   char buffer[50];
   short *osel = areawin->selectlist;
   genericptr setel;
   float flval;

   for (; osel < areawin->selectlist + areawin->selects; osel++) {
      setel = *(topobject->begin() + (*osel));
      if (IS_ARC(setel)) {
	 flval = ((arcptr)setel)->width;
	 break;
      }
      else if (IS_POLYGON(setel)) {
	 flval = ((polyptr)setel)->width;
	 break;
      }
      else if (IS_SPLINE(setel)) {
	 flval = ((splineptr)setel)->width;
	 break;
      }
      else if (IS_PATH(setel)) {
	 flval = ((pathptr)setel)->width;
	 break;
      }
   }
   if (osel == areawin->selectlist + areawin->selects) {
      sprintf(buffer, "%4.2f", areawin->linewidth);
      popupQuestion(a, "Enter new default line width:", buffer, setwwidth, setel);
   }
   else {
      sprintf(buffer, "%4.2f", flval);
      popupQuestion(a, "Enter new line width:", buffer, setwwidth, setel);
   }
}

/*----------------------------------------------------------------*/
/* Generic popup prompt for getting a floating-point value	  */
/*----------------------------------------------------------------*/

void getfloat(QAction* a, void *floatptr_, void*)
{
   float* floatptr = (float*)floatptr_;
   char buffer[50];
   sprintf(buffer, "%4.2f", *floatptr);
   popupQuestion(a, "Enter value:", buffer, setfloat, floatptr);
}

/*----------------------------------------------------------------*/
/* Set the filename for the current page			  */
/*----------------------------------------------------------------*/

void setfilename(QAction*, const QString &str, void *dataptr_)
{
   char ** dataptr = (char**)dataptr_;
   short cpage;
   QString oldstr = xobjs.pagelist[areawin->page].filename;

   if ((*dataptr != NULL) && !strcmp(*dataptr, str.toLocal8Bit()))
      return;   /* no change in string */

   /* Make the change to the current page */
   xobjs.pagelist[areawin->page].filename = str;

   /* All existing filenames which match the old string should also be changed */
   for (cpage = 0; cpage < xobjs.pages; cpage++) {
      if ((xobjs.pagelist[cpage].pageinst != NULL) && (cpage != areawin->page)) {
         if (xobjs.pagelist[cpage].filename == oldstr) {
            xobjs.pagelist[cpage].filename = str;
	 }
      }
   }
}

/*----------------------------------------------------------------*/
/* Set the page label for the current page			  */
/*----------------------------------------------------------------*/

void setpagelabel(Widget, const QString &str, void *dataptr_)
{
   char* dataptr = (char*)dataptr_;
   short i;

   QByteArray str2 = str.toLocal8Bit();

   /* Whitespace and non-printing characters not allowed */
   for (i = 0; i < str2.length(); i++) {
      if ((!isprint(str2[i])) || (isspace(str2[i]))) {
         str2[i] = '_';
         Wprintf("Replaced illegal whitespace in name with underscore");
      }
   }

   if (!strcmp(dataptr, str2.data())) return; /* no change in string */
   if (str2.length() == 0)
      sprintf(topobject->name, "Page %d", areawin->page + 1);
   else
      sprintf(topobject->name, "%.79s", str2.data());

   /* For schematics, all pages with associations to symbols must have	*/
   /* unique names.							*/
   if (topobject->symschem != NULL) checkpagename(topobject);

   printname(topobject);
   renamepage(areawin->page);
}

/*--------------------------------------------------------------*/
/* Change the Button1 binding for a particular mode (non-Tcl)	*/
/*--------------------------------------------------------------*/

/*--------------------------------------------------------------*/
/* Change to indicated tool (key binding w/o value) */
/*--------------------------------------------------------------*/

void changetool(QAction* w, void* value, void*)
{
   mode_rebinding((intptr_t)value, -1);
   highlightexcl(w, (intptr_t)value, -1);
}

/*--------------------------------------------------------------*/
/* Execute function binding for the indicated tool if something	*/
/* is selected;  otherwise, change to the indicated tool mode.	*/
/*--------------------------------------------------------------*/

void exec_or_changetool(QAction* w, void* value, void*)
{
   if (areawin->selects > 0)
      mode_tempbinding((intptr_t)value, -1);
   else {
      mode_rebinding((intptr_t)value, -1);
      highlightexcl(w, (intptr_t)value, -1);
   }
}

/*--------------------------------------------------------------*/
/* Special case of exec_or_changetool(), where an extra value	*/
/* is passed to the routine indication amount of rotation, or	*/
/* type of flip operation.					*/
/*--------------------------------------------------------------*/

void rotatetool(QAction* w, void* value, void*)
{
   if (areawin->selects > 0)
      mode_tempbinding(XCF_Rotate, (intptr_t)value);
   else {
      mode_rebinding(XCF_Rotate, (intptr_t)value);
      highlightexcl(w, XCF_Rotate, (intptr_t)value);
   }
}

/*--------------------------------------------------------------*/
/* Special case of changetool() for pan mode, where a value is	*/
/* required to pass to the key binding routine.			*/
/*--------------------------------------------------------------*/

void pantool(QAction* w, void* value, void*)
{
   mode_rebinding(XCF_Pan, (intptr_t)value);
   highlightexcl(w, XCF_Pan, (intptr_t)value);
}

/*--------------------------------------------------------------*/
/* Add a new font name to the list of known fonts		*/
/* Register the font number for the Alt-F cycling mechanism	*/
/* Tcl: depends on command tag mechanism for GUI menu update.	*/
/*--------------------------------------------------------------*/

void makenewfontbutton()
{
   if (fontcount == 0) return;

   QMenu * cascade = qobject_cast<QMenu*>(menuAction("Font_AddNewFont")->parentWidget());
   QAction * newaction = cascade->addAction(fonts[fontcount-1].family);
   newaction->setCheckable(true);
   XtAddCallback (newaction, setfont, Number(fontcount - 1));
   fontnumbers.append(fontcount - 1);
}

/*--------------------------------------------------------------*/
/* Make new encoding menu button				*/
/*--------------------------------------------------------------*/

void makenewencodingbutton(const char *ename, char value)
{
    QMenu * cascade = qobject_cast<QMenu*>(menuAction("Encoding_Standard")->parentWidget());

    /* return if button has already been made */
    foreach (QAction * action, cascade->actions()) {
        if (action->text() == ename) return;
    }

    QAction * newaction = cascade->addAction(ename);
    newaction->setCheckable(true);
    XtAddCallback (newaction, setfontencoding, Number(value));
}

/*--------------------------------------------------------------*/
/* Set the menu checkmarks on the font menu			*/
/*--------------------------------------------------------------*/

void togglefontmark(int fontval)
{
    QMenu * cascade = qobject_cast<QMenu*>(menuAction("Font_AddNewFont")->parentWidget());

    foreach (QAction * action, cascade->actions()) {
        action->setChecked(action->text() == fonts[fontval].family);
    }
}

/*--------------------------------------------------------------------*/
/* Toggle one of a set of menu items, only one of which can be active */
/* The set must be delimited by menu separators or ends of the menu   */
/*--------------------------------------------------------------------*/

void toggleexcl(QAction* action)
{
    if (!action) return;

    QMenu * menu = qobject_cast<QMenu*>(action->parentWidget());
    QAction * top = NULL;

    // find first action of the action group, delimited by menu separators
    foreach (QAction * a, menu->actions()) {
        if (!top) top = a;
        if (a == action) break;
        if (a->isSeparator()) top = NULL;
    }
    Q_ASSERT(top != NULL);

    // check one action within a group
    bool ready = false;
    foreach (QAction * a, menu->actions()) {
        if (a == top) ready = true;
        if (ready) {
            if (a->isSeparator()) break;
            a->setChecked(a == action);
        }
    }
}

/* Enable/disable a set of menu items. The set must be delimited by menu
   separators or ends of the menu */

void setEnabled(QAction* action, bool ena)
{
    if (!action) return;

    QMenu * menu = qobject_cast<QMenu*>(action->parentWidget());
    QAction * top = NULL;

    // find first action of the action group, delimited by menu separators
    foreach (QAction * a, menu->actions()) {
        if (!top) top = a;
        if (a == action) break;
        if (a->isSeparator()) top = NULL;
    }
    Q_ASSERT(top != NULL);

    // check one action within a group
    bool ready = false;
    foreach (QAction * a, menu->actions()) {
        if (a == top) ready = true;
        if (ready) {
            if (a->isSeparator()) break;
            a->setEnabled(ena);
        }
    }
}

/*----------------------------------------------------------------------*/
/* Cursor changes based on toolbar mode					*/
/*----------------------------------------------------------------------*/

void toolcursor(int mode)
{
   /* Some cursor types for different modes */
   switch(mode) {
      case XCF_Spline:
      case XCF_Move:
      case XCF_Join:
      case XCF_Unjoin:
         XDefineCursor(areawin->viewport, ARROW);
	 areawin->defaultcursor = &ARROW;
	 break;
      case XCF_Pan:
         XDefineCursor(areawin->viewport, HAND);
	 areawin->defaultcursor = &HAND;
	 break;
      case XCF_Delete:
         XDefineCursor(areawin->viewport, SCISSORS);
	 areawin->defaultcursor = &SCISSORS;
	 break;
      case XCF_Copy:
         XDefineCursor(areawin->viewport, COPYCURSOR);
	 areawin->defaultcursor = &COPYCURSOR;
	 break;
      case XCF_Push:
      case XCF_Select_Save:
         XDefineCursor(areawin->viewport, QUESTION);
	 areawin->defaultcursor = &QUESTION;
	 break;
      case XCF_Rotate:
      case XCF_Flip_X:
      case XCF_Flip_Y:
         XDefineCursor(areawin->viewport, ROTATECURSOR);
	 areawin->defaultcursor = &ROTATECURSOR;
	 break;
      case XCF_Edit:
         XDefineCursor(areawin->viewport, EDCURSOR);
	 areawin->defaultcursor = &EDCURSOR;
	 break;
      case XCF_Arc:
         XDefineCursor(areawin->viewport, CIRCLE);
	 areawin->defaultcursor = &CIRCLE;
	 break;
      case XCF_Text:
      case XCF_Pin_Label:
      case XCF_Pin_Global:
      case XCF_Info_Label:
         XDefineCursor(areawin->viewport, TEXTPTR);
	 areawin->defaultcursor = &TEXTPTR;
	 break;
      default:
         XDefineCursor(areawin->viewport, CROSS);
	 areawin->defaultcursor = &CROSS;
	 break;
   }
}

/*--------------------------------------------------------------------------*/
/* Highlight one of a set of toolbar items, only one of which can be active */
/*--------------------------------------------------------------------------*/

void highlightexcl(QAction* action, int func, int value)
{
   QWidget* const toolbar = top->findChild<QToolBar*>("Toolbar");

   QList<QWidget*> widgets = action->associatedWidgets();
   QAbstractButton * button;
   foreach(QWidget* widget, widgets) {
       button = qobject_cast<QAbstractButton*>(widget);
       if (button) break;
   }

   // Since graphicsEffect doesn't work on widgets on all platforms, we instead
   // use the up/down state of a QAbstractButton to "highlight" the current
   // toolbar item. Changing up/down state doesn't fire anything, only changes
   // the visualization.

   /* remove highlight from all buttons in the toolbar */
   foreach (QAbstractButton * b, toolbar->findChildren<QAbstractButton*>()) {
       b->setDown(false);
   }

   if (button == NULL) {
      /* We invoked a toolbar button from somewhere else.  Highlight	*/
      /* the toolbar associated with the function value.		*/
      switch (func) {
	 case XCF_Pan:
            button = toolbar->findChild<QAbstractButton*>("Pan");
	    break;

	 case XCF_Rotate:
	    if (value > 0)
               button = toolbar->findChild<QAbstractButton*>("RotP");
	    else
               button = toolbar->findChild<QAbstractButton*>("RotN");
	    break;

	 default:
            for (toolbarptr titem = ToolBar; titem->name; titem++) {
               if (func == (intptr_t)titem->passeddata) {
                  button = toolbar->findChild<QAbstractButton*>(titem->name);
	          break;
	       }
	    }
	    break;
      }
   }

   /* Now highlight the currently pushed widget */
   if (button != NULL) {
       button->setDown(true);
   }
}

/*--------------------*/
/* Toggle a menu item */
/*--------------------*/

void toggle(QAction* a, void* soffset_, void* setdata)
{
   bool *boolvalue;
   unsigned int soffset = (uintptr_t)soffset_;

   if (soffset == -1)
      boolvalue = (bool*)setdata;
   else
      boolvalue = (bool *)(((char*)areawin) + soffset);

   *boolvalue = !(*boolvalue);
   a->setChecked(*boolvalue);
   areawin->update();
}

/*----------------------------------------------------------------*/
/* Invert the color scheme used for the background/foreground	  */
/*----------------------------------------------------------------*/

void inversecolor(QAction* a, void* soffset_, void* calldata)
{
   const unsigned int soffset = (uintptr_t)soffset_;
   bool *boolvalue = (bool *)(((char*)areawin) + soffset);

   setcolorscheme(*boolvalue);

   if (a == NULL) a = menuAction("Options_AltColors");
   toggle(a, soffset_, calldata);

   if (eventmode == NORMAL_MODE)
      XDefineCursor(areawin->viewport, DEFAULTCURSOR);
}

/*----------------------------------------------------------------*/
/* Change menu selection for reported measurement units		  */
/*----------------------------------------------------------------*/

void togglegrid(u_short type)
{
    QAction * a = NULL;
    switch (type) {
    case CM:
        a = menuAction("GridTypeDisplay_Centimeters");
        break;
    case FRAC_INCH:
        a = menuAction("GridTypeDisplay_FractionalInches");
        break;
    case DEC_INCH:
        a = menuAction("GridTypeDisplay_DecimalInches");
        break;
    case INTERNAL:
        break;
    }
    toggleexcl(a);
}

/*----------------------------------------------------------------*/
/* Set the default reported grid units to inches or centimeters   */
/*----------------------------------------------------------------*/

void setgridtype(char *string)
{
    QAction * a;
    if (!strcmp(string, "inchscale")) {
        a = menuAction("GridTypeDisplay_FractionalInches");
        getgridtype(a, (void*)FRAC_INCH, NULL);
    }
    else if (!strcmp(string, "cmscale")) {
        a = menuAction("GridTypeDisplay_Centimeters");
        getgridtype(a, (void*)CM, NULL);
    }
}

/*----------------------------------------------*/
/* Make new library and add a new button to the */
/* "Libraries" cascaded menu.			*/
/*----------------------------------------------*/

int createlibrary(bool force)
{
   int libnum;
   objectptr newlibobj;

   /* If there's an empty library, return its number */
   if ((!force) && (libnum = findemptylib()) >= 0) return (libnum + LIBRARY);
   libnum = (xobjs.numlibs++) + LIBRARY;
   xobjs.libtop = (objinstptr *)realloc(xobjs.libtop,
		(libnum + 1) * sizeof(objinstptr));
   xobjs.libtop[libnum] = xobjs.libtop[libnum - 1];
   libnum--;

   newlibobj = new object;
   xobjs.libtop[libnum] = new objinst(newlibobj);

   sprintf(newlibobj->name, "Library %d", libnum - LIBRARY + 1);

   /* Create the library */

   xobjs.userlibs = (Library *) realloc(xobjs.userlibs, xobjs.numlibs
	* sizeof(Library));
   xobjs.userlibs[libnum + 1 - LIBRARY] = xobjs.userlibs[libnum - LIBRARY];
   xobjs.userlibs[libnum - LIBRARY].library = (objectptr *) malloc(sizeof(objectptr));
   xobjs.userlibs[libnum - LIBRARY].number = 0;
   xobjs.userlibs[libnum - LIBRARY].instlist = NULL;

   /* To-do:  initialize technology list */
   /*
   xobjs.userlibs[libnum - LIBRARY].filename = NULL;
   xobjs.userlibs[libnum - LIBRARY].flags = (char)0;
   */

   QMenu * libmenu = menuAction("Window_Gotolibrary")->menu();

   /* Former "User Library" button becomes new library pointer */
   QAction * oldAction = libmenu->actions().last();
   oldAction->setText(xobjs.libtop[libnum]->thisobject->name);

   /* Add a new entry in the menu to restore the User Library button */
   QAction * newAction = libmenu->addAction("User Library");
   XtAddCallback(newAction, startcatalog, Number(libnum + 1));

   /* Update the library directory to include the new page */
   composelib(LIBLIB);

   return libnum;
}

/*--------------------------------------------------------------*/
/* Routine called by newpage() if new button needs to be made 	*/
/* to add to the "Pages" cascaded menu.				*/
/*--------------------------------------------------------------*/

void makepagebutton()
{
   /* make new entry in the menu */
   QMenu *pageMenu = menuAction("Window_Gotopage")->menu();
   QAction *newAction = pageMenu->addAction(QString("Page %1").arg(xobjs.pages));
   XtAddCallback (newAction, newpagemenu, Number(xobjs.pages - 1));

   /* Update the page directory */
   composelib(PAGELIB);
}

/*----------------------------------------------------------------*/
/* Find the Page menu button associated with the page number	  */
/* (passed parameter) and set the label of that button to the	  */
/* object name (= page label)					  */
/*----------------------------------------------------------------*/

void renamepage(short pagenumber)
{
    objinstptr thisinst = xobjs.pagelist[pagenumber].pageinst;
    QMenu *pageMenu = menuAction("Window_Gotopage")->menu();
    if (thisinst) pageMenu->actions()[pagenumber+2]->setText(thisinst->thisobject->name);
}

/*--------------------------------------------------------------*/
/* Same routine as above, for Library page menu buttons		*/
/*--------------------------------------------------------------*/

void renamelib(short libnumber)
{
    QMenu *libraryMenu = menuAction("Window_Gotolibrary")->menu();
    QAction *action = libraryMenu->actions()[libnumber - LIBRARY + 2];
    if (xobjs.libtop[libnumber]->thisobject->name != NULL) {
        action->setText(xobjs.libtop[libnumber]->thisobject->name);
    } else {
        action->setText(QString("Library %1").arg(libnumber - LIBRARY + 1));
    }
}

/*----------------------------------------------------------------*/
/* Set the checkmarks on the element styles menu		  */
/*----------------------------------------------------------------*/

void setallstylemarks(u_short styleval)
{
   QAction *a;

   menuAction("Border_Closed")->setChecked(! (styleval & UNCLOSED));
   menuAction("Border_BoundingBox")->setChecked(styleval & BBOX);

   if (styleval & NOBORDER)
      a = menuAction("Border_Unbordered");
   else if (styleval & DASHED)
      a = menuAction("Border_Dashed");
   else if (styleval & DOTTED)
      a = menuAction("Border_Dotted");
   else
      a = menuAction("Border_Solid");
   toggleexcl(a);

   if (styleval & OPAQUE)
      a = menuAction("Fill_Opaque");
   else
      a = menuAction("Fill_Transparent");
   toggleexcl(a);

   if (!(styleval & FILLED))
      a = menuAction("Fill_White");
   else {
      styleval &= FILLSOLID;
      styleval /= STIP0;
      switch(styleval) {
      case 0: a = menuAction("Fill_Gray87"); break;
      case 1: a = menuAction("Fill_Gray75"); break;
      case 2: a = menuAction("Fill_Gray62"); break;
      case 3: a = menuAction("Fill_Gray50"); break;
      case 4: a = menuAction("Fill_Gray37"); break;
      case 5: a = menuAction("Fill_Gray25"); break;
      case 6: a = menuAction("Fill_Gray12"); break;
      case 7: a = menuAction("Fill_Black"); break;
      }
   }
   toggleexcl(a);
}

/*--------------------------------------------------------------*/
/* The following five routines are all wrappers for		*/
/* setelementstyle(),	  					*/
/* used in menudefs to differentiate between sections, each of  */
/* which has settings independent of the others.		*/
/*--------------------------------------------------------------*/

void setfill(QAction* w, void* value, void*)
{
   setelementstyle(w, (uintptr_t)value, OPAQUE | FILLED | FILLSOLID);
}

/*--------------------------------------------------------------*/

void makebbox(QAction* w, void* value, void*)
{
   setelementstyle(w, (uintptr_t)value, BBOX);
}

/*--------------------------------------------------------------*/

void setclosure(QAction* w, void* value, void*)
{
   setelementstyle(w, (uintptr_t)value, UNCLOSED);
}

/*----------------------------------------------------------------*/

void setopaque(QAction* w, void* value, void*)
{
   setelementstyle(w, (uintptr_t)value, OPAQUE);
}
   
/*----------------------------------------------------------------*/

void setline(QAction* w, void* value, void*)
{
   setelementstyle(w, (uintptr_t)value, BORDERS);
}
   
/*-----------------------------------------------*/
/* Set the color value for all selected elements */
/*-----------------------------------------------*/

void setcolor(QAction* a, void* value_, void*)
{
    /* Get the color index value from the menu button widget itself */
    QMenu* colorMenu = qobject_cast<QMenu*>(a->parentWidget());
    int cindex = colorMenu->actions().indexOf(a)-3;

    unsigned int value = (uintptr_t)value_;
    short *scolor;
    int cval;
    bool selected = false;
    stringpart *strptr, *nextptr;

    if (value == 1 || cindex < -1)
        cindex = cval = -1;
    else {
        if (cindex < colorlist.count()) {
            cval = colorlist[cindex];
        } else {
            Wprintf("Error: No such color!");
            return;
        }
    }

    if (eventmode == TEXT_MODE || eventmode == ETEXT_MODE) {
        labelptr curlabel = TOLABEL(EDITPART);
        strptr = findstringpart(areawin->textpos - 1, NULL, curlabel->string,
                        areawin->topinstance);
        nextptr = findstringpart(areawin->textpos, NULL, curlabel->string,
                        areawin->topinstance);
        if (strptr->type == FONT_COLOR) {
            strptr->data.color = cindex;
        }
        else if (nextptr && nextptr->type == FONT_COLOR) {
            nextptr->data.color = cindex;
        }
        else {
            labeltext(FONT_COLOR, (char *)&cindex);
        }
    }

    else if (areawin->selects > 0) {
        for (scolor = areawin->selectlist; scolor < areawin->selectlist
           + areawin->selects; scolor++) {
            SELTOGENERIC(scolor)->color = cval;
            selected = true;
        }
    }

    setcolormark(cval);
    if (!selected) {
        if (eventmode != TEXT_MODE && eventmode != ETEXT_MODE)
            areawin->color = cval;
        overdrawpixmap(a);
    }

    unselect_all();
}

/*----------------------------------------------------------------*/
/* Generate popup dialog for adding a new color                   */
/*----------------------------------------------------------------*/

void addnewcolor(QAction*, void*, void*)
{
    QColor c = QColorDialog::getColor();
    if (c.isValid()) addnewcolorentry(c.rgb());
}

/*------------------------------------------------------*/

void setfontmarks(short fvalue, short jvalue)
{
   QAction* a;

   if ((fvalue >= 0) && (fontcount > 0)) {
      switch(fonts[fvalue].flags & 0x03) {
         case 0: a = menuAction("Style_Normal"); break;
         case 1: a = menuAction("Style_Bold"); break;
         case 2: a = menuAction("Style_Italic"); break;
         case 3: a = menuAction("Style_BoldItalic"); break;
      }
      toggleexcl(a);

      switch((fonts[fvalue].flags & 0xf80) >> 7) {
         case 0: a = menuAction("Encoding_Standard"); break;
         case 2: a = menuAction("Encoding_ISOLatin1"); break;
         default: a = NULL;
      }
      toggleexcl(a);
      togglefontmark(fvalue);
   }
   if (jvalue >= 0) {
      switch(jvalue & (RLJUSTFIELD)) {
         case NORMAL: a = menuAction("Justification_LeftJustified"); break;
         case NOTLEFT: a = menuAction("Justification_CenterJustified"); break;
         case RIGHT|NOTLEFT: a = menuAction("Justification_RightJustified"); break;
      }
      toggleexcl(a);

      switch(jvalue & (TBJUSTFIELD)) {
         case NORMAL: a = menuAction("Justification_BottomJustified"); break;
         case NOTBOTTOM: a = menuAction("Justification_MiddleJustified"); break;
         case TOP|NOTBOTTOM: a = menuAction("Justification_TopJustified"); break;
      }
      toggleexcl(a);

      /* Flip Invariance property */
      a = menuAction("Justification_FlipInvariant");
      a->setChecked(jvalue & FLIPINV);

      /* Pin visibility property */
      a = menuAction("Netlist_PinVisibility");
      a->setChecked(jvalue & PINVISIBLE);
   }
}

/*----------------------------------------------*/
/* GUI wrapper for startparam()			*/
/*----------------------------------------------*/

void promptparam(QAction* a, void*, void*)
{
   if (areawin->selects == 0) return;  /* nothing was selected */

   /* Get a name for the new object */
   eventmode = NORMAL_MODE;
   popupQuestion(a, "Enter name for new parameter:", "\0", stringparam);
}

/*---------------------------*/
/* Set polygon editing style */
/*---------------------------*/

void boxedit(QAction*, void* value_, void*)
{
    unsigned int value = (uintptr_t)value_;
    Q_UNUSED(value);
    /// \todo
#if 0
   if (w == NULL) {
      switch (value) {
         case MANHATTAN: w = PolygonEditManhattanBoxEditButton; break;
         case RHOMBOIDX: w = PolygonEditRhomboidXButton; break;
	 case RHOMBOIDY: w = PolygonEditRhomboidYButton; break;
	 case RHOMBOIDA: w = PolygonEditRhomboidAButton; break;
	 case NORMAL: w = PolygonEditNormalButton; break;
      }
   }

   if (areawin->boxedit == value) return;

   toggleexcl(w);
   areawin->boxedit = value;
#endif
}

/*----------------------------------------------------*/
/* Generate popup dialog for entering a new font name */
/*----------------------------------------------------*/

void addnewfont(QAction* a, void*, void*)
{
   popupQuestion(a, "Enter font name:", "", locloadfont);
}

/*-------------------------------------------------*/
/* Wrapper for labeltext when called from the menu */
/*-------------------------------------------------*/

void addtotext(QAction*, void* value, void*)
{
   if (eventmode != TEXT_MODE && eventmode != ETEXT_MODE) return;
   if (value == Number(SPECIAL))
      dospecial();
   else
      labeltext((intptr_t)value, (char *)1);
}

/*----------------------------------------------------------*/
/* Position a popup menu directly beside the toolbar button */
/*----------------------------------------------------------*/

void position_popup(Widget toolbutton, Widget menubutton)
{
   int n = 0;
   Arg wargs[2];
   Position pz, pw, ph;
   int dx, dy;

   Widget cascade = XtParent(menubutton);
   Widget pshell = XtParent(XtParent(cascade));

   XtnSetArg(XtNheight, &pz);
   XtGetValues(toolbutton, wargs, n); n = 0;

   XtnSetArg(XtNwidth, &pw);
   XtnSetArg(XtNheight, &ph);
   XtGetValues(cascade, wargs, n); n = 0;

   dx = -pw - 6;
   dy = (pz - ph) >> 1;

   XwPostPopup(pshell, cascade, toolbutton, dx, dy);
}

/*------------------------------------------------------*/
/* Functions which pop up a menu cascade in sticky mode */
/*------------------------------------------------------*/

void border_popup(QAction* a, void*, void*)
{
    QMenu::exec(menuAction("Elements_Border")->menu()->actions(), actionCenter(a));
}

/*-------------------------------------------------------------------------*/

void color_popup(QAction* a, void*, void*)
{
    QMenu::exec(menuAction("Elements_Color")->menu()->actions(), actionCenter(a), colormarkaction());
}

/*-------------------------------------------------------------------------*/

void fill_popup(QAction* a, void*, void*)
{
    QMenu::exec(menuAction("Elements_Fill")->menu()->actions(), actionCenter(a));
}

/*-------------------------------------------------------------------------*/

void param_popup(QAction* a, void*, void*)
{
    QMenu::exec(menuAction("Elements_Parameters")->menu()->actions(), actionCenter(a));
}

/*-------------------------------------------------------------------------*/
