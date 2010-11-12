#include "elements.h"
#include "xcircuit.h"
#include "prototypes.h"
#include "colors.h"
#include "matrix.h"

label::label() :
        positionable(LABEL),
        cycle(NULL),
        string(NULL)
{
}

label::label(u_char dopin, XPoint pos) :
        positionable(LABEL),
        cycle(NULL),
        string(new stringpart)
{
   rotation = 0;
   color = areawin->color;
   scale = areawin->textscale;

   /* initialize string with font designator */
   string->type = FONT_NAME;
   string->data.font = areawin->psfont;
   string->nextpart = NULL;

   pin = dopin;
   if (dopin == LOCAL) color = LOCALPINCOLOR;
   else if (dopin == GLOBAL) color = GLOBALPINCOLOR;
   else if (dopin == INFO) color = INFOLABELCOLOR;

   justify = areawin->justify;
   position = pos;
}

label::label(const label & src) :
        positionable(LABEL),
        cycle(NULL),
        string(new stringpart)
{
    *this = src;
}

label & label::operator=(const label & src)
{
    if (&src == this) return *this;
    positionable::operator=(src);
    freelabel(string);
    string = stringcopy(src.string);
    position = src.position;
    rotation = src.rotation;
    scale = src.scale;
    justify = src.justify;
    free(cycle);
    copycycles(&cycle, &src.cycle);
    pin = src.pin;
    return *this;
}

label::~label()
{
    freelabel(string);
}

generic* label::copy() const
{
    return new label(*this);
}

void label::doGetBbox(XPoint * npoints, float scale, int, objinstptr callinst) const
{
    XPoint points[4];
    TextExtents tmpext;
    short j;

    tmpext = ULength(NULL, this, callinst, 0.0, 0, NULL);
    points[0].x = points[1].x = (justify & NOTLEFT ?
                (justify & RIGHT ? -tmpext.width :
                 -tmpext.width / 2) : 0);
    points[2].x = points[3].x = points[0].x + tmpext.width;
    points[0].y = points[3].y = (justify & NOTBOTTOM ?
                (justify & TOP ? -tmpext.ascent :
                 -(tmpext.ascent + tmpext.base) / 2) : -tmpext.base)
                 + tmpext.descent;
    points[1].y = points[2].y = points[0].y + tmpext.ascent - tmpext.descent;

    /* separate bounding box for pinlabels and infolabels */
    if (pin)
       for (j = 0; j < 4; j++)
          pinadjust(justify, &points[j].x, &points[j].y, 1);

    if (scale == 0.0) scale = this->scale;
    UTransformPoints(points, npoints, 4, position,
                 scale, rotation);
}

void label::indicate(Context* ctx, eparamptr, oparamptr ops) const
{
    UDrawCircle(ctx, &position, ops->which);
}

/*--------------------------------------------------------------*/
/* Label constructor:  Create a new label element in the object	*/
/* whose instance is "destinst" and return a pointer to it.	*/
/*								*/
/*	"destinst" is the destination instance.  If NULL, the	*/
/*	   top-level instance (areawin->topinstance) is used.	*/
/*	"strptr" is a pointer to a stringpart string, and may	*/
/*	   be NULL.  If non-NULL, should NOT be free'd by the	*/
/*	   calling routine.					*/
/*	"pintype" is NORMAL, LOCAL, GLOBAL, or INFO		*/
/*	"x" and "y" are the label coordinates.			*/
/*								*/
/* Other properties must be set individually by the calling	*/
/* 	routine.						*/
/*								*/
/* Return value is a pointer to the newly created label.	*/
/*--------------------------------------------------------------*/

labelptr new_label(objinstptr destinst, stringpart *strptr, int pintype,
        int x, int y)
{
   labelptr *newlab;
   objectptr destobject;
   objinstptr locdestinst;

   locdestinst = (destinst == NULL) ? areawin->topinstance : destinst;
   destobject = locdestinst->thisobject;

   newlab = destobject->append(new label(pintype, XPoint(x,y)));

   if (strptr->type == FONT_NAME) {
      free ((*newlab)->string);
      (*newlab)->string = strptr;
   }
   else
      (*newlab)->string->nextpart = strptr;

   calcbboxvalues(locdestinst, (genericptr *)newlab);
   updatepagebounds(destobject);
   incr_changes(destobject);
   return *newlab;
}

/*--------------------------------------------------------------*/
/* Variant of the above; creates a label from a (char *) string	*/
/* instead of a stringpart pointer.  Like the stringpart 	*/
/* pointer above, "cstr" should NOT be free'd by the calling	*/
/* routine.							*/
/*--------------------------------------------------------------*/

labelptr new_simple_label(objinstptr destinst, char *cstr,
        int pintype, int x, int y)
{
   stringpart *strptr;

   strptr = new stringpart;
   strptr->type = TEXT_STRING;
   strptr->nextpart = NULL;
   strptr->data.string = cstr;

   return new_label(destinst, strptr, pintype, x, y);
}

bool label::operator ==(const label &o) const
{
    return
            position == o.position &&
            rotation == o.rotation &&
            scale == o.scale &&
            justify == o.justify &&
            pin == o.pin &&
            !stringcomp(string, o.string);
}

bool label::isEqual(const generic &other) const
{
    return *this == static_cast<const label&>(other);
}

/*--------------------------------------------------------------*/
/* Another variant of the above; creates a "temporary" label	*/
/* from a (char *) string.  As above, "cstr" should NOT be	*/
/* free'd by the calling routine.  The "temporary" label has no	*/
/* font information, and cannot be displayed nor saved/loaded	*/
/* from the PostScript output file.  Used to name networks or	*/
/* to mark port positions.  Pin type is always LOCAL.  Does not	*/
/* require updating bounding box info since it cannot be	*/
/* displayed.  Consequently, only requires passing the object	*/
/* to get the new label, not its instance.			*/
/*--------------------------------------------------------------*/

labelptr new_temporary_label(objectptr destobject, char *cstr,
        int x, int y)
{
   labelptr *newlab;

   destobject->append(new label(LOCAL, XPoint(x,y)));

   (*newlab)->string->type = TEXT_STRING; /* overwrites FONT record */
   (*newlab)->string->data.string = cstr;
   return *newlab;
}
