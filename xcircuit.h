/*----------------------------------------------------------------------*/
/* xcircuit.h 								*/
/* Copyright (c) 2002  Tim Edwards, Johns Hopkins University        	*/
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*      written by Tim Edwards, 8/20/93    				*/
/*----------------------------------------------------------------------*/

#ifndef XCIRCUIT_H
#define XCIRCUIT_H

#include <stdint.h>

#include <QPoint>
#include <QStringList>

#include "monitoredvar.h"
#include "xctypes.h"
#include "elements.h"

class QAction;
class QScrollBar;

#ifndef HAVE_U_CHAR
typedef unsigned char	u_char;
typedef unsigned short	u_short;
typedef unsigned int	u_int;
typedef unsigned long	u_long;
#endif

#define SetThinLineAttributes	XSetLineAttributes
#define SetLineAttributes(b, c, d, e, f) \
         XSetLineAttributes(b, ((c) >= 1.55 ? (int)(c + 0.45) : 0), d, e, f)
#define flusharea()	

#define Fprintf fprintf
#define Flush fflush

#define Number(a) reinterpret_cast<void*>(static_cast<intptr_t>(a))

/*----------------------------------------------------------------------*/
/* Definition shortcuts 						*/
/*----------------------------------------------------------------------*/

#define XtnSetArg(a,b)  	do { XtSetArg(wargs[n], a, b); n++; } while (0)
template <typename T> static inline T sign(T a) { return (a <= 0) ? -1 : 1; }

/*----------------------------------------------------------------------*/
/* Lengthier define constructs						*/
/*----------------------------------------------------------------------*/

/* Names used for convenience throughout the source code */

#define eventmode	(areawin->event_mode)
#define topobject	(areawin->topinstance->thisobject)
#define hierobject	(areawin->hierstack->thisinst->thisobject)

#define ENDPART		(topobject->end())
#define EDITPART	(topobject->begin() + *areawin->selectlist)

/*----------------------------------------------------------------------*/

typedef struct {
   short width, ascent, descent, base;
} TextExtents;

/*----------------------------------------------------------------------*/
/* Implementation-specific definitions 					*/
/*----------------------------------------------------------------------*/

#define LIBS	    5 /* initial number of library pages 		*/
#define PAGES      10 /* default number of top-level pages 		*/
#define SCALEFAC  1.5 /* zoom in/out scaling multiplier 		*/
#define SUBSCALE 0.67 /* ratio of subscript size to normal script size 	*/
#define SBARSIZE   13 /* Pixel size of the scrollbar 			*/
#define DELBUFSIZE 10 /* Number of delete events to save for undeleting */
#define MINAUTOSCALE 0.75F /* Won't automatically scale closer than this */
#define MAXCHANGES 20 /* Number of changes to induce a temp file save	*/
#define PADSPACE   10 /* Spacing of pinlabels from their origins	*/

#define TBBORDER   1  /* border around toolbar buttons */

#define TOPLEVEL	0
#define SINGLE		1

#define NOFILENAME	(char *)(-1)

#define FONTHEIGHT(y)	 (y->ascent + y->descent + 6) 
#define ROWHEIGHT	 FONTHEIGHT(appdata.xcfont)
#define FILECHARASCENT   (appdata.filefont->ascent)
#define FILECHARHEIGHT   (FILECHARASCENT + appdata.filefont->descent)
#define LISTHEIGHT	 200
#define TEXTHEIGHT	 28 /* Height of xcircuit vectored font at nominal size */
#define BASELINE	 40 /* Height of baseline */
#define DEFAULTGRIDSPACE 32
#define DEFAULTSNAPSPACE 16

/*----------------------------------------------------------------------*/

#define RADFAC 0.0174532925199  /* (pi / 180) */
#define INVRFAC 57.295779 /* (180 / pi) */

/*----------------------------------------------------------------------*/

#define INCHSCALE 0.375     /* Scale of .25 inches to PostScript units */
#define CMSCALE 0.35433071  /* Scale of .5 cm to PostScript units */
#define IN_CM_CONVERT 28.3464567	/* 72 (in) / 2.54 (cm/in) */

/*----------------------------------------------------------------------*/
/* File loading modes							*/
/*----------------------------------------------------------------------*/

enum loadmodes {IMPORT = 1, PSBKGROUND, SCRIPT, RECOVER,
#ifdef ASG
	IMPORTSPICE,
#endif
	LOAD_MODES
};

