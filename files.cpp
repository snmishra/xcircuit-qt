/*-----------------------------------------------------------------------*/
/* files.c --- file handling routines for xcircuit			 */
/* Copyright (c) 2002  Tim Edwards, Johns Hopkins University        	 */
/*-----------------------------------------------------------------------*/

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QTemporaryFile>
#include <QAction>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cctype>
#include <stdint.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef XC_WIN32
#include <winsock2.h>
#endif

#ifdef TCL_WRAPPER 
#include <tk.h>
#endif

#include "xcircuit.h"
#include "prototypes.h"
#include "xcqt.h"
#include "colors.h"

#ifdef ASG
extern void Route(XCWindowData *, bool);
extern int ReadSpice(FILE *);
#endif

/*------------------------------------------------------------------------*/
/* Useful (local) defines						  */
/*------------------------------------------------------------------------*/

#define OUTPUTWIDTH 80	/* maximum text width of output */

#define S_OBLIQUE   13	/* position of Symbol-Oblique in font array */

#define VEPS	    1e-3

/*------------------------------------------------------------------------*/
/* External Variable definitions                                          */
/*------------------------------------------------------------------------*/

#ifdef TCL_WRAPPER
extern Tcl_Interp *xcinterp;
#endif

extern char _STR[150];
extern XCWindowData *areawin;
extern fontinfo *fonts;
extern short fontcount;
extern Cursor appcursors[NUM_CURSORS];
extern XtAppContext app;
extern Window win;
extern short beeper;


/*------------------------------------------------------*/
/* Global variable definitions				*/
/*------------------------------------------------------*/

bool load_in_progress = false;
float version;

/* Structure for remembering what names refer to the same object */

aliasptr aliastop;
   
/*------------------------------------------------------*/
/* Simple utility---get rid of newline character	*/
/*------------------------------------------------------*/

char *ridnewline(char *sptr)
{
   char *tstrp;

   for (tstrp = sptr; *tstrp != '\0' && *tstrp != '\n'; tstrp++) {};
   if (*tstrp == '\n') *tstrp = '\0';
   return tstrp;
}

/*----------------------------------------------------------------------*/
/* Check if two filenames are equivalent. The absolute file paths       */
/* are compared. Returns 0 on match, 1 on mismatch.                     */
/*----------------------------------------------------------------------*/

#define PATHSEP (QDir::separator())

int filecmp(const QString & filename1, const QString & filename2)
{
    QFileInfo fi1(filename1);
    QFileInfo fi2(filename2);
    if (fi1.absoluteFilePath() == fi2.absoluteFilePath()) return 0;
    return 1;
}

/*--------------------------------------------------------------*/
/* Make sure that a string (object or parameter name) is a	*/
/* valid PostScript name.  We do this by converting illegal	*/
/* characters to the PostScript \ooo (octal value) form.	*/
/*								*/
/* This routine does not consider whether the name might be a	*/
/* PostScript numeric value.  This problem is taken care of by	*/
/* having the load/save routines prepend '@' to parameters and	*/
/* a technology namespace to object names.			*/
/*								*/
/* If "need_prefix" is true, then prepend "@" to the result	*/
/* string, unless teststring is a numerical parameter name	*/
/* (p_...).							*/
/*--------------------------------------------------------------*/

char *create_valid_psname(char *teststring, bool need_prefix)
{
   int i, isize, ssize;
   static char *optr = NULL;
   char *sptr, *pptr;
   bool prepend = need_prefix;
   char illegalchars[] = {'/', '}', '{', ']', '[', ')', '(', '<', '>', ' ', '%'};

   /* Check for illegal characters which have syntactical meaning in */
   /* PostScript, and the presence of nonprintable characters or     */
   /* whitespace.		     				     */

   ssize = strlen(teststring);
   isize = ssize;

   if (need_prefix && !strncmp(teststring, "p_", 2))
      prepend = false;
   else
      isize++;

   for (sptr = teststring; *sptr != '\0'; sptr++) {
      if ((!isprint(*sptr)) || isspace(*sptr))
	 isize += 3;
      else {
         for (i = 0; i < sizeof(illegalchars); i++) {
	    if (*sptr == illegalchars[i]) {
	       isize += 3;
	       break;
	    }
	 }
      }
   }
   if (isize == ssize) return teststring;
   isize++;

   if (optr == NULL)
      optr = (char *)malloc(isize);
   else
      optr = (char *)realloc(optr, isize);

   pptr = optr;

   if (prepend) *pptr++ = '@';

   for (sptr = teststring; *sptr != '\0'; sptr++) {
      if ((!isprint(*sptr)) || isspace(*sptr)) {
	 sprintf(pptr, "\\%03o", *sptr);
	 pptr += 4;
      }
      else {
         for (i = 0; i < sizeof(illegalchars); i++) {
	    if (*sptr == illegalchars[i]) {
	       sprintf(pptr, "\\%03o", *sptr);
	       pptr += 4;
	       break;
	    }
         }
	 if (i == sizeof(illegalchars))
	    *pptr++ = *sptr;
      }
   }
   *pptr++ = '\0';
   return optr;
}

/*------------------------------------------------------*/
/* Turn a PostScript string with possible backslash	*/
/* escapes into a normal character string.		*/
/*							*/
/* if "spacelegal" is true, we are parsing a PostScript	*/
/* string in parentheses () where whitespace is legal.	*/
/* If false, we are parsing a PostScript name where	*/
/* whitespace is illegal, and any whitespace should be	*/
/* considered the end of the name.			*/
/*							*/
/* "dest" is ASSUMED to be large enough to hold the	*/
/* result.  "dest" is always equal to or smaller than	*/
/* "src" in length.  "size" should be the maximum size	*/
/* of the string, or 1 less that the allocated memory,	*/
/* allowing for a final NULL byte to be added.		*/
/*							*/
/* The fact that "dest" is always smaller than or equal	*/
/* to "src" means that parse_ps_string(a, a, ...) is	*/
/* legal.						*/
/*							*/
/* Return 0 if the result is empty, 1 otherwise.	*/
/*------------------------------------------------------*/

int parse_ps_string(char *src, char *dest, int size, bool spacelegal, bool strip)
{
   char *sptr = src;
   char *tptr = dest;
   int tmpdig, rval = 0;

   /* Strip leading "@", inserted by XCircuit to	*/
   /* prevent conflicts with PostScript reserved	*/
   /* keywords or numbers.				*/

   if (strip && (*sptr == '@')) sptr++;

   for (;; sptr++) {
      if ((*sptr == '\0') || (isspace(*sptr) && !spacelegal)) {
	 *tptr = '\0';
	 break;
      }
      else {
         if (*sptr == '\\') {
            sptr++;
	    if (*sptr >= '0' && *sptr < '8') {
	       sscanf(sptr, "%3o", &tmpdig);
	       *tptr++ = (u_char)tmpdig;
	       sptr += 2;
	    }
            else
	       *tptr++ = *sptr;
         }
	 else
	    *tptr++ = *sptr;
	 rval = 1;
      }
      if ((int)(tptr - dest) > size) {
	 Wprintf("Warning:  Name \"%s\" in input exceeded buffer length!\n", src);
	 *tptr = '\0';
	 return rval;
      }
   }
   return rval;
}

/*------------------------------------------------------*/
/* Free memory allocated to a label string		*/
/*------------------------------------------------------*/

void freelabel(stringpart *string)
{
   stringpart *strptr = string, *tmpptr;

   while (strptr != NULL) {
      if (strptr->type == TEXT_STRING || strptr->type == PARAM_START)
         free(strptr->data.string);
      tmpptr = strptr->nextpart;
      free(strptr);
      strptr = tmpptr;
   }
}

/*---------------------------------------------------------*/

void pagereset(short rpage)
{
    /* New pages pick up their properties from page 0, which can be changed */
    /* from the .xcircuitrc file on startup (or loaded from a script).	   */
    /* Thanks to Norman Werner (norman.werner@student.uni-magdeburg.de) for */
    /* pointing out this more obvious way of doing the reset, and providing */
    /* a patch.								   */

    Pagedata & page = xobjs.pagelist[rpage];

    page = xobjs.pagelist[0];
    page.filename.clear();
    page.background.name.clear();
    clearselects();

    if (page.coordstyle == CM) {
        page.pagesize = XPoint(595, 842); // A4
    }
    else {
        page.pagesize = XPoint(612, 792); // letter
    }
}

/*------------------------*/
/* scale renormalization  */
/*------------------------*/

float getpsscale(float value, short page)
{
   if (xobjs.pagelist[page].coordstyle != CM)
      return (value * INCHSCALE);
   else
      return (value * CMSCALE);
}

/*---------------------------------------------------------------*/
/* Keep track of columns of output and split lines when too long */
/*---------------------------------------------------------------*/

void dostcount(FILE *ps, short *count, short addlength)
{
   *count += addlength;
   if (*count > OUTPUTWIDTH) {
      *count = addlength;
      fprintf(ps, "\n");
   }
}

/*----------------------------------------------------------------------*/
/* Write a numerical value as a string to _STR, making a parameter	*/
/* substitution if appropriate.						*/
/*----------------------------------------------------------------------*/

void varpcheck(FILE *ps, short value, objectptr localdata, int pointno,
	short *stptr, genericptr thiselem, u_char which)
{
   oparamptr ops;
   eparamptr epp;
   bool done = false;

   for (epp = thiselem->passed; epp != NULL; epp = epp->next) {
      if (epp->pdata.pointno != pointno) continue;
      ops = match_param(localdata, epp->key);
      if (ops != NULL && (ops->which == which)) {
	 sprintf(_STR, "%s ", epp->key);
	 done = true;
	 break;
      }
   }
      
   if (!done)
      sprintf(_STR, "%d ", (int)value);

   dostcount (ps, stptr, strlen(_STR));
   fputs(_STR, ps);
}

/*----------------------------------------------------------------------*/
/* like varpcheck(), but without pointnumber				*/
/*----------------------------------------------------------------------*/

void varcheck(FILE *ps, short value, objectptr localdata,
	short *stptr, genericptr thiselem, u_char which)
{
   varpcheck(ps, value, localdata, 0, stptr, thiselem, which);
}

/*----------------------------------------------------------------------*/
/* like varcheck(), but for floating-point values			*/
/*----------------------------------------------------------------------*/

void varfcheck(FILE *ps, float value, objectptr localdata, short *stptr,
	genericptr thiselem, u_char which)
{
   oparamptr ops;
   eparamptr epp;
   bool done = false;

   for (epp = thiselem->passed; epp != NULL; epp = epp->next) {
      ops = match_param(localdata, epp->key);
      if (ops != NULL && (ops->which == which)) {
	 sprintf(_STR, "%s ", epp->key);
	 done = true;
	 break;
      }
   }
   
   if (!done)
      sprintf(_STR, "%3.3f ", value);

   dostcount (ps, stptr, strlen(_STR));
   fputs(_STR, ps);
}

/*----------------------------------------------------------------------*/
/* Like varpcheck(), for path types only.				*/
/*----------------------------------------------------------------------*/

void varpathcheck(FILE *ps, short value, objectptr localdata, int pointno,
	short *stptr, genericptr *thiselem, pathptr thispath, u_char which)
{
   oparamptr ops;
   eparamptr epp;
   bool done = false;

   for (epp = thispath->passed; epp != NULL; epp = epp->next) {
      if (epp->pdata.pathpt[1] != pointno) continue;
      if (epp->pdata.pathpt[0] != (short)(thiselem - thispath->begin())) continue;
      ops = match_param(localdata, epp->key);
      if (ops != NULL && (ops->which == which)) {
	 sprintf(_STR, "%s ", epp->key);
	 done = true;
	 break;
      }
   }
      
   if (!done)
      sprintf(_STR, "%d ", (int)value);

   dostcount (ps, stptr, strlen(_STR));
   fputs(_STR, ps);
}

/* Structure used to hold data specific to each load mode.  See	*/
/* xcircuit.h for the list of load modes (enum loadmodes)	*/

typedef struct _loaddata {
    PopupCallback func;	/* Routine to run to load the file */
    const char *prompt;		/* Substring name of action, for prompting */
    const char *filext;		/* Default extention of file to load */
} loaddata;

/*-------------------------------------------------------*/
/* Load a PostScript or Python (interpreter script) file */
/*-------------------------------------------------------*/

void getfile(QAction* a, void* mode, void* crashfn)
{
   static loaddata loadmodes[LOAD_MODES] = {
	{normalloadfile,  "load",    "ps"},	/* mode NORMAL */
	{importfile,     "import",  "ps"},	/* mode IMPORT */
	{loadbackground, "render",  "ps"},	/* mode PSBKGROUND */
#ifdef HAVE_PYTHON
	{execscript,	"execute",  "py"},
#else
	{execscript,	"execute",    ""},	/* mode SCRIPT */
#endif
	{crashrecover,	"recover",  "ps"},	/* mode RECOVER */
#ifdef ASG
	{importspice,	"import", "spice"},	/* mode IMPORTSPICE */
#endif
   };

   char *promptstr = NULL;
   int idx = (intptr_t)mode;

   if (is_page(topobject) == -1) {
      Wprintf("Can only read file into top-level page!");
      return;
   }
   else if (idx >= LOAD_MODES) {
      Wprintf("Unknown mode passed to routine getfile()\n");
      return;
   }
   if (idx == RECOVER) {
      char *cfile = getcrashfilename(*(QString*)crashfn);
      promptstr = (char *)malloc(18 + ((cfile == NULL) ? 9 : strlen(cfile)));
      sprintf(promptstr, "Recover file \'%s\'?", (cfile == NULL) ? "(unknown)" : cfile);
      popupYesNo(a, promptstr, loadmodes[idx].func, mode);
      if (cfile) free(cfile);
   }
   else {
      promptstr = (char *)malloc(18 + strlen(loadmodes[idx].prompt));
      sprintf(promptstr, "Select file to %s:", loadmodes[idx].prompt);
      popupFile(a, promptstr, "\0", loadmodes[idx].filext, loadmodes[idx].func, mode);
   }
   free(promptstr);
}

/*--------------------------------------------------------------*/
/* Tilde ('~') expansion in file name.  */
/*--------------------------------------------------------------*/

bool xc_tilde_expand(QString & filename)
{
    QString homepath;

    if (! filename.startsWith("~")) return false;

    if (filename.isEmpty() | filename[0] == QDir::separator() || filename[0] == ' ') {
        // current user
        homepath = QDir::homePath();
        if (! homepath.isEmpty()) {
            filename.replace(0, 1, homepath + QDir::separator());
            return true;
        }
    }
    else {
        // some other user
        /// \todo add a Windows implementation
#ifndef _MSC_VER
        struct passwd *passwd;

        int len = filename.indexOf(QDir::separator());
        len == (len == -1) ? filename.length() : len - 1;

        QString username = filename.mid(1, len-1);

        passwd = getpwnam(username.toLocal8Bit());
        if (passwd) {
            homepath = QString::fromLocal8Bit(passwd->pw_dir);
            filename.replace(0, len+1, homepath);
        }
        return true;
#endif
    }
    return false;
} 

/*--------------------------------------------------------------*/
/* Variable ('$') expansion in file name 			*/
/*--------------------------------------------------------------*/

bool xc_variable_expand(QString & filename)
{
   /// \todo
#if 0
   char *expanded, *sptr, tmpchar, *varpos, *varsub;

   if ((varpos = strchr(filename, '$')) != NULL) {
      for (sptr = varpos; *sptr != '/' && *sptr != '\0'; sptr++) ;
      if (*sptr == '\0') *(sptr + 1) = '\0';
      tmpchar = *sptr;
      *sptr = '\0';

#ifdef TCL_WRAPPER
      /* Interpret as a Tcl variable */
      varsub = (char *)Tcl_GetVar(xcinterp, varpos + 1, TCL_NAMESPACE_ONLY);
#else
      /* Interpret as an environment variable */
      varsub = (char *)getenv((const char *)(varpos + 1));
#endif

      if (varsub != NULL) {

	 *varpos = '\0';
	 expanded = (char *)malloc(strlen(varsub) + strlen(filename) + 
		strlen(sptr + 1) + 2);
	 strcpy(expanded, filename);
         strcat(expanded, varsub);
	 *sptr = tmpchar;
	 strcat(expanded, sptr);
	 strncpy(filename, expanded, nchars);
         free(expanded);
      }
      else
         *sptr = tmpchar;
      return true;
   }
#endif
   return false;
} 

/*--------------------------------------------------------------*/
/* Attempt to find a file and open it.				*/
/*--------------------------------------------------------------*/

static FILE *common_open(const QString &filename, const char *suffix, const QStringList & path, QString * name_return)
{
    FILE *file = NULL;
    QString expname = filename;
    QString inname;
    int pathidx;

    xc_tilde_expand(expname);
    while (xc_variable_expand(expname)) ;

    pathidx = 0;
    while (1) {
        if (pathidx >= path.count() || expname[0] == QDir::separator()) {
            inname = expname;
        }
        else {
            inname = path[pathidx];
            if (! inname.endsWith(QDir::separator())) inname += QDir::separator();
            inname += expname;
        }

        /* Attempt to open the filename with a suffix */
        QFileInfo fileinfo(inname);
        QString savedname = inname;
        if (! fileinfo.fileName().contains('.') && suffix) {
            if (suffix[0] != '.') inname += '.';
            inname += suffix;
            file = fopen(inname.toLocal8Bit(), "r");
        }

        /* Attempt to open the filename as given, without a suffix */
        if (! file) {
            inname = savedname;
            file = fopen(inname.toLocal8Bit(), "r");
        }

        ++ pathidx;
        if (file || pathidx > path.count()) break;
   }

   if (name_return) *name_return = inname;
   return file;
}

FILE *fileopen(const QString &filename, const char *suffix, QString *name_return)
{
    return common_open(filename, suffix, xobjs.filesearchpath, name_return);
}

/*------------------------------------------------------*/
/* Open a library file by name and return a pointer to	*/
/* the file structure, or NULL if an error occurred.	*/
/*------------------------------------------------------*/

FILE *libopen(const QString &libname, short mode, QString *name_return)
{
   FILE *file = NULL;
   const char *suffix = (mode == FONTENCODING) ? ".xfe" : ".lps";

   file = common_open(libname, suffix, xobjs.libsearchpath, name_return);

   if (!file && xobjs.libsearchpath.isEmpty()) {
      QProcessEnvironment env;
      /* if not found in cwd and there is no library search	  */
      /* path, look for environment variable "XCIRCUIT_LIB_DIR"	  */
      /* defined (Thanks to Ali Moini, U. Adelaide, S. Australia) */

      const QString libdir = env.value("XCIRCUIT_LIB_DIR");
      if (!libdir.isEmpty()) file = common_open(libname, suffix, QStringList(libdir), name_return);

      /* last resort:  hard-coded directory BUILTINS_DIR */
      if (!file) {
          file = common_open(libname, suffix, QStringList(BUILTINS_DIR), name_return);
      }
   }
   return file;
}

/*---------------------------------------------------------*/

#if 0
bool nextfilename()	/* extract next filename from _STR2 into _STR */
{
   char *cptr, *slptr;

   sprintf(_STR, "%.149s", _STR2);
   if ((cptr = strrchr(_STR2, ',')) != NULL) {
      slptr = strrchr(_STR, '/');
      if (slptr == NULL || ((slptr - _STR) > (cptr - _STR2))) slptr = _STR - 1;
      sprintf(slptr + 1, "%s", cptr + 1); 
      *cptr = '\0';
      return true;
   }
   else return false;
}
#endif

/*------------------------------------------------------*/
/* Handle library loading and refresh current page if 	*/
/* it is a library page that just changed.		*/
/*------------------------------------------------------*/

void loadglib(bool lflag, short ilib, short tlib, const QString & filestr)
{
   QStringList files = filestr.split(',', QString::SkipEmptyParts);
   foreach (QString file, files) {
      if (lflag)
	 lflag = false;
      else
         ilib = createlibrary(false);
      loadlibrary(ilib, file);
      /* if (ilib == tlib) zoomview(NULL, NULL, NULL); */
   }
}

/*------------------------------------------------------*/
/* Load new library:  Create new library page and load	*/
/* to it.						*/
/*------------------------------------------------------*/

void loadulib(QAction*, const QString& str, void*)
{
   loadglib(false, (short)0, (short)is_library(topobject) + LIBRARY, str);
}

/*-----------------------------------------------------------*/
/* Add to library:  If on library page, add to that library. */
/* Otherwise, create a new library page and load to it.	     */
/*-----------------------------------------------------------*/

void loadblib(QAction*, const QString& str, void*)
{
   short ilib, tlib;
   bool lflag = true;

   /* Flag whether current page is a library page or not */

   if ((tlib = is_library(topobject)) < 0) {
      ilib = LIBRARY;
      lflag = false;
   }
   else
      ilib = tlib + LIBRARY;

   loadglib(lflag, ilib, tlib + LIBRARY, str);
}

/*---------------------------------------------------------*/

void getlib(QAction* a, pointertype, pointertype)
{
   popupFile(a, "Enter library to load:", "\0", "lps", loadblib);
}

/*---------------------------------------------------------*/

void getuserlib(QAction* a, pointertype, pointertype)
{
   popupFile(a, "Enter library to load:", "\0", "lps", loadulib);
}

/*------------------------------------------------------*/
/* Add a new name to the list of aliases for an object	*/
/*------------------------------------------------------*/

bool addalias(objectptr thisobj, char *newname)
{
   aliasptr aref;
   slistptr sref;
   char *origname = thisobj->name;

   for (aref = aliastop; aref != NULL; aref = aref->next)
      if (aref->baseobj == thisobj)
	 break;

   /* An equivalence, not an alias */
   if (!strcmp(origname, newname)) return true;

   if (aref == NULL) {	/* entry does not exist;  add new baseobj */
      aref = new alias;
      aref->baseobj = thisobj;
      aref->aliases = NULL;
      aref->next = aliastop;
      aliastop = aref;
   }

   for (sref = aref->aliases; sref != NULL; sref = sref->next)
      if (!strcmp(sref->alias, newname))
	 break;

   if (sref == NULL) {		/* needs new entry */
      sref = new stringlist;
      sref->alias = strdup(newname);
      sref->next = aref->aliases;
      aref->aliases = sref;
      return false;
   }
   else return true;		/* alias already exists! */
}

/*------------------------------------------------------*/
/* Remove all object name aliases			*/
/*------------------------------------------------------*/

void cleanupaliases(short mode)
{
   aliasptr aref;
   slistptr sref;
   objectptr baseobj;
   char *sptr;
   int i, j;

   if (aliastop == NULL) return;

   for (aref = aliastop; aref != NULL; aref = aref->next) {
      baseobj = aref->baseobj;
      for (sref = aref->aliases; sref != NULL; sref = sref->next)
	 free(sref->alias);
   }

   for (; (aref = aliastop->next); aliastop = aref)
      delete aliastop;
   delete aliastop;
   aliastop = NULL;

   /* Get rid of propagating underscores in names */

   for (i = 0; i < ((mode == FONTLIB) ? 1 : xobjs.numlibs); i++) {
      for (j = 0; j < ((mode == FONTLIB) ? xobjs.fontlib.number :
		xobjs.userlibs[i].number); j++) {
	 baseobj = (mode == FONTLIB) ? *(xobjs.fontlib.library + j) :
		*(xobjs.userlibs[i].library + j);

	 sptr = baseobj->name;
	 while (*sptr == '_') sptr++;
	 /* need memmove to avoid overwriting? */
	 memmove((void *)baseobj->name, (const void *)sptr, strlen(sptr) + 1);
	 checkname(baseobj);
      }
   }
}

/*--------------------------------------------------------------*/
/* Add a record to the instlist list of the library indexed by  */
/* mode, create a new instance, and add it to the record.	*/
/*								*/
/* objname is the name of the library object to be instanced,	*/
/* and buffer is the line containing the instance parameters	*/
/* of the new instance, optionally preceded by scale and	*/
/* rotation values.						*/
/*								*/
/*--------------------------------------------------------------*/

objinstptr new_library_instance(short mode, char *objname, char *buffer,
	TechPtr defaulttech)
{
   char *lineptr;
   objectptr libobj, localdata;
   objinstptr newobjinst;
   int j;
   char *nsptr, *fullname = objname;

   localdata = xobjs.libtop[mode + LIBRARY]->thisobject;

   /* For (older) libraries that do not use technologies, give the      */
   /* object a technology name in the form <library>::<object>          */
   
   if ((nsptr = strstr(objname, "::")) == NULL) {
      int deftechlen = (defaulttech == NULL) ? 0 : strlen(defaulttech->technology);
      fullname = (char *)malloc(deftechlen + strlen(objname) + 3);
      if (defaulttech == NULL)
         sprintf(fullname, "::%s", objname);
      else
         sprintf(fullname, "%s::%s", defaulttech->technology, objname);
   }

   for (j = 0; j < xobjs.userlibs[mode].number; j++) {
      libobj = *(xobjs.userlibs[mode].library + j);
      if (!strcmp(fullname, libobj->name)) {
         newobjinst = addtoinstlist(mode, libobj, true);

	 lineptr = buffer;
	 while (isspace(*lineptr)) lineptr++;
	 if (*lineptr != '<') {
	    /* May declare instanced scale and rotation first */
	    lineptr = varfscan(localdata, lineptr, &newobjinst->scale,
                        newobjinst, P_SCALE);
	    lineptr = varscan(localdata, lineptr, &newobjinst->rotation,
                        newobjinst, P_ROTATION);
	 }
	 readparams(NULL, newobjinst, libobj, lineptr);
	 if (fullname != objname) free(fullname);
	 return newobjinst;
      }
   }
   if (fullname != objname) free(fullname);
   return NULL;		/* error finding the library object */
}

