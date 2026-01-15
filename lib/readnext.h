/*--------------------------------------------------------------------*/
/*    r e a d n e x t . h                                             */
/*                                                                    */
/*    Reads a spooling directory with optional pattern matching       */
/*                                                                    */
/*    Copyright 1991 (C), Andrew H. Derbyshire                        */
/*--------------------------------------------------------------------*/

/*
 *       $Id: READNEXT.H 1.2 1993/04/05 04:38:55 ahd Exp $
 *
 *       $Log: READNEXT.H $
 *     Revision 1.2  1993/04/05  04:38:55  ahd
 *     Return timestamp and size on file found
 *
 */

char     *readnext(char *xname,
                   const char *remote,
                   const char *subdir,
                   time_t *modified,
                   long   *size );