/*----------------------------------------------------------------------*/
/* Text justification styles and other parameters (bitmask)		*/
/*----------------------------------------------------------------------*/

#define NOTLEFT		1
#define RIGHT		2
#define NOTBOTTOM	4
#define TOP		8
#define FLIPINV		16	/* 1 if text is flip-invariant		*/
#define PINVISIBLE	32	/* 1 if pin visible outside of object	*/
#define PINNOOFFSET	64	/* 0 if pin label offset from position	*/
#define LATEXLABEL	128

#define RLJUSTFIELD	3	/* right-left justification bit field	*/
#define TBJUSTFIELD	12	/* top-bottom justification bit field	*/
#define NONJUSTFIELD	240	/* everything but justification fields	*/

/*----------------------------------------------------------------------*/
/* Reset modes								*/
/*----------------------------------------------------------------------*/

enum resetmode {
    NORMAL = 0,
    SAVE = 1,
    DESTROY = 2
};

/*----------------------------------------------------------------------*/
/* Coordinate display types						*/
/*----------------------------------------------------------------------*/

#define DEC_INCH	0
#define FRAC_INCH	1
#define CM		2
#define INTERNAL	3

/*----------------------------------------------------------------------*/
/* Library types							*/
/*----------------------------------------------------------------------*/

#define FONTENCODING   -1  /* Used only by libopen() */
#define FONTLIB		0
#define PAGELIB		1
#define LIBLIB		2
#define LIBRARY		3
#define USERLIB		(xobjs.numlibs + LIBRARY - 1)

/*----------------------------------------------------------------------*/
/* Box styles								*/
/*----------------------------------------------------------------------*/

//#define NORMAL 0
#define UNCLOSED 1
#define DASHED 2
#define DOTTED 4
#define NOBORDER 8
#define FILLED 16
#define STIP0 32
#define STIP1 64
#define STIP2 128
#define FILLSOLID 224  /* = 32 + 64 + 128 */
#ifdef OPAQUE
#undef OPAQUE
#endif
#define OPAQUE 256
#define BBOX 512
#define SQUARECAP 1024

/*----------------------------------------------------------------------*/
/* Box edit styles							*/
/*----------------------------------------------------------------------*/

#define NONE 0
#define MANHATTAN 1
#define RHOMBOIDX 2
#define RHOMBOIDY 4
#define RHOMBOIDA 8

/*----------------------------------------------------------------------*/
/* Cycle points (selected points) (flag bits, can be OR'd together)	*/
/*----------------------------------------------------------------------*/

#define EDITX 	  0x01
#define EDITY	  0x02
#define LASTENTRY 0x04
#define PROCESS	  0x08
#define REFERENCE 0x10

/*----------------------------------------------------------------------*/
/* Arc creation and edit styles						*/
/*----------------------------------------------------------------------*/

#define CENTER 1
#define RADIAL 2

/*----------------------------------------------------------------------*/
/* Schematic object types and pin label types	   			*/
/*----------------------------------------------------------------------*/

#define PRIMARY 0		/* Primary (master) schematic page */
#define SECONDARY 1		/* Secondary (slave) schematic page */
#define TRIVIAL 2		/* Symbol as non-schematic element */
#define SYMBOL 3		/* Symbol associated with a schematic */
#define FUNDAMENTAL 4		/* Standalone symbol */
#define NONETWORK 5		/* Do not netlist this object */
#define GLYPH 6			/* Symbol is a font glyph */

#define LOCAL 1
#define GLOBAL 2
#define INFO 3

#define HIERARCHY_LIMIT 256	/* Stop if recursion goes this deep */

/*----------------------------------------------------------------------*/
/* Save types.	This list incorporates "ALL_PAGES", below.		*/
/*----------------------------------------------------------------------*/

#define CURRENT_PAGE	0	/* Current page + all associated pages	*/
#define NO_SUBCIRCUITS	1	/* Current page w/o subcircuit pages	*/

/*----------------------------------------------------------------------*/
/* Modes used when ennumerating page totals.				*/
/*----------------------------------------------------------------------*/

#define INDEPENDENT	0
#define DEPENDENT	1
#define TOTAL_PAGES	2
#define LINKED_PAGES	3
#define PAGE_DEPEND	4
#define ALL_PAGES	5

