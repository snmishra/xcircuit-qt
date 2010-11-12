/*--------------------------------------------------------------*/
/* keybindings.c:  List of key bindings				*/
/* Copyright (c) 2002  Tim Edwards, Johns Hopkins University	*/
/*--------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*      written by Tim Edwards, 2/27/01                                 */
/*----------------------------------------------------------------------*/

#include <qnamespace.h>
#include <QKeySequence>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cmath>

#if defined(TCL_WRAPPER)
#define  XK_MISCELLANY
#define  XK_LATIN1
#define  XK_XKB_KEYS
#endif

#ifdef TCL_WRAPPER 
#include <tk.h>
#endif

#include "xcircuit.h"
#include "prototypes.h"
#include "xcqt.h"

/*----------------------------------------------------------------------*/
/* Global variables							*/
/*----------------------------------------------------------------------*/

keybinding *keylist = NULL;

extern int pressmode;
extern XCWindowData *areawin;

#define ALL_WINDOWS (Widget)NULL

/*--------------------------------------------------------------*/
/* This list declares all the functions which can be bound to   */
/* keys.  It must match the order of the enumeration listed in	*/
/* xcircuit.h!							*/
/*--------------------------------------------------------------*/

static const char *function_names[NUM_FUNCTIONS + 1] = {
   "Page", "Justify", "Superscript", "Subscript", "Normalscript",
   "Nextfont", "Boldfont", "Italicfont", "Normalfont", "Underline",
   "Overline", "ISO Encoding", "Halfspace", "Quarterspace",
   "Special", "Tab Stop", "Tab Forward", "Tab Backward",
   "Text Return", "Text Delete", "Text Right", "Text Left",
   "Text Up", "Text Down", "Text Split", 
   "Text Home", "Text End", "Linebreak", "Parameter",
   "Parameterize Point", "Change Style", "Delete Point", "Insert Point",
   "Append Point", "Next Point", "Attach", "Next Library", "Library Directory",
   "Library Move", "Library Copy", "Library Edit", "Library Delete",
   "Library Duplicate", "Library Hide", "Library Virtual Copy",
   "Page Directory", "Library Pop", "Virtual Copy",
   "Help", "Redraw", "View", "Zoom In", "Zoom Out", "Pan",
   "Double Snap", "Halve Snap", "Write", "Rotate", "Flip X",
   "Flip Y", "Snap", "Snap To",
   "Pop", "Push", "Delete", "Select", "Box", "Arc", "Text",
   "Exchange", "Copy", "Move", "Join", "Unjoin", "Spline", "Edit",
   "Undo", "Redo", "Select Save", "Unselect", "Dashed", "Dotted",
   "Solid", "Prompt", "Dot", "Wire", "Cancel", "Nothing", "Exit",
   "Netlist", "Swap", "Pin Label", "Pin Global", "Info Label", "Graphic",
   "Select Box", "Connectivity", "Continue Element", "Finish Element",
   "Continue Copy", "Finish Copy", "Finish", "Cancel Last", "Sim",
   "SPICE", "PCB", "SPICE Flat", "Rescale", "Text Delete Param",
   NULL			/* sentinel */
};

/*--------------------------------------------------------------*/
/* Return true if the indicated key (keysym + bit-shifted state)*/
/* is bound to some function.					*/
/*--------------------------------------------------------------*/

bool ismacro(Widget window, int keywstate)
{
   keybinding *ksearch;

   for (ksearch = keylist; ksearch != NULL; ksearch = ksearch->nextbinding)
      if (ksearch->window == ALL_WINDOWS || ksearch->window == window)
         if (keywstate == ksearch->keywstate)
	    return true;

   return false;
}

/*--------------------------------------------------------------*/
/* Return the first key binding for the indicated function	*/
/*--------------------------------------------------------------*/

