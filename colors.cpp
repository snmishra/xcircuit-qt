#include <QPixmap>
#include <QPainter>
#include <QMenu>
#include <QColor>
#include <QToolBar>
#include <QToolButton>

#include "xcircuit.h"
#include "colors.h"
#include "xcolors.h"
#include "xcqt.h"
#include "prototypes.h"
#include "menus.h"

class CaseInsensitiveKey : public QString {
public:
    CaseInsensitiveKey(const char * s) : QString(s) {}
    inline bool operator<(const CaseInsensitiveKey & other) const {
        return compare(other, *this, Qt::CaseInsensitive) < 0;
    }
};

static QMap<CaseInsensitiveKey, QRgb> namedColors;

//
//
//

int *appcolors;
QVector<QRgb> colorlist;

static void initialize();
static struct Init { Init() { initialize(); } } init;

//
//
//

static void initialize()
{
    appcolors = new int[NUMBER_OF_COLORS];
    for (int i = 0; X11Colors[i].name != NULL; ++i) {
        namedColors.insert(X11Colors[i].name, qRgb(X11Colors[i].red, X11Colors[i].green, X11Colors[i].blue));
    }
}

//
//
//

QRgb getnamedcolor(const char *name)
{
    if (! namedColors.contains(name)) return BADCOLOR;
    return namedColors[name];
}


/*----------------------------------------------------------------------*/
/* Check if color within RGB roundoff error exists in xcircuit's color	*/
/* table.  Assume 24-bit color, in which resolution can be no less than	*/
/* 256 for each color component.  Visual acuity is a bit less than 24-	*/
/* bit color, so assume difference should be no less than 512 per	*/
/* component for colors to be considered "different".  Psychologically,	*/
/* we should really find the just-noticable-difference for each color	*/
/* component separately!  But that's too complicated for this simple	*/
/* routine.								*/
/*									*/
/* Return the table entry of the color, if it is in xcircuit's color	*/
/* table, or ERRORCOLOR if not.  If it is in the color table, then	*/
/* return the actual pixel value from the table in the "pixval" pointer	*/
/*----------------------------------------------------------------------*/

#if 0
static int rgb_querycolor(QRgb c, int *pixval = NULL)
{
   for (int i = 0; i < colorlist.count(); i++) {
      if (abs(qRed(colorlist[i]) - qRed(c)) < 2 &&
             abs(qGreen(colorlist[i]) - qGreen(c)) < 2 &&
             abs(qBlue(colorlist[i]) - qBlue(c)) < 2) {
         if (pixval)
            *pixval = colorlist[i];
         return i;
         break;
      }
   }
   return ERRORCOLOR;
}
#endif

//
//
//

int addnewcolorentry(QRgb ccolor, QMenu* mainMenu)
{
    static const int iconWidth = 24;
    static const int iconHeight = 24;

    int i = colorlist.indexOf(ccolor);
    if (i >= 0) return i;

    /* color icon */
    QPixmap pm(iconWidth, iconHeight);
    QPainter p(&pm);
    p.fillRect(pm.rect(), QColor(ccolor));

    /* add action to the main menu */
    if (!mainMenu) mainMenu = menuAction("Elements_Color")->menu();
    QAction * action = mainMenu->addAction(QIcon(pm), "");
    action->setCheckable(true);
    XtAddCallback (action, setcolor, NULL);
    colorlist.append(ccolor);

    return colorlist.count()-1;
}

void setcolormark(QRgb colorval)
{
    int i;
    if (colorval != DEFAULTCOLOR) {
        i = 3 + colorlist.indexOf(colorval);
        if (i<3) return; // no such color :(
    } else {
        i = 2; // index of the "Inherit Color" action
    }

    // 1. mark the color in the menu
    toggleexcl(menuAction("Elements_Color")->menu()->actions()[i]);
    // 2. mark the color on the toolbar
    QAbstractButton *button = toolbar->findChild<QAbstractButton*>("Colors");
    int toolIndex = button->property("index").toInt();
    QImage img(ToolBar[toolIndex].icon_data);
    QPixmap pix(img.size());
    QPainter p(&pix);
    if (i==2) {
        // inherit color -- default pixmap
        p.drawImage(0, 0, img);
    } else {
        // color pixmap
        p.fillRect(img.rect(), QColor(colorval));
    }
    button->actions().first()->setIcon(QIcon(pix));
}

QAction *colormarkaction()
{
    QAction *action = NULL;
    int i = colorlist.indexOf(areawin->color);
    action = menuAction("Elements_Color")->menu()->actions()[i+3];
    return action;
}
