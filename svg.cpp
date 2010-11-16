/*----------------------------------------------------------------------*/
/* svg.c 								*/
/* Copyright (c) 2009  Tim Edwards, Open Circuit Design			*/
/*----------------------------------------------------------------------*/

#include <QImage>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <climits>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#ifdef TCL_WRAPPER 
#include <tk.h>
#endif

#include "xcircuit.h"
#include "matrix.h"
#include "prototypes.h"
#include "colors.h"
#include "context.h"

static void SVGDrawString(Context*, labelptr, int, objinstptr);

/*----------------------------------------------------------------------*/
/* External Variable definitions					*/
/*----------------------------------------------------------------------*/

extern fontinfo *fonts;
extern short fontcount;

/*----------------------------------------------------------------------*/
/* The output file is a global variable used by all routines.		*/
/*----------------------------------------------------------------------*/

FILE *svgf;

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

static void svg_printcolor(int passcolor, const char *prefix)
{
   if (passcolor != DEFAULTCOLOR) {
      int i = colorlist.indexOf(passcolor);
      if (i >= 0) {
         fprintf(svgf, "%s\"#%02x%02x%02x\" ",
		prefix,
                qRed(colorlist[i]),
                qGreen(colorlist[i]),
                qBlue(colorlist[i]));
      }
   }
}

/*----------------------------------------------------------------------*/
/* Since we can't do stipples in SVG, and since the whole stipple thing	*/
/* was only put in because we can't (easily) do transparency effects	*/
/* in X11, we convert stipples to transparency when the stipple is a	*/
/* mask, and do a color blend with white when the stipple is opaque.	*/
/*									*/
/* Blend amount is 1 (almost original color) to 7 (almost white)	*/
/*----------------------------------------------------------------------*/

static void svg_blendcolor(int passcolor, const char *prefix, int amount)
{
   int bred, bgreen, bblue;

   if (passcolor != DEFAULTCOLOR) {
      int i = colorlist.indexOf(passcolor);
      if (i >= 0) {
         bred = qRed(colorlist[i]);
         bgreen = qGreen(colorlist[i]);
         bblue = qBlue(colorlist[i]);
      }
   }
   else {
	 bred = bgreen = bblue = 0;
   }
   bred = ((bred * amount) + (255 * (8 - amount))) >> 3;
   bgreen = ((bgreen * amount) + (255 * (8 - amount))) >> 3;
   bblue = ((bblue * amount) + (255 * (8 - amount))) >> 3;

   fprintf(svgf, "%s\"#%02x%02x%02x\" ", prefix, bred, bgreen, bblue);
}

/*----------------------------------------------------------------------*/
/* Fill and/or draw a border around an element				*/
/*----------------------------------------------------------------------*/

static void svg_stroke(Context* ctx, int passcolor, short style, float width)
{
   float        tmpwidth;
   short	minwidth, solidpart, shade;

   tmpwidth = ctx->UTopTransScale(xobjs.pagelist[areawin->page].wirewidth * width);
   minwidth = qMax(1.0F, tmpwidth);

   if (style & FILLED || (!(style & FILLED) && style & OPAQUE)) {
      if ((style & FILLSOLID) == FILLSOLID) {
         svg_printcolor(passcolor, "fill=");
      }
      else if (!(style & FILLED)) {
	 fprintf(svgf, "fill=\"white\" ");
      }
      else {
	 shade = 1 + ((style & FILLSOLID) >> 5);
  	 if (style & OPAQUE) {
            svg_blendcolor(passcolor, "fill=", shade);
	 }
  	 else {
            svg_printcolor(passcolor, "fill=");
            fprintf(svgf, "fill-opacity=\"%g\" ", (float)shade / 8);
	 }
      }
   }
   else
      fprintf(svgf, "fill=\"none\" ");

   if (!(style & NOBORDER)) {
      /* set up dots or dashes */
      if (style & DASHED) solidpart = 4 * minwidth;
      else if (style & DOTTED) solidpart = minwidth;
      if (style & (DASHED | DOTTED)) {
	 fprintf(svgf, "style=\"stroke-dasharray:%d,%d\" ", solidpart, 4 * minwidth);

	 fprintf(svgf, "stroke-width=\"%g\" ", tmpwidth);
	 fprintf(svgf, "stroke-linecap=\"butt\" ");
	 if (style & SQUARECAP)
	    fprintf(svgf, "stroke-linejoin=\"miter\" ");
	 else
	    fprintf(svgf, "stroke-linejoin=\"bevel\" ");
      }
      else {
	 fprintf(svgf, "stroke-width=\"%g\" ", tmpwidth);
	 if (style & SQUARECAP) {
	    fprintf(svgf, "stroke-linejoin=\"miter\" ");
	    fprintf(svgf, "stroke-linecap=\"projecting\" ");
	 }
	 else {
	    fprintf(svgf, "stroke-linejoin=\"bevel\" ");
	    fprintf(svgf, "stroke-linecap=\"round\" ");
	 }
      }
      svg_printcolor(passcolor, "stroke=");
   }
   else
      fprintf(svgf, "stroke=\"none\" ");
   fprintf(svgf, "/>\n");
}

