#include <QPalette>
#include <QBitmap>
#include <QVariant>
#include <QLabel>
#include <QScrollBar>
#include <QToolButton>
#include <QGridLayout>
#include <QBoxLayout>
#include <QScrollBar>
#include <QPushButton>
#include <QToolBar>
#include <QMenuBar>
#include <QAction>
#include <QStack>
#include <QStyle>
#include <QByteArray>
#include <QMap>
#include <QMapIterator>
#include <QLineEdit>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>

#include "area.h"
#include "xcqt.h"
#include "xcqt_p.h"
#include "xcircuit.h"
#include "menus.h"
#include "prototypes.h"
#include "pixmaps.h"
#include "colors.h"

extern ApplicationData appdata;

QWidget *top;
QLabel *wschema, *wsymb, *message1, *message2, *message3;
QToolBar *toolbar;
QWidget *overlay = NULL;

enum { STIPPLES = 8 }; /* Number of predefined stipple patterns		*/

static QPixmap * STIPPLE[STIPPLES*2];  /* Polygon fill-style stipple patterns, first transparent then opaque */

static char STIPDATA[STIPPLES][5] = {
   "\000\004\000\001",
   "\000\005\000\012",
   "\001\012\005\010",
   "\005\012\005\012",
   "\016\005\012\007",
   "\017\012\017\005",
   "\017\012\017\016",
   "\000\000\000\000"
};

//
// CallbackObject for menu/toolbar building
//

WidgetCallbackObject::WidgetCallbackObject(QWidget * w, WidgetCallback proc, XtPointer d) :
        QObject(w), obj(w), function(proc), data(d)
{}

void WidgetCallbackObject::callback()
{
    function(obj, data, NULL);
}

void WidgetCallbackObject::callback(void * calldata)
{
    function(obj, data, calldata);
}

void WidgetCallbackObject::callback(int calldata)
{
    function(obj, data, (void*)calldata);
}

ActionCallbackObject::ActionCallbackObject(QAction * a, ActionCallback proc, XtPointer d) :
        QObject(a), obj(a), function(proc), data(d)
{
    connect(a, SIGNAL(triggered()), SLOT(callback()));
}

void ActionCallbackObject::callback()
{
    function(obj, data, NULL);
}

void ActionCallbackObject::callback(void * calldata)
{
    function(obj, data, calldata);
}

//
// Xt-style Action Callbacks
//

void XtAddCallback(QAction * a, ActionCallback proc, XtPointer data)
{
    if (proc) new ActionCallbackObject(a, proc, data);
}

void XtRemoveAllCallbacks(QAction * a)
{
    foreach (ActionCallbackObject* o, a->findChildren<ActionCallbackObject*>()) {
        delete o;
    }
}

//
// Xt-style Widget Callbacks
//

class CallbackFilter : public QObject
{
public:
    CallbackFilter() : QObject() {}
protected:
    bool eventFilter(QObject *, QEvent *);
};

struct Callback {
    WidgetCallback proc;
    XtPointer data;
    inline Callback() : proc(NULL), data(NULL) {}
    inline Callback(WidgetCallback p, void* d) : proc(p), data(d) {}
    inline Callback(const QVariant & v) { *this = v.value<Callback>(); }
    operator QVariant() { return QVariant::fromValue(*this); }
};
Q_DECLARE_METATYPE(Callback)

void XtAddCallback(QWidget * w, String name, WidgetCallback proc, XtPointer data)
{
    Q_ASSERT(w != NULL);
    if (strcmp(name, XtNexecute) == 0) {
        QObject * co = new WidgetCallbackObject(w, proc, data);
        co->connect(w, SIGNAL(returnPressed()), SLOT(callback()));
        return;
    }
    static CallbackFilter * filter  = new CallbackFilter();
    QByteArray prop("callbacks+");
    prop.append(name);
    Callback c(proc, data);
    QVariant callbacks = w->property(prop);
    if (! callbacks.isValid()) callbacks.setValue(QList<QVariant>());
    QList<QVariant> list = callbacks.toList();
    list.append(c);
    w->setProperty(prop, list);
    if (! w->property("hasCallbacks").isValid()) {
        w->installEventFilter(filter);
        w->setProperty("hasCallbacks", true);
    }
}

void XtRemoveCallback(Widget w, String name, WidgetCallback proc, XtPointer data)
{
    Q_ASSERT(w != NULL);
    if (strcmp(name, XtNexecute) == 0) {
        foreach (WidgetCallbackObject* o, w->findChildren<WidgetCallbackObject*>()) {
            delete o;
        }
        return;
    }
    QByteArray prop("callbacks+");
    prop.append(name);
    QVariant callbacks = w->property(prop);
    if (! callbacks.isValid()) return;
    QList<QVariant> list = callbacks.toList();
    for (int i = 0; i < list.count(); ) {
        // remove only when both proc and data match
        Callback c = list[i];
        if (c.data == data && c.proc == proc) {
            list.removeAt(i);
        } else {
            ++ i;
        }
    }
    w->setProperty(prop, list);
}