/*------------------------------------------------------*/
/* Import a single object from a library file.  "mode"	*/
/* is the number of the library into which the object	*/
/* will be placed.  This function allows new libraries	*/
/* to be generated by "cutting and pasting" from	*/
/* existing libraries.  It also allows better library	*/
/* management when the number of objects becomes very	*/
/* large, such as when using "diplib" (7400 series	*/
/* chips, created from the PCB library).		*/ 	
/*------------------------------------------------------*/

void importfromlibrary(short mode, char *libname, char *objname)
{
   FILE *ps;
   char temp[150], keyword[100];
   QString inname;
   char *tptr;
   objectptr *newobject;
   objlistptr redef;
   float saveversion;
   bool dependencies = false;
   TechPtr nsptr = NULL;
   
   ps = libopen(libname, mode, &inname);
   if (ps == NULL) {
      Fprintf(stderr, "Cannot open library %s for import.\n", libname);
      return;
   }

   version = 2.0;   /* Assume version is 2.0 unless found in header */

   for(;;) {
      if (fgets(temp, 149, ps) == NULL) {
         Wprintf("Error in library.");
	 goto endload;
      }
      else if (temp[0] == '/') {
	 int s = 1;
	 if (temp[1] == '@') s = 2;
	 sscanf(&temp[s], "%s", keyword);
	 if (!strcmp(keyword, objname))
	    break;
      }
      else if (*temp == '%') {
	 char *tptr = temp + 1;
	 while (isspace(*tptr)) tptr++;
	 if (!strncmp(tptr, "Version:", 8)) {
	    float tmpv;
	    tptr += 9;
	    if (sscanf(tptr, "%f", &tmpv) > 0) version = tmpv;
	 }
	 else if (!strncmp(tptr, "Library", 7)) {
	    char *techname = strstr(tptr, ":");
	    if (techname != NULL) {
	       techname++;	/* skip over colon */
	       while(isspace(*techname)) techname++;
	       ridnewline(techname);	/* Remove newline character */
	       if ((tptr = strrchr(techname, '/')) != NULL)
		  techname = tptr + 1;
	       if ((tptr = strrchr(techname, '.')) != NULL)
		  if (!strncmp(tptr, ".lps", 4))
		     *tptr = '\0';
               nsptr = AddNewTechnology(techname, inname.toLocal8Bit().data());
	    }
	 }
         else if (!strncmp(tptr, "Depend", 6)) {
            dependencies = true;
	    tptr += 7;
	    sscanf(tptr, "%s", keyword);
	    if (!strcmp(keyword, objname)) {
	       /* Load dependencies */
	       while (1) {
	          tptr += strlen(keyword) + 1;
	          if (sscanf(tptr, "%s", keyword) != 1) break;
	          if (keyword[0] == '\n' || keyword[0] == '\0') break;
	          /* Recursive import */
	          saveversion = version;
	          importfromlibrary(mode, libname, keyword);
	          version = saveversion;
	       }
	    }
	 }
      }
   }

   if ((version < 3.2) && (!dependencies)) {
      Fprintf(stderr, "Library does not have dependency list and cannot "
		"be trusted.\nLoad and rewrite library to update.\n");
      goto endload;
   }

   newobject = new_library_object(mode, keyword, &redef, nsptr);

   load_in_progress = true;
   if (! objectread(ps, *newobject, 0, 0, mode, temp, DEFAULTCOLOR, nsptr)) {

      if (library_object_unique(mode, *newobject, redef)) {
	 add_object_to_library(mode, *newobject);
	 cleanupaliases(mode);

	 /* pull in any instances of this object that	*/
	 /* are defined in the library			*/

	 for(;;) {
	    if (fgets(temp, 149, ps) == NULL) {
	       Wprintf("Error in library.");
	       goto endload;
	    }
	    else if (!strncmp(temp, "% EndLib", 8))
	       break;
	    else if (strstr(temp, "libinst") != NULL) {
	       if ((tptr = strstr(temp, objname)) != NULL) {
	          if (*(tptr - 1) == '/') {
		     char *eptr = tptr;
                     while (!isspace(*++eptr)) ;
		     *eptr = '\0';
		     new_library_instance(mode - LIBRARY, tptr, temp, nsptr);
		  }
	       }
	    }
	 }

	 if (mode != FONTLIB) {
	    composelib(mode);
	    centerview(xobjs.libtop[mode]);
	 }
      }
   }

endload:
   fclose(ps);
   version = PROG_VERSION;
   load_in_progress = false;
}

/*------------------------------------------------------*/
/* Copy all technologies' replace flags into temp flag.	*/
/* Then clear the replace flags (replace none)		*/
/*------------------------------------------------------*/

void TechReplaceSave()
{
   TechPtr nsp;

   for (nsp = xobjs.technologies; nsp != NULL; nsp = nsp->next)
   {
      if (nsp->flags & LIBRARY_REPLACE)
	 nsp->flags |= LIBRARY_REPLACE_TEMP;
      else
         nsp->flags &= ~LIBRARY_REPLACE_TEMP;
      nsp->flags &= ~LIBRARY_REPLACE;
   }
}

/*------------------------------------------------------*/
/* Restore all technologies' replace flags 		*/
/*------------------------------------------------------*/

void TechReplaceRestore()
{
   TechPtr nsp;

   for (nsp = xobjs.technologies; nsp != NULL; nsp = nsp->next)
   {
      if (nsp->flags & LIBRARY_REPLACE_TEMP)
	 nsp->flags |= LIBRARY_REPLACE;
      else
         nsp->flags &= ~LIBRARY_REPLACE;
   }
}

/*------------------------------------------------------*/
/* Set all technologies' replace flags (replace all)	*/
/*------------------------------------------------------*/

void TechReplaceAll()
{
   TechPtr nsp;

   for (nsp = xobjs.technologies; nsp != NULL; nsp = nsp->next)
      nsp->flags |= LIBRARY_REPLACE;
}

/*------------------------------------------------------*/
/* Clear all technologies' replace flags (replace none)	*/
/*------------------------------------------------------*/

void TechReplaceNone()
{
   TechPtr nsp;

   for (nsp = xobjs.technologies; nsp != NULL; nsp = nsp->next)
      nsp->flags &= ~LIBRARY_REPLACE;
}


/*------------------------------------------------------*/
/* Compare an object's name with a specific technology	*/
/*------------------------------------------------------*/

bool CompareTechnology(objectptr cobj, char *technology)
{
   char *cptr;
   bool result = false;

   if ((cptr = strstr(cobj->name, "::")) != NULL) {
      if (technology == NULL) result = false;
      else {
         *cptr = '\0';
         if (!strcmp(cobj->name, technology)) result = true;
         *cptr = ':';
      }
   }
   else if (technology == NULL)
      result = true;

   return result;
}

/*------------------------------------------------------*/
/* Find a technology record				*/
/*------------------------------------------------------*/

TechPtr LookupTechnology(char *technology)
{
   TechPtr nsp;

   if (technology == NULL) return NULL;

   for (nsp = xobjs.technologies; nsp != NULL; nsp = nsp->next)
      if (!strcmp(technology, nsp->technology))
	 return nsp;

   return NULL;
}

/*------------------------------------------------------*/
/* Find a technology according to the filename		*/
/*------------------------------------------------------*/

TechPtr GetFilenameTechnology(char *filename)
{
   TechPtr nsp;

   if (filename == NULL) return NULL;

   for (nsp = xobjs.technologies; nsp != NULL; nsp = nsp->next)
      if (!filecmp(filename, nsp->filename))
	 return nsp;

   return NULL;
}

/*------------------------------------------------------*/
/* Find a technology record corresponding to the	*/
/* indicated object's technology.			*/
/*------------------------------------------------------*/

TechPtr GetObjectTechnology(objectptr thisobj)
{
   TechPtr nsp;
   char *cptr;

   cptr = strstr(thisobj->name, "::");
   if (cptr == NULL) return NULL;
   else *cptr = '\0';

   for (nsp = xobjs.technologies; nsp != NULL; nsp = nsp->next)
      if (!strcmp(thisobj->name, nsp->technology))
	 break;

   *cptr = ':';
   return nsp;
}

/*------------------------------------------------------*/
/* Add a new technology name to the list		*/
/*------------------------------------------------------*/

TechPtr AddNewTechnology(char *technology, char *filename)
{
   TechPtr nsp;

   if (technology == NULL) return NULL;

   for (nsp = xobjs.technologies; nsp != NULL; nsp = nsp->next) {
      if (!strcmp(technology, nsp->technology)) {

	 /* A namespace may be created for an object that is a dependency */
	 /* in a different technology.  If so, it will have a NULL	  */
	 /* filename, and the filename should be replaced if we ever load */
	 /* the file that properly defines the technology.		  */

	 if ((nsp->filename == NULL) && (filename != NULL)) 
	    nsp->filename = strdup(filename);

	 return nsp;	/* Namespace already exists */
      }
   }

   nsp = new Technology;
   nsp->next = xobjs.technologies;
   if (filename == NULL)
      nsp->filename = NULL;
   else
      nsp->filename = strdup(filename);
   nsp->technology = strdup(technology);
   nsp->flags = (u_char)0;
   xobjs.technologies = nsp;

   return nsp;
}

/*------------------------------------------------------*/
/* Check an object's name for a technology, and add it	*/
/* to the list of technologies if it has one.		*/
/*------------------------------------------------------*/

void AddObjectTechnology(objectptr thisobj)
{
   char *cptr;

   cptr = strstr(thisobj->name, "::");
   if (cptr != NULL) {
      *cptr = '\0';
      AddNewTechnology(thisobj->name, NULL);
      *cptr = ':';
   }
}

/*------------------------------------------------------*/
/* Load a library page (given in parameter "mode") and	*/
/* rename the library page to match the library name as */
/* found in the file header.				*/
/*------------------------------------------------------*/

bool loadlibrary(short mode, const QString & filename)
{
   FILE *ps;
   objinstptr saveinst;
   QString inname;
   char temp[150], keyword[30], percentc;
   TechPtr nsptr = NULL;

   ps = libopen(filename, mode, &inname);

   if ((ps == NULL) && (mode == FONTLIB)) {
      /* We automatically try looking in all the usual places plus a	*/
      /* subdirectory named "fonts".					*/

      sprintf(temp, "fonts/%s", filename.toLocal8Bit().data());
      ps = libopen(temp, mode, &inname);
   }
   if (ps == NULL) {
      Wprintf("Library not found.");
      return false;
   }

   /* current version is PROG_VERSION;  however, all libraries newer than */
   /* version 2.0 require "Version" in the header.  So unnumbered	  */
   /* libraries may be assumed to be version 1.9 or earlier.		  */

   version = 1.9;
   for(;;) {
      if (fgets(temp, 149, ps) == NULL) {
         Wprintf("Error in library.");
	 fclose(ps);
         return false;
      }
      sscanf(temp, "%c %29s", &percentc, keyword);

      /* Commands in header are PostScript comments (%) */
      if (percentc == '%') {

	 /* The library name in the header corresponds to the object	*/
	 /* technology defined by the library.  This no longer has 	*/
	 /* anything to do with the name of the library page where we	*/
	 /* are loading this file.					*/

	 /* Save the technology, filename, and flags in the technology list. */

         if ((mode != FONTLIB) && !strcmp(keyword, "Library")) {
	    char *cptr, *nptr;
	    cptr = strchr(temp, ':');
	    if (cptr != NULL) {
	       cptr += 2;

	       /* Don't write terminating newline to the object's name string */
	       ridnewline(cptr);

	       /* Removing any leading pathname from the library name */
	       if ((nptr = strrchr(cptr, '/')) != NULL) cptr = nptr + 1;

	       /* Remove any ".lps" extension from the library name */

	       nptr = strrchr(cptr, '.');
	       if ((nptr != NULL) && !strcmp(nptr, ".lps")) *nptr = '\0';

               nsptr = AddNewTechnology(cptr, inname.toLocal8Bit().data());
	    }
         }

         /* This comment gives the Xcircuit version number */
	 else if (!strcmp(keyword, "Version:")) {
	    float tmpv;
	    if (sscanf(temp, "%*c %*s %f", &tmpv) > 0) version = tmpv;
	 }

         /* This PostScript comment marks the end of the file header */
         else if (!strcmp(keyword, "XCircuitLib")) break;
      }
   }

   /* Set the current top object to the library page so that any	*/
   /* expression parameters are computed with respect to the library,	*/
   /* not a page.  Revert back to the page after loading the library.	*/

   saveinst = areawin->topinstance;
   areawin->topinstance = xobjs.libtop[mode];

   load_in_progress = true;
   objectread(ps, topobject, 0, 0, mode, temp, DEFAULTCOLOR, nsptr);
   load_in_progress = false;
   cleanupaliases(mode);

   areawin->topinstance = saveinst;

   if (mode != FONTLIB) {
      composelib(mode);
      centerview(xobjs.libtop[mode]);
      if (nsptr == NULL) nsptr = GetFilenameTechnology(inname.toLocal8Bit().data());
      if (nsptr != NULL)
         Wprintf("Loaded library file %ls", inname.utf16());
      else
         Wprintf("Loaded library file %ls (technology %s)", inname.utf16(),
		nsptr->technology);
   }
   else
        Wprintf("Loaded font file %ls", inname.utf16());

   version = PROG_VERSION;
   fclose(ps);

   /* Check if the library is read-only by opening for append */

   if ((mode != FONTLIB) && (nsptr != NULL)) {
      ps = fopen(inname.toLocal8Bit(), "a");
      if (ps == NULL)
         nsptr->flags |= LIBRARY_READONLY;
      else
         fclose(ps);
   }

   return true;
}

/*---------------------------------------------------------*/

void startloadfile(int libnum, const QString & filelist)
{
   QStringList files = filelist.split(',', QString::SkipEmptyParts);
   short firstpage = areawin->page;

   foreach (QString file, files) {
      loadfile(0, libnum, file);

      /* find next undefined page */

      while(areawin->page < xobjs.pages &&
           xobjs.pagelist[areawin->page].pageinst != NULL) areawin->page++;
      changepage(areawin->page);
   }

   /* Display the first page loaded */
   newpage(firstpage);

   setsymschem();
}

/*------------------------------------------------------*/
/* normalloadfile() is a call to startloadfile(-1)	*/
/* meaning load symbols to the User Library 		*/
/*------------------------------------------------------*/

void normalloadfile(QAction*, const QString& str, void*)
{
   startloadfile(-1, str);
}

/*------------------------------------------------------*/
/* Import an xcircuit file onto the current page	*/
/*------------------------------------------------------*/

void importfile(QAction*, const QString& filelist, void*)
{
   QStringList files = filelist.split(',', QString::SkipEmptyParts);
   foreach (QString file, files) loadfile(1, -1, file);
}

/*--------------------------------------------------------------*/
/* Skip forward in the input file to the next comment line	*/
/*--------------------------------------------------------------*/

void skiptocomment(char *temp, int length, FILE *ps)
{
   int pch;

   do {
      pch = getc(ps);
   } while (pch == '\n');

   ungetc(pch, ps);
   if (pch == '%') fgets(temp, length, ps);
}

/*--------------------------------------------------------------*/
/* ASG file import functions:					*/
/* This function loads a SPICE deck (hspice format)		*/
/*--------------------------------------------------------------*/

#ifdef ASG

void importspice(QAction*, void*, void*)
{
   char inname[250];
   FILE *spcfile;

   if (eventmode == CATALOG_MODE) {
      Wprintf("Cannot import a netlist while in the library window.");
      return;
   }

   if (!nextfilename()) {
      xc_tilde_expand(_STR, 149);
      sscanf(_STR, "%149s", inname);
      spcfile = fopen(inname, "r");
      ReadSpice(spcfile);
      Route(areawin, false);
      fclose(spcfile);
   }
   else {
      Wprintf("Error:  No spice file to read.");
      return;
   }
}

#endif

/*--------------------------------------------------------------*/
/* Load an xcircuit file into xcircuit				*/
/*								*/
/*    mode = 0 is a "load" function.  Behavior:  load on 	*/
/*	current page if empty.  Otherwise, load on first empty	*/
/*	page found.  If no empty pages exist, create a new page	*/
/*	and load there.						*/
/*    mode = 1 is an "import" function.  Behavior:  add		*/
/*	contents of file to the current page.			*/
/*    mode = 2 is a "library load" function.  Behavior:  add	*/
/*	objects in file to the user library but ignore the	*/
/*	page contents.						*/
/*								*/
/* Return value:  true if file was successfully loaded, false	*/
/*	if not.							*/
/*--------------------------------------------------------------*/

struct connects {
   short page;
   char *master;
   connects* next;
   inline ~connects() { free(master); }
};
typedef connects *connectptr;

bool loadfile(short mode, int libnum, const QString & filename)
{
   FILE *ps;
   QString inname;
   char temp[150], keyword[30], percentc, *pdchar;
   char teststr[50], teststr2[20], pagestr[100];
   short offx, offy, multipage, page, temppmode = 0;
   float tmpfl;
   XPoint pagesize;
   connects *connlist = NULL;
   struct stat statbuf;
   int loclibnum = (libnum == -1) ? USERLIB : libnum;
   QByteArray str = filename.toLocal8Bit();

   /* First, if we're in catalog mode, return with error */

   if (eventmode == CATALOG_MODE) {
      Wprintf("Cannot load file from library window");
      return false;
   }

   /* Do tilde/variable expansions on filename and open */
   ps = fileopen(str, "ps", &inname);

   /* Could possibly be a library file?				*/
   /* (Note---loadfile() has no problems loading libraries	*/
   /* except for setting technology names and setting the	*/
   /* library page view at the end.  The loadlibrary() routine	*/
   /* should probably be merged into this one.)			*/

   if (ps == NULL) {
      ps = fileopen(str, "lps");
      if (ps != NULL) {
	 fclose(ps);
         loadlibrary(loclibnum, str);
	 return true;
      }
   }
   else if (inname.endsWith(".lps")) {
      fclose(ps);
      loadlibrary(loclibnum, str);
      return true;
   }
   

#ifdef LGF
   /* Could possibly be an LGF file? */
   if (ps == NULL) {
      ps = fileopen(str, "lgf");
      if (ps != NULL) {
	 fclose(ps);
	 loadlgf(mode);
	 return true;
      }
   }

   /* Could possibly be an LGF backup (.lfo) file? */
   if (ps == NULL) {
      ps = fileopen(str, "lfo");
      if (ps != NULL) {
	 fclose(ps);
	 loadlgf(mode);
	 return true;
      }
   }
#endif /* LGF */

   /* Check for empty file---don't attempt to read empty files */
   if (ps != NULL) {
      if (fstat(fileno(ps), &statbuf) == 0 && (statbuf.st_size == (off_t)0)) {
         fclose(ps);
	 ps = NULL;
      }
   }

   /* What to do if no file was found. . . */

   if (ps == NULL) {
      if (topobject->parts == 0 && (mode == 0)) {

         /* Check for file extension, and remove if "ps". */

         if ((pdchar = strchr(str.data(), '.')) != NULL)
	    if (!strcmp(pdchar + 1, "ps")) *pdchar = '\0';

         xobjs.pagelist[areawin->page].filename = str;

         /* If the name has a path component, use only the root		*/
         /* for the object name, but the full path for the filename.	*/

         if ((pdchar = strrchr(str.data(), '/')) != NULL)
            sprintf(topobject->name, "%s", pdchar + 1);
         else
            sprintf(topobject->name, "%s", str.data());

         renamepage(areawin->page);
         printname(topobject);
         Wprintf("Starting new drawing");
      }
      else {
         Wprintf("Can't find file %s, won't overwrite current page", str.data());
      }
      return false;
   }

   version = 1.0;
   multipage = 1;
   pagesize.x = 612;
   pagesize.y = 792;

   for(;;) {
      if (fgets(temp, 149, ps) == NULL) {
	 Wprintf("Error: EOF in or before prolog.");
	 return false;
      }
      sscanf(temp, "%c%29s", &percentc, keyword);
      for (pdchar = keyword; isspace(*pdchar); pdchar++) ;
      if (percentc == '%') {
	 if (!strcmp(pdchar, "XCircuit")) break;
	 if (!strcmp(pdchar, "XCircuitLib")) {
	    /* version control in libraries is post version 1.9 */
	    if (version == 1.0) version = 1.9;
	    break;
	 }
	 if (!strcmp(pdchar, "%Page:")) break;
	 if (strstr(pdchar, "PS-Adobe") != NULL)
	    temppmode = (strstr(temp, "EPSF") != NULL) ? 0 : 1;
         else if (!strcmp(pdchar, "Version:"))
	    sscanf(temp, "%*c%*s %f", &version);
	 else if (!strcmp(pdchar, "%Pages:")) {
	    pdchar = advancetoken(temp);
	    multipage = atoi(pdchar);
	 }
	 /* Crash files get renamed back to their original filename */
	 else if (!strcmp(pdchar, "%Title:")) {
	    if (xobjs.tempfile != NULL)
                if (inname == xobjs.tempfile) {
                  inname = QString(temp).split(' ')[temp[0] == ' ' ? 2 : 1];
                  //sscanf(temp, "%*c%*s %s", inname);
                }
	 }
	 else if ((temppmode == 1) && !strcmp(pdchar, "%BoundingBox:")) {
	    short botx, boty;
	    sscanf(temp, "%*s %hd %hd %hd %hd", &botx, &boty,
		&(pagesize.x), &(pagesize.y));
	    pagesize.x += botx;
	    pagesize.y += boty;
	 }
	
      }
#ifdef LGF
      else if (percentc == '-' && !strcmp(keyword, "5")) {
	 fclose(ps);
	 loadlgf(mode);
	 return true;
      }
#endif
   }

   /* Look for old-style files (no %%Page; maximum one page in file) */

   if (!strcmp(pdchar, "XCircuit"))
      skiptocomment(temp, 149, ps);

   for (page = 0; page < multipage; page++) {
      sprintf(pagestr, "%d", page + 1);

      /* read out-of-page library definitions */

      if (strstr(temp, "%%Page:") == NULL && strstr(temp, "offsets") == NULL) {
         load_in_progress = true;
	 objectread(ps, topobject, 0, 0, loclibnum, temp, DEFAULTCOLOR, NULL);
         load_in_progress = false;
      }

      if (strstr(temp, "%%Page:") != NULL) {
	 sscanf(temp + 8, "%99s", pagestr);

	 /* Read the next line so any keywords in the Page name don't	*/
	 /* confuse the parser.						*/
	 if (fgets(temp, 149, ps) == NULL) {
            Wprintf("Error: bad page header.");
            return false;
	 }

	 /* Library load mode:  Ignore all pages, just load objects */
	 if (mode == 2) {
	    while (strstr(temp, "showpage") == NULL) {
	       if (fgets(temp, 149, ps) == NULL) {
		  Wprintf("Error: bad page definition.");
		  return false;
	       }
	    }
	    skiptocomment(temp, 149, ps);
	    continue;
	 }
      }

      /* go to new page if necessary */

      if (page > 0) {

	 /* find next undefined page */

	 while(areawin->page < xobjs.pages &&
                xobjs.pagelist[areawin->page].pageinst != NULL) areawin->page++;
	 changepage(areawin->page);
      }

      /* If this file was a library file then there is no page to load */

      if (strstr(temp, "EndLib") != NULL) {
   	 composelib(loclibnum);
	 centerview(xobjs.libtop[mode]);
	 Wprintf("Loaded library.");
	 return true;
      }

      /* good so far;  let's clear out the old data structure */

      if (mode == 0) {
         topobject->clear();
         pagereset(areawin->page);
         xobjs.pagelist[areawin->page].pmode = temppmode;
	 if (temppmode == 1) {
            xobjs.pagelist[areawin->page].pagesize = pagesize;
	 }
      }
      else {
	 invalidate_netlist(topobject);
	 /* ensure that the netlist for topobject is destroyed */
	 freenetlist(topobject);
      }

      /* clear the undo record */
      flush_undo_stack();

      /* read to the "scale" line, picking up inch/cm type, drawing */
      /* scale, and grid/snapspace along the way		    */

      offx = offy = 0;
      for(;;) {
         if (strstr(temp, "offsets") != NULL) {
	    /* Prior to version 3.1.28 only. . . */
            sscanf(temp, "%c %hd %hd %*s", &percentc, &offx, &offy);
            if(percentc != '%') {
               Wprintf("Something wrong in offsets line.");
               offx = offy = 0;
            }
	 }

	 if ((temppmode == 1) && strstr(temp, "%%PageBoundingBox:") != NULL) {
	    /* Recast the individual page size if specified per page */
	    sscanf(temp, "%*s %*d %*d %hd %hd",
                &xobjs.pagelist[areawin->page].pagesize.x,
                &xobjs.pagelist[areawin->page].pagesize.y);
	 }
	 else if (strstr(temp, "drawingscale") != NULL)
	    sscanf(temp, "%*c %hd:%hd %*s",
                &xobjs.pagelist[areawin->page].drawingscale.x,
                &xobjs.pagelist[areawin->page].drawingscale.y);

	 else if (strstr(temp, "hidden") != NULL)
	    topobject->hidden = true;

	 else if (strstr(temp, "is_symbol") != NULL) {
	    sscanf(temp, "%*c %49s", teststr);
	    checkschem(topobject, teststr);
	 }
         else if (strstr(temp, "is_primary") != NULL) {
	    connects *newconn;

	    /* Save information about master schematic and link at end of load */
	    sscanf(temp, "%*c %49s", teststr);
            newconn = new connects;
	    newconn->next = connlist;
	    connlist = newconn;
	    newconn->page = areawin->page;
	    newconn->master = strdup(teststr);
         }  

	 else if (strstr(temp, "gridspace"))
            sscanf(temp, "%*c %f %f %*s", &xobjs.pagelist[areawin->page].gridspace,
                 &xobjs.pagelist[areawin->page].snapspace);
         else if (strstr(temp, "scale") != NULL || strstr(temp, "rotate") != NULL) {
            /* rotation (landscape mode) is optional; parse accordingly */

            sscanf(temp, "%f %49s", &tmpfl, teststr);
            if (strstr(teststr, "scale") != NULL) {
	       setgridtype(teststr);
               xobjs.pagelist[areawin->page].outscale = tmpfl;
            }
            else if (!strcmp(teststr, "rotate")) {
               xobjs.pagelist[areawin->page].orient = (short)tmpfl;
               fgets(temp, 149, ps);
               sscanf(temp, "%f %19s", &tmpfl, teststr2);
	       if (strstr(teststr2, "scale") != NULL) {
	          setgridtype(teststr2);
                  xobjs.pagelist[areawin->page].outscale = tmpfl;
	       }
	       else {
	          sscanf(temp, "%*f %*f %19s", teststr2);
	          if (!strcmp(teststr2, "scale"))
                     xobjs.pagelist[areawin->page].outscale = tmpfl /
		          getpsscale(1.0, areawin->page);
	          else {
	             Wprintf("Error in scale/rotate constructs.");
	             return false;
	          }
	       }
            }
            else {     /* old style scale? */
               sscanf(temp, "%*f %*s %19s", teststr2);
	       if ((teststr2 != NULL) && (!strcmp(teststr2, "scale")))
                  xobjs.pagelist[areawin->page].outscale = tmpfl /
		        getpsscale(1.0, areawin->page);
	       else {
                  Wprintf("Error in scale/rotate constructs.");
                  return false;
	       }
            }
	 }
         else if (strstr(temp, "setlinewidth") != NULL) {
            sscanf(temp, "%f %*s", &xobjs.pagelist[areawin->page].wirewidth);
            xobjs.pagelist[areawin->page].wirewidth /= 1.3;
	    break;
	 }
	 else if (strstr(temp, "insertion") != NULL) {
	    /* read in an included background image */
	    readbackground(ps);
	 }
	 else if (strstr(temp, "<<") != NULL) {
	    char *buffer = temp, *endptr;
	    /* Make sure we have the whole dictionary before calling */
	    while (strstr(buffer, ">>") == NULL) {
	       if (buffer == temp) {
		  buffer = (char *)malloc(strlen(buffer) + 150);
		  strcpy(buffer, temp);
	       }
	       else
	          buffer = (char *)realloc(buffer, strlen(buffer) + 150);
	       endptr = ridnewline(buffer);
	       *endptr++ = ' ';
               fgets(endptr, 149, ps);
	    }
	    /* read top-level parameter dictionary */
	    readparams(NULL, NULL, topobject, buffer);
	    if (buffer != temp) free(buffer);
	 }

	 if (fgets(temp, 149, ps) == NULL) {
            Wprintf("Error: Problems encountered in page header.");
            return false;
         }
      }

      load_in_progress = true;
      objectread(ps, topobject, offx, offy, LIBRARY, temp, DEFAULTCOLOR, NULL);
      load_in_progress = false;

      /* skip to next page boundary or file trailer */

      if (strstr(temp, "showpage") != NULL && multipage != 1) {
	 char *fstop;

	 skiptocomment(temp, 149, ps);

	 /* check for new filename if this is a crash recovery file */
         if ((fstop = strstr(temp, "is_filename")) != NULL) {
            inname = temp + 2;
            inname.truncate(fstop-temp-3);
	    fgets(temp, 149, ps);
	    skiptocomment(temp, 149, ps);
         }
      }

      /* Finally: set the filename and pagename for this page */

      if (mode == 0) {
         char tpstr[6];

	 /* Set filename and page title.	      */
         xobjs.pagelist[areawin->page].filename = inname;

	 /* Get the root name (after all path components) */
         QFileInfo fileinfo(xobjs.pagelist[areawin->page].filename);

	 /* If we didn't read in a page name from the %%Page: header line, then */
	 /* set the page name to the root name of the file.			*/

	 sprintf(tpstr, "%d", page + 1);
	 if (!strcmp(pagestr, tpstr)) {
            if (xobjs.pagelist[areawin->page].filename.isEmpty())
	      sprintf(topobject->name, "Page %d", page + 1);
            else {
              QString filename = fileinfo.fileName();
              if (filename.endsWith(".ps")) filename.chop(3);
              else if (filename.endsWith(".eps")) filename.chop(4);
              sprintf(topobject->name, "%.79s", filename.toLocal8Bit().data());
            }
	 }
	 else
	    sprintf(topobject->name, "%.79s", pagestr);

         renamepage(areawin->page);
      }

      /* set object position to fit to window separately for each page */
      calcbbox(areawin->topinstance);
      centerview(areawin->topinstance);
   }

   /* Crash file recovery: read any out-of-page library definitions tacked */
   /* onto the end of the file into the user library.			   */
   if (strncmp(temp, "%%Trailer", 9)) {
      load_in_progress = true;
      objectread(ps, topobject, 0, 0, USERLIB, temp, DEFAULTCOLOR, NULL);
      load_in_progress = false;
   }

   cleanupaliases(USERLIB);

   /* Connect master->slave schematics */
   while (connlist != NULL) {
      connects *thisconn = connlist;
      objectptr master = NameToPageObject(thisconn->master, NULL, NULL);
      if (master) {
         xobjs.pagelist[thisconn->page].pageinst->thisobject->symschem = master;
         xobjs.pagelist[thisconn->page].pageinst->thisobject->schemtype = SECONDARY;
      }     
      else {
	 Fprintf(stderr, "Error:  Cannot find primary schematic for %s\n",
                xobjs.pagelist[thisconn->page].pageinst->thisobject->name);
      }
      connlist = thisconn->next;
      delete thisconn;
   }        

   Wprintf("Loaded file: %ls (%d page%s)", inname.utf16(), multipage,
	(multipage > 1 ? "s" : ""));

   composelib(loclibnum);
   centerview(xobjs.libtop[loclibnum]);
   composelib(PAGELIB);

   if (version > PROG_VERSION + VEPS) {
      Wprintf("WARNING: file %ls is version %2.1f vs. executable version %2.1f",
                inname.utf16(), version, PROG_VERSION);
   }

   version = PROG_VERSION;
   fclose(ps);
   return true;
}