int firstbinding(Widget window, int function)
{
   keybinding *ksearch;
   int keywstate = -1;

   for (ksearch = keylist; ksearch != NULL; ksearch = ksearch->nextbinding) {
      if (ksearch->function == function) {
         if (ksearch->window == window)
	    return ksearch->keywstate;
         else if (ksearch->window == ALL_WINDOWS)
	    keywstate = ksearch->keywstate;
      }
   }
   return keywstate;
}

/*--------------------------------------------------------------*/
/* Find the first function bound to the indicated key that is	*/
/* compatible with the current state (eventmode).  Window-	*/
/* specific bindings shadow ALL_WINDOWS bindings.  Return the	*/
/* function number if found, or -1 if no compatible functions	*/
/* are bound to the key state.					*/
/*--------------------------------------------------------------*/

int boundfunction(Widget window, int keywstate, short *retnum)
{
   keybinding *ksearch;
   int tempfunc = -1;

   for (ksearch = keylist; ksearch != NULL; ksearch = ksearch->nextbinding) {
      if (keywstate == ksearch->keywstate) {
	 if (compatible_function(ksearch->function)) {
	    if (ksearch->window == window) {
	       if (retnum != NULL) *retnum = (ksearch->value);
	       return ksearch->function;
	    }
	    else if (ksearch->window == ALL_WINDOWS) {
	       if (retnum != NULL) *retnum = (ksearch->value);
	       tempfunc = ksearch->function;
	    }
	 }
      }
   }
   return tempfunc;
}

/*--------------------------------------------------------------*/
/* Check if an entry exists for a given key-function pair	*/
/*--------------------------------------------------------------*/

int isbound(Widget window, int keywstate, int function, short value)
{
   keybinding *ksearch;

   for (ksearch = keylist; ksearch != NULL; ksearch = ksearch->nextbinding)
      if (keywstate == ksearch->keywstate && function == ksearch->function)
	 if (window == ALL_WINDOWS || window == ksearch->window ||
		ksearch->window == ALL_WINDOWS)
	    if (value == -1 || value == ksearch->value || ksearch->value == -1)
	       return true;

   return false;
}

/*--------------------------------------------------------------*/
/* Return the string associated with a function, or NULL if the	*/
/* function value is out-of-bounds.				*/
/*--------------------------------------------------------------*/

QString func_to_string(int function)
{
   if ((function < 0) || (function >= NUM_FUNCTIONS)) return QString();
   return function_names[function];
}

/*--------------------------------------------------------------*/
/* Identify a function with the string version of its name	*/
/*--------------------------------------------------------------*/

int string_to_func(const char *funcstring, short *value)
{
   int i;

   for (i = 0; i < NUM_FUNCTIONS; i++)
   {
      if (function_names[i] == NULL) {
	 Fprintf(stderr, "Error: resolve bindings and function strings!\n");
	 return -1;	  /* should not happen? */
      }
      if (!strcmp(funcstring, function_names[i]))
	 return i;
   }

   /* Check if this string might have a value attached */

   if (value != NULL)
      for (i = 0; i < NUM_FUNCTIONS; i++)
         if (!strncmp(funcstring, function_names[i], strlen(function_names[i]))) {
	    sscanf(funcstring + strlen(function_names[i]), "%hd", value);
	    return i;
	 }

   return -1;
}

/*--------------------------------------------------------------*/
/* Make a key sym from a string representing the key state 	*/
/*--------------------------------------------------------------*/

int string_to_key(const char *keystring)
{
    QString s = QString::fromLocal8Bit(keystring);

    s.replace("Button1", "F30");
    s.replace("Button2", "F31");
    s.replace("Button3", "F32");
    s.replace("Button4", "F33");
    s.replace("Button5", "F34");
    s.replace("Hold", "Mode");

    // TODO: there was a hack here: presence of SHIFT would automatically uppercase
    // the last character; this may be unnecessary
    QKeySequence ks(s);
    Q_ASSERT(ks.count() == 1);
    return ks[0];
}

