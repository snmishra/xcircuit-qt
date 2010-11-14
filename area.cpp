#include <QPainter>
#include <QScrollBar>
#include <QToolButton>
#include <QPinchGesture>

#include "area.h"
#include "matrix.h"
#include "context.h"
#include "xcircuit.h"
#include "prototypes.h"
#include "colors.h"
#include "xcqt.h"

Area::Area(QWidget *parent) :
    QAbstractScrollArea(parent),
    ignoreScrolls(false)
{
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    verticalScrollBar()->setInvertedAppearance(true);
    verticalScrollBar()->setInvertedControls(true);
    grabGesture(Qt::PinchGesture);
    setFocusPolicy(Qt::StrongFocus);
    setFocus();

    /* corner beauty button */
    QWidget * corner = new QToolButton();
    corner->setObjectName("corner");
    setCornerWidget(corner);

    QMetaObject::connectSlotsByName(this);
}

bool Area::event(QEvent * ev)
{
    if (ev->type() == QEvent::Gesture) {
        QGestureEvent *ge = (QGestureEvent*)ev;
        if (QPinchGesture *pinch = (QPinchGesture*)ge->gesture(Qt::PinchGesture)) {
            pinchEvent(pinch);
            return true;
        }
    }
    return QAbstractScrollArea::event(ev);
}

