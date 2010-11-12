#include "elements.h"
#include "xcircuit.h"
#include "prototypes.h"
#include "xcqt.h"
#include "colors.h"
#include "context.h"
#include "matrix.h"

objinst::objinst() :
        positionable(OBJINST),
        thisobject(NULL),
        params(NULL),
        schembbox(NULL)
{
}

objinst::objinst(object* thisobj, int x, int y) :
        positionable(OBJINST, XPoint(x,y)),
        thisobject(thisobj),
        params(NULL),
        bbox(thisobj->bbox),
        schembbox(NULL)
{
    color = areawin->color;
}

objinst::objinst(const objinst & src) :
        positionable(OBJINST),
        thisobject(NULL),
        params(NULL),
        schembbox(NULL)
{
    *this = src;
}

static void clearops(oparamptr ops)
{
    while (ops != NULL) {
        oparamptr fops;
        /* Don't try to free data from indirect parameters */
        /* (That's not true---all data are copied by epsubstitute) */
        /* if (find_indirect_param(geninst, ops->key) == NULL) { */
        switch(ops->type) {
        case XC_STRING:
            freelabel(ops->parameter.string);
            break;
        case XC_EXPR:
            free(ops->parameter.expr);
            break;
        }
        fops = ops;
        ops = ops->next;
        delete fops;
    }
}

objinst::~objinst()
{
    clearops(params);
    delete schembbox;
}


objinst & objinst::operator=(const objinst & src)
{
    if (&src == this) return *this;
    positionable::operator=(src);
    position = src.position;
    rotation = src.rotation;
    scale = src.scale;
    thisobject = src.thisobject;
    bbox = src.bbox;

    clearops(params);
    this->params = NULL;
    copyparams(this, &src);

    /* If the parameters are the same, the bounding box should be, too. */
    delete schembbox;
    if (src.schembbox) {
       schembbox = new BBox(*src.schembbox);
    }
    else
       schembbox = NULL;

    return *this;
}

generic* objinst::copy() const
{
    return new objinst(*this);
}

bool objinst::operator ==(const objinst &o) const
{
    return
            position == o.position &&
            rotation == o.rotation &&
            scale == o.scale &&
            thisobject == o.thisobject;
}

bool objinst::isEqual(const generic & other) const
{
    return *this == static_cast<const objinst&>(other);
}

void objinst::doGetBbox(XPoint * npoints, float scale, int extend, objinst *) const
{
   XPoint points[4];

   points[0].x = points[1].x = bbox.lowerleft.x - extend;
   points[1].y = points[2].y = bbox.lowerleft.y + bbox.height
                + extend;
   points[2].x = points[3].x = bbox.lowerleft.x + bbox.width
                + extend;
   points[0].y = points[3].y = bbox.lowerleft.y - extend;

   if (scale == 0.0) scale = this->scale;
   UTransformPoints(points, npoints, 4, position, scale, rotation);
}

/*----------------------------------------------------------------------*/
/* Main recursive object instance drawing routine.			*/
/*    context is the instance information passed down from above	*/
/*    theinstance is the object instance to be drawn			*/
/*    level is the level of recursion 					*/
/*    passcolor is the inherited color value passed to object		*/
/*----------------------------------------------------------------------*/

