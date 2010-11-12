/*-------------------------------------------------------------------------*/
/* menus.h								   */
/* Copyright (c) 2002  Tim Edwards, Johns Hopkins University        	   */
/*-------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------*/
/* Hierarchical menus must be listed in bottom-up order			   */
/*-------------------------------------------------------------------------*/
/* Note:  underscore (_) before name denotes a color to paint the button.  */
/*	  colon (:) before name denotes a stipple, defined by the data	   */
/*		    passed to setfill().				   */
/*-------------------------------------------------------------------------*/

#ifndef MENUS_H
#define MENUS_H

#include "xcircuit.h"

extern menustruct TopButtons[];
extern toolbarstruct ToolBar[];

#endif
