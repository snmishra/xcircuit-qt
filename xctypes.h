#ifndef XCTYPES_H
#define XCTYPES_H

#include <QKeyEvent>
#include <QRgb>
#include <QList>
#include <QVector>

#include <cmath>

#define NO_FREE(type) void free(type)

/*
 * Types
 */

class QPaintDevice;
class QPixmap;
class QFont;
class QLineEdit;
class QImage;

typedef int Status;

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;
typedef char * caddr_t;
typedef const char* String;

typedef QPaintDevice *Drawable;
typedef QPixmap* Pixmap;
typedef QCursor *Cursor;
typedef QWidget Screen;
typedef QWidget* Window;
typedef QFont* Font;
typedef QMouseEvent XMotionEvent, XPointerMovedEvent, XButtonEvent;
typedef QImage XImage;
typedef QWidget *Widget;
typedef QLineEdit *XwTextEditWidget;

typedef struct {} Visual;
typedef unsigned long Atom;
typedef void XWMHints;
typedef struct {} XrmOptionDescRec;
typedef unsigned int Cardinal;
typedef unsigned long EventMask;
typedef struct {} XComposeStatus;
typedef struct {} _XtTranslationsRec, *XtTranslations;
typedef long XtArgVal;

typedef void* XtPointer;
typedef void* XCrossingEvent;

typedef short Dimension;
typedef short Position;
typedef int WidgetClass;
typedef Widget* WidgetList;
typedef unsigned long XID;
typedef XID KeySym;
typedef unsigned char KeyCode;

typedef void* Colormap;
typedef int XtIntervalId;

typedef QRgb Pixel;

typedef void (*PopupCallback)(QAction*, const QString &, void*);
typedef void (*WidgetCallback)(QWidget*, void*, void*);
typedef void (*ActionCallback)(QAction*, void*, void*);
typedef void (*XtEventHandler)(Widget, XtPointer, QEvent*, bool*);
typedef void (*XtTimerCallbackProc)(XtPointer, XtIntervalId*);
typedef String (*XtLanguageProc)(String, XtPointer);

typedef struct __CharStruct *XCharStruct;

typedef enum {XtGrabNone, XtGrabNonexclusive, XtGrabExclusive} XtGrabKind;

typedef QRgb XColor;
#define DoRed	0x1
#define DoGreen	0x2
#define DoBlue	0x4

// Points

class XfPoint {
public:
   float x, y;
   inline XfPoint() {}
   inline XfPoint(float _x, float _y) : x(_x), y(_y) {}
   inline XfPoint & operator+=(const XfPoint& other) { x += other.x; y += other.y; return *this; }
   inline XfPoint & operator-=(const XfPoint& other) { x -= other.x; y -= other.y; return *this; }
};
typedef XfPoint* fpointlist;
NO_FREE(XfPoint*);

struct XlPoint;
class XPoint {
public:
    short x, y;
    inline XPoint() {}
    inline XPoint(short _x, short _y) : x(_x), y(_y) {}
    inline XPoint(const QPoint & p) : x(p.x()), y(p.y()) {}
    inline XPoint(const XfPoint & p) : x(round(p.x)), y(round(p.y)) {}
    inline operator QPoint() const { return QPoint(x, y); }
    inline operator XfPoint() const { return XfPoint(x, y); }
    XPoint & operator=(const XlPoint & other);
    inline XPoint & operator=(const XfPoint& other) { x = other.x; y = other.y; return *this; }
    inline XPoint & operator+=(const XPoint& other) { x += other.x; y += other.y; return *this; }
    inline XPoint & operator-=(const XPoint& other) { x -= other.x; y -= other.y; return *this; }
};
inline const XPoint operator+(const XPoint& a, const XPoint& b) { return XPoint(a.x + b.x, a.y + b.y); }
inline const XPoint operator-(const XPoint& a, const XPoint& b) { return XPoint(a.x - b.x, a.y - b.y); }
inline const XPoint operator-(const XPoint& a) { return XPoint(-a.x, -a.y); }
inline const XPoint abs(const XPoint & a) { return XPoint(abs(a.x), abs(a.y)); }
NO_FREE(XPoint*);

