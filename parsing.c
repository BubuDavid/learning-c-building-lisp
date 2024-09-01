#include <editline/history.h>
#include <editline/readline.h>

#include "./mpc.h"

enum { LVAL_NUM, LVAL_ERR, LVAL_EXIT, LVAL_SYM, LVAL_SEXPR };

typedef struct lval {
  int type;
  long num;
  /* Error and Symbol types have some string data */
  char *err;
  char *sym;
  /* Count and pointer to a list of lval* */
  int count;
  struct lval **cell;
} lval;

lval *lval_num(long x) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

lval *lval_err(char *m) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  v->err = malloc(strlen(m) + 1);
  strcpy(v->err, m);
  return v;
}

lval *lval_sym(char *s) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);
  return v;
}

lval *lval_sexpr(void) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

lval *lval_exit() {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_EXIT;
  return v;
}

void lval_del(lval *v) {
  switch (v->type) {
  case LVAL_NUM:
    break;
  case LVAL_ERR:
    free(v->err);
    break;
  case LVAL_SYM:
    free(v->sym);
    break;

  case LVAL_SEXPR:
    for (int i = 0; i < v->count; i++) {
      lval_del(v->cell[i]);
    }
    free(v->cell);
  }

  free(v);
}

void lval_print(lval v) {
  switch (v.type) {
  case LVAL_NUM:
    printf("%li", v.num);
    break;
  case LVAL_ERR:
    if (v.err == LERR_DIV_ZERO) {
      printf("Error: Divison by Zero!");
    }
    if (v.err == LERR_BAD_OP) {
      printf("Error: Invalid operation!");
    }
    if (v.err == LERR_BAD_NUM) {
      printf("Error: Invalid number!");
    }
    break;
  }
}

void lval_println(lval v) {
  lval_print(v);
  putchar('\n');
}

lval eval_op(lval x, char *op, lval y) {
  if (x.type == LVAL_ERR) {
    return x;
  }
  if (y.type == LVAL_ERR) {
    return y;
  }

  if (strcmp(op, "+") == 0) {
    return lval_num(x.num + y.num);
  }
  if (strcmp(op, "-") == 0) {
    return lval_num(x.num - y.num);
  }
  if (strcmp(op, "*") == 0) {
    return lval_num(x.num * y.num);
  }
  if (strcmp(op, "/") == 0) {
    return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num / y.num);
  }

  if (strcmp(op, "%") == 0) {
    return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num % y.num);
  }

  return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t *t) {
  if (strstr(t->tag, "exit")) {
    return lval_exit();
  }
  if (strstr(t->tag, "number")) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
  }

  char *op = t->children[1]->contents;

  lval x = eval(t->children[2]);

  int i = 3;
  while (strstr(t->children[i]->tag, "expr")) {
    x = eval_op(x, op, eval(t->children[i]));
    i++;
  }

  return x;
}

int main(int argc, char **argv) {

  /* Create Some Parsers */
  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Symbol = mpc_new("operator");
  mpc_parser_t *ExitFunction = mpc_new("exit");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Sexpr = mpc_new("sexpr");
  mpc_parser_t *Lispy = mpc_new("lispy");

  /* Define them with the following Language */
  mpca_lang(MPCA_LANG_DEFAULT, " \
		exit   : \"exit\" ;                              \
    number : /-?[0-9]+/ ;                            \
    symbol : '+' | '-' | '*' | '/' ;                 \
    sexpr  : '(' <expr>* ')' ;                       \
		expr   : <number> | <symbol> | <sexpr> ; 				 \
    lispy  : /^/ <expr>* /$/ | <exit> ;              \
",
            ExitFunction, Number, Symbol, Sexpr, Expr, Lispy);

  puts("Lispy Version 0.0.0.0.2");
  puts("Press Ctrl+c to Exit\n");

  while (1) {

    char *input = readline("lispy> ");
    add_history(input);

    /* Attempt to parse the user input */
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      /* On success print and delete the AST */
      lval result = eval(r.output);
      if (result.type == LVAL_EXIT) {
        printf("Exiting...\n");
        break;
      }
      lval_println(result);
      mpc_ast_delete(r.output);
    } else {
      /* Otherwise print and delete the Error */
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    free(input);
  }

  /* Undefine and delete our parsers */
  mpc_cleanup(6, ExitFunction, Number, Symbol, Sexpr, Expr, Lispy);

  return 0;
}