/*----------------------------------------------------------------------*/
/* Color scheme styles (other than NORMAL)				*/
/*----------------------------------------------------------------------*/

#define INVERSE		1

/*----------------------------------------------------------------------*/
/* Cursor definitions 							*/
/*----------------------------------------------------------------------*/

#define NUM_CURSORS	11

#define ARROW 		appcursors[0]
#define CROSS 		appcursors[1]
#define SCISSORS 	appcursors[2]
#define COPYCURSOR	appcursors[3]
#define ROTATECURSOR    appcursors[4]
#define EDCURSOR	appcursors[5]
#define TEXTPTR 	appcursors[6]
#define CIRCLE		appcursors[7]
#define QUESTION	appcursors[8]
#define WAITFOR		appcursors[9]
#define HAND		appcursors[10]

#define DEFAULTCURSOR	(*areawin->defaultcursor)

/*----------------------------------------------------------------------*/
/* Allowed parameterization types					*/
/*----------------------------------------------------------------------*/

enum paramwhich {
   P_NUMERIC = 0,	/* uncommitted numeric parameter */
   P_SUBSTRING,
   P_POSITION_X,
   P_POSITION_Y,
   P_STYLE,
   P_JUSTIFY,
   P_ANGLE1,
   P_ANGLE2,
   P_RADIUS,
   P_MINOR_AXIS,
   P_ROTATION,
   P_SCALE,
   P_LINEWIDTH,
   P_COLOR,
   P_EXPRESSION,
   P_POSITION, 		/* mode only, not a real parameter */
   NUM_PARAM_TYPES
};

/*----------------------------------------------------------------------*/
/* structures of all main elements which can be displayed & manipulated	*/
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/* selection-mechanism structures					*/
/*----------------------------------------------------------------------*/

class uselection {
public:
    short number;
    genericptr * const element;
    short * const idx;
    uselection(objinstptr, short*, int);
    ~uselection();
    short* regen(objinstptr);
};
NO_FREE(uselection *);

/* Linked-list for objects */

struct objlist {
   int		libno;		/* library in which object appears */
   object*	thisobject;	/* pointer to the object */
   objlist*	next;
};
typedef objlist *objlistptr;

/* Linked-list for object instances */

struct pushlist {
   objinst*	thisinst;
   pushlist*	next;
};
typedef pushlist *pushlistptr;

/* Same as above, but for the list of instances in a library, so it	*/
/* needs an additional record showing whether or not the instance is	*/
/* "virtual" (not the primary library instance of the object)		*/

struct liblist {
   objinstptr	thisinst;
   u_char	isvirtual;
   liblist*	next;
};
typedef liblist *liblistptr;

/*----------------------------------------------------------------------*/
/* selection from top-level object 					*/
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/* Undo mechanism definitions						*/
/*----------------------------------------------------------------------*/

enum UNDO_MODES {UNDO_DONE = 0, UNDO_MORE, MODE_CONNECT, MODE_RECURSE_WIDE,
	MODE_RECURSE_NARROW};

class selection {
public:
    int selects;
    short *selectlist;
    objinst *thisinst;
    selection *next;
    inline selection() : selectlist(NULL) {}
    ~selection();
};
NO_FREE(selection*);

#define select_element(a) recurse_select_element(a, UNDO_MORE) 
#define select_add_element(a) recurse_select_element(a, UNDO_DONE) 

#define easydraw(a, b)	 geneasydraw(a, b, topobject, areawin->topinstance)

/*----------------------------------------------------------------------*/
/* Netlist structures for schematic capture				*/
/*----------------------------------------------------------------------*/

/* Structure to hold net and subnet IDs for a bus */

typedef struct {
   int netid;
   int subnetid;
} buslist;

/* Structure mimicking the top part of a Polylist or Labellist	*/
/* when we just want the netlist information and don't care	*/
/* which one we're looking at.					*/

typedef struct {
   union {
      int id;
      buslist *list;
   } net;
   int subnets;
} Genericlist; 

/* Linked polygon list */

struct object;
typedef object *objectptr;

typedef struct _Polylist *PolylistPtr;
typedef struct _Polylist
{
   union {
      int  id;		/* A single net ID, if subnets == 0	*/
      buslist *list;	/* List of net and subnet IDs for a bus	*/
   } net;
   int subnets;		/* Number of subnets; 0 if no subnets	*/
   objectptr cschem;	/* Schematic containing the polygon	*/
   polyptr poly;
   PolylistPtr next;	/* Next polygon in the linked list	*/
} Polylist;

