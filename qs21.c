/**
 *  (c) 2023 Marcin Ślusarczyk (quakcin)
 *      https://github.com/qs-lang/qs21
 * 
 *  - tree vmem -> simple single linked list [DONE]
 *  - bring back eval for first argument in call [DONE]
 *  - remove eval function
 *  - migrate existing libs to *.qs format
 *  - create external package manager
 *    make it use compiler and source
 *    code for better effect
 *  - create auto transpiler from C AST 
 *    directly to qs21_lib bindings so
 *    implementation of common api's
 *    frameworks and libs will be really
 *    fast
 */

#include "qs21.h"

qs_t * qs_ctor ()
{
  qs_t * self = (qs_t *) malloc(sizeof(struct qs_s));

  self->alive = true;
  self->vmem = NULL;
  self->rets = NULL;
  self->cfun = NULL;

  self->vmem = (vmem_t *) malloc(sizeof(struct vmem_s));
  memset(self->vmem, 0, sizeof(struct vmem_s));

  return self;
}

void qs_dtor (qs_t * self)
{
  vmem_t * ptr, * nxt;
  for (ptr = self->vmem; ptr != NULL; ptr = nxt)
  {
    nxt = ptr->next;

    if (ptr->value != NULL)
      free(ptr->value);

    free(ptr->name);
    free(ptr);
  }
  free(self);
}

vmem_t * qs_vmem_loc (qs_t * self, char * name)
{
  vmem_t * ptr;

  for (ptr = self->vmem->next; ptr != NULL; ptr = ptr->next)
    if (strcmp(ptr->name, name) == 0)
      return ptr;
  
  return NULL;
}

void MEMDUMP (qs_t * self)
{
  printf("[memdump]\n");
  vmem_t * ptr; int i = 0;
  for (ptr = self->vmem; ptr != NULL; ptr = ptr->next)
  {
    printf("%d; %p; %s; %p; %p; %p\n", i++, ptr, ptr->name, ptr->value, ptr->cfun, ptr->next);
  }
  printf("\n\n");
}

vmem_t * qs_vmem_def (qs_t * self, /*STACK:*/char * name, /*HEAP:*/void * value, void (*cfun)(qs_t *))
{
  vmem_t * v = qs_vmem_loc(self, name);

  if (v == NULL)
  {
    v = (vmem_t *) malloc(sizeof(struct vmem_s));
    v->next = self->vmem->next;
    v->name = qs_str_alloc(name);
    self->vmem->next = v;
  }
  else
  {
    if (v->value != NULL)
      free(v->value);
  }

  if (value != NULL && cfun == NULL && strcmp(value, "") == 0)
  { /* Remove empty variables from vmem */
    vmem_t * ptr;
    for (ptr = self->vmem; ptr->next != NULL; ptr = ptr->next)
    {
      if (ptr->next == v)
      {
        ptr->next = v->next;
        free(v->name);
        free(v);
        return NULL;
      }
    }
  }

  v->value = value;
  v->cfun = cfun;
  return v;
}

strb_t * qs_strb_ctor (void)
{
  strb_t * self = (strb_t *) malloc(sizeof(struct strb_s));
  self->ptr = malloc(STRB_DEF_SIZE);
  self->size = STRB_DEF_SIZE;
  self->off = 0;
  self->ptr[0] = '\0';
  return self;
}

void qs_strb_catc (strb_t * self, char c)
{
  if (self->off + 1 >= self->size)
  {
    self->size += STRB_OVH_SIZE;
    self->ptr = realloc(self->ptr, self->size);
  }
  self->ptr[self->off++] = c;
  self->ptr[self->off] = '\0';
}

void qs_strb_strcat (strb_t * self, char * str, size_t strl)
{
  size_t maxs = self->off + strl + STRB_OVH_SIZE;
  if (self->size <= maxs)
  {
    self->ptr = realloc(self->ptr, maxs);
    self->size = maxs;
  }

  memcpy(self->ptr + self->off, str, strl);
  self->off = self->off + strl;
  self->ptr[self->off] = '\0';
}