/*------------------------------------------------------*/
/* Object name comparison:  true if names are equal,    */
/* not counting leading underscores.			*/
/*------------------------------------------------------*/

int objnamecmp(char *name1, char *name2)
{
   char *n1ptr = name1;
   char *n2ptr = name2;

   while (*n1ptr == '_') n1ptr++;                           
   while (*n2ptr == '_') n2ptr++;

   return (strcmp(n1ptr, n2ptr));
}

/*------------------------------------------------------*/
/* Standard delimiter matching character.		*/
/*------------------------------------------------------*/

char standard_delimiter_end(char source)
{
   char target;
   switch(source) {
      case '(':  target = ')'; break;
      case '[':  target = ']'; break;
      case '{':  target = '}'; break;
      case '<':  target = '>'; break;
      default:   target = source;
   }
   return target;
}

/*------------------------------------------------------*/
/* Find matching parenthesis, bracket, brace, or tag	*/
/* Don't count when backslash character '\' is in front */
/*------------------------------------------------------*/

char *find_delimiter(char *fstring)
{
   int count = 1;

   char *search = fstring;
   char source = *fstring;
   char target;

   target = (char)standard_delimiter_end((char)source);
   while (*++search != '\0') {
      if (*search == source && *(search - 1) != '\\') count++;
      else if (*search == target && *(search - 1) != '\\') count--;
      if (count == 0) break;
   }
   return search;
}

/*----------------------------------------------------------------------*/
/* Remove unnecessary font change information from a label		*/
/*----------------------------------------------------------------------*/

void cleanuplabel(stringpart **strhead)
{
   stringpart *curpart = *strhead;
   int oldfont, curfont;
   bool fline = false;

   oldfont = curfont = -1;

   while (curpart != NULL) {
      switch (curpart->type) {
	 case FONT_NAME:
	    if (curfont == curpart->data.font) {
	       /* Font change is redundant:  remove it */
	       /* Careful!  font changes remove overline/underline; if	*/
	       /* either one is in effect, replace it with "noline"	*/
	       if (fline)
		  curpart->type = NOLINE;
	       else
	          curpart = deletestring(curpart, strhead, NULL);
	    }
	    else {
	       curfont = curpart->data.font;
	    }
	    break;

	 case FONT_SCALE:
	    /* Old style font scale is always written absolute, not relative.	*/
	    /* Changes in scale were not allowed, so just get rid of them.	*/
	    if (version < 2.25)
	       curpart = deletestring(curpart, strhead, areawin->topinstance);
	    break;

	 /* A font change may occur inside a parameter, so any font	*/
	 /* declaration after a parameter must be considered to be	*/
	 /* intentional.						*/

	 case PARAM_END:
	    curfont = oldfont = -1;
	    break;

	 case OVERLINE: case UNDERLINE:
	    fline = true;
	    break;

	 case NOLINE:
	    fline = false;
	    break;

	 case NORMALSCRIPT: case RETURN:
	    if (oldfont != -1) {
	       curfont = oldfont;
	       oldfont = -1;
	    }
	    break;

	 case SUBSCRIPT: case SUPERSCRIPT:
	    if (oldfont == -1)
	       oldfont = curfont;
	    break;
      }
      if (curpart != NULL)
	 curpart = curpart->nextpart;
   }
}

/*----------------------------------------------------------------------*/
/* Read label segments							*/
/*----------------------------------------------------------------------*/

void readlabel(objectptr localdata, char *lineptr, stringpart **strhead)
{
   bool fline = false;
   short j;
   char *endptr, *segptr = lineptr;
   char key[100];
   stringpart *newpart;
   oparamptr ops;

   while (*segptr != '\0') {	/* Look through all segments */

      while (isspace(*segptr) && (*segptr != '\0')) segptr++;

      if (*segptr == '(' || *segptr == '{') {
         endptr = find_delimiter(segptr);
         *endptr++ = '\0';
	 /* null string (e.g., null parameter substitution) */
	 if ((*segptr == '(') && (*(segptr + 1) == '\0')) {
	    segptr = endptr;
	    continue;
	 }
      }
      else if (*segptr == '\0' || *segptr == '}') break;

      makesegment(strhead, *strhead);
      newpart = *strhead;

      /* Embedded command is in braces: {} */

      if (*segptr == '{') {	

         /* Find the command for this PostScript procedure */
	 char *cmdptr = endptr - 2;
         while (isspace(*cmdptr)) cmdptr--;
         while (!isspace(*cmdptr) && (cmdptr > segptr)) cmdptr--;
	 cmdptr++;
	 segptr++;

         if (!strncmp(cmdptr, "Ss", 2))
	    newpart->type = SUPERSCRIPT;
         else if (!strncmp(cmdptr, "ss", 2))
            newpart->type = SUBSCRIPT;
         else if (!strncmp(cmdptr, "ns", 2))
            newpart->type = NORMALSCRIPT;
         else if (!strncmp(cmdptr, "hS", 2))
            newpart->type = HALFSPACE;
         else if (!strncmp(cmdptr, "qS", 2))
            newpart->type = QTRSPACE;
         else if (!strncmp(cmdptr, "CR", 2))
	    newpart->type = RETURN;
         else if (!strcmp(cmdptr, "Ts")) /* "Tab set" command */
	    newpart->type = TABSTOP;
         else if (!strcmp(cmdptr, "Tf")) /* "Tab forward" command */
	    newpart->type = TABFORWARD;
         else if (!strcmp(cmdptr, "Tb")) /* "Tab backward" command */
	    newpart->type = TABBACKWARD;
         else if (!strncmp(cmdptr, "ol", 2)) {
            newpart->type = OVERLINE;
            fline = true;
         }
         else if (!strncmp(cmdptr, "ul", 2)) {
            newpart->type = UNDERLINE;
            fline = true;
         }
         else if (!strncmp(cmdptr, "sce", 3)) {  /* revert to default color */
            newpart->type = FONT_COLOR;
	    newpart->data.color = DEFAULTCOLOR;
         }
         else if (cmdptr == segptr) {   /* cancel over- or underline */
            newpart->type = NOLINE;
            fline = false;
         }
	 /* To-do:  replace old-style backspace with tab stop */
         else if (!strcmp(cmdptr, "bs")) {
	    Wprintf("Warning:  Obsolete backspace command ommitted in text");
         }
         else if (!strcmp(cmdptr, "Kn")) {  /* "Kern" command */
	    int kx, ky;
	    sscanf(segptr, "%d %d", &kx, &ky);  
	    newpart->type = KERN;
	    newpart->data.kern[0] = kx;
	    newpart->data.kern[1] = ky;
         }
         else if (!strcmp(cmdptr, "scb")) {  /* change color command */
	    float cr, cg, cb;
	    int cval, cindex;
	    sscanf(segptr, "%f %f %f", &cr, &cg, &cb);  
            newpart->type = FONT_COLOR;
            cval = qRgb(cr * 255, cg * 255, cb * 255);
            cindex = colorlist.indexOf(cval);
            if (cindex < 0) {
	       Wprintf("Error:  Cannot allocate another color");
	       cindex = DEFAULTCOLOR;
	    }
	    newpart->data.color = cindex;
         }
         else if (!strcmp(cmdptr, "cf")) {  /* change font or scale command */
	    char *nextptr, *newptr = segptr;

	    /* Set newptr to the fontname and nextptr to the next token. */
	    while (*newptr != '/' && *newptr != '\0') newptr++;
	    if (*newptr++ == '\0') {
	       Wprintf("Error:  Bad change-font command");
	       newpart->type = NOLINE;	/* placeholder */
	    }
	    for (nextptr = newptr; !isspace(*nextptr); nextptr++);
	    *(nextptr++) = '\0';
	    while (isspace(*nextptr)) nextptr++;

            for (j = 0; j < fontcount; j++)
               if (!strcmp(newptr, fonts[j].psname))
	          break;

            if (j == fontcount) 		/* this is a non-loaded font */
               if (loadfontfile(newptr) < 0) {
		  if (fontcount > 0) {
		     Wprintf("Error:  Font \"%s\" not found---using default.", newptr);
		     j = 0;
		  }
		  else {
		     Wprintf("Error:  No fonts!");
	    	     newpart->type = NOLINE;	/* placeholder */
		  }
	       }

            if (isdigit(*nextptr)) { /* second form of "cf" command---includes scale */
	       float locscale;
	       sscanf(nextptr, "%f", &locscale);
	       if (locscale != 1.0) {
	          newpart->type = FONT_SCALE;
	          newpart->data.scale = locscale;
	          makesegment(strhead, *strhead);
	          newpart = *strhead;
	       }
            }
	    newpart->type = FONT_NAME;
	    newpart->data.font = j;
         }
         else {   /* This exec isn't a known label function */
	    Wprintf("Error:  unknown substring function");
	    newpart->type = NOLINE;	/* placeholder */
         }
      }

      /* Text substring is in parentheses: () */

      else if (*segptr == '(') {
         if (fline) {
	    newpart->type = NOLINE;
            makesegment(strhead, *strhead);
	    newpart = *strhead;
	    fline = false;
         }
         newpart->type = TEXT_STRING;
         newpart->data.string = (char*)malloc(1 + strlen(++segptr));

         /* Copy string, translating octal codes into 8-bit characters */
         parse_ps_string(segptr, newpart->data.string, strlen(segptr), true, true);
      }

      /* Parameterized substrings are denoted by parameter key.	*/
      /* The parameter (default and/or substitution value) is 	*/
      /* assumed to exist.					*/

      else {

         parse_ps_string(segptr, key, 99, false, true);
	 if (strlen(key) > 0) {
	    newpart->type = PARAM_START;
	    newpart->data.string = (char *)malloc(1 + strlen(key));
	    strcpy(newpart->data.string, key);

	    /* check for compatibility between the parameter value and	*/
	    /* the number of parameters and parameter type.		*/

	    ops = match_param(localdata, key);
	    if (ops == NULL) {
	       Fprintf(stderr, "readlabel() error:  No such parameter %s!\n", key);
	       deletestring(newpart, strhead, areawin->topinstance);
	    }

            /* Fprintf(stdout, "Parameter %s called from object %s\n",	*/
	    /* key,	localdata->name);				*/
         }
	 endptr = segptr + 1;
	 while (!isspace(*endptr) && (*endptr != '\0')) endptr++;
      }
      segptr = endptr;
   }
}

/*--------------------------------------*/
/* skip over whitespace in string input */
/*--------------------------------------*/

char *skipwhitespace(char *lineptr)
{
   char *locptr = lineptr;

   while (isspace(*locptr) && (*locptr != '\n') && (*locptr != '\0')) locptr++;
   return locptr;
}

/*-------------------------------------------*/
/* advance to the next token in string input */
/*-------------------------------------------*/

char *advancetoken(char *lineptr)
{
   char *locptr = lineptr;

   while (!isspace(*locptr) && (*locptr != '\n') && (*locptr != '\0')) locptr++;
   while (isspace(*locptr) && (*locptr != '\n') && (*locptr != '\0')) locptr++;
   return locptr;
}

/*------------------------------------------------------*/
/* Read a parameter list for an object instance call.	*/
/* This uses the key-value dictionary method but also	*/
/* allows the "old style" in which each parameter	*/
/* was automatically assigned the key v1, v2, etc.	*/
/*------------------------------------------------------*/

void readparams(objectptr localdata, objinstptr newinst, objectptr libobj,
		char *buffer)
{
   oparamptr newops, objops, fops;
   char *arrayptr, *endptr, *arraynext;
   int paramno = 0;
   char paramkey[100];

   if ((arrayptr = strstr(buffer, "<<")) == NULL)
      if ((arrayptr = strchr(buffer, '[')) == NULL)
	 return;

   endptr = find_delimiter(arrayptr);
   if (*arrayptr == '<') {
      arrayptr++;	/* move to second '<' in "<<"	*/
      endptr--;		/* back up to first '>' in ">>"	*/
   }

   /* move to next non-space token after opening bracket */
   arrayptr++;
   while (isspace(*arrayptr) && *arrayptr != '\0') arrayptr++;

   while ((*arrayptr != '\0') && (arrayptr < endptr)) {

      newops = new oparam;

      /* Arrays contain values only.  Dictionaries contain key:value pairs */
      if (*endptr == '>') {	/* dictionary type */
	 if (*arrayptr != '/') {
	    Fprintf(stdout, "Error: Dictionary key is a literal, not a name\n");
	 }
	 else arrayptr++;	/* Skip PostScript name delimiter */
         parse_ps_string(arrayptr, paramkey, 99, false, true);
	 newops->key = (char *)malloc(1 + strlen(paramkey));
	 strcpy(newops->key, paramkey);
	 arrayptr = advancetoken(arrayptr);
      }
      else {		/* array type; keys are "v1", "v2", etc. */
	 paramno++;
	 newops->key = (char *)malloc(6);
	 sprintf(newops->key, "v%d", paramno);
      }

      /* Find matching parameter in object definition */
      if (newinst) {
	 objops = match_param(libobj, newops->key);
	 if (objops == NULL) {
	    Fprintf(stdout, "Error: parameter %s does not exist in object %s!\n",
			newops->key, libobj->name);
            delete newops;
	    return;
	 }
      }

      /* Add to instance's parameter list */
      /* If newinst is null, then the parameters are added to libobj */
      newops->next = NULL;
      if (newinst) {

	 /* Delete any parameters with duplicate names.  This	*/
	 /* This may indicate an expression parameter that was	*/
	 /* precomputed while determining the bounding box.	*/

	 for (fops = newinst->params; fops != NULL; fops = fops->next)
	    if (!strcmp(fops->key, newops->key))
	       if ((fops = free_instance_param(newinst, fops)) == NULL)
		  break;

	 if (newinst->params == NULL)
	    newinst->params = newops;
	 else {
	    for (fops = newinst->params; fops->next != NULL; fops = fops->next);
	    fops->next = newops;
	 }
      }
      else {
	 if (libobj->params == NULL)
	    libobj->params = newops;
	 else {
	    for (fops = libobj->params; fops->next != NULL; fops = fops->next);
	    fops->next = newops;
	 }
      }

      /* Fill in "which" entry from the object default */
      newops->which = (newinst) ? objops->which : 0;

      /* Check next token.  If not either end-of-dictionary or	*/
      /* the next parameter key, then value is an expression.	*/
      /* Expressions are written as two strings, the first the	*/
      /* result of evaluting the expression, and the second the	*/
      /* expression itself, followed by "pop" to prevent the	*/
      /* PostScript interpreter from trying to evaluate the	*/
      /* expression (which is not in PostScript).		*/

      if (*arrayptr == '(' || *arrayptr == '{')
	 arraynext = find_delimiter(arrayptr);
      else
	 arraynext = arrayptr;
      arraynext = advancetoken(arraynext);

      if ((*endptr == '>') && (arraynext < endptr) && (*arraynext != '/')) {
	 char *substrend, *arraysave;

	 if (*arraynext == '(' || *arraynext == '{') {

	    substrend = find_delimiter(arraynext);
	    arraysave = arraynext + 1;
	    arraynext = advancetoken(substrend);

	    newops->type = (u_char)XC_EXPR;
	    newops->which = P_EXPRESSION;	/* placeholder */
	 }

	 if (strncmp(arraynext, "pop ", 4)) {
	    Wprintf("Error:  bad expression parameter!\n");
#ifdef TCL_WRAPPER
	    newops->parameter.expr = strdup("expr 0");
#else
	    newops->parameter.expr = strdup("0");
#endif
	    arrayptr = advancetoken(arrayptr);
	 } else {
	    *substrend = '\0';
	    newops->parameter.expr = strdup(arraysave);
	    arrayptr = advancetoken(arraynext);
	 }
      }

      else if (*arrayptr == '(' || *arrayptr == '{') { 
	 float r, g, b;
	 char *substrend, csave;
	 stringpart *endpart;

	 /* type XC_STRING */

	 substrend = find_delimiter(arrayptr);
	 csave = *(++substrend);
	 *substrend = '\0';
	 if (*arrayptr == '{') arrayptr++;

	 /* A numerical value immediately following the opening */
	 /* brace indicates a color parameter.			*/
	 if (sscanf(arrayptr, "%f %f %f", &r, &g, &b) == 3) {
	    newops->type = (u_char)XC_INT;
	    newops->which = P_COLOR;
            newops->parameter.ivalue = qRgb(r * 255, g * 255, b * 255);
	    addnewcolorentry(newops->parameter.ivalue);
	    *substrend = csave;
	 }
	 else {
	    char *arraytmp = arrayptr;
	    char linkdefault[5] = "(%n)";

	    newops->type = (u_char)XC_STRING;
	    newops->which = P_SUBSTRING;
	    newops->parameter.string = NULL;

	    /* Quick check for "link" parameter:  make object name into "%n" */
	    if (!strcmp(newops->key, "link"))
	       if (!strncmp(arrayptr + 1, libobj->name, strlen(libobj->name)) &&
			!strcmp(arrayptr + strlen(libobj->name) + 1, ")"))
		  arraytmp = linkdefault;
	    
	    readlabel(libobj, arraytmp, &(newops->parameter.string));
	    *substrend = csave;

	    /* Append a PARAM_END to the parameter string */

	    endpart = makesegment(&(newops->parameter.string), NULL);
	    endpart->type = PARAM_END;
            endpart->data.string = (char*)NULL;
	 }
	 arrayptr = substrend;
	 while (isspace(*arrayptr) && *arrayptr != '\0')
	    arrayptr++;
      }
      else {
	 int scanned = 0;

	 /* type XC_FLOAT or XC_INT, or an indirect reference */

	 newops->type = (newinst) ? objops->type : (u_char)XC_FLOAT;

	 if (newops->type == XC_FLOAT) {
	    scanned = sscanf(arrayptr, "%f", &(newops->parameter.fvalue));
	    /* Fprintf(stdout, "Object %s called with parameter "
			"%s value %g\n", libobj->name,
			newops->key, newops->parameter.fvalue); */
	 }
	 else if (newops->type == XC_INT) {
	    scanned = sscanf(arrayptr, "%d", &(newops->parameter.ivalue));
	    /* Fprintf(stdout, "Object %s called with parameter "
			"%s value %d\n", libobj->name,
			newops->key, newops->parameter.ivalue); */
	 }
	 else if (newops->type == XC_EXPR) {
	    /* Instance values of parameters hold the last evaluated	*/
	    /* result and will be regenerated, so we can ignore them	*/
	    /* here.  By ignoring it, we don't have to deal with issues	*/
	    /* like type promotion.					*/
	    free_instance_param(newinst, newops);
	    scanned = 1;	/* avoid treating as an indirect ref */
	 }
	 else if (newops->type == XC_STRING) {
	    /* Fill string record, so we have a valid record.  This will */
	    /* be blown away and replaced by opsubstitute(), but it must */
	    /* have an initial valid entry.				 */
	    stringpart *tmpptr;
	    newops->parameter.string = NULL;
	    tmpptr = makesegment(&newops->parameter.string, NULL);
	    tmpptr->type = TEXT_STRING;
	    tmpptr = makesegment(&newops->parameter.string, NULL);
	    tmpptr->type = PARAM_END;
	 }
	 else {
	    Fprintf(stderr, "Error: unknown parameter type!\n");
	 }

	 if (scanned == 0) {
	    /* Indirect reference --- create an eparam in the instance */
            parse_ps_string(arrayptr, paramkey, 99, false, true);

	    if (!newinst || !localdata) {
		/* Only object instances can use indirect references */
		Fprintf(stderr, "Error: parameter default %s cannot "
			"be parsed!\n", paramkey);
	    }
	    else if (match_param(localdata, paramkey) == NULL) {
		/* Reference key must exist in the calling object */
		Fprintf(stderr, "Error: parameter value %s cannot be parsed!\n",
			paramkey);
	    }
	    else {
	       /* Create an eparam record in the instance */
	       eparamptr newepp = make_new_eparam(paramkey);
	       newepp->flags |= P_INDIRECT;
	       newepp->pdata.refkey = strdup(newops->key);
	       newepp->next = newinst->passed;
	       newinst->passed = newepp;
	    }

	 }
	 arrayptr = advancetoken(arrayptr);
      }
   }

   /* Calculate the unique bounding box for the instance */

   if (newinst && (newinst->params != NULL)) {
      opsubstitute(libobj, newinst);
      calcbboxinst(newinst);
   }
}