/* Linked label list */

typedef struct _Labellist *LabellistPtr;
typedef struct _Labellist
{
   union {
      int  id;		/* A single net ID, if subnets == 0	*/
      buslist *list;	/* List of net and subnet IDs for a bus	*/
   } net;
   int subnets;		/* Number of subnets; 0 if no subnets	*/
   objectptr cschem;	/* Schematic containing the label	*/
   objinstptr cinst;	/* Specific label instance, if applicable */
   labelptr label;
   LabellistPtr next;
} Labellist;

/* List of object's networks by (flattened) name */

typedef struct _Netname *NetnamePtr;
typedef struct _Netname
{
   int netid;
   stringpart *localpin;
   NetnamePtr next;
} Netname;

/* List of object's I/O ports */

typedef struct _Portlist *PortlistPtr;
typedef struct _Portlist
{
   int portid;
   int netid;
   PortlistPtr next;
} Portlist;

/* List of calls to instances of objects */
/* or subcircuit objects.		 */

typedef struct _Calllist *CalllistPtr;
typedef struct _Calllist
{
   objectptr   cschem;		/* Schematic containing the instance called */
   objinstptr  callinst;	/* Instance called */
   objectptr   callobj;		/* Object of instance called */
   char	       *devname;	/* if non-null, name of device in netlist */
   int         devindex;	/* if non-negative, index of device in netlist */
   PortlistPtr ports;
   CalllistPtr next;
} Calllist;

/* PCB netlist structures */

struct Pnet {
   int numnets;
   int *netidx;
   struct Pnet *next;
};

struct Pstr {
   stringpart *string;
   struct Pstr *next;
};

struct Ptab {
   objectptr cschem;
   struct Pnet *nets;
   struct Pstr *pins;
   struct Ptab *next;
};

/*----------------------------------------------------------------------*/
/* Information needed to highlight a net				*/
/*----------------------------------------------------------------------*/

typedef struct {
   Genericlist	*netlist;
   objinstptr	thisinst;
} Highlight;

/*----------------------------------------------------------------------*/
/* Main object structure						*/
/*----------------------------------------------------------------------*/

class object : public Plist {
public:
   char		name[80];
   u_short	changes;	/* Number of unsaved changes to object */
   bool	hidden;
   float	viewscale;
   XPoint	pcorner;	/* position relative to window */
   BBox		bbox;		/* bounding box information (excluding */
                                /* parameterized elements) */
   oparamptr	params;		/* list of parameters, with default values */

   Highlight	highlight;	/* net to be highlighted on redraw */
   u_char	schemtype;
   objectptr	symschem;	/* schematic page support */
   bool	valid;		/* Is current netlist valid? */
   bool	traversed;	/* Flag to indicate object was processed */
   LabellistPtr	labels;		/* Netlist pins	 */
   PolylistPtr	polygons;	/* Netlist wires */
   PortlistPtr  ports;		/* Netlist ports */
   CalllistPtr  calls;		/* Netlist subcircuits and connections */
   NetnamePtr   netnames;	/* Local names for flattening */
                                /* (this probably shouldn't be here. . .) */
   object();
   ~object();
   void clear();
   void clear_nodelete();
   bool operator==(const object &) const;
   short find(object* other) const;
protected:
   void set_defaults();
};
NO_FREE(object*);

/*----------------------------------------------------------------------*/
/* button tap/press definitions 					*/
/*----------------------------------------------------------------------*/

#define PRESSTIME	200	/* milliseconds of push to be a "press" */

/*----------------------------------------------------------------------*/
/* object name alias structures						*/
/*----------------------------------------------------------------------*/

struct stringlist {
   char *alias;
   stringlist* next;
};
typedef stringlist *slistptr;

struct alias {
   objectptr baseobj;       /* pointer to object (actual name) */
   slistptr aliases;    /* linked list of alias names */
   alias* next;
};
typedef struct alias *aliasptr;

/*----------------------------------------------------------------------*/
/* To facilitate compiling the Tcl/Tk version, we make some convenient	*/
/* definitions for types "xc..." and "Xc..." which can be mapped both	*/
/* to X and Xt or to Tk functions and types.				*/
/*----------------------------------------------------------------------*/