/*--------------------------------------------------------------*/
/* Convert a function into a string representing all of its	*/
/* key bindings.						*/
/*--------------------------------------------------------------*/

QString function_binding_to_string(Widget window, int function)
{
   QString tmpstr, retstr;
   keybinding *ksearch;
   bool first = true;

   for (ksearch = keylist; ksearch != NULL; ksearch = ksearch->nextbinding) {
      if (function == ksearch->function) {
         if (ksearch->window == ALL_WINDOWS || ksearch->window == window) {
	    tmpstr = key_to_string(ksearch->keywstate);
            if (! tmpstr.isEmpty()) {
               if (!first) retstr += ", ";
               retstr += tmpstr;
	    }
	    first = false;
	 }
      }
   }
   if (retstr.isEmpty()) retstr = "<unbound>";
   return retstr;
}

/*--------------------------------------------------------------*/
/* Convert a key into a string representing all of its function	*/
/* bindings.							*/
/*--------------------------------------------------------------*/

QString key_binding_to_string(Widget window, int keywstate)
{
   keybinding *ksearch;
   QString retstr, tmpstr;
   bool first = true;

   for (ksearch = keylist; ksearch != NULL; ksearch = ksearch->nextbinding) {
      if (keywstate == ksearch->keywstate) {
         if (ksearch->window == ALL_WINDOWS || ksearch->window == window) {
	    tmpstr = function_names[ksearch->function];
            if (! tmpstr.isEmpty()) {
               if (!first) retstr += ", ";
               retstr += tmpstr;
	    }
	    first = false;
	 }
      }
   }
   if (retstr.isEmpty()) retstr = "<unbound>";
   return retstr;
}

/*--------------------------------------------------------------*/
/* Return an allocated string name of the function that		*/
/* is bound to the indicated key state for the indicated	*/
/* window and compattible with the current event mode.		*/
/*--------------------------------------------------------------*/

QString compat_key_to_string(Widget window, int keywstate)
{
   QString retstr;
   int function;

   function = boundfunction(window, keywstate, NULL);
   retstr = func_to_string(function);

   /* Pass back "Nothing" for unbound key states, since a	*/
   /* wrapper script may want to use the result as an action.	*/

   if (retstr.isEmpty()) retstr = "Nothing";

   return retstr;
}

/*--------------------------------------------------------------*/
/* Convert a key sym into a string				*/
/*--------------------------------------------------------------*/

QString key_to_string(int keywstate)
{
    QKeySequence ks(keywstate);
    QString s = ks.toString();
    s.replace("F30", "Button1");
    s.replace("F31", "Button2");
    s.replace("F32", "Button3");
    s.replace("F33", "Button4");
    s.replace("F34", "Button5");
    s.replace("Mode", "Hold");
    return s;
}

/*--------------------------------------------------------------*/
/* Print the bindings for the (polygon) edit functions		*/
/*--------------------------------------------------------------*/

void printeditbindings()
{
   QString str;
   const int functions[] = { XCF_Edit_Delete, XCF_Edit_Insert, XCF_Edit_Param, XCF_Edit_Next };

   for (int i = 0; i < XtNumber(functions); ++ i)  {
       str = key_to_string(firstbinding(areawin->viewport, functions[i])) + "=";
       if (i < XtNumber(functions)-1) str += func_to_string(XCF_Edit_Delete) + ", ";
   }

   /* Use W3printf().  In the Tcl version, this prints to the	*/
   /* message window but does not duplicate the output to 	*/
   /* stdout, where it would be just an annoyance.		*/

   W3printf("%ls", str.utf16());
}

/*--------------------------------------------------------------*/
/* Remove a key binding from the list				*/
/*								*/
/* Note:  This routine needs to correctly handle ALL_WINDOWS	*/
/* bindings that shadow specific window bindings.		*/
/*--------------------------------------------------------------*/

