#ifndef CONTEXT_H
#define CONTEXT_H

#include <qglobal.h>

class QPainter;
class Matrix;

class Context
{
public:
    Context(QPainter *);
    void setPainter(QPainter *);
    inline QPainter* gc() const { return gc_; }
    inline Matrix* DCTM() const { return matStack; }
    inline Matrix& CTM() const { return *matStack; }

    void UPopCTM();
    void UPushCTM();
    float UTopScale() const;
    float UTopDrawingScale() const;
    int UTopRotation() const;
    float UTopTransScale(float length) const;
    void UTopOffset(int *offx, int *offy) const;
    void UTopDrawingOffset(int *offx, int *offy) const;
    short flipadjust(short justify);

    // hack purgatory
    int gccolor, gctype;
private:
    QPainter* gc_;
    Matrix* matStack;
    Q_DISABLE_COPY(Context)
};

#endif // CONTEXT_H