#ifndef ClientData
   #define ClientData			XtPointer
#endif

/*----------------------------------------------------------------------*/
/* structures for managing the popup prompt widgets 			*/
/*----------------------------------------------------------------------*/

typedef struct {
   QAction*	button;
   int		foreground;
   ActionCallback buttoncall;
   void		*dataptr;
} buttonsave;

typedef struct {
   Widget	popup;		/* Popup widget		*/
   Widget	textw;		/* Text entry widget	*/
   Widget	filew;		/* File list window 	*/
   Widget	scroll;		/* Scrollbar widget	*/
   //void		(*setvalue)();	/* Callback function	*/
   WidgetCallback setvalue;
   buttonsave	*buttonptr;	/* Button widget calling popup	     */
   char		*filter;	/* Extension filter for highlighting */
				/*  files. NULL for no file window.  */
				/*  NULL-string for no highlights.   */
} popupstruct;

typedef struct {
   Widget	textw;
   Widget	buttonw;
   PopupCallback setvalue;
   void		*dataptr;
} propstruct;

typedef struct {
   char		*filename;
   int		filetype;
} fileliststruct;

enum {DIRECTORY = 0, MATCH, NONMATCH};	/* file types */

/*----------------------------------------------------------------------*/
/* Initial Resource Management						*/
/*----------------------------------------------------------------------*/

typedef struct {
   /* schematic layout colors */
   Pixel	globalcolor, localcolor, infocolor, ratsnestcolor;

   /* non-schematic layout color(s) */
   Pixel	bboxpix;

   /* color scheme 1 */
   Pixel	fg, bg;
   Pixel	gridpix, snappix, selectpix, axespix;
   Pixel	buttonpix, filterpix, auxpix, barpix, parampix;
   Pixel querypix;

   /* color scheme 2 */
   Pixel	fg2, bg2;
   Pixel	gridpix2, snappix2, selectpix2, axespix2;
   Pixel	buttonpix2, auxpix2, parampix2;
   Pixel querypix2, filterpix2, barpix2;

   int		width, height, timeout;
   XFontStruct	*filefont;

#ifndef TCL_WRAPPER
   XFontStruct	*xcfont, *textfont, *titlefont, *helpfont;
#endif

} ApplicationData, *ApplicationDataPtr;

/*----------------------------------------------------------------------*/
/* Font information structure						*/
/*----------------------------------------------------------------------*/
/* Flags:   bit  description						*/
/* 	    0    bold = 1,  normal = 0					*/
/*	    1    italic = 1, normal = 0					*/
/*	    2    <reserved, possibly for narrow font type>		*/
/*	    3    drawn = 1,  PostScript = 0				*/
/* 	    4	 <reserved, possibly for LaTeX font type>		*/
/*	    5    special encoding = 1, Standard Encoding = 0		*/
/*	    6	 ISOLatin1 = 2, ISOLatin2 = 3				*/
/*	    7    <reserved for other encoding schemes>			*/
/*----------------------------------------------------------------------*/

typedef struct {
   char *psname;
   char *family;
   float scale;
   u_short flags;
   objectptr *encoding;
} fontinfo;

/*----------------------------------------------------------------------*/

class psbkground {
public:
   QString name;
   BBox bbox;
};
NO_FREE(psbkground*);

/*----------------------------------------------------------------------*/
/* Key macro information						*/
/*----------------------------------------------------------------------*/

struct keybinding {
   Widget window;		/* per-window function, or NULL */
   int keywstate;
   int function;
   short value;
   keybinding* nextbinding;
};
typedef keybinding *keybindingptr;

/*----------------------------------------------------------------------*/
/* Enumeration of functions available for binding to keys/buttons	*/
/* IMPORTANT!  Do not alter this list without also fixing the text	*/
/* in keybindings.c!							*/
/*----------------------------------------------------------------------*/

