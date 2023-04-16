
/**
 *  (c) 2023 Marcin Ślusarczyk (quakcin)
 *      https://github.com/qs-lang/qs21
 */

#include <stdio.h>
#include <stdlib.h>
#include "qs21.h"

int main (int argc, char * argv[])
{
  /**
   *  1. create qs vm and link stdlib 
   */

  qs_t * qvm = qs_ctor();
  qs_lib(qvm);

  /**
   *  2. copy argv[] to vmem
   */
  int i;

  for (i = 2; i < argc; i++)
  {
    char buff[0xFF];
    sprintf(buff, "args-%d", i - 2);
    qs_vmem_def(qvm, buff, qs_str_alloc(argv[i]), NULL);
  }

  char len[32];
  sprintf(len, "%d", argc - 2);
  qs_vmem_def(qvm, "args-len", qs_str_alloc(len), NULL);


  /**
   *  3. execute given script - argv[1]
   */

  char expr[0xFF];
  sprintf(expr, "{use> %s}", argv[1]);
  char * rets = qs_eval(qvm, expr);

  /**
   *  4. clean up
   */

  free(rets);
  qs_dtor(qvm); 

  return 0;
}
