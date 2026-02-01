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

#define LASSERT(args, cond, err) \
  if (!(cond)) {                 \
    lval_del(args);              \
    return lval_err(err);        \
  }

#define LASSERT_TWO_ARGS(arg1, arg2, cond, err) \
  if (!(cond)) {                                \
    lval_del(arg1);                             \
    lval_del(arg2);                             \
    return lval_err(err);                       \
  }

#define LASSERT_NUM_ARGS(args, count, exp, err) \
  if (count != exp) {                           \
    lval_del(args);                             \
    return lval_err(err);                       \
  }

#define LASSERT_EMPTY(args, count, err) \
  if (count == 0) {                     \
    lval_del(args);                     \
    return lval_err(err);               \
  }

enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };

typedef struct lval {
  int type;
  long num;
  char* err;
  char* sym;
  int count;
  struct lval** cell;
} lval;

static lval* lval_num(long x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;

  return v;
}

static lval* lval_err(char* m) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  v->err = malloc(strlen(m) + 1);
  strcpy(v->err, m);

  return v;
}

static lval* lval_sym(char* s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);

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

static void lval_del(lval* v) {
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
  }

  free(v);
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

static lval* builtin_op(lval* a, char* op) {
  // Ensure all arguments are numbers
  for (int i = 0; i < a->count; i++) {
    if (a->cell[i]->type != LVAL_NUM) {
      lval_del(a);
      return lval_err("Cannot operate on non-number.");
    }
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

static lval* builtin_head(lval* a) {
  // Check if argument is a single one
  LASSERT_NUM_ARGS(a, a->count, 1,
                   "Function \"head\" passed incorrect number of arguments.");

  // Check if single argument is a Q-Expression
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "Function \"head\" passed incorrect types.");

  // Check if single argument is not empty
  LASSERT_EMPTY(a, a->cell[0]->count, "Function \"head\" passed {}.");

  // Otherwise take first (single) argument
  lval* v = lval_take(a, 0);

  // Delete all elements that are not head and return
  while (v->count > 1) {
    lval_del(lval_pop(v, 1));
  }

  return v;
}

static lval* builtin_tail(lval* a) {
  // Check if argument is a single one
  LASSERT_NUM_ARGS(a, a->count, 1,
                   "Function \"tail\" passed incorrect number of arguments.");

  // Check if single argument is a Q-Expression
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "Function \"tail\" passed incorrect types.");

  // Check if single argument is not empty
  LASSERT_EMPTY(a, a->cell[0]->count, "Function \"tail\" passed {}.");

  // Otherwise take first (single) argument
  lval* v = lval_take(a, 0);

  // Delete first element and return
  lval_del(lval_pop(v, 0));

  return v;
}

static lval* builtin_list(lval* a) {
  a->type = LVAL_QEXPR;

  return a;
}

static lval* lval_eval(lval* v);

static lval* builtin_eval(lval* a) {
  // Check if argument is a single one
  LASSERT_NUM_ARGS(a, a->count, 1,
                   "Function \"eval\" passed incorrect number of arguments.");

  // Check if single argument is a Q-Expression
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "Function \"eval\" passed incorrect type.");

  // Otherwise take first (single) argument
  lval* x = lval_take(a, 0);
  x->type = LVAL_SEXPR;

  return lval_eval(x);
}

static lval* builtin_join(lval* a) {
  // Check if all arguments are Q-Expressions
  for (int i = 0; i < a->count; i++) {
    LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
            "Function \"join\" passed incorrect type.");
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

static lval* builtin_cons(lval* a) {
  // Check if the number of arguments is two
  LASSERT_NUM_ARGS(a, a->count, 2,
                   "Function \"cons\" passed incorrect number of arguments.");

  // Take the first argument
  lval* x = a->cell[0];

  // Take the second argument
  lval* y = a->cell[1];

  // Check if the second argument is a valid type
  LASSERT_TWO_ARGS(
      a, x,
      x->type == LVAL_SEXPR || x->type == LVAL_QEXPR || x->type == LVAL_NUM,
      "Function \"cons\" passed incorrect type for the first argument.");

  // Check if the second argument is a Q-Expression
  LASSERT_TWO_ARGS(
      a, x, y->type == LVAL_QEXPR,
      "Function \"cons\" passed incorrect type for the second argument.");

  lval* r = lval_qexpr();

  // Add the value to the result
  r = lval_add(r, x);

  // Join in the result the second argument (Q-Expression)
  r = lval_join(r, y);

  return r;
}

static lval* builtin_len(lval* a) {
  // Check if argument is a single one
  LASSERT_NUM_ARGS(a, a->count, 1,
                   "Function \"len\" passed incorrect number of arguments.");

  // Check if argument is a Q-Expression
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "Function \"len\" passed incorrect type.");

  lval* r = lval_num(a->cell[0]->count);

  return r;
}

static lval* builtin(lval* a, char* func) {
  if (strcmp("list", func) == 0) {
    return builtin_list(a);
  }

  if (strcmp("head", func) == 0) {
    return builtin_head(a);
  }

  if (strcmp("tail", func) == 0) {
    return builtin_tail(a);
  }

  if (strcmp("join", func) == 0) {
    return builtin_join(a);
  }

  if (strcmp("eval", func) == 0) {
    return builtin_eval(a);
  }

  if (strcmp("cons", func) == 0) {
    return builtin_cons(a);
  }

  if (strcmp("len", func) == 0) {
    return builtin_len(a);
  }

  if (strstr("+-/*", func)) {
    return builtin_op(a, func);
  }

  lval_del(a);

  return lval_err("Unknown function.");
}

static lval* lval_eval_sexpr(lval* v) {
  // Evaluate children
  for (int i = 0; i < v->count; i++) {
    v->cell[i] = lval_eval(v->cell[i]);
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

  // Ensure first element is a symbol
  if (f->type != LVAL_SYM) {
    // Free pointers
    lval_del(f);
    lval_del(v);

    return lval_err("S-expression does not start with symbol.");
  }

  lval* result = builtin(v, f->sym);
  lval_del(f);

  return result;
}

static lval* lval_eval(lval* v) {
  // Evaluate S-expressions
  if (v->type == LVAL_SEXPR) {
    return lval_eval_sexpr(v);
  }

  return v;
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
      "										\
			number		: /-?[0-9]+/ ;					\
			symbol		: \"list\" | \"head\" | \"tail\"		\
					| \"join\" | \"eval\" | \"cons\" | \"len\"	\
					| '+' | '-' | '*' | '/' ;			\
			sexpr		: '(' <expr>* ')' ; 				\
			qexpr		: '{' <expr>* '}' ;				\
			expr		: <number> | <symbol> | <sexpr> | <qexpr> ;	\
			noodle		: /^/ <expr>* /$/ ;				\
		",
      Number, Symbol, Sexpr, Qexpr, Expr, Noodle);

  while (1) {
    char* input = readline("noodle> ");

    add_history(input);

    mpc_result_t r;

    if (mpc_parse("<stdin>", input, Noodle, &r)) {
      lval* x = lval_eval(lval_read(r.output));
      lval_println(x);
      lval_del(x);

      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    free(input);
  }

  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Noodle);

  return 0;
}
