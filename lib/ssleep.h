/*--------------------------------------------------------------------*/
/*    Header files for ssleep.c for UUPC/extended                     */
/*--------------------------------------------------------------------*/

#include <sys\timeb.h>
#include <time.h>

boolean ssleep(time_t interval);
boolean ddelay(int milliseconds);
boolean sdelay(int milliseconds);
boolean keyb_control(boolean kb_only);
long msecs_from(struct timeb *start);
void nokbd(void);

#define DELAY(n) {volatile junk; for (junk = 0; junk < (n); junk++) ;}
