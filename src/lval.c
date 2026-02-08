#include "lval.h"

#include <stdarg.h>

#include "common.h"
#include "lenv.h"

char* ltype_name(int t) {
  switch (t) {
    case LVAL_FUN:
      return "Function";
    case LVAL_NUM:
      return "Number";
    case LVAL_ERR:
      return "Error";
    case LVAL_SYM:
      return "Symbol";
    case LVAL_SEXPR:
      return "S-Expression";
    case LVAL_QEXPR:
      return "Q-Expression";
    case LVAL_EXIT:
      return "Exit";
    default:
      return "unknown";
  }
}

lval* lval_num(long x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;

  return v;
}

lval* lval_err(char* fmt, ...) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;

  // Create a va list and initialize it
  va_list va;
  va_start(va, fmt);

  // Allocate 512 bytes of space
  v->err = malloc(512);

  // printf the error string with a maximum of 511 characters
  vsnprintf(v->err, 511, fmt, va);

  // Reallocate to the number of bytes actually used
  v->err = realloc(v->err, strlen(v->err) + 1);

  // Clean up va list
  va_end(va);

  return v;
}

lval* lval_sym(char* s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);

  return v;
}

lval* lval_fun(lbuiltin func) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->builtin = func;

  return v;
}

lval* lval_lambda(lval* formals, lval* body) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;

  // Set builtin to NULL
  v->builtin = NULL;

  // Create new (local) environment
  v->env = lenv_new();

  // Set formals and body
  v->formals = formals;
  v->body = body;

  return v;
}

lval* lval_sexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;

  return v;
}

lval* lval_qexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;

  return v;
}

lval* lval_exit(int i) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_EXIT;
  v->num = i;

  return v;
}

void lval_del(lval* v) {
  switch (v->type) {
    case LVAL_NUM:
      break;

    case LVAL_ERR:
      free(v->err);
      break;

    case LVAL_SYM:
      free(v->sym);
      break;

    case LVAL_QEXPR:
    case LVAL_SEXPR:
      for (int i = 0; i < v->count; i++) {
        lval_del(v->cell[i]);
      }

      free(v->cell);
      break;

    case LVAL_FUN:
      // If its an user defined function
      if (!v->builtin) {
        lenv_del(v->env);
        lval_del(v->formals);
        lval_del(v->body);
      }
      break;
  }

  // Free itself
  free(v);
}

lval* lval_read_num(mpc_ast_t* t) {
  errno = 0;

  long x = strtol(t->contents, NULL, 10);

  return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t) {
  if (strstr(t->tag, "number")) {
    return lval_read_num(t);
  }

  if (strstr(t->tag, "symbol")) {
    return lval_sym(t->contents);
  }

  lval* x = NULL;

  if (strcmp(t->tag, ">") == 0) {
    x = lval_sexpr();
  }

  if (strstr(t->tag, "sexpr")) {
    x = lval_sexpr();
  }

  if (strstr(t->tag, "qexpr")) {
    x = lval_qexpr();
  }

  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) {
      continue;
    }

    if (strcmp(t->children[i]->contents, ")") == 0) {
      continue;
    }

    if (strcmp(t->children[i]->contents, "{") == 0) {
      continue;
    }

    if (strcmp(t->children[i]->contents, "}") == 0) {
      continue;
    }

    if (strcmp(t->children[i]->tag, "regex") == 0) {
      continue;
    }

    x = lval_add(x, lval_read(t->children[i]));
  }

  return x;
}

lval* lval_add(lval* v, lval* x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count - 1] = x;

  return v;
}

lval* lval_pop(lval* v, int i) {
  // Find the item at index "i"
  lval* x = v->cell[i];

  // Shift memory after the item at index "i" over the top
  memmove(&v->cell[i], &v->cell[i + 1], sizeof(lval*) * (v->count - i - 1));

  // Decrease the count of items in the list
  v->count--;

  // Reallocate the memory used
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);

  return x;
}

lval* lval_take(lval* v, int i) {
  // Pop item out of the list
  lval* x = lval_pop(v, i);

  // Free pointer(s)
  lval_del(v);

  return x;
}

lval* lval_join(lval* x, lval* y) {
  // For each cell in "y" add it to "x"
  while (y->count) {
    x = lval_add(x, lval_pop(y, 0));
  }

  // Delete the empty "y" and return "x"
  lval_del(y);

  return x;
}

lval* lval_copy(lval* v) {
  // Allocate memory for new "lval"
  lval* x = malloc(sizeof(lval));

  // Specify its type
  x->type = v->type;

  switch (v->type) {
    // Copy function directly
    case LVAL_FUN:
      if (v->builtin) {
        x->builtin = v->builtin;
      } else {
        x->builtin = NULL;
        x->env = lenv_copy(v->env);
        x->formals = lval_copy(v->formals);
        x->body = lval_copy(v->body);
      }
      break;

    // Copy number directly
    case LVAL_NUM:
      x->num = v->num;
      break;

    // Copy string using malloc and strcpy
    case LVAL_ERR:
      x->err = malloc(strlen(v->err) + 1);
      strcpy(x->err, v->err);
      break;

    // Copy string using malloc and strcpy
    case LVAL_SYM:
      x->sym = malloc(strlen(v->sym) + 1);
      strcpy(x->sym, v->sym);
      break;

    // Allocate enough memory and copy each element (recursively)
    case LVAL_SEXPR:
    case LVAL_QEXPR:
      x->count = v->count;
      x->cell = malloc(sizeof(lval*) * x->count);

      for (int i = 0; i < x->count; i++) {
        x->cell[i] = lval_copy(v->cell[i]);
      }
      break;
  }

  return x;
}

