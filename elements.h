#ifndef ELEMENTS_H
#define ELEMENTS_H

#include <QVector>

#include "xctypes.h"
#include "colors.h"

/*----------------------------------------------------------------------*/
/* Labels are constructed of strings and executables 			*/
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/* Text string part: types						*/
/*----------------------------------------------------------------------*/

#define TEXT_STRING	 0  /* data is a text string 			*/
#define SUBSCRIPT	 1  /* start subscript; no data			*/
#define SUPERSCRIPT	 2  /* start superscript; no data		*/
#define NORMALSCRIPT	 3  /* stop super-/subscript; no data		*/
#define UNDERLINE	 4  /* start underline; no data			*/
#define OVERLINE	 5  /* start overline; no data			*/
#define NOLINE		 6  /* stop over-/underline; no data		*/
#define TABSTOP	 	 7  /* insert tab stop position			*/
#define TABFORWARD 	 8  /* insert tab stop position			*/
#define TABBACKWARD 	 9  /* insert tab stop position			*/
#define HALFSPACE	10  /* insert half-space; no data		*/
#define QTRSPACE	11  /* insert quarter space; no data		*/
#define RETURN		12  /* carriage-return character; no data	*/
#define FONT_NAME       13  /* inline font designator; data = font name */
#define FONT_SCALE	14  /* font scale change; data = scale		*/
#define FONT_COLOR	15  /* font color change; data = color		*/
#define KERN		16  /* set new kern values; data = kern x, y	*/
#define PARAM_START	17  /* bounds a parameter; data = param key	*/
#define PARAM_END	18  /* bounds a parameter; no data 		*/

/* Actions translated to keystates (numbering continues from above) */

#define TEXT_RETURN	19
#define TEXT_HOME	20
#define TEXT_END	21
#define TEXT_SPLIT	22
#define TEXT_DOWN	23
#define TEXT_UP		24
#define TEXT_LEFT	25
#define TEXT_RIGHT	26
#define TEXT_DELETE	27
#define TEXT_DEL_PARAM	28

#define SPECIAL		63  /* used only when called from menu		*/
#define NULL_TYPE	255 /* used as a placeholder			*/

typedef struct {
   short number;
   u_char flags;
} pointselect;

class stringpart {
public:
    stringpart* nextpart;
    char        type;
    union {
        char	*string; // FIXME: could have been
        int	color;
        int	font;
        float	scale;
        short	kern[2];
    } data;
    stringpart() : nextpart(NULL), type(NULL_TYPE) {}
};

/*----------------------------------------------------------------------*/
/* Object & object instance parameter structure				*/
/* (Add types as necessary)						*/
/*----------------------------------------------------------------------*/

/// \todo perhaps use a QVariant for <parameter>

enum paramtypes {XC_INT = 0, XC_FLOAT, XC_STRING, XC_EXPR};

/* Object parameters: general key:value parameter model */
/* Note that this really should be a hash table, not a linked list. . . */

class oparam {
public:
   char *	key;		/* name of the parameter */
   u_char	type;		/* type is from paramtypes list above */
   u_char	which;		/* what the parameter represents (P_*) */
   union {
      stringpart *string;	/* xcircuit label type */
      char	 *expr;		/* TCL (string) expression */
      int	  ivalue;	/* also covers type short int by typecasting */
      float	  fvalue;
   } parameter;			/* default or substitution value */
   oparam*	next;		/* next parameter in linked list */
   ~oparam();
};
typedef oparam *oparamptr;
NO_FREE(oparamptr);

/* Element parameters: reference back to the object's parameters */
/* These parameters are forward-substituted when descending into */
/* an object instance.						 */
/* Note that this really should be a hash table, not a linked list. . . */

class eparam {
public:
   char *	key;	    /* name of the parameter */
   u_char	flags;	    /* namely, bit declaring an indirect parameter */
   union {
      int	pointno;    /* point number in point array, for polygons */
      short	pathpt[2];  /* element number and point number, for paths */
      char	*refkey;    /* parameter reference key, for instances */
   } pdata;
   eparam*	next;	    /* next parameter in linked list */
   ~eparam();
};
typedef eparam *eparamptr;
NO_FREE(eparamptr);

#define P_INDIRECT	0x01	/* indirect parameter indicator */

/*----------------------------------------------------------------------*/
/* Basic element types 							*/
/*									*/
/* These values determine how a genericptr should be cast into one of	*/
/* labelptr, polyptr, etc.  To query the element type, use the macros	*/
/* ELEMENTTYPE(), IS_LABEL(), IS_POLYGON(), etc.                      */
/*----------------------------------------------------------------------*/