int remove_binding(Widget window, int keywstate, int function)
{
   keybinding *ksearch, *klast = NULL;

   for (ksearch = keylist; ksearch != NULL; ksearch = ksearch->nextbinding) {
      if (window == ALL_WINDOWS || window == ksearch->window) {
         if ((function == ksearch->function)
		&& (keywstate == ksearch->keywstate)) {
	    if (klast == NULL)
	       keylist = ksearch->nextbinding;
	    else
	       klast->nextbinding = ksearch->nextbinding;
            delete ksearch;
	    return 0;
         }
      }
      klast = ksearch;
   }
   return -1;
}

/*--------------------------------------------------------------*/
/* Wrapper for remove_binding					*/
/*--------------------------------------------------------------*/

void remove_keybinding(Widget window, const char *keystring, const char *fstring)
{
   int function = string_to_func(fstring, NULL);
   int keywstate = string_to_key(keystring);

   if ((function < 0) || (remove_binding(window, keywstate, function) < 0)) {
      Wprintf("Key binding \'%s\' to \'%s\' does not exist in list.",
		keystring, fstring);
   }
}

/*--------------------------------------------------------------*/
/* Add a key binding to the list				*/
/*--------------------------------------------------------------*/

int add_binding(Widget window, int keywstate, int function, short value)
{
   keybinding *newbinding;

   /* If key is already bound to the function, ignore it */

   if (isbound(window, keywstate, function, value)) return 1;
	   
   /* Add the new key binding */

   newbinding = new keybinding;
   newbinding->window = window;
   newbinding->keywstate = keywstate;
   newbinding->function = function;
   newbinding->value = value;
   newbinding->nextbinding = keylist;
   keylist = newbinding;
   return 0;
}

/*--------------------------------------------------------------*/
/* Wrapper function for key binding with function as string	*/
/*--------------------------------------------------------------*/

int add_keybinding(Widget window, const char *keystring, const char *fstring)
{
   short value = -1;
   int function = string_to_func(fstring, &value);
   int keywstate = string_to_key(keystring);

   if (function < 0)
      return -1;
   else
      return add_binding(window, keywstate, function, value);
}

/*--------------------------------------------------------------*/
/* Create list of default key bindings.				*/
/* These are conditional upon any bindings set in the startup	*/
/* file .xcircuitrc.						*/
/*--------------------------------------------------------------*/