char * qs_strb_cstr (strb_t * self)
{
  char * fin = self->ptr;
  free(self);
  return fin;
}

char * qs_eval (qs_t * self, char * expr)
{
  strb_t * rets = qs_strb_ctor();
  char * args [MAX_ARGS_SIZE];

  const size_t strl = strlen(expr);
  size_t c;

  unsigned int nst = 0;
  unsigned int arg = 0;
  unsigned int off = 0;

  bool text_mode = true;
  bool eval_next = false;

  for (c = 0; c < strl; c++)
  {
    const char cc = expr[c];
    if (text_mode)
    { /* text mode */
      if (cc == '{')
      {
        text_mode = false;
        eval_next = true;
        nst = 0;
        off = c; 
        arg = 0;
      }
      else
      { 
        if ((c + 1) >= strl || expr[c + 1] == '{')
        { 
          size_t clen = c - off + 1;
          char buff[clen + 1];
          memcpy(buff, expr + off, clen);
          buff[clen] = '\0';
          qs_strb_strcat(rets, buff, clen);
        }
      }
    } /* text mode */
    else
    { /* call mode */
      if ((nst == 0) && (cc == ':' || cc == '>' || cc == '}')) /* arg */
      { /* arg */
        size_t beg = off + 1;
        size_t end = c - 1;

        qs_str_trim(expr, &beg, &end);
        const int len = end - beg + 2;

        if (len <= 0)
        { /* empty string */
          beg = off + 1;
          end = beg - 1; 
        }

        args[arg] = (char *) malloc(sizeof(char) * (end - beg + 2));
        memcpy(args[arg], expr + beg, end - beg + 1);
        args[arg][end - beg + 1] = '\0';

        if (eval_next)
        {
          char * tmp = qs_eval(self, args[arg]);
          free(args[arg]);
          args[arg] = tmp;
        }

        eval_next = (cc == ':');
        off = c;
        arg++;
      } /* arg */
      if (cc == '{')
        nst += 1;
      if (cc == '}')
      {
        if (nst != 0)
          nst -= 1;
        else
        {
          /* def arguments */ 
          unsigned int i;
          for (i = 1; i < arg; i++)
          {
            char name [MAX_ARGN_SIZE];
            sprintf(name, "%s-%u", args[0], i - 1);
            qs_vmem_def(self, name, args[i], NULL);
          }
          char name[MAX_ARGN_SIZE];
          sprintf(name, "%s-len", args[0]);
          char * argc = malloc(sizeof(char) * 16);
          sprintf(argc, "%d", i - 1);
          qs_vmem_def(self, name, argc, NULL);
          /* now call */
          vmem_t * vmem = qs_vmem_loc(self, args[0]);
          if (vmem != NULL)
          {
            if (vmem->cfun != NULL)
            { /* env call */
              strb_t * tmpret = self->rets;
              char * tmpfun = self->cfun;
              self->cfun = args[0];
              self->rets = rets;
              vmem->cfun(self);
              self->rets = tmpret;
              self->cfun = tmpfun;
            }
            else if (vmem->value != NULL)
            { /* evaluate and append to rets */
              char * iexpr = qs_str_alloc(vmem->value);
              char * cret = qs_eval(self, iexpr);
              qs_strb_strcat(rets, cret, strlen(cret));
              free(cret);
              free(iexpr);
            }
          }

          /* cleanup */
          free(args[0]);
          text_mode = true;
          off = c + 1;

          /* end of caller */
        }
      }
    } /* call mode */
  }
  return qs_strb_cstr(rets);
}

/**
 * TODO: Make sure that return "" as char * is valid
 */
char * qs_vmem_cstr (qs_t * self, char * name)
{
  vmem_t * vmem = qs_vmem_loc(self, name);
  return vmem != NULL ? vmem->value : "";
}

char * qs_vmem_arr_cstr (qs_t * self, char * name, int index)
{
  char buff[strlen(name) + 16];
  sprintf(buff, "%s-%d", name, index);
  return qs_vmem_cstr(self, buff);
}