/* Single-bit flags */
enum Type {
    OBJINST = 0x01,
    LABEL = 0x02,
    POLYGON = 0x04,
    ARC = 0x08,
    SPLINE = 0x10,
    PATH = 0x20,
    GRAPHIC = 0x40,
    ARRAY = 0x80,      // unused for now
    ALL_TYPES = OBJINST | LABEL | POLYGON | ARC | SPLINE | PATH | GRAPHIC | ARRAY
};

/*----------------------------------------------------------------------*/
/* Generic element type	is a superset of all elements.			*/
/*----------------------------------------------------------------------*/

class DrawContext;
class objinst;
class generic {
public:
    const Type type;
    int		color;
    eparamptr	passed;
    virtual ~generic();
    void indicateparams(DrawContext*);
    virtual generic* copy() const = 0;
    virtual void draw(DrawContext*) const {}
    virtual void indicate(DrawContext*, eparamptr, oparamptr) const {}
    void getBbox(XPoint* pts, float scale = 0.0) const;
    void getBbox(XPoint* pts, int extend, float scale = 0.0) const;
    void getBbox(XPoint* pts, objinst* callinst, float scale = 0.0) const;
    virtual float rescaleBox(const XPoint & corner, XPoint newpoints[5]) const { Q_UNUSED(corner); Q_UNUSED(newpoints); return 0.0; }
    virtual void calc() {}
    virtual void reverse() {}
    bool operator==(const generic&) const;
protected:
    virtual void doGetBbox(XPoint*, float scale, int extend, objinst* callinst) const { Q_UNUSED(scale); Q_UNUSED(extend); Q_UNUSED(callinst); }
    inline generic(Type _type) : type(_type), color(DEFAULTCOLOR), passed(NULL) {}
    generic(const generic&);
    generic & operator=(const generic &);
    virtual bool isEqual(const generic&) const;
private:
    generic();
};
typedef generic *genericptr;
NO_FREE(genericptr);

/*----------------------------------------------------------------------*/
/* Generic parent class that holds multiple children */
/* Designed to be used with multiple inheritance. */
/*----------------------------------------------------------------------*/

class Plist {
public:
    short	parts;
private:
    genericptr	*plist;
public:
    typedef const genericptr * const_iterator;
    typedef genericptr * iterator;

    inline const_iterator begin() const { return plist; }
    inline const_iterator end() const { return plist + parts; }
    inline iterator begin() { return plist; }
    inline iterator end() { return plist + parts; }

    template <typename T> class type_iterator {
    public:
        inline type_iterator() : p(NULL) {}
        //inline explicit iterator(generic** ptr) : p(ptr) {}
        inline T* operator->() const { return ((*p)->type == T::deftype()) ? *(T**)p : NULL; }
        inline T* operator*() const { return ((*p)->type == T::deftype()) ? *(T**)p : NULL; }
        inline operator T*() const { return ((*p)->type == T::deftype()) ? *(T**)p : NULL; }
        inline operator bool() const { return p != NULL; }
        inline bool operator==(generic* const* other) const { return p == other; }
        inline bool operator!=(generic* const* other) const { return p != other; }
        inline bool operator<(generic* const* other) const { return p < other; }
        inline bool operator==(generic* other) const { return p && *p == other; }
        inline bool operator!=(generic* other) const { return !p || *p != other; }
        inline bool operator==(T* other) const { return p && *p == other; }
        inline bool operator!=(T* other) const { return !p || *p != other; }
        inline int operator-(generic* const* other) const { return p - other; }
        inline type_iterator& operator=(generic* const* other) { p = other; return *this; }
        inline type_iterator& operator++() { ++p; return *this; }
        inline void clear() { if (p) *const_cast<generic**>(p) = NULL; }
        inline generic** peek() const { return (generic**)p; }
    private:
        inline type_iterator& operator=(void *);
        generic* const* p;
    };

    template <typename T> inline T** append(T* ptr = 0) {
        return reinterpret_cast<T**>(append(static_cast<genericptr>(ptr)));
    }
    template <typename T> inline T** temp_append(T* ptr = 0) {
        return reinterpret_cast<T**>(temp_append(static_cast<genericptr>(ptr)));
    }
    genericptr* append(genericptr ptr = 0);
    genericptr* temp_append(genericptr ptr = 0);
    void replace_last(genericptr ptr);
    inline genericptr take_last() {
        return *(plist+(--parts));
    }
    inline genericptr operator[](int i) const { return plist[i]; }
    inline genericptr at(int i) const { return plist[i]; }

    Plist();
    Plist(const Plist &);
    ~Plist();
    Plist & operator=(const Plist &);
    void clear();
    template <typename T> inline bool values(type_iterator<T>& it) const {
        if (!it) it = begin(); else ++it;
        while (it < end()) {
            if (*it) return true;
            ++it;
        }
        return false;
    }
    inline bool values(const genericptr*& it) const {
        if (!it) it = begin(); else ++it;
        return it < end();
    }
    inline bool values(genericptr*& it) {
        if (!it) it = begin(); else ++it;
        return it < end();
    }
    bool operator==(const Plist & other) const;
};
NO_FREE(Plist*);

