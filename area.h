#ifndef AREA_H
#define AREA_H

#include <QAbstractScrollArea>

class QPinchGesture;

class Area : public QAbstractScrollArea
{
    Q_OBJECT
public:
    explicit Area(QWidget *parent = 0);

protected:
    bool event(QEvent *);
    void paintEvent(QPaintEvent*);
    void resizeEvent(QResizeEvent*);
    void mousePressEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);
    void scrollContentsBy(int, int);
    void wheelEvent(QWheelEvent *);
    virtual void pinchEvent(QPinchGesture *);

public slots:
    void refresh();
    void zoom(const QPoint & center, float scale);
    void zoom(float scale);
    void zoomin();
    void zoomout();

protected slots:
    void on_corner_clicked();

private:
    bool ignoreScrolls;
    float initialScale;
};

#endif // AREA_H