/*----------------------------------------------------------------------*/
/* Finish a path and fill and/or stroke					*/
/*----------------------------------------------------------------------*/

static void svg_strokepath(Context* ctx, int passcolor, short style, float width)
{
   /* Finish the path, closing if necessary */
   if (!(style & UNCLOSED))
      fprintf(svgf, "z\" ");
   else
      fprintf(svgf, "\" ");

   svg_stroke(ctx, passcolor, style, width);
}

/*-------------------------------------------------------------------------*/

void SVGCreateImages(int page)
{
    Imagedata *img;
    int i, x, y;
    short *glist;
    FILE *ppf;
    char *fname, outname[128], *pptr;
    pid_t pid;

    /* Check which images are used on this page */
    glist = (short *)malloc(xobjs.images * sizeof(short));
    for (i = 0; i < xobjs.images; i++) glist[i] = 0;
    count_graphics(xobjs.pagelist[page].pageinst->thisobject, glist);

    for (i = 0; i < xobjs.images; i++) {
       if (glist[i] == 0) continue;
       img = xobjs.imagelist + i;

       /* Generate a PPM file, then convert it to PNG */
       fname = tmpnam(NULL);
       ppf = fopen(fname, "w");
       if (ppf != NULL) {
          fprintf(ppf, "P6 %d %d 255\n", img->image->width(), img->image->height());
          for (y = 0; y < img->image->height(); y++) {
             for (x = 0; x < img->image->width(); x++) {
                QRgb pixel = img->image->pixel(x, y);
                const u_char rgb[3] = { qRed(pixel), qGreen(pixel), qBlue(pixel) };
                fwrite(rgb, 3, 1, ppf); // red,green,blue
	     }
	  }
       }
       fclose(ppf);

       /* Run "convert" to make this into a png file */

       strcpy(outname, img->filename);
       if ((pptr = strrchr(outname, '.')) != NULL)
	  strcpy(pptr, ".png");
       else
	  strcat(outname, ".png");

       if ((pid = vfork()) == 0) {
	  execlp("convert", "convert", fname, outname, NULL);
	  exit(0);  	/* not reached */
       }
       waitpid(pid, NULL, 0);
       unlink(fname);
       Fprintf(stdout, "Generated standalone PNG image file %s\n", outname);
    }
    free(glist);
}

/*-------------------------------------------------------------------------*/

static void SVGDrawGraphic(Context* ctx, graphicptr gp)
{
    XPoint ppt, corner;
    Imagedata *img;
    int i;
    char outname[128], *pptr;
    float tscale;
    int rotation;

    for (i = 0; i < xobjs.images; i++) {
       img = xobjs.imagelist + i;
       if (img->image == gp->source)
	  break;
    }
    if (i == xobjs.images) return;

    strcpy(outname, img->filename);
    if ((pptr = strrchr(outname, '.')) != NULL)
       strcpy(pptr, ".png");
    else
       strcat(outname, ".png");

    Matrix ctm;
    ctm.preMult(gp->position, gp->scale, gp->rotation);
    corner.x = -(gp->source->width() >> 1);
    corner.y = (gp->source->height() >> 1);
    ctm.transform(&corner, &ppt, 1);

    tscale = gp->scale * ctx->UTopScale();
    rotation = gp->rotation + ctx->UTopRotation();
    if (rotation >= 360) rotation -= 360;
    else if (rotation < 0) rotation += 360;
    
    fprintf(svgf, "<image transform=\"translate(%d,%d) scale(%g) rotate(%d)\"\n",
			ppt.x, ppt.y, tscale, rotation);
    fprintf(svgf, "  width=\"%dpx\" height=\"%dpx\"",
                        gp->source->width(), gp->source->height());
    fprintf(svgf, " xlink:href=\"%s\">\n", outname);
    fprintf(svgf, "</image>\n");
}

/*-------------------------------------------------------------------------*/

static void SVGDrawSpline(Context* ctx, splineptr thespline, int passcolor)
{
   XPoint       tmppoints[4];

   ctx->CTM().transform(thespline->ctrl, tmppoints, 4);

   fprintf(svgf, "<path d=\"M%d,%d C%d,%d %d,%d %d,%d ",
		tmppoints[0].x, tmppoints[0].y,
		tmppoints[1].x, tmppoints[1].y,
		tmppoints[2].x, tmppoints[2].y,
		tmppoints[3].x, tmppoints[3].y);
   svg_strokepath(ctx, passcolor, thespline->style, thespline->width);
}

/*-------------------------------------------------------------------------*/