int qs_vmem_arr_int (qs_t * self, char * name, int index)
{
  char buff[strlen(name) + 16];
  sprintf(buff, "%s-%d", name, index);
  return qs_vmem_int(self, buff);
}

int qs_vmem_arr (qs_t * self, char * arr[], char * name, int maxlen)
{
  char buff[strlen(name) + 8];
  sprintf(buff, "%s-len", name);
  const int size = qs_vmem_int(self, buff);
  const int count = size > maxlen ? maxlen : size;
  int i;
  for (i = 0; i < count; i++)
    arr[i] = qs_vmem_arr_cstr(self, name, i);
  return count;
}

int qs_vmem_int (qs_t * self, char * name)
{
  char * str = qs_vmem_cstr(self, name);
  return str != NULL 
    ? atoi(str)
    : 0;
}

char * qs_str_alloc (char * stack)
{
  size_t len = strlen(stack);
  char * heap = malloc(sizeof(char) * (len + 1));
  memcpy(heap, stack, len + 1);
  return heap;
}

void qs_str_trim (char * str, size_t * beg, size_t * end)
{
  while (str[*beg] == ' ' || str[*beg] == '\n' || str[*beg] == '\t')
    *beg = *beg + 1; /* trim left */

  while (str[*end] == ' ' || str[*end] == '\n' || str[*end] == '\t')
    *end = *end - 1; /* trim right */
}

/*
  qs21 core lib
*/

void __qs_lib_def (qs_t * self)
{
  const int len = qs_vmem_int(self, "def-len");
  if (len <= 0)
    return;

  char * args[len];
  qs_vmem_arr(self, args, "def", len);

  if (len == 2)
  { /* do not wrap */
    qs_vmem_def(self, args[0], qs_str_alloc(args[1]), NULL);
    return;
  }

  strb_t * strb = qs_strb_ctor();
  qs_strb_strcat(strb, "{priv>", 6);
  int i; /* {def: NAME: ARG1: ARG2: ...> CEXPR}*/
  for (i = 1; i < len - 1; i++)
  { /* wrap around {priv} */
    qs_strb_strcat(strb, args[i], strlen(args[i]));
    qs_strb_catc(strb, '>');
  }
  for (i = 1; i < len - 1; i++)
  { /* append defines */
    char buff[0xFF];
    sprintf(buff, "{def>%s:{v>%s-%d}}", args[i], args[0], i - 1);
    qs_strb_strcat(strb, buff, strlen(buff));
  }
  qs_strb_strcat(strb, args[len - 1], strlen(args[len - 1]));
  qs_strb_catc(strb, '}');

  qs_vmem_def(self, args[0], qs_strb_cstr(strb), NULL);
}

void __qs_lib_use (qs_t * self)
{
  char * path = qs_vmem_cstr(self, "use-0");
  int fd = open(path, O_RDONLY);
  if (fd < 0) return; /* failed to open */

  strb_t * strb = qs_strb_ctor();
  char buff[0xFF]; size_t r;
  while ((r = read(fd, buff, sizeof(buff))) != 0)
    qs_strb_strcat(strb, buff, r);
  close(fd);
  char * expr = qs_strb_cstr(strb);

  int off = 0; /* remove \n and \t */
  size_t i; 
  for (i = 0; i < strlen(expr); i++)
    if (expr[i] == '\n' || expr[i] == '\t' || expr[i] == '\r')
      off++;
    else
      expr[i - off] = expr[i];

  expr[strlen(expr) - off] = '\0';
  char * ret = qs_eval(self, expr);
  qs_strb_strcat(self->rets, ret, strlen(ret));
  free(ret);
  free(expr);
}