void Area::paintEvent(QPaintEvent*)
{
    QPainter p(viewport());
    p.setRenderHint(QPainter::Antialiasing, areawin->antialias);
    float x, y, spc, spc2, i, j, fpart;
    XPoint originpt;
    Context c(&p);
    areawin->markUpdated();

    if (xobjs.pagelist[areawin->page].background.name != (char *)NULL)
      copybackground(&c);

    SetThinLineAttributes(c.gc(), 0, LineSolid, CapRound, JoinBevel);

    p.fillRect(rect(), QColor(BACKGROUND));

    /* draw GRIDCOLOR lines for grid; mark axes in AXESCOLOR */

    if (eventmode != CATALOG_MODE && eventmode != ASSOC_MODE
        && eventmode != FONTCAT_MODE && eventmode != EFONTCAT_MODE
        && eventmode != CATMOVE_MODE && eventmode != CATTEXT_MODE) {

      float major_snapspace, spc3;

      spc = xobjs.pagelist[areawin->page].gridspace * areawin->vscale;
      if (areawin->gridon && spc > 8) {
         fpart = (float)(-areawin->pcorner.x)
                        / xobjs.pagelist[areawin->page].gridspace;
         x = xobjs.pagelist[areawin->page].gridspace *
                (fpart - (float)((int)fpart)) * areawin->vscale;
         fpart = (float)(-areawin->pcorner.y)
                        / xobjs.pagelist[areawin->page].gridspace;
         y = xobjs.pagelist[areawin->page].gridspace *
                (fpart - (float)((int)fpart)) * areawin->vscale;

         SetForeground(c.gc(), GRIDCOLOR);
         for (i = x; i < (float)areawin->width; i += spc)
            p.drawLine((int)(i + 0.5), 0, (int)(i + 0.5), areawin->height);
         for (j = (float)areawin->height - y; j > 0; j -= spc)
            p.drawLine(0, (int)(j - 0.5), areawin->width, (int)(j - 0.5));
      };
      if (areawin->axeson) {
         XPoint zeropt;
         zeropt.x = zeropt.y = 0;
         SetForeground(c.gc(), AXESCOLOR);
         user_to_window(zeropt, &originpt);
         p.drawLine(originpt.x, 0, originpt.x, areawin->height);
         p.drawLine(0, originpt.y, areawin->width, originpt.y);
      }

      /* bounding box goes beneath everything except grid/axis lines */
      UDrawBBox(&c);

      /* draw a little red dot at each snap-to point */

      spc2 = xobjs.pagelist[areawin->page].snapspace * areawin->vscale;
      if (areawin->snapto && spc2 > 8) {
         float x2, y2;

         fpart = (float)(-areawin->pcorner.x)
                        / xobjs.pagelist[areawin->page].snapspace;
         x2 = xobjs.pagelist[areawin->page].snapspace *
                (fpart - (float)((int)fpart)) * areawin->vscale;
         fpart = (float)(-areawin->pcorner.y)
                        / xobjs.pagelist[areawin->page].snapspace;
         y2 = xobjs.pagelist[areawin->page].snapspace *
                (fpart - (float)((int)fpart)) * areawin->vscale;

         SetForeground(c.gc(), SNAPCOLOR);
         for (i = x2; i < areawin->width; i += spc2)
            for (j = areawin->height - y2; j > 0; j -= spc2)
               p.drawPoint((int)(i + 0.5), (int)(j - 0.5));
      };

      /* Draw major snap points (code contributed by John Barry) */

      major_snapspace = xobjs.pagelist[areawin->page].gridspace * 20;
      spc3 = major_snapspace * areawin->vscale;
      if (spc > 4) {
         fpart = (float)(-areawin->pcorner.x) / major_snapspace;
         x = major_snapspace * (fpart - (float)((int)fpart)) * areawin->vscale;
         fpart = (float)(-areawin->pcorner.y) / major_snapspace;
         y = major_snapspace * (fpart - (float)((int)fpart)) * areawin->vscale;

         SetForeground(c.gc(), GRIDCOLOR);
         for (i = x; i < (float)areawin->width; i += spc3) {
            for (j = (float)areawin->height - y; j > 0; j -= spc3) {
                p.drawEllipse((int)(i + 0.5) - 1, (int)(j - 0.5) - 1, 2, 2);
            }
         }
      }

      /* Determine the transformation matrix for the topmost object */
      /* and draw the hierarchy above the current edit object (if   */
      /* "edit-in-place" is selected).				    */

      if (areawin->editinplace) {
         if (areawin->stack != NULL) {
            pushlistptr lastlist = NULL, thislist;

            c.UPushCTM();	/* save our current state */

            /* It's easiest if we first push the current page onto the stack, */
            /* then we don't need to treat the top-level page separately.  We */
            /* pop it at the end.					      */
            push_stack(&areawin->stack, areawin->topinstance);

            thislist = areawin->stack;

            while ((thislist != NULL) &&
                        (is_library(thislist->thisinst->thisobject) < 0)) {

               /* Invert the transformation matrix of the instance on the stack */
               /* to get the proper transformation matrix of the drawing one	*/
               /* up in the hierarchy.						*/

               Matrix mtmp;
               mtmp.preMult(thislist->thisinst->position,
                        thislist->thisinst->scale, thislist->thisinst->rotation);
               mtmp.invert();
               c.CTM().preMult(mtmp);

               lastlist = thislist;
               thislist = thislist->next;

               /* The following will be true for moves between schematics and symbols */
               if ((thislist != NULL) && (thislist->thisinst->thisobject->symschem
                        == lastlist->thisinst->thisobject))
                  break;
            }

            if (lastlist != NULL) {
               pushlistptr stack = NULL;
               SetForeground(c.gc(), OFFBUTTONCOLOR);
               UDrawObject(&c, lastlist->thisinst, SINGLE, DOFORALL, &stack);
               /* This shouldn't happen, but just in case. . . */
               free_stack(&stack);
            }

            pop_stack(&areawin->stack); /* restore the original stack state */
            c.UPopCTM();			  /* restore the original matrix state */
         }
      }
    }

    /* draw all of the elements on the screen */

    SetForeground(c.gc(), FOREGROUND);

    /* Initialize hierstack */
    free_stack(&areawin->hierstack);
    UDrawObject(&c, areawin->topinstance, TOPLEVEL, FOREGROUND, &areawin->hierstack);
    free_stack(&areawin->hierstack);

    /* draw the highlighted netlist, if any */
    if (checkvalid(topobject) != -1)
      if (topobject->highlight.netlist != NULL)
         highlightnetlist(&c, topobject, areawin->topinstance);

    /* draw selected elements in the SELECTION color */
    /* special case---single label partial text selected */

    if ((areawin->selects == 1) && SELECTTYPE(areawin->selectlist) == LABEL
                && areawin->textend > 0 && areawin->textpos > areawin->textend) {
      labelptr drawlabel = SELTOLABEL(areawin->selectlist);
      UDrawString(&c, drawlabel, DOSUBSTRING, areawin->topinstance);
    }
    else
      draw_all_selected(&c);

    /* draw pending elements, if any */

    if (eventmode != NORMAL_MODE) {
      SetFunction(c.gc(), GXcopy);
      if (eventmode == TEXT_MODE) {
         labelptr newlabel = TOLABEL(EDITPART);
         UDrawString(&c, newlabel, newlabel->color, areawin->topinstance);
         UDrawTLine(&c, newlabel);
      }
      else if (eventmode == ETEXT_MODE || eventmode == CATTEXT_MODE) {
         labelptr newlabel = TOLABEL(EDITPART);
         UDrawTLine(&c, newlabel);
      }
      else if (eventmode == SELAREA_MODE) {
         UDrawBox(&c, areawin->origin, areawin->save);
      }
      else if (eventmode == RESCALE_MODE) {
         UDrawRescaleBox(&c, areawin->save);
      }
      else {
         SetFunction(c.gc(), GXxor);
         if (eventmode == BOX_MODE || eventmode == WIRE_MODE) {
            polyptr newpoly = TOPOLY(EDITPART);
            XSetXORFg(&c, newpoly->color, BACKGROUND);
            newpoly->draw(&c);
         }
         else if (eventmode == ARC_MODE || eventmode == EARC_MODE) {
            arcptr newarc = TOARC(EDITPART);
            XSetXORFg(&c, newarc->color, BACKGROUND);
            newarc->draw(&c);
            UDrawXLine(&c, areawin->save, newarc->position);
         }
         else if (eventmode == SPLINE_MODE) {
            splineptr newspline = TOSPLINE(EDITPART);
            XSetXORFg(&c, newspline->color, BACKGROUND);
            newspline->draw(&c);
         }
      }
    }
}

