#include "monitoredvar.h"
#include "xcircuit.h"
#include "prototypes.h"

EventMode & EventMode::operator = (editmode m)
{
    mode = m;
    if (!areawin) return *this;

    // disable actions that only apply in text mode
    bool inTextMode = m == TEXT_MODE || m == ETEXT_MODE;
    setEnabled(menuAction("Style_Subscript"), inTextMode);
    setEnabled(menuAction("Style_Underline"), inTextMode);
    setEnabled(menuAction("Text_Insert"), inTextMode);

    return *this;
}

void EventMode::update()
{
    *this = (editmode)*this;
}