enum {
   XCF_ENDDATA = -2,		XCF_SPACER  /* -1 */,
   XCF_Page	 /* 0 */,	XCF_Justify /* 1 */,
   XCF_Superscript /* 2 */,	XCF_Subscript /* 3 */,
   XCF_Normalscript /* 4 */,	XCF_Font /* 5 */,
   XCF_Boldfont /* 6 */,	XCF_Italicfont /* 7 */,
   XCF_Normalfont /* 8 */,	XCF_Underline /* 9 */,
   XCF_Overline /* 10 */,	XCF_ISO_Encoding /* 11 */,
   XCF_Halfspace /* 12 */,	XCF_Quarterspace /* 13 */,
   XCF_Special /* 14 */,	XCF_TabStop /* 15 */,
   XCF_TabForward /* 16 */,	XCF_TabBackward /* 17 */,
   XCF_Text_Return /* 18 */,	XCF_Text_Delete /* 19 */,
   XCF_Text_Right /* 20 */,	XCF_Text_Left /* 21 */,
   XCF_Text_Up /* 22 */,	XCF_Text_Down /* 23 */,
   XCF_Text_Split /* 24 */,	XCF_Text_Home /* 25 */,
   XCF_Text_End /* 26 */,	XCF_Linebreak /* 27 */,
   XCF_Parameter /* 28 */,	XCF_Edit_Param /* 29 */,
   XCF_ChangeStyle /* 30 */,	XCF_Edit_Delete /* 31 */,
   XCF_Edit_Insert /* 32 */,	XCF_Edit_Append /* 33 */,
   XCF_Edit_Next /* 34 */,	XCF_Attach /* 35 */,
   XCF_Next_Library /* 36 */,	XCF_Library_Directory /* 37 */,
   XCF_Library_Move /* 38 */,	XCF_Library_Copy /* 39 */,
   XCF_Library_Edit /* 40 */,	XCF_Library_Delete /* 41 */,
   XCF_Library_Duplicate /* 42 */, XCF_Library_Hide /* 43 */,
   XCF_Library_Virtual /* 44 */, XCF_Page_Directory /* 45 */,
   XCF_Library_Pop /* 46 */,	XCF_Virtual /* 47 */,
   XCF_Help /* 48 */,		XCF_Redraw /* 49 */,
   XCF_View /* 50 */,		XCF_Zoom_In /* 51 */,
   XCF_Zoom_Out /* 52 */,	XCF_Pan /* 53 */,
   XCF_Double_Snap /* 54 */,	XCF_Halve_Snap /* 55 */,
   XCF_Write /* 56 */,		XCF_Rotate /* 57 */,
   XCF_Flip_X /* 58 */,		XCF_Flip_Y /* 59 */,
   XCF_Snap /* 60 */,		XCF_SnapTo /* 61 */,
   XCF_Pop /* 62 */,		XCF_Push /* 63 */,
   XCF_Delete /* 64 */,		XCF_Select /* 65 */,
   XCF_Box /* 66 */,		XCF_Arc /* 67 */,
   XCF_Text /* 68 */,		XCF_Exchange /* 69 */,
   XCF_Copy /* 70 */,		XCF_Move /* 71 */,
   XCF_Join /* 72 */,		XCF_Unjoin /* 73 */,
   XCF_Spline /* 74 */,		XCF_Edit /* 75 */,
   XCF_Undo /* 76 */,		XCF_Redo /* 77 */,
   XCF_Select_Save /* 78 */,	XCF_Unselect /* 79 */,
   XCF_Dashed /* 80 */,		XCF_Dotted /* 81 */,
   XCF_Solid /* 82 */,		XCF_Prompt /* 83 */,
   XCF_Dot /* 84 */,		XCF_Wire /* 85 */,
   XCF_Cancel /* 86 */,		XCF_Nothing /* 87 */,
   XCF_Exit /* 88 */,		XCF_Netlist /* 89 */,
   XCF_Swap /* 90 */,		XCF_Pin_Label /* 91 */,
   XCF_Pin_Global /* 92 */,	XCF_Info_Label /* 93 */,
   XCF_Graphic	/* 94 */,	XCF_SelectBox /* 95 */,
   XCF_Connectivity /* 96 */,	XCF_Continue_Element /* 97 */,
   XCF_Finish_Element /* 98 */, XCF_Continue_Copy /* 99 */,
   XCF_Finish_Copy /* 100 */,	XCF_Finish /* 101 */,
   XCF_Cancel_Last /* 102 */,	XCF_Sim /* 103 */,
   XCF_SPICE /* 104 */,		XCF_PCB /* 105 */,
   XCF_SPICEflat /* 106 */,	XCF_Rescale /* 107 */,		
   XCF_Text_Delete_Param /* 108 */,
   NUM_FUNCTIONS
};