void XtRemoveAllCallbacks(Widget w, String name)
{
    Q_ASSERT(w != NULL);
    if (strcmp(name, XtNexecute) == 0) {
        foreach (WidgetCallbackObject* o, w->findChildren<WidgetCallbackObject*>()) {
            delete o;
        }
        return;
    }
    QByteArray prop("callbacks+");
    prop.append(name);
    w->setProperty(prop, QVariant());
}

bool CallbackFilter::eventFilter(QObject * src, QEvent * event)
{
    if (! src->isWidgetType()) return QObject::eventFilter(src, event);
    QWidget * w = qobject_cast<QWidget*>(src);

    bool handled = false;

    static QMultiMap<QEvent::Type, const char *> evmap;
    static QMultiMap<const char *, QEvent::Type> revmap;

    if (evmap.count() == 0) {
        evmap.insert(QEvent::MouseMove, "callbacks+" XtNdrag);
        evmap.insert(QEvent::Resize, "callbacks+" XtNresize);
        evmap.insert(QEvent::Paint, "callbacks+" XtNexpose);
        evmap.insert(QEvent::KeyPress, "callbacks+" XtNkeyDown);
        evmap.insert(QEvent::KeyRelease, "callbacks+" XtNkeyUp);
        evmap.insert(QEvent::MouseButtonPress, "callbacks+" XtNselect);
        evmap.insert(QEvent::MouseButtonPress, "callbacks+" XtNdrag);
        evmap.insert(QEvent::MouseButtonRelease, "callbacks+" XtNrelease);
        evmap.insert(QEvent::Close, "callbacks+" XtNclose);

        QMapIterator<QEvent::Type, const char *> it(evmap);
        while (it.hasNext()) {
            it.next();
            revmap.insert(it.value(), it.key());
        }
    }

    QList<const char *> targetProps;
    if (evmap.contains(event->type())) targetProps = evmap.values(event->type());

    if (targetProps.isEmpty()) return handled;

    foreach (const QByteArray & prop, w->dynamicPropertyNames()) {
        bool match = false;
        foreach (const char * targetProp, targetProps) {
            if (targetProp == prop) {
                match = true;
                break;
            }
        }
        if (match) {
            //qDebug("  callback on object %x \"%s\" %s", (unsigned int)src, src->objectName().toLocal8Bit().data(), prop.data());
            QVariant callbacks = w->property(prop);
            Q_ASSERT(callbacks.isValid());
            foreach (Callback c, callbacks.toList()) {
                c.proc(w, c.data, event);
                if (event->type() == QEvent::Close) event->ignore();
                handled = true;
            }
        }
    }

    return handled;
}

//
// Xt-Style Event Handlers
//

class EventFilter : public QObject
{
public:
    EventFilter() : QObject() {}
protected:
    bool eventFilter(QObject *, QEvent *);
};

struct EventHandler {
    XtEventHandler handler;
    XtPointer data;
    EventHandler() {}
    inline EventHandler(XtEventHandler h, XtPointer d) : handler(h), data(d) {}
};
Q_DECLARE_METATYPE(EventHandler)

static const EventMask eventMasks[] = {
    PointerMotionMask, ButtonMotionMask, Button1MotionMask, Button2MotionMask,
    EnterWindowMask,LeaveWindowMask, ButtonPressMask, KeyPressMask };

static void addEventHandler(Widget w, EventMask event, EventMask &mask, const EventHandler & eh)
{
    if ((event & mask) == 0) return;
    QByteArray prop(31, '\0');
    qsnprintf(prop.data(), prop.size(), "events+%x", event);
    QVariant events = w->property(prop);
    if (! events.isValid()) events.setValue(QList<QVariant>());
    QList<QVariant> eventList = events.toList();
    eventList.append(QVariant::fromValue(eh));
    w->setProperty(prop, eventList);
    if (event & PointerMotionMask) {
        w->setMouseTracking(true);
        //qDebug("widget %x \"%s\" tracks mouse", (unsigned int)w, w->objectName().toLocal8Bit().data());
    }
    mask = mask & ~event;
}

static void removeEventHandler(Widget w, EventMask event, EventMask &mask, const EventHandler & match)
{
    if ((event & mask) == 0) return;
    mask = mask & ~event;
    QByteArray prop(31, '\0');
    qsnprintf(prop.data(), prop.size(), "events+%x", event);
    QVariant events = w->property(prop);
    if (! events.isValid()) return;

    QList<QVariant> eventList = events.toList();
    for (int i = 0; i < eventList.count();) {
        // remove only when both proc and data match
        EventHandler eh = eventList[i].value<EventHandler>();
        if (eh.data == match.data && eh.handler == match.handler) {
            eventList.removeAt(i);
        } else {
            ++ i;
        }
    }
    w->setProperty(prop, eventList);
    if (event & PointerMotionMask) {
        w->setMouseTracking(false);
        //qDebug("widget %x \"%s\" has stopped tracking mouse", (unsigned int)w, w->objectName().toLocal8Bit().data());
    }
}