void __qs_lib_priv (qs_t * self)
{ 
  strb_t * strb = qs_strb_ctor();
  const int len = qs_vmem_int(self, "priv-len");
  int i;
  for (i = 0; i < len - 1; i++)
  {
    qs_strb_strcat(strb, "{def>", 5);
      char * name = qs_vmem_arr_cstr(self, "priv", i); 
      if (name != NULL)
      {
        qs_strb_strcat(strb, name, strlen(name)); 
        qs_strb_catc(strb, '>');
        char * val = qs_vmem_cstr(self, name); 
        if (val != NULL)
          qs_strb_strcat(strb, val, strlen(val));
      }
    qs_strb_catc(strb, '}');
  }
  char * inl = qs_strb_cstr(strb);
  char * expr = qs_str_alloc(qs_vmem_arr_cstr(self, "priv", len - 1));
  char * rets = qs_eval(self, expr);
  qs_strb_strcat(self->rets, rets, strlen(rets));
  free(rets); 
  free(expr);
  free(qs_eval(self, inl));
  free(inl);
}

void __qs_lib_clc (qs_t * self)
{
  const int count = qs_vmem_int(self, "clc-len");
  int ac = qs_vmem_arr_int(self, "clc", 0);
  int i;
  for (i = 1; i < count; i += 2)
  {
    char * op = qs_vmem_arr_cstr(self, "clc", i);
    int val = qs_vmem_arr_int(self, "clc", i + 1);
    switch (op[0])
    {
      case '+': ac += val; break;
      case '-': ac -= val; break;
      case '*': ac *= val; break;
      case '/': ac /= val; break;
      case '%': ac %= val; break;
      case '&': ac &= val; break;
      case '|': ac |= val; break;
      case '^': ac ^= val; break;
    }
  }
  char buff[16];
  sprintf(buff, "%d", ac);
  qs_strb_strcat(self->rets, buff, strlen(buff));
}

void __qs_lib_strlen (qs_t * self)
{
  char buff[32];
  sprintf(buff, "%d", (int) strlen(qs_vmem_cstr(self, "strlen-0")));
  qs_strb_strcat(self->rets, buff, strlen(buff));
}

void __qs_lib_do (qs_t * self)
{
  char * iter = qs_str_alloc(qs_vmem_cstr(self, "do-0"));
  char * expr = qs_str_alloc(qs_vmem_cstr(self, "do-3"));
  const int from = qs_vmem_int(self, "do-1");
  int to = qs_vmem_int(self, "do-2");
  const int delta = from < to ? 1 : -1;
  to = delta > 0 ? to + 1 : to - 1;
  int i;
  for (i = from; (i + delta) != to; i += delta)
  {
    char buff[strlen(expr) + 0xFF]; 
    sprintf(buff, "{priv>%s>{def>%s>%d}%s}", iter, iter, i, expr);
    char * rets = qs_eval(self, buff);
    qs_strb_strcat(self->rets, rets, strlen(rets));
    free(rets);
  }
  free(iter); free(expr);
}

void __qs_lib_if (qs_t * self)
{
  bool test = false;
  const int left = qs_vmem_int(self, "if-0");
  const int right = qs_vmem_int(self, "if-2");
  char * op = qs_vmem_cstr(self, "if-1");
  switch (op[0])
  {
    case '=': test = left == right; break;
    case '(': test = left < right; break;
    case ')': test = left > right; break;
    case '[': test = left <= right; break;
    case ']': test = left >= right; break;
    case '!': test = left != right; break;
    case '~': test = strcmp(qs_vmem_cstr(self, "if-0"), qs_vmem_cstr(self, "if-2")) == 0; break;
  }
  op = NULL; // better not dangle
  char * expr = qs_str_alloc(test 
    ? qs_vmem_cstr(self, "if-3") 
    : qs_vmem_cstr(self, "if-4"));

  char * ret = qs_eval(self, expr);
  qs_strb_strcat(self->rets, ret, strlen(ret));
  free(ret); free(expr);
}

void __qs_lib_chr (qs_t * self)
{
  qs_strb_catc(self->rets, (char) qs_vmem_int(self, "chr-0"));
}

void __qs_lib_asc (qs_t * self)
{ 
  char * str = qs_vmem_cstr(self, "asc-0");
  if (str == NULL)  
    return;
  char buff[32];
  int idx = qs_vmem_int(self, "asc-1");
  if (idx < 0 || idx >= (int) strlen(str))
    return;

  sprintf(buff, "%d", (int) str[qs_vmem_int(self, "asc-1")]);
  qs_strb_strcat(self->rets, buff, strlen(buff));
}

