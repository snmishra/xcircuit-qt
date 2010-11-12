#ifndef XC_PROTOTYPES_H
#define XC_PROTOTYPES_H

/*----------------------------------------------------------------------*/
/* prototypes.h:							*/
/*     Master list of function prototypes				*/
/*----------------------------------------------------------------------*/

#include <QString>
class QAction;
class uselection;

/* from undo.c */

/* Note variable argument list for register_for_undo() */
void register_for_undo(u_int, u_char, objinstptr, ...);
void undo_finish_series(void);
void undo_action(void);
void redo_action(void);
void flush_undo_stack(void);
void flush_redo_stack(void);
void truncate_undo_stack(void);
void free_undo_record(Undoptr);
void free_redo_record(Undoptr);
stringpart *get_original_string(labelptr);
short *recover_selectlist(Undoptr);
void undo_call(QAction*, void*, void*);
void redo_call(QAction*, void*, void*);

char *evaluate_expr(objectptr, oparamptr, objinstptr);

/* from elements.c: */

/* element constructor functions */

int getsubpartindex(pathptr);
labelptr new_label(objinstptr, stringpart *, int, int, int);
labelptr new_simple_label(objinstptr, char *, int, int, int);
labelptr new_temporary_label(objectptr, char *, int, int);
polyptr new_polygon(objinstptr, pointlist *);
splineptr new_spline(objinstptr, pointlist);
arcptr new_arc(objinstptr, int, int, int);
objinstptr new_objinst(objinstptr, objinstptr, int, int);

/* element destructor function */
void remove_element(objinstptr, genericptr);

void drawdot(int, int);
void copyalleparams(genericptr dst, const generic * src);
void copyparams(objinstptr dst, const objinst * src);

void textbutton(u_char, int, int);
void charreport(labelptr);
bool labeltext(int, char *);
void textreturn(void);
void rejustify(short);
void findconstrained(polyptr);
void reversefpoints(XfPoint *, short);
void freeparts(short *, short);
void removep(short *, short);
void unjoin(void);
void unjoin_call(QAction*, void*, void*);
labelptr findlabelcopy(labelptr, stringpart *);
bool neartest(XPoint *, XPoint *);
void join();
void join_call(QAction*, void*, void*);
genericptr getsubpart(pathptr, int*);
void updatepath(pathptr thepath);

/* interactive manipulation of elements */
void splinebutton(int, int);
void trackelement(Widget, caddr_t, caddr_t);
void arcbutton(int, int);
void trackarc(Widget, caddr_t, caddr_t);
void boxbutton(int, int);
void trackbox(Widget, caddr_t, caddr_t);
void trackwire(Widget, caddr_t, caddr_t);
void startwire(XPoint *);
void setendpoint(short *, short, XPoint **, XPoint *);
void wire_op(int, int ,int);

/* from events.c: */

