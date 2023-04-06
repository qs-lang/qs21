/**
 *  (c) 2023 Marcin Ślusarczyk (quakcin)
 *      https://github.com/qs-lang/qs21
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#ifndef _WIN32
  #include <unistd.h>
#endif

#define MAX_ARGS_SIZE 32
#define MAX_ARGN_SIZE 128

typedef struct qs_s qs_t;
typedef struct strb_s strb_t;

typedef struct vmem_s
{
  void (*cfun)(qs_t *);
  struct vmem_s * next;
  char * name;
  void * value;
} vmem_t;

// typedef struct sched_s
// {
//   struct sched_s * next;
//   char * expr;
//   long delay;
// } sched_t;

typedef struct qs_s
{
  vmem_t * vmem;
  strb_t * rets;
  char * cfun;
  bool alive;
  bool clr;
  // int brk;

} qs_t;

#define STRB_SIZE 256

typedef struct strn_s
{
  char chars[STRB_SIZE];
  struct strn_s * next;
} strn_t;

typedef struct strb_s
{
  unsigned int idx;
  unsigned int length;
  strn_t * head;
  strn_t * tail;
} strb_t;

qs_t * qs_ctor ();
void qs_dtor (qs_t * self);
// void qs_upt (qs_t * self);
void qs_lib (qs_t * self);
char * qs_eval (qs_t * self, char * expr);

vmem_t * qs_vmem_def (qs_t * self, char * name, void * value, void (*cfun)(qs_t *));
char * qs_vmem_cstr (qs_t * self, char * name);
char * qs_vmem_arr_cstr (qs_t * self, char * name, int index);
void qs_strb_strcat (strb_t * self, char * str, size_t strl);
char * qs_strb_cstr (strb_t * self);
int qs_vmem_int (qs_t * self, char * name);
// float qs_vmem_float (qs_t * self, char * name);
vmem_t * qs_vmem_loc (qs_t * self, char * name);
void qs_vmem_DUMP (qs_t * self);

char * qs_str_alloc (char * stack);
strb_t * qs_strb_ctor (void);
void qs_strb_catc (strb_t * self, char c);
void qs_str_trim (char * str, size_t * beg, size_t * end);
