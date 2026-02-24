#include "builtin.h"

#include "lenv.h"
#include "lval.h"

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

lval* builtin_add(lenv* e, lval* a) { return builtin_op(e, a, "+"); }

lval* builtin_sub(lenv* e, lval* a) { return builtin_op(e, a, "-"); }

lval* builtin_mul(lenv* e, lval* a) { return builtin_op(e, a, "*"); }

lval* builtin_div(lenv* e, lval* a) { return builtin_op(e, a, "/"); }

lval* builtin_head(lenv* e, lval* a) {
  // Check if argument is a single one
  LASSERT_NUM("head", a, 1);

  // Check if single argument is a Q-Expression
  LASSERT_TYPE("head", a, 0, LVAL_QEXPR);

  // Check if single argument is not empty
  LASSERT_NONEMPTY("head", a, 0);

  // Otherwise take first (single) argument
  lval* v = lval_take(a, 0);

  // Delete all elements that are not head and return
  while (v->count > 1) {
    lval_del(lval_pop(v, 1));
  }

  return v;
}

lval* builtin_tail(lenv* e, lval* a) {
  // Check if argument is a single one
  LASSERT_NUM("tail", a, 1);

  // Check if single argument is a Q-Expression
  LASSERT_TYPE("tail", a, 0, LVAL_QEXPR);

  // Check if single argument is not empty
  LASSERT_NONEMPTY("tail", a, 0);

  // Otherwise take first (single) argument
  lval* v = lval_take(a, 0);

  // Delete first element and return
  lval_del(lval_pop(v, 0));

  return v;
}

lval* builtin_list(lenv* e, lval* a) {
  a->type = LVAL_QEXPR;

  return a;
}

lval* builtin_eval(lenv* e, lval* a) {
  // Check if argument is a single one
  LASSERT_NUM("eval", a, 1);

  // Check if single argument is a Q-Expression
  LASSERT_TYPE("eval", a, 0, LVAL_QEXPR);

  // Otherwise take first (single) argument
  lval* x = lval_take(a, 0);
  x->type = LVAL_SEXPR;

  return lval_eval(e, x);
}

