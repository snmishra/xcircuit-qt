#include <algorithm>

#include "elements.h"
#include "xcircuit.h"
#include "prototypes.h"

spline::spline() :
        generic(SPLINE),
        cycle(NULL)
{
}

spline::spline(const spline & src) :
        generic(SPLINE),
        cycle(NULL)
{
    *this = src;
}

spline::spline(const arc & thearc) :
        generic(SPLINE),
        cycle(NULL)
{
    /*----------------------------------------------------------------------*/
    /* Decompose an arc segment into one to four bezier curves according	*/
    /* the approximation algorithm lifted from the paper by  L. Maisonobe	*/
    /* (spaceroots.org).  This decomposition is done when an arc in a path	*/
    /* is read from an (older) xcircuit file, or when an arc is a selected	*/
    /* item when a path is created.	*/
    /*----------------------------------------------------------------------*/

   float radius = thearc.radius;
   float fnc, ang1, ang2;
   short ncurves, i;
   double nu1, nu2, lambda1, lambda2, alpha, tansq;
   XfPoint E1, E2, Ep1, Ep2;
   bool reverse = false;

   if (radius < 0) {
      reverse = true;
      radius = -radius;
   }

   fnc = (thearc.angle2 - thearc.angle1) / 90.0;
   ncurves = (short)fnc;
   if (fnc - (float)((int)fnc) > 0.01) ncurves++;

   for (i = 0; i < ncurves; i++) {
      if (reverse) {			/* arc path is reverse direction */
         if (i == 0)
            ang1 = thearc.angle2;
         else
            ang1 -= 90;

         if (i == ncurves - 1)
            ang2 = thearc.angle1;
         else
            ang2 = ang1 - 90;
      }
      else {				/* arc path is forward direction */
         if (i == 0)
            ang1 = thearc.angle1;
         else
            ang1 += 90;

         if (i == ncurves - 1)
            ang2 = thearc.angle2;
         else
            ang2 = ang1 + 90;
      }

      lambda1 = (double)ang1 * RADFAC;
      lambda2 = (double)ang2 * RADFAC;

      nu1 = atan2(sin(lambda1) / (double)thearc.yaxis,
                cos(lambda1) / radius);
      nu2 = atan2(sin(lambda2) / (double)thearc.yaxis,
                cos(lambda2) / radius);
      E1.x = (float)thearc.position.x +
                radius * (float)cos(nu1);
      E1.y = (float)thearc.position.y +
                (float)thearc.yaxis * (float)sin(nu1);
      E2.x = (float)thearc.position.x +
                radius * (float)cos(nu2);
      E2.y = (float)thearc.position.y +
                (float)thearc.yaxis * (float)sin(nu2);
      Ep1.x = -radius * (float)sin(nu1);
      Ep1.y = (float)thearc.yaxis * (float)cos(nu1);
      Ep2.x = -radius * (float)sin(nu2);
      Ep2.y = (float)thearc.yaxis * (float)cos(nu2);

      tansq = tan((nu2 - nu1) / 2.0);
      tansq *= tansq;
      alpha = sin(nu2 - nu1) * 0.33333 * (sqrt(4 + (3 * tansq)) - 1);

      this->style = thearc.style;
      this->color = thearc.color;
      this->width = thearc.width;

      this->ctrl[0].x = E1.x;
      this->ctrl[0].y = E1.y;

      this->ctrl[1].x = E1.x + alpha * Ep1.x;
      this->ctrl[1].y = E1.y + alpha * Ep1.y;

      this->ctrl[2].x = E2.x - alpha * Ep2.x;
      this->ctrl[2].y = E2.y - alpha * Ep2.y;

      this->ctrl[3].x = E2.x;
      this->ctrl[3].y = E2.y;

      calc();
   }
}

spline::spline(int x, int y) :
        generic(SPLINE),
        cycle(NULL)
{
    std::fill(ctrl, ctrl+4, XPoint(x,y));
    ctrl[1].x += (int)(xobjs.pagelist[areawin->page].gridspace / 2);
    ctrl[2].x -= (int)(xobjs.pagelist[areawin->page].gridspace / 2);
    width = areawin->linewidth;
    style = areawin->style;
    color = areawin->color;
    calc();
}