lval* builtin_eval(lenv* e, lval* a);  // TODO: remove once properly defined.

lval* builtin_list(lenv* e, lval* a);  // TODO: remove once properly defined.

lval* lval_call(lenv* e, lval* f, lval* a) {
  // If builtin, then simply apply that
  if (f->builtin) {
    return f->builtin(e, a);
  }

  // Record argument counts
  int given = a->count;
  int total = f->formals->count;

  // While arguments still remain to be processed
  while (a->count) {
    // If we've ran out of formal arguments to bind
    if (f->formals->count == 0) {
      lval_del(a);

      return lval_err(
          "Function passed too many arguments. "
          "Got %i, but expected %i.",
          given, total);
    }

    // Pop the first (i.e next) symbol from the formals
    lval* sym = lval_pop(f->formals, 0);

    // Special case to deal with "&"
    if (strcmp(sym->sym, "&") == 0) {
      // Ensure "&" is followed by another symbol
      if (f->formals->count != 1) {
        lval_del(a);

        return lval_err(
            "Function format invalid. "
            "Symbol \"&\" not followed by single symbol.");
      }

      // Next formal should be bound to remaining arguments
      lval* nsym = lval_pop(f->formals, 0);
      lenv_put(f->env, nsym, builtin_list(e, a));

      lval_del(sym);
      lval_del(nsym);

      break;
    }

    // Pop the first (i.e next) argument from the list
    lval* val = lval_pop(a, 0);

    // Bind a copy into the function's environment
    lenv_put(f->env, sym, val);

    // Delete symbol and value
    lval_del(sym);
    lval_del(val);
  }

  // Argument list is now bound, so can be cleaned up
  lval_del(a);

  // If "&" remains in formal list, bind to empty list
  if (f->formals->count > 0 && strcmp(f->formals->cell[0]->sym, "&") == 0) {
    // Check to ensure that "&" is not passed invalidly
    if (f->formals->count != 2) {
      return lval_err(
          "Function format invalid. "
          "Symbol \"&\" not followed by single symbol.");
    }

    // Pop and delete "&" symbol
    lval_del(lval_pop(f->formals, 0));

    // Pop next symbol and create empty list
    lval* sym = lval_pop(f->formals, 0);
    lval* val = lval_qexpr();

    // Bind to environment
    lenv_put(f->env, sym, val);

    // Delete
    lval_del(sym);
    lval_del(val);
  }

  // If all formals have been bound, evaluate
  if (f->formals->count == 0) {
    // Set environment parent to evaluation (i.e function) environment
    f->env->par = e;

    // Evaluate and return
    return builtin_eval(f->env, lval_add(lval_sexpr(), lval_copy(f->body)));
  } else {
    // Otherwise, return partially evaluated function
    return lval_copy(f);
  }
}

lval* lval_eval_sexpr(lenv* e, lval* v) {
  // Evaluate children
  for (int i = 0; i < v->count; i++) {
    v->cell[i] = lval_eval(e, v->cell[i]);
  }

  // Error checking
  for (int i = 0; i < v->count; i++) {
    if (v->cell[i]->type == LVAL_ERR) {
      return lval_take(v, i);
    }
  }

  // Empty expression
  if (v->count == 0) {
    return v;
  }

  // Single expression
  if (v->count == 1) {
    return lval_take(v, 0);
  }

  lval* f = lval_pop(v, 0);

  // Ensure first element is a function after evaluation
  if (f->type != LVAL_FUN) {
    lval* err = lval_err(
        "S-Expression starts with incorrect type. "
        "Got %s, but expected %s.",
        ltype_name(f->type), ltype_name(LVAL_FUN));

    // Free pointers
    lval_del(f);
    lval_del(v);

    return err;
  }

  // If so call function to get result
  lval* result = lval_call(e, f, v);
  lval_del(f);

  return result;
}

lval* lval_eval(lenv* e, lval* v) {
  // Evaluate symbol (identifiers like variable names and builtin functions)
  if (v->type == LVAL_SYM) {
    lval* x = lenv_get(e, v);
    lval_del(v);

    return x;
  }

  // Evaluate S-expressions
  if (v->type == LVAL_SEXPR) {
    return lval_eval_sexpr(e, v);
  }

  return v;
}

void lval_print(lval* v) {
  switch (v->type) {
    case LVAL_NUM:
      printf("%li", v->num);
      break;

    case LVAL_ERR:
      printf("Error: %s", v->err);
      break;

    case LVAL_SYM:
      printf("%s", v->sym);
      break;

    case LVAL_FUN:
      if (v->builtin) {
        printf("<function>");
      } else {
        printf("(\\");

        lval_print(v->formals);

        putchar(' ');

        lval_print(v->body);

        putchar(')');
      }
      break;

    case LVAL_SEXPR:
      lval_expr_print(v, '(', ')');
      break;

    case LVAL_QEXPR:
      lval_expr_print(v, '{', '}');
      break;
  }
}

void lval_expr_print(lval* v, char open, char close) {
  putchar(open);

  for (int i = 0; i < v->count; i++) {
    lval_print(v->cell[i]);

    if (i != (v->count - 1)) {
      putchar(' ');
    }
  }

  putchar(close);
}

void lval_println(lval* v) {
  lval_print(v);
  putchar('\n');
}