void default_keybindings()
{
   add_binding(ALL_WINDOWS, '1', XCF_Page, 1);
   add_binding(ALL_WINDOWS, '2', XCF_Page, 2);
   add_binding(ALL_WINDOWS, '3', XCF_Page, 3);
   add_binding(ALL_WINDOWS, '4', XCF_Page, 4);
   add_binding(ALL_WINDOWS, '5', XCF_Page, 5);
   add_binding(ALL_WINDOWS, '6', XCF_Page, 6);
   add_binding(ALL_WINDOWS, '7', XCF_Page, 7);
   add_binding(ALL_WINDOWS, '8', XCF_Page, 8);
   add_binding(ALL_WINDOWS, '9', XCF_Page, 9);
   add_binding(ALL_WINDOWS, '0', XCF_Page, 10);

   add_binding(ALL_WINDOWS, SHIFT | XK_KP_1, XCF_Justify, 0);
   add_binding(ALL_WINDOWS, SHIFT | XK_KP_2, XCF_Justify, 1);
   add_binding(ALL_WINDOWS, SHIFT | XK_KP_3, XCF_Justify, 2);
   add_binding(ALL_WINDOWS, SHIFT | XK_KP_4, XCF_Justify, 3);
   add_binding(ALL_WINDOWS, SHIFT | XK_KP_5, XCF_Justify, 4);
   add_binding(ALL_WINDOWS, SHIFT | XK_KP_6, XCF_Justify, 5);
   add_binding(ALL_WINDOWS, SHIFT | XK_KP_7, XCF_Justify, 6);
   add_binding(ALL_WINDOWS, SHIFT | XK_KP_8, XCF_Justify, 7);
   add_binding(ALL_WINDOWS, SHIFT | XK_KP_9, XCF_Justify, 8);

   add_binding(ALL_WINDOWS, XK_KP_End, XCF_Justify, 0);
   add_binding(ALL_WINDOWS, XK_KP_Down, XCF_Justify, 1);
   add_binding(ALL_WINDOWS, XK_KP_Next, XCF_Justify, 2);
   add_binding(ALL_WINDOWS, XK_KP_Left, XCF_Justify, 3);
   add_binding(ALL_WINDOWS, XK_KP_Begin, XCF_Justify, 4);
   add_binding(ALL_WINDOWS, XK_KP_Right, XCF_Justify, 5);
   add_binding(ALL_WINDOWS, XK_KP_Home, XCF_Justify, 6);
   add_binding(ALL_WINDOWS, XK_KP_Up, XCF_Justify, 7);
   add_binding(ALL_WINDOWS, XK_KP_Prior, XCF_Justify, 8);

   add_binding(ALL_WINDOWS, XK_Delete, XCF_Text_Delete);
   add_binding(ALL_WINDOWS, XK_Return, XCF_Text_Return);
   add_binding(ALL_WINDOWS, BUTTON1, XCF_Text_Return);
   add_binding(ALL_WINDOWS, XK_BackSpace, XCF_Text_Delete);
   add_binding(ALL_WINDOWS, XK_Left, XCF_Text_Left);
   add_binding(ALL_WINDOWS, XK_Right, XCF_Text_Right);
   add_binding(ALL_WINDOWS, XK_Up, XCF_Text_Up);
   add_binding(ALL_WINDOWS, XK_Down, XCF_Text_Down);
   add_binding(ALL_WINDOWS, ALT | Qt::Key_X, XCF_Text_Split);
   add_binding(ALL_WINDOWS, XK_Home, XCF_Text_Home);
   add_binding(ALL_WINDOWS, XK_End, XCF_Text_End);
   add_binding(ALL_WINDOWS, XK_Tab, XCF_TabForward);
   add_binding(ALL_WINDOWS, SHIFT | XK_Tab, XCF_TabBackward);
#ifdef XK_ISO_Left_Tab
   add_binding(ALL_WINDOWS, SHIFT | XK_ISO_Left_Tab, XCF_TabBackward);
#endif
   add_binding(ALL_WINDOWS, ALT | XK_Tab, XCF_TabStop);
   add_binding(ALL_WINDOWS, XK_KP_Add, XCF_Superscript);
   add_binding(ALL_WINDOWS, XK_KP_Subtract, XCF_Subscript);
   add_binding(ALL_WINDOWS, XK_KP_Enter, XCF_Normalscript);
   add_binding(ALL_WINDOWS, ALT | Qt::Key_F, XCF_Font, 1000);
   add_binding(ALL_WINDOWS, ALT | Qt::Key_B, XCF_Boldfont);
   add_binding(ALL_WINDOWS, ALT | Qt::Key_I, XCF_Italicfont);
   add_binding(ALL_WINDOWS, ALT | Qt::Key_N, XCF_Normalfont);
   add_binding(ALL_WINDOWS, ALT | Qt::Key_U, XCF_Underline);
   add_binding(ALL_WINDOWS, ALT | Qt::Key_O, XCF_Overline);
   add_binding(ALL_WINDOWS, ALT | Qt::Key_E, XCF_ISO_Encoding);
   add_binding(ALL_WINDOWS, ALT | XK_Return, XCF_Linebreak);
   add_binding(ALL_WINDOWS, ALT | Qt::Key_H, XCF_Halfspace);
   add_binding(ALL_WINDOWS, ALT | Qt::Key_Q, XCF_Quarterspace);
#ifndef TCL_WRAPPER
   add_binding(ALL_WINDOWS, ALT | Qt::Key_P, XCF_Parameter);
#endif
   add_binding(ALL_WINDOWS, '\\', XCF_Special);
   add_binding(ALL_WINDOWS, ALT | Qt::Key_C, XCF_Special);
   add_binding(ALL_WINDOWS, 'p', XCF_Edit_Param);
   add_binding(ALL_WINDOWS, 'd', XCF_Edit_Delete);
   add_binding(ALL_WINDOWS, XK_Delete, XCF_Edit_Delete);
   add_binding(ALL_WINDOWS, 'i', XCF_Edit_Insert);
   add_binding(ALL_WINDOWS, XK_Insert, XCF_Edit_Insert);
   add_binding(ALL_WINDOWS, 'e', XCF_Edit_Next);
   add_binding(ALL_WINDOWS, BUTTON1, XCF_Edit_Next);
   add_binding(ALL_WINDOWS, 'A', XCF_Attach);
   add_binding(ALL_WINDOWS, 'V', XCF_Virtual);
   add_binding(ALL_WINDOWS, 'l', XCF_Next_Library);
   add_binding(ALL_WINDOWS, 'L', XCF_Library_Directory);
   add_binding(ALL_WINDOWS, 'c', XCF_Library_Copy);
   add_binding(ALL_WINDOWS, 'E', XCF_Library_Edit);
   add_binding(ALL_WINDOWS, 'e', XCF_Library_Edit);
   add_binding(ALL_WINDOWS, 'D', XCF_Library_Delete);
   add_binding(ALL_WINDOWS, 'C', XCF_Library_Duplicate);
   add_binding(ALL_WINDOWS, 'H', XCF_Library_Hide);
   add_binding(ALL_WINDOWS, 'V', XCF_Library_Virtual);
   add_binding(ALL_WINDOWS, 'M', XCF_Library_Move);
   add_binding(ALL_WINDOWS, 'm', XCF_Library_Move);
   add_binding(ALL_WINDOWS, 'p', XCF_Page_Directory);
   add_binding(ALL_WINDOWS, '<', XCF_Library_Pop);
   add_binding(ALL_WINDOWS, HOLD | BUTTON1, XCF_Library_Pop);
   add_binding(ALL_WINDOWS, 'h', XCF_Help);
   add_binding(ALL_WINDOWS, '?', XCF_Help);
   add_binding(ALL_WINDOWS, ' ', XCF_Redraw);
   add_binding(ALL_WINDOWS, XK_Redo, XCF_Redraw);
   add_binding(ALL_WINDOWS, XK_Undo, XCF_Redraw);
   add_binding(ALL_WINDOWS, XK_Home, XCF_View);
   add_binding(ALL_WINDOWS, 'v', XCF_View);
   add_binding(ALL_WINDOWS, 'Z', XCF_Zoom_In);
   add_binding(ALL_WINDOWS, 'z', XCF_Zoom_Out);
   add_binding(ALL_WINDOWS, 'p', XCF_Pan, 0);
   add_binding(ALL_WINDOWS, '+', XCF_Double_Snap);
   add_binding(ALL_WINDOWS, '-', XCF_Halve_Snap);
   /// \todo handled by the scroll area, we should pass those on when
   /// not needed
   add_binding(ALL_WINDOWS, XK_Left, XCF_Pan, 1);
   add_binding(ALL_WINDOWS, XK_Right, XCF_Pan, 2);
   add_binding(ALL_WINDOWS, XK_Up, XCF_Pan, 3);
   add_binding(ALL_WINDOWS, XK_Down, XCF_Pan, 4);
   add_binding(ALL_WINDOWS, 'W', XCF_Write);
   add_binding(ALL_WINDOWS, 'O', XCF_Rotate, -5);
   add_binding(ALL_WINDOWS, 'o', XCF_Rotate, 5);
   add_binding(ALL_WINDOWS, 'R', XCF_Rotate, -15);
   add_binding(ALL_WINDOWS, 'r', XCF_Rotate, 15);
   add_binding(ALL_WINDOWS, 'f', XCF_Flip_X);
   add_binding(ALL_WINDOWS, 'F', XCF_Flip_Y);
   add_binding(ALL_WINDOWS, 'S', XCF_Snap);
   add_binding(ALL_WINDOWS, '<', XCF_Pop);
   add_binding(ALL_WINDOWS, '>', XCF_Push);
   add_binding(ALL_WINDOWS, XK_Delete, XCF_Delete);
   add_binding(ALL_WINDOWS, 'd', XCF_Delete);
   add_binding(ALL_WINDOWS, XK_F19, XCF_Select);
   add_binding(ALL_WINDOWS, 'b', XCF_Box);
   add_binding(ALL_WINDOWS, 'a', XCF_Arc);
   add_binding(ALL_WINDOWS, 't', XCF_Text);
   add_binding(ALL_WINDOWS, 'X', XCF_Exchange);
   add_binding(ALL_WINDOWS, 'c', XCF_Copy);
   add_binding(ALL_WINDOWS, 'j', XCF_Join);
   add_binding(ALL_WINDOWS, 'J', XCF_Unjoin);
   add_binding(ALL_WINDOWS, 's', XCF_Spline);
   add_binding(ALL_WINDOWS, 'e', XCF_Edit);
   add_binding(ALL_WINDOWS, 'u', XCF_Undo);
   add_binding(ALL_WINDOWS, 'U', XCF_Redo);
   add_binding(ALL_WINDOWS, 'M', XCF_Select_Save);
   add_binding(ALL_WINDOWS, 'm', XCF_Select_Save);
   add_binding(ALL_WINDOWS, 'x', XCF_Unselect);
   add_binding(ALL_WINDOWS, '|', XCF_Dashed);
   add_binding(ALL_WINDOWS, ':', XCF_Dotted);
   add_binding(ALL_WINDOWS, '_', XCF_Solid);
   add_binding(ALL_WINDOWS, '%', XCF_Prompt);
   add_binding(ALL_WINDOWS, '.', XCF_Dot);
#ifndef TCL_WRAPPER
   /* TCL_WRAPPER version req's binding to specific windows */
   add_binding(ALL_WINDOWS, BUTTON1, XCF_Wire);
#endif
   add_binding(ALL_WINDOWS, 'w', XCF_Wire);
   add_binding(ALL_WINDOWS, CTRL | ALT | Qt::Key_Q, XCF_Exit);
   add_binding(ALL_WINDOWS, HOLD | BUTTON1, XCF_Move);
   add_binding(ALL_WINDOWS, BUTTON1, XCF_Continue_Element);
   add_binding(ALL_WINDOWS, BUTTON1, XCF_Continue_Copy);
   add_binding(ALL_WINDOWS, BUTTON1, XCF_Finish);
   add_binding(ALL_WINDOWS, XK_Escape, XCF_Cancel);
   add_binding(ALL_WINDOWS, ALT | Qt::Key_R, XCF_Rescale);
   add_binding(ALL_WINDOWS, ALT | Qt::Key_S, XCF_SnapTo);
   add_binding(ALL_WINDOWS, ALT | Qt::Key_Q, XCF_Netlist);
   add_binding(ALL_WINDOWS, '/', XCF_Swap);
   add_binding(ALL_WINDOWS, 'T', XCF_Pin_Label);
   add_binding(ALL_WINDOWS, 'G', XCF_Pin_Global);
   add_binding(ALL_WINDOWS, 'I', XCF_Info_Label);
   add_binding(ALL_WINDOWS, ALT | Qt::Key_W, XCF_Connectivity);

   /* These are for test purposes only.  Menu selection is	*/
   /* preferred.						*/
   if (0) {
       add_binding(ALL_WINDOWS, ALT | Qt::Key_D, XCF_Sim);
       add_binding(ALL_WINDOWS, ALT | Qt::Key_A, XCF_SPICE);
       add_binding(ALL_WINDOWS, ALT | Qt::Key_F, XCF_SPICEflat);
       add_binding(ALL_WINDOWS, ALT | Qt::Key_P, XCF_PCB);
   }

   /* 2-button vs. 3-button mouse bindings (set with -2	*/
   /* commandline option; 3-button bindings default)	*/

   if (pressmode == 1) {
      add_binding(ALL_WINDOWS, BUTTON3, XCF_Text_Return);
      add_binding(ALL_WINDOWS, BUTTON3, XCF_Select);
      add_binding(ALL_WINDOWS, HOLD | BUTTON3, XCF_SelectBox);
      add_binding(ALL_WINDOWS, BUTTON3, XCF_Finish_Element);
      add_binding(ALL_WINDOWS, BUTTON3, XCF_Finish_Copy);

      add_binding(ALL_WINDOWS, XK_BackSpace, XCF_Cancel_Last);
      add_binding(ALL_WINDOWS, XK_BackSpace, XCF_Cancel);
   }
   else {
      add_binding(ALL_WINDOWS, BUTTON2, XCF_Text_Return);
      add_binding(ALL_WINDOWS, SHIFT | BUTTON1, XCF_Text_Return);
      add_binding(ALL_WINDOWS, BUTTON2, XCF_Select);
      add_binding(ALL_WINDOWS, SHIFT | BUTTON1, XCF_Select);
      add_binding(ALL_WINDOWS, HOLD | BUTTON2, XCF_SelectBox);
      add_binding(ALL_WINDOWS, SHIFT | HOLD | BUTTON1, XCF_Select);
      add_binding(ALL_WINDOWS, BUTTON2, XCF_Finish_Element);
      add_binding(ALL_WINDOWS, SHIFT | BUTTON1, XCF_Finish_Element);
      add_binding(ALL_WINDOWS, BUTTON2, XCF_Finish_Copy);
      add_binding(ALL_WINDOWS, SHIFT | BUTTON1, XCF_Finish_Copy);
      add_binding(ALL_WINDOWS, BUTTON3, XCF_Cancel_Last);
      add_binding(ALL_WINDOWS, BUTTON3, XCF_Cancel);
   }
}