/*--------------------------------------------------------------*/
/* Read a value which might be a short integer or a parameter.	*/
/* If the value is a parameter, check the parameter list to see */
/* if it needs to be re-typecast.  Return the position to the	*/
/* next token in "lineptr".					*/
/*--------------------------------------------------------------*/

char *varpscan(objectptr localdata, char *lineptr, short *hvalue,
	genericptr thiselem, int pointno, int offset, u_char which)
{
   oparamptr ops = NULL;
   char key[100];
   eparamptr newepp;

   if (sscanf(lineptr, "%hd", hvalue) != 1) {
      parse_ps_string(lineptr, key, 99, false, true);
      ops = match_param(localdata, key);
      newepp = make_new_eparam(key);

      /* Add parameter to the linked list */
      newepp->next = thiselem->passed;
      thiselem->passed = newepp;
      newepp->pdata.pointno = pointno;

      if (ops != NULL) {

	 /* It cannot be known whether a parameter value is a float or int */
	 /* until we see how the parameter is used.  So we always read the */
	 /* parameter default as a float, and re-typecast it if necessary. */

	 if (ops->type == XC_FLOAT) {
	    ops->type = XC_INT;
	    /* (add 0.1 to avoid roundoff error in conversion to integer) */
	    ops->parameter.ivalue = (int)(ops->parameter.fvalue +
			((ops->parameter.fvalue < 0) ? -0.1 : 0.1));
	 }
	 ops->which = which;
	 *hvalue = (short)ops->parameter.ivalue;
      }
      else {
	 *hvalue = 0; /* okay; will get filled in later */
	 Fprintf(stderr, "Error:  parameter %s was used but not defined!\n", key);
      }
   }

   *hvalue -= (short)offset;
  
   return advancetoken(skipwhitespace(lineptr));
}

/*--------------------------------------------------------------*/
/* Read a value which might be a short integer or a parameter,	*/
/* but which is not a point in a pointlist.			*/
/*--------------------------------------------------------------*/

char *varscan(objectptr localdata, char *lineptr, short *hvalue,
	 genericptr thiselem, u_char which)
{
   return varpscan(localdata, lineptr, hvalue, thiselem, 0, 0, which);
}

/*--------------------------------------------------------------*/
/* Read a value which might be a float or a parameter.		*/
/* Return the position to the next token in "lineptr".		*/
/*--------------------------------------------------------------*/

char *varfscan(objectptr localdata, char *lineptr, float *fvalue,
	genericptr thiselem, u_char which)
{
   oparamptr ops = NULL;
   eparamptr newepp;
   char key[100];

   if (sscanf(lineptr, "%f", fvalue) != 1) {
      parse_ps_string(lineptr, key, 99, false, true);
      ops = match_param(localdata, key);
      newepp = make_new_eparam(key);

      /* Add parameter to the linked list */
      newepp->next = thiselem->passed;
      thiselem->passed = newepp;

      if (ops != NULL) {
	 ops->which = which;
	 *fvalue = ops->parameter.fvalue;
      }
      else
	 Fprintf(stderr, "Error: no parameter defined!\n");
   }

   /* advance to next token */
   return advancetoken(skipwhitespace(lineptr));
}

/*--------------------------------------------------------------*/
/* Same as varpscan(), but for path types only.			*/
/*--------------------------------------------------------------*/

char *varpathscan(objectptr localdata, char *lineptr, short *hvalue,
	genericptr *thiselem, pathptr thispath, int pointno, int offset,
	u_char which, eparamptr *nepptr)
{
   oparamptr ops = NULL;
   char key[100];
   eparamptr newepp;

   if (nepptr != NULL) *nepptr = NULL;

   if (sscanf(lineptr, "%hd", hvalue) != 1) {
      parse_ps_string(lineptr, key, 99, false, true);
      ops = match_param(localdata, key);
      newepp = make_new_eparam(key);
      newepp->pdata.pathpt[1] = pointno;

      if (thiselem == NULL)
         newepp->pdata.pathpt[0] = (short)0;
      else {
         short elemidx = (short)(thiselem - thispath->begin());
	 if (elemidx >= 0 && elemidx < thispath->parts)
            newepp->pdata.pathpt[0] = (short)(thiselem - thispath->begin());
	 else {
	    Fprintf(stderr, "Error:  Bad parameterized path point!\n");
            delete newepp;
	    goto pathdone;
	 }
      }
      if (nepptr != NULL) *nepptr = newepp;

      /* Add parameter to the linked list. */

      newepp->next = thispath->passed;
      thispath->passed = newepp;

      if (ops != NULL) {

	 /* It cannot be known whether a parameter value is a float or int */
	 /* until we see how the parameter is used.  So we always read the */
	 /* parameter default as a float, and re-typecast it if necessary. */

	 if (ops->type == XC_FLOAT) {
	    ops->type = XC_INT;
	    /* (add 0.1 to avoid roundoff error in conversion to integer) */
	    ops->parameter.ivalue = (int)(ops->parameter.fvalue +
			((ops->parameter.fvalue < 0) ? -0.1 : 0.1));
	 }
	 ops->which = which;
	 *hvalue = (short)ops->parameter.ivalue;
      }
      else {
	 *hvalue = 0; /* okay; will get filled in later */
	 Fprintf(stderr, "Error:  parameter %s was used but not defined!\n", key);
      }
   }

pathdone:
   *hvalue -= (short)offset;
   return advancetoken(skipwhitespace(lineptr));
}

/*--------------------------------------------------------------*/
/* Create a new instance of an object in the library's list of	*/
/* instances.  This instance will be used on the library page	*/
/* when doing "composelib()".					*/
/*--------------------------------------------------------------*/

objinstptr addtoinstlist(int libnum, objectptr libobj, bool isvirtual)
{
   objinstptr newinst = new objinst(libobj, 0, 0);
   liblistptr spec = new liblist;
   liblistptr srch;

   spec->isvirtual = (u_char)isvirtual;
   spec->thisinst = newinst;
   spec->next = NULL;

   /* Add to end, so that duplicate, parameterized instances	*/
   /* always come after the original instance with the default	*/
   /* parameters.						*/

   if ((srch = xobjs.userlibs[libnum].instlist) == NULL)
      xobjs.userlibs[libnum].instlist = spec;
   else {
      while (srch->next != NULL) srch = srch->next;
      srch->next = spec;
   }

   /* Calculate the instance-specific bounding box */
   calcbboxinst(newinst);

   return newinst;
}

/*--------------------------------------------------------------*/
/* Deal with object reads:  Create a new object and prepare for	*/
/* reading.  The library number is passed as "mode".		*/
/*--------------------------------------------------------------*/

objectptr *new_library_object(short mode, char *name, objlistptr *retlist,
		TechPtr defaulttech)
{
   objlistptr newdef, redef = NULL;
   objectptr *newobject, *libobj;
   objectptr *curlib = (mode == FONTLIB) ?
		xobjs.fontlib.library : xobjs.userlibs[mode - LIBRARY].library;
   short *libobjects = (mode == FONTLIB) ?
		&xobjs.fontlib.number : &xobjs.userlibs[mode - LIBRARY].number;
   int i, j;
   char *nsptr, *fullname = name;

   curlib = (objectptr *) realloc(curlib, (*libobjects + 1)
		      * sizeof(objectptr));
   if (mode == FONTLIB) xobjs.fontlib.library = curlib;
   else xobjs.userlibs[mode - LIBRARY].library = curlib;

   /* For (older) libraries that do not use technologies, give the	*/
   /* object a technology name in the form <library>::<object>		*/

   if ((nsptr = strstr(name, "::")) == NULL) {
      int deftechlen = (defaulttech == NULL) ? 0 : strlen(defaulttech->technology);
      fullname = (char *)malloc(deftechlen + strlen(name) + 3);
      if (defaulttech == NULL)
         sprintf(fullname, "::%s", name);
      else
         sprintf(fullname, "%s::%s", defaulttech->technology, name);
   }

   /* initial 1-pointer allocations */

   newobject = curlib + (*libobjects);
   *newobject = new object;

   /* check that this object is not already in list of objects */

   if (mode == FONTLIB) {
      for (libobj = xobjs.fontlib.library; libobj != xobjs.fontlib.library +
            xobjs.fontlib.number; libobj++) {
	 /* This font character may be a redefinition of another */
	 if (!objnamecmp(fullname, (*libobj)->name)) {
            newdef = new objlist;
	    newdef->libno = FONTLIB;
	    newdef->thisobject = *libobj;
	    newdef->next = redef;
	    redef = newdef;
	 }
      }
   }
   else {
      for (i = 0; i < xobjs.numlibs; i++) {
	 for (j = 0; j < xobjs.userlibs[i].number; j++) {
	    libobj = xobjs.userlibs[i].library + j;
	    /* This object may be a redefinition of another object */
	    if (!objnamecmp(fullname, (*libobj)->name)) {
               newdef = new objlist;
	       newdef->libno = i + LIBRARY;
	       newdef->thisobject = *libobj;
	       newdef->next = redef;
	       redef = newdef;
	    }
	 }
      }
   }

   (*libobjects)++;
   sprintf((*newobject)->name, "%s", fullname);
   if (fullname != name) free(fullname);

   /* object::object() initialized schemtype to PRIMARY;  change it. */
   (*newobject)->schemtype = (mode == FONTLIB) ? GLYPH : SYMBOL;

   /* If the object declares a technology name that is different from the */
   /* default, then add the technology name to the list of technologies,  */
   /* with a NULL filename.						  */

   if (mode != FONTLIB) AddObjectTechnology(*newobject);

   *retlist = redef;
   return newobject;
}

/*--------------------------------------------------------------*/
/* do an exhaustive comparison between a new object and any	*/
/* object having the same name. If they are the same, destroy	*/
/* the duplicate.  If different, rename the original one.	*/
/*--------------------------------------------------------------*/

bool library_object_unique(short mode, objectptr newobject, objlistptr redef)
{
   bool is_unique = true;
   objlistptr newdef;
   short *libobjects = (mode == FONTLIB) ?
		&xobjs.fontlib.number : &xobjs.userlibs[mode - LIBRARY].number;

   if (redef == NULL)
      return is_unique;	/* No name conflicts; object is okay as-is */

   for (newdef = redef; newdef != NULL; newdef = newdef->next) {

      /* Must make sure that default parameter values are */
      /* plugged into both objects!		  	  */
      opsubstitute(newdef->thisobject, NULL);
      opsubstitute(newobject, NULL);

      if (*newobject == *newdef->thisobject) {
	  addalias(newdef->thisobject, newobject->name);

	  /* If the new object has declared an association to a */
	  /* schematic, transfer it to the original, and make   */
	  /* sure that the page points to the object which will */
	  /* be left, not the one which will be destroyed.	*/

	  if (newobject->symschem != NULL) {
	     newdef->thisobject->symschem = newobject->symschem;
	     newdef->thisobject->symschem->symschem = newdef->thisobject;
	  }

          delete newobject;
          newobject = NULL;
	  (*libobjects)--;
	  is_unique = false;
	  break;
       }

       /* Not the same object, but has the same name.  This can't	*/
       /* happen within the same input file, so the name of the		*/
       /* original object can safely be altered.			*/

       else if (!strcmp(newobject->name, newdef->thisobject->name)) {

	  /* Replacement---for project management, allow the technology	*/
	  /* master version to take precedence over a local version.	*/

	  TechPtr nsptr = GetObjectTechnology(newobject);

	  if (nsptr && (nsptr->flags & LIBRARY_REPLACE)) {
             delete newobject;
             newobject = NULL;
	     (*libobjects)--;
	     is_unique = false;
	  }
	  else
	     checkname(newdef->thisobject);
	  break;
       }
    }
    for (; (newdef = redef->next); redef = newdef)
       delete redef;
    delete redef;

    return is_unique;
}

/*--------------------------------------------------------------*/
/* Add an instance of the object to the library's instance list */
/*--------------------------------------------------------------*/

void add_object_to_library(short mode, objectptr newobject)
{
   objinstptr libinst;

   if (mode == FONTLIB) return;

   libinst = addtoinstlist(mode - LIBRARY, newobject, false);
   calcbboxvalues(libinst, (genericptr *)NULL);

   /* Center the view of the object in its instance */
   centerview(libinst);
}

/*--------------------------------------------------------------*/
/* Continuation Line --- add memory to "buffer" as necessary.	*/
/* Add a space character to the current text in "buffer" and	*/
/* return a pointer to the new end-of-text.			*/
/*--------------------------------------------------------------*/

char *continueline(char **buffer)
{
   char *lineptr;
   int bufsize;

   for (lineptr = *buffer; (*lineptr != '\n') && (*lineptr != '\0'); lineptr++);
   if (*lineptr == '\n') *lineptr++ = ' ';

   bufsize = (int)(lineptr - (*buffer)) + 256;
   *buffer = (char *)realloc((*buffer), bufsize * sizeof(char));

   return ((*buffer) + (bufsize - 256));
}

/*--------------------------------------------------------------*/
/* Read image data out of the Setup block of the input		*/
/* We assume that width and height have been parsed from the	*/
/* "imagedata" line and the file pointer is at the next line.	*/
/*--------------------------------------------------------------*/

void readimagedata(FILE *ps, int width, int height)
{
   char temp[150], ascbuf[6];
   int x, y, p, q, r, g, b, ilen;
   char *pptr;
   Imagedata *iptr;
   bool do_flate = false, do_ascii = false;
   u_char *filtbuf, *flatebuf;
   union {
      u_char b[4];
      u_long i;
   } pixel;

   iptr = addnewimage(NULL, width, height);

   /* Read the image data */
   
   fgets(temp, 149, ps);
   if (strstr(temp, "ASCII85Decode") != NULL) do_ascii = true;
#ifdef HAVE_LIBZ
   if (strstr(temp, "FlateDecode") != NULL) do_flate = true;
#else
   if (strstr(temp, "FlateDecode") != NULL)
      Fprintf(stderr, "Error:  Don't know how to Flate decode!"
		"  Get zlib and recompile xcircuit!\n");
#endif
   while (strstr(temp, "ReusableStreamDecode") == NULL)
      fgets(temp, 149, ps);  /* Additional piped filter lines */

   fgets(temp, 149, ps);  /* Initial data line */
   q = 0;
   pptr = temp;
   ilen = 3 * width * height;
   filtbuf = (u_char *)malloc(ilen + 4);

   if (!do_ascii) {	/* ASCIIHexDecode algorithm */
      q = 0;
      for (y = 0; y < height; y++) {
         for (x = 0; x < width; x++) {
            sscanf(pptr, "%02x%02x%02x", &r, &g, &b);
            filtbuf[q++] = (u_char)r;
            filtbuf[q++] = (u_char)g;
            filtbuf[q++] = (u_char)b;
            pptr += 6;
            if (*pptr == '\n') {
               fgets(temp, 149, ps);
               pptr = temp;
            }
         }
      }
   }
   else {		/* ASCII85Decode algorithm */
      p = 0;
      while (1) {
         ascbuf[0] = *pptr;
         pptr++;
         if (ascbuf[0] == '~')
	    break;
         else if (ascbuf[0] == 'z') {
	    for (y = 0; y < 5; y++) ascbuf[y] = '\0';
         }
         else {
	    for (y = 1; y < 5; y++) {
	       if (*pptr == '\n') {
	          fgets(temp, 149, ps);
	          pptr = temp;
	       }
	       ascbuf[y] = *pptr;
	       if (ascbuf[y] == '~') {
	          for (; y < 5; y++) {
		     ascbuf[y] = '!';
		     p++;
		  }
	          break;
	       }
	       else pptr++;
	    }
	    for (y = 0; y < 5; y++) ascbuf[y] -= '!';
         }

         if (*pptr == '\n') {
	    fgets(temp, 149, ps);
	    pptr = temp;
         }

         /* Decode from ASCII85 to binary */

         pixel.i = ascbuf[4] + ascbuf[3] * 85 + ascbuf[2] * 7225 +
		ascbuf[1] * 614125 + ascbuf[0] * 52200625;

	 /* Add in roundoff for final bytes */
	 if (p > 0) {
	    switch (p) {
	       case 3:
		  pixel.i += 0xff0000;
	       case 2:
		  pixel.i += 0xff00;
	       case 1:
		  pixel.i += 0xff;
	    }
	 }

         for (y = 0; y < (4 - p); y++) {
	    filtbuf[q + y] = pixel.b[3 - y];
         }
         q += (4 - p);
         if (q >= ilen) break;
      }
   }

   /* Extra decoding goes here */

#ifdef HAVE_LIBZ
   if (do_flate) {
      flatebuf = (u_char *)malloc(ilen);
      large_inflate(filtbuf, q, &flatebuf, ilen);
      free(filtbuf);
   }
   else
#endif

   flatebuf = filtbuf;

   pixel.b[3] = 0;
   q = 0;
   for (y = 0; y < height; y++) {
      for (x = 0; x < width; x++) {
         iptr->image->setPixel(x, y, qRgb(flatebuf[q+0], flatebuf[q+1], flatebuf[q+2]));
         q += 3;
      }
   }

   free(flatebuf);

   fgets(temp, 149, ps);	/* definition line */
   fgets(temp, 149, ps);	/* pick up name of image from here */
   for (pptr = temp; !isspace(*pptr); pptr++);
   *pptr = '\0';
   iptr->filename = strdup(temp + 1);
   for (x = 0; x < 5; x++) fgets(temp, 149, ps);  /* skip image dictionary */
}

/*--------------------------------------------------------------*/
/* Read an object (page) from a file into xcircuit		*/
/*--------------------------------------------------------------*/

