#include "elements.h"
#include "xcircuit.h"
#include "prototypes.h"
#include "matrix.h"
#include "context.h"

polygon::polygon() :
        generic(POLYGON),
        cycle(NULL)
{
}

polygon::polygon(const polygon & src) :
        generic(POLYGON),
        cycle(NULL)
{
    *this = src;
}

polygon::polygon(int number, int x, int y) :
        generic(POLYGON),
        cycle(NULL)
{
    style = areawin->style & ~UNCLOSED;
    color = areawin->color;
    width = areawin->linewidth;
    points.fill(XPoint(x, y), number);
}

polygon & polygon::operator=(const polygon & src)
{
    if (&src == this) return *this;
    generic::operator =(src);
    style = src.style;
    width = src.width;
    free(cycle);
    copycycles(&cycle, &src.cycle);
    points = src.points;
    return *this;
}

generic* polygon::copy() const
{
    return new polygon(*this);
}

void polygon::draw(Context* ctx) const
{
   pointlist tmppoints(points.count());

   ctx->CTM().transform(points.begin(), tmppoints.begin(), tmppoints.count());
   strokepath(ctx, tmppoints.begin(), tmppoints.count(), style, width);
}

void polygon::indicate(Context* ctx, eparamptr epp, oparamptr ops) const
{
    int k = epp->pdata.pointno;
    if (k < 0) k = 0;
    UDrawCircle(ctx, points + k, ops->which);
}

void polygon::reverse()
{
    std::reverse(points.begin(), points.end());
}

bool polygon::operator ==(const polygon & o) const
{
    return
            style == o.style &&
            width == o.width &&
            points == o.points;
}

bool polygon::isEqual(const generic & other) const
{
    return *this == static_cast<const polygon&>(other);
}

/*--------------------------------------------------------------*/
/* Polygon constructor:  Create a new polygon element in the	*/
/* object whose instance is "destinst" and return a pointer to  */
/* it.								*/
/*								*/
/*	"destinst" is the destination instance.  If NULL, the	*/
/*	   top-level instance (areawin->topinstance) is used.	*/
/*	"points" is a list of XPoint pointers, should not be	*/
/*	   NULL.  It is transferred to the polygon verbatim,	*/
/*	   and should NOT be free'd by the calling routine.	*/
/*	"number" is the number of points in the list, or zero	*/
/*	   if "points" is NULL.					*/
/*								*/
/* Other properties must be set individually by the calling	*/
/* 	routine.						*/
/*								*/
/* Return value is a pointer to the newly created polygon.	*/
/*--------------------------------------------------------------*/

polyptr new_polygon(objinstptr destinst, pointlist *points)
{
   polyptr *newpoly;
   objectptr destobject;
   objinstptr locdestinst;

   locdestinst = (destinst == NULL) ? areawin->topinstance : destinst;
   destobject = locdestinst->thisobject;

   newpoly = destobject->append(new polygon(0, 0, 0));
   (*newpoly)->points = *points;

   calcbboxvalues(locdestinst, (genericptr *)newpoly);
   updatepagebounds(destobject);
   incr_changes(destobject);
   return *newpoly;
}
