// native_fns.h - Native (C-implemented) functions callable from RHelix
//
// Native functions are installed into the global environment at REPL
// startup by native_fns_install. They live for the entire program
// lifetime (immortal) and are shared - only one copy per native.
//
// Adding a new native function:
//   1. Write the C implementation with signature Value(int argc, Value* argv)
//   2. Register it via env_define in native_fns_install
//   3. Set arity to expected argc, or -1 for variadic

#ifndef RHELIX_NATIVE_FNS_H
#define RHELIX_NATIVE_FNS_H

#include "environment.h"

// Install all built-in native functions into the given (global) env.
// Call once at REPL/program startup after env_create.
void native_fns_install(Environment* global);

#endif // RHELIX_NATIVE_FNS_H
