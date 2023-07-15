# The Quacking Script Language
## About
**Quacking Script** is both a minimalistic and powerful scripting language. Its virtual machine, with only a few hundred lines of code, provides all the functionality you will need. It's capable of many things, but most importantly, it does: memory management, process scheduling, and it also has the ability to call your custom C procedures inside your code. Though its embedded standard library isn't particularly huge, it has a large and active [extension repository](https://github.com/qs-lang/ext "Quacking Script Extension Library") that contains a bunch of useful libraries and packages. Among them, you will find *core lib*, which is a suite of the most commonly usunctions and mac, s such as string operatia on functions, mathematical expressions, simple if-like macros, array islation functions and so on. **Quacking Script** has a lot to offer, but most impmanipulation offers **simplicity**. 

#
```
{use> ./lib/core}
{puts: 🦆 quacks!{nl}}
```
> Simple Hello World like example

## Compilation
Simply **git clone** this repository and use **make** and **gcc** in order to compile binaries. 
```
# git clone https://github.com/qs-lang/qs21.git
# cd qs21
# make
```
> You should get ./qs executable

You can try to use different compilers, though I haven't tested them yet. **qs21.c** and **qs21.h** are both the language/vm files, **main.c** is just a simple wrapper that makes **qs** load and execute scripts given in *command line arguments*. 
> Alternatively you can write a custom wrapper around qs

## Integration
In case you want to use **quacking script** interpreter in your own project, here is a simple example of how to do it.
```C
/* include lang sources */
#include "qs21.h" 

int main ()
{
  /* create interpreter/vm instance */
  qs_t * qvm = qs_ctor();
  
  /* link standard library */
  qs_lib(qvm);
  
  /* finally eval an expression */
  char * rets = qs_eval(qvm, "{io> p> Hello, World!}");
  
  /* free up memory */
  free(rets);
  qs_dtor(qvm);
}
```
## License
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
