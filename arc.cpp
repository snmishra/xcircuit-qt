#include "elements.h"
#include "xcircuit.h"
#include "prototypes.h"
#include "matrix.h"
#include "context.h"

arc::arc() :
        generic(ARC),
        cycle(NULL)
{
}

arc::arc(int x, int y) :
        generic(ARC),
        cycle(NULL)
{
    style = areawin->style;
    color = areawin->color;
    position.x = x;
    position.y = y;
    width = areawin->linewidth;
    radius = 0;
    yaxis = 0;
    angle1 = 0;
    angle2 = 360;
    calc();
}

arc::arc(const arc & src) :
        generic(ARC),
        cycle(NULL)
{
    *this = src;
}

arc & arc::operator=(const arc & src)
{
    if (&src == this) return *this;
    generic::operator =(src);
    style = src.style;
    position = src.position;
    radius = src.radius;
    yaxis = src.yaxis;
    angle1 = src.angle1;
    angle2 = src.angle2;
    width = src.width;
    free(cycle);
    copycycles(&cycle, &src.cycle);
    calc();
    return *this;
}

generic* arc::copy() const
{
    return new arc(*this);
}

void arc::draw(Context* ctx) const
{
   XPoint  tmppoints[RSTEPS + 2];

   ctx->CTM().transform(points, tmppoints, number);
   strokepath(ctx, tmppoints, number, style, width);
}

void arc::calc()
{
    short idx;
    int sarc;
    float theta, delta;

    /* assume that angle2 > angle1 always: must be guaranteed by other routines */

    sarc = (int)(angle2 - angle1) * RSTEPS;
    number = (sarc / 360) + 1;
    if (sarc % 360 != 0) number++;

    delta = RADFAC * ((float)(angle2 - angle1) / (number - 1));
    theta = angle1 * RADFAC;

    for (idx = 0; idx < number - 1; idx++) {
       points[idx].x = (float)position.x +
            fabs((float)radius) * cos(theta);
       points[idx].y = (float)position.y +
            (float)yaxis * sin(theta);
       theta += delta;
    }

    /* place last point exactly to avoid roundoff error */

    theta = angle2 * RADFAC;
    points[number - 1].x = (float)position.x +
            fabs((float)radius) * cos(theta);
    points[number - 1].y = (float)position.y +
            (float)yaxis * sin(theta);

    if (radius < 0) reversefpoints(points, number);
}

void arc::indicate(Context* ctx, eparamptr, oparamptr ops) const
{
    UDrawCircle(ctx, &position, ops->which);
}

void arc::reverse()
{
    radius = -radius;
}

bool arc::operator ==(const arc & o) const
{
    return
            position == o.position &&
            style == o.style &&
            width == o.width &&
            abs(radius) == abs(o.radius) &&
            yaxis == o.yaxis &&
            angle1 == o.angle1 &&
            angle2 == o.angle2;
}

bool arc::isEqual(const generic &other) const
{
    return *this == static_cast<const arc&>(other);
}

/*--------------------------------------------------------------*/
/* Arc constructor:  Create a new arc element in the object	*/
/* whose instance is "destinst" and return a pointer to it.	*/
/*								*/
/*	"destinst" is the destination instance.  If NULL, the	*/
/*	   top-level instance (areawin->topinstance) is used.	*/
/*	"radius" is the radius of the (circular) arc.		*/
/*	"x" and "y" represents the arc center position.		*/
/*								*/
/* Other properties must be set individually by the calling	*/
/* 	routine.						*/
/*								*/
/* Return value is a pointer to the newly created arc.		*/
/*--------------------------------------------------------------*/

arcptr new_arc(objinstptr destinst, int radius, int x, int y)
{
   arcptr *newarc;
   objectptr destobject;
   objinstptr locdestinst;

   locdestinst = (destinst == NULL) ? areawin->topinstance : destinst;
   destobject = locdestinst->thisobject;

   newarc = destobject->append(new arc(x, y));
   (*newarc)->radius = (*newarc)->yaxis = radius;

   (*newarc)->calc();
   calcbboxvalues(locdestinst, (genericptr *)newarc);
   updatepagebounds(destobject);
   incr_changes(destobject);
   return *newarc;
}