static void SVGDrawPolygon(Context* ctx, polyptr thepoly, int passcolor)
{
   int i;
   pointlist tmppoints(thepoly->points.count());

   ctx->CTM().transform(thepoly->points.begin(), tmppoints.begin(), thepoly->points.count());
   
   fprintf(svgf, "<path ");
   if (thepoly->style & BBOX) fprintf(svgf, "visibility=\"hidden\" ");
   fprintf(svgf, "d=\"M%d,%d L", tmppoints[0].x, tmppoints[0].y);
   for (i = 1; i < thepoly->points.count(); i++) {
      fprintf(svgf, "%d,%d ", tmppoints[i].x, tmppoints[i].y);
   }

   svg_strokepath(ctx, passcolor, thepoly->style, thepoly->width);
}

/*-------------------------------------------------------------------------*/

static void SVGDrawArc(Context* ctx, arcptr thearc, int passcolor)
{
   XPoint  endpoints[2];
   int	   radius[2];
   int	   tarc;

   radius[0] = ctx->UTopTransScale(thearc->radius);
   radius[1] = ctx->UTopTransScale(thearc->yaxis);

   tarc = (thearc->angle2 - thearc->angle1);
   if (tarc == 360) {
      ctx->CTM().transform(&(thearc->position), endpoints, 1);
      fprintf(svgf, "<ellipse cx=\"%d\" cy=\"%d\" rx=\"%d\" ry=\"%d\" ",
		endpoints[0].x, endpoints[0].y, radius[0], radius[1]);
      svg_stroke(ctx, passcolor, thearc->style, thearc->width);
   }
   else {
      ctx->CTM().transform(thearc->points, endpoints, 1);
      ctx->CTM().transform(thearc->points + thearc->number - 1, endpoints + 1, 1);

      /* When any arc is flipped, the direction of travel reverses. */
      fprintf(svgf, "<path d=\"M%d,%d A%d,%d 0 %d,%d %d,%d ",
		endpoints[0].x, endpoints[0].y,
		radius[0], radius[1],
		((tarc > 180) ? 1 : 0),
                (((ctx->DCTM()->a() * ctx->DCTM()->e()) >= 0) ? 1 : 0),
		endpoints[1].x, endpoints[1].y);
      svg_strokepath(ctx, passcolor, thearc->style, thearc->width);
   }
}

/*-------------------------------------------------------------------------*/

static void SVGDrawPath(Context* ctx, pathptr thepath, int passcolor)
{
   pointlist	tmppoints;
   genericptr	*genpath;
   polyptr	thepoly;
   splineptr	thespline;
   int		i, firstpt = 1;
   
   fprintf(svgf, "<path d=\"");

   for (genpath = thepath->begin(); genpath != thepath->end(); genpath++) {
      switch(ELEMENTTYPE(*genpath)) {
	 case POLYGON:
	    thepoly = TOPOLY(genpath);
            tmppoints.resize(thepoly->points.count());
            ctx->CTM().transform(thepoly->points.begin(), tmppoints.begin(), thepoly->points.count());
	    if (firstpt) {
	       fprintf(svgf, "M%d,%d ", tmppoints[0].x, tmppoints[0].y);
	       firstpt = 0;
	    }
	    fprintf(svgf, "L");
            for (i = 1; i < thepoly->points.count(); i++) {
	       fprintf(svgf, "%d,%d ", tmppoints[i].x, tmppoints[i].y);
	    }
	    break;
	 case SPLINE:
	    thespline = TOSPLINE(genpath);
            tmppoints.resize(4);
            ctx->CTM().transform(thespline->ctrl, tmppoints.begin(), tmppoints.count());
	    if (firstpt) {
	       fprintf(svgf, "M%d,%d ", tmppoints[0].x, tmppoints[0].y);
	       firstpt = 0;
	    }
	    fprintf(svgf, "C%d,%d %d,%d %d,%d ",
		tmppoints[1].x, tmppoints[1].y,
		tmppoints[2].x, tmppoints[2].y,
		tmppoints[3].x, tmppoints[3].y);
	    break;
      }
   } 
   svg_strokepath(ctx, passcolor, thepath->style, thepath->width);
}

/*----------------------------------------------------------------------*/
/* Main recursive object instance drawing routine.			*/
/*    context is the instance information passed down from above	*/
/*    theinstance is the object instance to be drawn			*/
/*    level is the level of recursion 					*/
/*    passcolor is the inherited color value passed to object		*/
/*----------------------------------------------------------------------*/

