#ifndef CONTEXT_H
#define CONTEXT_H

#include <qglobal.h>
#include <QVector>

class QPainter;
class Matrix;
class UIContext;

class DrawContext
{
public:
    DrawContext(QPainter *, const UIContext * ui = NULL);
    ~DrawContext();
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
    const UIContext* ui;
    bool ownUi;
    Matrix* matStack;
    Q_DISABLE_COPY(DrawContext)
};

class UIContext
{
public:
    QVector<short> selects;
};

#endif // CONTEXT_H
