#include <cstdlib>

#include "elements.h"

Plist::Plist():
        parts(0),
        plist((genericptr*)malloc(1*sizeof(genericptr)))
{}

Plist::Plist(const Plist & src):
        parts(0),
        plist(NULL)
{
    *this = src;
}

Plist::~Plist()
{
    QString s;
    if (plist) {
        for (genericptr* p = begin(); p != end(); ++p) s += QString("%1 ").arg((int)*p,0,16);
    }
    if (false) qDebug("~Plist() %p [plist=%p]: %ls", this, plist, s.utf16());
    free(plist);
    plist=0;
}

Plist & Plist::operator=(const Plist & src)
{
    if (&src == this) return *this;
    parts = 0;
    free(plist);
    plist = (genericptr *)malloc(src.parts * sizeof(genericptr));
    if (false) qDebug("= plist %p gets a new plist %p", this, plist);

    for (generic* const *gen = src.begin(); gen != src.end(); ++gen) {
        append((*gen)->copy());
    }
    return *this;
}

void Plist::clear()
{
    if (parts) {
        free(plist);
        parts = 0;
        plist = (genericptr *)malloc(1*sizeof(genericptr));
        if (false) qDebug("clr plist %p gets a new plist %p", this, plist);
    }
}

bool Plist::operator==(const Plist & o) const
{
    if (parts != o.parts) return false;
    for (genericptr *src = plist, *dst = o.plist; src < end(); ++src, ++dst) {
        if (*src != *dst) return false;
    }
    return true;
}

genericptr* Plist::append(genericptr ptr) {
    plist = (genericptr*) realloc(plist, (parts+1)*sizeof(genericptr));
    if (false) qDebug("+ plist %p gets a resized plist %p", this, plist);
    *(plist+parts) = ptr;
    ++parts;
    return plist+parts-1;
}

genericptr* Plist::temp_append(genericptr ptr) {
    plist = (genericptr*) realloc(plist, (parts+1)*sizeof(genericptr));
    if (false) qDebug("t+ plist %p gets a resized plist %p", this, plist);
    *(plist+parts) = ptr;
    return plist+parts;
}

void Plist::replace_last(genericptr ptr) {
    delete end()[-1];
    end()[-1] = ptr;
}