/*----------------------------------------------------------------------*/
/* Per-drawing-page parameters						*/
/*----------------------------------------------------------------------*/

class Pagedata {
public:
   /* per-drawing-page parameters */
   objinstptr	pageinst;
   QString     filename;	/* file to save as */
   u_char	idx;		/* page index */
   psbkground	background;	/* background rendered file info */
   float	wirewidth;
   float	outscale;
   float	gridspace;
   float	snapspace;
   short	orient;
   short	pmode;
   short	coordstyle;
   XPoint	drawingscale;
   XPoint	pagesize;	/* size of page to print on */
   XPoint	margins;
};
NO_FREE(Pagedata*);

/*----------------------------------------------------------------------*/
/* Structure holding information about graphic images used.  These hold	*/
/* the original data for the images, and may be shared.			*/
/*----------------------------------------------------------------------*/

typedef struct {
   XImage	*image;
   int		refcount;
   char		*filename;
} Imagedata;

/*----------------------------------------------------------------------*/
/* The main globally-accessible data structure.  This structure holds	*/
/* all the critical data needed by the drawing window 			*/
/*----------------------------------------------------------------------*/

class Area;
class Matrix;

struct XCWindowData {

   XCWindowData* next;	/* next window in list */

   /* widgets */
   QWidget      *viewport;
   Area         *area;
   QObject      *menubar;
   XtIntervalId time_id;

   /* global page parameters */
   short	width, height;
   short	page;
   float	vscale;		/* proper scale */
   XPoint	pcorner;	/* page position */

   /* global option defaults */
   float	textscale;
   float	linewidth;
   float	zoomfactor;
   short	psfont;
   short	justify;
   u_short	style;
   int		color;
   short	filter;		/* selection filter */
   bool	manhatn;
   bool	boxedit;
   bool	snapto;
   bool      antialias;
   bool	bboxon;
   bool	center;
   bool	gridon;
   bool	axeson;
   bool	invert;
   bool	mapped;		/* indicates if window is drawable */
   char		buschar;	/* Character indicating vector notation */
   bool	editinplace;
   bool	pinpointon;
   bool	pinattach;	/* keep wires attached to pins when moving objinsts */
   bool	toolbar_on;

   /* buffers and associated variables */
   XPoint	save, origin;
   short	selects;
   short	*selectlist;
   short	attachto;
   short	lastlibrary;
   short	textpos;
   short	textend;
   objinstptr	topinstance;
   objectptr	editstack;
   pushlistptr	stack;
   pushlistptr	hierstack;
   EventMode	event_mode;
   char		*lastbackground;
   Cursor	*defaultcursor;

   void update() const;
};
typedef XCWindowData *XCWindowDataPtr;

/* Record for undo function */ 

struct Undostack {
   Undostack* next;	/* next record in undo stack */
   Undostack* last;	/* double-linked for "redo" function */
   u_int type;		/* type of event */
   short idx;		/* counter for undo event */
   objinstptr thisinst;	/* instance of object in which event occurred */
   XCWindowData *window;/* window in which event occurred */
   int idata;		/* simple undedicated integer datum */
   char *undodata;	/* free space to be malloc'd */
			/* (size dependent on "type") */
};
typedef Undostack *Undoptr;

/* This whole thing needs to be cleaned up. . . now I've got objects in */
/* "library" which are repeated both in the "instlist" pair and the	*/
/* "thisobject" pointer in each instance.  The instance is repeated on	*/
/* the library page.  Ideally, the instance list should only exist on	*/
/* the library page, and not be destroyed on every call to "composelib"	*/

typedef struct {
   short	number;
   objectptr	*library;
   liblistptr	instlist;	/* List of instances */
} Library;

typedef struct _Technology *TechPtr;

typedef struct _Technology {
   u_char	flags;		/* Flags for library page (changed, read-only) */
   char		*technology;	/* Namespace name (without the "::") */
   char		*filename;	/* Library file associated with technology */
   TechPtr 	next;		/* Linked list */
} Technology;

/* Known flags for library pages */

#define LIBRARY_CHANGED		0x01
#define LIBRARY_READONLY	0x02
#define LIBRARY_REPLACE		0x04	/* Replace instances when reading */
#define LIBRARY_REPLACE_TEMP	0x08	/* Temporary store */
#define LIBRARY_USED		0x10	/* Temporary marker flag */