static void SVGDrawObject(Context* ctx, objinstptr theinstance, short level, int passcolor, pushlistptr *stack)
{
   genericptr	*areagen;
   float	tmpwidth;
   int		defaultcolor = passcolor;
   int		curcolor = passcolor;
   int		thispart;
   objectptr	theobject = theinstance->thisobject;

   /* All parts are given in the coordinate system of the object, unless */
   /* this is the top-level object, in which they will be interpreted as */
   /* relative to the screen.						 */

   ctx->UPushCTM();

   if (stack) push_stack(stack, theinstance);
   if (level != 0)
       ctx->CTM().preMult(theinstance->position, theinstance->scale,
			theinstance->rotation);

   /* make parameter substitutions */
   psubstitute(theinstance);

   /* draw all of the elements */
   
   tmpwidth = ctx->UTopTransScale(xobjs.pagelist[areawin->page].wirewidth);

   /* Here---set a default style using "g" like PostScript "gsave"	*/
   /* stroke-width = tmpwidth, stroke = passcolor 			*/

   /* guard against plist being regenerated during a redraw by the	*/
   /* expression parameter mechanism (should that be prohibited?)	*/

   for (thispart = 0; thispart < theobject->parts; thispart++) {
      areagen = theobject->begin() + thispart;

      if (defaultcolor != DOFORALL) {
	 if ((*areagen)->color != curcolor) {
	    if ((*areagen)->color == DEFAULTCOLOR)
	       curcolor = defaultcolor;
	    else
	       curcolor = (*areagen)->color;
	 }
      }

      switch(ELEMENTTYPE(*areagen)) {
	 case(POLYGON):
	    if (level == 0 || !((TOPOLY(areagen))->style & BBOX))
               SVGDrawPolygon(ctx, TOPOLY(areagen), curcolor);
	    break;
   
	 case(SPLINE):
            SVGDrawSpline(ctx, TOSPLINE(areagen), curcolor);
	    break;
   
	 case(ARC):
            SVGDrawArc(ctx, TOARC(areagen), curcolor);
	    break;

	 case(PATH):
            SVGDrawPath(ctx, TOPATH(areagen), curcolor);
	    break;

	 case(GRAPHIC):
            SVGDrawGraphic(ctx, TOGRAPHIC(areagen));
	    break;
   
         case(OBJINST):
	    if (areawin->editinplace && stack && (TOOBJINST(areagen)
			== areawin->topinstance)) {
	       /* If stack matches areawin->stack, then don't	*/
	       /* draw because it would be redundant.		 */
	       pushlistptr alist = *stack, blist = areawin->stack;
	       while (alist && blist) {
		  if (alist->thisinst != blist->thisinst) break;
		  alist = alist->next;
		  blist = blist->next;
	       }
	       if ((!alist) || (!blist)) break;
	    }
            SVGDrawObject(ctx, TOOBJINST(areagen), level + 1, curcolor, stack);
	    break;
   
  	  case(LABEL): 
            if (level == 0 || TOLABEL(areagen)->pin == 0 ||
			(TOLABEL(areagen)->justify & PINVISIBLE))
            SVGDrawString(ctx, TOLABEL(areagen), curcolor, theinstance);
	    break;
      }
   }

   ctx->UPopCTM();
   if (stack) pop_stack(stack);
}

/*----------------------------------------------------------------------*/

static void addlinepoint(QVector<XPoint>& points, int x, int y)
{
   points.last().x = x;
   points.append(XPoint(0, -y));
}

/*----------------------------------------------------------------------*/
/* Draw an entire string, including parameter substitutions		*/
/*----------------------------------------------------------------------*/

