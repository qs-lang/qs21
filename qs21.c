/**
 *  (c) 2023 Marcin Ślusarczyk (quakcin)
 *      https://github.com/qs-lang/qs21
 */

#include "qs21.h"

qs_t * qs_ctor ()
{
  qs_t * self = (qs_t *) malloc(sizeof(struct qs_s));

  self->alive = true;
  self->vmem = NULL;
  self->rets = NULL;
  self->cfun = NULL;

  return self;
}

void qs_dtor (qs_t * self)
{
  // destroy vmem
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
  for (ptr = self->vmem; ptr != NULL; ptr = ptr->next)
    if (strcmp(ptr->name, name) == 0)
      return ptr;
  return NULL;
}

vmem_t * qs_vmem_def (qs_t * self, /*STACK:*/char * name, /*HEAP:*/void * value, void (*cfun)(qs_t *))
{
  vmem_t * v = qs_vmem_loc(self, name);

  if (v == NULL)
  {
    v = (vmem_t *) malloc(sizeof(struct vmem_s));
    v->next = self->vmem;
    self->vmem = v;
    v->name = (char*) malloc(sizeof(char) * (strlen(name) + 1));
    strcpy(v->name, name);
  }
  else
  {
    if (v->value != NULL)
      free(v->value);
  }

  v->value = value;
  v->cfun = cfun;
  return v;
}

strb_t * qs_strb_ctor (void)
{
  strb_t * self = (strb_t *) malloc(sizeof(struct strb_s));
  self->head = (strn_t *) malloc(sizeof(struct strn_s));
  self->head->next = NULL;
  self->tail = self->head;
  self->length = 0;
  self->idx = 0;
  return self;
}

void qs_strb_catc (strb_t * self, char c)
{
  if (self->idx == STRB_SIZE)
  {
    self->tail->next = (strn_t *) malloc(sizeof(struct strn_s));
    self->tail = self->tail->next;
    self->tail->next = NULL;
    self->idx = 0;
  }
  self->tail->chars[self->idx] = c;
  self->length++;
  self->idx++;
}

void qs_strb_strcat (strb_t * self, char * str, size_t strl)
{
  unsigned int cidx = 0;

  while (cidx < strl)
  {
    if (self->idx == STRB_SIZE)
    {
      self->tail->next = (strn_t *) malloc(sizeof(struct strn_s));
      self->tail = self->tail->next;
      self->tail->next = NULL;
      self->idx = 0;
    }
    unsigned int size = STRB_SIZE - self->idx;
    if (size + cidx > strl)
      size = strl - cidx;
    memcpy(self->tail->chars + self->idx, str + cidx, size);
    cidx += size;
    self->idx += size;
  }
  self->length += strl;
}

