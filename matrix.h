#ifndef MATRIX_H
#define MATRIX_H

#include <qglobal.h>

#include "xctypes.h"

/*----------------------------------------------------------------------*/
/* Transformation matrices						*/
/* Works like this (see also PostScript reference manual):		*/
/*   [x' y' 1] = [x y 1] * | a  d  0 |					*/
/*			   | b  e  0 |					*/
/*			   | c  f  1 |					*/
/*----------------------------------------------------------------------*/

class Matrix {
public:
    Matrix();
    Matrix(const Matrix&);
    void reset();
    static void pop(Matrix*&);
    static void push(Matrix*&);

    float getScale() const;
    int getRotation() const;
    XPoint getOffset() const;
    void getOffset(int* x, int* y) const;

    void invert();
    void slant(float beta);
    void preMult(const Matrix&);
    void preMult(XPoint, float, short);
    void mult(XPoint, float, short);
    void preScale();

    void transform(const XPoint *ipoints, XPoint *points, short number) const;
    void transform(const XfPoint *fpoints, XPoint *points, short number) const;

public:
    float a, b, c, d, e, f;
private:
    Matrix* next;
    Matrix& operator=(const Matrix&);
};
typedef Matrix* Matrixptr;

static const float EPS = 1e-9;

void UMakeWCTM(Matrix*);
void UTransformPoints(XPoint *, XPoint *, short, XPoint, float, short);
void InvTransformPoints(XPoint *, XPoint *, short, XPoint, float, short);

#endif // MATRIX_H
