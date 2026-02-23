#include "lenv.h"

#include "common.h"
#include "lval.h"

lenv* lenv_new(void) {
  lenv* e = malloc(sizeof(lenv));
  e->par = NULL;  // The parent environment
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;

  return e;
}

void lenv_del(lenv* e) {
  for (int i = 0; i < e->count; i++) {
    free(e->syms[i]);
    lval_del(e->vals[i]);
  }

  free(e->syms);
  free(e->vals);
  free(e);
}

lval* lenv_get(lenv* e, lval* k) {
  // Iterate over all items in the environment
  for (int i = 0; i < e->count; i++) {
    // Check if the stored string matches the symbol string
    // If it does, return a copy of the value
    if (strcmp(e->syms[i], k->sym) == 0) {
      return lval_copy(e->vals[i]);
    }
  }

  // If no symbol, check in parent; otherwise, error
  if (e->par) {
    return lenv_get(e->par, k);
  } else {
    return lval_err("Unbound symbol \"%s\".", k->sym);
  }
}

void lenv_put(lenv* e, lval* k, lval* v) {
  // Iterate over all items in environment
  // This is to see if the variable already exists
  for (int i = 0; i < e->count; i++) {
    // If variable is found, delete the item at that position
    // And replace with the variable supplied by user
    if (strcmp(e->syms[i], k->sym) == 0) {
      lval_del(e->vals[i]);

      e->vals[i] = lval_copy(v);
      return;
    }
  }

  // If no existing entry is found, allocate space for a new entry
  e->count++;
  e->vals = realloc(e->vals, sizeof(lval*) * e->count);
  e->syms = realloc(e->syms, sizeof(char*) * e->count);

  // Copy the contents of the lval and the symbol string into the new location
  e->vals[e->count - 1] = lval_copy(v);
  e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
  strcpy(e->syms[e->count - 1], k->sym);
}

void lenv_def(lenv* e, lval* k, lval* v) {
  // Iterate till "e" has no parent
  while (e->par) {
    e = e->par;
  }

  // Put value in "e"
  lenv_put(e, k, v);
}

lenv* lenv_copy(lenv* e) {
  lenv* n = malloc(sizeof(lenv));
  n->par = e->par;
  n->count = e->count;

  n->syms = malloc(sizeof(char*) * n->count);
  n->vals = malloc(sizeof(lval*) * n->count);

  for (int i = 0; i < e->count; i++) {
    n->syms[i] = malloc(strlen(e->syms[i]) + 1);
    strcpy(n->syms[i], e->syms[i]);

    n->vals[i] = lval_copy(e->vals[i]);
  }

  return n;
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
  lval* k = lval_sym(name);
  lval* v = lval_fun(func);

  lenv_put(e, k, v);

  lval_del(k);
  lval_del(v);
}

void lenv_add_builtins(lenv* e) {
  // Environment functions
  lenv_add_builtin(e, "env", builtin_env);
  lenv_add_builtin(e, "exit", builtin_exit);

  // List functions
  lenv_add_builtin(e, "list", builtin_list);
  lenv_add_builtin(e, "head", builtin_head);
  lenv_add_builtin(e, "tail", builtin_tail);
  lenv_add_builtin(e, "eval", builtin_eval);
  lenv_add_builtin(e, "join", builtin_join);
  lenv_add_builtin(e, "cons", builtin_cons);
  lenv_add_builtin(e, "len", builtin_len);
  lenv_add_builtin(e, "init", builtin_init);

  // Variable functions
  lenv_add_builtin(e, "\\", builtin_lambda);
  lenv_add_builtin(e, "fun", builtin_fun);

  // Variable definition functions
  lenv_add_builtin(e, "def", builtin_def);
  lenv_add_builtin(e, "=", builtin_put);

  // Mathematical functions
  lenv_add_builtin(e, "+", builtin_add);
  lenv_add_builtin(e, "-", builtin_sub);
  lenv_add_builtin(e, "*", builtin_mul);
  lenv_add_builtin(e, "/", builtin_div);

  // Comparison functions
  lenv_add_builtin(e, "==", builtin_eq);
  lenv_add_builtin(e, "!=", builtin_neq);
  lenv_add_builtin(e, ">", builtin_gt);
  lenv_add_builtin(e, "<", builtin_lt);
  lenv_add_builtin(e, ">=", builtin_geq);
  lenv_add_builtin(e, "<=", builtin_leq);
}
