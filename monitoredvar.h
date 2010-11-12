#ifndef MONITOREDVAR_H
#define MONITOREDVAR_H

#include <QtGlobal>

/*----------------------------------------------------------------------*/
/* Event mode definitions (state of drawing area)			*/
/*----------------------------------------------------------------------*/

enum editmode {
  NORMAL_MODE = 0,	/* On the drawing page, none of the situations below */
  UNDO_MODE,		/* In the process of an undo/redo operation */
  MOVE_MODE,		/* In the process of moving elements */
  COPY_MODE,		/* In the process of copying elements */
  PAN_MODE,		/* In the process of panning to follow the cursor */
  SELAREA_MODE,		/* Area selection box */
  PENDING_MODE,		/* Temporary mode to select without drawing selection */
  RESCALE_MODE,		/* Interactive element rescaling box */
  CATALOG_MODE,		/* On a library page, library directory, or page directory */
  CATTEXT_MODE,		/* Editing an existing object name in the library */
  FONTCAT_MODE,		/* 10 Accessing the font character page from TEXT_MODE */
  EFONTCAT_MODE,	/* Accessing the font character page from ETEXT_MODE */
  TEXT_MODE,		/* Creating a new label */
  WIRE_MODE,		/* Creating a new polygon (wire) */
  BOX_MODE,		/* Creating a new box */
  ARC_MODE,		/* Creating a new arc */
  SPLINE_MODE,		/* Creating a new spline */
  ETEXT_MODE,		/* Editing an exiting label */
  EPOLY_MODE,		/* Editing an existing polygon */
  EARC_MODE,		/* Editing an existing arc */
  ESPLINE_MODE,		/* 20 Editing an existing spline */
  EPATH_MODE,		/* Editing an existing path */
  EINST_MODE,		/* Editing an instance (from the level above) */
  ASSOC_MODE,		/* Choosing an associated schematic or symbol */
  CATMOVE_MODE		/* Moving objects in or between libraries */
};

class EventMode
{
public:
    inline EventMode() : mode(NORMAL_MODE) {}
    inline bool operator == (editmode m) const { return mode == m; }
    inline bool operator != (editmode m) const { return mode != m; }
    inline operator editmode() const { return mode; }
    EventMode & operator = (editmode m);
    void update();
protected:
    editmode mode;
private:
    Q_DISABLE_COPY(EventMode);
};

#endif // MONITOREDVAR_H
