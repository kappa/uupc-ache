/* Ache -- seq moved to spooldir */
/* Changed to %7s%c%4s */
/*--------------------------------------------------------------------*/
/*    g e t s e q . c                                                 */
/*                                                                    */
/*    Job sequence number routines for UUPC/extended                  */
/*--------------------------------------------------------------------*/

#include <stdio.h>
#include <io.h>
#include <time.h>

#include "lib.h"
#include "hlib.h"
#include "getseq.h"

currentfile();

/*--------------------------------------------------------------------*/
/*    g e t s e q                                                     */
/*                                                                    */
/*    Return next available sequence number for UUPC processing       */
/*--------------------------------------------------------------------*/

long getseq()
{
   char seqfile[FILENAME_MAX];
   FILE *seqfile_fp;
   long seq;

   mkfilename(seqfile, spooldir, SFILENAME);
   printmsg(4, "getseq: opening %s", seqfile);
   if ((seqfile_fp = FOPEN(seqfile, "r+", TEXT)) != nil(FILE)) {
	fd_lock(seqfile, fileno(seqfile_fp));
	if (fscanf(seqfile_fp, "%ld", &seq)!=1) {
		printmsg(0, "getseq: can't get seq num from %s", seqfile);
		seq = time(NULL);
	}
	rewind(seqfile_fp);
   } else {
	printmsg(0, "getseq: can't find %s, creating", seqfile);
	if ((seqfile_fp = FOPEN(seqfile, "w", TEXT)) == nil(FILE)) {
		printerr("getseq", seqfile);
		panic();
	}
	fd_lock(seqfile, fileno(seqfile_fp));
	seq = time(NULL);
   };

   seq %= 0x40000000l; /* sure not overflow */

/*--------------------------------------------------------------------*/
/*                       Update sequence number                       */
/*--------------------------------------------------------------------*/

   fprintf(seqfile_fp, "%ld\n", seq+1);
   fclose(seqfile_fp);

   printmsg(5, "getseq: seq#=%ld", seq);

   return seq;

} /*getseq*/

/*--------------------------------------------------------------------*/
/*    J o b N u m b e r                                               */
/*                                                                    */
/*    Given a job sequence number, returns a character string for use */
/*    in file names                                                   */
/*--------------------------------------------------------------------*/

char *JobNumber( long sequence )
{
	  static char buf[5];
	  const long base = 62;
	  static const char set[] =
		 "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	  size_t count = sizeof buf - 1;

	  buf[count] = '\0';

	  sequence %= (base*base*base);

	  while( count-- > 0 )
	  {
		 buf[count] = set[ (int) (sequence % base) ];
		 sequence /= base ;
	  } /* while */

	  return buf;

} /* JobNumber */
