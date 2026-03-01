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

mpc_parser_t* Number;
mpc_parser_t* Symbol;
mpc_parser_t* String;
mpc_parser_t* Comment;
mpc_parser_t* Sexpr;
mpc_parser_t* Qexpr;
mpc_parser_t* Expr;
mpc_parser_t* Noodle;

int main(int argc, char** argv) {
  Number = mpc_new("number");
  Symbol = mpc_new("symbol");
  String = mpc_new("string");
  Comment = mpc_new("comment");
  Sexpr = mpc_new("sexpr");
  Qexpr = mpc_new("qexpr");
  Expr = mpc_new("expr");
  Noodle = mpc_new("noodle");

  mpca_lang(
      MPCA_LANG_DEFAULT,
      "														\
			number		: /-?[0-9]+/ ;								\
			symbol		: /[a-zA-Z0-9_+\\-*\\/\\\\%=<>!&|]+/ ;					\
			string		: /\"(\\\\.|[^\"])*\"/ ;						\
			comment		: /;[^\\r\\n]*/ ;							\
			sexpr		: '(' <expr>* ')' ; 							\
			qexpr		: '{' <expr>* '}' ;							\
			expr		: <number> | <symbol> | <string> | <comment> | <sexpr> | <qexpr> ;	\
			noodle		: /^/ <expr>* /$/ ;							\
		",
      Number, Symbol, String, Comment, Sexpr, Qexpr, Expr, Noodle);

  lenv* e = lenv_new();
  lenv_add_builtins(e);

  int status = 0;

  // Supplied with list of files
  if (argc >= 2) {
    // Loop over each supplied filename (starting from 1)
    for (int i = 1; i < argc; i++) {
      // Argument list with a single argument, the filename
      lval* args = lval_add(lval_sexpr(), lval_str(argv[i]));

      // Pass to builtin load and get the result
      lval* x = builtin_load(e, args);

      // If the result in an exit, break loop
      if (x->type == LVAL_EXIT) {
        status = x->num;
        lval_del(x);
        break;
      }

      // If the result is an error, print it
      if (x->type == LVAL_ERR) {
        lval_println(x);
      }

      lval_del(x);
    }
  } else {
    puts("Noodle Version 0.0.0.0.1");
    puts("Press Ctrl+C to Exit\n");

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
  }

  lenv_del(e);

  mpc_cleanup(8, Number, Symbol, String, Comment, Sexpr, Qexpr, Expr, Noodle);

  return status;
}
