#include <fcntl.h>
#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "uupcmoah.h"

/*--------------------------------------------------------------------*/
/*    c p                                                             */
/*                                                                    */
/*    Copy Local Files                                                */
/*--------------------------------------------------------------------*/

boolean cp(const char *from, boolean ftmp, const char *to, boolean ttmp)
{
      int  fd_from, fd_to;
      int  nr, md;
      int  nw = -1;
      char buf[4096];            /* faster if we alloc a big buffer */

      printmsg(5, "cp: copy from %s(%d) to %s(%d)", from, ftmp, to, ttmp);
      md = ftmp ? 0 : O_DENYNONE;
      if ((fd_from = open(from, O_RDONLY|O_BINARY|md)) == -1)
	 return FALSE;        /* failed */

      /* what if the to is a directory? */
      /* possible with local source & dest uucp */

      md = ttmp ? 0 : O_DENYNONE;
      if ((fd_to = open(to, O_CREAT|O_BINARY|O_WRONLY|md, S_IWRITE|S_IREAD)) == -1) {
	 close(fd_from);
	 return FALSE;        /* failed */
	 /* NOTE - this assumes all the required directories exist!  */
      }
      if (!ttmp)
	      fd_lock(to, fd_to);

      while  ((nr = read(fd_from, buf, sizeof buf)) > 0 &&
		(nw = write(fd_to, buf, nr)) == nr)
		;

      close(fd_to);
      close(fd_from);

      if (nr != 0 || nw == -1)
	 return FALSE;        /* failed in copy */
      return TRUE;
}