void editpoints(genericptr *ssgen, const XPoint & delta);
bool recursefind(objectptr, objectptr);
void transferselects(void);
void select_invalidate_netlist(void);
void setpage(bool);
int changepage(short);
void newpage(short);
void pushobject(objinstptr);
void push_stack(pushlistptr *, objinstptr);
void pop_stack(pushlistptr *);
void free_stack(pushlistptr *);
void popobject(QAction*, void*, void*);
void resetbutton(QAction*, void*, void*);
void zoombox(Widget, caddr_t, caddr_t);
void warppointer(int, int);
void panbutton(u_int, int, int, float);
void zoomin_call(QAction*, void*, void*);
void zoominrefresh(int, int);
void zoomout_call(QAction*, void*, void*);
void zoomoutrefresh(int, int);
void panrefresh(u_int, int, int, float);
void checkwarp(XPoint *);
void warparccycle(arcptr, short);
int checkcycle(genericptr, short);
extern pointselect *getrefpoint(genericptr, XPoint **);
void copyvirtual(void);
void nextpathcycle(pathptr, short);
void nextpolycycle(polyptr*, short);
void nextsplinecycle(splineptr*, short);
void nextarccycle(arcptr*, short);
void keyhandler(Widget, caddr_t, XKeyEvent *);
bool compatible_function(int);
int eventdispatch(int, int, int);
int functiondispatch(int, short, int, int);
void releasehandler(Widget, caddr_t, XKeyEvent *);
void setsnap(short);
void snapelement(void);
int ipow10(int);
int calcgcf(int, int);
void fraccalc(float, char *);
void printpos(const XPoint &);
void findwirex(XPoint *, XPoint *, XPoint *, XPoint *, int *);
void findattach(XPoint *, int *, XPoint *);
XPoint *pathclosepoint(pathptr, XPoint *);
void placeselects(const XPoint &, XPoint *);
void drag(int, int);
void xlib_drag(Widget, caddr_t, QEvent* *);
void elemrotate(genericptr, short, XPoint *);
void elementrotate(short, XPoint *);
void edit(int, int);
void pathedit(genericptr);
void xc_lower(short *);
void xc_raise(short *);
void exchange(void);
void elhflip(genericptr *, short);
void elvflip(genericptr *, short);
void elementflip(XPoint *);
void elementvflip(XPoint *);
short getkeynum(void);
void makepress(XtPointer, XtIntervalId *);
void reviseselect(short *, int, short *);
void deletebutton(int, int);
void delete_one_element(objinstptr, genericptr);
short *xc_undelete(objinstptr, objectptr, short *);
objectptr delete_element(objinstptr, short *, int);
void printname(objectptr);
bool checkname(objectptr);
char *checkvalidname(char *, objectptr);
objectptr finddot(void);
void movepoints(genericptr *, const XPoint & delta);
void xlib_makeobject(QAction*, const QString &, void*);
objinstptr domakeobject(int, char *, bool);
void selectsave(QAction*, void*, void*);
void arceditpush(arcptr);
void splineeditpush(splineptr);
void polyeditpush(polyptr);
void patheditpush(pathptr);
void delete_more(objinstptr, genericptr);;
void createcopies(void);
void copydrag(void);
void copy_op(int, int, int);
//bool checkmultiple(XButtonEvent *);
void continue_op(int, int, int);
void finish_op(int, int, int);
void path_op(genericptr, int, int, int);
void inst_op(genericptr, int, int, int);
void standard_element_delete();
void delete_for_xfer(short *, int);
void delete_noundo();

/* from filelist.c: */

int fcompare(const void *, const void *);
void dragfilebox(Widget, caddr_t, XMotionEvent *);
void startfiletrack(Widget, caddr_t, XCrossingEvent *);
void endfiletrack(Widget, caddr_t, XCrossingEvent *);
char *getcrashfilename(const QString &);
void crashrecover(QAction*, const QString &, void*);
void findcrashfiles(void);
void listfiles(Widget, popupstruct *, caddr_t);
void newfilelist(Widget, popupstruct *);
void fileselect(Widget, popupstruct *, XButtonEvent *);
void showlscroll(Widget, caddr_t, caddr_t);
void draglscroll(Widget, popupstruct *, XButtonEvent *);
void genfilelist(Widget, popupstruct *, Dimension);

/* from files.c: */

#ifdef ASG
void importspice(QAction*, void*, void*);
#endif

char *ridnewline(char *);
void pagereset(short);
void freelabel(stringpart *);
float getpsscale(float, short);
void dostcount(FILE *, short *, short);
short printparams(FILE *, objinstptr, short);
void printobjectparams(FILE *, objectptr);
void varcheck(FILE *, short, objectptr, short *, genericptr, u_char);
void varfcheck(FILE *, float, objectptr, short *, genericptr, u_char);
void varpcheck(FILE *, short, objectptr, int, short *, genericptr, u_char);
void getfile(QAction*, void*, void*);
int filecmp(const QString &, const QString &);
void loadglib(bool, short, short, const QString &);
void loadulib(QAction*, const QString&, void*);
void loadblib(QAction*, const QString&, void*);
void getlib(QAction*, void*, void*);
void getuserlib(QAction*, void*, void*);
bool loadlibrary(short, const QString &);
void startloadfile(int, const QString & list);
void normalloadfile(QAction*, const QString&, void*);
void importfile(QAction*, const QString&, void*);
bool loadfile(short, int, const QString&);
void readlabel(objectptr, char *, stringpart **);
void readparams(objectptr, objinstptr, objectptr, char *);
u_char *find_match(u_char *);
char *advancetoken(char *);
char *varpscan(objectptr, char *, short *, genericptr, int, int, u_char);
char *varscan(objectptr, char *, short *, genericptr, u_char);
char *varfscan(objectptr, char *, float *, genericptr, u_char);
objinstptr addtoinstlist(int, objectptr, bool);
bool objectread(FILE *, objectptr, short, short, short, char *,
		int, TechPtr);
