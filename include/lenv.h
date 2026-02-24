#ifndef noodle_lenv_h
#define noodle_lenv_h

#include "builtin.h"

// Forward declaration
struct lval;
struct lenv;

typedef struct lenv lenv;

struct lenv {
  struct lenv* par;
  int count;
  char** syms;
  struct lval** vals;
};

lenv* lenv_new(void);

void lenv_del(lenv* e);

struct lval* lenv_get(lenv* e, struct lval* k);

/// @brief Assigns a value to a variable, overriding it or creating it,
/// in the given (local) environment.
/// @param e The environment
/// @param k A lval representing the variable name
/// @param v A lval representing the value to be assigned
void lenv_put(lenv* e, struct lval* k, struct lval* v);

/// @brief Defines a variable in the global environment,
/// traversing from the current environment to the global one.
/// If the variable already exists, it is overridden.
/// @param e The current environment
/// @param k A lval representing the variable name
/// @param v A lval representing the value to be defined
void lenv_def(lenv* e, struct lval* k, struct lval* v);

lenv* lenv_copy(lenv* e);

void lenv_add_builtins(lenv* e);

#endif
