/*--------------------------------------------------------------------*/
/*       Program:    testimp     06/09/91                             */
/*       Author:     Andrew H. Derbyshire                             */
/*       Function:   Test UUPC/extended filename mapping              */
/*                   functions                                        */
/*                                                                    */
/*       Copyright (C) 1991, Andrew H. Derbyshire                     */
/*--------------------------------------------------------------------*/

#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <stdlib.h>

#include "lib.h"
#include "import.h"
#include "export.h"
#include "timestmp.h"

currentfile();

void main( int argc , char **argv )
{
   char canon[FILENAME_MAX];
   char host[FILENAME_MAX];
   size_t count;
   banner( argv );            /* Out of habit, I guess               */
/*--------------------------------------------------------------------*/
/*       Load system configuration and then the UUPC host stats       */
/*--------------------------------------------------------------------*/

   if (!configure())
	  panic();

   if (argc < 3) {
	printmsg(0, "main: usage: testexp system spoolfile [system spoolfile]...");
	panic();
   }
   assert( argc > 2 );
   for( count = 1; count < argc; count = count + 2)
   {
	  exportpath( canon , argv[count+1],  argv[count] );
	  printf("\nexport %s\t%s\t yields %s\n",
			   argv[count+1], canon, host );
	  importpath( host, canon, argv[count] );
	  printf("import %s\t%s\t yields %s\n",
			   canon, host, argv[count+1]);
   }
} /* void */

