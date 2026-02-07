#include <stdio.h>
#include <stdlib.h>

#include "mpc.h"

#ifdef _WIN32

#include <string.h>

#define BUFFER_LENGTH 2048

static char buffer[BUFFER_LENGTH];

// Fake implementation of the readline function.
char* readline(char* prompt) {
  fputs(prompt, stdout);

  fgets(buffer, BUFFER_LENGTH, stdin);

  char* cpy = malloc(strlen(buffer) + 1);

  strcpy(cpy, buffer);

  cpy[strlen(cpy) - 1] = '\0';

  return cpy;
}

// Fake implementation of the add_history function.
void add_history(char* unused) {}

#else

#include <editline/history.h>
#include <editline/readline.h>

#endif

#define LASSERT(args, cond, fmt, ...)         \
  if (!(cond)) {                              \
    lval* err = lval_err(fmt, ##__VA_ARGS__); \
    lval_del(args);                           \
    return err;                               \
  }

#define LASSERT_NUM_ARGS(func, args, count, exp)                 \
  if (count != exp) {                                            \
    lval* err = lval_err(                                        \
        "Function \"%s\" passed incorrect number of arguments. " \
        "Got %i, but expected %i.",                              \
        func, count, exp);                                       \
    lval_del(args);                                              \
    return err;                                                  \
  }

#define LASSERT_NOT_EMPTY_QEXPR(func, args, count)                            \
  if (count == 0) {                                                           \
    lval* err = lval_err("Function \"%s\" passed {}. Expected non-empty %s.", \
                         func, ltype_name(LVAL_QEXPR));                       \
    lval_del(args);                                                           \
    return err;                                                               \
  }

// Lisp value

enum {
  LVAL_ERR,
  LVAL_NUM,
  LVAL_SYM,
  LVAL_FUN,
  LVAL_SEXPR,
  LVAL_QEXPR,
  LVAL_EXIT
};

// Forward declarations

struct lval;
struct lenv;

typedef struct lval lval;
typedef struct lenv lenv;

typedef lval* (*lbuiltin)(lenv*, lval*);

struct lval {
  int type;

  long num;
  char* err;
  char* sym;
  lbuiltin fun;

  int count;
  struct lval** cell;
};

struct lenv {
  int count;
  char** syms;
  lval** vals;
};

static char* ltype_name(int t) {
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

static lval* lval_num(long x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;

  return v;
}

static lval* lval_err(char* fmt, ...) {
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

static lval* lval_sym(char* s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);

  return v;
}

static lval* lval_fun(lbuiltin func) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->fun = func;

  return v;
}

static lval* lval_sexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;

  return v;
}

static lval* lval_qexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;

  return v;
}

static lval* lval_exit(int i) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_EXIT;
  v->num = i;

  return v;
}

static void lval_del(lval* v) {
  switch (v->type) {
    case LVAL_NUM:
    case LVAL_FUN:
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
  }

  free(v);
}

static lenv* lenv_new(void) {
  lenv* e = malloc(sizeof(lenv));
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;

  return e;
}

static void lenv_del(lenv* e) {
  for (int i = 0; i < e->count; i++) {
    free(e->syms[i]);
    lval_del(e->vals[i]);
  }

  free(e->syms);
  free(e->vals);
  free(e);
}

static lval* lval_read_num(mpc_ast_t* t) {
  errno = 0;

  long x = strtol(t->contents, NULL, 10);

  return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

static lval* lval_add(lval* v, lval* x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count - 1] = x;

  return v;
}

static lval* lval_read(mpc_ast_t* t) {
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

static void lval_print(lval* v);

static void lval_expr_print(lval* v, char open, char close) {
  putchar(open);

  for (int i = 0; i < v->count; i++) {
    lval_print(v->cell[i]);

    if (i != (v->count - 1)) {
      putchar(' ');
    }
  }

  putchar(close);
}

static void lval_print(lval* v) {
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
      printf("<function>");
      break;

    case LVAL_SEXPR:
      lval_expr_print(v, '(', ')');
      break;

    case LVAL_QEXPR:
      lval_expr_print(v, '{', '}');
      break;
  }
}

static void lval_println(lval* v) {
  lval_print(v);
  putchar('\n');
}