#ifndef TCL_WRAPPER
/*----------------------------------------------*/
/* Mode-setting rebindings (non-Tcl version)	*/
/*----------------------------------------------*/

static int button1mode = XCF_Wire;

/*--------------------------------------------------------------*/
/* Re-bind BUTTON1 to the indicated function and optional value */
/*--------------------------------------------------------------*/

void mode_rebinding(int newmode, int newvalue)
{
   Widget window = areawin->viewport;

   remove_binding(window, BUTTON1, button1mode);
   add_binding(window, BUTTON1, newmode, (short)newvalue);
   button1mode = newmode;
   toolcursor(newmode);
}

/*--------------------------------------------------------------*/
/* Execute the function associated with the indicated BUTTON1	*/
/* mode, but return the keybinding to its previous state.	*/
/*--------------------------------------------------------------*/

void mode_tempbinding(int newmode, int newvalue)
{
   short saveval;
   XPoint cpos;
   Widget window = areawin->viewport;

   if (boundfunction(window, BUTTON1, &saveval) == button1mode) {
      remove_binding(window, BUTTON1, button1mode);
      add_binding(window, BUTTON1, newmode, (short)newvalue);
      cpos = UGetCursor();
      eventdispatch(BUTTON1, (int)cpos.x, (int)cpos.y);
      remove_binding(window, BUTTON1, newmode);
      add_binding(window, BUTTON1, button1mode, saveval);
   }
   else
      fprintf(stderr, "Error: No such button1 binding %s\n",
                func_to_string(button1mode).toLocal8Bit().data());
}

#endif /* TCL_WRAPPER */

#undef ALT
#undef CTRL
#undef CAPSLOCK
#undef SHIFT
#undef BUTTON1
#undef BUTTON2
#undef BUTTON3

/*--------------------------------------------------------------*/
