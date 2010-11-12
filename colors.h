#ifndef COLORS_H
#define COLORS_H

#include <QString>
#include <QVector>
#include <QMap>
#include <QRgb>

class QAction;
class QMenu;

/*-------------------------------------------------------------------------*/
/* Add colors here as needed. 						   */
/* Reflect all changes in the resource manager, xcircuit.c		   */
/* And ApplicationDataPtr in xcircuit.h					   */
/*-------------------------------------------------------------------------*/

extern QVector<QRgb> colorlist;
extern int 	  *appcolors;

#define NUMBER_OF_COLORS	16

#define BACKGROUND	appcolors[0]
#define FOREGROUND	appcolors[1]
#define SELECTCOLOR     appcolors[2]
#define FILTERCOLOR	appcolors[3]
#define GRIDCOLOR	appcolors[4]
#define SNAPCOLOR	appcolors[5]
#define AXESCOLOR	appcolors[6]
#define OFFBUTTONCOLOR	appcolors[7]
#define AUXCOLOR	appcolors[8]
#define BARCOLOR	appcolors[9]
#define PARAMCOLOR	appcolors[10]

/* The rest of the colors are layout colors, not GUI colors */

#define BBOXCOLOR	appcolors[11]
#define LOCALPINCOLOR	appcolors[12]
#define GLOBALPINCOLOR	appcolors[13]
#define INFOLABELCOLOR	appcolors[14]
#define RATSNESTCOLOR	appcolors[15]

#define DEFAULTCOLOR	-1U		/* Inherits color of parent */
#define DOFORALL	-2		/* All elements inherit same color */
#define DOSUBSTRING	-3		/* Only selected substring drawn */
                                        /* in the SELECTCOLOR		*/
#define BADCOLOR	-1		/* When returned from query_named_color */
#define ERRORCOLOR	-2		/* When returned from query_named_color */

//
//
//

QRgb getnamedcolor(const char *);
/// Add a new color entry to colorlist, update GUI accordingly
/// menu is specified only during GUI_init when the menus aren't fully set up yet
int addnewcolorentry(QRgb, QMenu* menu = NULL);
/// Mark given color as current color in the GUI
void setcolormark(QRgb);
/// Finds the action for the current color mark
QAction *colormarkaction();

//
// Macros for QPainter* color and function handling
//

#define XTopSetForeground(gc, a) if (a == DEFAULTCOLOR) SetForeground( \
        gc, FOREGROUND); else SetForeground(gc, a)

#define XSetXORFg(ctx, a,b) if (a == DEFAULTCOLOR) SetForeground( \
        (ctx)->gc(), FOREGROUND ); else SetForeground((ctx)->gc(),\
        a)

#define XSetXORFg_orig(ctx, a,b) if (a == DEFAULTCOLOR) SetForeground( \
        (ctx)->gc(), FOREGROUND ^ b); else SetForeground((ctx)->gc(),\
        a ^ b)

#define XcTopSetForeground(ctx, z) do { XTopSetForeground((ctx)->gc(), z); (ctx)->gccolor = \
        ((z) == DEFAULTCOLOR) ? FOREGROUND : (z); } while (false)

#define XcSetXORFg(ctx, y,z) do { XSetXORFg(ctx,y,z); (ctx)->gccolor = \
        ((y) == DEFAULTCOLOR) ? (FOREGROUND) : (y); } while (false)

#define XcSetXORFg_orig(ctx, y,z) do { XSetXORFg(ctx,y,z); (ctx)->gccolor = \
        ((y) == DEFAULTCOLOR) ? (FOREGROUND ^ z) : (y ^ z); } while (false)

//

#endif // COLORS_H
