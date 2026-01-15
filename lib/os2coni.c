#define INCL_SUB
#include <os2.h>

void clrscr(void)
{
#if 0
  VioWrtNCell(NULL, 0, 0, 0, 0);
#else
  VioWrtCellStr(NULL, 0, 0, 0, 0);
#endif
}
