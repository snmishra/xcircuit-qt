/*----------------------------------------------------------------------*/
/* graphic.c --- xcircuit routines handling rendered graphic elements	*/
/* Copyright (c) 2005  Tim Edwards, MultiGiG, Inc.			*/
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*	written by Tim Edwards, 7/11/05					*/
/*----------------------------------------------------------------------*/

#include <QImage>
#include <QRgb>
#include <QPainter>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

#ifdef TCL_WRAPPER
#include <tk.h>
#endif

#include "context.h"
#include "matrix.h"
#include "xcircuit.h"
#include "colors.h"
#include "prototypes.h"

/*----------------------------------------------------------------------*/
/* Recursive search for graphic images in an object.			*/
/* Updates list "glist" when any image is found in an object or its	*/
/* descendents.								*/
/*----------------------------------------------------------------------*/

void count_graphics(objectptr thisobj, short *glist)
{
   genericptr *ge;
   graphicptr gp;
   Imagedata *iptr;
   int i;

   for (ge = thisobj->begin(); ge < thisobj->end(); ge++) {
      if (IS_GRAPHIC(*ge)) {
	 gp = TOGRAPHIC(ge);
         for (i = 0; i < xobjs.images; i++) {
	    iptr = xobjs.imagelist + i;
            if (iptr->image == gp->source) {
	       glist[i]++;
	    }
	 }
      }
      else if (IS_OBJINST(*ge)) {
	 count_graphics(TOOBJINST(ge)->thisobject, glist);
      }
   }
}

/*----------------------------------------------------------------------*/
/* Given a list of pages, return a list of indices into the graphics	*/
/* buffer area of each graphic used on any of the indicated pages.	*/
/* The returned list is allocated and it is the responsibility of the	*/
/* calling routine to free it.						*/
/*----------------------------------------------------------------------*/

short *collect_graphics(short *pagelist)
{
   short *glist;
   int i;

   glist = (short *)malloc(xobjs.images * sizeof(short));

   for (i = 0; i < xobjs.images; i++) glist[i] = 0;

   for (i = 0; i < xobjs.pages; i++)
      if (pagelist[i] > 0)
         count_graphics(xobjs.pagelist[i].pageinst->thisobject, glist);
	
   return glist;
}

/*----------------------------------------------------------------------*/
/* Generate the target view of the indicated graphic image, combining	*/
/* the image's scale and rotation and the zoom factor of the current	*/
/* view.								*/
/*									*/
/* If the graphic is at the wrong scale or rotation but is not redrawn	*/
/* because it is outside the screen viewing area, return false. 	*/
/* Otherwise, return true.						*/
/*----------------------------------------------------------------------*/

bool graphic::transform(Context* ctx) const
{
    const graphic * const gp = this;
    int width, height, twidth, theight, rotation;
    float scale, tscale;
    double cosr, sinr;
    int x, y, c, s, hw, hh, thw, thh, xorig, yorig, crot, xc, yc;

    tscale = ctx->UTopScale();
    scale = gp->scale * tscale;
    rotation = gp->rotation + ctx->UTopRotation();

    if (rotation >= 360) rotation -= 360;
    else if (rotation < 0) rotation += 360;

    /* Check if the top-level rotation and scale match the	*/
    /* saved target image.  If so, then we're done.		*/
    if ((rotation == gp->trot) && (scale == gp->tscale)) return true;

    cosr = cos(RADFAC * rotation);
    sinr = sin(RADFAC * rotation);
    c = (int)(8192 * cosr / scale);
    s = (int)(8192 * sinr / scale);

    /* Determine the necessary width and height of the pixmap	*/
    /* that fits the rotated and scaled image.			*/

    crot = rotation;
    if (crot > 90 && crot < 180) crot = 180 - crot;
    if (crot > 270 && crot < 360) crot = 360 - crot;
    cosr = cos(RADFAC * crot);
    sinr = sin(RADFAC * crot);
    width = gp->source->width() * scale;
    height = gp->source->height() * scale;

    twidth = (int)(fabs(width * cosr + height * sinr));
    theight = (int)(fabs(width * sinr + height * cosr));
    if (twidth & 1) twidth++;
    if (theight & 1) theight++;

    /* Check whether this instance is going to be off-screen,	*/
    /* to avoid excessive computation.				*/

    ctx->UTopOffset(&xc, &yc);
    xc += (int)((float)gp->position.x * tscale);
    yc = areawin->height() - yc;
    yc += (int)((float)gp->position.y * tscale);

    if (xc - (twidth >> 1) > areawin->width()) return false;
    if (xc + (twidth >> 1) < 0) return false;
    if (yc - (theight >> 1) > areawin->height()) return false;
    if (yc + (theight >> 1) < 0) return false;

    /* Generate the new target image */
    delete gp->target;
    /// \todo generate new image

    hh = gp->source->height() >> 1;
    hw = gp->source->width() >> 1;
    thh = theight >> 1;
    thw = twidth >> 1;
    for (y = -thh; y < thh; y++) {
	for (x = -thw; x < thw; x++) {
	    xorig = ((x * c + y * s) >> 13) + hw;
	    yorig = ((-x * s + y * c) >> 13) + hh;

	    if ((xorig >= 0) && (yorig >= 0) &&
                        (xorig < gp->source->width()) && (yorig < gp->source->height()))
               gp->target->setPixel(x + thw, y + thh, gp->source->pixel(xorig, yorig));
            else
               gp->target->setPixel(x + thw, y + thh, qRgba(0,0,0,0)); // transparent pixel instead of a clipmask
	}
    }
    gp->tscale = scale;
    gp->trot = rotation;
    return true;
}

