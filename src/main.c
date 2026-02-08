#include "lenv.h"
#include "lval.h"

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
