#include "elements.h"
#include "xcircuit.h"
#include "prototypes.h"
#include "matrix.h"
#include "context.h"

path::path() :
        generic(PATH), Plist()
{    
    style = NORMAL;
    width = areawin->linewidth;
    style = areawin->style;
    color = areawin->color;
}

#if 0
path::path(const path & src) :
        generic(PATH), Plist()
{
    *this = src;
}

path & path::operator=(const path & src)
{
    generic::operator =(src);
    plist::operator =(src);
    style = src.style;
    width = src.width;
    plist = src.plist;
    return *this;
}
#endif

generic* path::copy() const
{
    return new path(*this);
}

void path::draw(DrawContext* ctx) const
{
    pointlist   tmppoints;
    const genericptr	*genpath;
    polyptr	thepoly;
    splineptr	thespline;
    int		pathsegs = 0, curseg = 0;

    for (genpath = 0; values(genpath); ) {
       if (!*genpath) qDebug("%s %p trying to paint element %d=NULL", __FUNCTION__, this, genpath-begin());
       switch(ELEMENTTYPE(*genpath)) {
          case POLYGON:
             thepoly = TOPOLY(genpath);
             pathsegs += thepoly->points.count();
             tmppoints.resize(pathsegs);
             ctx->CTM().transform(thepoly->points.begin(), tmppoints.begin() + curseg, thepoly->points.count());
             curseg = pathsegs;
             break;
          case SPLINE:
             thespline = TOSPLINE(genpath);
             pathsegs += SPLINESEGS;
             tmppoints.resize(pathsegs);
             makesplinepath(ctx, thespline, tmppoints.begin() + curseg);
             curseg = pathsegs;

             if (thespline->cycle != NULL) {
                // currently edited spline
                UDrawXLine(ctx, thespline->ctrl[0], thespline->ctrl[1]);
                UDrawXLine(ctx, thespline->ctrl[3], thespline->ctrl[2]);
             }
             break;
       }
    }
    strokepath(ctx, tmppoints.begin(), pathsegs, style, width);
}

void path::calc()
{
    for (genericptr* elem = 0; values(elem); ) (*elem)->calc();
}

void path::indicate(DrawContext* ctx, eparamptr epp, oparamptr ops) const
{
    int k = epp->pdata.pathpt[1];
    if (k < 0) k = 0;
    genericptr pgen;
    if (epp->pdata.pathpt[0] < 0)
        pgen = at(0);
    else
        pgen = at(epp->pdata.pathpt[0]);
    if (ELEMENTTYPE(pgen) == POLYGON)
        UDrawCircle(ctx, TOPOLY(&pgen)->points + k, ops->which);
    else	/* spline */
        UDrawCircle(ctx, TOSPLINE(&pgen)->ctrl + k, ops->which);
}

bool path::operator ==(const path & o) const
{
    return
            style == o.style &&
            width == o.width &&
            static_cast<const Plist&>(*this) == static_cast<const Plist&>(o);
}

bool path::isEqual(const generic & other) const
{
    return *this == static_cast<const path&>(other);
}
