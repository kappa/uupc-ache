/*--------------------------------------------------------------------*/
/*    g e t s e q . h                                                 */
/*                                                                    */
/*    Header file for get sequence and related functions              */
/*--------------------------------------------------------------------*/

#define SPOOLFMT "%c.%.7s%c%4.4s"
#define DATAFFMT "%c.%.7s%c%.4s"

long getseq( void );

char *JobNumber( long sequence );