void importfromlibrary(short, char *, char *);
objectptr *new_library_object(short, char *, objlistptr *, TechPtr);
bool library_object_unique(short, objectptr, objlistptr);
void add_object_to_library(short, objectptr);
char *find_delimiter(char *);
char standard_delimiter_end(char);

bool CompareTechnology(objectptr, char *);
TechPtr LookupTechnology(char *);
TechPtr GetObjectTechnology(objectptr);
TechPtr AddNewTechnology(char *, char *);
void AddObjectTechnology(objectptr);
void TechReplaceSave();
void TechReplaceRestore();
void TechReplaceAll();
void TechReplaceNone();

void setfile(Widget, Widget, caddr_t);
void savetemp(XtPointer, XtIntervalId *);
void incr_changes(objectptr);
void savelibpopup(QAction*, void*, void*);
void savelibrary(QAction*, const QString &, void*);
void savetechnology(char *, char *);
void findfonts(objectptr, short *);
void savefile(short);
int printRGBvalues(char *, int, const char *);
char *nosprint(char *);
FILE *fileopen(const QString &, const char *, QString *name_return = 0);
FILE *libopen(const QString &, short, QString *name_return = 0);

bool xc_tilde_expand(QString &);
bool xc_variable_expand(QString &);
short writelabel(FILE *, stringpart *, short *);
char *writesegment(stringpart *, float *, int *);
int writelabelsegs(FILE *, short *, stringpart *);
void printobjects(FILE *, objectptr, objectptr **, short *, int);
void printrefobjects(FILE *, objectptr, objectptr **, short *);
void printpageobject(FILE *, objectptr, short, short);


/* from fontfile.c: */

FILE *findfontfile(const char *);
int loadfontfile(const char *);

/* from formats.c: */

void loadlgf(int);
void loadmat4(caddr_t);

/* from functions.c: */

long sqwirelen(const XPoint *, const XPoint *);
float fsqwirelen(XfPoint *, XfPoint *);
int wirelength(const XPoint *, const XPoint *);
long finddist(const XPoint *, const XPoint *, const XPoint *);
void initsplines(void);
void computecoeffs(splineptr, float *, float *, float *, float *,
                          float *, float *);
void findsplinepos(splineptr, float, XPoint *, int *);
void ffindsplinepos(splineptr, float, XfPoint *);
float findsplinemin(splineptr, XPoint *);
short closepoint(polyptr, XPoint *);
short closedistance(polyptr, XPoint *);
void updateinstparam(objectptr);
short checkbounds(Context*);
void window_to_user(short, short, XPoint *);
void user_to_window(XPoint, XPoint *);

void UDrawBBox(Context*);

XPoint UGetCursor(void);
XPoint UGetCursorPos(void);
XPoint u2u_getSnapped(XPoint f);
void u2u_snap(XPoint &);
void snap(short, short, XPoint *);
void manhattanize(XPoint *, polyptr, short, bool);
void bboxcalc(short, short *, short *);
void calcextents(genericptr *, short *, short *, short *, short *);
void calcinstbbox(genericptr *, short *, short *, short *, short *);
void calcbboxsingle(genericptr *, objinstptr, short *, short *, short *, short *);
bool object_in_library(short, objectptr);
void calcbboxinst(objinstptr);
void updatepagebounds(objectptr);
void calcbbox(objinstptr);
void calcbboxparam(objectptr, int);
void singlebbox(genericptr *);
void calcbboxselect(void);
void calcbboxvalues(objinstptr, genericptr *);
void centerview(objinstptr);
void refresh(QAction*, void*, void*);
void zoomview(QAction*, void*, void*);
void UDrawSimpleLine(Context*, const XPoint *, const XPoint *);
void UDrawLine(Context*, const XPoint *, const XPoint *);
void UDrawCircle(Context*, const XPoint *, u_char);
void UDrawX(Context*, labelptr);
void UDrawXDown(Context*, labelptr);
int  toplevelwidth(objinstptr, short *);
int  toplevelheight(objinstptr, short *);
void extendschembbox(objinstptr, XPoint *, XPoint *);
void pinadjust(short, short *, short *, short);
void UDrawTextLine(Context*, labelptr, short);
void UDrawTLine(Context*, labelptr);
void UDrawXLine(Context*, XPoint, XPoint);
void UDrawBox(Context*, XPoint, XPoint);
float UDrawRescaleBox(Context*, const XPoint &);
void strokepath(Context*, XPoint *, short, short, float);
void makesplinepath(Context*, const spline *, XPoint *);
void UDrawObject(Context*, objinstptr, short, int, pushlistptr *);
void TopDoLatex(void);