lval* builtin_join(lenv* e, lval* a) {
  // Check if all arguments are Q-Expressions
  for (int i = 0; i < a->count; i++) {
    LASSERT_TYPE("join", a, i, LVAL_QEXPR);
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

lval* builtin_cons(lenv* e, lval* a) {
  // Check if the number of arguments is two
  LASSERT_NUM("cons", a, 2);

  // Check if the second argument is a valid type
  LASSERT(a,
          a->cell[0]->type == LVAL_SEXPR || a->cell[0]->type == LVAL_QEXPR ||
              a->cell[0]->type == LVAL_NUM,
          "Function \"cons\" passed incorrect type for argument 0. "
          "Got %s, but expected %s, %s or %s.",
          ltype_name(a->cell[0]->type), ltype_name(LVAL_SEXPR),
          ltype_name(LVAL_QEXPR), ltype_name(LVAL_NUM));

  // Check if the second argument is a Q-Expression
  LASSERT_TYPE("cons", a, 1, LVAL_QEXPR);

  // Create result Q-Expression
  lval* r = lval_qexpr();

  // Add the value to the result
  r = lval_add(r, a->cell[0]);

  // Join in the result the second argument (Q-Expression)
  r = lval_join(r, a->cell[1]);

  return r;
}

lval* builtin_len(lenv* e, lval* a) {
  // Check if argument is a single one
  LASSERT_NUM("len", a, 1);

  // Check if argument is a Q-Expression
  LASSERT_TYPE("len", a, 0, LVAL_QEXPR);

  // Return a number containing the amount of elements
  lval* r = lval_num(a->cell[0]->count);

  return r;
}

lval* builtin_init(lenv* e, lval* a) {
  // Check if argument is a single one
  LASSERT_NUM("init", a, 1);

  // Check if argument is a Q-Expression
  LASSERT_TYPE("init", a, 0, LVAL_QEXPR);

  // Check if argument is not an empty Q-Expr
  LASSERT_NONEMPTY("init", a, 0);

  // Take first argument
  lval* x = lval_take(a, 0);

  // Pop the last value and delete it
  lval* y = lval_pop(x, x->count - 1);
  lval_del(y);

  return x;
}

lval* builtin_var(lenv* e, lval* a, char* func) {
  // Check if argument is a Q-Expression
  LASSERT_TYPE(func, a, 0, LVAL_QEXPR);

  // First argument is the symbol list
  lval* syms = a->cell[0];

  // Ensure all elements of first list are symbols
  for (int i = 0; i < syms->count; i++) {
    LASSERT(a, (syms->cell[i]->type == LVAL_SYM),
            "Function \"%s\" cannot define non-symbol. Got %s for element %i.",
            func, ltype_name(syms->cell[i]->type), i);
  }

  // Check correct number of symbols and values
  // (the first argument is the symbol list)
  LASSERT(a, (syms->count == a->count - 1),
          "Function \"%s\" cannot define incorrect "
          "number of values to symbols. Got %i %s for %i %s.",
          func, syms->count, syms->count > 1 ? "symbols" : "symbol",
          a->count - 1, a->count - 1 > 1 ? "values" : "value");

  // Assign copies of values to symbols
  for (int i = 0; i < syms->count; i++) {
    // If "def", define in globally
    if (strcmp(func, "def") == 0) {
      lenv_def(e, syms->cell[i], a->cell[i + 1]);
    }

    // If "put", define in locally
    if (strcmp(func, "=") == 0) {
      lenv_put(e, syms->cell[i], a->cell[i + 1]);
    }
  }

  lval_del(a);

  return lval_sexpr();
}

lval* builtin_def(lenv* e, lval* a) { return builtin_var(e, a, "def"); }

lval* builtin_put(lenv* e, lval* a) { return builtin_var(e, a, "="); }

lval* builtin_lambda(lenv* e, lval* a) {
  // Check two arguments, each of which are Q-Expressions
  LASSERT_NUM("\\", a, 2);
  LASSERT_TYPE("\\", a, 0, LVAL_QEXPR);
  LASSERT_TYPE("\\", a, 1, LVAL_QEXPR);

  // Check first Q-Expression contains only symbols
  for (int i = 0; i < a->cell[0]->count; i++) {
    LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
            "Cannot define non-symbol. Got %s, but expected %s.",
            ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
  }

  // Pop first argument (formal arguments)
  lval* formals = lval_pop(a, 0);

  // Pop next argument (body)
  lval* body = lval_pop(a, 0);

  lval_del(a);

  // Create lambda
  return lval_lambda(formals, body);
}

lval* builtin_fun(lenv* e, lval* a) {
  // Check two arguments, each of which are Q-Expressions
  LASSERT_NUM("fun", a, 2);
  LASSERT_TYPE("fun", a, 0, LVAL_QEXPR);
  LASSERT_TYPE("fun", a, 1, LVAL_QEXPR);

  // Check first Q-Expression has at least two symbols (identifier and formal
  // argument)
  LASSERT(a, (a->cell[0]->count >= 2),
          "Function \"fun\" passed incorrect number of symbols for argument 0. "
          "Got %i, but expected %i.",
          a->cell[0]->count, 2);

  // Check first Q-Expression contains only symbols
  for (int i = 0; i < a->cell[0]->count; i++) {
    LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
            "Cannot define non-symbol. Got %s, but expected %s.",
            ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
  }

  // Pop first argument
  lval* args = lval_pop(a, 0);

  // Pop next argument
  lval* body = lval_pop(a, 0);

  // Pop first symbol from "args" (the function identifier)
  lval* k = lval_pop(args, 0);

  // Create lambda with remaining "args" symbol(s) and body
  lval* lambda = lval_lambda(args, body);

  // Assign lambda to environment
  lenv_put(e, k, lambda);

  // Delete arguments
  lval_del(args);
  lval_del(body);

  // Delete function identifier
  lval_del(k);

  lval_del(a);

  return lval_sexpr();
}

lval* builtin_exit(lenv* e, lval* a) {
  // Check if argument is a single one
  LASSERT_NUM("exit", a, 1);

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

lval* builtin_env(lenv* e, lval* a) {
  // Check if argument is a single one
  LASSERT_NUM("env", a, 1);

  // Check if argument is a S-Expression
  LASSERT_TYPE("env", a, 0, LVAL_SEXPR);

  // Print all variables in the environment
  for (int i = 0; i < e->count; i++) {
    printf("%s\t", e->syms[i]);

    lval_print(e->vals[i]);

    putchar('\n');
  }

  lval_del(a);

  return lval_sexpr();
}

static lval* builtin_ord(lenv* e, lval* a, char* op) {
  // Check two arguments, each of which are numbers
  LASSERT_NUM(op, a, 2);
  LASSERT_TYPE(op, a, 0, LVAL_NUM);
  LASSERT_TYPE(op, a, 1, LVAL_NUM);

  int r;

  if (strcmp(op, ">") == 0) {
    r = (a->cell[0]->num > a->cell[1]->num);
  }

  if (strcmp(op, "<") == 0) {
    r = (a->cell[0]->num < a->cell[1]->num);
  }

  if (strcmp(op, ">=") == 0) {
    r = (a->cell[0]->num >= a->cell[1]->num);
  }

  if (strcmp(op, "<=") == 0) {
    r = (a->cell[0]->num <= a->cell[1]->num);
  }

  lval_del(a);

  return lval_num(r);
}

lval* builtin_gt(lenv* e, lval* a) { return builtin_ord(e, a, ">"); }

lval* builtin_lt(lenv* e, lval* a) { return builtin_ord(e, a, "<"); }

lval* builtin_ge(lenv* e, lval* a) { return builtin_ord(e, a, ">="); }

lval* builtin_le(lenv* e, lval* a) { return builtin_ord(e, a, "<="); }

static int lval_eq(lval* x, lval* y) {
  // Different types are always unequal
  if (x->type != y->type) {
    return 0;
  }

  // Compare based upon type
  switch (x->type) {
    // Compare number values
    case LVAL_NUM:
      return (x->num == y->num);

    // Compare string values
    case LVAL_ERR:
      return (strcmp(x->err, y->err) == 0);
    case LVAL_SYM:
      return (strcmp(x->sym, y->sym) == 0);

    // If builtin, compare; otherwise, compare formals and body
    case LVAL_FUN:
      if (x->builtin || y->builtin) {
        return x->builtin == y->builtin;
      } else {
        return lval_eq(x->formals, y->formals) && lval_eq(x->body, y->body);
      }

    // If list, compare every individual element
    case LVAL_QEXPR:
    case LVAL_SEXPR:
      if (x->count != y->count) {
        return 0;
      }

      for (int i = 0; i < x->count; i++) {
        // If any element not equal, then whole list not equal
        if (!lval_eq(x->cell[i], y->cell[i])) {
          return 0;
        }
      }

      // Otherwise, list must be equal
      return 1;
      break;
  }

  return 0;
}

static lval* builtin_cmp(lenv* e, lval* a, char* op) {
  // Check two arguments
  LASSERT_NUM(op, a, 2);

  int r;

  if (strcmp(op, "==") == 0) {
    r = lval_eq(a->cell[0], a->cell[1]);
  }

  if (strcmp(op, "!=") == 0) {
    r = !lval_eq(a->cell[0], a->cell[1]);
  }

  lval_del(a);

  return lval_num(r);
}

lval* builtin_eq(lenv* e, lval* a) { return builtin_cmp(e, a, "=="); }

lval* builtin_ne(lenv* e, lval* a) { return builtin_cmp(e, a, "!="); }

lval* builtin_if(lenv* e, lval* a) {
  // Check three arguments
  LASSERT_NUM("if", a, 3);

  // First must be number
  LASSERT_TYPE("if", a, 0, LVAL_NUM);

  // Second and third must be Q-Expressions
  LASSERT_TYPE("if", a, 1, LVAL_QEXPR);
  LASSERT_TYPE("if", a, 2, LVAL_QEXPR);

  // Mark both expressions as evaluable
  lval* x;
  a->cell[1]->type = LVAL_SEXPR;
  a->cell[2]->type = LVAL_SEXPR;

  if (a->cell[0]->num != 0) {
    // If condition is true, evaluate first expression
    x = lval_eval(e, lval_pop(a, 1));
  } else {
    // Otherwise, evaluate second expression
    x = lval_eval(e, lval_pop(a, 2));
  }

  // Delete argument list and return
  lval_del(a);

  return x;
}

lval* builtin_or(lenv* e, lval* a) {
  // Check two arguments, each of which are Q-Expressions
  LASSERT_NUM("||", a, 2);
  LASSERT_TYPE("||", a, 0, LVAL_QEXPR);
  LASSERT_TYPE("||", a, 1, LVAL_QEXPR);

  // Mark both expressions as evaluable
  a->cell[0]->type = LVAL_SEXPR;
  a->cell[1]->type = LVAL_SEXPR;

  // Evaluate left side
  lval* x = lval_eval(e, lval_pop(a, 0));

  if (x->num != 0) {
    // If not false, delete
    lval_del(a);

    // Return true
    return lval_num(1);
  }

  // Otherwise, evaluate right side
  lval* y = lval_eval(e, lval_pop(a, 0));

  if (y->num != 0) {
    // If not false, delete
    lval_del(a);

    // Return true
    return lval_num(1);
  }

  // Delete
  lval_del(a);

  // Return false
  return lval_num(0);
}

lval* builtin_and(lenv* e, lval* a) {
  // Check two arguments, each of which are Q-Expressions
  LASSERT_NUM("||", a, 2);
  LASSERT_TYPE("||", a, 0, LVAL_QEXPR);
  LASSERT_TYPE("||", a, 1, LVAL_QEXPR);

  // Mark both expressions as evaluable
  a->cell[0]->type = LVAL_SEXPR;
  a->cell[1]->type = LVAL_SEXPR;

  // Evaluate left side
  lval* x = lval_eval(e, lval_pop(a, 0));

  // Evaluate right side
  lval* y = lval_eval(e, lval_pop(a, 0));

  // Check if both sides are not false
  int r = x->num != 0 && y->num != 0;

  // Delete
  lval_del(a);

  return lval_num(r);
}

lval* builtin_not(lenv* e, lval* a) {
  // Check if argument is a single one
  LASSERT_NUM("!", a, 1);

  // Check it's a Q-Expression
  LASSERT_TYPE("!", a, 0, LVAL_QEXPR);

  // Mark expression as evaluable
  a->cell[0]->type = LVAL_SEXPR;

  // Evaluate expression
  lval* x = lval_eval(e, lval_pop(a, 0));

  // Delete
  lval_del(a);

  return lval_num(!x->num);
}