/*----------------------------------------------------------------------*/
/* A convenient structure for holding all the object lists		*/
/*----------------------------------------------------------------------*/

class Pagelist : public QList<Pagedata> {
public:
    Pagelist() : QList<Pagedata>() {}
    int indexOf(objinstptr exchobj) {
        for (int i=0; i<count(); ++i) {
            if (at(i).pageinst == exchobj) {
                return i;
            }
        }
        Q_ASSERT(false);
        return -1;
    }
    inline operator int() { return count(); }
};
NO_FREE(Pagelist*);

class Globaldata {
public:
   QStringList libsearchpath;	 /* list of directories to search */
   QStringList filesearchpath; /* list of directories to search */
   QString tempfile;
   QString tempdir;
   bool	retain_backup;
   XtIntervalId	timeout_id;
   int		save_interval;
   bool	filefilter;	/* Is the file list filtered? */
   bool	hold;		/* allow HOLD modifiers on buttons */
   bool	showtech;	/* Write technology names in library */
   u_short	new_changes;
   char		suspend;	/* suspend graphics updates if true */
   short	numlibs;
   Pagelist&   pages;
   Pagelist    pagelist;
   Undoptr	undostack;	/* actions to undo */
   Undoptr	redostack;	/* actions to redo */
   Library	fontlib;
   Library	*userlibs;
   TechPtr 	technologies;
   objinstptr   *libtop;
   Imagedata	*imagelist;
   short	images;
   XCWindowData *windowlist;	/* linked list of known windows */

   Globaldata() : pages(pagelist) {}
};
NO_FREE(Globaldata*);

/*----------------------------------------------------------------------*/
/* structures previously defined in menudefs.h				*/
/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
/* Menu Definitions for hierarchical pulldown menus                     */
/*----------------------------------------------------------------------*/

#ifndef TCL_WRAPPER

typedef struct _menustruct *menuptr;

typedef struct _menustruct {
        const char *name;
        menuptr submenu;
        short size;
        ActionCallback func;
        void *passeddata;
} menustruct;

/*----------------------------------------------------------------------*/
/* Structure for calling routines from the Toolbar icons		*/
/*----------------------------------------------------------------------*/

typedef struct _toolbarstruct *toolbarptr;

typedef struct _toolbarstruct {
        const char *name;
        const char **icon_data;
        ActionCallback func;
        void *passeddata;
        const char *hint;
} toolbarstruct;

#endif
/* Menus and Toolbars are taken care of entirely by scripts in the Tcl/	*/
/* Tk version of xcircuit.						*/
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/* Global variables                                                     */
/*----------------------------------------------------------------------*/

extern int	 pressmode;   /* Whether we are in a press & hold state */
extern XCWindowData *areawin;
extern Globaldata xobjs;

/* conversions from a selection to a specific type */

static inline genericptr * SELTOGENERICPTR(short* a) {
    return  (areawin->hierstack == NULL) ?
            (topobject->begin() + *a) :
            (hierobject->begin() + *a);
}

static inline genericptr * SELTOGENERICPTR(short a) {
    return  (areawin->hierstack == NULL) ?
            (topobject->begin() + a) :
            (hierobject->begin() + a);
}

static inline polyptr SELTOPOLY(short* a) { return TOPOLY(SELTOGENERICPTR(a)); }
static inline labelptr SELTOLABEL(short* a) { return TOLABEL(SELTOGENERICPTR(a)); }
static inline objinstptr SELTOOBJINST(short* a) { return TOOBJINST(SELTOGENERICPTR(a)); }
static inline arcptr SELTOARC(short* a) { return TOARC(SELTOGENERICPTR(a)); }
static inline splineptr SELTOSPLINE(short* a) { return TOSPLINE(SELTOGENERICPTR(a)); }
static inline pathptr SELTOPATH(short* a) { return TOPATH(SELTOGENERICPTR(a)); }
static inline graphicptr SELTOGRAPHIC(short* a) { return TOGRAPHIC(SELTOGENERICPTR(a)); }
static inline genericptr SELTOGENERIC(short* a) { return TOGENERIC(SELTOGENERICPTR(a)); }

static inline u_short SELECTTYPE(short* a) { return SELTOGENERIC(a)->type & ALL_TYPES; }
static inline int SELTOCOLOR(short* a) { return SELTOGENERIC(a)->color; }

QAction* menuAction(const char *m);

#endif
