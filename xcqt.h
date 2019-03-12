#ifndef XCQT_H
#define XCQT_H

#include <QApplication>
#include <QWidget>
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QDesktopWidget>
#include <QRgb>
#include <QPoint>

#include <cstddef>

#include "xctypes.h"

class QLabel;
class QToolBar;

extern QWidget *top;
extern QLabel *wschema, *wsymb, *message1, *message2, *message3;
extern QToolBar *toolbar;
extern QWidget *overlay;

void popupQuestion(QAction*, const char * request, const char * current, PopupCallback cb, void * data = NULL);
void popupYesNo(QAction*, const char * request, PopupCallback cb, void * data = NULL);
void popupYesNo(QAction*, const char * request, ActionCallback cb, void * data = NULL);
void popupFile(QAction*, const char * request, const char * current, const char * filter, PopupCallback, void * data = NULL);

QPoint actionCenter(QAction*);

XtIntervalId xcAddTimeout(unsigned long interval, XtTimerCallbackProc proc, XtPointer client_data);
void xcRemoveTimeout(XtIntervalId timer);
#define XtSetArg(arg,t,d) ((arg).name = (t), (arg).value = (XtArgVal)(d))
void XtGetValues(Widget w, ArgList args, Cardinal num_args);
void XtSetValues(Widget w, ArgList args, Cardinal num_args);
void XSetLineAttributes(QPainter* gc, unsigned int w, Qt::PenStyle style, Qt::PenCapStyle cap, Qt::PenJoinStyle join);
inline Widget XtParent(Widget w) { return w ? w->parentWidget() : w; }
Widget XtNameToWidget(Widget reference, String names);
Widget XtCreateManagedWidget(String name, WidgetClass, Widget parent, ArgList args, Cardinal num_args);
Widget XtCreateWidget(String name, WidgetClass, Widget parent, ArgList args, Cardinal num_args);

Widget XtCreatePopupShell(String name, WidgetClass widget_class, Widget parent, ArgList args, Cardinal num_args);
void XtPopup(Widget popup_shell, XtGrabKind grab_kind);
char *XKeysymToString(KeySym keysym);
void XChangeProperty(Window w, Atom prop, Atom type, int format, int mode, const unsigned char *data, int nelements);
Pixmap CreateBitmapFromData(uint8_t *data, unsigned int w, unsigned int h);
Cursor XCreatePixmapCursor(Pixmap source, Pixmap mask, QRgb, QRgb, unsigned int xhot, unsigned int yhot);
int XDefineCursor(Window w, Cursor cursor);
inline unsigned long BlackPixel(int) { return qRgb(0,0,0); }
inline unsigned long WhitePixel(int) { return qRgb(255,255,255); }

inline void XBell(int) { QApplication::beep(); }
inline void XSendEvent(Window, bool, long, QEvent**) { qDebug("Should send an event"); }
inline int DisplayWidth(int) { return QApplication::desktop()->width(); }
inline int DisplayHeight(int) { return QApplication::desktop()->height(); }
void DrawArc(QPainter*, int x, int y, unsigned int w, unsigned int h, int a1, int a2);
void FillPolygon(QPainter*, XPoint *points, int npoints);
void XClearArea(Window win, int x, int y, unsigned width, unsigned height, bool exposures);
void SetForeground(QPainter*, unsigned long foreground);
void SetFunction(QPainter*, int function);
int XTextWidth(XFontStruct *font_struct, const char *string, int count);
void SetStipple(QPainter*, int stipple_index, bool opaque);
void XDrawRectangle(Drawable, QPainter*, int x, int y, unsigned int w, unsigned int h);
inline void XtManageChild(Widget child) { return; if (child) child->show(); }
inline void XtUnmanageChild(Widget child) { if (child) child->hide(); }
void XTextExtents(XFontStruct *font_struct, const char *string, int nchars, int *direction_return,
              int *font_ascent_return, int *font_descent_return, XCharStruct *overall_return);
void XtTranslateCoords(Widget w, Position x, Position y, Position *rootx_return, Position *rooty_return);