static lval* lval_pop(lval* v, int i) {
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

static lval* lval_take(lval* v, int i) {
  // Pop item out of the list
  lval* x = lval_pop(v, i);

  // Free pointer(s)
  lval_del(v);

  return x;
}

static lval* lval_join(lval* x, lval* y) {
  // For each cell in "y" add it to "x"
  while (y->count) {
    x = lval_add(x, lval_pop(y, 0));
  }

  // Delete the empty "y" and return "x"
  lval_del(y);

  return x;
}

static lval* lval_copy(lval* v) {
  // Allocate memory for new "lval"
  lval* x = malloc(sizeof(lval));

  // Specify its type
  x->type = v->type;

  switch (v->type) {
    // Copy function directly
    case LVAL_FUN:
      x->fun = v->fun;
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

static lval* lenv_get(lenv* e, lval* k) {
  // Iterate over all items in the environment
  for (int i = 0; i < e->count; i++) {
    // Check if the stored string matches the symbol string
    // If it does, return a copy of the value
    if (strcmp(e->syms[i], k->sym) == 0) {
      return lval_copy(e->vals[i]);
    }
  }

  return lval_err("Unbound symbol \"%s\".", k->sym);
}

/// @brief Assigns a value to a variable, overriding it or creating it,
/// in the given environment.
/// @param e The environment
/// @param k A lval representing the variable name
/// @param v A lval representing the value to be assigned
static void lenv_put(lenv* e, lval* k, lval* v) {
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

static lval* builtin_op(lenv* e, lval* a, char* op) {
  // Ensure all arguments are numbers
  for (int i = 0; i < a->count; i++) {
    LASSERT(a, a->cell[i]->type == LVAL_NUM,
            "Cannot operate on non-number. Got %s.",
            ltype_name(a->cell[i]->type));
  }

  // Pop the first element
  lval* x = lval_pop(a, 0);

  // If no arguments and subtraction then perform unary negation
  if ((strcmp(op, "-") == 0) && a->count == 0) {
    x->num = -x->num;
  }

  // While there are still elements remaining
  while (a->count > 0) {
    // Pop the next element
    lval* y = lval_pop(a, 0);

    if (strcmp(op, "+") == 0) {
      x->num += y->num;
    }

    if (strcmp(op, "-") == 0) {
      x->num -= y->num;
    }

    if (strcmp(op, "*") == 0) {
      x->num *= y->num;
    }

    if (strcmp(op, "/") == 0) {
      if (y->num == 0) {
        lval_del(x);
        lval_del(y);

        x = lval_err("Attempt to divide by zero.");
        break;
      }

      x->num /= y->num;
    }

    lval_del(y);
  }

  lval_del(a);

  return x;
}

static lval* builtin_add(lenv* e, lval* a) { return builtin_op(e, a, "+"); }

static lval* builtin_sub(lenv* e, lval* a) { return builtin_op(e, a, "-"); }

static lval* builtin_mul(lenv* e, lval* a) { return builtin_op(e, a, "*"); }

static lval* builtin_div(lenv* e, lval* a) { return builtin_op(e, a, "/"); }

static lval* builtin_head(lenv* e, lval* a) {
  // Check if argument is a single one
  LASSERT_NUM_ARGS("head", a, a->count, 1);

  // Check if single argument is a Q-Expression
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "Function \"head\" passed incorrect type for argument 0. "
          "Got %s, but expected %s.",
          ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));

  // Check if single argument is not empty
  LASSERT_NOT_EMPTY_QEXPR("head", a, a->cell[0]->count);

  // Otherwise take first (single) argument
  lval* v = lval_take(a, 0);

  // Delete all elements that are not head and return
  while (v->count > 1) {
    lval_del(lval_pop(v, 1));
  }

  return v;
}

static lval* builtin_tail(lenv* e, lval* a) {
  // Check if argument is a single one
  LASSERT_NUM_ARGS("tail", a, a->count, 1);

  // Check if single argument is a Q-Expression
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "Function \"tail\" passed incorrect type for argument 0. "
          "Got %s, but expected %s.",
          ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));

  // Check if single argument is not empty
  LASSERT_NOT_EMPTY_QEXPR("tail", a, a->cell[0]->count);

  // Otherwise take first (single) argument
  lval* v = lval_take(a, 0);

  // Delete first element and return
  lval_del(lval_pop(v, 0));

  return v;
}

static lval* builtin_list(lenv* e, lval* a) {
  a->type = LVAL_QEXPR;

  return a;
}

static lval* lval_eval(lenv* e, lval* v);