char * qs_strb_cstr (strb_t * self)
{
  unsigned int idx = 0;
  char * ret = (char *) malloc(sizeof(char) * (self->length + 1));
  strn_t * ptr, * nxt;
  for (ptr = self->head; ptr != NULL; ptr = nxt)
  {
    nxt = ptr->next;
    unsigned int copy = (ptr == self->tail) ? self->idx : STRB_SIZE;
    memcpy((void *) (ret + idx), (void *) ptr->chars, copy);
    idx += copy;
    free(ptr);
  }
  free(self);
  ret[idx] = '\0';
  return ret;
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
  bool eval_next = true;

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
      { /* WARN: Might break, watch out */
        // qs_strb_catc(rets, cc); <-- old way
        if ((c + 1) >= strl || expr[c + 1] == '{')
        { /* new way is wild */
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
          /* def arguments */ /* TODO: FIXME: Move only to FCALL */
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
              char * cret = qs_eval(self, vmem->value);
              qs_strb_strcat(rets, cret, strlen(cret));
              free(cret);
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

char * qs_vmem_cstr (qs_t * self, char * name)
{
  vmem_t * vmem = qs_vmem_loc(self, name);
  return vmem != NULL ? vmem->value : NULL;
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

void __qs_lib_puts (qs_t * self)
{
  printf("%s", qs_vmem_cstr(self, "puts-0"));
}

void __qs_lib_def (qs_t * self)
{
  const int len = qs_vmem_int(self, "def-len");
  char * args[len];
  qs_vmem_arr(self, args, "def", len);

  if (len == 2)
  { /* do not wrap in local */
    qs_vmem_def(self, args[0], qs_str_alloc(args[1]), NULL);
    return;
  }

  strb_t * strb = qs_strb_ctor();
  qs_strb_strcat(strb, "{priv>", 6);
  int i; /* {def: NAME: ARG1: ARG2: ...> CEXPR}*/
  for (i = 1; i < len - 1; i++)
  { /* wrap around {priv} */
    // char buff[0xFF];
    // sprintf(buff, "%s>{v>%s-%d}>", args[i], args[0], i - 1);
    qs_strb_strcat(strb, args[i], strlen(args[i]));
    qs_strb_catc(strb, '>');
    // qs_strb_strcat(strb, buff, strlen(buff));
  }
  for (i = 1; i < len - 1; i++)
  { /* append defines */
    char buff[0xFF];
    sprintf(buff, "{def>%s>{v>%s-%d}}", args[i], args[0], i - 1);
    qs_strb_strcat(strb, buff, strlen(buff));
  }
  qs_strb_strcat(strb, args[len - 1], strlen(args[len - 1]));
  qs_strb_catc(strb, '}');

  qs_vmem_def(self, args[0], qs_strb_cstr(strb), NULL);
}

void __qs_lib_use (qs_t * self)
{
  char * path = qs_vmem_cstr(self, "use-0");
  strb_t * strb = qs_strb_ctor();
  int fd = open(path, O_RDONLY);
  char buff[0xFF]; size_t r;
  while ((r = read(fd, buff, sizeof(buff))) != 0)
    qs_strb_strcat(strb, buff, r);
  close(fd);
  char * expr = qs_strb_cstr(strb);

  int off = 0; /* remove \n and \t */
  int i; 
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
{ /* temporarly remove defines */
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
  sprintf(buff, "%ld", strlen(qs_vmem_cstr(self, "strlen-0")));
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
{ /* {asc: Hello: 0}*/
  char * str = qs_vmem_cstr(self, "asc-0");
  if (str == NULL)  
    return;
  char buff[32];
  sprintf(buff, "%d", (int) str[qs_vmem_int(self, "asc-1")]);
  qs_strb_strcat(self->rets, buff, strlen(buff));
}

void __qs_lib_v (qs_t * self)
{
  char * val = qs_vmem_cstr(self, qs_vmem_cstr(self, "v-0"));
  if (val != NULL)
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

/**
 * TODO: Ensure removal is possible,
 *       causes more issues than anything
 *       else, besides str-trim works
 *       very well!
 */
// void __qs_lib_trim (qs_t * self)
// {
//   char * str = qs_vmem_cstr(self, "trim-0");
//   if (str == NULL)
//     return;

//   size_t loff = 0;
//   size_t roff = strlen(str) - 1;

//   qs_str_trim(str, &loff, &roff);
//   if (roff < loff || roff == -1 || loff >= strlen(str))
//     return;


//   if (roff - loff + 1 == 1)
//   { /* weird things happen */
//     qs_strb_catc(self->rets, *(str + loff));
//     return;
//   }

//   char newstr[roff - loff + 1];
//   memcpy(newstr, str + loff, roff + 1); /* weird addressing issues ahead */
//   qs_strb_strcat(self->rets, newstr, roff - loff + 1);
// }

void __qs_lib_loop (qs_t * self)
{ /* loops expr until test returns 0 */
  char * expr = qs_str_alloc(qs_vmem_cstr(self, "loop-1"));
  char * testv = qs_str_alloc(qs_vmem_cstr(self, "loop-0"));
  while (qs_vmem_int(self, testv) != 0)
  {
    free(qs_eval(self, expr));
  }
  free(expr);
  free(testv);
}

// void __qs_lib_clr (qs_t * self)
// { 
//   self->clr = true;
// }

void qs_lib (qs_t * self)
{
  qs_vmem_def(self, "puts", NULL, __qs_lib_puts);
  qs_vmem_def(self, "def", NULL, __qs_lib_def);
  qs_vmem_def(self, "use", NULL, __qs_lib_use);
  // qs_vmem_def(self, "local", NULL, __qs_lib_local);
  qs_vmem_def(self, "priv", NULL, __qs_lib_priv);
  qs_vmem_def(self, "clc", NULL, __qs_lib_clc);
  // qs_vmem_def(self, "cc", NULL, __qs_lib_cc);
  qs_vmem_def(self, "strlen", NULL, __qs_lib_strlen);
  // qs_vmem_def(self, "trim", NULL, __qs_lib_trim);
  qs_vmem_def(self, "loop", NULL, __qs_lib_loop);
  qs_vmem_def(self, "do", NULL, __qs_lib_do);
  qs_vmem_def(self, "if", NULL, __qs_lib_if);
  qs_vmem_def(self, "chr", NULL, __qs_lib_chr);
  qs_vmem_def(self, "asc", NULL, __qs_lib_asc);
  qs_vmem_def(self, "sys", NULL, __qs_lib_sys);
  // qs_vmem_def(self, "clr", NULL, __qs_lib_clr);
  // qs_vmem_def(self, "brk", NULL, __qs_lib_brk);
  qs_vmem_def(self, "v", NULL, __qs_lib_v);

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