Atom XInternAtom(const char * name, bool only_if_exists);
void XSetClipOrigin(QPainter*, int clip_x_origin, int clip_y_origin);
void xcAddEventHandler(Widget w, EventMask event_mask, bool nonmaskable, XtEventHandler
              proc, XtPointer client_data);
void xcRemoveEventHandler(Widget w, EventMask event_mask, bool nonmaskable, XtEventHandler
              proc, XtPointer client_data);

void XtAddCallback(QAction* a, ActionCallback proc, void* data);
void XtRemoveCallback(QAction* a, ActionCallback proc, void* data);
void XtRemoveAllCallbacks(QAction * a);

void XtAddCallback(QWidget* w, String name, WidgetCallback proc, void* data);
void XtRemoveCallback(QWidget* w, String name, WidgetCallback proc, void* data);
void XtRemoveAllCallbacks(QWidget * w, String callback_name);

char *XwTextCopyBuffer(XwTextEditWidget w);
void XwTextClearBuffer(XwTextEditWidget w);
int XLookupString(XKeyEvent *event_struct, char *buffer_return, int bytes_buffer, KeySym *keysym_return, XComposeStatus *status_in_out);
void XwPostPopup(Widget shell, Widget menu, Widget button, Position dx, Position dy);

void DrawLines(QPainter* gc, XPoint* point, int number);
void XFillRectangle(Drawable, QPainter* gc, int x, int y, int w, int h);
void DrawLine(QPainter* gc, int x1, int y1, int x2, int y2);
void XCopyArea(Drawable src, Drawable dst, QPainter* gc, int x, int y, int w, int h, int offx, int offy);
void XDrawString(Drawable, QPainter* gc, int x, int y, const char *str, int len);


#define KPMOD   Qt::KeypadModifier

#define XK_Home		Qt::Key_Home
#define XK_Delete	Qt::Key_Delete
#define XK_Escape	Qt::Key_Escape
#define XK_F19		Qt::Key_F19
#define XK_Right	Qt::Key_Right
#define XK_Left		Qt::Key_Left
#define XK_Down 	Qt::Key_Down
#define XK_Up		Qt::Key_Up
#define XK_Insert	Qt::Key_Insert
#define XK_End		Qt::Key_End
#define XK_Return	Qt::Key_Return
#define XK_BackSpace	Qt::Key_Backspace
#define XK_Undo		Qt::Key_F6
#define XK_Redo		Qt::Key_F7
#define XK_Tab		Qt::Key_Tab
#define XK_KP_1 (KPMOD|Qt::Key_1)
#define XK_KP_2 (KPMOD|Qt::Key_2)
#define XK_KP_3 (KPMOD|Qt::Key_3)
#define XK_KP_4 (KPMOD|Qt::Key_4)
#define XK_KP_5 (KPMOD|Qt::Key_5)
#define XK_KP_6 (KPMOD|Qt::Key_6)
#define XK_KP_7 (KPMOD|Qt::Key_7)
#define XK_KP_8 (KPMOD|Qt::Key_8)
#define XK_KP_9 (KPMOD|Qt::Key_9)
#define XK_KP_0 (KPMOD|Qt::Key_0)
#define XK_KP_Enter	Qt::Key_Enter
#define XK_KP_Add	(KPMOD|Qt::Key_Plus)
#define XK_KP_Subtract	(KPMOD|Qt::Key_Minus)
#define XK_KP_End	(KPMOD|Qt::Key_End)
#define XK_KP_Home	(KPMOD|Qt::Key_Home)
#define XK_KP_Up	(KPMOD|Qt::Key_Up)
#define XK_KP_Down	(KPMOD|Qt::Key_Down)
#define XK_KP_Right	(KPMOD|Qt::Key_Right)
#define XK_KP_Left	(KPMOD|Qt::Key_Left)
#define XK_KP_Next	(KPMOD|Qt::Key_Forward)
#define XK_KP_Prior	(KPMOD|Qt::Key_Back)
#define XK_KP_Begin	(KPMOD|Qt::Key_Clear)

#endif // XCQT_H