class pointlist : public QVector<XPoint> {
public:
    inline pointlist(XPoint * src, int count) : QVector<XPoint>(count) {
        for (int i = 0; i < count; ++i) { (*this)[i] = src[i]; }
    }
    inline pointlist(const pointlist & src) : QVector<XPoint>(src) {}
    inline pointlist(int size) : QVector<XPoint>(size) {}
    inline pointlist() : QVector<XPoint>() {}
    inline const XPoint* operator+(int el) const { return begin()+el; }
    inline XPoint* operator+(int el) { return begin()+el; }
    inline pointlist & operator+=(const XPoint& other) {
        for (int i = 0; i < count(); ++i) (*this)[i] += other; return *this;
        return *this;
    }
};
NO_FREE(pointlist*);

struct XlPoint {
   long x, y;
   inline XlPoint & operator=(const XPoint & other) { x = other.x; y = other.y; return *this; }
   inline XlPoint & operator+=(const XPoint & other) { x += other.x; y += other.y; return *this; }
   inline XlPoint & operator-=(const XPoint & other) { x -= other.x; y -= other.y; return *this; }
};
NO_FREE(XlPoint*);

inline XPoint & XPoint::operator=(const XlPoint & other) { x = other.x; y = other.y; return *this; }


// BBox

class BBox {
public:
   XPoint	lowerleft;
   Dimension	width, height;
};
NO_FREE(BBox*);

//

typedef struct {
        int ascent, descent;
        Font fid;
} XFontStruct;

// this hack is used by the area window to treat key and mouse events the same
class XKeyEvent : public QKeyEvent {
public:
    XKeyEvent(const QKeyEvent &);
    XKeyEvent(const QMouseEvent &);
    inline int x() const { return p.x(); }
    inline int y() const { return p.y(); }
    inline Qt::MouseButtons buttons() const { return mouseState; }
protected:
    QPoint p;
    Qt::MouseButtons mouseState;
};
NO_FREE(XKeyEvent*);

typedef struct {
        int type;
        Atom message_type;
        int format;
        Window window;
        union {
                long l[5];
        } data;
} XClientMessageEvent;

typedef struct {
        String name;
        XtArgVal value;
} Arg, *ArgList;

typedef int XtAddressMode;
typedef struct {
    XtAddressMode   address_mode;
    XtPointer       address_id;
    Cardinal        size;
} XtConvertArgRec, *XtConvertArgList;
typedef struct {
    unsigned int    size;
    char*           addr;
} XrmValue, *XrmValuePtr;

typedef void (*XtConverter)(XrmValue*, Cardinal*, XrmValue*, XrmValue*);

/*
 * Defines
 */

#define XtNumber(arr) (sizeof(arr) / sizeof(arr[0]))
#define XtOffset(p_type,field) (offsetof(typeof(*(p_type)1), field))

#define GCForeground		0x01
#define GCBackground		0x02
#define GCFont			0x04
#define GCFunction		0x08
#define GCGraphicsExposures	0x10

#define ZPixmap                2 /* drawable depth */

#define GXcopy QPainter::CompositionMode_Source
#define GXxor QPainter::RasterOp_SourceXorDestination

/* X11 note: Mod2Mask is used by fvwm2 for some obscure purpose,	*/
/* so I went to the other end (Mod5Mask is just below Button1Mask).	*/
/* Thanks to John Heil for reporting the problem and confirming the	*/
/* Mod5Mask solution.							*/

// Modifier shorthands
#define ALT (Qt::ALT)
#define CTRL (Qt::CTRL)
#define CAPSLOCK (Qt::SHIFT)
#define SHIFT (Qt::SHIFT)
#define HOLD (Qt::GroupSwitchModifier << 1)

// Button shorthands
#define BUTTON1 Key_Button1
#define BUTTON2 Key_Button2
#define BUTTON3 Key_Button3
#define BUTTON4 Key_Button4
#define BUTTON5 Key_Button5

// Xt Event Handler masks
#define PointerMotionMask	0x0001
#define ButtonMotionMask	0x0002
#define Button1MotionMask	0x0004
#define Button2MotionMask	0x0008
#define EnterWindowMask		0x0010
#define LeaveWindowMask		0x0020
#define ButtonPressMask		0x0040
#define KeyPressMask		0x0080