void Area::resizeEvent(QResizeEvent* ev)
{
    areawin->width = ev->size().width();
    areawin->height = ev->size().height();

    /* Re-compose the directories to match the new dimensions */
    composelib(LIBLIB);
    composelib(PAGELIB);

    /* Re-center image in resized window */
    zoomview(NULL, NULL, NULL);
}

void Area::mousePressEvent(QMouseEvent * ev)
{
    XKeyEvent kev(*ev);
    keyhandler(viewport(), NULL, &kev);
}

void Area::mouseReleaseEvent(QMouseEvent * ev)
{
    XKeyEvent kev(*ev);
    keyhandler(viewport(), NULL, &kev);
}

void Area::scrollContentsBy(int, int)
{
    if (ignoreScrolls || eventmode == SELAREA_MODE) return;
    areawin->pcorner.x = horizontalScrollBar()->value()/areawin->vscale + topobject->bbox.lowerleft.x;
    areawin->pcorner.y = verticalScrollBar()->value()/areawin->vscale + topobject->bbox.lowerleft.y;
    viewport()->update();
}

//

static const int scrollMargin = 10;

void Area::refresh()
{
    const int fullWidth = topobject->bbox.width * areawin->vscale;
    const int pageStepH = areawin->width;
    const int maxH = fullWidth - pageStepH + scrollMargin;
    const int minH = -scrollMargin;
    const int valH = (areawin->pcorner.x - topobject->bbox.lowerleft.x) * areawin->vscale;

    const int fullHeight = topobject->bbox.height * areawin->vscale;
    const int pageStepV = areawin->height;
    const int maxV = fullHeight - pageStepH + scrollMargin;
    const int minV = -scrollMargin;
    const int valV = (areawin->pcorner.y - topobject->bbox.lowerleft.y) * areawin->vscale;

    ignoreScrolls = true;

    QScrollBar * hbar = horizontalScrollBar();
    hbar->setPageStep(pageStepH);
    hbar->setMinimum(minH);
    hbar->setMaximum(maxH);
    hbar->setValue(valH);

    QScrollBar* vbar = verticalScrollBar();
    vbar->setPageStep(pageStepV);
    vbar->setMinimum(minV);
    vbar->setMaximum(maxV);
    vbar->setValue(valV);

    ignoreScrolls = false;
    viewport()->update();
    printname(topobject);
}

