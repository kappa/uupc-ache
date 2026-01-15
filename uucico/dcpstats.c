/*--------------------------------------------------------------------*/
/*    File transfer information for UUPC/extended                     */
/*                                                                    */
/*    Copyright (c) 1991, Andrew H. Derbyshire                        */
/*--------------------------------------------------------------------*/

/*--------------------------------------------------------------------*/
/*                       standard include files                       */
/*--------------------------------------------------------------------*/

#include "uupcmoah.h"

/*--------------------------------------------------------------------*/
/*                    UUPC/extended include files                     */
/*--------------------------------------------------------------------*/

#include "dcp.h"
#include "dcpstats.h"
#include "hostable.h"
#include "hostatus.h"
#include "timestmp.h"
#include "lock.h"
#include "ssleep.h"
#include "stater.h"

extern requests, totreqs;

/*--------------------------------------------------------------------*/
/*                          Global variables                          */
/*--------------------------------------------------------------------*/

currentfile();

/*--------------------------------------------------------------------*/
/*    d c s t a t s                                                   */
/*                                                                    */
/*    Report transmission information for a connection                */
/*--------------------------------------------------------------------*/

void dcstats(void)
{
   if (hostp == BADHOST)
   {
      printmsg(0,"dcstats: host structure pointer is NULL");
	  panic();
   }

   if (!equal(rmtname , hostp->hostname))
	  return;

   if (remote_stats.lconnect <= 0)
		return;

   {
	  time_t connected;
	  unsigned long bytes;
	  unsigned long bps;

	  connected = time(NULL) - remote_stats.lconnect;
	  remote_stats.connect += connected;
	  bytes = remote_stats.bsent + remote_stats.breceived;
	  if ( connected <= 0 )
		 connected = 1;
	  bps = bytes / connected;

	  if (requests == 0)
		  printmsg(1, "dcstats: no messages sent to %s (%s)",
						 rmtname, hostp->hostname);
	  else
		  printmsg(1, "dcstats: %d message(s) sent to %s (%s)",
						requests, rmtname, hostp->hostname);

	  printmsg(1,"%ld files sent, %ld files received, \
%ld bytes sent, %ld bytes received",
			remote_stats.fsent, remote_stats.freceived ,
			remote_stats.bsent, remote_stats.breceived);
	  printmsg(1, "%ld packets transferred, %ld errors, \
connection time %ld:%02ld, %ld bytes/second",
			(long) remote_stats.packets,
			(long) remote_stats.errors,
			(long) connected / 60L, (long) connected % 60L, bps);

   }

   totreqs += requests;

   if (remote_stats.lconnect > hostp->status.lconnect)
      hostp->status.lconnect = remote_stats.lconnect;
   if (remote_stats.ltime > hostp->status.ltime)
      hostp->status.lconnect = remote_stats.lconnect;

   hostp->status.connect   += remote_stats.connect;
   hostp->status.calls     += remote_stats.calls;
   hostp->status.fsent     += remote_stats.fsent;
   hostp->status.freceived += remote_stats.freceived;
   hostp->status.bsent     += remote_stats.bsent;
   hostp->status.breceived += remote_stats.breceived;
   hostp->status.errors    += remote_stats.errors;
   hostp->status.packets   += remote_stats.packets;

} /* dcstats */

/*--------------------------------------------------------------------*/
/*    d c u p d a t e                                                 */
/*                                                                    */
/*    Update the status of all known hosts                            */
/*--------------------------------------------------------------------*/

LOCKSTACK savelock={"",NULL};

void dcupdate( void )
{
   boolean firsthost = TRUE;
   struct HostTable *host;
   FILE *stream;
   char fname[FILENAME_MAX];
   long size;
   unsigned short len1 = (unsigned short) strlen(compilep);
   unsigned short len2 = (unsigned short) strlen(compilev);
   boolean gotlock;
   short retries = 30;


   mkfilename( fname, E_spooldir, DCSTATUS );

/*--------------------------------------------------------------------*/
/*            Save lock status, then lock host status file            */
/*--------------------------------------------------------------------*/

   PushLock( &savelock );

   do {
      gotlock = LockSystem( "*status", B_UUSTAT );
      if ( ! gotlock )
         ssleep(2);
   } while ( ! gotlock && retries-- );

   if ( ! gotlock )
   {
      printmsg(0,"Cannot obtain lock for %s", fname );
      return;
   }

/*--------------------------------------------------------------------*/
/*                  Old previous status as required                   */
/*--------------------------------------------------------------------*/

   HostStatus();              /* Get new data, if needed          */

   if ((stream  = FOPEN(fname, "w", IMAGE_MODE)) == NULL)
   {
      printerr( fname, "open" );
      return;
   }

   fwrite( &len1, sizeof len1, 1, stream );
   fwrite( &len2, sizeof len2, 1, stream );
   fwrite( compilep , 1, len1, stream);
   fwrite( compilev , 1, len2, stream);
   fwrite( &start_stats , sizeof start_stats , 1,  stream);

   while  ((host = nexthost( firsthost )) != BADHOST)
   {
      unsigned short saveStatus = host->status.hstatus;

      len1 = (unsigned short) strlen( host->hostname );
      len2 = (unsigned short) sizeof host->status;

      firsthost = FALSE;

      fwrite( &len1, sizeof len1, 1, stream );
      fwrite( &len2, sizeof len2, 1, stream );
      fwrite( host->hostname , sizeof hostp->hostname[0], len1, stream);

      if ( host->status.hstatus == called )
	 host->status.hstatus = succeeded;

      fwrite( &host->status, len2, 1,  stream);

      host->status.hstatus = saveStatus;
			      /* Restore to insure we don't call
				 host again this invocation of UUCICO */
   }

/*--------------------------------------------------------------------*/
/*         Make we sure got end of file and not an I/O error          */
/*--------------------------------------------------------------------*/

   if (ferror( stream ))
   {
      printerr( fname, "write" );
      clearerr( stream );
   }

   fclose( stream );

   hstatus_age = stater( fname , &size );

/*--------------------------------------------------------------------*/
/*                      Restore locks and return                      */
/*--------------------------------------------------------------------*/

   UnlockSystem( );
   PopLock( &savelock );
   savelock.locket=NULL;

} /* dcupdate */