/* from help.c: */

void showhsb(Widget, caddr_t, caddr_t);
void printhelppix(void);
void starthelp(QAction*, void*, void*);
void simplescroll(Widget, Widget, XPointerMovedEvent *);
void exposehelp(Widget, caddr_t, caddr_t);
void printhelp(Widget);

/* from keybindings.c */

int firstbinding(Widget, int);
bool ismacro(Widget, int);
int boundfunction(Widget, int, short *);
int string_to_func(const char *, short *);
int string_to_key(const char *);
QString function_binding_to_string(Widget, int);
QString key_binding_to_string(Widget, int);
QString compat_key_to_string(Widget, int);
QString func_to_string(int);
QString key_to_string(int);
void printeditbindings(void);
int add_binding(Widget, int, int, short value = -1);
int add_keybinding(Widget, const char *, const char *);
void default_keybindings(void);
int remove_binding(Widget, int, int);
void remove_keybinding(Widget, const char *, const char *);

void mode_rebinding(int, int);
void mode_tempbinding(int, int);

/* from libraries.c: */

short findhelvetica(void);
void catreturn(void);
int pageposition(short, int, int, int);
short pagelinks(int);
short *pagetotals(int, short);
bool is_virtual(objinstptr);
int is_page(objectptr);
int is_library(objectptr);
int NameToLibrary(char *);
void tech_set_changes(TechPtr);
void tech_mark_changed(TechPtr);
int libfindobject(objectptr, int *);
int libmoveobject(objectptr, int);
void pagecat_op(int, int, int);
void pageinstpos(short, short, objinstptr, int, int, int, int);
void computespacing(short, int *, int *, int *, int *);
void composepagelib(short);
void updatepagelib(short, short);
void pagecatmove(int, int);
void composelib(short);
short finddepend(objinstptr, objectptr **);
void cathide(void);
void catvirtualcopy(void);
void catdelete(void);
void catmove(int, int);
void copycat(void);
void catalog_op(int, int, int);
void changecat(void);
void changecat_call(QAction*, void*, void*);
void startcatalog(QAction*, void*, void*);

/* from menucalls.c: */

void setgrid(QAction*, const QString&, void*);
void measurestr(float, char *);
void setwidth(QAction*, const QString&, void*);
void changetextscale(float);
void autoscale(int);
float parseunits(char *);
bool setoutputpagesize(XPoint *, const QString &);
void setkern(QAction*, const QString&, void*);
void setdscale(QAction*, const QString&, void*);
void setosize(QAction*, const QString&, void*);
void setwwidth(QAction*, const QString&, void*);
#ifdef TCL_WRAPPER
void renamepage(short);
void renamelib(short);
void setallstylemarks(u_short);
#endif
labelptr gettextsize(float **);
void stringparam(QAction*, const QString &, void*);
int setelementstyle(QAction*, u_short, u_short);
void togglegrid(u_short);
void togglefontmark(int);
void setcolorscheme(bool);
void getgridtype(QAction*, void*, void*);
void newlibrary(QAction*, void*, void*);
int createlibrary(bool);
void makepagebutton(void);
int findemptylib(void);
polyptr checkforbbox(objectptr);
#ifdef TCL_WRAPPER
void setfontmarks(short, short);
#endif
void startparam(QAction*, void*, void*);
void startunparam(QAction*, void*, void*);
void setdefaultfontmarks(void);
void setjustbit(QAction*, void*, void*);
void setpinjustbit(QAction*, void*, void*);
void setjust(QAction*, void*, labelptr, short);
void setvjust(QAction*, void*, void*);
void sethjust(QAction*, void*, void*);
void boxedit(QAction*, void*, void*);
void locloadfont(QAction*, const QString &, void*);
short findbestfont(short, short, short, short);
void setfontval(QAction*, void*, labelptr);
void setfont(QAction*, void*, void*);
void setfontstyle(QAction*, void*, labelptr);
void fontstyle(QAction*, void*, void*);
void setfontencoding(QAction*, void*, void*);
void fontencoding(QAction*, void*, void*);
void addtotext(QAction*, void*, void*);
bool dospecial(void);