static bool invokeEventHandler(Widget w, EventMask event, EventMask mask, QEvent * theEvent)
{
    bool continueDispatch = true;

    if ((event & mask) == 0) return continueDispatch;
    QByteArray prop(31, '\0');
    qsnprintf(prop.data(), prop.size(), "events+%x", event);
    QVariant events = w->property(prop);
    if (! events.isValid()) return continueDispatch;

    QList<QVariant> eventList = events.toList();
    if (! eventList.isEmpty()) {
        //qDebug("  event on object %x \"%s\" %x", (int)w, w->objectName().toLocal8Bit().data(), (int)event);
    }
    for (int i = 0; i < eventList.count(); ++i) {
        EventHandler eh = eventList[i].value<EventHandler>();
        eh.handler(w, eh.data, theEvent, &continueDispatch);
    }
    return continueDispatch;
}

void xcAddEventHandler(Widget w, EventMask event_mask, bool, XtEventHandler
              proc, XtPointer data)
{
    Q_ASSERT(w != NULL);
    static EventFilter * filter  = new EventFilter();

    EventHandler eh(proc, data);
    if (event_mask && ! w->property("hasEvents").isValid()) {
        w->installEventFilter(filter);
        w->setProperty("hasEvents", true);
    }

    for (int i = 0; i < XtNumber(eventMasks); ++i) {
        addEventHandler(w, eventMasks[i], event_mask, eh);
    }
    Q_ASSERT(event_mask == 0); // we should have handled everything
}

void xcRemoveEventHandler(Widget w, EventMask event_mask, bool nonmaskable, XtEventHandler
              proc, XtPointer data)
{
    Q_UNUSED(nonmaskable);
    Q_ASSERT(w != NULL);
    if (! w->property("hasEvents").isValid()) return;

    EventHandler eh(proc,data);
    for (int i = 0; i < XtNumber(eventMasks); ++i) {
        removeEventHandler(w, eventMasks[i], event_mask, eh);
    }
    Q_ASSERT(event_mask == 0); // we should have handled everything
}

bool EventFilter::eventFilter(QObject * src, QEvent * event)
{
    if (! src->isWidgetType()) return QObject::eventFilter(src, event);
    QWidget * w = qobject_cast<QWidget*>(src);

    EventMask events = 0;
    QMouseEvent * mev;
    switch (event->type()) {
    case QEvent::MouseMove:
        //qDebug("mouse move on widget %x \"%s\"", (unsigned int)w, w->objectName().toLocal8Bit().data());
        mev = (QMouseEvent*)event;
        if (mev->buttons().testFlag(Qt::LeftButton)) events = events | ButtonMotionMask | Button1MotionMask;
        if (mev->buttons().testFlag(Qt::RightButton)) events = events | ButtonMotionMask | Button2MotionMask;
        if (mev->buttons() == 0) events = events | PointerMotionMask;
        break;
    case QEvent::Enter:
        events = events | EnterWindowMask;
        break;
    case QEvent::Leave:
        events = events | LeaveWindowMask;
        break;
    case QEvent::MouseButtonPress:
        events = events | ButtonPressMask;
        break;
    case QEvent::KeyPress:
        events = events | KeyPressMask;
        break;
    default:
        break;
    }

    bool handled = false;

    if (! events) return handled;

    // process event handlers
    for (int i = 0; i < XtNumber(eventMasks); ++i) {
        handled = ! invokeEventHandler(w, eventMasks[i], events, event);
    }
    return handled;
}

//
// Xt-style Timer Handler
//

static QMap<int, QObject*> timerMap;

TimerObject::TimerObject(unsigned long interval, XtTimerCallbackProc p, XtPointer d) :
    proc(p),
    data(d)
{
    timer.start(interval, this);
    timerMap.insert(timer.timerId(), this);
}

void TimerObject::timerEvent(QTimerEvent * ev)
{
    if (ev->timerId() == timer.timerId()) {
        XtIntervalId id = timer.timerId();
        timer.stop(); // timeout events are single-shot
        proc(data, &id);
    }
}

XtIntervalId xcAddTimeout(XtAppContext, unsigned long interval, XtTimerCallbackProc proc, XtPointer client_data)
{
    Q_ASSERT(interval != 0);
    int id = (new TimerObject(interval, proc, client_data))->id();
    //qDebug("adding %ldms timeout #%d", interval, id);
    return id;
}