bool objectread(FILE *ps, objectptr localdata, short offx, short offy,
	short mode, char *retstr, int ccolor, TechPtr defaulttech)
{
   char *temp, *buffer, keyword[80];
   short tmpfont = -1;
   float tmpscale = 0.0;
   objectptr	*libobj;
   int curcolor = ccolor;
   char *colorkey = NULL;
   int i, j, k;
   objinstptr *newinst;
   eparamptr epptrx, epptry;	/* used for paths only */

   /* path-handling variables */
   pathptr *newpath;
   XPoint startpoint;

   keyword[0] = '\0';

   buffer = (char *)malloc(256 * sizeof(char));
   temp = buffer;

   for(;;) {
      char *lineptr, *keyptr;

      if (fgets(temp, 255, ps) == NULL) {
	 if (strcmp(keyword, "restore")) {
            Wprintf("Error: end of file.");
	    *retstr = '\0';
	 }
	 break;
      }
      temp = buffer;

      /* because PostScript is a stack language, we will scan from the end */
      for (lineptr = buffer; (*lineptr != '\n') && (*lineptr != '\0'); lineptr++);
      if (lineptr != buffer) {  /* ignore any blank lines */
         for (keyptr = lineptr - 1; isspace(*keyptr) && keyptr != buffer; keyptr--);
         for (; !isspace(*keyptr) && keyptr != buffer; keyptr--);
         sscanf(keyptr, "%79s", keyword);

         if (!strcmp(keyword, "showpage")) {
            strncpy(retstr, buffer, 150);
            retstr[149] = '\0';
	    free(buffer);

	    /* If we have just read a schematic that is attached	*/
	    /* to a symbol, check all of the pin labels in the symbol	*/
	    /* to see if they correspond to pin names in the schematic.	*/
	    /* The other way around (pin in schematic with no		*/
	    /* corresponding name in the symbol) is not an error.	*/

	    if (localdata->symschem != NULL) {

               genericptr *lgen;
               labelptr lcmp;
               for (labeliter plab; localdata->symschem->values(plab); ) {
                 if (plab->pin == LOCAL) {
                    for (lgen = localdata->begin(); lgen < localdata->end(); lgen++) {
                       if (IS_LABEL(*lgen)) {
                          lcmp = TOLABEL(lgen);
                          if (lcmp->pin == LOCAL)
                             if (!stringcomprelaxed(lcmp->string, plab->string,
                                            areawin->topinstance))
                                break;
                       }
                    }
                    if (lgen == localdata->end()) {
                       char *pch = textprint(plab->string, areawin->topinstance);
                       Fprintf(stderr, "Warning:  Unattached pin \"%s\" in "
                                    "symbol %s\n", pch,
                                    localdata->symschem->name);
                       free(pch);
                    }
                 }
	       }
	    }
	    return false;  /* end of page */
	 }

	 /* make a color change, adding the color if necessary */

	 else if (!strcmp(keyword, "scb")) {
	    float red, green, blue;
	    if (sscanf(buffer, "%f %f %f", &red, &green, &blue) == 3) {
               curcolor = qRgb(red * 255, green * 255, blue * 255);
	       addnewcolorentry(curcolor);
	       colorkey = NULL;
	    }
	    else {
	       char tmpkey[30];
	       oparamptr ops;

               parse_ps_string(buffer, tmpkey, 29, false, true);
	       ops = match_param(localdata, tmpkey);
	       if (ops != NULL) {
		  /* Recast expression parameter, if necessary */
		  if (ops->which == P_EXPRESSION) ops->which = P_COLOR;
		  if (ops->which == P_COLOR) {
		     colorkey = ops->key;
		     switch (ops->type) {
			case XC_INT:
		           curcolor = ops->parameter.ivalue;
			   break;
			default:
		           curcolor = DEFAULTCOLOR;	/* placeholder */
			   break;
		     }
		  }
	       }
	    }
	 }

	 /* end the color change, returning to default */

	 else if (!strcmp(keyword, "sce")) {
	    curcolor = ccolor;
	    colorkey = NULL;
	 }

	 /* begin a path constructor */

	 else if (!strcmp(keyword, "beginpath")) {
	    
            newpath = localdata->append(new path);
            (*newpath)->color = curcolor;

	    lineptr = varpathscan(localdata, buffer, &startpoint.x,
			(genericptr *)NULL, *newpath, 0, offx, P_POSITION_X,
			&epptrx);
	    lineptr = varpathscan(localdata, lineptr, &startpoint.y,
			(genericptr *)NULL, *newpath, 0, offy, P_POSITION_Y,
			&epptry);

            std_eparam(*newpath, colorkey);
	 }

	 /* end the path constructor */

	 else if (!strcmp(keyword, "endpath")) {

            lineptr = varscan(localdata, buffer, (short int*)&(*newpath)->style,
                        *newpath, P_STYLE);
	    lineptr = varfscan(localdata, lineptr, &(*newpath)->width,
                        *newpath, P_LINEWIDTH);

            if ((*newpath)->parts <= 0) {
                /* in case of an empty path */
               --localdata->parts;
               delete *newpath;
	    }
	    newpath = NULL;
	 }

	 /* read path parts */

	 else if (!strcmp(keyword, "polyc")) {
	    polyptr *newpoly;
            XPoint* newpoints;
	    short tmpnum;

            newpoly = (*newpath)->append(new polygon);

	    for (--keyptr; *keyptr == ' '; keyptr--);
	    for (; *keyptr != ' '; keyptr--);
	    sscanf(keyptr, "%hd", &tmpnum);
            (*newpoly)->points.resize(tmpnum + 1);
	    (*newpoly)->width = 1.0;
	    (*newpoly)->style = UNCLOSED;
            (*newpoly)->color = curcolor;
            (*newpoly)->cycle = NULL;

	    /* If the last point on the last path part was parameterized, then	*/
	    /* the first point of the spline must be, too.			*/

	    if (epptrx != NULL) {
               eparamptr newepp = copyeparam(epptrx, *newpath);
	       newepp->next = (*newpath)->passed;
	       (*newpath)->passed = newepp;
	       newepp->pdata.pathpt[1] = 0;
	       newepp->pdata.pathpt[0] = (*newpath)->parts - 1;
	    }
	    if (epptry != NULL) {
               eparamptr newepp = copyeparam(epptry, *newpath);
	       newepp->next = (*newpath)->passed;
	       (*newpath)->passed = newepp;
	       newepp->pdata.pathpt[1] = 0;
	       newepp->pdata.pathpt[0] = (*newpath)->parts - 1;
	    }

	    lineptr = buffer;

            newpoints = (*newpoly)->points.end() - 1;
	    lineptr = varpathscan(localdata, lineptr, &newpoints->x,
                        (genericptr *)newpoly, *newpath, newpoints - (*newpoly)->points.begin(),
			 offx, P_POSITION_X, &epptrx);
	    lineptr = varpathscan(localdata, lineptr, &newpoints->y,
                        (genericptr *)newpoly, *newpath, newpoints - (*newpoly)->points.begin(),
			offy, P_POSITION_Y, &epptry);

            for (--newpoints; newpoints > (*newpoly)->points.begin(); newpoints--) {

	       lineptr = varpathscan(localdata, lineptr, &newpoints->x,
                        (genericptr *)newpoly, *newpath, newpoints - (*newpoly)->points.begin(),
			 offx, P_POSITION_X, NULL);
	       lineptr = varpathscan(localdata, lineptr, &newpoints->y,
                        (genericptr *)newpoly, *newpath, newpoints - (*newpoly)->points.begin(),
			offy, P_POSITION_Y, NULL);
	    }
            *newpoints = startpoint;
            startpoint = *(newpoints + (*newpoly)->points.count() - 1);
	 }

	 else if (!strcmp(keyword, "arc") || !strcmp(keyword, "arcn")) {
	    arcptr *newarc;
            newarc = (*newpath)->append(new arc);
	    (*newarc)->width = 1.0;
	    (*newarc)->style = UNCLOSED;
            (*newarc)->color = curcolor;
            (*newarc)->cycle = NULL;

	    lineptr = varpscan(localdata, buffer, &(*newarc)->position.x,
                        *newarc, 0, offx, P_POSITION_X);
	    lineptr = varpscan(localdata, lineptr, &(*newarc)->position.y,
                        *newarc, 0, offy, P_POSITION_Y);
	    lineptr = varscan(localdata, lineptr, &(*newarc)->radius,
                        *newarc, P_RADIUS);
	    lineptr = varfscan(localdata, lineptr, &(*newarc)->angle1,
                        *newarc, P_ANGLE1);
	    lineptr = varfscan(localdata, lineptr, &(*newarc)->angle2,
                        *newarc, P_ANGLE2);

	    (*newarc)->yaxis = (*newarc)->radius;
	    if (!strcmp(keyword, "arcn")) {
	       float tmpang = (*newarc)->angle1;
	       (*newarc)->radius = -((*newarc)->radius);
	       (*newarc)->angle1 = (*newarc)->angle2;
	       (*newarc)->angle2 = tmpang;
	    }
		
            (*newarc)->calc();
            startpoint = (*newarc)->points[(*newarc)->number - 1];
            (*newpath)->replace_last(new spline(**newarc));
	 }

	 else if (!strcmp(keyword, "pellip") || !strcmp(keyword, "nellip")) {
	    arcptr *newarc;
            newarc = (*newpath)->append(new arc);
	    (*newarc)->width = 1.0;
	    (*newarc)->style = UNCLOSED;
            (*newarc)->color = curcolor;
            (*newarc)->cycle = NULL;

	    lineptr = varpscan(localdata, buffer, &(*newarc)->position.x,
                        *newarc, 0, offx, P_POSITION_X);
	    lineptr = varpscan(localdata, lineptr, &(*newarc)->position.y,
                        *newarc, 0, offy, P_POSITION_Y);
	    lineptr = varscan(localdata, lineptr, &(*newarc)->radius,
                        *newarc, P_RADIUS);
	    lineptr = varscan(localdata, lineptr, &(*newarc)->yaxis,
                        *newarc, P_MINOR_AXIS);
	    lineptr = varfscan(localdata, lineptr, &(*newarc)->angle1,
                        *newarc, P_ANGLE1);
	    lineptr = varfscan(localdata, lineptr, &(*newarc)->angle2,
                        *newarc, P_ANGLE2);

	    if (!strcmp(keyword, "nellip")) {
	       float tmpang = (*newarc)->angle1;
	       (*newarc)->radius = -((*newarc)->radius);
	       (*newarc)->angle1 = (*newarc)->angle2;
	       (*newarc)->angle2 = tmpang;
		
	    }
            (*newarc)->calc();;
            startpoint = (*newarc)->points[(*newarc)->number - 1];
            (*newpath)->replace_last(new spline(**newarc));
	 }

	 else if (!strcmp(keyword, "curveto")) {
	    splineptr *newspline;
            newspline = (*newpath)->append(new spline);

            (*newspline)->cycle = NULL;
	    (*newspline)->width = 1.0;
	    (*newspline)->style = UNCLOSED;
	    (*newspline)->color = curcolor;

	    /* If the last point on the last path part was parameterized, then	*/
	    /* the first point of the spline must be, too.			*/

	    if (epptrx != NULL) {
               eparamptr newepp = copyeparam(epptrx, *newpath);
	       newepp->next = (*newpath)->passed;
	       (*newpath)->passed = newepp;
	       newepp->pdata.pathpt[1] = 0;
	       newepp->pdata.pathpt[0] = (*newpath)->parts - 1;
	    }
	    if (epptry != NULL) {
               eparamptr newepp = copyeparam(epptry, *newpath);
	       newepp->next = (*newpath)->passed;
	       (*newpath)->passed = newepp;
	       newepp->pdata.pathpt[1] = 0;
	       newepp->pdata.pathpt[0] = (*newpath)->parts - 1;
	    }


	    lineptr = varpathscan(localdata, buffer, &(*newspline)->ctrl[1].x,
			(genericptr *)newspline, *newpath, 1, offx, P_POSITION_X,
			NULL);
	    lineptr = varpathscan(localdata, lineptr, &(*newspline)->ctrl[1].y,
			(genericptr *)newspline, *newpath, 1, offy, P_POSITION_Y,
			NULL);
	    lineptr = varpathscan(localdata, lineptr, &(*newspline)->ctrl[2].x,
			(genericptr *)newspline, *newpath, 2, offx, P_POSITION_X,
			NULL);
	    lineptr = varpathscan(localdata, lineptr, &(*newspline)->ctrl[2].y,
			(genericptr *)newspline, *newpath, 2, offy, P_POSITION_Y,
			NULL);
	    lineptr = varpathscan(localdata, lineptr, &(*newspline)->ctrl[3].x,
			(genericptr *)newspline, *newpath, 3, offx, P_POSITION_X,
			&epptrx);
	    lineptr = varpathscan(localdata, lineptr, &(*newspline)->ctrl[3].y,
			(genericptr *)newspline, *newpath, 3, offy, P_POSITION_Y,
			&epptry);

            (*newspline)->ctrl[0] = startpoint;

            (*newspline)->calc();
            startpoint = (*newspline)->ctrl[3];
	 }

         /* read arcs */

         else if (!strcmp(keyword, "xcarc")) {
            arcptr *newarc;
            newarc = localdata->append(new arc);
            (*newarc)->color = curcolor;
            (*newarc)->cycle = NULL;

	    /* backward compatibility */
	    if (version < 1.5) {
	       sscanf(buffer, "%hd %hd %hd %f %f %f %hd", &(*newarc)->position.x,
	          &(*newarc)->position.y, &(*newarc)->radius, &(*newarc)->angle1,
	          &(*newarc)->angle2, &(*newarc)->width, &(*newarc)->style);
	       (*newarc)->position.x -= offx;
	       (*newarc)->position.y -= offy;
	    }
	    else {
               lineptr = varscan(localdata, buffer, (short int*)&(*newarc)->style,
                                *newarc, P_STYLE);
               lineptr = varfscan(localdata, lineptr, &(*newarc)->width,
                                *newarc, P_LINEWIDTH);
	       lineptr = varpscan(localdata, lineptr, &(*newarc)->position.x,
                                *newarc, 0, offx, P_POSITION_X);
	       lineptr = varpscan(localdata, lineptr, &(*newarc)->position.y,
                                *newarc, 0, offy, P_POSITION_Y);
	       lineptr = varscan(localdata, lineptr, &(*newarc)->radius,
                                *newarc, P_RADIUS);
	       lineptr = varfscan(localdata, lineptr, &(*newarc)->angle1,
                                *newarc, P_ANGLE1);
	       lineptr = varfscan(localdata, lineptr, &(*newarc)->angle2,
                                *newarc, P_ANGLE2);
	    }

	    (*newarc)->yaxis = (*newarc)->radius;
            (*newarc)->calc();
            std_eparam((*newarc), colorkey);
         }

	 /* read ellipses */

         else if (!strcmp(keyword, "ellipse")) {
	    arcptr *newarc;
            newarc = localdata->append(new arc);
            (*newarc)->color = curcolor;
            (*newarc)->cycle = NULL;

            lineptr = varscan(localdata, buffer, (short int*)&(*newarc)->style,
                        *newarc, P_STYLE);
	    lineptr = varfscan(localdata, lineptr, &(*newarc)->width,
                        *newarc, P_LINEWIDTH);
	    lineptr = varpscan(localdata, lineptr, &(*newarc)->position.x,
                        *newarc, 0, offx, P_POSITION_X);
	    lineptr = varpscan(localdata, lineptr, &(*newarc)->position.y,
                        *newarc, 0, offy, P_POSITION_Y);
	    lineptr = varscan(localdata, lineptr, &(*newarc)->radius,
                        *newarc, P_RADIUS);
	    lineptr = varscan(localdata, lineptr, &(*newarc)->yaxis,
                        *newarc, P_MINOR_AXIS);
	    lineptr = varfscan(localdata, lineptr, &(*newarc)->angle1,
                        *newarc, P_ANGLE1);
	    lineptr = varfscan(localdata, lineptr, &(*newarc)->angle2,
                        *newarc, P_ANGLE2);

            (*newarc)->calc();
            std_eparam((*newarc), colorkey);
         }

         /* read polygons */
	 /* (and wires---backward compatibility for v1.5 and earlier) */

         else if (!strcmp(keyword, "polygon") || !strcmp(keyword, "wire")) {
	    polyptr *newpoly;
            XPoint* newpoints;

            newpoly = localdata->append(new polygon);
	    lineptr = buffer;

            (*newpoly)->cycle = NULL;

	    if (!strcmp(keyword, "wire")) {
               (*newpoly)->points.resize(2);
	       (*newpoly)->width = 1.0;
	       (*newpoly)->style = UNCLOSED;
	    }
	    else {
	       /* backward compatibility */
	       if (version < 1.5) {
	          for (--keyptr; *keyptr == ' '; keyptr--);
	          for (; *keyptr != ' '; keyptr--);
	          sscanf(keyptr, "%hd", &(*newpoly)->style);
	          for (--keyptr; *keyptr == ' '; keyptr--);
	          for (; *keyptr != ' '; keyptr--);
	          sscanf(keyptr, "%f", &(*newpoly)->width);
	       }
	       for (--keyptr; *keyptr == ' '; keyptr--);
	       for (; *keyptr != ' '; keyptr--);
               short int number;
               sscanf(keyptr, "%hd", &number);
               (*newpoly)->points.resize(number);

	       if (version >= 1.5) {
                  lineptr = varscan(localdata, lineptr, (short int*)&(*newpoly)->style,
                                *newpoly, P_STYLE);
	          lineptr = varfscan(localdata, lineptr, &(*newpoly)->width,
                                *newpoly, P_LINEWIDTH);
	       }
	    }

	    if ((*newpoly)->style & BBOX)
	       (*newpoly)->color = BBOXCOLOR;
	    else
	       (*newpoly)->color = curcolor;

            for (newpoints = (*newpoly)->points.begin(); newpoints < (*newpoly)->points.end();
                newpoints++) {
	       lineptr = varpscan(localdata, lineptr, &newpoints->x,
                        *newpoly, newpoints - (*newpoly)->points.begin(),
			offx, P_POSITION_X);
	       lineptr = varpscan(localdata, lineptr, &newpoints->y,
                        *newpoly, newpoints - (*newpoly)->points.begin(),
			offy, P_POSITION_Y);
            }
            std_eparam((*newpoly), colorkey);
         }

	 /* read spline curves */

         else if (!strcmp(keyword, "spline")) {
            splineptr *newspline;

            newspline = localdata->append(new spline);
            (*newspline)->color = curcolor;
            (*newspline)->cycle = NULL;

	    /* backward compatibility */
	    if (version < 1.5) {
               sscanf(buffer, "%f %hd %hd %hd %hd %hd %hd %hd %hd %hd", 
	          &(*newspline)->width, &(*newspline)->ctrl[1].x,
	          &(*newspline)->ctrl[1].y, &(*newspline)->ctrl[2].x,
	          &(*newspline)->ctrl[2].y, &(*newspline)->ctrl[3].x,
	          &(*newspline)->ctrl[3].y, &(*newspline)->ctrl[0].x,
	          &(*newspline)->ctrl[0].y, &(*newspline)->style);
	       (*newspline)->ctrl[1].x -= offx; (*newspline)->ctrl[2].x -= offx;
	       (*newspline)->ctrl[0].x -= offx;
	       (*newspline)->ctrl[3].x -= offx;
	       (*newspline)->ctrl[1].y -= offy; (*newspline)->ctrl[2].y -= offy;
	       (*newspline)->ctrl[3].y -= offy;
	       (*newspline)->ctrl[0].y -= offy;
	    }
	    else {

               lineptr = varscan(localdata, buffer, (short int*)&(*newspline)->style,
                                *newspline, P_STYLE);
	       lineptr = varfscan(localdata, lineptr, &(*newspline)->width,
                                *newspline, P_LINEWIDTH);
	       lineptr = varpscan(localdata, lineptr, &(*newspline)->ctrl[1].x,
                                *newspline, 1, offx, P_POSITION_X);
	       lineptr = varpscan(localdata, lineptr, &(*newspline)->ctrl[1].y,
                                *newspline, 1, offy, P_POSITION_Y);
	       lineptr = varpscan(localdata, lineptr, &(*newspline)->ctrl[2].x,
                                *newspline, 2, offx, P_POSITION_X);
	       lineptr = varpscan(localdata, lineptr, &(*newspline)->ctrl[2].y,
                                *newspline, 2, offy, P_POSITION_Y);
	       lineptr = varpscan(localdata, lineptr, &(*newspline)->ctrl[3].x,
                                *newspline, 3, offx, P_POSITION_X);
	       lineptr = varpscan(localdata, lineptr, &(*newspline)->ctrl[3].y,
                                *newspline, 3, offy, P_POSITION_Y);
	       lineptr = varpscan(localdata, lineptr, &(*newspline)->ctrl[0].x,
                                *newspline, 0, offx, P_POSITION_X);
	       lineptr = varpscan(localdata, lineptr, &(*newspline)->ctrl[0].y,
                                *newspline, 0, offy, P_POSITION_Y);
	    }

            (*newspline)->calc();
            std_eparam((*newspline), colorkey);
         }

         /* read graphics image instances */

	 else if (!strcmp(keyword, "graphic")) {
            graphicptr *newgp;
	    Imagedata *img;

            newgp = localdata->append(new graphic);
            (*newgp)->color = curcolor;

	    lineptr = buffer + 1;
	    for (i = 0; i < xobjs.images; i++) {
	       img = xobjs.imagelist + i;
	       if (!strncmp(img->filename, lineptr, strlen(img->filename))) {
		  (*newgp)->source = img->image;
		  img->refcount++;
		  break;
	       }
	    }
	    lineptr += strlen(img->filename) + 1;

	    lineptr = varfscan(localdata, lineptr, &(*newgp)->scale,
                                *newgp, P_SCALE);
	    lineptr = varscan(localdata, lineptr, &(*newgp)->rotation,
                                *newgp, P_ROTATION);
	    lineptr = varpscan(localdata, lineptr, &(*newgp)->position.x,
                                *newgp, 0, offx, P_POSITION_X);
	    lineptr = varpscan(localdata, lineptr, &(*newgp)->position.y,
                                *newgp, 0, offy, P_POSITION_Y);
            std_eparam(*newgp, colorkey);
	 }

         /* read labels */

         else if (!strcmp(keyword, "fontset")) { 	/* old style */
            char tmpstring[100];
            int i;
            sscanf(buffer, "%f %*c%99s", &tmpscale, tmpstring);
            for (i = 0; i < fontcount; i++)
               if (!strcmp(tmpstring, fonts[i].psname)) {
		  tmpfont = i;
		  break;
	       }
	    if (i == fontcount) i = 0;	/* Why bother with anything fancy? */
         }

         else if (!strcmp(keyword, "label") || !strcmp(keyword, "pinlabel")
		|| !strcmp(keyword, "pinglobal") || !strcmp(keyword, "infolabel")) {

	    labelptr *newlabel;
	    stringpart *firstscale, *firstfont;

            newlabel = localdata->append(new label);
            (*newlabel)->color = curcolor;
            (*newlabel)->cycle = NULL;

	    /* scan backwards to get the number of substrings */
	    lineptr = keyptr - 1;
	    for (i = 0; i < ((version < 2.25) ? 5 : 6); i++) {
	       for (; *lineptr == ' '; lineptr--);
	       for (; *lineptr != ' '; lineptr--);
	    }
	    if ((strchr(lineptr, '.') != NULL) && (version < 2.25)) {
	       Fprintf(stderr, "Error:  File version claims to be %2.1f,"
			" but has version %2.1f labels\n", version, PROG_VERSION);
	       Fprintf(stderr, "Attempting to resolve problem by updating version.\n");
	       version = PROG_VERSION;
	       for (; *lineptr == ' '; lineptr--);
	       for (; *lineptr != ' '; lineptr--);
	    }
	    /* no. segments is ignored---may be a derived quantity, anyway */
	    if (version < 2.25) {
	       sscanf(lineptr, "%*s %hd %hd %hd %hd", &(*newlabel)->justify,
		   &(*newlabel)->rotation, &(*newlabel)->position.x,
		   &(*newlabel)->position.y);
	       (*newlabel)->position.x -= offx; (*newlabel)->position.y -= offy;
	       *lineptr = '\0';
	    }
	    else {
	       *lineptr++ = '\0';
	       lineptr = advancetoken(lineptr);  /* skip string token */
	       lineptr = varscan(localdata, lineptr, &(*newlabel)->justify,
                                *newlabel, P_JUSTIFY);
	       lineptr = varscan(localdata, lineptr, &(*newlabel)->rotation,
                                *newlabel, P_ROTATION);
	       lineptr = varfscan(localdata, lineptr, &(*newlabel)->scale,
                                *newlabel, P_SCALE);
	       lineptr = varpscan(localdata, lineptr, &(*newlabel)->position.x,
                                *newlabel, 0, offx, P_POSITION_X);
	       lineptr = varpscan(localdata, lineptr, &(*newlabel)->position.y,
                                *newlabel, 0, offy, P_POSITION_Y);
	    }
	    if (version < 2.4)
	       (*newlabel)->rotation = -(*newlabel)->rotation;
	    while ((*newlabel)->rotation < 0) (*newlabel)->rotation += 360;

	    (*newlabel)->pin = false;
	    if (strcmp(keyword, "label")) {	/* all the schematic types */
	       /* enable schematic capture if it is not already on. */
	       if (!strcmp(keyword, "pinlabel"))
		  (*newlabel)->pin = LOCAL;
	       else if (!strcmp(keyword, "pinglobal"))
		  (*newlabel)->pin = GLOBAL;
	       else if (!strcmp(keyword, "infolabel")) {
		  /* Do not turn top-level pages into symbols! */
		  /* Info labels on schematics are treated differently. */
		  if (localdata != topobject)
		     localdata->schemtype = FUNDAMENTAL;
		  (*newlabel)->pin = INFO;
		  if (curcolor == DEFAULTCOLOR)
		     (*newlabel)->color = INFOLABELCOLOR;
	       }
	    }

	    lineptr = buffer;	/* back to beginning of string */
	    if (!strncmp(lineptr, "mark", 4)) lineptr += 4;

	    readlabel(localdata, lineptr, &(*newlabel)->string);

	    if (version < 2.25) {
	       /* Switch 1st scale designator to overall font scale */

	       firstscale = (*newlabel)->string->nextpart;
	       if (firstscale->type != FONT_SCALE) {
	          if (tmpscale != 0.0)
	             (*newlabel)->scale = 0.0;
	          else
	             (*newlabel)->scale = 1.0;
	       }
	       else {
	          (*newlabel)->scale = firstscale->data.scale;
	          deletestring(firstscale, &((*newlabel)->string),
				areawin->topinstance);
	       }
	    }

	    firstfont = (*newlabel)->string;
	    if ((firstfont == NULL) || (firstfont->type != FONT_NAME)) {
	       if (tmpfont == -1) {
	          Fprintf(stderr, "Error:  Label with no font designator?\n");
		  tmpfont = 0;
	       }
	       firstfont = makesegment(&((*newlabel)->string), (*newlabel)->string);  
	       firstfont->type = FONT_NAME;
	       firstfont->data.font = tmpfont;
	    }
	    cleanuplabel(&(*newlabel)->string);

            std_eparam(*newlabel, colorkey);
         }

	 /* read symbol-to-schematic connection */

	 else if (!strcmp(keyword, "is_schematic")) {
	    char tempstr[50];
	    for (lineptr = buffer; *lineptr == ' '; lineptr++);
            parse_ps_string(++lineptr, tempstr, 49, false, false);
	    checksym(localdata, tempstr);
	 }

         /* read bounding box (font files only)	*/

         else if (!strcmp(keyword, "bbox")) {
	    for (lineptr = buffer; *lineptr == ' '; lineptr++);
            if (*lineptr != '%') {
	       Wprintf("Illegal bbox.");
	       free(buffer);
               *retstr = '\0';
	       return true;
	    }
	    sscanf(++lineptr, "%hd %hd %hd %hd",
		&localdata->bbox.lowerleft.x, &localdata->bbox.lowerleft.y,
		&localdata->bbox.width, &localdata->bbox.height);
         }

	 /* read "hidden" attribute */

	 else if (!strcmp(keyword, "hidden")) {
	    localdata->hidden = true;
	 }

	 /* read "libinst" special instance of a library part */

	 else if (!strcmp(keyword, "libinst")) {

	    /* Read backwards from keyword to find name of object instanced. */
	    for (lineptr = keyptr; *lineptr != '/' && lineptr > buffer;
			lineptr--);
            parse_ps_string(++lineptr, keyword, 79, false, false);
	    new_library_instance(mode - LIBRARY, keyword, buffer, defaulttech);
	 }

	 /* read objects */

         else if (!strcmp(keyword, "{")) {  /* This is an object definition */
	    objlistptr redef;
	    objectptr *newobject;

	    for (lineptr = buffer; *lineptr == ' '; lineptr++);
	    if (*lineptr++ != '/') {
	       /* This may be part of a label. . . treat as a continuation line */
	       temp = continueline(&buffer);
	       continue;
	    }
            parse_ps_string(lineptr, keyword, 79, false, false);

	    newobject = new_library_object(mode, keyword, &redef, defaulttech);

	    if (objectread(ps, *newobject, 0, 0, mode, retstr, curcolor,
                        defaulttech)) {
               strncpy(retstr, buffer, 150);
               retstr[149] = '\0';
	       free(buffer);
	       return true;
            }
	    else {
	       if (library_object_unique(mode, *newobject, redef))
	          add_object_to_library(mode, *newobject);
	    }
         }
         else if (!strcmp(keyword, "def")) {
            strncpy(retstr, buffer, 150);
            retstr[149] = '\0';
	    free (buffer);
	    return false; /* end of object def or end of object library */
	 }

	 else if (!strcmp(keyword, "loadfontencoding")) {
	    /* Deprecated, but retained for backward compatibility. */
	    /* Load from script, .xcircuitrc, or command line instead. */
	    for (lineptr = buffer; *lineptr != '%'; lineptr++);
	    sscanf (lineptr + 1, "%149s", _STR);
	    if (*(lineptr + 1) != '%') loadfontfile(_STR);
	 }
	 else if (!strcmp(keyword, "loadlibrary")) {
	    /* Deprecated, but retained for backward compatibility */
	    /* Load from script, .xcircuitrc, or command line instead. */
	    int ilib, tlib;

	    for (lineptr = buffer; *lineptr != '%'; lineptr++);
            char str[150];
            sscanf (++lineptr, "%149s", str);
	    while (isspace(*lineptr)) lineptr++;
	    while (!isspace(*++lineptr));
	    while (isspace(*++lineptr));
	    if (sscanf (lineptr, "%d", &ilib) > 0) {
	       while ((ilib - 2 + LIBRARY) > xobjs.numlibs) {
		  tlib = createlibrary(false);
		  if (tlib != xobjs.numlibs - 2 + LIBRARY) {
		     ilib = tlib;
		     break;
		  }
	       }
	       mode = ilib - 1 + LIBRARY;
	    }
            loadlibrary(mode, str);
	 }
	 else if (!strcmp(keyword, "beginparm")) { /* parameterized object */
	    short tmpnum, i;
	    for (--keyptr; *keyptr == ' '; keyptr--);
	    for (; isdigit(*keyptr) && (keyptr >= buffer); keyptr--);
	    sscanf(keyptr, "%hd", &tmpnum);
	    lineptr = buffer;
	    while (isspace(*lineptr)) lineptr++;

	    if (tmpnum < 256) {		/* read parameter defaults in order */
               stringpart *newpart;

	       for (i = 0; i < tmpnum; i++) {
		  oparamptr newops;

                  newops = new oparam;
		  newops->next = localdata->params;
		  localdata->params = newops;
		  newops->key = (char *)malloc(6);
		  sprintf(newops->key, "v%d", i + 1);

		  if (*lineptr == '(' || *lineptr == '{') {  /* type is XC_STRING */
		     char *linetmp, csave;
		     
		     newops->parameter.string = NULL;

		     /* get simple substring or set of substrings and commands */
		     linetmp = find_delimiter(lineptr);
		     csave = *(++linetmp);
		     *linetmp = '\0';
		     if (*lineptr == '{') lineptr++;
		     readlabel(localdata, lineptr, &(newops->parameter.string));

		     /* Add the ending part to the parameter string */
		     newpart = makesegment(&(newops->parameter.string), NULL);  
		     newpart->type = PARAM_END;
                     newpart->data.string = (char *)NULL;

		     newops->type = (u_char)XC_STRING;
		     newops->which = P_SUBSTRING;
		     /* Fprintf(stdout, "Parameter %d to object %s defaults "
				"to string \"%s\"\n", i + 1, localdata->name,
				ops->parameter.string); */
		     *linetmp = csave;
		     lineptr = linetmp;
		     while (isspace(*lineptr)) lineptr++;
		  }
		  else {	/* type is assumed to be XC_FLOAT */
		     newops->type = (u_char)XC_FLOAT;
	             sscanf(lineptr, "%g", &newops->parameter.fvalue);
		     /* Fprintf(stdout, "Parameter %s to object %s defaults to "
				"value %g\n", newops->key, localdata->name,
				newops->parameter.fvalue); */
		     lineptr = advancetoken(lineptr);
		  }
	       }
	    }
	 }
	 else if (!strcmp(keyword, "nonetwork")) {
	    localdata->valid = true;
	    localdata->schemtype = NONETWORK;
	 }
	 else if (!strcmp(keyword, "trivial")) {
	    localdata->schemtype = TRIVIAL;
	 }
         else if (!strcmp(keyword, "begingate")) {
	    localdata->params = NULL;
	    /* read dictionary of parameter key:value pairs */
	    readparams(NULL, NULL, localdata, buffer);
	 }

         else if (!strcmp(keyword, "%%Trailer")) break;
         else if (!strcmp(keyword, "EndLib")) break;
	 else if (!strcmp(keyword, "restore"));    /* handled at top */
	 else if (!strcmp(keyword, "grestore"));   /* ignore */
         else if (!strcmp(keyword, "endgate"));    /* also ignore */
	 else if (!strcmp(keyword, "xyarray"));	   /* ignore for now */
         else {
	    char *tmpptr, *libobjname;
	    bool matchtech, found = false;

	    /* First, make sure this is not a general comment line */
	    /* Return if we have a page boundary	   	   */
	    /* Read an image if this is an imagedata line.	   */

	    for (tmpptr = buffer; isspace(*tmpptr); tmpptr++);
	    if (*tmpptr == '%') {
	       if (strstr(buffer, "%%Page:") == tmpptr) {
                  strncpy(retstr, buffer, 150);
                  retstr[149] = '\0';
		  free (buffer);
		  return true;
	       }
	       else if (strstr(buffer, "%imagedata") == tmpptr) {
		  int width, height;
		  sscanf(buffer, "%*s %d %d", &width, &height);
		  readimagedata(ps, width, height);
	       }
	       continue;
	    }

            parse_ps_string(keyword, keyword, 79, false, false);
            matchtech =  (strstr(keyword, "::") == NULL) ? false : true;

	    /* (Assume that this line calls an object instance) */
	    /* Double loop through user libraries 		*/

            for (k = 0; k < ((mode == FONTLIB) ? 1 : xobjs.numlibs); k++) {
	       for (j = 0; j < ((mode == FONTLIB) ? xobjs.fontlib.number :
			xobjs.userlibs[k].number); j++) {

		  libobj = (mode == FONTLIB) ? xobjs.fontlib.library + j :
			xobjs.userlibs[k].library + j;

		  /* Objects which have a technology ("<lib>::<obj>")	*/
		  /* must compare exactly.  Objects which don't	 will	*/
		  /* match any object of the same name in any library	*/
		  /* technology.					*/

		  libobjname = (*libobj)->name;
		  if (!matchtech) {
		     char *objnamestart = strstr(libobjname, "::");
		     if (objnamestart != NULL) libobjname = objnamestart + 2;
		  }
	          if (!objnamecmp(keyword, libobjname)) {

		     /* If the name is not exactly the same (appended underscores) */
		     /* check if the name is on the list of aliases. */

		     if (strcmp(keyword, libobjname)) {
			bool is_alias = false;
			aliasptr ckalias = aliastop;
			slistptr sref;

			for (; ckalias != NULL; ckalias = ckalias->next) {
			   if (ckalias->baseobj == (*libobj)) {
			      sref = ckalias->aliases;
			      for (; sref != NULL; sref = sref->next) {
			         if (!strcmp(keyword, sref->alias)) {
				    is_alias = true;
				    break;
				 }
			      }
			      if (is_alias) break;
			   }
			}
			if (!is_alias) continue;
		     }

		     found = true;
                     newinst = localdata->append(new objinst);
		     (*newinst)->thisobject = *libobj;
                     (*newinst)->color = curcolor;
                     (*newinst)->bbox.lowerleft = (*libobj)->bbox.lowerleft;
                     (*newinst)->bbox = (*libobj)->bbox;

	             lineptr = varfscan(localdata, buffer, &(*newinst)->scale,
                                *newinst, P_SCALE);
	       	     lineptr = varscan(localdata, lineptr, &(*newinst)->rotation,
                                *newinst, P_ROTATION);
	             lineptr = varpscan(localdata, lineptr, &(*newinst)->position.x,
                                *newinst, 0, offx, P_POSITION_X);
	             lineptr = varpscan(localdata, lineptr, &(*newinst)->position.y,
                                *newinst, 0, offy, P_POSITION_Y);

		     /* Negative rotations = flip in x in version 2.3.6 and    */
		     /* earlier.  Later versions don't allow negative rotation */

	    	     if (version < 2.4) {
                        if ((*newinst)->rotation < 0) {
			   (*newinst)->scale = -((*newinst)->scale);
			   (*newinst)->rotation++;
		        }
		        (*newinst)->rotation = -(*newinst)->rotation;
		     }

                     while ((*newinst)->rotation > 360) (*newinst)->rotation -= 360;
                     while ((*newinst)->rotation < 0) (*newinst)->rotation += 360;

                     std_eparam(*newinst, colorkey);

		     /* Does this instance contain parameters? */
		     readparams(localdata, *newinst, *libobj, buffer);

		     calcbboxinst(*newinst);

		     break;

	          } /* if !strcmp */
	       } /* for j loop */
	       if (found) break;
	    } /* for k loop */
	    if (!found)		/* will assume that we have a continuation line */
	       temp = continueline(&buffer);
         }
      }
   }
   strncpy(retstr, buffer, 150);
   retstr[149] = '\0';
   free(buffer);
   return true;
}