static lval* builtin_eval(lenv* e, lval* a) {
  // Check if argument is a single one
  LASSERT_NUM_ARGS("eval", a, a->count, 1);

  // Check if single argument is a Q-Expression
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "Function \"eval\" passed incorrect type for argument 0. "
          "Got %s, but expected %s.",
          ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));

  // Otherwise take first (single) argument
  lval* x = lval_take(a, 0);
  x->type = LVAL_SEXPR;

  return lval_eval(e, x);
}

static lval* builtin_join(lenv* e, lval* a) {
  // Check if all arguments are Q-Expressions
  for (int i = 0; i < a->count; i++) {
    LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
            "Function \"join\" passed incorrect type for argument %i. "
            "Got %s, but expected %s.",
            i, ltype_name(a->cell[i]->type), ltype_name(LVAL_QEXPR));
  }

  // Take first argument
  lval* x = lval_pop(a, 0);

  // For each element in "a" join it to "x"
  while (a->count) {
    x = lval_join(x, lval_pop(a, 0));
  }

  // Delete the empty "a" and return "x"
  lval_del(a);

  return x;
}

static lval* builtin_cons(lenv* e, lval* a) {
  // Check if the number of arguments is two
  LASSERT_NUM_ARGS("cons", a, a->count, 2);

  // Check if the second argument is a valid type
  LASSERT(a,
          a->cell[0]->type == LVAL_SEXPR || a->cell[0]->type == LVAL_QEXPR ||
              a->cell[0]->type == LVAL_NUM,
          "Function \"cons\" passed incorrect type for argument 0. "
          "Got %s, but expected %s, %s or %s.",
          ltype_name(a->cell[0]->type), ltype_name(LVAL_SEXPR),
          ltype_name(LVAL_QEXPR), ltype_name(LVAL_NUM));

  // Check if the second argument is a Q-Expression
  LASSERT(a, a->cell[1]->type == LVAL_QEXPR,
          "Function \"cons\" passed incorrect type for argument 1. "
          "Got %s, but expected %s.",
          ltype_name(a->cell[1]->type), ltype_name(LVAL_QEXPR));

  // Create result Q-Expression
  lval* r = lval_qexpr();

  // Add the value to the result
  r = lval_add(r, a->cell[0]);

  // Join in the result the second argument (Q-Expression)
  r = lval_join(r, a->cell[1]);

  return r;
}

static lval* builtin_len(lenv* e, lval* a) {
  // Check if argument is a single one
  LASSERT_NUM_ARGS("len", a, a->count, 1);

  // Check if argument is a Q-Expression
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "Function \"len\" passed incorrect type for argument 0. "
          "Got %s, but expected %s.",
          ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));

  // Return a number containing the amount of elements
  lval* r = lval_num(a->cell[0]->count);

  return r;
}

static lval* builtin_init(lenv* e, lval* a) {
  // Check if argument is a single one
  LASSERT_NUM_ARGS("init", a, a->count, 1);

  // Check if argument is a Q-Expression
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "Function \"init\" passed incorrect type for argument 0. "
          "Got %s, but expected %s.",
          ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));

  // Check if argument is not an empty Q-Expr
  LASSERT_NOT_EMPTY_QEXPR("init", a, a->cell[0]->count);

  // Take first argument
  lval* x = lval_take(a, 0);

  // Pop the last value and delete it
  lval* y = lval_pop(x, x->count - 1);
  lval_del(y);

  return x;
}

static lval* builtin_def(lenv* e, lval* a) {
  // Check if argument is a Q-Expression
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "Function \"def\" passed incorrect type for argument 0. "
          "Got %s, but expected %s.",
          ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));

  // First argument is the symbol list
  lval* syms = a->cell[0];

  // Ensure all elements of first list are symbols
  for (int i = 0; i < syms->count; i++) {
    LASSERT(a, syms->cell[i]->type == LVAL_SYM,
            "Function \"def\" cannot define non-symbol. Got %s for element %i.",
            ltype_name(syms->cell[i]->type), i);
  }

  // Check correct number of symbols and values
  // (the first argument is the symbol list)
  LASSERT(a, syms->count == a->count - 1,
          "Function \"def\" cannot define incorrect "
          "number of values to symbols. Got %i %s for %i %s.",
          syms->count, syms->count > 1 ? "symbols" : "symbol", a->count - 1,
          a->count - 1 > 1 ? "values" : "value");

  // Assign copies of values to symbols
  for (int i = 0; i < syms->count; i++) {
    lenv_put(e, syms->cell[i], a->cell[i + 1]);
  }

  lval_del(a);

  return lval_sexpr();
}