void xcRemoveTimeout(XtIntervalId tid)
{
    //qDebug("removing timeout #%d", tid);
    Q_ASSERT(timerMap.contains(tid));
    if (timerMap.contains(tid)) {
        delete timerMap[tid];
        timerMap.remove(tid);
    }
}

//
// Painting
//

void XSetLineAttributes(QPainter* gc, unsigned int w, Qt::PenStyle style, Qt::PenCapStyle cap, Qt::PenJoinStyle join)
{
    Q_ASSERT(gc);
    QPen p(gc->pen());
    p.setCapStyle(cap);
    p.setJoinStyle(join);
    p.setStyle(style);
    p.setWidth(w);
    gc->setPen(p);
}

void DrawArc(QPainter* gc, int x, int y, unsigned int w, unsigned int h, int a1, int a2)
{
    Q_ASSERT(gc);
    gc->drawArc(x, y, w, h, a1/4, a2/4);
}

void FillPolygon(QPainter* gc, XPoint *points, int npoints)
{
    Q_ASSERT(gc);
    QPoint * qpoints = new QPoint[npoints];
    std::copy(points, points+npoints, qpoints);
    gc->drawPolygon(qpoints, npoints);
}

void XClearArea(Window win, int x, int y, unsigned w, unsigned h, bool exposures)
{
    Q_UNUSED(exposures);
    QPalette pal;
    QPainter p(win);
    p.fillRect(x, y, w, h, pal.color(QPalette::Background));
}

void SetForeground(QPainter* gc, unsigned long foreground)
{
    Q_ASSERT(gc);
    QPen p(gc->pen());
    p.setColor(QColor((QRgb)foreground));
    gc->setPen(p);
}

void SetFunction(QPainter* gc, int function)
{
    Q_ASSERT(gc);
    Q_UNUSED(function);
}

int XTextWidth(XFontStruct *font_struct, const char *string, int count)
{
    Q_UNUSED(font_struct);
    Q_UNUSED(string);
    Q_UNUSED(count);
    return 0;
}

void SetStipple(QPainter* gc, int stipple_index, bool opaque)
{
    Q_ASSERT(gc);
    Q_ASSERT(stipple_index >= 0 && stipple_index < STIPPLES);
    QBrush b(*STIPPLE[stipple_index + (opaque ? STIPPLES : 0)]);
    gc->setBrush(b);
}

void XDrawRectangle(Drawable, QPainter* gc, int x, int y, unsigned int w, unsigned int h)
{
    Q_ASSERT(gc);
    Q_UNUSED(x); Q_UNUSED(y); Q_UNUSED(w); Q_UNUSED(h);
}

void DrawLine(QPainter* gc, int x1, int y1, int x2, int y2)
{
    Q_ASSERT(gc);
    gc->drawLine(x1, y1, x2, y2);
}

void DrawLines(QPainter* gc, XPoint* point, int number)
{
    Q_ASSERT(gc);
    for (int i = 0; i < number-1; ++ i) {
        //qDebug("drawing line %d,%d -> %d,%d", point[i].x, point[i].y, point[i+1].x, point[i+1].y);
        gc->drawLine(point[i].x, point[i].y, point[i+1].x, point[i+1].y);
    }
}

void XFillRectangle(Drawable, QPainter* gc, int x, int y, int w, int h)
{
    Q_ASSERT(gc);
    gc->fillRect(x, y, w, h, Qt::SolidPattern);
}

void XCopyArea(Drawable, Drawable, QPainter* gc, int sx, int sy, int w, int h, int dx, int dy)
{
    Q_ASSERT(gc);
    Q_UNUSED(sx); Q_UNUSED(sy); Q_UNUSED(w); Q_UNUSED(h); Q_UNUSED(dx); Q_UNUSED(dy);
}

void XDrawString(Drawable, QPainter* gc, int x, int y, const char *str, int len)
{
    Q_ASSERT(gc);
    QString s = QString::fromLocal8Bit(str, len);
    gc->drawText(x, y, s);
}

//
//
//

void XtGetValues(Widget, ArgList, Cardinal)
{
}

void XtSetValues(Widget, ArgList, Cardinal)
{
}

Widget XtNameToWidget(Widget reference, String names)
{
    Q_UNUSED(reference); Q_UNUSED(names);
    return NULL;
}

Widget XtCreateManagedWidget(String name, WidgetClass, Widget parent, ArgList args, Cardinal num_args)
{
    Q_UNUSED(name); Q_UNUSED(parent); Q_UNUSED(args); Q_UNUSED(num_args);
    return NULL;
}

Widget XtCreateWidget(String name, WidgetClass cls, Widget parent, ArgList args, Cardinal num_args)
{
    Q_UNUSED(name); Q_UNUSED(cls); Q_UNUSED(args); Q_UNUSED(num_args);
    //if (cls == XwmenubuttonWidgetClass) return NULL;
    return new QWidget(parent);
}

