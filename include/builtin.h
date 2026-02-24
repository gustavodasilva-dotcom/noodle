#ifndef noodle_builtin_h
#define noodle_builtin_h

#define LASSERT(a, cond, fmt, ...)            \
  if (!(cond)) {                              \
    lval* err = lval_err(fmt, ##__VA_ARGS__); \
    lval_del(a);                              \
    return err;                               \
  }

#define LASSERT_NUM(f, a, c)                                     \
  if (a->count != c) {                                           \
    lval* err = lval_err(                                        \
        "Function \"%s\" passed incorrect number of arguments. " \
        "Got %i, but expected %i.",                              \
        f, a->count, c);                                         \
    lval_del(a);                                                 \
    return err;                                                  \
  }

#define LASSERT_NONEMPTY(f, a, n)                                             \
  if (a->cell[n]->count == 0) {                                               \
    lval* err = lval_err("Function \"%s\" passed {}. Expected non-empty %s.", \
                         f, ltype_name(LVAL_QEXPR));                          \
    lval_del(a);                                                              \
    return err;                                                               \
  }

#define LASSERT_TYPE(f, a, n, t)                                  \
  if (a->cell[n]->type != t) {                                    \
    lval* err = lval_err(                                         \
        "Function \"%s\" passed incorrect type for argument %i. " \
        "Got %s, but expected %s.",                               \
        f, n, ltype_name(a->cell[n]->type), ltype_name(t));       \
    lval_del(a);                                                  \
    return err;                                                   \
  }

// Forward declarations
struct lenv;
struct lval;

typedef struct lval* (*lbuiltin)(struct lenv*, struct lval*);

struct lval* builtin_add(struct lenv* e, struct lval* a);

struct lval* builtin_sub(struct lenv* e, struct lval* a);

struct lval* builtin_mul(struct lenv* e, struct lval* a);

struct lval* builtin_div(struct lenv* e, struct lval* a);

struct lval* builtin_head(struct lenv* e, struct lval* a);

struct lval* builtin_tail(struct lenv* e, struct lval* a);

/// @brief Converts a lval to a Q-Expression.
/// @param e The current environment
/// @param a The lval to be converted
/// @return The converted lval
struct lval* builtin_list(struct lenv* e, struct lval* a);

/// @brief Evaluates a Q-Expression.
/// @param e The current environment
/// @param a The Q-Expression to be evaluated
/// @return The result of evaluating the Q-Expression
struct lval* builtin_eval(struct lenv* e, struct lval* a);

struct lval* builtin_join(struct lenv* e, struct lval* a);

struct lval* builtin_cons(struct lenv* e, struct lval* a);

struct lval* builtin_len(struct lenv* e, struct lval* a);

struct lval* builtin_init(struct lenv* e, struct lval* a);

struct lval* builtin_var(struct lenv* e, struct lval* a, char* func);

struct lval* builtin_def(struct lenv* e, struct lval* a);

struct lval* builtin_put(struct lenv* e, struct lval* a);

struct lval* builtin_lambda(struct lenv* e, struct lval* a);

struct lval* builtin_fun(struct lenv* e, struct lval* a);

struct lval* builtin_exit(struct lenv* e, struct lval* a);

struct lval* builtin_env(struct lenv* e, struct lval* a);

struct lval* builtin_gt(struct lenv* e, struct lval* a);

struct lval* builtin_lt(struct lenv* e, struct lval* a);

struct lval* builtin_ge(struct lenv* e, struct lval* a);

struct lval* builtin_le(struct lenv* e, struct lval* a);

struct lval* builtin_eq(struct lenv* e, struct lval* a);

struct lval* builtin_ne(struct lenv* e, struct lval* a);

struct lval* builtin_if(struct lenv* e, struct lval* a);

struct lval* builtin_or(struct lenv* e, struct lval* a);

struct lval* builtin_and(struct lenv* e, struct lval* a);

struct lval* builtin_not(struct lenv* e, struct lval* a);

#endif