static lval* builtin_exit(lenv* e, lval* a) {
  // Check if argument is a single one
  LASSERT_NUM_ARGS("exit", a, a->count, 1);

  // Check if argument is a valid type
  LASSERT(a, a->cell[0]->type == LVAL_NUM || a->cell[0]->type == LVAL_SEXPR,
          "Function \"exit\" passed incorrect type for argument 0. "
          "Got %s, but expected %s or %s.",
          ltype_name(a->cell[0]->type), ltype_name(LVAL_NUM),
          ltype_name(LVAL_SEXPR));

  int i = 0;

  // If argument is a number, use it as exit code
  if (a->cell[0]->type == LVAL_NUM) {
    i = a->cell[0]->num;
  }

  // Create exit lval with exit code
  lval* x = lval_exit(i);

  lval_del(a);

  return x;
}

static lval* builtin_env(lenv* e, lval* a) {
  // Check if argument is a single one
  LASSERT_NUM_ARGS("env", a, a->count, 1);

  // Check if argument is a S-Expression
  LASSERT(a, a->cell[0]->type == LVAL_SEXPR,
          "Function \"env\" passed incorrect type for argument 0. "
          "Got %s, but expected %s.",
          ltype_name(a->cell[0]->type), ltype_name(LVAL_SEXPR));

  // Print all variables in the environment
  for (int i = 0; i < e->count; i++) {
    printf("%s\t", e->syms[i]);

    lval_print(e->vals[i]);

    putchar('\n');
  }

  lval_del(a);

  return lval_sexpr();
}

static lval* lval_eval_sexpr(lenv* e, lval* v) {
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
    // Free pointers
    lval_del(f);
    lval_del(v);

    return lval_err("First element is not a function.");
  }

  // If so call function to get result
  lval* result = f->fun(e, v);
  lval_del(f);

  return result;
}

static lval* lval_eval(lenv* e, lval* v) {
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

static void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
  lval* k = lval_sym(name);
  lval* v = lval_fun(func);

  lenv_put(e, k, v);

  lval_del(k);
  lval_del(v);
}

void static lenv_add_builtins(lenv* e) {
  // List functions
  lenv_add_builtin(e, "list", builtin_list);
  lenv_add_builtin(e, "head", builtin_head);
  lenv_add_builtin(e, "tail", builtin_tail);
  lenv_add_builtin(e, "eval", builtin_eval);
  lenv_add_builtin(e, "join", builtin_join);
  lenv_add_builtin(e, "cons", builtin_cons);
  lenv_add_builtin(e, "len", builtin_len);
  lenv_add_builtin(e, "init", builtin_init);
  lenv_add_builtin(e, "def", builtin_def);
  lenv_add_builtin(e, "exit", builtin_exit);
  lenv_add_builtin(e, "env", builtin_env);

  // Mathematical functions
  lenv_add_builtin(e, "+", builtin_add);
  lenv_add_builtin(e, "-", builtin_sub);
  lenv_add_builtin(e, "*", builtin_mul);
  lenv_add_builtin(e, "/", builtin_div);
}

int main(int argc, char** argv) {
  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Symbol = mpc_new("symbol");
  mpc_parser_t* Sexpr = mpc_new("sexpr");
  mpc_parser_t* Qexpr = mpc_new("qexpr");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Noodle = mpc_new("noodle");

  puts("Noodle Version 0.0.0.0.1");
  puts("Press Ctrl+C to Exit\n");

  mpca_lang(
      MPCA_LANG_DEFAULT,
      "											\
			number		: /-?[0-9]+/ ;					\
			symbol		: /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;		\
			sexpr		: '(' <expr>* ')' ; 				\
			qexpr		: '{' <expr>* '}' ;				\
			expr		: <number> | <symbol> | <sexpr> | <qexpr> ;	\
			noodle		: /^/ <expr>* /$/ ;				\
		",
      Number, Symbol, Sexpr, Qexpr, Expr, Noodle);

  lenv* e = lenv_new();
  lenv_add_builtins(e);

  int status = 0;

  int running = 1;

  while (running) {
    char* input = readline("noodle> ");

    add_history(input);

    mpc_result_t r;

    if (mpc_parse("<stdin>", input, Noodle, &r)) {
      lval* x = lval_eval(e, lval_read(r.output));

      if (x->type == LVAL_EXIT) {
        status = x->num;

        running = 0;
      } else {
        lval_println(x);
      }

      lval_del(x);

      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    free(input);
  }

  lenv_del(e);

  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Noodle);

  return status;
}