static void SVGDrawString(Context* ctx, labelptr drawlabel, int passcolor, objinstptr localinst)
{
   stringpart *strptr;
   char *textptr;
   short  fstyle, ffont, tmpjust, baseline, deltay;
   int    pos, defaultcolor, curcolor;
   short  oldx, oldfont, oldstyle;
   int olinerise = 4;
   float  tmpscale = 1.0, natscale = 1.0;
   XPoint newpoint;
   TextExtents tmpext;
   QVector<short> tabstops;
   short tabno, group = 0;
   int open_text, open_span, open_decor;
   QVector<XPoint> decorations;

   const char *symbol_html_encoding[] = {
	" ", "!", "&#8704;", "#", "&#8707;", "%", "&", "?", "(", ")",
	"*", "+", ",", "&#8722;", ".", "/", "0", "1", "2", "3", "4",
	"5", "6", "7", "8", "9", ":", ";", "<", "=", ">", "?", "&#8773;",
	"&#913;", "&#914;", "&#935;", "&#916;", "&#917;", "&#934;",
	"&#915;", "&#919;", "&#921;", "&#977;", "&#922;", "&#923;",
	"&#924;", "&#925;", "&#927;", "&#928;", "&#920;", "&#929;",
	"&#931;", "&#932;", "&#933;", "&#963;", "&#937;", "&#926;",
	"&#936;", "&#918;", "[", "&#8756;", "]", "&#8869;", "_",
	"&#8254;", "&#945;", "&#946;", "&#967;", "&#948;", "&#949;",
	"&#966;", "&#947;", "&#951;", "&#953;", "&#966;", "&#954;",
	"&#955;", "&#956;", "&#957;", "&#959;", "&#960;", "&#952;",
	"&#961;", "&#963;", "&#964;", "&#965;", "&#969;", "&#969;",
	"&#958;", "&#968;", "&#950;", "{", "|", "}", "~", "", "", "",
	"", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
	"", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
	"&#978;", "&#8242;", "&#8804;", "&#8260;", "&#8734;", "&#402;",
	"&#9827;", "&#9830;", "&#9829;", "&#9824;", "&#8596;",
	"&#8592;", "&#8593;", "&#8594;", "&#8595;", "&#176;", "&#177;",
	"&#8243;", "&#8805;", "&#215;", "&#8733;", "&#8706;", "&#8226;",
	"&#247;", "&#8800;", "&#8801;", "&#8773;", "&#8230;"
   };

   /* Standard encoding vector, in HTML, from character 161 to 255 */
   const u_int standard_html_encoding[] = {
	161, 162, 163, 8725, 165, 131, 167, 164, 146, 147, 171, 8249,
	8250, 64256, 64258, 0, 8211, 8224, 8225, 183, 0, 182, 8226,
	8218, 8222, 8221, 187, 8230, 8240, 0, 191, 0, 96, 180, 710,
	126, 713, 728, 729, 168, 0, 730, 184, 0, 733, 731, 711, 8212,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 508, 0,
	170, 0, 0, 0, 0, 321, 216, 338, 186, 0, 0, 0, 0, 0, 230, 0,
	0, 0, 185, 0, 0, 322, 248, 339, 223};

   if (fontcount == 0) return;

   /* Don't draw temporary labels from schematic capture system */
   if (drawlabel->string->type != FONT_NAME) return;

   if (passcolor == DOSUBSTRING)
      defaultcolor = curcolor = drawlabel->color;
   else
      defaultcolor = curcolor = passcolor;

   if (defaultcolor != DOFORALL) {
      if (drawlabel->color != DEFAULTCOLOR)
	 curcolor = drawlabel->color;
      else
	 curcolor = defaultcolor;
   }

   /* calculate the transformation matrix for this object */
   /* in natural units of the alphabet vectors		  */
   /* (conversion to window units)			  */

   /* Labels don't rotate in Firefox, so use <g> record for transform */

   ctx->UPushCTM();
   ctx->CTM().preMult(drawlabel->position, drawlabel->scale, drawlabel->rotation);
   Matrix* const DCTM = ctx->DCTM();

   /* check for flip invariance; recompute CTM and justification if necessary */
   tmpjust = ctx->flipadjust(drawlabel->justify);

   /* Note that the Y-scale is inverted or text comes out upside-down.  But we	*/
   /* need to adjust to the Y baseline.						*/

   fprintf(svgf, "<g transform=\"matrix(%4g %4g %4g %4g %3g %3g)\" ",
        DCTM->a(), DCTM->d(), -(DCTM->b()), -(DCTM->e()), DCTM->c(), DCTM->f());

   svg_printcolor(passcolor, "fill=");
   fprintf(svgf, ">\n");

   /* "natural" (unscaled) length */
   tmpext = ULength(drawlabel, localinst, 0, NULL);

   newpoint.x = (tmpjust & NOTLEFT ?
       (tmpjust & RIGHT ? -tmpext.width : -tmpext.width >> 1) : 0);
   newpoint.y = (tmpjust & NOTBOTTOM ?
       (tmpjust & TOP ? -tmpext.ascent : -(tmpext.ascent + tmpext.base) >> 1)
		: -tmpext.base);

   /* Pinlabels have an additional offset spacing to pad */
   /* them from the circuit point to which they attach.  */

   if (drawlabel->pin) {
      pinadjust(tmpjust, &(newpoint.x), &(newpoint.y), 1);
   }

   oldx = newpoint.x;
   baseline = newpoint.y;

   open_text = -1;
   open_span = 0;
   open_decor = 0;
   pos = 0;
   for (strptr = drawlabel->string; strptr != NULL;
		strptr = nextstringpart(strptr, localinst)) {

      /* All segments other than text cancel any	*/
      /* existing overline/underline in effect.		*/

      if (strptr->type != TEXT_STRING)
	 fstyle &= 0xfc7;

      switch(strptr->type) {
	 case RETURN:
	    while (open_span > 0) {
	       fprintf(svgf, "</tspan>");
	       open_span--;
	    }
	    while (open_text > 0) {
	       fprintf(svgf, "</text>");
	       open_text--;
	    }
            if (open_decor) {
               addlinepoint(decorations, newpoint.x, group);
	       open_decor--;
	    }
	    break;

	 case FONT_SCALE:
	 case FONT_NAME:
	    while (open_span > 0) {
	       fprintf(svgf, "</tspan>");
	       open_span--;
	    }
	    while (open_text > 0) {
	       fprintf(svgf, "</text>");
	       open_text--;
	    }
	    if (open_decor) {
               addlinepoint(decorations, newpoint.x, group);
	       open_decor--;
	    }
	    break;

	 case KERN:
	 case TABFORWARD:
	 case TABBACKWARD:
	 case TABSTOP:
	 case HALFSPACE:
	 case QTRSPACE:
	 case NOLINE:
	 case UNDERLINE:
	 case OVERLINE:
	 case SUBSCRIPT:
	 case SUPERSCRIPT:
	 case NORMALSCRIPT:
	    while (open_span > 1) {
	       fprintf(svgf, "</tspan>");
	       open_span--;
	    }
	    if (open_decor) {
               addlinepoint(decorations, newpoint.x, group);
	       open_decor--;
	    }
	    break;

	 /* These do not need to be handled */
	 case TEXT_STRING:
	 case PARAM_START:
	 case PARAM_END:
	    break;

	 /* These are not handled yet, but should be */
	 case FONT_COLOR:
	    break;

	 default:
	    break;
      }

      /* deal with each text segment type */

      switch(strptr->type) {
	 case FONT_SCALE:
	 case FONT_NAME:
	    if (strptr->data.font < fontcount) {
	       ffont = strptr->data.font;
	       fstyle = 0;		   /* style reset by font change */
	       if (baseline == newpoint.y) {  /* set top-level font and style */
	          oldfont = ffont;
	          oldstyle = fstyle;
	       }
	    }
	    fprintf(svgf, "<text stroke=\"none\" ");
	    fprintf(svgf, "font-family=");
	    if (issymbolfont(ffont))
		fprintf(svgf, "\"Times\" ");
	    else if (!strncmp(fonts[ffont].family, "Times", 5))
		fprintf(svgf, "\"Times\" ");
	    else
	       fprintf(svgf, "\"%s\" ", fonts[ffont].family);

	    if (fonts[ffont].flags & 0x1)
	       fprintf(svgf, " font-weight=\"bold\" ");
	    if (fonts[ffont].flags & 0x2) {
	       if (issansfont(ffont))
	          fprintf(svgf, " font-style=\"oblique\" ");
	       else
		  fprintf(svgf, " font-style=\"italic\" ");
	    }
	    olinerise = (issansfont(ffont)) ? 7 : 4;
		      
	    if (strptr->type == FONT_SCALE) {
	       tmpscale = natscale * strptr->data.scale;
	       if (baseline == newpoint.y) /* reset top-level scale */
		  natscale = tmpscale;
	    }
	    else
	       tmpscale = 1;

	    /* Actual scale taken care of by transformation matrix */
	    fprintf(svgf, "font-size=\"%g\" >", tmpscale * 40);
	    fprintf(svgf, "<tspan x=\"%d\" y=\"%d\">", newpoint.x, -newpoint.y);
	    open_text++;
	    open_span++;
	    break;

	 case KERN:
	    newpoint.x += strptr->data.kern[0];
	    newpoint.y += strptr->data.kern[1];
	    fprintf(svgf, "<text dx=\"%d\" dy=\"%d\">",
			strptr->data.kern[0], strptr->data.kern[1]);
	    open_text++;
	    break;
		
	 case FONT_COLOR:
	    if (defaultcolor != DOFORALL) {
	       if (strptr->data.color != DEFAULTCOLOR)
                  curcolor = colorlist[strptr->data.color];
	       else {
	          curcolor = DEFAULTCOLOR;
	       }
	    }
	    break;

	 case TABBACKWARD:	/* find first tab value with x < xtotal */
            for (tabno = tabstops.count()-1; tabno >= 0; tabno--) {
	       if (tabstops[tabno] < newpoint.x) {
	          newpoint.x = tabstops[tabno];
	          break;
	       }
	    }
	    fprintf(svgf, "<tspan x=\"%d\">", newpoint.x);
	    open_span++;
	    break;

	 case TABFORWARD:	/* find first tab value with x > xtotal */
            for (tabno = 0; tabno < tabstops.count(); tabno++) {
	       if (tabstops[tabno] > newpoint.x) {
		  newpoint.x = tabstops[tabno];
		  break;
	       }
	    }
	    fprintf(svgf, "<tspan x=\"%d\">", newpoint.x);
	    open_span++;
	    break;

	 case TABSTOP:
            tabstops.append(newpoint.x);
	    /* Force a tab at this point so that the output aligns	*/
	    /* to our computation of the position, not its own.	*/
	    fprintf(svgf, "<tspan x=\"%d\">", newpoint.x);
	    open_span++;
	    break;

	 case RETURN:
	    tmpscale = natscale = 1.0;
	    baseline -= BASELINE;
	    newpoint.y = baseline;
	    newpoint.x = oldx;
	    fprintf(svgf, "<tspan x=\"%d\" y=\"%d\">", newpoint.x, -newpoint.y);
	    open_span++;
	    break;
	
	 case SUBSCRIPT:
	    natscale *= SUBSCALE; 
	    tmpscale = natscale;
	    deltay = (short)((TEXTHEIGHT >> 1) * natscale);
	    newpoint.y -= deltay;
	    fprintf(svgf, "<tspan dy=\"%d\" font-size=\"%g\">", deltay,
			40 * natscale);
	    open_span++;
	    break;

	 case SUPERSCRIPT:
	    natscale *= SUBSCALE;
	    tmpscale = natscale;
	    deltay = (short)(TEXTHEIGHT * natscale);
	    newpoint.y += deltay;
	    fprintf(svgf, "<tspan dy=\"%d\" font-size=\"%g\">", -deltay,
			40 * natscale);
	    open_span++;
	    break;

	 case NORMALSCRIPT:
	    tmpscale = natscale = 1.0;
	    ffont = oldfont;	/* revert to top-level font and style */
	    fstyle = oldstyle;
	    newpoint.y = baseline;
	    fprintf(svgf, "<tspan y=\"%d\">", baseline); 
	    open_span++;
	    break;

	 case UNDERLINE:
	    fstyle |= 8;
	    group = newpoint.y - 6;
            addlinepoint(decorations, newpoint.x, group);
	    open_decor++;
	    break;

	 case OVERLINE:
	    if (strptr->nextpart != NULL && strptr->nextpart->type == TEXT_STRING) {
	       objectptr charptr;
	       int tmpheight;

	       group = 0;
	       for (textptr = strptr->nextpart->data.string;
				textptr && *textptr != '\0'; textptr++) {
		  charptr = fonts[ffont].encoding[*(u_char *)textptr];
		  tmpheight = (int)((float)charptr->bbox.height
				* fonts[ffont].scale);
		  if (group < tmpheight) group = (short)tmpheight;
	       }
	       fstyle |= 16;
	       group += olinerise + newpoint.y;
               addlinepoint(decorations, newpoint.x, group);
	    }
	    open_decor++;
	    break;

	 case NOLINE:
	    break;

	 case HALFSPACE: case QTRSPACE: {
	    short addx;
	    objectptr drawchar = fonts[ffont].encoding[(u_char)32];
	    addx = (drawchar->bbox.lowerleft.x + drawchar->bbox.width) *
			fonts[ffont].scale;
	    addx >>= ((strptr->type == HALFSPACE) ? 1 : 2);
	    newpoint.x += addx;
	    fprintf(svgf, "<tspan dx=\"%d\">", addx);
	    open_span++;

	    } break;
	    
	 case TEXT_STRING:
	    textptr = strptr->data.string;

	    if (issymbolfont(ffont)) {
	       for (; *textptr != '\0'; textptr++)
		  if (((u_char)(*textptr) >= 32) && ((u_char)(*textptr) < 158))
		     fprintf(svgf, "%s", symbol_html_encoding[(*textptr) - 32]);
	    }
	    else {
	       /* Handle "&" and non-ASCII characters in the text */
	       if (isisolatin1(ffont)) {
		  for (; *textptr != '\0'; textptr++) {
		     if (*textptr == '&')
		        fprintf(svgf, "&amp;");
		     else if ((u_char)(*textptr) >= 128)
		        fprintf(svgf, "&#%d;", (int)((u_char)*textptr));
		     else if ((u_char)(*textptr) >= 32)
			fprintf(svgf, "%c", *textptr);
		  }
	       }
	       else {
		  for (; *textptr != '\0'; textptr++) {
		     if (*textptr == '&')
		        fprintf(svgf, "&amp;");
		     else if ((u_char)(*textptr) >= 161)
		        fprintf(svgf, "&#%d;",
				standard_html_encoding[(u_char)(*textptr)
				- 161]);
		     else if ((u_char)(*textptr) >= 32 && (u_char)(*textptr) < 161)
			fprintf(svgf, "%c", *textptr);
		  }
	       }
	    }
	    pos--;

	    /* Compute the new X position */

	    for (textptr = strptr->data.string; *textptr != '\0'; textptr++) {
	       objectptr drawchar = fonts[ffont].encoding[(u_char)(*textptr)];
	       short addx = (drawchar->bbox.lowerleft.x + drawchar->bbox.width) *
			fonts[ffont].scale;
	       newpoint.x += addx;
	    }
	    break;
      }
      pos++;
   }
   while (open_span > 0) {
      fprintf(svgf, "</tspan>");
      open_span--;
   }
   while (open_text > 0) {
      fprintf(svgf, "</text>");
      open_text--;
   }
   fprintf(svgf, "\n</text>");

   ctx->UPopCTM();

   /* If there were decorations (underlines, overlines), generate them */

   if (! decorations.isEmpty()) {
      int i;
      if (open_decor) {
         addlinepoint(decorations, newpoint.x, group);
      }
      for (i = 0; i < decorations.count(); i += 2) {
	 fprintf(svgf, "\n<line stroke-width=\"2\" stroke-linecap=\"square\" "
		"x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" />",
		decorations[i].x, decorations[i].y, decorations[i + 1].x,
		decorations[i + 1].y);
      }
   }
   fprintf(svgf, "</g>\n");
}