void __qs_lib_v (qs_t * self)
{
  char * name = qs_vmem_cstr(self, "v-0");
  if (name == NULL)
    return;

  char * val = qs_vmem_cstr(self, name);
  if (val == NULL)
    return;

  qs_strb_strcat(self->rets, val, strlen(val));
}

void __qs_lib_sys (qs_t * self)
{ 
  /* most powerfull endpoint */
  char * cmd = qs_vmem_cstr(self, "sys-0");
  char buff[0xFF];
  FILE * handle = popen(cmd, "r");

  while (fgets(buff, sizeof(buff), handle) != NULL)
    qs_strb_strcat(self->rets, buff, strlen(buff));

  pclose(handle);
}

void __qs_lib_io (qs_t * self)
{ /* puts and sys replacement */
  char * op = qs_vmem_cstr(self, "io-0");
  if (op[0] == 'p')
  { 
    printf("%s", qs_vmem_cstr(self, "io-1"));
    return;
  }

  if (op[0] == 'u')
  {
    char buff[32];
    sprintf(buff, "%lu", time(NULL));
    qs_strb_strcat(self->rets, buff, strlen(buff));
    return;
  }

  const char facs[2] = {op[0], '\0'};
  char * io1 = qs_vmem_cstr(self, "io-1");
  char * io2 = qs_vmem_cstr(self, "io-2");
  char buff[0xFF];
  FILE * handle = op[0] == 't'
    ? popen(io1, "r")
    : fopen(io1, facs);

  if (op[0] == 't' || op[0] == 'r')
    while (fgets(buff, sizeof(buff), handle) != NULL)
      qs_strb_strcat(self->rets, buff, strlen(buff));
  else
    fwrite(io2, 1, strlen(io2), handle);

  if (op[0] == 't')
    pclose(handle);
  else
    fclose(handle);
}

void __qs_lib_loop (qs_t * self)
{ /* loops expr until test returns 0 */
  char * expr = qs_str_alloc(qs_vmem_cstr(self, "loop-1"));
  char * testv = qs_str_alloc(qs_vmem_cstr(self, "loop-0"));
  while (qs_vmem_int(self, testv) != 0)
  {
    char * ret = qs_eval(self, expr);
    qs_strb_strcat(self->rets, ret, strlen(ret));
    free(ret);
  }
  free(expr);
  free(testv);
}

void __qs_lib_e (qs_t * self)
{
  char * expr = qs_str_alloc(qs_vmem_cstr(self, "e-0"));
  char * rets = qs_eval(self, expr);
  qs_strb_strcat(self->rets, rets, strlen(rets));
  free(expr);
  free(rets);
}

void qs_lib (qs_t * self)
{
  qs_vmem_def(self, "def", NULL, __qs_lib_def);
  qs_vmem_def(self, "use", NULL, __qs_lib_use);
  qs_vmem_def(self, "priv", NULL, __qs_lib_priv);
  qs_vmem_def(self, "clc", NULL, __qs_lib_clc);
  qs_vmem_def(self, "strlen", NULL, __qs_lib_strlen);
  qs_vmem_def(self, "loop", NULL, __qs_lib_loop);
  qs_vmem_def(self, "do", NULL, __qs_lib_do);
  qs_vmem_def(self, "if", NULL, __qs_lib_if);
  qs_vmem_def(self, "chr", NULL, __qs_lib_chr);
  qs_vmem_def(self, "asc", NULL, __qs_lib_asc);
  qs_vmem_def(self, "v", NULL, __qs_lib_v);
  qs_vmem_def(self, "e", NULL, __qs_lib_e);
  qs_vmem_def(self, "io", NULL, __qs_lib_io);
  qs_vmem_def(self, "os", qs_str_alloc(
  #ifdef __linux__
      "__linux__"
  #elif _WIN32
      "_WIN32"
  #elif __APPLE__
      "__APPLE__"
  #endif
  ), NULL);

}