void graphic::doGetBbox(XPoint * npoints, float scale, int, objinst *) const
{
   XPoint points[4];
   int hw = source->width() >> 1;
   int hh = source->height() >> 1;

   points[1].x = points[2].x = hw;
   points[0].x = points[3].x = -hw;

   points[0].y = points[1].y = -hh;
   points[2].y = points[3].y = hh;

   if (scale == 0.0) scale = this->scale;
   UTransformPoints(points, npoints, 4, position,
                scale, rotation);
}

/*----------------------------------------------------------------------*/
/* Draw a graphic image by copying from the image to the window.	*/
/* Image is centered on the center point of the graphic image.		*/
/*----------------------------------------------------------------------*/

void graphic::draw(Context* ctx) const
{
    XPoint ppt;

    /* transform to current scale and rotation, if necessary */
    if (! transform(ctx)) return;  /* Graphic off-screen */

    /* transform to current position */
    ctx->CTM().transform(&position, &ppt, 1);

    /* user_to_window(position, &ppt); */

    ppt.x -= (target->width() >> 1);
    ppt.y -= (target->height() >> 1);

    ctx->gc()->drawImage(ppt.x, ppt.y, *target);
}

void graphic::indicate(Context* ctx, eparamptr, oparamptr ops) const
{
    UDrawCircle(ctx, &position, ops->which);
}

/*----------------------------------------------------------------------*/
/* Allocate space for a new graphic source image of size width x height	*/
/*----------------------------------------------------------------------*/

Imagedata *addnewimage(char *name, int width, int height)
{
   Imagedata *iptr;

   /* Create the image and store in the global list of images */

   xobjs.images++;
   if (xobjs.imagelist)
      xobjs.imagelist = (Imagedata *)realloc(xobjs.imagelist,
		xobjs.images * sizeof(Imagedata));
   else
      xobjs.imagelist = new Imagedata;
    
   /* Save the image source in a file */
   iptr = xobjs.imagelist + xobjs.images - 1;
   if (name)
      iptr->filename = strdup(name);
   else
      iptr->filename = NULL;	/* must be filled in later! */
   iptr->refcount = 0;		/* no calls yet */
   iptr->image = new QImage(width,height, QImage::Format_ARGB32_Premultiplied);

   return iptr;
}

/*----------------------------------------------------------------------*/
/* Create a new graphic image from a PPM file, and position it at the	*/
/* indicated (px, py) coordinate in user space.				*/
/*									*/
/* This should be expanded to incorporate more PPM formats.  Also, it	*/
/* needs to be combined with the render.c routines to transform		*/
/* PostScript graphics into an internal XImage for faster rendering.	*/
/*----------------------------------------------------------------------*/