/*------------------------*/
/* Save a PostScript file */
/*------------------------*/

#ifdef TCL_WRAPPER

void setfile(char *filename, int mode)
{
   if ((filename == NULL) || (xobjs.pagelist[areawin->page]->filename == NULL)) {
      Wprintf("Error: No filename for schematic.");
      if (beeper) XBell(100);
      return;
   }

   /* see if name has been changed in the buffer */

   if (strcmp(xobjs.pagelist[areawin->page]->filename, filename)) {
      Wprintf("Changing name of edit file.");
      free(xobjs.pagelist[areawin->page]->filename);
      xobjs.pagelist[areawin->page]->filename = strdup(filename);
   }

   if (strstr(xobjs.pagelist[areawin->page]->filename, "Page ") != NULL) {
      Wprintf("Warning: Enter a new name.");
      if (beeper) XBell(100);
   }
   else {
      savefile(mode); 
      if (beeper) XBell(100);
   }
}

#else  /* !TCL_WRAPPER */

void setfile(Widget button, Widget fnamewidget, caddr_t)
{
   /* see if name has been changed in the buffer */

   char str2[250];
   sprintf(str2, "%.249s", (char *)XwTextCopyBuffer((XwTextEditWidget)fnamewidget));
   if (xobjs.pagelist[areawin->page].filename.isEmpty()) {
       xobjs.pagelist[areawin->page].filename = str2;

   }
   else if (xobjs.pagelist[areawin->page].filename != QString::fromLocal8Bit(str2)) {
      Wprintf("Changing name of edit file.");
      xobjs.pagelist[areawin->page].filename = QString::fromLocal8Bit(str2);
   }
   if (xobjs.pagelist[areawin->page].filename.contains("Page ")) {
      Wprintf("Warning: Enter a new name.");
      if (beeper) XBell(100);
   }
   else {
      Arg wargs[1];
      Widget db, di;

      savefile(CURRENT_PAGE); 

      /* Change "close" button to read "done" */

      di = XtParent(button);
      db = XtNameToWidget(di, "Close");
      XtSetArg(wargs[0], XtNlabel, "  Done  ");
      XtSetValues(db, wargs, 1);
      if (beeper) XBell(100);
   }
}

#endif  /* TCL_WRAPPER */

/*--------------------------------------------------------------*/
/* Update number of changes for an object and initiate a temp	*/
/* file save if total number of unsaved changes exceeds 20.	*/
/*--------------------------------------------------------------*/

void incr_changes(objectptr thisobj)
{
   /* It is assumed that empty pages are meant to be that way */
   /* and are not to be saved, so changes are marked as zero. */

   if (thisobj->parts == 0) {
      thisobj->changes = 0;
      return;
   }

   /* Remove any pending timeout */

   if (xobjs.timeout_id != 0) {
      xcRemoveTimeout(xobjs.timeout_id);
      xobjs.timeout_id = 0;
   }

   thisobj->changes++;

   /* When in "suspend" mode, we assume that we are reading commands from
    * a script, and therefore we should not continuously increment changes
    * and keep saving the backup file.
    */

   if (xobjs.suspend < 0)
      xobjs.new_changes++;

   if (xobjs.new_changes > MAXCHANGES) {
#ifdef TCL_WRAPPER
      savetemp(NULL);
#else
      savetemp(NULL, NULL);
#endif
   }

   /* Generate a new timeout */

   xobjs.timeout_id = xcAddTimeout(app, 60000 * xobjs.save_interval,
 		savetemp, NULL);
}

/*--------------------------------------------------------------*/
/* tempfile save						*/
/*--------------------------------------------------------------*/

#ifdef TCL_WRAPPER

