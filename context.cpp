#include "context.h"
#include "matrix.h"
#include "xctypes.h"
#include "xcircuit.h"
#include "prototypes.h"

DrawContext::DrawContext(QPainter * gc, const UIContext * uic) :
        gccolor(0),
        gctype(0),
        gc_(gc),
        ui(uic),
        ownUi(uic == NULL),
        matStack(new Matrix)
{
    if (ownUi) ui = new UIContext;
    DCTM()->makeWCTM();
}

DrawContext::~DrawContext()
{
    if (ownUi) delete ui;
}

void DrawContext::setPainter(QPainter * gc)
{
    gc_ = gc;
}

void DrawContext::UPopCTM()
{
    Matrix::pop(matStack);
}

void DrawContext::UPushCTM()
{
    Matrix::push(matStack);
}

/*----------------------------------------------------------------------*/
/* Return scale relative to window					*/
/*----------------------------------------------------------------------*/

float DrawContext::UTopScale() const
{
   return DCTM()->getScale();
}

/*----------------------------------------------------------------------*/
/* Return scale relative to the top-level schematic (not the window)	*/
/*----------------------------------------------------------------------*/

float DrawContext::UTopDrawingScale() const
{
   Matrix lctm(*DCTM()), wctm;

   wctm.makeWCTM();
   wctm.invert();
   wctm.preMult(lctm);
   return wctm.getScale();
}

/*----------------------------------------------------------------------*/
/* Return rotation relative to the top level				*/
/* Note that UTopRotation() is also the rotation relative to the window	*/
/* since the top-level drawing page is always upright relative to the	*/
/* window.  Thus, there is no routine UTopDrawingRotation().		*/
/*----------------------------------------------------------------------*/

int DrawContext::UTopRotation() const
{
   return DCTM()->getRotation();
}

/*----------------------------------------------------------------------*/
/* Return scale multiplied by length					*/
/*----------------------------------------------------------------------*/

float DrawContext::UTopTransScale(float length) const
{
   return length * UTopScale();
}

/*----------------------------------------------------------------------*/
/* Return position offset relative to top-level				*/
/*----------------------------------------------------------------------*/

void DrawContext::UTopOffset(int *offx, int *offy) const
{
   DCTM()->getOffset(offx, offy);
}

/*----------------------------------------------------------------------*/
/* Return postion relative to the top-level schematic (not the window)	*/
/*----------------------------------------------------------------------*/

void DrawContext::UTopDrawingOffset(int *offx, int *offy) const
{
   Matrix lctm(*DCTM()), wctm;
   wctm.makeWCTM();
   wctm.invert();
   wctm.preMult(lctm);
   wctm.getOffset(offx, offy);
}

/*----------------------------------------------------------------------*/
/* Adjust justification and CTM as necessary for flip invariance	*/
/*----------------------------------------------------------------------*/

short DrawContext::flipadjust(short justify)
{
   Matrix* const CTM = DCTM();
   short tmpjust = justify & (~FLIPINV);

   if (justify & FLIPINV) {
      if ((CTM->a() < -EPS) || ((CTM->a() < EPS) && (CTM->a() > -EPS) &&
                ((CTM->d() * CTM->b()) < 0))) {
         if ((tmpjust & (RIGHT | NOTLEFT)) != NOTLEFT)
            tmpjust ^= (RIGHT | NOTLEFT);
      }
      if (CTM->e() > EPS) {
         if ((tmpjust & (TOP | NOTBOTTOM)) != NOTBOTTOM)
            tmpjust ^= (TOP | NOTBOTTOM);
      }
      CTM->preScale();
   }
   return tmpjust;
}