/* from xtfuncs.c: */

void addnewcolor(QAction*, void*, void*);
void color_popup(QAction*, void*, void*);
void setcolor(QAction*, void*, void*);
void makenewfontbutton(void);  /* either here or menucalls.c */
void setfloat(QAction*, const QString&, void*);
void autoset(QObject*, WidgetList, caddr_t);
void autostop(QAction*, caddr_t, caddr_t);
void getkern(QAction*, void*, void*);
void setfill(QAction*, void*, void*);
void makebbox(QAction*, void*, void*);
void setclosure(QAction*, void*, void*);
void setopaque(QAction*, void*, void*);
void setline(QAction*, void*, void*);
void changetool(QAction*, void*, void*);
void exec_or_changetool(QAction*, void*, void*);
void rotatetool(QAction*, void*, void*);
void pantool(QAction*, void*, void*);
void toggleexcl(QAction*);
void setEnabled(QAction*, bool);
void highlightexcl(QAction*, int, int);
void toolcursor(int);
void promptparam(QAction*, void*, void*);
void gettsize(QAction*, void*, void*);
void settsize(QAction*, const QString &, void*);
void dotoolbar(QAction*, void*, void*);
void overdrawpixmap(QAction*);
void getsnapspace(QAction*, void*, void*);
void getgridspace(QAction*, void*, void*);
void setscaley(Widget, const QString&, void*);
void setscalex(Widget, const QString&, void*);
void setorient(Widget, void*, void*);
void setpmode(Widget, void*, void*);
void setpagesize(QWidget *, XPoint*, const QString &);
void getdscale(QAction*, void*, void*);
void getosize(QAction*, void*, void*);
void getwirewidth(QAction*, void*, void*);
void getwwidth(QAction*, void*, void*);
void getfloat(QAction*, void*, void*);
void setfilename(QAction*, const QString&, void*);
void setpagelabel(Widget, void*, void*);
void makenewfontbutton(void);
void newpagemenu(QAction*, void*, void*);
void makenewencodingbutton(const char *, char);
void toggle(QAction *, void*, void*);
void inversecolor(QAction*, void*, void*);
void setgridtype(char *);
void renamepage(short);
void renamelib(short);

void setallstylemarks(u_short);
void setfontmarks(short, short);
void position_popup(Widget, Widget);
void border_popup(QAction*, void*, void*);
void fill_popup(QAction*, void*, void*);
void param_popup(QAction*, void*, void*);
void addnewfont(QAction*, void*, void*);


/* from netlist.c: */

#ifdef TCL_WRAPPER
Tcl_Obj *tclglobals(objinstptr);
Tcl_Obj *tcltoplevel(objinstptr);
void ratsnest(objinstptr);
#endif

void ReferencePosition(objinstptr, XPoint *, XPoint *);
int NameToPinLocation(objinstptr, char *, int *, int *);
bool RemoveFromNetlist(objectptr, genericptr);
labelptr NetToLabel(int, objectptr);
void NameToPosition(objinstptr, labelptr, XPoint *);
XPoint *NetToPosition(int, objectptr);
int getsubnet(int, objectptr);
void invalidate_netlist(objectptr);
void remove_netlist_element(objectptr, genericptr);
int updatenets(objinstptr, bool);
void createnets(objinstptr, bool);
bool nonnetwork(polyptr);
int globalmax(void);
LabellistPtr geninfolist(objectptr, objinstptr, const char *);
void gennetlist(objinstptr);
void gencalls(objectptr);
void search_on_siblings(objinstptr, objinstptr, pushlistptr,
		short, short, short, short);