void savetemp(ClientData clientdata)
{

#else 

void savetemp(XtPointer, XtIntervalId *)
{

#endif

   /* Remove the timeout ID.  If this routine is called from	*/
   /* incr_changes() instead of from the timeout interrupt	*/
   /* service routine, then this value will already be NULL.	*/

   xobjs.timeout_id = (XtIntervalId)NULL;

   /* First see if there are any unsaved changes in the file.	*/
   /* If not, then just reset the counter and continue.  	*/

   if (xobjs.new_changes > 0) {
      if (xobjs.tempfile == NULL)
      {
         QString atemplate = QString("%1/XC%2.XXXXXX").arg(xobjs.tempdir).arg(getpid());
         QTemporaryFile temp(atemplate);
         if (!temp.open()) {
             Fprintf(stderr, "Error generating file for savetemp\n");
         }
         else {
             xobjs.tempfile = temp.fileName();
         }
      }
      /* Show the "wristwatch" cursor, as graphic data may cause this	*/
      /* step to be inordinately long.					*/

      XDefineCursor(areawin->viewport, WAITFOR);
      savefile(ALL_PAGES);
      XDefineCursor(areawin->viewport, DEFAULTCURSOR);
      xobjs.new_changes = 0;	/* reset the count */
   }
}

/*----------------------------------------------------------------------*/
/* Set all objects in the list "wroteobjs" as having no unsaved changes */
/*----------------------------------------------------------------------*/

void setassaved(objectptr *wroteobjs, short written)
{
   int i;

   for (i = 0; i < written; i++)
      (*(wroteobjs + i))->changes = 0;
}

/*---------------------------------------------------------------*/
/* Save indicated library.  If libno is 0, save current page if	 */
/* the current page is a library.  If not, save the user library */
/*---------------------------------------------------------------*/

void savelibpopup(QAction* a, pointertype technology, pointertype)
{
   TechPtr nsptr;

   nsptr = LookupTechnology((char*)technology);

   if (nsptr != NULL) {
      if ((nsptr->flags & LIBRARY_READONLY) != 0) {
         Wprintf("Library technology \"%s\" is read-only.", technology);
         return;
      }
   }

   popupQuestion(a, "Enter technology:", "\0", savelibrary, technology);
}

/*---------------------------------------------------------*/

void savelibrary(QAction*, const QString & name, void *technology)
{
   savetechnology((char*)technology, name.toLocal8Bit().data());
}

/*---------------------------------------------------------*/

void savetechnology(char *technology, char *outname)
{
   FILE *ps;
   QString outfile;
   char *outptr, *validname;
   objectptr *wroteobjs, libobjptr, *optr, depobj;
   liblistptr spec;
   short written;
   char *uname = NULL;
   char *hostname = NULL;
   struct passwd *mypwentry = NULL;
   int i, j, ilib;
   TechPtr nsptr;

   nsptr = LookupTechnology(technology);

   if (nsptr != NULL) {
      if ((nsptr->flags & LIBRARY_READONLY) != 0) {
         Wprintf("Library technology \"%s\" is read-only.", technology);
         return;
      }
   }

   if ((outptr = strrchr(outname, '/')) == NULL)
      outptr = outname;
   else
      outptr++;
   outfile = outname;
   if (strchr(outptr, '.') == NULL) outfile += ".lps";

   xc_tilde_expand(outfile);
   while (xc_variable_expand(outfile));

   ps = fopen(outfile.toLocal8Bit(), "w");
   if (ps == NULL) {
      Wprintf("Can't open PS file.");
      return;
   }

   fprintf(ps, "%%! PostScript set of library objects for XCircuit\n");
   fprintf(ps, "%%  Version: %2.1f\n", version);
   fprintf(ps, "%%  Library name is: %s\n",
		(technology == NULL) ? "(user)" : technology);
#ifdef _MSC_VER
   uname = getenv((const char *)"USERNAME");
#else
   uname = getenv((const char *)"USER");
   if (uname != NULL) mypwentry = getpwnam(uname);
#endif

   /* Check for both $HOST and $HOSTNAME environment variables.  Thanks */
   /* to frankie liu <frankliu@Stanford.EDU> for this fix.		*/
   
   if ((hostname = getenv((const char *)"HOSTNAME")) == NULL)
      if ((hostname = getenv((const char *)"HOST")) == NULL) {
	 if (gethostname(_STR, 149) != 0)
	    hostname = uname;
	 else
	    hostname = _STR;
      }

#ifndef _MSC_VER
   if (mypwentry != NULL)
      fprintf(ps, "%%  Author: %s <%s@%s>\n", mypwentry->pw_gecos, uname,
		hostname);
#endif

   fprintf(ps, "%%\n\n");

   /* Print lists of object dependencies, where they exist.		*/
   /* Note that objects can depend on objects in other technologies;	*/
   /* this is allowed.							*/

   wroteobjs = (objectptr *) malloc(sizeof(objectptr));
   for (ilib = 0; ilib < xobjs.numlibs; ilib++) {
      for (j = 0; j < xobjs.userlibs[ilib].number; j++) {

	 libobjptr = *(xobjs.userlibs[ilib].library + j);
         if (CompareTechnology(libobjptr, technology)) {
	    written = 0;

	    /* Search for all object definitions instantiated in this object, */
	    /* and add them to the dependency list (non-recursive).	   */

            for (objinstiter oiptr; libobjptr->values(oiptr); ) {
              depobj = oiptr->thisobject;

              /* Search among the list of objects already collected.	*/
              /* If this object has been previously, then ignore it.	*/
              /* Otherwise, update the list of object dependencies	*/

              for (optr = wroteobjs; optr < wroteobjs + written; optr++)
                 if (*optr == depobj)
                    break;

              if (optr == wroteobjs + written) {
                 wroteobjs = (objectptr *)realloc(wroteobjs, (written + 1) *
                            sizeof(objectptr));
                 *(wroteobjs + written) = depobj;
                 written++;
             }
	    }
	    if (written > 0) {
	       fprintf(ps, "%% Depend %s", libobjptr->name);
	       for (i = 0; i < written; i++) {
	          depobj = *(wroteobjs + i);
	          fprintf(ps, " %s", depobj->name);
	       }
	       fprintf(ps, "\n");
	    }
         }
      }
   }

   fprintf(ps, "\n%% XCircuitLib library objects\n");

   /* list of library objects already written */

   wroteobjs = (objectptr *)realloc(wroteobjs, sizeof(objectptr));
   written = 0;

   /* write all of the object definitions used, bottom up, with virtual	*/
   /* instances in the correct placement.  The need to find virtual	*/
   /* instances is why we do a look through the library pages and not	*/
   /* the libraries themselves when looking for objects matching the	*/
   /* given technology.							*/

   for (ilib = 0; ilib < xobjs.numlibs; ilib++) {
      for (spec = xobjs.userlibs[ilib].instlist; spec != NULL; spec = spec->next) {
	 libobjptr = spec->thisinst->thisobject;
         if (CompareTechnology(libobjptr, technology)) {
	    if (!spec->isvirtual) {
               printobjects(ps, spec->thisinst->thisobject, &wroteobjs,
			&written, DEFAULTCOLOR);
      	    }
            else {
	       if ((spec->thisinst->scale != 1.0) || (spec->thisinst->rotation != 0)) {
	    	  fprintf(ps, "%3.3f %d ", spec->thisinst->scale,
				spec->thisinst->rotation);
	       }
               printparams(ps, spec->thisinst, 0);
               validname = create_valid_psname(spec->thisinst->thisobject->name, false);
	       /* Names without technologies get '::' string (blank technology) */
	       if (technology == NULL)
                  fprintf(ps, "/::%s libinst\n", validname);
	       else
                  fprintf(ps, "/%s libinst\n", validname);
	       if ((spec->next != NULL) && (!(spec->next->isvirtual)))
	          fprintf(ps, "\n");
            }
         }
      }
   }

   setassaved(wroteobjs, written);
   if (nsptr) nsptr->flags &= (~LIBRARY_CHANGED);
   xobjs.new_changes = countchanges(NULL);

   /* and the postlog */

   fprintf(ps, "\n%% EndLib\n");
   fclose(ps);
   if (technology != NULL)
      Wprintf("Library technology \"%s\" saved as file %s.",technology, outname);
   else
      Wprintf("Library technology saved as file %s.", outname);

   free(wroteobjs);
}

/*----------------------------------------------------------------------*/
/* Recursive routine to search the object hierarchy for fonts used	*/
/*----------------------------------------------------------------------*/

void findfonts(objectptr writepage, short *fontsused) {
   genericptr *dfp;
   stringpart *chp;
   int findex;

   for (dfp = writepage->begin(); dfp != writepage->end(); dfp++) {
      if (IS_LABEL(*dfp)) {
         for (chp = TOLABEL(dfp)->string; chp != NULL; chp = chp->nextpart) {
	    if (chp->type == FONT_NAME) {
	       findex = chp->data.font;
	       if (fontsused[findex] == 0) {
	          fontsused[findex] = 0x8000 | fonts[findex].flags;
	       }
	    }
	 }
      }
      else if (IS_OBJINST(*dfp)) {
	 findfonts(TOOBJINST(dfp)->thisobject, fontsused);
      }
   }
}

/*----------------------------------------------------------------------*/
/* Main file saving routine						*/
/*----------------------------------------------------------------------*/
/*	mode 		description					*/
/*----------------------------------------------------------------------*/
/*	ALL_PAGES	saves a crash recovery backup file		*/
/*	CURRENT_PAGE	saves all pages associated with the same	*/
/*			filename as the current page, and all		*/
/*			dependent schematics (which have their		*/
/*			filenames changed to match).			*/
/*	NO_SUBCIRCUITS	saves all pages associated with the same	*/
/*			filename as the current page, only.		*/
/*----------------------------------------------------------------------*/

void savefile(short mode) 
{
   FILE *ps, *pro;
   QString fname, outname, basename;
   char temp[150], prologue[150], ascbuf[6];
   short written, fontsused[256], i, page, curpage, multipage;
   short savepage, stcount, *pagelist, *glist;
   objectptr *wroteobjs;
   objinstptr writepage;
   int findex, j;
   time_t tdate;
   char *tmp_s;

   if (mode != ALL_PAGES) {
      /* doubly-protected file write: protect against errors during file write */
      fname = xobjs.pagelist[areawin->page].filename;
      outname = fname + "~";
      QFile::rename(fname, outname);
   }
   else {
      /* doubly-protected backup: protect against errors during file write */
      outname = xobjs.tempfile + "B";
      QFile::rename(xobjs.tempfile, outname);
      fname = xobjs.tempfile;
   }

   QFileInfo fileinfo(fname);
   basename = fileinfo.fileName();

   if ((mode != ALL_PAGES) && ! basename.contains("."))
      outname = fname + ".ps";
   else outname = fname;

   xc_tilde_expand(outname);
   while(xc_variable_expand(outname));

   ps = fopen(outname.toLocal8Bit(), "w");
   if (ps == NULL) {
      Wprintf("Can't open file %s for writing.", outname.toLocal8Bit().data());
      return;
   }

   if ((mode != NO_SUBCIRCUITS) && (mode != ALL_PAGES))
      collectsubschems(areawin->page);

   /* Check for multiple-page output: get the number of pages;	*/
   /* ignore empty pages.					*/

   multipage = 0;

   if (mode == NO_SUBCIRCUITS)
      pagelist = pagetotals(areawin->page, INDEPENDENT);
   else if (mode == ALL_PAGES)
      pagelist = pagetotals(areawin->page, ALL_PAGES);
   else
      pagelist = pagetotals(areawin->page, TOTAL_PAGES);

   for (page = 0; page < xobjs.pagelist.count(); page++)
      if (pagelist[page] > 0)
	  multipage++;

   if (multipage == 0) {
      Wprintf("Panic:  could not find this page in page list!");
      free (pagelist);
      fclose(ps);
      return;
   }

   /* Print the PostScript DSC Document Header */

   fprintf(ps, "%%!PS-Adobe-3.0");
   if (multipage == 1 && !(xobjs.pagelist[areawin->page].pmode & 1))
      fprintf(ps, " EPSF-3.0\n");
   else
      fprintf(ps, "\n");
   fprintf(ps, "%%%%Title: %s\n", basename.toLocal8Bit().data());
   fprintf(ps, "%%%%Creator: XCircuit v%2.1f rev%d\n", PROG_VERSION, PROG_REVISION);
   tdate = time(NULL);
   fprintf(ps, "%%%%CreationDate: %s", asctime(localtime(&tdate)));
   fprintf(ps, "%%%%Pages: %d\n", multipage);

   /* This is just a default value; each bounding box is declared per	*/
   /* page by the DSC "PageBoundingBox" keyword.			*/
   /* However, encapsulated files adjust the bounding box to center on	*/
   /* the object, instead of centering the object on the page.		*/

   if (multipage == 1 && !(xobjs.pagelist[areawin->page].pmode & 1)) {
      objectptr thisobj = xobjs.pagelist[areawin->page].pageinst->thisobject;
      float psscale = getpsscale(xobjs.pagelist[areawin->page].outscale,
			areawin->page);

      /* The top-level bounding box determines the size of an encapsulated */
      /* drawing, regardless of the PageBoundingBox numbers.  Therefore,   */
      /* we size this to bound just the picture by closing up the 1" (72   */
      /* PostScript units) margins, except for a tiny sliver of a margin   */
      /* (4 PostScript units) which covers a bit of sloppiness in the font */
      /* measurements.							   */
	
      fprintf(ps, "%%%%BoundingBox: 68 68 %d %d\n",
	 (int)((float)thisobj->bbox.width * psscale)
                        + xobjs.pagelist[areawin->page].margins.x + 4,
	 (int)((float)thisobj->bbox.height * psscale)
                        + xobjs.pagelist[areawin->page].margins.y + 4);
   }
   else if (xobjs.pagelist[0].coordstyle == CM)
      fprintf(ps, "%%%%BoundingBox: 0 0 595 842\n");  /* A4 default */
   else
      fprintf(ps, "%%%%BoundingBox: 0 0 612 792\n");  /* letter default */

   for (i = 0; i < fontcount; i++) fontsused[i] = 0;
   fprintf(ps, "%%%%DocumentNeededResources: font ");
   stcount = 32;

   /* find all of the fonts used in this document */
   /* log all fonts which are native PostScript   */

   for (curpage = 0; curpage < xobjs.pagelist.count(); curpage++)
      if (pagelist[curpage] > 0) {
         writepage = xobjs.pagelist[curpage].pageinst;
         findfonts(writepage->thisobject, fontsused);
      }
      
   for (i = 0; i < fontcount; i++) {
      if (fontsused[i] & 0x8000)
	 if ((fonts[i].flags & 0x8018) == 0x0) {
	    stcount += strlen(fonts[i].psname) + 1;
	    if (stcount > OUTPUTWIDTH) {
	       stcount = strlen(fonts[i].psname) + 11;
	       fprintf(ps, "\n%%%%+ font ");
	    }
	    fprintf(ps, "%s ", fonts[i].psname);
	 }
   }

   fprintf(ps, "\n%%%%EndComments\n");

   tmp_s = getenv((const char *)"XCIRCUIT_LIB_DIR");
   if (tmp_s != NULL) {
      sprintf(prologue, "%s/%s", tmp_s, PROLOGUE_FILE);
      pro = fopen(prologue, "r");
   }
   else
      pro = NULL;

   if (pro == NULL) {
      sprintf(prologue, "%s/%s", PROLOGUE_DIR, PROLOGUE_FILE);
      pro = fopen(prologue, "r");
      if (pro == NULL) {
         sprintf(prologue, "%s", PROLOGUE_FILE);
         pro = fopen(prologue, "r");
         if (pro == NULL) {
            Wprintf("Can't open prolog.");
	    free(pagelist);
	    fclose(ps);
            return;
	 }
      }
   }

   /* write the prolog to the output */

   for(;;) {
      if (fgets(temp, 149, pro) == NULL) break;
      fputs(temp, ps);
   }
   fclose(pro);

   /* Special font handling */

   for (findex = 0; findex < fontcount; findex++) {

      /* Derived font slant */

      if ((fontsused[findex] & 0x032) == 0x032)
         fprintf(ps, "/%s /%s .167 fontslant\n\n",
		fonts[findex].psname, fonts[findex].family);

      /* Derived ISO-Latin1 encoding */

      if ((fontsused[findex] & 0xf80) == 0x100) {
	 char *fontorig = NULL;
	 short i;
	 /* find the original standard-encoded font (can be itself) */
	 for (i = 0; i < fontcount; i++) {
	    if (i == findex) continue;
	    if (!strcmp(fonts[i].family, fonts[findex].family) &&
		 ((fonts[i].flags & 0x03) == (fonts[findex].flags & 0x03)))
	       fontorig = fonts[i].psname;
	    if (fontorig == NULL) fontorig = fonts[findex].psname;
	 }
	 fprintf(ps, "/%s findfont dup length dict begin\n", fontorig);
	 fprintf(ps, "{1 index /FID ne {def} {pop pop} ifelse} forall\n");
	 fprintf(ps, "/Encoding ISOLatin1Encoding def currentdict end\n");
	 fprintf(ps, "/%s exch definefont pop\n\n", fonts[findex].psname);
      }

      /* To do:  ISO-Latin2 encoding */

      if ((fontsused[findex] & 0xf80) == 0x180) {
      }

      /* To do:  Special encoding */

      if ((fontsused[findex] & 0xf80) == 0x80) {
      }

      /* To do:  Vectored (drawn) font */

      if (fontsused[findex] & 0x8) {
      }
   }

   /* List of objects already written */
   wroteobjs = (objectptr *) malloc (sizeof(objectptr));
   written = 0;

   fprintf(ps, "%% XCircuit output starts here.\n\n");
   fprintf(ps, "%%%%BeginSetup\n\n");

   /* Write out all of the images used */

   glist = collect_graphics(pagelist);

   for (i = 0; i < xobjs.images; i++) {
      Imagedata *img = xobjs.imagelist + i;
      int ilen, flen, k, m = 0, n, q = 0;
      u_char *filtbuf, *flatebuf;
      bool lastpix = false;
      union {
	u_long i;
	u_char b[4];
      } pixel;

      if (glist[i] == 0) continue;

      fprintf(ps, "%%imagedata %d %d\n", img->image->width(), img->image->height());
      fprintf(ps, "currentfile /ASCII85Decode filter ");

#ifdef HAVE_LIBZ
      fprintf(ps, "/FlateDecode filter\n");
#endif

      fprintf(ps, "/ReusableStreamDecode filter\n");

      /* creating a stream buffer is wasteful if we're just using ASCII85	*/
      /* decoding but is a must for compression filters. 			*/

      ilen = 3 * img->image->width() * img->image->height();
      filtbuf = (u_char *)malloc(ilen + 4);
      q = 0;
      for (j = 0; j < img->image->height(); j++) {
         for (k = 0; k < img->image->width(); k++) {
            QRgb pixel = img->image->pixel(k, j);
            filtbuf[q++] = qRed(pixel);
            filtbuf[q++] = qGreen(pixel);
            filtbuf[q++] = qBlue(pixel);
	 }
      }
      for (j = 0; j < 4; j++)
	 filtbuf[q++] = 0;

      /* Extra encoding goes here */
#ifdef HAVE_LIBZ
      flen = ilen * 2;
      flatebuf = (u_char *)malloc(flen);
      ilen = large_deflate(flatebuf, flen, filtbuf, ilen);
      free(filtbuf);
#else
      flatebuf = filtbuf;
#endif
	    
      ascbuf[5] = '\0';
      for (j = 0; j < ilen; j += 4) {
         if ((j + 4) > ilen) lastpix = true;
	 if (!lastpix && (flatebuf[j] + flatebuf[j + 1] + flatebuf[j + 2]
			+ flatebuf[j + 3] == 0)) {
	    fprintf(ps, "z");
	    m++;
	 }
	 else {
	    for (n = 0; n < 4; n++)
	       pixel.b[3 - n] = flatebuf[j + n];

	    ascbuf[0] = '!' + (pixel.i / 52200625);
	    pixel.i %= 52200625;
	    ascbuf[1] = '!' + (pixel.i / 614125);
	    pixel.i %= 614125;
	    ascbuf[2] = '!' + (pixel.i / 7225);
	    pixel.i %= 7225;
	    ascbuf[3] = '!' + (pixel.i / 85);
	    pixel.i %= 85;
	    ascbuf[4] = '!' + pixel.i;
	    if (lastpix)
	       for (n = 0; n < ilen + 1 - j; n++)
	          fprintf(ps, "%c", ascbuf[n]);
	    else
	       fprintf(ps, "%5s", ascbuf);
	    m += 5;
	 }
	 if (m > 75) {
	    fprintf(ps, "\n");
	    m = 0;
	 }
      }
      fprintf(ps, "~>\n");
      free(flatebuf);

      /* Remove any filesystem path information from the image name.	*/
      /* Otherwise, the slashes will cause PostScript to err.		*/

      char * fptr = strrchr(img->filename, '/');
      if (fptr == NULL)
	 fptr = img->filename;
      else
	 fptr++;
      fprintf(ps, "/%sdata exch def\n", fptr);
      fprintf(ps, "/%s <<\n", fptr);
      fprintf(ps, "  /ImageType 1 /Width %d /Height %d /BitsPerComponent 8\n",
                img->image->width(), img->image->height());
      fprintf(ps, "  /MultipleDataSources false\n");
      fprintf(ps, "  /Decode [0 1 0 1 0 1]\n");
      fprintf(ps, "  /ImageMatrix [1 0 0 -1 %d %d]\n",
                (img->image->width() >> 1), (img->image->height() >> 1));
      fprintf(ps, "  /DataSource %sdata >> def\n\n", fptr);
   }
   free(glist);

   for (curpage = 0; curpage < xobjs.pagelist.count(); curpage++) {
      if (pagelist[curpage] == 0) continue;

      /* Write all of the object definitions used, bottom up */
      printrefobjects(ps, xobjs.pagelist[curpage].pageinst->thisobject,
		&wroteobjs, &written);
   }

   fprintf(ps, "\n%%%%EndSetup\n\n");

   page = 0;
   for (curpage = 0; curpage < xobjs.pagelist.count(); curpage++) {
      if (pagelist[curpage] == 0) continue;

      /* Print the page header, all elements in the page, and page trailer */
      savepage = areawin->page;
      /* Set the current page for the duration of printing so that any	*/
      /* page parameters will be printed correctly.			*/
      areawin->page = curpage;
      printpageobject(ps, xobjs.pagelist[curpage].pageinst->thisobject,
		++page, curpage);
      areawin->page = savepage;

      /* For crash recovery, log the filename for each page */
      if (mode == ALL_PAGES) {
         fprintf(ps, "%% %s is_filename\n",
                (xobjs.pagelist[curpage].filename == NULL) ?
                xobjs.pagelist[curpage].pageinst->thisobject->name :
                xobjs.pagelist[curpage].filename.toLocal8Bit().data());
      }

      fprintf(ps, "\n");
      fflush(ps);
   }

   /* For crash recovery, save all objects that have been edited but are */
   /* not in the list of objects already saved.				 */

   if (mode == ALL_PAGES)
   {
      int i, j, k;
      objectptr thisobj;

      for (i = 0; i < xobjs.numlibs; i++) {
	 for (j = 0; j < xobjs.userlibs[i].number; j++) {
	    thisobj = *(xobjs.userlibs[i].library + j);
	    if (thisobj->changes > 0 ) {
	       for (k = 0; k < written; k++)
	          if (thisobj == *(wroteobjs + k)) break;
	       if (k == written)
      	          printobjects(ps, thisobj, &wroteobjs, &written, DEFAULTCOLOR);
	    }
	 }
      }
   }
   else {	/* No unsaved changes in these objects */
      setassaved(wroteobjs, written);
      for (i = 0; i < xobjs.pagelist.count(); i++)
	 if (pagelist[i] > 0)
            xobjs.pagelist[i].pageinst->thisobject->changes = 0;
      xobjs.new_changes = countchanges(NULL);
   }

   /* Free allocated memory */
   free(pagelist);
   free(wroteobjs);

   /* Done! */

   fprintf(ps, "%%%%Trailer\n");
   fprintf(ps, "XCIRCsave restore\n");
   fprintf(ps, "%%%%EOF\n");
   fclose(ps);

   Wprintf("File %ls saved (%d page%s).", fname.utf16(), multipage,
		(multipage > 1 ? "s" : ""));

   if (mode == ALL_PAGES) {
      /* Remove the temporary redundant backup */
      QFile::remove(xobjs.tempfile + 'B');
   }
   else if (!xobjs.retain_backup) {
      /* Remove the backup file */
      QFile::remove(fname + '~');
   }

   /* Write LATEX strings, if any are present */
   TopDoLatex();
}

/*----------------------------------------------------------------------*/
/* Given a color value, print the R, G, B values			*/
/*----------------------------------------------------------------------*/

int printRGBvalues(char *tstr, int value, const char *postfix)
{
   int i = colorlist.indexOf(value);
   if (i >= 0) {
        sprintf(tstr, "%4.3f %4.3f %4.3f %s",
               (float)qRed(colorlist[i])   / 255.0,
               (float)qGreen(colorlist[i]) / 255.0,
               (float)qBlue(colorlist[i])  / 255.0,
               postfix);
        return 0;
   }

   /* The program can reach this point for any color which is	*/
   /* not listed in the table.  This can happen when parameters	*/
   /* printed from printobjectparams object contain the string	*/
   /* "@p_color".  Therefore print the default top-level	*/
   /* default color, which is black.				*/

   /* If the index is *not* DEFAULTCOLOR (-1), return an error	*/
   /* code.							*/

   sprintf(tstr, "0 0 0 %s", postfix);
   return (value == DEFAULTCOLOR) ? 0 : -1;
}

/*----------------------------------------------------*/
/* Write string to PostScript string, ignoring NO_OPs */
/*----------------------------------------------------*/

char *nosprint(char *sptr)
{
   int qtmp, slen = 100;
   char *pptr, *qptr, *bptr;

   bptr = (char *)malloc(slen);	/* initial length 100 */
   qptr = bptr;

   *qptr++ = '(';

   /* Includes extended character set (non-ASCII) */

   for (pptr = sptr; pptr && *pptr != '\0'; pptr++) {
      /* Ensure enough space for the string, including everything */
      /* following the "for" loop */
      qtmp = qptr - bptr;
      if (qtmp + 7 >= slen) {
	 slen += 7;
	 bptr = (char *)realloc(bptr, slen);
	 qptr = bptr + qtmp;
      }

      /* Deal with non-printable characters and parentheses */
      if (*pptr > (char)126) {
	 sprintf(qptr, "\\%3o", (int)(*pptr));
	 qptr += 4; 
      }
      else {
         if ((*pptr == '(') || (*pptr == ')') || (*pptr == '\\'))
	    *qptr++ = '\\';
         *qptr++ = *pptr;
      }
   }
   if (qptr == bptr + 1) {	/* Empty string gets a NULL result, not "()" */
      qptr--;
   }
   else {
      *qptr++ = ')';
      *qptr++ = ' ';
   }
   *qptr++ = '\0';

   return (char *)bptr;
}

/*--------------------------------------------------------------*/
/* Write label segments to the output (in reverse order)	*/
/*--------------------------------------------------------------*/

short writelabel(FILE *ps, stringpart *chrtop, short *stcount)
{
   short i, segs = 0;
   stringpart *chrptr;
   char **ostr = (char **)malloc(sizeof(char *));
   char *tmpstr;
   float lastscale = 1.0;
   int lastfont = -1;

   /* Write segments into string array, in forward order */

   for (chrptr = chrtop; chrptr != NULL; chrptr = chrptr->nextpart) {
      ostr = (char **)realloc(ostr, (segs + 1) * sizeof(char *));
      if (chrtop->type == PARAM_END) {	/* NULL parameter is empty string */
	 ostr[segs] = (char *)malloc(4);
	 strcpy(ostr[segs], "() ");
      }
      else {
	 tmpstr = writesegment(chrptr, &lastscale, &lastfont);
	 if (tmpstr[0] != '\0')
            ostr[segs] = tmpstr;
	 else
	    segs--;
      }
      segs++;
   }

   /* Write string array to output in reverse order */
   for (i = segs - 1; i >= 0; i--) {
      dostcount(ps, stcount, strlen(ostr[i]));
      fputs(ostr[i], ps);
      free(ostr[i]);
   }
   free(ostr);	 

   return segs;
}

/*--------------------------------------------------------------*/
/* Write a single label segment to the output			*/
/* (Recursive, so we can write segments in the reverse order)	*/
/*--------------------------------------------------------------*/

char *writesegment(stringpart *chrptr, float *lastscale, int *lastfont)
{
   int type = chrptr->type;
   char *retstr, *validname;

   switch(type) {
      case PARAM_START:
         validname = create_valid_psname(chrptr->data.string, true);
	 sprintf(_STR, "%s ", validname);
	 break;
      case PARAM_END:
	 _STR[0] = '\0';
	 chrptr->nextpart = NULL;
	 break;
      case SUBSCRIPT:
	 sprintf(_STR, "{ss} ");
	 break;
      case SUPERSCRIPT:
	 sprintf(_STR, "{Ss} ");
	 break;
      case NORMALSCRIPT:
	 *lastscale = 1.0;
         sprintf(_STR, "{ns} ");
         break;
      case UNDERLINE:
         sprintf(_STR, "{ul} ");
         break;
      case OVERLINE:
         sprintf(_STR, "{ol} ");
         break;
      case NOLINE:
         sprintf(_STR, "{} ");
         break;
      case HALFSPACE:
         sprintf(_STR, "{hS} ");
         break;
      case QTRSPACE:
	 sprintf(_STR, "{qS} ");
	 break;
      case RETURN:
	 *lastscale = 1.0;
	 sprintf(_STR, "{CR} ");
	 break;
      case TABSTOP:
	 sprintf(_STR, "{Ts} ");
	 break;
      case TABFORWARD:
	 sprintf(_STR, "{Tf} ");
	 break;
      case TABBACKWARD:
	 sprintf(_STR, "{Tb} ");
	 break;
      case FONT_NAME:
	 /* If font specifier is followed by a scale specifier, then	*/
	 /* record the font change but defer the output.  Otherwise,	*/
	 /* output the font record now.					*/

	 if ((chrptr->nextpart == NULL) || (chrptr->nextpart->type != FONT_SCALE))
	 {
	    if (*lastscale == 1.0)
	       sprintf(_STR, "{/%s cf} ", fonts[chrptr->data.font].psname);
	    else
	       sprintf(_STR, "{/%s %5.3f cf} ", fonts[chrptr->data.font].psname,
			*lastscale);
	 }
	 else
	    _STR[0] = '\0';
	 *lastfont = chrptr->data.font;
	 break;
      case FONT_SCALE:
	 if (*lastfont == -1) {
	    Fprintf(stderr, "Warning:  Font may not be the one that was intended.\n");
	    *lastfont = 0;
	 }
	 *lastscale = chrptr->data.scale;
	 sprintf(_STR, "{/%s %5.3f cf} ", fonts[*lastfont].psname, *lastscale);
	 break;
      case FONT_COLOR:
	 strcpy(_STR, "{");
	 if (chrptr->data.color == DEFAULTCOLOR)
	    strcat(_STR, "sce} ");
	 else
	    if (printRGBvalues(_STR + 1,
                    colorlist[chrptr->data.color], "scb} ") < 0)
	       strcat(_STR, "sce} ");
	 break;
      case KERN:
	 sprintf(_STR, "{%d %d Kn} ", chrptr->data.kern[0], chrptr->data.kern[1]);
	 break;
      case TEXT_STRING:
	 /* Everything except TEXT_STRING will always fit in the _STR fixed-length character array. */
	 return nosprint(chrptr->data.string);
   }

   retstr = (char *)malloc(1 + strlen(_STR));
   strcpy(retstr, _STR);
   return retstr;
}

/*--------------------------------------------------------------*/
/* Routine to write all the label segments as stored in _STR	*/
/*--------------------------------------------------------------*/

int writelabelsegs(FILE *ps, short *stcount, stringpart *chrptr)
{
   bool ismultipart;
   int segs;

   if (chrptr == NULL) return 0;

   ismultipart = ((chrptr->nextpart != NULL) &&
	   (chrptr->nextpart->type != PARAM_END)) ? true : false;

   /* If there is only one part, but it is not a string or the	*/
   /* end of a parameter (empty parameter), then set multipart	*/
   /* anyway so we get the double brace {{ }}.	  		*/

   if ((!ismultipart) && (chrptr->type != TEXT_STRING) &&
		(chrptr->type != PARAM_END))
      ismultipart = true;

   /* nextpart is not NULL if there are multiple parts to the string */
   if (ismultipart) {
      fprintf(ps, "{");
      (*stcount)++;
   }
   segs = writelabel(ps, chrptr, stcount);

   if (ismultipart) {
      fprintf(ps, "} ");
      (*stcount) += 2;
   }
   return segs;
}

/*--------------------------------------------------------------*/
/* Write the dictionary of parameters belonging to an object	*/
/*--------------------------------------------------------------*/

void printobjectparams(FILE *ps, objectptr localdata)
{
   int segs;
   short stcount;
   oparamptr ops;
   char *ps_expr, *validkey;
   float fp;

   /* Check for parameters and default values */
   if (localdata->params == NULL) return;

   fprintf(ps, "<<");
   stcount = 2;

   for (ops = localdata->params; ops != NULL; ops = ops->next) {
      validkey = create_valid_psname(ops->key, true);
      fprintf(ps, "/%s ", validkey);
      dostcount (ps, &stcount, strlen(validkey) + 2);

      switch (ops->type) {
	 case XC_EXPR:
	    ps_expr = evaluate_expr(localdata, ops, NULL);
	    if (ops->which == P_SUBSTRING || ops->which == P_EXPRESSION) {
	       dostcount(ps, &stcount, 3 + strlen(ps_expr));
	       fputs("(", ps);
	       fputs(ps_expr, ps);
	       fputs(") ", ps);
	    }
	    else if (ops->which == P_COLOR) {
	       int ccol;
	       /* Write R, G, B components for PostScript */
	       if (sscanf(ps_expr, "%d", &ccol) == 1) {
	          fputs("{", ps);
	          printRGBvalues(_STR, ccol, "} ");
	          dostcount(ps, &stcount, 1 + strlen(_STR));
	          fputs(_STR, ps);
	       }
	       else {
	          dostcount(ps, &stcount, 8);
	          fputs("{0 0 0} ", ps);
	       }
	    }
	    else if (sscanf(ps_expr, "%g", &fp) == 1) {
	       dostcount(ps, &stcount, 1 + strlen(ps_expr));
	       fputs(ps_expr, ps);
	       fputs(" ", ps);
	    }
	    else {	/* Expression evaluates to error in object */
	       dostcount(ps, &stcount, 2);
	       fputs("0 ", ps);
            }
	    dostcount(ps, &stcount, 7 + strlen(ops->parameter.expr));
	    fputs("(", ps);
	    fputs(ops->parameter.expr, ps);
	    fputs(") pop ", ps);
	    free(ps_expr);
	    break;
	 case XC_STRING:
	    segs = writelabelsegs(ps, &stcount, ops->parameter.string);
	    if (segs == 0) {
	       /* When writing object parameters, we cannot allow a */
	       /* NULL value.  Instead, print an empty string ().   */
	       dostcount(ps, &stcount, 3);
	       fputs("() ", ps);
	    }
	    break;
	 case XC_INT:
	    sprintf(_STR, "%d ", ops->parameter.ivalue);
	    dostcount(ps, &stcount, strlen(_STR));
	    fputs(_STR, ps);
	    break;
	 case XC_FLOAT:
	    sprintf(_STR, "%g ", ops->parameter.fvalue);
	    dostcount(ps, &stcount, strlen(_STR));
	    fputs(_STR, ps);
	    break;
      }
   }

   fprintf(ps, ">> ");
   dostcount (ps, &stcount, 3);
}

/*--------------------------------------------------------------*/
/* Write the list of parameters belonging to an object instance */
/*--------------------------------------------------------------*/

short printparams(FILE *ps, objinstptr sinst, short stcount)
{
   short loccount;
   oparamptr ops, objops;
   eparamptr epp;
   char *ps_expr, *validkey, *validref;
   short instances = 0;

   if (sinst->params == NULL) return stcount;

   for (ops = sinst->params; ops != NULL; ops = ops->next) {
      validref = strdup(create_valid_psname(ops->key, true));

      /* Check for indirect parameter references */
      for (epp = sinst->passed; epp != NULL; epp = epp->next) {
	 if ((epp->flags & P_INDIRECT) && (epp->pdata.refkey != NULL)) {
	    if (!strcmp(epp->pdata.refkey, ops->key)) {
	       if (instances++ == 0) {
		  fprintf(ps, "<<");		/* begin PostScript dictionary */
		  loccount = stcount + 2;
	       }
	       dostcount(ps, &loccount, strlen(validref + 3));
	       fprintf(ps, "/%s ", validref);
	       dostcount(ps, &loccount, strlen(epp->key + 1));
               validkey = create_valid_psname(epp->key, true);
	       fprintf(ps, "%s ", validkey);
	       break;
	    }
	 }
      }
      if (epp == NULL) {	/* No indirection */
         bool nondefault = true;
	 char *deflt_expr = NULL;
	
	 /* For instance values that are expression results, ignore if	*/
	 /* the instance value is the same as the default value.	*/
	 /* Correction 9/08:  We can't second-guess expression results,	*/
	 /* in particular, this doesn't work for an expression like	*/
	 /* "page", where the local and default values will evaluate to	*/
	 /* the same result, when clearly each page is intended to have	*/
	 /* its own instance value, and "page" for an object is not a	*/
	 /* well-defined concept.					*/

//	 ps_expr = NULL;
//	 objops = match_param(sinst->thisobject, ops->key);
//	 if (objops && (objops->type == XC_EXPR)) {
//	    int i;
//	    float f;
//	    deflt_expr = evaluate_expr(sinst->thisobject, objops, NULL);
//	    switch (ops->type) {
//	       case XC_STRING:
//		  if (!textcomp(ops->parameter.string, deflt_expr, sinst))
//		     nondefault = false;
//		  break;
//	       case XC_EXPR:
//	          ps_expr = evaluate_expr(sinst->thisobject, ops, sinst);
//		  if (!strcmp(ps_expr, deflt_expr)) nondefault = false;
//		  break;
//	       case XC_INT:
//		  sscanf(deflt_expr, "%d", &i);
//		  if (i == ops->parameter.ivalue) nondefault = false;
//		  break;
//	       case XC_FLOAT:
//		  sscanf(deflt_expr, "%g", &f);
//		  if (f == ops->parameter.fvalue) nondefault = false;
//		  break;
//	    }
//	    if (deflt_expr) free(deflt_expr);
//	    if (nondefault == false) {
//	       if (ps_expr) free(ps_expr);
//	       continue;
//	    }
//	 }

	 if (instances++ == 0) {
	    fprintf(ps, "<<");		/* begin PostScript dictionary */
	    loccount = stcount + 2;
	 }
         dostcount(ps, &loccount, strlen(validref) + 2);
         fprintf(ps, "/%s ", validref);

         switch (ops->type) {
	    case XC_STRING:
	       writelabelsegs(ps, &loccount, ops->parameter.string);
	       break;
	    case XC_EXPR:
	       ps_expr = evaluate_expr(sinst->thisobject, ops, sinst);
	       dostcount(ps, &loccount, 3 + strlen(ps_expr));
	       fputs("(", ps);
	       fputs(ps_expr, ps);
	       fputs(") ", ps);
	       free(ps_expr);

	       /* The instance parameter expression may have the	*/ 
	       /* same expression as the object but a different		*/
	       /* result if the expression uses another parameter.	*/
	       /* Only print the expression itself if it's different	*/
	       /* from the object's expression.				*/

	       objops = match_param(sinst->thisobject, ops->key);
	       if (objops && strcmp(ops->parameter.expr, objops->parameter.expr)) {
	          dostcount(ps, &loccount, 3 + strlen(ops->parameter.expr));
	          fputs("(", ps);
	          fputs(ops->parameter.expr, ps);
	          fputs(") pop ", ps);
	       }
	       break;
	    case XC_INT:
	       if (ops->which == P_COLOR) {
		  /* Write R, G, B components */
		  _STR[0] = '{';
	          printRGBvalues(_STR + 1, ops->parameter.ivalue, "} ");
	       }
	       else
	          sprintf(_STR, "%d ", ops->parameter.ivalue);
	       dostcount(ps, &loccount, strlen(_STR));
	       fputs(_STR, ps);
	       break;
	    case XC_FLOAT:
	       sprintf(_STR, "%g ", ops->parameter.fvalue);
	       dostcount(ps, &loccount, strlen(_STR));
	       fputs(_STR, ps);
	       break;
	 }
      }
      free(validref);
   }
   if (instances > 0) {
      fprintf(ps, ">> ");		/* end PostScript dictionary */
      loccount += 3;
   }
   return loccount;
}

/*------------------------------------------------------------------*/
/* Macro for point output (calls varpcheck() on x and y components) */
/*------------------------------------------------------------------*/

#define xyvarcheck(z, n, t) \
    varpcheck(ps, (z).x, localdata, n, &stcount, *(t), P_POSITION_X); \
    varpcheck(ps, (z).y, localdata, n, &stcount, *(t), P_POSITION_Y)

#define xypathcheck(z, n, t, p) \
    varpathcheck(ps, (z).x, localdata, n, &stcount, t, TOPATH(p), P_POSITION_X); \
    varpathcheck(ps, (z).y, localdata, n, &stcount, t, TOPATH(p), P_POSITION_Y)
  
/*--------------------------------------------------------------*/
/* Main routine for writing the contents of a single object to	*/
/* output file "ps".						*/
/*--------------------------------------------------------------*/

void printOneObject(FILE *ps, objectptr localdata, int ccolor)
{
   int i, curcolor = ccolor;
   genericptr *savegen, *pgen;
   objinstptr sobj;
   graphicptr sg;
   Imagedata *img;
   XPoint* savept;
   short stcount;
   short segs;
   bool has_parameter;
   char *fptr, *validname;

   /* first, get a total count of all objects and give warning if large */


   if ((is_page(localdata) == -1) && (localdata->parts > 255)) {
      Wprintf("Warning: \"%s\" may exceed printer's PS limit for definitions",
	    localdata->name);
   }
	   
   for (savegen = localdata->begin(); savegen != localdata->end(); savegen++) {

      /* Check if this color is parameterized */
      eparamptr epp;
      oparamptr ops;

      for (epp = (*savegen)->passed; epp != NULL; epp = epp->next) {
	 ops = match_param(localdata, epp->key);
	 if (ops != NULL && (ops->which == P_COLOR)) {
	    /* Ensure that the next element forces a color change */
	    curcolor = ERRORCOLOR;
	    sprintf(_STR, "%s scb\n", epp->key);
	    fputs(_STR, ps);
	    break;
	 }
      }

      /* change current color if different */

      if ((epp == NULL) && ((*savegen)->color != curcolor)) {
	 if ((curcolor = (*savegen)->color) == DEFAULTCOLOR)
	    fprintf(ps, "sce\n");
	 else {
	    if (printRGBvalues(_STR, (*savegen)->color, "scb\n") < 0) {
	       fprintf(ps, "sce\n");
	       curcolor = DEFAULTCOLOR;
	    }
	    else
	       fputs(_STR, ps); 
	 }
      }

      stcount = 0;
      switch(ELEMENTTYPE(*savegen)) {

	 case(POLYGON):
	    varcheck(ps, TOPOLY(savegen)->style, localdata, &stcount,
			*savegen, P_STYLE);
	    varfcheck(ps, TOPOLY(savegen)->width, localdata, &stcount,
			*savegen, P_LINEWIDTH);
            for (savept = TOPOLY(savegen)->points.begin(); savept < TOPOLY(savegen)->
                      points.end(); savept++) {
	       varpcheck(ps, savept->x, localdata,
                        savept - TOPOLY(savegen)->points.begin(), &stcount, *savegen,
			P_POSITION_X);
	       varpcheck(ps, savept->y, localdata,
                        savept - TOPOLY(savegen)->points.begin(), &stcount, *savegen,
			P_POSITION_Y);
	    }
            sprintf(_STR, "%d polygon\n", TOPOLY(savegen)->points.count());
	    dostcount (ps, &stcount, strlen(_STR));
	    fputs(_STR, ps);
	    break;

	 case(PATH):
            pgen = TOPATH(savegen)->begin();
	    switch(ELEMENTTYPE(*pgen)) {
	       case POLYGON:
		  xypathcheck(TOPOLY(pgen)->points[0], 0, pgen, savegen);
		  break;
	       case SPLINE:
		  xypathcheck(TOSPLINE(pgen)->ctrl[0], 0, pgen, savegen);
		  break;
	    }
	    dostcount(ps, &stcount, 9);
	    fprintf(ps, "beginpath\n");
            for (pgen = TOPATH(savegen)->begin(); pgen != TOPATH(savegen)->end(); pgen++) {
	       switch(ELEMENTTYPE(*pgen)) {
		  case POLYGON:
                     for (savept = TOPOLY(pgen)->points.end() - 1;
                        savept > TOPOLY(pgen)->points.begin(); savept--) {
                        xypathcheck(*savept, savept - TOPOLY(pgen)->points.begin(), pgen,
				savegen);
	       	     }
                     sprintf(_STR, "%d polyc\n", TOPOLY(pgen)->points.count() - 1);
	       	     dostcount (ps, &stcount, strlen(_STR));
	       	     fputs(_STR, ps);
	       	     break;
		  case SPLINE:
		     xypathcheck(TOSPLINE(pgen)->ctrl[1], 1, pgen, savegen);
		     xypathcheck(TOSPLINE(pgen)->ctrl[2], 2, pgen, savegen);
		     xypathcheck(TOSPLINE(pgen)->ctrl[3], 3, pgen, savegen);
		     fprintf(ps, "curveto\n");
		     break;
	       }
	    }
	    varcheck(ps, TOPATH(savegen)->style, localdata, &stcount,
			*savegen, P_STYLE);
	    varfcheck(ps, TOPATH(savegen)->width, localdata, &stcount,
			*savegen, P_LINEWIDTH);
	    fprintf(ps, "endpath\n");
	    break;

	 case(SPLINE):
	    varcheck(ps, TOSPLINE(savegen)->style, localdata, &stcount,
			*savegen, P_STYLE);
	    varfcheck(ps, TOSPLINE(savegen)->width, localdata, &stcount,
			*savegen, P_LINEWIDTH);
	    xyvarcheck(TOSPLINE(savegen)->ctrl[1], 1, savegen);
	    xyvarcheck(TOSPLINE(savegen)->ctrl[2], 2, savegen);
	    xyvarcheck(TOSPLINE(savegen)->ctrl[3], 3, savegen);
	    xyvarcheck(TOSPLINE(savegen)->ctrl[0], 0, savegen);
	    fprintf(ps, "spline\n");
	    break;

	 case(ARC):
	    varcheck(ps, TOARC(savegen)->style, localdata, &stcount,
			*savegen, P_STYLE);
	    varfcheck(ps, TOARC(savegen)->width, localdata, &stcount,
			*savegen, P_LINEWIDTH);
	    xyvarcheck(TOARC(savegen)->position, 0, savegen);
	    varcheck(ps, abs(TOARC(savegen)->radius), localdata, &stcount,
			*savegen, P_RADIUS);
	    if (abs(TOARC(savegen)->radius) == TOARC(savegen)->yaxis) {
	       varfcheck(ps, TOARC(savegen)->angle1, localdata, &stcount,
			*savegen, P_ANGLE1);
	       varfcheck(ps, TOARC(savegen)->angle2, localdata, &stcount,
			*savegen, P_ANGLE2);
	       fprintf(ps, "xcarc\n");
	    }
	    else {
	       varcheck(ps, abs(TOARC(savegen)->yaxis), localdata, &stcount,
			*savegen, P_MINOR_AXIS);
	       varfcheck(ps, TOARC(savegen)->angle1, localdata, &stcount,
			*savegen, P_ANGLE1);
	       varfcheck(ps, TOARC(savegen)->angle2, localdata, &stcount,
			*savegen, P_ANGLE2);
	       fprintf(ps, "ellipse\n");
	    }
	    break;

	 case(OBJINST):
	    sobj = TOOBJINST(savegen);
	    varfcheck(ps, sobj->scale, localdata, &stcount, *savegen, P_SCALE);
	    varcheck(ps, sobj->rotation, localdata, &stcount, *savegen, P_ROTATION);
	    xyvarcheck(sobj->position, 0, savegen);

	    opsubstitute(sobj->thisobject, sobj);
	    stcount = printparams(ps, sobj, stcount);

            validname = create_valid_psname(sobj->thisobject->name, false);

	    /* Names without technologies get a leading string '::' 	*/
	    /* (blank technology)					*/

	    if (strstr(validname, "::") == NULL)
	       fprintf(ps, "::%s\n", validname);
	    else
	       fprintf(ps, "%s\n", validname);
	    break;
            
	 case(GRAPHIC):
	    sg = TOGRAPHIC(savegen);
	    for (i = 0; i < xobjs.images; i++) {
	       img = xobjs.imagelist + i;
	       if (img->image == sg->source)
		   break;
	    }

	    fptr = strrchr(img->filename, '/');
	    if (fptr == NULL)
	       fptr = img->filename;
	    else
	       fptr++;
	    fprintf(ps, "/%s ", fptr);
	    stcount += (2 + strlen(fptr));

	    varfcheck(ps, sg->scale, localdata, &stcount, *savegen, P_SCALE);
	    varcheck(ps, sg->rotation, localdata, &stcount, *savegen, P_ROTATION);
	    xyvarcheck(sg->position, 0, savegen);
	    fprintf(ps, "graphic\n");
	    break;

	 case(LABEL):

	    /* Don't save temporary labels from schematic capture system */
	    if (TOLABEL(savegen)->string->type != FONT_NAME) break;

	    /* Check for parameter --- must use "mark" to count # segments */
	    has_parameter = hasparameter(TOLABEL(savegen));

	    if (has_parameter) {
	       fprintf(ps, "mark ");
	       stcount += 5;
	    }

	    segs = writelabel(ps, TOLABEL(savegen)->string, &stcount);

	    if (segs > 0) {
	       if (has_parameter)
                  sprintf(_STR, "ctmk ");
	       else
                  sprintf(_STR, "%hd ", segs);
	       dostcount(ps, &stcount, strlen(_STR));
	       fputs(_STR, ps);
	       varcheck(ps, TOLABEL(savegen)->justify, localdata, &stcount,
			*savegen, P_JUSTIFY);
	       varcheck(ps, TOLABEL(savegen)->rotation, localdata, &stcount,
			*savegen, P_ROTATION);
	       varfcheck(ps, TOLABEL(savegen)->scale, localdata, &stcount,
			*savegen, P_SCALE);
	       xyvarcheck(TOLABEL(savegen)->position, 0, savegen);

	       switch(TOLABEL(savegen)->pin) {
		  case LOCAL:
		     strcpy(_STR, "pinlabel\n"); break;
		  case GLOBAL:
		     strcpy(_STR, "pinglobal\n"); break;
		  case INFO:
		     strcpy(_STR, "infolabel\n"); break;
		  default:
		     strcpy(_STR, "label\n");
	       }
	       dostcount(ps, &stcount, strlen(_STR));
	       fputs(_STR, ps);
	    }
	    break;
      }
   }
}

/*----------------------------------------------------------------------*/
/* Recursive routine to print out the library objects used in this	*/
/* drawing, starting at the bottom of the object hierarchy so that each	*/
/* object is defined before it is called.  A list of objects already	*/
/* written is maintained so that no object is written twice. 		*/
/*									*/
/* When object "localdata" is not a top-level page, call this routine	*/
/* with mpage=-1 (simpler than checking whether localdata is a page).	*/
/*----------------------------------------------------------------------*/

void printobjects(FILE *ps, objectptr localdata, objectptr **wrotelist,
	short *written, int ccolor)
{
   objectptr *optr;
   char *validname;
   int curcolor = ccolor;

   /* Search among the list of objects already written to the output	*/
   /* If this object has been written previously, then we ignore it.	*/

   for (optr = *wrotelist; optr < *wrotelist + *written; optr++)
      if (*optr == localdata)
	  return;

   /* If this page is a schematic, write out the definiton of any symbol */
   /* attached to it, because that symbol may not be used anywhere else. */

   if (localdata->symschem && (localdata->schemtype == PRIMARY))
      printobjects(ps, localdata->symschem, wrotelist, written, curcolor);

   /* Search for all object definitions instantiated in this object,	*/
   /* and (recursively) print them to the output.			*/

   for (objinstiter gptr; localdata->values(gptr); )
         printobjects(ps, gptr->thisobject, wrotelist, written, curcolor);

   /* Update the list of objects already written to the output */

   *wrotelist = (objectptr *)realloc(*wrotelist, (*written + 1) * 
		sizeof(objectptr));
   *(*wrotelist + *written) = localdata;
   (*written)++;

   validname = create_valid_psname(localdata->name, false);
   if (strstr(validname, "::") == NULL)
      fprintf(ps, "/::%s {\n", validname);
   else
      fprintf(ps, "/%s {\n", validname);

   /* No longer writes "bbox" record */
   if (localdata->hidden) fprintf(ps, "%% hidden\n");

   /* For symbols with schematics, and "trivial" schematics */
   if (localdata->symschem != NULL)
      fprintf(ps, "%% %s is_schematic\n", localdata->symschem->name);
   else if (localdata->schemtype == TRIVIAL)
      fprintf(ps, "%% trivial\n");
   else if (localdata->schemtype == NONETWORK)
      fprintf(ps, "%% nonetwork\n");

   printobjectparams(ps, localdata);
   fprintf(ps, "begingate\n");

   /* Write all the elements in order */

   printOneObject(ps, localdata, curcolor);

   /* Write object (gate) trailer */

   fprintf(ps, "endgate\n} def\n\n");
}

/*--------------------------------------------------------------*/
/* Print a page header followed by everything in the page.	*/
/* this routine assumes that all objects used by the page have	*/
/* already been handled and written to the output.		*/
/*								*/
/* "page" is the page number, counting consecutively from one.	*/
/* "mpage" is the page number in xcircuit's pagelist structure.	*/
/*--------------------------------------------------------------*/

void printpageobject(FILE *ps, objectptr localdata, short page, short mpage)
{
   XPoint origin, corner;
   objinstptr writepage;
   int width, height;
   float psnorm, psscale;
   float xmargin, ymargin;
   char *rootptr = NULL;
   polyptr framebox;

   /* Output page header information */

   writepage = xobjs.pagelist[mpage].pageinst;

   psnorm = xobjs.pagelist[mpage].outscale;
   psscale = getpsscale(psnorm, mpage);

   /* Determine the margins (offset of drawing from page corner)	*/
   /* If a bounding box has been declared in the drawing, it is		*/
   /* centered on the page.  Otherwise, the drawing itself is		*/
   /* centered on the page.  If encapsulated, the bounding box		*/
   /* encompasses only the object itself.				*/

   width = toplevelwidth(writepage, &origin.x);
   height = toplevelheight(writepage, &origin.y);

   corner.x = origin.x + width;
   corner.y = origin.y + height;

   if (xobjs.pagelist[mpage].pmode & 1) {	/* full page */

      if (xobjs.pagelist[mpage].orient == 90) {
         xmargin = (xobjs.pagelist[mpage].pagesize.x -
		((float)height * psscale)) / 2;
         ymargin = (xobjs.pagelist[mpage].pagesize.y -
		((float)width * psscale)) / 2;
      }
      else {
         xmargin = (xobjs.pagelist[mpage].pagesize.x -
		((float)width * psscale)) / 2;
         ymargin = (xobjs.pagelist[mpage].pagesize.y -
		((float)height * psscale)) / 2;
      }
   }
   else {	/* encapsulated --- should have 1" border so that any */
		/* drawing passed directly to a printer will not clip */
      xmargin = xobjs.pagelist[mpage].margins.x;
      ymargin = xobjs.pagelist[mpage].margins.y;
   }

   /* If a framebox is declared, then we adjust the page to be	*/
   /* centered on the framebox by translating through the	*/
   /* difference between the object center and the framebox	*/
   /* center.							*/

   if ((framebox = checkforbbox(localdata)) != NULL) {
      int i, fcentx = 0, fcenty = 0;

      for (i = 0; i < framebox->points.count(); i++) {
	 fcentx += framebox->points[i].x;
	 fcenty += framebox->points[i].y;
      }
      fcentx /= framebox->points.count();
      fcenty /= framebox->points.count();

      xmargin += psscale * (float)(origin.x + (width >> 1) - fcentx);
      ymargin += psscale * (float)(origin.y + (height >> 1) - fcenty);
   }

   /* If the page label is just the root name of the file, or has been left	*/
   /* as "Page n" or "Page_n", just do the normal page numbering.  Otherwise,	*/
   /* write out the page label explicitly.					*/

   if ((rootptr == NULL) || (!strcmp(rootptr, localdata->name))
		|| (strchr(localdata->name, ' ') != NULL)
		|| (strstr(localdata->name, "Page_") != NULL))
      fprintf (ps, "%%%%Page: %d %d\n", page, page);
   else
      fprintf (ps, "%%%%Page: %s %d\n", localdata->name, page);

   if (xobjs.pagelist[mpage].orient == 90)
      fprintf (ps, "%%%%PageOrientation: Landscape\n");
   else
      fprintf (ps, "%%%%PageOrientation: Portrait\n");

   if (xobjs.pagelist[mpage].pmode & 1) { 	/* full page */
      fprintf(ps, "%%%%PageBoundingBox: 0 0 %d %d\n",
                xobjs.pagelist[mpage].pagesize.x,
                xobjs.pagelist[mpage].pagesize.y);
   }

   /* Encapsulated files do not get a PageBoundingBox line,	*/
   /* unless the bounding box was explicitly drawn.		*/

   else if (framebox != NULL) {
      fprintf(ps, "%%%%PageBoundingBox: %g %g %g %g\n",
	xmargin, ymargin,
	xmargin + psscale * (float)(width),
	ymargin + psscale * (float)(height));
   }
     
   fprintf (ps, "/pgsave save def bop\n");

   /* Top-page definitions */
   if (localdata->params != NULL) {
      printobjectparams(ps, localdata);
      fprintf(ps, "begin\n");
   }

   if (localdata->symschem != NULL) {
      if (is_page(localdata->symschem) == -1)
         fprintf(ps, "%% %s is_symbol\n", localdata->symschem->name);
      else if (localdata->schemtype == SECONDARY)
         fprintf(ps, "%% %s is_primary\n", localdata->symschem->name);
      else
	 Wprintf("Something is wrong. . . schematic \"%s\" is connected to"
			" schematic \"%s\" but is not declared secondary.\n",
			localdata->name, localdata->symschem->name);
   }

   /* Extend bounding box around schematic pins */
   extendschembbox(xobjs.pagelist[mpage].pageinst, &origin, &corner);

   if (xobjs.pagelist[mpage].drawingscale.x != 1
                || xobjs.pagelist[mpage].drawingscale.y != 1)
      fprintf(ps, "%% %hd:%hd drawingscale\n", xobjs.pagelist[mpage].drawingscale.x,
                xobjs.pagelist[mpage].drawingscale.y);

   if (xobjs.pagelist[mpage].gridspace != 32
                || xobjs.pagelist[mpage].snapspace != 16)
      fprintf(ps, "%% %4.2f %4.2f gridspace\n", xobjs.pagelist[mpage].gridspace,
                xobjs.pagelist[mpage].snapspace);

   if (xobjs.pagelist[mpage].background.name != (char *)NULL) {
      if (xobjs.pagelist[mpage].orient == 90)
	 fprintf(ps, "%5.4f %d %d 90 psinsertion\n", psnorm,
		  (int)(ymargin - xmargin),
		  -((int)((float)(corner.y - origin.y) * psscale) +
		  (int)(xmargin + ymargin)));
      else
	 fprintf(ps, "%5.4f %d %d 0 psinsertion\n", psnorm,
		(int)(xmargin / psscale) - origin.x,
		(int)(ymargin / psscale) - origin.y);
      savebackground(ps, xobjs.pagelist[mpage].background.name);
      fprintf(ps, "\nend_insert\n");
   }

   if (xobjs.pagelist[mpage].orient == 90)
      fprintf(ps, "90 rotate %d %d translate\n", (int)(ymargin - xmargin),
	     -((int)((float)(corner.y - origin.y) * psscale) + 
	     (int)(xmargin + ymargin)));

   fprintf(ps, "%5.4f ", psnorm);
   switch(xobjs.pagelist[mpage].coordstyle) {
      case CM:
	 fprintf(ps, "cmscale\n");
	 break;
      default:
	 fprintf(ps, "inchscale\n");
	 break;
   }

   /* Final scale and translation */
   fprintf(ps, "%5.4f setlinewidth %d %d translate\n\n",
                1.3 * xobjs.pagelist[mpage].wirewidth,
		(int)(xmargin / psscale) - origin.x,
		(int)(ymargin / psscale) - origin.y);

   /* Output all the elements in the page */
   printOneObject(ps, localdata, DEFAULTCOLOR);
   /* Page trailer */
   if (localdata->params != NULL) fprintf(ps, "end ");
   fprintf(ps, "pgsave restore showpage\n");
}

/*--------------------------------------------------------------*/
/* Print objects referenced from a particular page.  These get	*/
/* bundled together at the beginning of the output file under	*/
/* the DSC "Setup" section, so that the PostScript		*/
/* interpreter knows that these definitions may be used by any	*/
/* page.  This prevents ghostscript from producing an error	*/
/* when backing up in a multi-page document.			*/
/*--------------------------------------------------------------*/

void printrefobjects(FILE *ps, objectptr localdata, objectptr **wrotelist,
	short *written)
{
   /* If this page is a schematic, write out the definiton of any symbol */
   /* attached to it, because that symbol may not be used anywhere else. */

   if (localdata->symschem && (localdata->schemtype == PRIMARY))
      printobjects(ps, localdata->symschem, wrotelist, written, DEFAULTCOLOR);

   /* Search for all object definitions instantiated on the page and	*/
   /* write them to the output.						*/

   for (objinstiter gptr; localdata->values(gptr); )
         printobjects(ps, gptr->thisobject, wrotelist, written, DEFAULTCOLOR);
}

/*----------------------------------------------------------------------*/