spline & spline::operator=(const spline & src)
{
    if (&src == this) return *this;
    generic::operator=(src);
    style = src.style;
    width = src.width;
    free(cycle);
    copycycles(&cycle, &src.cycle);
    std::copy(src.ctrl, src.ctrl+4, ctrl);
    std::copy(src.points, src.points+INTSEGS, points);
    return *this;
}

generic* spline::copy() const
{
    return new spline(*this);
}

void spline::draw(Context* ctx) const
{
    XPoint tmppoints[SPLINESEGS];

    makesplinepath(ctx, this, tmppoints);
    strokepath(ctx, tmppoints, SPLINESEGS, style, width);
    if (cycle != NULL) {
        // this spline is being edited
        UDrawXLine(ctx, ctrl[0], ctrl[1]);
        UDrawXLine(ctx, ctrl[3], ctrl[2]);
    }
}

bool spline::operator ==(const spline & o) const
{
    return
            style == o.style &&
            width == o.width &&
            ctrl[0] == o.ctrl[0] &&
            ctrl[1] == o.ctrl[1] &&
            ctrl[2] == o.ctrl[2] &&
            ctrl[3] == o.ctrl[3];
}

bool spline::isEqual(const generic &other) const
{
    return *this == static_cast<const spline&>(other);
}

/*------------------------------------------------------------------------*/
/* Create a Bezier curve approximation from control points		  */
/* (using PostScript formula for Bezier cubic curve)			  */
/*------------------------------------------------------------------------*/

static float par[INTSEGS];
static float parsq[INTSEGS];
static float parcb[INTSEGS];

void initsplines()
{
   float t;
   short idx;

   for (idx = 0; idx < INTSEGS; idx++) {
      t = (float)(idx + 1) / (INTSEGS + 1);
      par[idx] = t;
      parsq[idx] = t * t;
      parcb[idx] = parsq[idx] * t;
   }
}

void spline::calc()
{
    float ax, bx, cx, ay, by, cy;

    computecoeffs(this, &ax, &bx, &cx, &ay, &by, &cy);
    for (int idx = 0; idx < INTSEGS; idx++) {
       points[idx].x = ax * parcb[idx] + bx * parsq[idx] +
          cx * par[idx] + (float)ctrl[0].x;
       points[idx].y = ay * parcb[idx] + by * parsq[idx] +
          cy * par[idx] + (float)ctrl[0].y;
    }
}

void spline::indicate(Context* ctx, eparamptr epp, oparamptr ops) const
{
    int k = epp->pdata.pointno;
    UDrawCircle(ctx, ctrl + k, ops->which);
}

void spline::reverse()
{
    std::reverse(ctrl, ctrl + 4);
    calc();
}

/*--------------------------------------------------------------*/
/* Spline constructor:  Create a new spline element in the	*/
/* object whose instance is "destinst" and return a pointer to  */
/* it.								*/
/*								*/
/*	"destinst" is the destination instance.  If NULL, the	*/
/*	   top-level instance (areawin->topinstance) is used.	*/
/*	"points" is a array of 4 XPoints; should not be	NULL.	*/
/*								*/
/* Other properties must be set individually by the calling	*/
/* 	routine.						*/
/*								*/
/* Return value is a pointer to the newly created spline.	*/
/*--------------------------------------------------------------*/

splineptr new_spline(objinstptr destinst, pointlist points)
{
   splineptr *newspline;
   objectptr destobject;
   objinstptr locdestinst;

   locdestinst = (destinst == NULL) ? areawin->topinstance : destinst;
   destobject = locdestinst->thisobject;

   newspline = destobject->append(new spline(0, 0));

   std::copy(points + 0, points+4, (*newspline)->ctrl);

   (*newspline)->calc();
   calcbboxvalues(locdestinst, (genericptr *)newspline);
   updatepagebounds(destobject);
   incr_changes(destobject);
   return *newspline;
}

// Sara