char *GetHierarchy(pushlistptr *, bool);
bool HierNameToObject(objinstptr, char *, pushlistptr *);
void resolve_devindex(objectptr, bool);
void copy_bus(Genericlist *, Genericlist *);
Genericlist *is_resolved(genericptr *, pushlistptr, objectptr *);
void highlightnetlist(Context*, objectptr, objinstptr);
void remove_highlights(objinstptr);
int pushnetwork(pushlistptr, objectptr);
bool match_buses(Genericlist *, Genericlist *, int);
int onsegment(XPoint *, XPoint *, XPoint *);
bool neardist(long);
bool nearpoint(XPoint *, XPoint *);
int searchconnect(XPoint *, int, objinstptr, int);
Genericlist *translateup(Genericlist *, objectptr, objectptr, objinstptr);
Genericlist *addpoly(objectptr, polyptr, Genericlist *);
long zsign(long, long);
bool mergenets(objectptr, Genericlist *, Genericlist *);
void removecall(objectptr, CalllistPtr);
Genericlist *addpin(objectptr, objinstptr, labelptr, Genericlist *);
Genericlist *addglobalpin(objectptr, objinstptr, labelptr, Genericlist *);
void addcall(objectptr, objectptr, objinstptr);
void addport(objectptr, Genericlist *);
bool addportcall(objectptr, Genericlist *, Genericlist *);
void makelocalpins(objectptr, CalllistPtr, const char *);
int porttonet(objectptr, int);
stringpart *nettopin(int, objectptr, const char *);
Genericlist *pointtonet(objectptr, objinstptr, XPoint *);
Genericlist *pintonet(objectptr, objinstptr, labelptr);
Genericlist *nametonet(objectptr, objinstptr, char *);
Genericlist *new_tmp_pin(objectptr, XPoint *, char *, const char *, Genericlist *);
Genericlist *make_tmp_pin(objectptr, objinstptr, XPoint *, Genericlist *);
void resolve_devnames(objectptr);
void resolve_indices(objectptr, bool);
void clear_indices(objectptr);
void unnumber(objectptr);
char *parseinfo(objectptr, objectptr, CalllistPtr, const char *, const char *, bool,
                bool);
int writedevice(FILE *, const char *, objectptr, CalllistPtr, const char *);
void writeflat(objectptr, CalllistPtr, const char *, FILE *, const char *);
void writeglobals(objectptr, FILE *);
void writehierarchy(objectptr, objinstptr, CalllistPtr, FILE *, const char *);
void writenet(objectptr, const char *, const char *);
bool writepcb(struct Ptab **, objectptr, CalllistPtr, const char *, const char *);
void outputpcb(struct Ptab *, FILE *);
void freepcb(struct Ptab *);
void freegenlist(Genericlist *);
void freepolylist(PolylistPtr *);
void freenetlist(objectptr);
void freelabellist(LabellistPtr *);
void freecalls(CalllistPtr);
void freenets(objectptr);
void freetemplabels(objectptr);
void freeglobals(void);
void destroynets(objectptr);
int  cleartraversed(objectptr);
int  checkvalid(objectptr);
void clearlocalpins(objectptr);
void append_included(char *);
bool check_included(char *);
void free_included(void);

/* from ngspice.c: */
int exit_spice(void);

/* from parameter.c: */ 

void param_init();
char *find_indirect_param(objinstptr, char *);
oparamptr match_param(const object*, const char *);
oparamptr match_instance_param(objinstptr, const char *);
oparamptr find_param(objinstptr, const char *);
int get_num_params(objectptr);
void free_object_param(objectptr, oparamptr);
oparamptr free_instance_param(objinstptr, oparamptr);
void free_element_param(genericptr, eparamptr);

oparamptr make_new_parameter(char *);
eparamptr make_new_eparam(char *);