/*----------------------------------------------------------------------*/
/* Write the SVG file output						*/
/*----------------------------------------------------------------------*/

#define PMARGIN	6		/* Pixel margin around drawing */

static void OutputSVG(Context* ctx, char *filename, bool fullscale)
{
   short	savesel;
   objinstptr	pinst;
   int cstyle;
   float outwidth, outheight, cscale;

   svgf = fopen(filename, "w");
   if (svgf == NULL) {
      Fprintf(stderr, "Cannot open file %s for writing.\n", filename);
      return;
   }

   /* Generate external image files, if necessary */
   SVGCreateImages(areawin->page);

   /* Save the number of selections and set it to zero while we do the	*/
   /* object drawing.							*/

   savesel = areawin->selects;
   areawin->selects = 0;
   pinst = xobjs.pagelist[areawin->page].pageinst;

   ctx->UPushCTM();	/* Save the top-level graphics state */
   Matrix* const DCTM = ctx->DCTM();

   /* This is like UMakeWCTM()---it inverts the whole picture so that	*/
   /* The origin is at the top left, and all data points fit in a box	*/
   /* at (0, 0) to the object (width, height)				*/

   DCTM->set(1.0, 0.0,-pinst->bbox.lowerleft.x, 0.0, -1.0, pinst->bbox.lowerleft.y + pinst->bbox.height);

   fprintf(svgf, "<svg xmlns=\"http://www.w3.org/2000/svg\"\n");
   fprintf(svgf, "   xmlns:xlink=\"http://www.w3.org/1999/xlink\"\n");
   fprintf(svgf, "   version=\"1.1\"\n");
   fprintf(svgf, "   id=\"%s\" ", pinst->thisobject->name);

   if (fullscale) {
      fprintf(svgf, "width=\"100%%\" height=\"100%%\" ");
   }
   else {
      cscale = getpsscale(xobjs.pagelist[areawin->page].outscale, areawin->page);
      cstyle = xobjs.pagelist[areawin->page].coordstyle;

      outwidth = toplevelwidth(pinst, NULL) * cscale;
      outwidth /= (cstyle == CM) ?  IN_CM_CONVERT : 72.0;
      outheight = toplevelheight(pinst, NULL) * cscale;
      outheight /= (cstyle == CM) ?  IN_CM_CONVERT : 72.0;

      /* Set display height to that specified in the output properties (in inches) */
      fprintf(svgf, "width=\"%.3g%s\" height=\"%.3g%s\" ",
		outwidth, (cstyle == CM) ? "cm" : "in",
		outheight, (cstyle == CM) ? "cm" : "in");
   }
   fprintf(svgf, " viewBox=\"%d %d %d %d\">\n",
		-PMARGIN, -PMARGIN, pinst->bbox.width + PMARGIN,
		pinst->bbox.height + PMARGIN);

   fprintf(svgf, "<desc>\n");
   fprintf(svgf, "XCircuit Version %2.1f\n", PROG_VERSION);
   fprintf(svgf, "File \"%s\" Page %d\n", xobjs.pagelist[areawin->page].filename.toLocal8Bit().data(),
		areawin->page + 1); 
   fprintf(svgf, "</desc>\n");

   /* Set default color to black */
   fprintf(svgf, "<g stroke=\"black\">\n");

   pushlistptr hierstack = NULL;
   SVGDrawObject(ctx, areawin->topinstance, TOPLEVEL, FOREGROUND, &hierstack);
   free_stack(&hierstack);

   /* restore the selection list (if any) */
   areawin->selects = savesel;

   fprintf(svgf, "</g>\n</svg>\n");
   fclose(svgf);

   ctx->UPopCTM();	/* Restore the top-level graphics state */
}

