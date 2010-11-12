#include "context.h"
#include "matrix.h"
#include "xctypes.h"
#include "xcircuit.h"
#include "prototypes.h"

Context::Context(QPainter * gc) :
        gccolor(0),
        gctype(0),
        gc_(gc),
        matStack(new Matrix)
{
    UMakeWCTM(DCTM());
}

void Context::setPainter(QPainter * gc)
{
    gc_ = gc;
}

void Context::UPopCTM()
{
    Matrix::pop(matStack);
}

void Context::UPushCTM()
{
    Matrix::push(matStack);
}

/*----------------------------------------------------------------------*/
/* Return scale relative to window					*/
/*----------------------------------------------------------------------*/

float Context::UTopScale() const
{
   return DCTM()->getScale();
}

/*----------------------------------------------------------------------*/
/* Return scale relative to the top-level schematic (not the window)	*/
/*----------------------------------------------------------------------*/

float Context::UTopDrawingScale() const
{
   Matrix lctm(*DCTM()), wctm;

   UMakeWCTM(&wctm);
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

int Context::UTopRotation() const
{
   return DCTM()->getRotation();
}

/*----------------------------------------------------------------------*/
/* Return scale multiplied by length					*/
/*----------------------------------------------------------------------*/

float Context::UTopTransScale(float length) const
{
   return length * UTopScale();
}

/*----------------------------------------------------------------------*/
/* Return position offset relative to top-level				*/
/*----------------------------------------------------------------------*/

void Context::UTopOffset(int *offx, int *offy) const
{
   DCTM()->getOffset(offx, offy);
}

/*----------------------------------------------------------------------*/
/* Return postion relative to the top-level schematic (not the window)	*/
/*----------------------------------------------------------------------*/

void Context::UTopDrawingOffset(int *offx, int *offy) const
{
   Matrix lctm(*DCTM()), wctm;
   UMakeWCTM(&wctm);
   wctm.invert();
   wctm.preMult(lctm);
   wctm.getOffset(offx, offy);
}

/*----------------------------------------------------------------------*/
/* Adjust justification and CTM as necessary for flip invariance	*/
/*----------------------------------------------------------------------*/

short Context::flipadjust(short justify)
{
   Matrix* const CTM = DCTM();
   short tmpjust = justify & (~FLIPINV);

   if (justify & FLIPINV) {
      if ((CTM->a < -EPS) || ((CTM->a < EPS) && (CTM->a > -EPS) &&
                ((CTM->d * CTM->b) < 0))) {
         if ((tmpjust & (RIGHT | NOTLEFT)) != NOTLEFT)
            tmpjust ^= (RIGHT | NOTLEFT);
      }
      if (CTM->e > EPS) {
         if ((tmpjust & (TOP | NOTBOTTOM)) != NOTBOTTOM)
            tmpjust ^= (TOP | NOTBOTTOM);
      }
      CTM->preScale();
   }
   return tmpjust;
}

