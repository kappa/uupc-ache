#ifndef UUNDIR_H
#define UUNDIR_H

#ifdef WIN32
#include <time.h>
#endif
#ifdef __WATCOMC__
#include <direct.h>
#else
#include <sys/types.h>
#include <dirent.h>
#endif

/*--------------------------------------------------------------------*/
/*       u u n d i r . h                                              */
/*                                                                    */
/*       UUPC/extended directory search functions                     */
/*--------------------------------------------------------------------*/

/*--------------------------------------------------------------------*/
/*       ndir.h for MS-DOS by Samuel Lam <skl@van-bc.UUCP>, June/87   */
/*--------------------------------------------------------------------*/

/*--------------------------------------------------------------------*/
/*    Changes Copyright (c) 1989-1994 by Kendra Electronic            */
/*    Wonderworks.                                                    */
/*                                                                    */
/*    All rights reserved except those explicitly granted by the      */
/*    UUPC/extended license agreement.                                */
/*--------------------------------------------------------------------*/

/*--------------------------------------------------------------------*/
/*                          RCS Information                           */
/*--------------------------------------------------------------------*/

/*
 *    $Id: uundir.h 1.7 1994/01/01 19:10:14 ahd Exp $
 *
 *    Revision history:
 *    $Log: uundir.h $
 *     Revision 1.7  1994/01/01  19:10:14  ahd
 *     Annual Copyright Update
 *
 *     Revision 1.6  1993/12/23  03:19:49  rommel
 *     OS/2 32 bit support for additional compilers
 *
 *     Revision 1.5  1993/11/06  17:57:46  rhg
 *     Drive Drew nuts by submitting cosmetic changes mixed in with bug fixes
 *
 *     Revision 1.4  1993/10/12  01:22:27  ahd
 *     Normalize comments to PL/I style
 *
 *     Revision 1.3  1993/04/10  21:35:30  dmwatt
 *     Windows/NT fixes
 *
 *     Revision 1.2  1993/04/05  04:38:55  ahd
 *     Add time stamp/size information
 *
 */

#ifndef FAMILY_API

#ifndef _MSC_VER
#define _MSC_VER 0
#endif
#if _MSC_VER >= 800
#pragma warning(disable:4214)   /* suppress non-standard bit-field warnings */
#elif _MSC_VER >= 700
#pragma warning(disable:4001)   /* suppress non-standard bit-field warnings */
#endif

#if _MSC_VER >= 800
#pragma warning(default:4214)   /* restore non-standard bit-field warnings */
#elif _MSC_VER >= 700
#pragma warning(disable:4001)   /* restore non-standard bit-field warnings */
#endif

#endif

#endif