Widget XtCreatePopupShell(String name, WidgetClass cls, Widget parent, ArgList args, Cardinal num_args)
{
    Q_UNUSED(name); Q_UNUSED(cls); Q_UNUSED(parent); Q_UNUSED(args); Q_UNUSED(num_args);
    return NULL;
}

void XtPopup(Widget popup_shell, XtGrabKind grab_kind)
{
    Q_UNUSED(popup_shell); Q_UNUSED(grab_kind);

}

char *XKeysymToString(KeySym keysym)
{
    Q_UNUSED(keysym);
    return NULL;
}

void XChangeProperty(Window w, Atom prop, Atom type, int format, int mode, const unsigned char *data, int nelements)
{
    Q_UNUSED(w); Q_UNUSED(prop); Q_UNUSED(type); Q_UNUSED(format); Q_UNUSED(mode); Q_UNUSED(data); Q_UNUSED(nelements);

}

Pixmap CreateBitmapFromData(char *data, unsigned int w, unsigned int h)
{
    return new QBitmap(QBitmap::fromData(QSize(w,h), (const uchar*)data));
}

Cursor XCreatePixmapCursor(Pixmap source, Pixmap mask, QRgb, QRgb, unsigned int xhot, unsigned int yhot)
{
    QCursor *cur = new QCursor(*source, *mask, xhot, yhot);
    delete source;
    delete mask;
    return cur;
}

int XDefineCursor(Window w, Cursor cursor)
{
    w->setCursor(*cursor);
    return -1;
}

void XTextExtents(XFontStruct *font_struct, const char *string, int nchars, int *direction_return,
              int *font_ascent_return, int *font_descent_return, XCharStruct *overall_return)
{
    Q_UNUSED(font_struct); Q_UNUSED(string); Q_UNUSED(nchars); Q_UNUSED(direction_return);
    Q_UNUSED(font_ascent_return); Q_UNUSED(font_descent_return); Q_UNUSED(overall_return);

}

void XtTranslateCoords(Widget w, Position x, Position y, Position *rootx_return, Position *rooty_return)
{
    Q_UNUSED(w); Q_UNUSED(x); Q_UNUSED(y); Q_UNUSED(rootx_return); Q_UNUSED(rooty_return);

}

Atom XInternAtom(const char * name, bool only_if_exists)
{
    Q_UNUSED(name); Q_UNUSED(only_if_exists);
    return 0;
}

char *XwTextCopyBuffer(XwTextEditWidget)
{
    return NULL;
}

void XwTextClearBuffer(XwTextEditWidget w)
{
    if (w) w->setText("");
}

int XLookupString(XKeyEvent *event_struct, char *buffer_return, int bytes_buffer, KeySym *keysym_return, XComposeStatus *status_in_out)
{
    return 0;
}

void XwPostPopup(Widget shell, Widget menu, Widget button, Position dx, Position dy)
{

}

void overdrawpixmap(QAction *action)
{
    /// \todo
#if 0
    MenuItem mitem = (MenuItem)button;
    ToolItem titem = NULL;
    int idx;

    if (button == NULL) return;
    idx = mitem->toolbar_idx;

    if (mitem->parentMenu == ((MenuItem)ColorInheritColorButton)->parentMenu)
            titem = (ToolItem)ColorsToolButton;
    else if (mitem->parentMenu == ((MenuItem)FillBlackButton)->parentMenu)
            titem = (ToolItem)FillsToolButton;
    else
            return;

    if (idx == 0) {
            if (mitem == (MenuItem)ColorInheritColorButton) {
                    idx = ((ToolItem)ColorsToolButton)->ID-((ToolItem)PanToolButton)->ID;
            } else if (mitem == (MenuItem)FillWhiteButton) {
                    idx = ((ToolItem)FillsToolButton)->ID-((ToolItem)PanToolButton)->ID;
            } else {
                    HBITMAP hBitmap = NULL;
                    TBADDBITMAP tb;
                    if (titem == (ToolItem)ColorsToolButton)
                            hBitmap = create_color_icon(mitem->menudata, 20, 20);
                    else {
                            if (mitem->menudata == (OPAQUE | FILLED | FILLSOLID))
                                    hBitmap = create_color_icon(RGB(0, 0, 0), 20, 20);
                            else if (mitem != (MenuItem)FillOpaqueButton && mitem != (MenuItem)FillTransparentButton)
                                    hBitmap = create_stipple_icon((mitem->menudata & FILLSOLID) >> 5, 20, 20);
                    }
                    if (hBitmap != NULL) {
                            tb.hInst = NULL;
                            tb.nID = (INT_PTR)hBitmap;
                            idx = mitem->toolbar_idx = SendMessage(titem->toolbar->handle, TB_ADDBITMAP, 1, (LPARAM)&tb);
                    }
            }
    }
#endif
}

