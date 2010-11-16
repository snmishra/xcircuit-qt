#include "elements.h"
#include "xcircuit.h"
#include "prototypes.h"

// Remove all element parameters from an element
static void free_all_eparams(genericptr thiselem)
{
   while (thiselem->passed != NULL)
      free_element_param(thiselem, thiselem->passed);
}

generic::~generic()
{
    free_all_eparams(this);
    if (false) qDebug("~generic %p", this);
}

generic::generic(const generic & src) :
        type(src.type),
        passed(NULL)
{
    *this = src;
}

generic & generic::operator=(const generic & src)
{
    if (&src == this) return *this;
    Q_ASSERT(this->type == src.type);
    free_all_eparams(this);
    copyalleparams(this, &src);
    this->color = src.color;
    return *this;
}

/*----------------------------------------------*/
/* Draw a circle at all parameter positions	*/
/*----------------------------------------------*/

void generic::indicateparams(DrawContext* ctx)
{
    oparamptr ops;
    eparamptr epp;

    for (epp = passed; epp != NULL; epp = epp->next) {
        ops = match_param(topobject, epp->key);
        if (ops == NULL) continue;	/* error condition */
        switch(ops->which) {
        case P_POSITION: case P_POSITION_X: case P_POSITION_Y:
            this->indicate(ctx, epp, ops);
            break;
        }
    }
}

/*--------------------------------------------------------------*/
/* Generic element destructor function				*/
/* (Note---this function is not being used anywhere. . .)	*/
/*--------------------------------------------------------------*/

#if 0
/// \todo -- compare to delete_more() and remove it
void remove_element(objinstptr destinst, genericptr genelem)
{
   objectptr destobject;
   objinstptr locdestinst;

   locdestinst = (destinst == NULL) ? areawin->topinstance : destinst;
   destobject = locdestinst->thisobject;

   delete_tagged(locdestinst);
   calcbboxvalues(locdestinst, (genericptr *)NULL);
   updatepagebounds(destobject);
}
#endif

bool generic::operator==(const generic& other) const
{
    return type == other.type && isEqual(other);
}

bool generic::isEqual(const generic&) const
{
    return false;
}

void generic::getBbox(XPoint *pts, float scale) const
{
    doGetBbox(pts, scale, 0, NULL);
}

void generic::getBbox(XPoint* pts, int extend, float scale) const
{
    doGetBbox(pts, scale, extend, NULL);
}

void generic::getBbox(XPoint* pts, objinstptr callinst, float scale) const
{
    doGetBbox(pts, scale, 0, callinst);
}
