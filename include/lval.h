#ifndef noodle_lval_h
#define noodle_lval_h

#include "builtin.h"
#include "mpc.h"

// Forward declaration

struct lenv;
struct lval;

typedef struct lval lval;

struct lval {
  int type;

  // Basic
  long num;
  char* err;
  char* sym;

  // Function
  lbuiltin builtin;
  struct lenv* env;
  struct lval* formals;
  struct lval* body;

  // Expression
  int count;
  struct lval** cell;
};

// Lisp value

enum {
  LVAL_ERR,
  LVAL_NUM,
  LVAL_BOOL,
  LVAL_SYM,
  LVAL_FUN,
  LVAL_SEXPR,
  LVAL_QEXPR,
  LVAL_EXIT
};

char* ltype_name(int t);

/// @brief Constructor for number type.
/// @param x The number value
/// @return A pointer to the newly created lval of type number.
lval* lval_num(long x);

/// @brief Constructor for boolean type.
/// @param b The boolean value
/// @return A pointer to the newly created lval of type boolean.
lval* lval_bool(int b);

/// @brief Constructor for error type.
/// @param fmt The format string for the error message
/// @param ... Additional arguments for the format string
/// @return A pointer to the newly created lval of type error.
lval* lval_err(char* fmt, ...);

/// @brief Constructor for symbol type.
/// @param s The symbol value
/// @return A pointer to the newly created lval of type symbol.
lval* lval_sym(char* s);

/// @brief Constructor for builtin function type.
/// @param func The builtin function value
/// @return A pointer to the newly created lval of type builtin function.
lval* lval_fun(lbuiltin func);

/// @brief Constructor for user defined function (lambda) type.
/// @param formals A lval representing the function's formal arguments
/// @param body A lval representing the function's body
/// @return A pointer to the newly created lval of type user defined function
/// (lambda).
lval* lval_lambda(lval* formals, lval* body);

/// @brief Constructor for S-Expression type.
/// @param
/// @return A pointer to the newly created lval of type S-Expression.
lval* lval_sexpr(void);

/// @brief Constructor for Q-Expression type.
/// @param
/// @return A pointer to the newly created lval of type Q-Expression.
lval* lval_qexpr(void);

/// @brief Constructor for exit value.
/// @param i The exit code
/// @return A pointer to the newly created lval of type exit.
lval* lval_exit(int i);

/// @brief Free a lval and all its associated memory.
/// @param v The lval
void lval_del(lval* v);

/// @brief Read a number from an AST node and convert it to a lval.
/// @param t The AST node containing the number
/// @return A pointer to the newly created lval of type number, or an error
/// lval if the conversion fails.
lval* lval_read_num(mpc_ast_t* t);

/// @brief Read an AST node and convert it to a lval.
/// @param t The AST node to read
/// @return A pointer to the newly created lval.
lval* lval_read(mpc_ast_t* t);

/// @brief Operation to add an element to a S-Expression or Q-Expression lval.
/// @param v The lval to add to
/// @param x The element to add
/// @return A pointer to the updated lval with the new element added.
lval* lval_add(lval* v, lval* x);

/// @brief Operation to pop an element from a S-Expression or Q-Expression lval
/// at a specified index.
/// @param v The lval to pop from
/// @param i The index of the element to pop
/// @return A pointer to the popped lval.
lval* lval_pop(lval* v, int i);

/// @brief Operation to take an element from a S-Expression or Q-Expression lval
/// at a specified index.
/// @param v The lval to take from
/// @param i The index of the element to take
/// @return A pointer to the taken lval.
lval* lval_take(lval* v, int i);

/// @brief Operation to join two S-Expressions or Q-Expressions.
/// @param x The first lval
/// @param y The second lval
/// @return A pointer to the joined lval.
lval* lval_join(lval* x, lval* y);

lval* lval_copy(lval* v);

/// @brief Assembles and evaluates a function call.
/// @param e The current environment
/// @param f The function to be called
/// @param a The arguments to be passed to the function
/// @return A lval representing the result of the function call.
lval* lval_call(struct lenv* e, lval* f, lval* a);

lval* lval_eval_sexpr(struct lenv* e, lval* v);

lval* lval_eval(struct lenv* e, lval* v);

/// @brief Print a lval to standard output.
/// @param v The lval to print
void lval_print(lval* v);

/// @brief Print a lval expression to standard output.
/// @param v The lval expression to print
/// @param open The opening character for the expression
/// @param close The closing character for the expression
void lval_expr_print(lval* v, char open, char close);

/// @brief Print a lval followed by a newline to standard output.
/// @param v The lval to print
void lval_println(lval* v);

#endif
