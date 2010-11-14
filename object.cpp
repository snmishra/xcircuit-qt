#include "xcircuit.h"
#include "prototypes.h"

void object::set_defaults()
{
    Plist::clear();

    hidden = false;
    changes = 0;
    while (params != NULL) {
        oparamptr ops = params;
        params = ops->next;
        delete ops;
    }
    viewscale = 0.5;

    /* Object should not reference the window:  this needs to be rethunk! */
    if (areawin != NULL) {
      pcorner.x = -areawin->width();
      pcorner.y = -areawin->height();
    }
    bbox.width = 0;
    bbox.height = 0;
    bbox.lowerleft.x = 0;
    bbox.lowerleft.y = 0;

    highlight.netlist = NULL;
    highlight.thisinst = NULL;
    schemtype = PRIMARY;
    symschem = NULL;
    netnames = NULL;
    polygons = NULL;
    labels = NULL;
    ports = NULL;
    calls = NULL;
    valid = false;
    traversed = false;
}

object::object() :
        Plist(),
        params(NULL)
{
    set_defaults();
}

object::~object()
{
    clear();
}

void object::clear() // replaces reset(this, NORMAL); use delete object to replace reset(this, DELETE)
{
    if (polygons != NULL || labels != NULL)
       destroynets(this);

    valid = false;
    if (parts > 0) {
       for (genericptr * gen = begin(); gen != end(); ++ gen) {
          /* (*gen == NULL) only on library pages		*/
          /* where the instances are kept in the library	*/
          /* definition, and are only referenced on the page.	*/
          delete *gen;
       }
       set_defaults();
    }
}

void object::clear_nodelete() // replaces reset(this, SAVE)
{
    if (polygons != NULL || labels != NULL)
       destroynets(this);

    valid = false;
    if (parts > 0) set_defaults();
}

bool object::operator ==(const object & other) const
{
    const object *obja = this, *objb = &other;

    const genericptr *compgen;
    genericptr *glist, *gchk, *remg;
    short      csize;
    bool    bres;

    /* quick check on equivalence of number of objects */

    if (obja->parts != objb->parts) return false;

    /* check equivalence of parameters.  Parameters need not be in any	*/
    /* order; they must only match by key and value.			*/

    if (obja->params == NULL && objb->params != NULL) return false;
    else if (obja->params != NULL && objb->params == NULL) return false;
    else if (obja->params != NULL || objb->params != NULL) {
      oparamptr opsa, opsb;
      for (opsa = obja->params; opsa != NULL; opsa = opsa->next) {
          opsb = match_param(objb, opsa->key);
          if (opsb == NULL) return false;
          else if (opsa->type != opsb->type) return false;
          switch (opsa->type) {
             case XC_STRING:
                if (stringcomp(opsa->parameter.string, opsb->parameter.string))
                   return false;
                break;
             case XC_EXPR:
                if (strcmp(opsa->parameter.expr, opsb->parameter.expr))
                   return false;
                break;
             case XC_INT: case XC_FLOAT:
                if (opsa->parameter.ivalue != opsb->parameter.ivalue)
                   return false;
                break;
          }
       }
    }

    /* For the exhaustive check we must match component for component. */
    /* Best not to assume that elements are in same order for both.    */

    csize = obja->parts;

    glist = new genericptr[csize];
    std::copy(objb->begin(), objb->begin() + csize, glist);
    for (compgen = 0; obja->values(compgen); ) {
       bres = false;
       for (gchk = glist; gchk < glist + csize; gchk++) {
          if ((*compgen)->color == (*gchk)->color)
             bres = **compgen == **gchk;
          if (bres) {
            csize--;
            for (remg = gchk; remg < glist + csize; remg++)
                *remg = *(remg + 1);
            break;
          }
       }
    }
    delete [] glist;
    if (csize != 0) return false;

    /* Both objects cannot attempt to set an associated schematic/symbol to  */
    /* separate objects, although it is okay for one to make the association */
    /* and the other not to.						    */

    if (obja->symschem != NULL && objb->symschem != NULL)
       if (obja->symschem != objb->symschem)
          return false;

    return true;
}

/*-----------------------------------------------------------*/
/* Find if an object is in the hierarchy of the given object */
/* Returns the number (position in plist) or -1 if not found */
/*-----------------------------------------------------------*/

short object::find(objectptr other) const
{
   for (objinstiter inst; values(inst); ) {
       if (inst->thisobject == other || inst->thisobject->find(other) >= 0)
           return inst - begin();
   }
   return -1;
}