// used only in ghostscript background rendering code
#define ClientMessage		0x05

#define None		0
#define NoSymbol	0

#define Key_Button1 Qt::Key_F30
#define Key_Button2 Qt::Key_F31
#define Key_Button3 Qt::Key_F32
#define Key_Button4 Qt::Key_F33
#define Key_Button5 Qt::Key_F34

#define LineSolid		Qt::SolidLine
#define LineOnOffDash		Qt::DashLine

#define CapRound		Qt::RoundCap
#define CapButt			Qt::FlatCap
#define CapProjecting		Qt::SquareCap
#define JoinBevel		Qt::BevelJoin
#define JoinMiter		Qt::MiterJoin

#define FillSolid		0
#define FillStippled		1
#define FillOpaqueStippled	2

#define XtNdrag                "drag"
#define XtNclose               "close"
#define XtNwidth		"width"
#define XtNheight		"height"
#define XtNx			"x"
#define XtNy			"y"
#define XtNfont			"font"
#define XtNexpose		"expose"
#define XtNset			"set"
#define XtNsquare		"square"
#define XtNlabelLocation	"labelLocation"
#define XtNborderWidth		"borderWidth"
#define XtNselect		"select"
#define XtNrelease		"release"
#define XtNlabel		"label"
#define XtNxResizable		"xResizable"
#define XtNyResizable		"yResizable"
#define XtNxRefWidget		"xRefWidget"
#define XtNyRefWidget		"yRefWidget"
#define XtNyAddHeight		"yAddHeight"
#define XtNyAttachBottom	"yAttachBottom"
#define XtNxAttachRight		"xAttachRight"
#define XtNyAddHeight		"yAddHeight"
#define XtNyAttachBottom	"yAttachBottom"
#define XtNxAttachRight		"xAttachRight"
#define XtNyAddHeight          "yAddHeight"
#define XtNyAttachBottom	"yAttachBottom"
#define XtNxAttachRight		"xAttachRight"
#define XtNxAddWidth		"xAddWidth"
#define XtNforeground		"foreground"
#define XtNstring		"string"
#define XtNsetMark		"setMark"
#define XtNbackground		"background"
#define XtNborderColor		"borderColor"
#define XtNrectColor		"rectColor"
#define XtNresize		"resize"
#define XtNlabelType		"labelType"
#define XtNrectStipple		"rectStipple"
#define XtNattachTo		"attachTo"
#define XtNsensitive		"sensitive"
#define XtNxOffset		"xOffset"
#define XtNyOffset		"yOffset"
#define XtNlayout		"layout"
#define XtNinsertPosition	"insertPosition"
#define XtNgravity		"gravity"
#define XtNscroll		"scroll"
#define XtNwrap			"wrap"
#define XtNexecute		"execute"
#define XtNkeyUp		"keyUp"
#define XtNkeyDown		"keyDown"
#define XtNcolormap		"colormap"
#define XtNstrip		"strip"
#define XtNforeground "foreground"

#define XtCHeight "Height"
#define XtCWidth "Width"
#define XtCFont "Font"
#define XtCForeground "Foreground"
#define XtCBackground "Background"

#define XtRFontStruct "FontStruct"
#define XtRString "String"
#define XtRPixel "Pixel"
#define XtRInt "Int"

#define XwLEFT		0
#define XwRECT		1
#define XwIMAGE		2
#define XwIGNORE	3

#define WestGravity	0
#define XwWrapOff	0
#define XwAutoScrollHorizontal	0

#define XwworkSpaceWidgetClass		0x01
#define XwtoggleWidgetClass		0x02
#define XwmenubuttonWidgetClass		0x03
#define XwformWidgetClass		0x04
#define XwmenuButtonWidgetClass		0x05
#define XwbulletinWidgetClass		0x06
#define XwcascadeWidgetClass		0x07
#define XwpopupmgrWidgetClass		0x08
#define XwstaticTextWidgetClass		0x09
#define XwpushButtonWidgetClass		0x0A
#define XwtextEditWidgetClass		0x0B
#define transientShellWidgetClass      0x0C
#define shellWidgetClass               0x0D
#define applicationShellWidgetClass    0x0E

#define XA_STRING ((Atom) 31)
#define PropModeReplace         0

#endif // XCTYPES_H
