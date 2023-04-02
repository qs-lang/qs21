
/**
 *  (c) 2023 Marcin Ślusarczyk (quakcin)
 *      https://github.com/qs-lang/qs21
 */

#include <stdio.h>
#include <stdlib.h>
#include "qs21.h"

int main (int argc, char * argv[])
{
  qs_t * qvm = qs_ctor();
  qs_lib(qvm);

  // execute each given script
  int i;
  for (i = 1; i < argc; i++)
  {
    char cmd[0xFF];
    sprintf(cmd, "{use> %s}", argv[i]);
    free(qs_eval(qvm, cmd));
  }

  qs_dtor(qvm); 

  return 0;
}