/*-------------------------------------------------------------------------*/
/* filelist.c --- Xcircuit routines for the filelist widget		   */ 
/* Copyright (c) 2002  Tim Edwards, Johns Hopkins University        	   */
/*-------------------------------------------------------------------------*/

#include <QDir>
#include <QDateTime>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <cctype>
#include <cerrno>
#include <climits>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef TCL_WRAPPER 
#include <tk.h>
#endif

#include "colors.h"
#include "xcircuit.h"
#include "prototypes.h"

/*-------------------------------------------------------------------------*/
/* Local definitions							   */
/*-------------------------------------------------------------------------*/

#define INITDIRS 10

/*-------------------------------------------------------------------------*/
/* Global Variable definitions						   */
/*-------------------------------------------------------------------------*/

#ifdef TCL_WRAPPER
extern Tcl_Interp *xcinterp;
#endif

extern XCWindowData *areawin;
extern ApplicationData appdata;
extern short	  popups;     /* total number of popup windows */

Pixmap   flistpix = (Pixmap)NULL;    /* For file-selection widget */
short    flstart, flfiles, flcurrent;
int	 flcurwidth;

QPainter*	 hgc = NULL, *sgc = NULL;
char	 *cwdname = NULL;
fileliststruct *files;

/*----------------------------------------------------------------------*/
/* Read a crash file to find the name of the original file.		*/
/*----------------------------------------------------------------------*/

char *getcrashfilename(const QString & filename)
{
   FILE *fi;
   char temp[256];
   char *retstr = NULL, *tpos, *spos;
   int slen;

   if ((fi = fopen(filename.toLocal8Bit(), "r")) != NULL) {
      while (fgets(temp, 255, fi) != NULL) {
	 if ((tpos = strstr(temp, "Title:")) != NULL) {
	    ridnewline(temp);
	    tpos += 7;
	    if ((spos = strrchr(temp, '/')) != NULL)
	       tpos = spos + 1;
	    retstr = (char *)malloc(1 + strlen(tpos));
	    strcpy(retstr, tpos);
	 }
	 else if ((tpos = strstr(temp, "CreationDate:")) != NULL) {
	    ridnewline(temp);
	    tpos += 14;
	    slen = strlen(retstr);
	    retstr = (char *)realloc(retstr, 4 + slen + strlen(tpos));
	    sprintf(retstr + slen, " (%s)", tpos);
	    break;
	 }
      }
      fclose(fi);
   }
   return retstr;
}

/*----------------------------------------------------------------------*/
/* Crash recovery:  load the file, and link the tempfile name to it.    */
/*----------------------------------------------------------------------*/

void crashrecover(QAction*, const QString &str, void*)
{
   if (!xobjs.tempfile.isEmpty()) {
      QFile::remove(xobjs.tempfile);
   }
   xobjs.tempfile = str;

   startloadfile(-1, str);
}

/*----------------------------------------------------------------------*/
/* Look for any files left over from a crash.                           */
/*----------------------------------------------------------------------*/

void findcrashfiles()
{
   QString filename;
#ifndef _MSC_VER
   uid_t userid = getuid();
#endif
   QDateTime recent;

   QDir tempdir(xobjs.tempdir);
   foreach (QString file, tempdir.entryList(QDir::Files)) {
      QString path = QString(xobjs.tempdir) + QDir::separator() + file;
      if (file.startsWith("XC")) {
         int pid = -1;
         int dotidx = file.indexOf('.');
         if (dotidx > 0) {
             bool ok;
             pid = file.mid(2, dotidx-3).toInt(&ok);
             if (!ok) pid = -1;
         }

         QFileInfo fi(path);
#ifndef _MSC_VER
         if (fi.ownerId() == userid) {
#endif
            if (recent.isNull() || fi.created() > recent) {

	       /* Check if the PID belongs to an active process	*/
	       /* by sending a CONT signal and checking if 	*/
	       /* there was no error result.			*/

#ifndef _MSC_VER
	       if (pid != -1)
		  if (kill((pid_t)pid, SIGCONT) == 0)
		     continue;
#endif

               recent = fi.created();
               filename = path;
	    }
	 }
      }
   }
   
   if (!recent.isNull()) {	/* There exists at least one temporary file */
			/* belonging to this user.  Ask to recover  */
			/* the most recent one.			    */

      /* Warn user of existing tempfile, and ask user if file	*/
      /* should be recovered immediately.			*/

#ifdef TCL_WRAPPER
      char *cfile = getcrashfilename();

      sprintf(_STR, ".query.title.field configure -text "
		"\"Recover file \'%s\'?\"", 
		(cfile == NULL) ? "(unknown)" : cfile);
      Tcl_Eval(xcinterp, _STR);
      Tcl_Eval(xcinterp, ".query.bbar.okay configure -command "
		"{filerecover; wm withdraw .query}; wm deiconify .query");
      free(cfile);
#else
      getfile(NULL, Number(RECOVER), &filename);   /* Crash recovery mode */
#endif
   }
}

/*-------------------------------------------------------------------------*/
