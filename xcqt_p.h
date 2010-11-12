#ifndef XCQT_P_H
#define XCQT_P_H

#include <QBasicTimer>
#include <QObject>
#include "xcqt.h"

// used for menu, toolbar and enter callbacks only
class ActionCallbackObject : public QObject
{
    Q_OBJECT
public:
    ActionCallbackObject(QAction * obj, ActionCallback aproc, void* d);
public slots:
    void callback();
    void callback(void * data);
private:
    QAction * obj;
    ActionCallback function;
    void* data;
};

class WidgetCallbackObject : public QObject
{
    Q_OBJECT
public:
    WidgetCallbackObject(QWidget * obj, WidgetCallback wproc, void* d);
public slots:
    void callback();
    void callback(void * data);
    void callback(int);
private:
    QWidget * obj;
    WidgetCallback function;
    void* data;
};

// used for timer callbacks
class TimerObject : public QObject
{
    Q_OBJECT
public:
    TimerObject(unsigned long interval, XtTimerCallbackProc proc, XtPointer d);
    inline int id() const { return timer.timerId(); }
private:
    void timerEvent(QTimerEvent*);

    QBasicTimer timer;
    XtTimerCallbackProc proc;
    XtPointer data;
};

#endif // XCQT_P_H