const char *getnumericalpkey(u_int);
char *makeexprparam(objectptr, char *, char *, int);
bool makefloatparam(objectptr, char *, float);
bool makestringparam(objectptr, char *, stringpart *);
void std_eparam(genericptr, char *);
void setparammarks(genericptr);
void makenumericalp(genericptr *, u_int, char *, short);
void noparmstrcpy(u_char *, u_char *);
void insertparam(void);
void makeparam(labelptr, char *);
void searchinst(objectptr, objectptr, char *);
stringpart *searchparam(stringpart *);
void unmakeparam(labelptr, stringpart *);
void removenumericalp(genericptr *, u_int);
void unparameterize(int);
void parameterize(int, char *, short);
genericptr findparam(objectptr, void *, u_char);
bool paramcross(objectptr, labelptr);
oparamptr parampos(objectptr, labelptr, char *, short *, short *);
int opsubstitute(objectptr, objinstptr);
void exprsub(genericptr);
int epsubstitute(genericptr, objectptr, objinstptr, bool *);
int psubstitute(objinstptr);
bool has_param(genericptr);
oparamptr copyparameter(oparamptr);
eparamptr copyeparam(eparamptr, const generic *);

void pwriteback(objinstptr);
short paramlen(u_char *);
void curtail(u_char *);
int checklibtop(void);
void removeinst(objinstptr);
void resolveparams(objinstptr);

/* from python.c: */

#ifdef HAVE_PYTHON
int python_key_command(int);
void init_interpreter(void);
void exit_interpreter(void);
#endif

Widget *pytoolbuttons(int *);

/* from rcfile.c: */

short execcommand(short, char *);
void defaultscript(void);
void execscript(QAction*, const QString&, void*);
void loadrcfile(void);
#ifndef HAVE_PYTHON
short readcommand(short, FILE *);
#endif

/* from graphic.c */

Imagedata *addnewimage(char *, int, int);
graphicptr new_graphic(objinstptr, char *, int, int);
void invalidate_graphics(objectptr);
short *collect_graphics(short *);
void count_graphics(objectptr thisobj, short *glist);

/* from flate.c */

#ifdef HAVE_LIBZ
u_long large_deflate(u_char *, u_long, u_char *, u_long);
u_long large_inflate(u_char *, u_long, u_char **, u_long);
unsigned long ps_deflate (unsigned char *, unsigned long,
	unsigned char *, unsigned long);
unsigned long ps_inflate (unsigned char *, unsigned long,
	unsigned char **, unsigned long);
#endif

/* from render.c: */

void ghostinit(void);
void send_client(Atom);
void ask_for_next(void);
void start_gs(void);
void parse_bg(FILE *, FILE *);
void bg_get_bbox(void);
void backgroundbbox(int);
void readbackground(FILE *);
void savebackground(FILE *, const QString &);
void register_bg(const QString &);
void loadbackground(QAction*, const QString&, void*);
void send_to_gs(const char *);
int renderbackground(void);
int copybackground(Context*);
int exit_gs(void);
int reset_gs(void);

#ifndef TCL_WRAPPER
bool render_client(QEvent* *);
#endif

/* from schema.c: */

objectptr NameToPageObject(char *, objinstptr *, int *);
objectptr NameToObject(char *, objinstptr *, bool);
int checkpagename(objectptr);
void callwritenet(QAction*, void*, void*);
void startconnect(QAction*, void*, void*);
void connectivity(QAction*, void*, void*);
bool setobjecttype(objectptr);
void pinconvert(labelptr, unsigned int);
void dopintype(QAction*, void*, void*);
void setsymschem(void);
int findpageobj(objectptr);
void collectsubschems(int);
int findsubschems(int, objectptr, int, short *, bool);
void copypinlabel(labelptr);
int checkschem(objectptr, char *);
int checksym(objectptr, char *);
int changeotherpins(labelptr, stringpart *);
void swapschem(int, int, char *);
void dobeforeswap(QAction*, void*, void*);
void schemdisassoc(void);
void startschemassoc(QAction*, void*, void*);
bool schemassoc(objectptr, objectptr);
void xlib_swapschem(Widget, void*, void*);

/* from selection.c: */

