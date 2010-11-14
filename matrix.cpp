#include <cmath>

#include "matrix.h"
#include "xctypes.h"
#include "xcircuit.h"
#include "prototypes.h"

Matrix::Matrix() :
        a(1.0),
        b(0.0),
        c(0.0),
        d(0.0),
        e(1.0),
        f(0.0),
        next(NULL)
{
}

Matrix::Matrix(const Matrix & src) :
        a(src.a),
        b(src.b),
        c(src.c),
        d(src.d),
        e(src.e),
        f(src.f),
        next(NULL)
{
}

void Matrix::reset()
{
   a = e = 1.0;
   b = c = d = f = 0.0;
}

void Matrix::pop(Matrix*& stack)
{
    if (!stack) {
        Wprintf("Matrix stack pop error");
        return;
    }
    Matrix* last = stack->next;
    delete stack;
    stack = last;
}

void Matrix::push(Matrix *& stack)
{
    Matrixptr nmatrix;
    if (stack) nmatrix = new Matrix(*stack);
    else nmatrix = new Matrix;
    nmatrix->next = stack;
    stack = nmatrix;
}

float Matrix::getScale() const
{
    return (float)sqrt(a * a + d * d);
}

int Matrix::getRotation() const
{
    double rads = atan2(d, a);
    return (int)(rads / RADFAC);
}

XPoint Matrix::getOffset() const
{
    return XPoint(c, f);
}

void Matrix::getOffset(int *x, int *y) const
{
    if (x) *x = c;
    if (y) *y = f;
}

void Matrix::invert()
{
   float det = a * e - b * d;
   float tx = b * f - c * e;
   float ty = d * c - a * f;

   float tmpa = a;

   b = -b / det;
   d = -d / det;

   a = e / det;
   e = tmpa / det;
   c = tx / det;
   f = ty / det;
}

/*----------------------------------------------------------------------*/
/* Slanting function x' = x + beta * y, y' = y				*/
/*----------------------------------------------------------------------*/

void Matrix::slant(float beta)
{
   b += a * beta;
   e += d * beta;
}

void Matrix::preMult(const Matrix & pre)
{
   float mata, matd;

   mata = pre.a * a + pre.d * b;
   c += pre.c * a + pre.f * b;
   b = pre.b * a + pre.e * b;
   a = mata;

   matd = pre.a * d + pre.d * e;
   f += pre.c * d + pre.f * e;
   e = pre.b * d + pre.e * e;
   d = matd;
}

void Matrix::preMult(XPoint position, float scale, short rotate)
{
   float tmpa, tmpb, tmpd, tmpe, yscale;
   float mata, matd;
   double drot = (double)rotate * RADFAC;

   yscale = fabs(scale);		/* negative scale value implies flip in x only */

   tmpa =  scale * cos(drot);
   tmpb = yscale * sin(drot);
   tmpd = -scale * sin(drot);
   tmpe = yscale * cos(drot);

   c += a * position.x + b * position.y;
   f += d * position.x + e * position.y;

   mata = a * tmpa + b * tmpd;
   b = a * tmpb + b * tmpe;

   matd = d * tmpa + e * tmpd;
   e = d * tmpb + e * tmpe;

   a = mata;
   d = matd;
}

void Matrix::mult(XPoint position, float scale, short rotate)
{
   float tmpa, tmpb, tmpd, tmpe, yscale;
   float mata, matb, matc;
   double drot = (double)rotate * RADFAC;

   yscale = fabs(scale);  /* -scale implies flip in x direction only */

   tmpa =  scale * cos(drot);
   tmpb = yscale * sin(drot);
   tmpd = -scale * sin(drot);
   tmpe = yscale * cos(drot);

   mata = a * tmpa + d * tmpb;
   matb = b * tmpa + e * tmpb;
   matc = c * tmpa + f * tmpb + position.x;

   d = d * tmpe + a * tmpd;
   e = e * tmpe + b * tmpd;
   f = f * tmpe + c * tmpd + position.y;

   a = mata;
   b = matb;
   c = matc;
}

/*----------------------------------------------------------------------*/
/* Transform text to make it right-side up within 90 degrees of page	*/
/* NOTE:  This is not yet resolved, as xcircuit does not agree with	*/
/* PostScript in a few cases!						*/
/*----------------------------------------------------------------------*/

void Matrix::preScale()
{
   /* negative X scale (-1, +1) */
   if ((a < -EPS) || ((a < EPS) && (a > -EPS) &&
                ((d * b) < 0))) {
      a = -a;
      d = -d;
   }

   /* negative Y scale (+1, -1) */
   if (e > EPS) {
      e = -e;
      b = -b;
   }

   /* At 90, 270 degrees need special attention to avoid discrepencies	*/
   /* with the PostScript output due to roundoff error.  This code	*/
   /* matches what PostScript produces.					*/
}

void Matrix::transform(const XPoint *ipoints, XPoint *points, short number) const
{
   const XPoint *current;
   XPoint *ptptr = points;
   float fx, fy;

   for (current = ipoints; current < ipoints + number; current++, ptptr++) {
      fx = a * (float)current->x + b * (float)current->y + c;
      fy = d * (float)current->x + e * (float)current->y + f;

      ptptr->x = (fx >= 0) ? (short)(fx + 0.5) : (short)(fx - 0.5);
      ptptr->y = (fy >= 0) ? (short)(fy + 0.5) : (short)(fy - 0.5);
   }
}

void Matrix::transform(const XfPoint *fpoints, XPoint *points, short number) const
{
   const XfPoint * current;
   XPoint *newlist = points;
   float fx, fy;

   for (current = fpoints; current < fpoints + number; current++, newlist++) {
      fx = a * current->x + b * current->y + c;
      fy = d * current->x + e * current->y + f;
      newlist->x = (fx >= 0) ? (short)(fx + 0.5) : (short)(fx - 0.5);
      newlist->y = (fy >= 0) ? (short)(fy + 0.5) : (short)(fy - 0.5);
   }
}

/*------------------------------------------------------------------------*/

void UTransformPoints(XPoint *points, XPoint *newpoints, short number,
        XPoint atpt, float scale, short rotate)
{
   Matrix LCTM;

   LCTM.mult(atpt, scale, rotate);
   LCTM.transform(points, newpoints, number);
}

/*----------------------------------------------------*/
/* Transform points inward to next hierarchical level */
/*----------------------------------------------------*/

void InvTransformPoints(XPoint *points, XPoint *newpoints, short number,
        XPoint atpt, float scale, short rotate)
{
   Matrix LCTM;

   LCTM.preMult(atpt, scale, rotate);
   LCTM.invert();
   LCTM.transform(points, newpoints, number);
}

/*-------------------------------------------------------------------------*/
/* Multiply CTM by current screen position and scale to get transformation */
/* matrix from a user point to the X11 window				   */
/*-------------------------------------------------------------------------*/

void UMakeWCTM(Matrix *ctm)
{
   ctm->a *= areawin->vscale;
   ctm->b *= areawin->vscale;
   ctm->c = (ctm->c - (float)areawin->pcorner.x) * areawin->vscale;

   ctm->d *= -areawin->vscale;
   ctm->e *= -areawin->vscale;
   ctm->f = (float)areawin->height() + ((float)areawin->pcorner.y - ctm->f) *
            areawin->vscale;
}