void UDrawObject(Context* ctx, objinstptr theinstance, short level, int passcolor, pushlistptr *stack)
{
   genericptr	*areagen;
   float	tmpwidth;
   int		defaultcolor = passcolor;
   int		curcolor = passcolor;
   short	savesel;
   XPoint 	bboxin[2], bboxout[2];
   u_char	xm, ym;
   objectptr	theobject = theinstance->thisobject;

   /* Save the number of selections and set it to zero while we do the	*/
   /* object drawing.							*/

   savesel = areawin->selects;
   areawin->selects = 0;

   /* All parts are given in the coordinate system of the object, unless */
   /* this is the top-level object, in which they will be interpreted as */
   /* relative to the screen.						 */

   ctx->UPushCTM();

   if (stack) push_stack(stack, theinstance);
   if (level != 0)
       ctx->CTM().preMult(theinstance->position, theinstance->scale,
                        theinstance->rotation);

   /* do a quick test for intersection with the display window */

   bboxin[0].x = theobject->bbox.lowerleft.x;
   bboxin[0].y = theobject->bbox.lowerleft.y;
   bboxin[1].x = theobject->bbox.lowerleft.x + theobject->bbox.width;
   bboxin[1].y = theobject->bbox.lowerleft.y + theobject->bbox.height;
   if (level == 0)
      extendschembbox(theinstance, &(bboxin[0]), &(bboxin[1]));
   ctx->CTM().transform(bboxin, bboxout, 2);

   xm = (bboxout[0].x < bboxout[1].x) ? 0 : 1;
   ym = (bboxout[0].y < bboxout[1].y) ? 0 : 1;

   if (bboxout[xm].x < areawin->width && bboxout[ym].y < areawin->height &&
       bboxout[1 - xm].x > 0 && bboxout[1 - ym].y > 0) {

     /* make parameter substitutions */
     psubstitute(theinstance);

     /* draw all of the elements */

     tmpwidth = ctx->UTopTransScale(xobjs.pagelist[areawin->page].wirewidth);
     SetLineAttributes(ctx->gc(), tmpwidth, LineSolid, CapRound,
                JoinBevel);

     /* guard against plist being regenerated during a redraw by the	*/
     /* expression parameter mechanism (should that be prohibited?)	*/

     for (areagen = 0; theobject->values(areagen); ) {

       if (defaultcolor != DOFORALL) {
          if ((*areagen)->color != curcolor) {
             if ((*areagen)->color == DEFAULTCOLOR)
                curcolor = defaultcolor;
             else
                curcolor = (*areagen)->color;
             XcTopSetForeground(ctx, curcolor);
          }
       }

       switch(ELEMENTTYPE(*areagen)) {
          case(POLYGON):
             if (level == 0 || !((TOPOLY(areagen))->style & BBOX))
                TOPOLY(areagen)->draw(ctx);
             break;

          case(OBJINST):
             if (areawin->editinplace && stack && (TOOBJINST(areagen)
                        == areawin->topinstance)) {
                /* If stack matches areawin->stack, then don't draw */
                /* because it would be redundant.		 */
                pushlistptr alist = *stack, blist = areawin->stack;
                while (alist && blist) {
                   if (alist->thisinst != blist->thisinst) break;
                   alist = alist->next;
                   blist = blist->next;
                }
                if ((!alist) || (!blist)) break;
             }
             UDrawObject(ctx, TOOBJINST(areagen), level + 1, curcolor, stack);
             break;

          case(LABEL):
             if (level == 0 || TOLABEL(areagen)->pin == false)
                UDrawString(ctx, TOLABEL(areagen), curcolor, theinstance);
             else if ((TOLABEL(areagen)->justify & PINVISIBLE) && areawin->pinpointon)
                UDrawString(ctx, TOLABEL(areagen), curcolor, theinstance);
             else if (TOLABEL(areagen)->justify & PINVISIBLE)
                UDrawString(ctx, TOLABEL(areagen), curcolor, theinstance, false);
             else if (level == 1 && TOLABEL(areagen)->pin &&
                        TOLABEL(areagen)->pin != INFO && areawin->pinpointon)
                UDrawXDown(ctx, TOLABEL(areagen));
             break;

          default:
             TOGENERIC(areagen)->draw(ctx);
             break;
       }
     }

     /* restore the color passed to the object, if different from current color */

     if ((defaultcolor != DOFORALL) && (passcolor != curcolor)) {
        XTopSetForeground(ctx->gc(), passcolor);
     }
   }

   /* restore the selection list (if any) */
   areawin->selects = savesel;
   ctx->UPopCTM();
   if (stack) pop_stack(stack);
}

void objinst::indicate(Context* ctx, eparamptr, oparamptr ops) const
{
    UDrawCircle(ctx, &position, ops->which);
}

/*--------------------------------------------------------------*/
/* Instance constructor:  Create a new object instance element	*/
/* in the object whose instance is "destinst" and return a	*/
/* pointer to it.						*/
/*								*/
/*	"destinst" is the destination instance.  If NULL, the	*/
/*	   top-level instance (areawin->topinstance) is used.	*/
/*	"srcinst" is the source instance of which this is a	*/
/*	   copy.						*/
/*	"x" and "y" represents the instance position.		*/
/*								*/
/* Other properties must be set individually by the calling	*/
/* 	routine.						*/
/*								*/
/* Return value is a pointer to the newly created arc.		*/
/*--------------------------------------------------------------*/

objinstptr new_objinst(objinstptr destinst, objinstptr srcinst, int x, int y)
{
   objinstptr *newobjinst;
   objectptr destobject;
   objinstptr locdestinst;

   locdestinst = (destinst == NULL) ? areawin->topinstance : destinst;
   destobject = locdestinst->thisobject;

   newobjinst = destobject->append(new objinst(*srcinst));
   (*newobjinst)->position.x = x;
   (*newobjinst)->position.y = y;

   calcbboxvalues(locdestinst, (genericptr *)newobjinst);
   updatepagebounds(destobject);
   incr_changes(destobject);
   return *newobjinst;
}
