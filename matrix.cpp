#include <cmath>

#include "matrix.h"
#include "xctypes.h"
#include "xcircuit.h"
#include "prototypes.h"

Matrix::Matrix() :
        QTransform(),
        next(NULL)
{
}

Matrix::Matrix(const Matrix & src) :
        QTransform(src),
        next(NULL)
{
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
    Matrix* nmatrix;
    if (stack) nmatrix = new Matrix(*stack);
    else nmatrix = new Matrix;
    nmatrix->next = stack;
    stack = nmatrix;
}

float Matrix::getScale() const
{
    return (float)sqrt(a() * a() + d() * d());
}

int Matrix::getRotation() const
{
    double rads = atan2(d(), a());
    return (int)(rads / RADFAC);
}

XPoint Matrix::getOffset() const
{
    return XPoint(c(), f());
}

void Matrix::getOffset(int *x, int *y) const
{
    if (x) *x = c();
    if (y) *y = f();
}

void Matrix::invert()
{
    *static_cast<QTransform*>(this) = inverted();
}

/*----------------------------------------------------------------------*/
/* Slanting function x' = x + beta * y, y' = y				*/
/*----------------------------------------------------------------------*/

void Matrix::slant(float beta)
{
    shear(beta, 0.0);
}

void Matrix::preMult(const Matrix & pre)
{
    *static_cast<QTransform*>(this) =
            static_cast<const QTransform &>(pre) * static_cast<QTransform>(*this);
}

void Matrix::preMult(XPoint position, float scale, short rotate)
{
   float tmpa, tmpb, tmpd, tmpe, yscale;
   double drot = (double)rotate * RADFAC;

   yscale = fabs(scale);		/* negative scale value implies flip in x only */

   tmpa =  scale * cos(drot);
   tmpb = yscale * sin(drot);
   tmpd = -scale * sin(drot);
   tmpe = yscale * cos(drot);

   set(
           a() * tmpa + b() * tmpd,
           a() * tmpb + b() * tmpe,
           c() + a() * position.x + b() * position.y,
           d() * tmpa + e() * tmpd,
           d() * tmpb + e() * tmpe,
           f() + d() * position.x + e() * position.y);
}

void Matrix::mult(XPoint position, float scale, short rotate)
{
   float tmpa, tmpb, tmpd, tmpe, yscale;
   double drot = (double)rotate * RADFAC;

   yscale = fabs(scale);  /* -scale implies flip in x direction only */

   tmpa =  scale * cos(drot);
   tmpb = yscale * sin(drot);
   tmpd = -scale * sin(drot);
   tmpe = yscale * cos(drot);

   set(
           a() * tmpa + d() * tmpb,
           b() * tmpa + e() * tmpb,
           c() * tmpa + f() * tmpb + position.x,
           d() * tmpe + a() * tmpd,
           e() * tmpe + b() * tmpd,
           f() * tmpe + c() * tmpd + position.y);
}

/*----------------------------------------------------------------------*/
/* Transform text to make it right-side up within 90 degrees of page	*/
/* NOTE:  This is not yet resolved, as xcircuit does not agree with	*/
/* PostScript in a few cases!						*/
/*----------------------------------------------------------------------*/

void Matrix::preScale()
{
   /* negative X scale (-1, +1) */
   if ((a() < -EPS) || (a() < EPS && a() > -EPS &&
                ((d() * b()) < 0))) {
       scale(-1.0, 1.0);
   }

   /* negative Y scale (+1, -1) */
   if (e() > EPS) {
       scale(1.0, -1.0);
   }

   /* At 90, 270 degrees need special attention to avoid discrepencies	*/
   /* with the PostScript output due to roundoff error.  This code	*/
   /* matches what PostScript produces.					*/
}

void Matrix::transform(const XPoint *ipoints, XPoint *points, short number) const
{
    const XPoint *in = ipoints;
    XPoint *out = points;

    for (; in < ipoints + number; ++in, ++out) {
        *out = map(*in);
    }
}

void Matrix::transform(const XfPoint *fpoints, XPoint *points, short number) const
{
   const XfPoint * in = fpoints;
   XPoint *out = points;

   for (; in < fpoints + number; ++in, ++out) {
       *out = map(*in).toPoint();
   }
}

void Matrix::set(float a, float b, float c, float d, float e, float f)
{
    setMatrix(a, d, 0.0, b, e, 0.0, c, f, 1.0);
}

/*-------------------------------------------------------------------------*/
/* Multiply CTM by current screen position and scale to get transformation */
/* matrix from a user point to the X11 window				   */
/*-------------------------------------------------------------------------*/

void Matrix::makeWCTM()
{
    float A,B,C,D,E,F;
    A = a() * areawin->vscale;
    B = b() * areawin->vscale;
    C = (c() - (float)areawin->pcorner.x) * areawin->vscale;

    D = d() * -areawin->vscale;
    E = e() * -areawin->vscale;
    F = (float)areawin->height() + ((float)areawin->pcorner.y - f()) *
            areawin->vscale;
    set(A, B, C, D, E, F);
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