/*----------------------------------------------------------------------*/
/* The TCL command-line for the SVG file write routine.			*/
/*----------------------------------------------------------------------*/

#ifdef TCL_WRAPPER
int xctcl_svg(ClientData clientData, Tcl_Interp *interp,
        int objc, Tcl_Obj *CONST objv[])
{
   char filename[128], *pptr;
   bool fullscale = 0;
   int locobjc = objc;
   char *lastarg;

   /* Argument "-full" forces full scale (not scaled per page output settings) */
   if (objc > 1) {
      lastarg = Tcl_GetString(objv[objc - 1]);
      if (lastarg[0] == '-') {
         if (!strncmp(lastarg + 1, "full", 4))
	    fullscale = 1;
	 else {
	    Tcl_SetResult(interp, "Unknown option.\n", NULL);
	    return TCL_ERROR;
	 }
	 locobjc--;
      }
   }


   if (locobjc >= 2) {
      /* If there is a non-option argument, use it for the output filename */
      sprintf(filename, Tcl_GetString(objv[1]));
   }
   else if (xobjs.pagelist[areawin->page]->pageinst->thisobject->name == NULL)
      sprintf(filename, xobjs.pagelist[areawin->page]->filename);
   else
      sprintf(filename, xobjs.pagelist[areawin->page]->pageinst->thisobject->name);

   pptr = strrchr(filename, '.');
   if (pptr != NULL)
      sprintf(pptr + 1, "svg");
   else if (strcmp(filename + strlen(filename) - 3, "svg"))
      strcat(filename, ".svg");

   OutputSVG(filename, fullscale);
   Fprintf(stdout, "Saved page as SVG format file \"%s\"\n", filename);
   return XcTagCallback(interp, objc, objv);
}
#endif

/*-------------------------------------------------------------------------*/
