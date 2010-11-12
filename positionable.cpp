#include "elements.h"
#include "xcircuit.h"
#include "prototypes.h"

positionable::positionable(const positionable & src) :
        generic(src.type)
{
    *this = src;
}

positionable& positionable::operator =(const positionable& src)
{
    generic::operator =(src);
    position = src.position;
    rotation = src.rotation;
    scale = src.scale;
    return *this;
}

float positionable::rescaleBox(const XPoint &corner, XPoint newpoints[5]) const
{
    float   newscale;
    long    mindist, testdist, refdist;
    int     i;
    bool    mynew = false;

    if (!newpoints) {
        newpoints = new XPoint[5];
        mynew = true;
    }
    doGetBbox(newpoints, 0.0, 0, areawin->topinstance);
    newpoints[4] = newpoints[0];
    mindist = LONG_MAX;
    for (i = 0; i < 4; i++) {
        testdist = finddist(&newpoints[i], &newpoints[i+1], &corner);
        if (testdist < mindist)
            mindist = testdist;
    }
    refdist = wirelength(&corner, &position);
    mindist = sqrt(fabs((double)mindist));
    if (!test_insideness(corner.x, corner.y, newpoints))
        mindist = -mindist;
    if (refdist == mindist) refdist = 1 - mindist;  /* avoid inf result */
    newscale = fabs(scale * (float)refdist / (float)(refdist + mindist));
    if (newscale > 10 * scale) newscale = 10 * scale;
    if (areawin->snapto) {
        float snapstep = 2 * (float)xobjs.pagelist[areawin->page].gridspace
                    / (float)xobjs.pagelist[areawin->page].snapspace;
        newscale = (float)((int)(newscale * snapstep)) / snapstep;
        if (newscale < (1.0 / snapstep)) newscale = (1.0 / snapstep);
    }
    else if (newscale < 0.1 * scale) newscale = 0.1 * scale;
    doGetBbox(newpoints, newscale, 0, areawin->topinstance);
    if (mynew) delete [] newpoints;
    return newscale;
}