void enable_selects(objectptr, short *, int);
void disable_selects(objectptr, short *, int);
void selectfilter(QAction *, void*, void*);
bool checkselect(short, bool draw_selected = false);
void geneasydraw(Context*, short, int, objectptr, objinstptr);
void gendrawselected(Context*, short *, objectptr, objinstptr);
selection *genselectelement(short, u_char, objectptr, objinstptr);
short *allocselect(void);
void setoptionmenu(void);
int test_insideness(int, int, const XPoint *);
bool pathselect(genericptr *, short, float);
bool areaelement(genericptr *, bool, short);
bool selectarea(objectptr, short);
void startdesel(QAction*, void*, void*);
void deselect(Widget, caddr_t, caddr_t);
void freeselects(void);
void draw_all_selected(Context*);
void clearselects_noundo(void);
void clearselects(void);
void unselect_all(void);
void select_connected_pins();
void reset_cycles();
selection *recurselect(short, u_char, pushlistptr *);
short *recurse_select_element(short, u_char);
void startselect(void);
void trackselarea(void);
void trackrescale(void);
pointselect *addcycle(genericptr *, short, u_char);
void copycycles(pointselect **, pointselect * const*);
void advancecycle(genericptr *, short);
void removecycle(genericptr);
bool checkforcycles(short *, int);
void makerefcycle(pointselect *, short);

/* from text.c: */

bool hasparameter(labelptr);
void joinlabels(void);
void drawparamlabels(labelptr, short);
stringpart *nextstringpart(stringpart *, objinstptr);
stringpart *nextstringpartrecompute(stringpart *, objinstptr);
stringpart *makesegment(stringpart **, stringpart *);
stringpart *splitstring(int, stringpart **, objinstptr);
stringpart *mergestring(stringpart *);
stringpart *linkstring(objinstptr, stringpart *, bool);
int findcurfont(int, stringpart *, objinstptr);
stringpart *findtextinstring(const char *, int *, stringpart *, objinstptr);
stringpart *findstringpart(int, int *, stringpart *, objinstptr);
QString charprint(stringpart *, int);
char *stringprint(stringpart *, objinstptr);
char *textprint(stringpart *, objinstptr);
char *textprintsubnet(stringpart *, objinstptr, int);
char *textprintnet(const char *, char *, Genericlist *);
int textcomp(stringpart *, const char *, objinstptr);
int textncomp(stringpart *, const char *, objinstptr);
int stringcomp(stringpart *, stringpart *);
bool issymbolfont(int);
bool issansfont(int);
bool isisolatin1(int);
int stringcomprelaxed(stringpart *, stringpart *, objinstptr);
int stringparts(stringpart *);
int stringlength(stringpart *, bool, objinstptr);
stringpart *stringcopy(stringpart *);
stringpart *stringcopyall(stringpart *, objinstptr);
stringpart *stringcopyback(stringpart *, objinstptr);
stringpart *deletestring(stringpart *, stringpart **, objinstptr);
Genericlist *break_up_bus(labelptr, objinstptr, Genericlist *);
int sub_bus_idx(labelptr, objinstptr);
bool pin_is_bus(labelptr, objinstptr);
int find_cardinal(int, labelptr, objinstptr);
int find_ordinal(int, labelptr, objinstptr);

short UDrawChar(Context*, u_char, short, short, int, int);
void UDrawString(Context*, labelptr, int, objinstptr, bool drawX = true);
TextExtents ULength(Context*, const label *, objinstptr, float, short, XPoint *);
void composefontlib(short);
void fontcat_op(int, int, int);

/* from xcircuit.c: */

void Wprintf(const char *, ...);
void W1printf(const char *, ...);
void W2printf(const char *, ...);
void W3printf(const char *, ...);

XCWindowData *create_new_window(void);
void pre_initialize(void);
void post_initialize(void);
void delete_window(XCWindowDataPtr);
void printeventmode(void);

void getproptext(Widget, void*, void*);
caddr_t CvtStringToPixel(XrmValuePtr, int *, XrmValuePtr, XrmValuePtr);
void outputpopup(QAction*, void*, void*);
void docommand(void);
int  installowncmap(void);  /* sometimes from xtgui.c */
void dointr(int);
void DoNothing(QAction*, void*, void*);
u_short countchanges(char **);
u_short getchanges(objectptr);
void quitcheck(QAction*, void*, void*);
void quit(QAction*, void*, void*);
void writescalevalues(char *, char *, char *);
#ifdef TCL_WRAPPER
Tcl_Obj *Tcl_NewHandleObj(void *);
int Tcl_GetHandleFromObj(Tcl_Interp *, Tcl_Obj *, void **);
#else
void updatetext(QObject*, void*, void*);
void delwin(Widget, popupstruct *, void*);
#endif


void makecursors(void);

#endif