void outputpopup(QAction* button, void*, void*)
{
#if 0
        if (is_page(topobject) == -1) {
                printf("Can only save a top-level page!");
                return;
        }
        //DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_OUTPUTDLG), top->handle, OutputDlgProc);
#endif
}

void starthelp(QAction* button, void*, void*)
{
        //DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_HELPDLG), areawin->viewport, HelpDlgProc, (LPARAM)NULL);
}

//
//
//

void popupQuestion(QAction* a, const char * request, const char * current, PopupCallback cb, void * data)
{
    if (a) a->setEnabled(false);
    QString result = QInputDialog::getText(top, "prompt", request, QLineEdit::Normal, current);
    if (!result.isNull()) cb(a, result, data);
    if (a) a->setEnabled(true);
}

void popupYesNo(QAction * a, const char * request, PopupCallback cb, void * data)
{
    if (QMessageBox::question(areawin->viewport, "XCircuit", request,
                              QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes) {
        cb(a, QString(), data);
    }
}

void popupYesNo(QAction * a, const char * request, ActionCallback cb, void * data)
{
    if (QMessageBox::question(areawin->viewport, "XCircuit", request,
                              QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes) {
        cb(a, data, NULL);
    }
}

void popupFile(QAction* a, const char * request, const char * /*current*/, const char * filter, PopupCallback cb, void * data)
{
    QString sfilter;
    sfilter.sprintf("Enable Filter (*%s);;Disable Filter (*.*)", filter);
    QString fn =
            QFileDialog::getOpenFileName(areawin->viewport,
                                         request,
                                         QString(),
                                         sfilter);
    if (! fn.isNull()) {
        cb(a, fn, data);
    }
}

QPoint actionCenter(QAction* a)
{
    QPoint p;
    foreach (QWidget * w, a->associatedWidgets()) {
        p = w->mapToGlobal(w->rect().center());
        qDebug(" action center for \"%s\" at %d,%d", w->objectName().toLocal8Bit().data(), p.x(), p.y());
        if (qobject_cast<QAbstractButton*>(w)) break;
    }
    return p;
}

//
//
//

static void setupAppData()
{
    appdata.globalcolor = getnamedcolor("Orange2");
    appdata.localcolor = getnamedcolor("Red");
    appdata.infocolor = getnamedcolor("SeaGreen");
    appdata.ratsnestcolor = getnamedcolor("Tan4");
    appdata.bboxpix = getnamedcolor("greenyellow");
    appdata.fg = getnamedcolor("Black");
    appdata.bg = getnamedcolor("White");
    appdata.gridpix = getnamedcolor("Gray95");
    appdata.snappix = getnamedcolor("Red");
    appdata.selectpix = getnamedcolor("Gold3");
    appdata.querypix = getnamedcolor("Turquoise");
    appdata.filterpix = getnamedcolor("SteelBlue3");
    appdata.axespix = getnamedcolor("Antique White");
    appdata.buttonpix = getnamedcolor("Gray85");
    appdata.auxpix = getnamedcolor("Green3");
    appdata.barpix = getnamedcolor("Tan");
    appdata.parampix = getnamedcolor("Plum3");
    appdata.fg2 = getnamedcolor("White");
    appdata.bg2 = getnamedcolor("DarkSlateGray");
    appdata.gridpix2 = getnamedcolor("Gray40");
    appdata.snappix2 = getnamedcolor("Red");
    appdata.selectpix2 = getnamedcolor("Gold");
    appdata.querypix2 = getnamedcolor("Turquoise");
    appdata.filterpix2 = getnamedcolor("SteelBlue1");
    appdata.axespix2 = getnamedcolor("NavajoWhite4");
    appdata.buttonpix2 = getnamedcolor("Gray50");
    appdata.auxpix2 = getnamedcolor("Green");
    appdata.barpix2 = getnamedcolor("Tan");
    appdata.parampix2 = getnamedcolor("Plum3");

    appdata.width = 950;
    appdata.height = 760;
    appdata.timeout = 10;
}

static inline char ucase(char p) { return (p>=0x61 && p<=0x7A) ? p & ~0x20 : p; }

static QByteArray menuName(const char * name)
{
    QByteArray n(name);
    int paren = n.indexOf('(');
    if (paren > 0) {
        n.truncate(paren);
    }
    n.replace(":", "");
    n.replace('-', ' ');
    n.replace('/', ' ');
    n = n.simplified().toLower();
    int sp = 0;
    n[0] = ucase(n[0]); // capitalize 1st word
    while ((sp = n.indexOf(' ', sp)) > 0) {
        n.remove(sp, 1); // remove space
        n[sp] = ucase(n[sp]); // capitalize subsequent words
    }
    return n;
}

namespace {
    struct StackEl {
        menuptr menup;
        QMenu*  menu;
        QByteArray name;
        StackEl() : menup(0), menu(0) {}
        StackEl(menuptr mp) : menup(mp), menu(0) {}
        StackEl(menuptr mp, QMenu * m, QByteArray n) : menup(mp), menu(m), name(n) {}
        StackEl(const StackEl & el, bool) :
                menup(el.menup->submenu),
                menu(el.menu->addMenu(el.menup->name)),
                name(menuName(el.menup->name)) {}
        QString actionName(const QByteArray & itemName) const {
            return QString().sprintf("%s_%s", name.data(), menuName(itemName).data());
        }
    };
}

static void createMenus (QMenuBar* menubar)
{
    QStack<StackEl> stack;

    stack.push(TopButtons);
    while (true) {
        menuptr menu = stack.top().menup;
        qDebug("menu %x \"%s\"@%x", (int)menu, menu->name, (unsigned int)(menu->name));
        if (stack.count() == 1) {
            // check if we're done
            if (menu->name == NULL) break;
            // top menu has only submenus
            ++ stack.top().menup;
            stack.push(StackEl(menu->submenu, menubar->addMenu(menu->name), menuName(menu->name)));
        } else {
            // below top menu
            if (menu->submenu == 0) {
                // menu item
                if (menu->name != NULL && strcmp(menu->name, " ") != 0) {
                    if (menu->name[0] == '_') {
                        // color entry
                        QRgb color = getnamedcolor(menu->name+1);
                        addnewcolorentry(color, stack.top().menu);
                    } else {
                        QAction * action = stack.top().menu->addAction(menu->name);
                        action->setObjectName(stack.top().actionName(menu->name));
                        new ActionCallbackObject(action, menu->func, menu->passeddata);
                        action->setCheckable(menu->size != 0);
                        if (menu->size != 0) action->setChecked(menu->size == 1);
                        qDebug(" path: %s", action->objectName().toLocal8Bit().data());
                    }
                } else {
                    stack.top().menu->addSeparator();
                }
                ++ stack.top().menup;
            } else {
                // submenu
                StackEl submenu(stack.top(), true);
                submenu.menu->setObjectName(stack.top().actionName(submenu.name));
                ++ stack.top().menup;
                stack.push(submenu);
                qDebug(" mpath: %s", submenu.menu->objectName().toLocal8Bit().data());
            }
        }
        if (stack.top().menup->name == NULL) {
            stack.pop();
            qDebug("stack has %d", stack.count());
        }
    }
}

static void createToolbar(QToolBar* toolbar)
{
    toolbar->setStyleSheet("QToolBar { spacing: 0px; }");
    for (int i = 0; ToolBar[i].name; ++i) {
        QImage img(ToolBar[i].icon_data);
        if (i == 0) toolbar->setIconSize(img.size());
        QPixmap pix(img.size());
        QPainter p(&pix);
        p.drawImage(0, 0, img);
        QAction * action = toolbar->addAction(QIcon(pix), "");
        QWidget * button = NULL;
        foreach(QWidget * w, action->associatedWidgets()) {
            if (w != toolbar) {
                button = w;
                break;
            }
        }
        if (button) {
            button->setObjectName(ToolBar[i].name);
            button->setProperty("index", QVariant::fromValue(i));
        }
        action->setStatusTip(ToolBar[i].hint);
        new ActionCallbackObject(action, ToolBar[i].func, ToolBar[i].passeddata);
    }
}

XCWindowData *GUI_init(int *argc, char *argv[])
{
   XCWindowData *newwin;

   QApplication * app = new QApplication(*argc, argv);

   for (int i = 0; i < STIPPLES; i++) {
      QPixmap *transp = CreateBitmapFromData(STIPDATA[i], 4, 4);
      QPixmap *opaque = new QPixmap(transp->size());
      QPainter p(opaque);
      p.fillRect(opaque->rect(), Qt::white);
      p.drawPixmap(0,0,*transp);
      STIPPLE[i] = transp;
      STIPPLE[i+STIPPLES] = opaque;
  }

   setupAppData();

   newwin = create_new_window();

   /* toplevel */
   top = new QWidget();
   top->setObjectName("Top");

   QGridLayout * formGrid = new QGridLayout();
   formGrid->setSpacing(2);
   formGrid->setMargin(2);
   top->setLayout(formGrid);

   /* menu bar */
   QBoxLayout * toplayout = new QBoxLayout(QBoxLayout::LeftToRight);

   newwin->menubar = new QMenuBar();
   createMenus((QMenuBar*)newwin->menubar);
#ifndef Q_WS_MAC
   toplayout->addWidget((QMenuBar*)newwin->menubar);
#endif

   message1 = new QLabel(QString("Welcome to Xcircuit Version %1").arg(PROG_VERSION, 3, 'f', 1));
   toplayout->addWidget(message1);
   formGrid->addLayout(toplayout, 0, 0, 1, 2);

   /* main drawing window */
   newwin->area = new Area();
   newwin->area->setObjectName("Area");
   newwin->area->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
   formGrid->addWidget(newwin->area, 1, 0, 1, 1);
   newwin->viewport = newwin->area->viewport();
   newwin->viewport->setObjectName("AreaViewport");

   /* message widgets */
   QBoxLayout * statusbar = new QBoxLayout(QBoxLayout::LeftToRight);

   wsymb = new QLabel("Symbol");
   wsymb->setFrameShape(QFrame::Box);
   statusbar->addWidget(wsymb);

   wschema = new QLabel("Schematic");
   wschema->setFrameShape(QFrame::Box);
   statusbar->addWidget(wschema);

   message2 = new QLabel("Editing: Page 1");
   message2->setFrameShape(QFrame::StyledPanel);
   statusbar->addWidget(message2);

   message3 = new QLabel("Don't Panic");
   message3->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
   statusbar->addWidget(message3);

   formGrid->addLayout(statusbar, 2, 0, 1, 2);

   /* toolbar */
   QToolBar * t = new QToolBar();
   t->setObjectName("Toolbar");
   t->setOrientation(Qt::Vertical);
   createToolbar(t);
   formGrid->addWidget(t, 0, 1, 2, 1);
   toolbar = t;

   /* Setup callback routines for the area widget */
   /* Use Button1Press event to add the callback which tracks motion;  this */
   /*   will reduce the number of calls serviced during normal operation */

   // an AbstractScrollArea will swallow movement keys, thus we need to
   // process keypresses right there
   XtAddCallback(newwin->area, XtNkeyDown, (WidgetCallback)keyhandler, NULL);
   XtAddCallback(newwin->area, XtNkeyUp, (WidgetCallback)keyhandler, NULL);

   xcAddEventHandler(newwin->viewport, Button1MotionMask | Button2MotionMask,
                false, (XtEventHandler)xlib_drag, NULL);

   XtAddCallback(top, XtNclose, (WidgetCallback)delwin, NULL);

   XtAddCallback(wsymb, XtNselect, xlib_swapschem, Number(0));
   XtAddCallback(wschema, XtNselect, xlib_swapschem, Number(0));

   top->setWindowIcon(QIcon(QPixmap(xcircuit_xpm)));

/* Don't know why this is necessary, but otherwise keyboard input focus    */
/* is screwed up under the WindowMaker window manager and possibly others. */

#ifdef INPUT_FOCUS
   xcAddEventHandler(top, SubstructureNotifyMask,
        true, (XtEventHandler)mappinghandler, NULL);
#endif

#if 0
   // ghostscript background rendering -- TODO in future
   void clientmessagehandler(Widget w, caddr_t clientdata, QEvent* *event);
   xcAddEventHandler(top, NoEventMask, true,
        (XtEventHandler)clientmessagehandler, NULL);
#endif

   return newwin;
}

/*----------------------------------------------------------------------*/
/* Main entry point when used as a standalone program			*/
/*----------------------------------------------------------------------*/

int main(int argc, char **argv)
{
   char  *argv0;		/* find root of argv[0] */
   short k = 0;

   /*-----------------------------------------------------------*/
   /* Find the root of the command called from the command line */
   /*-----------------------------------------------------------*/

   argv0 = strrchr(argv[0], '/');
   if (argv0 == NULL)
      argv0 = argv[0];
   else
      argv0++;

   pre_initialize();

   /*---------------------------*/
   /* Check for schematic flag  */
   /*---------------------------*/

   for (k = argc - 1; k > 0; k--) {
      if (!strncmp(argv[k], "-2", 2)) {
         pressmode = 1;		/* 2-button mouse indicator */
         break;
      }
   }

   areawin = NULL;
   areawin = GUI_init(&argc, argv);
   areawin->event_mode.update(); // set menu activity
   post_initialize();

   ghostinit(); // initialize the ghostscript renderer

   /*----------------------------------------------------------*/
   /* Check home directory for initial settings & other loads; */
   /* Load the (default) built-in set of objects 	       */
   /*----------------------------------------------------------*/

#ifdef HAVE_PYTHON
   init_interpreter();
#endif

   loadrcfile();
   pressmode = 0;	/* Done using this to mark 2-button mouse mode */

   composelib(PAGELIB);	/* make sure we have a valid page list */
   composelib(LIBLIB);	/* and library directory */

   /*----------------------------------------------------*/
   /* Parse the command line for initial file to load.   */
   /* Otherwise, look for possible crash-recovery files. */
   /*----------------------------------------------------*/

   if (argc == 2 + (k != 0) || argc == 2 + (k != 0)) {
      QString str = argv[(k == 1) ? 2 : 1];
      startloadfile(-1, str);  /* change the argument to load into library other
                                  than the User Library */
   }
   else {
      findcrashfiles();
   }

   top->show();
   return qApp->exec(); /* exit through quit() callback */
}