/// \param center is the zoom centerpoint in the viewport()
/// \param scale is the desired areawin->vscale
void Area::zoom(const QPoint & center, float scale)
{
    void postzoom(); // events.cpp

    float savescale;
    XPoint ucenter, ncenter, savell;
    XlPoint newll; // used to check for overflow

    savescale = areawin->vscale;
    savell = areawin->pcorner;

    // preserves the on-screen coordinates of center point
    window_to_user(center.x(), center.y(), &ucenter);
    areawin->vscale = scale;
    window_to_user(center.x(), center.y(), &ncenter);
    newll = areawin->pcorner;
    newll += ucenter;
    newll -= ncenter;
    areawin->pcorner = newll;
    window_to_user(center.x(), center.y(), &ncenter);
    if (false) qDebug("zoom dx=%d dy=%d", ncenter.x-ucenter.x, ncenter.y-ucenter.y);

    // check for out of bounds and for overflow
    Context ctx(NULL);
    if (checkbounds(&ctx) == -1 || newll.x != areawin->pcorner.x || newll.y != areawin->pcorner.y) {
        areawin->pcorner.x = savell.x;
        areawin->pcorner.y = savell.y;
        areawin->vscale = savescale;
        if (checkbounds(&ctx) == -1) {
            /* this is a rare case where an object gets out-of-bounds */
            Wprintf("Cannot scale further. Delete out-of-bounds object!");
        } else {
            Wprintf("Cannot scale further");
        }
    }
    else {
        postzoom();
        refresh();
    }
}

void Area::zoom(float scale)
{
    QPoint p = viewport()->mapFromGlobal(QCursor::pos());
    if (false && ! viewport()->rect().contains(p)) {
        p.setX(areawin->width / 2);
        p.setY(areawin->height / 2);
    }
    zoom(p, scale);
}

void Area::zoomin()
{
    zoom(areawin->vscale * areawin->zoomfactor);
}

void Area::zoomout()
{
    zoom(areawin->vscale / areawin->zoomfactor);
}

//

void Area::on_corner_clicked()
{
    zoomview(NULL, NULL, NULL);
}

void Area::wheelEvent(QWheelEvent * ev)
{
    const double wheelScale = 1/400.0;
    if (false) qDebug("wheel event: %s by %d", ev->orientation() == Qt::Horizontal ? "hor" : "ver", ev->delta());
    if (ev->orientation() == Qt::Horizontal) {
        const int delta = areawin->width * wheelScale * ev->delta();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta);
    } else {
        const int delta = areawin->height * wheelScale * ev->delta();
        verticalScrollBar()->setValue(verticalScrollBar()->value() + delta);
    }
}

void Area::pinchEvent(QPinchGesture * pinch)
{
    const float eps = 1E-6;
    if (false) qDebug("pinch %f -> %f", pinch->lastScaleFactor(), pinch->scaleFactor());
    if ((pinch->lastScaleFactor() - 1.0) < eps) {
        // we begin a pinch
        initialScale = areawin->vscale;
    }
    zoom(initialScale * pinch->scaleFactor());
}