/*----------------------------------------------------------------------*/
/* Generalized positionable object			*/
/*----------------------------------------------------------------------*/

class positionable : public generic {
public:
    XPoint  position;
    short   rotation;
    float   scale;
    virtual float rescaleBox(const XPoint & corner, XPoint newpoints[5]) const;
protected:
    inline positionable(Type _type) : generic(_type), position(0, 0), rotation(0), scale(1.0) {}
    inline positionable(Type _type, const XPoint & pos) : generic(_type), position(pos), rotation(0), scale(1.0) {}
    positionable(const positionable&);
    positionable & operator=(const positionable &);
private:
    positionable();
};
NO_FREE(positionable*);

/*----------------------------------------------------------------------*/
/* Generalized graphic object			*/
/*----------------------------------------------------------------------*/

class graphic : public positionable {
public:
    /* color is foreground, for bitmaps only */
    XImage	  *source;	/* source data */
    XImage	  *target;	/* target (scaled) data */
    mutable short	  trot;		/* target rotation */
    mutable float	  tscale;	/* target scale (0 = uninitialized) */
    bool	  valid;	/* does target need to be regenerated? */
    graphic();
    graphic(const graphic&);
    ~graphic();
    graphic & operator=(const graphic &);
    generic* copy() const;
    void draw(DrawContext*) const;
    void indicate(DrawContext*, eparamptr, oparamptr) const;
protected:
    void doGetBbox(XPoint*, float scale, int extend, objinst* callinst) const;
    bool transform(DrawContext*) const;
    static inline Type deftype() { return GRAPHIC; }
};
typedef graphic *graphicptr;
typedef Plist::type_iterator<graphic> graphiciter;
NO_FREE(graphicptr);

/*----------------------------------------------------------------------*/
/* Object instance type							*/
/*----------------------------------------------------------------------*/

class object;

class objinst : public positionable {
public:
    object*	thisobject;
    oparamptr	params;		/* parameter substitutions for this instance */
    BBox	bbox;		/* per-instance bounding box information */
    BBox	*schembbox;	/* Extra bounding box for pin labels */
    objinst();
    objinst(const objinst &);
    objinst(object* thisobj, int x = 0, int y = 0);
    ~objinst();
    objinst & operator=(const objinst &);

    generic* copy() const;
    void indicate(DrawContext*, eparamptr, oparamptr) const;
    static inline Type deftype() { return OBJINST; }
    bool operator==(const objinst &) const;
protected:
    void doGetBbox(XPoint*, float scale, int extend, objinst* callinst) const;
    bool isEqual(const generic &) const;
};
typedef objinst *objinstptr;
typedef Plist::type_iterator<objinst> objinstiter;
NO_FREE(objinstptr);

/*----------------------------------------------------------------------*/
/* Label								*/
/*----------------------------------------------------------------------*/

class label : public positionable {
public:
    pointselect* cycle;		/* Edit position(s), or NULL */
    short	justify;
    u_char	pin;
    stringpart	*string;
    label();
    label(const label&);
    label(u_char dopin, XPoint pos);
    ~label();
    label & operator=(const label &);
    generic* copy() const;
    void indicate(DrawContext*, eparamptr, oparamptr) const;
    static inline Type deftype() { return LABEL; }
    bool operator==(const label &) const;
protected:
    void doGetBbox(XPoint*, float scale = 0.0, int extend = 0, objinstptr callinst = NULL) const;
    bool isEqual(const generic &) const;
};
typedef label *labelptr;
typedef Plist::type_iterator<label> labeliter;
NO_FREE(labelptr);

/*----------------------------------------------------------------------*/
/* Polygon								*/
/*----------------------------------------------------------------------*/

class polygon : public generic {
public:
    pointselect* cycle;		/* Edit position(s), or NULL */
    u_short	style;
    float	width;
    pointlist	points;
    polygon();
    polygon(const polygon &);
    polygon(int number, int x, int y);
    polygon & operator=(const polygon &);
    generic* copy() const;
    void draw(DrawContext*) const;
    void indicate(DrawContext*, eparamptr, oparamptr) const;
    void reverse();
    static inline Type deftype() { return POLYGON; }
    bool operator==(const polygon &) const;
protected:
    bool isEqual(const generic &) const;
};
typedef polygon *polyptr;
typedef Plist::type_iterator<polygon> polyiter;
NO_FREE(polyptr);

/*----------------------------------------------------------------------*/
/* Bezier Curve								*/
/*----------------------------------------------------------------------*/

