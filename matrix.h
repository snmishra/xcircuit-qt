#ifndef MATRIX_H
#define MATRIX_H

#include <qglobal.h>
#include <QTransform>

#include "xctypes.h"

/*----------------------------------------------------------------------*/
/* Transformation matrices						*/
/* Works like this (see also PostScript reference manual):		*/
/*   [x' y' 1] = [x y 1] * | a  d  0 |					*/
/*			   | b  e  0 |					*/
/*			   | c  f  1 |					*/
/*----------------------------------------------------------------------*/

class Matrix : public QTransform {
public:
    Matrix();
    Matrix(const Matrix&);
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
    void set(float a, float b, float c, float d, float e, float f);

    void makeWCTM();

    inline float a() const { return m11(); }
    inline float b() const { return m21(); }
    inline float c() const { return m31(); }
    inline float d() const { return m12(); }
    inline float e() const { return m22(); }
    inline float f() const { return m32(); }

private:
    Matrix* next;
};

static const float EPS = 1e-9;

void UTransformPoints(XPoint *, XPoint *, short, XPoint, float, short);
void InvTransformPoints(XPoint *, XPoint *, short, XPoint, float, short);

#endif // MATRIX_H