graphicptr new_graphic(objinstptr destinst, char *filename, int px, int py)
{
    graphicptr *gp;
    objectptr destobject;
    objinstptr locdestinst;
    Imagedata *iptr;
    FILE *fg;
    int nr, width, height, imax, x, y, i;
    char id[5], c, buf[128];
    union {
       u_char b[4];
       u_long i;
    } pixel;

    locdestinst = (destinst == NULL) ? areawin->topinstance : destinst;
    destobject = locdestinst->thisobject;

    /* Check the existing list of images.  If there is a match,	*/
    /* re-use the source; don't load the file again.		*/

    for (i = 0; i < xobjs.images; i++) {
       iptr = xobjs.imagelist + i;
       if (!strcmp(iptr->filename, filename)) {
	  break;
       }
    }
    if (i == xobjs.images) {

       fg = fopen(filename, "r");
       if (fg == NULL) return NULL;

       /* This ONLY handles binary ppm files with max data = 255 */

       while (1) {
          nr = fscanf(fg, " %s", buf);
	  if (nr <= 0) return NULL;
	  if (buf[0] != '#') {
	     if (sscanf(buf, "%s", id) <= 0)
		return NULL;
	     break;
	  }
	  else fgets(buf, 127, fg);
       }
       if ((nr <= 0) || strncmp(id, "P6", 2)) return NULL;

       while (1) {
          nr = fscanf(fg, " %s", buf);
	  if (nr <= 0) return NULL;
	  if (buf[0] != '#') {
	     if (sscanf(buf, "%d", &width) <= 0)
		return NULL;
	     break;
	  }
	  else fgets(buf, 127, fg);
       }
       if (width <= 0) return NULL;

       while (1) {
          nr = fscanf(fg, " %s", buf);
	  if (nr <= 0) return NULL;
	  if (buf[0] != '#') {
	     if (sscanf(buf, "%d", &height) <= 0)
		return NULL;
	     break;
	  }
	  else fgets(buf, 127, fg);
       }
       if (height <= 0) return NULL;

       while (1) {
          nr = fscanf(fg, " %s", buf);
	  if (nr <= 0) return NULL;
	  if (buf[0] != '#') {
	     if (sscanf(buf, "%d", &imax) <= 0)
		return NULL;
	     break;
	  }
	  else fgets(buf, 127, fg);
       }
       if (imax != 255) return NULL;

       while (1) {
          fread(&c, 1, 1, fg);
          if (c == '\n') break;
          else if (c == '\0') return NULL;
       }

       iptr = addnewimage(filename, width, height);

       /* Read the image data from the PPM file */

       pixel.b[3] = 0;
       for (y = 0; y < height; y++)
          for (x = 0; x < width; x++) {
	     fread(&pixel.b[2], 1, 1, fg);
	     fread(&pixel.b[1], 1, 1, fg);
	     fread(&pixel.b[0], 1, 1, fg);
             iptr->image->setPixel(x, y, qRgb(pixel.b[2], pixel.b[1], pixel.b[0]));
          }
    }

    iptr->refcount++;
    gp = destobject->append(new graphic);

    (*gp)->scale = 1.0;
    (*gp)->position.x = px;
    (*gp)->position.y = py;
    (*gp)->rotation = 0;
    (*gp)->color = DEFAULTCOLOR;
    (*gp)->source = iptr->image;
    (*gp)->trot = 0;
    (*gp)->tscale = 0;

    calcbboxvalues(locdestinst, (genericptr *)gp);
    updatepagebounds(destobject);
    incr_changes(destobject);

    register_for_undo(XCF_Graphic, UNDO_DONE, areawin->topinstance, *gp);

    return *gp;
}

/*----------------------------------------------------------------------*/
/* Free memory associated with the XImage structure for a graphic.	*/
/*----------------------------------------------------------------------*/

static void freeimage(XImage *source)
{
   int i, j;
   Imagedata *iptr;

   if (! source) return;
   for (i = 0; i < xobjs.images; i++) {
      iptr = xobjs.imagelist + i;
      if (iptr->image == source) {
	 iptr->refcount--;
	 if (iptr->refcount <= 0) {
            delete iptr->image;
	    free(iptr->filename);

	    /* Remove this from the list of images */

	    for (j = i; j < xobjs.images - 1; j++)
	       *(xobjs.imagelist + j) = *(xobjs.imagelist + j + 1);
	    xobjs.images--;
	 }
	 break;
      }
   }
}

/*----------------------------------------------------------------------*/

graphic::graphic() :
        positionable(GRAPHIC),
        source(NULL),
        target(NULL),
        valid(false)
{
}

graphic::graphic(const graphic & src) :
        positionable(GRAPHIC),
        source(NULL),
        target(NULL),
        valid(false)
{
    *this = src;
}

graphic::~graphic()
{
   delete target;
   freeimage(source);
}

graphic& graphic::operator =(const graphic& src) {
    Imagedata *iptr;
    int i;

    positionable::operator =(src);
    freeimage(source);
    source = src.source;
    valid = false;
    delete target;
    target = NULL;

    /* Update the refcount of the source image */
    for (i = 0; i < xobjs.images; i++) {
       iptr = xobjs.imagelist + i;
       if (iptr->image == source) {
          iptr->refcount++;
          break;
       }
    }
    return *this;
}

generic* graphic::copy() const
{
    return new graphic(*this);
}