#define SPLINESEGS 20 /* Number of points per spline approximation 	*/
#define INTSEGS		(SPLINESEGS - 2)
#define LASTSEG		(SPLINESEGS - 3)

class arc;
class spline : public generic {
public:
    pointselect* cycle;		/* Edit position(s), or NULL */
    u_short	style;
    float	width;
    XPoint	ctrl[4];
    /* the following are for rendering only */
    XfPoint	points[INTSEGS];
    spline();
    explicit spline(const arc &);
    spline(const spline &);
    spline(int x, int y);
    spline & operator=(const spline &);
    generic* copy() const;
    void draw(DrawContext*) const;
    void calc();
    void indicate(DrawContext*, eparamptr, oparamptr) const;
    void reverse();
    static inline Type deftype() { return SPLINE; }
    bool operator==(const spline &) const;
protected:
    bool isEqual(const generic &) const;
};
typedef spline *splineptr;
typedef Plist::type_iterator<spline> splineiter;
NO_FREE(splineptr);

/*----------------------------------------------------------------------*/
/* Arc									*/
/*----------------------------------------------------------------------*/

#define RSTEPS     72 /* Number of points defining a circle approx.	*/

class arc : public generic {
public:
    pointselect	*cycle;		/* Edit position(s), or NULL */
    u_short	style;
    float	width;
    short	radius; 	/* x-axis radius */
    short	yaxis;		/* y-axis radius */
    float	angle1;		/* endpoint angles, in degrees */
    float	angle2;
    XPoint	position;
    /* the following are for rendering only */
    short	number;
    XfPoint	points[RSTEPS + 1];
    arc();
    arc(const arc &);
    arc(int x, int y);
    arc & operator=(const arc &);
    generic* copy() const;
    void draw(DrawContext*) const;
    void calc();
    void indicate(DrawContext*, eparamptr, oparamptr) const;
    void reverse();
    static inline Type deftype() { return ARC; }
    bool operator==(const arc &) const;
protected:
    bool isEqual(const generic &) const;
};
typedef arc *arcptr;
typedef Plist::type_iterator<arc> arciter;
NO_FREE(arcptr);

/*----------------------------------------------------------------------*/
/* Path									*/
/*----------------------------------------------------------------------*/

class path : public generic, public Plist {
public:
    u_short	style;
    float	width;
    path();
    //path(const path&);
    //path & operator=(const path &);
    generic* copy() const;
    void draw(DrawContext*) const;
    void calc();
    void indicate(DrawContext*, eparamptr, oparamptr) const;
    static inline Type deftype() { return PATH; }
    bool operator==(const path &) const;
protected:
    bool isEqual(const generic &) const;
};
typedef path *pathptr;
typedef Plist::type_iterator<path> pathiter;
NO_FREE(pathptr);

/*----------------------------------------------------------------------*/
/* Type System  							*/
/*----------------------------------------------------------------------*/

/* Type checks */

static inline Type ELEMENTTYPE(genericptr a) { return a->type; }

static inline bool IS_POLYGON(genericptr a) { return ELEMENTTYPE(a) == POLYGON; }
static inline bool IS_LABEL(genericptr a) { return ELEMENTTYPE(a) == LABEL; }
static inline bool IS_OBJINST(genericptr a) { return ELEMENTTYPE(a) == OBJINST; }
static inline bool IS_ARC(genericptr a) { return ELEMENTTYPE(a) == ARC; }
static inline bool IS_SPLINE(genericptr a) { return ELEMENTTYPE(a) == SPLINE; }
static inline bool IS_PATH(genericptr a) { return ELEMENTTYPE(a) == PATH; }
static inline bool IS_GRAPHIC(genericptr a) { return ELEMENTTYPE(a) == GRAPHIC; }

/* Conversions from generic to specific types */

static inline polyptr TOPOLY(const genericptr* a) { return IS_POLYGON(*a) ? (polyptr)*a : NULL; }
static inline labelptr TOLABEL(const genericptr* a) { return IS_LABEL(*a) ? (labelptr)*a : NULL; }
static inline objinstptr TOOBJINST(const genericptr* a) { return IS_OBJINST(*a) ? (objinstptr)*a : NULL; }
static inline arcptr TOARC(const genericptr* a) { return IS_ARC(*a) ? (arcptr)*a : NULL; }
static inline splineptr TOSPLINE(const genericptr* a) { return IS_SPLINE(*a) ? (splineptr)*a : NULL; }
static inline pathptr TOPATH(const genericptr* a) { return IS_PATH(*a) ? (pathptr)*a : NULL; }
static inline graphicptr TOGRAPHIC(const genericptr* a) { return IS_GRAPHIC(*a) ? (graphicptr)*a : NULL; }
static inline genericptr TOGENERIC(const genericptr* a) { return *a; }

#endif // ELEMENTS_H
