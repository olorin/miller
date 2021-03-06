#include "lib/mlrutil.h"
#include "containers/lrec.h"
#include "containers/sllv.h"
#include "mapping/mappers.h"
#include "mapping/lrec_evaluators.h"
#include "dsls/put_dsl_wrapper.h"
#include "cli/argparse.h"

typedef struct _mapper_put_state_t {
	int num_evaluators;
	char** output_field_names;
	lrec_evaluator_t** pevaluators;
} mapper_put_state_t;

// ----------------------------------------------------------------
static sllv_t* mapper_put_process(lrec_t* pinrec, context_t* pctx, void* pvstate) {
	if (pinrec != NULL) {
		mapper_put_state_t* pstate = (mapper_put_state_t*)pvstate;
		for (int i = 0; i < pstate->num_evaluators; i++) {
			// xxx decide ownership location for type conversions ...
			mv_t val = pstate->pevaluators[i]->pevaluator_func(pinrec,
				pctx, pstate->pevaluators[i]->pvstate);
			char* string = mt_format_val(&val);
			lrec_put(pinrec, pstate->output_field_names[i], string, LREC_FREE_ENTRY_VALUE);
			free(string);
		}
		return sllv_single(pinrec);
	}
	else {
		return sllv_single(NULL);
	}
}

// ----------------------------------------------------------------
static void mapper_put_free(void* pvstate) {
	mapper_put_state_t* pstate = (mapper_put_state_t*)pvstate;
	free(pstate->output_field_names);
	// xxx recursively free them.
	free(pstate->pevaluators);
}

// xxx comment me ...
static mapper_t* mapper_put_alloc(sllv_t* pasts) {
	mapper_put_state_t* pstate = mlr_malloc_or_die(sizeof(mapper_put_state_t));
	pstate->num_evaluators = pasts->length;
	pstate->output_field_names = mlr_malloc_or_die(pasts->length * sizeof(char*));
	pstate->pevaluators = mlr_malloc_or_die(pasts->length * sizeof(lrec_evaluator_t*));

	int i = 0;
	for (sllve_t* pe = pasts->phead; pe != NULL; pe = pe->pnext, i++) {
		mlr_dsl_ast_node_t* past = pe->pvdata;

		if ((past->type != MLR_DSL_AST_NODE_TYPE_OPERATOR) || !streq(past->text, "=")) {
			fprintf(stderr,
				"Expected assignment-rooted AST; got operator \"%s\" with node type %s.\n",
					past->text, mlr_dsl_ast_node_describe_type(past->type));
			return NULL;
		} else if ((past->pchildren == NULL) || (past->pchildren->length != 2)) {
			fprintf(stderr, "xxx write this error message please.\n");
			return NULL;
		}

		mlr_dsl_ast_node_t* pleft  = past->pchildren->phead->pvdata;
		mlr_dsl_ast_node_t* pright = past->pchildren->phead->pnext->pvdata;

		if (pleft->type != MLR_DSL_AST_NODE_TYPE_FIELD_NAME) {
			fprintf(stderr, "xxx write this error message please.\n");
			return NULL;
		} else if (pleft->pchildren != NULL) {
			fprintf(stderr, "xxx write this error message please.\n");
			return NULL;
		}

		char* output_field_name = pleft->text;
		lrec_evaluator_t* pevaluator = lrec_evaluator_alloc_from_ast(pright);

		pstate->pevaluators[i] = pevaluator;
		pstate->output_field_names[i] = output_field_name;
	}

	mapper_t* pmapper = mlr_malloc_or_die(sizeof(mapper_t));

	pmapper->pvstate       = (void*)pstate;
	pmapper->pprocess_func = mapper_put_process;
	pmapper->pfree_func    = mapper_put_free;

	return pmapper;
}

// ----------------------------------------------------------------
static void mapper_put_usage(char* argv0, char* verb) {
	fprintf(stdout, "Usage: %s %s [-v] {expression}\n", argv0, verb);
	fprintf(stdout, "Adds/updates specified field(s).\n");
	fprintf(stdout, "With -v, first prints the AST (abstract syntax tree) for the expression, which\n");
	fprintf(stdout, "gives full transparency on the precedence and associativity rules of Miller's grammar.\n");
	fprintf(stdout, "Please use a dollar sign for field names and double-quotes for string literals.\n");
	fprintf(stdout, "Miller built-in variables are NF NR FNR FILENUM FILENAME PI E.\n");
	fprintf(stdout, "Multiple assignments may be separated with a semicolon.\n");
	fprintf(stdout, "Examples:\n");
	fprintf(stdout, "  %s %s '$y = log10($x); $z = sqrt($y)'\n", argv0, verb);
	fprintf(stdout, "  %s %s '$filename = FILENAME'\n", argv0, verb);
	fprintf(stdout, "  %s %s '$colored_shape = $color . \"_\" . $shape'\n", argv0, verb);
	fprintf(stdout, "  %s %s '$y = cos($theta); $z = atan2($y, $x)'\n", argv0, verb);
	fprintf(stdout, "Please see http://johnkerl.org/miller/doc/reference.html for more information including function list.\n");
}

// ----------------------------------------------------------------
static mapper_t* mapper_put_parse_cli(int* pargi, int argc, char** argv) {
	char* verb = argv[(*pargi)++];
	char* mlr_dsl_expression = NULL;
	int   print_asts = FALSE;

	ap_state_t* pstate = ap_alloc();
	ap_define_true_flag(pstate, "-v", &print_asts);

	if (!ap_parse(pstate, verb, pargi, argc, argv)) {
		mapper_put_usage(argv[0], verb);
		return NULL;
	}

	if ((argc - *pargi) < 1) {
		mapper_put_usage(argv[0], verb);
		return NULL;
	}
	mlr_dsl_expression = argv[(*pargi)++];

	// Linked list of mlr_dsl_ast_node_t*.
	sllv_t* pasts = put_dsl_parse(mlr_dsl_expression);
	if (pasts == NULL) {
		mapper_put_usage(argv[0], verb);
		return NULL;
	}
	if (print_asts) {
		for (sllve_t* pe = pasts->phead; pe != NULL; pe = pe->pnext)
			mlr_dsl_ast_node_print(pe->pvdata);
	}

	return mapper_put_alloc(pasts);
}

// ----------------------------------------------------------------
mapper_setup_t mapper_put_setup = {
	.verb = "put",
	.pusage_func = mapper_put_usage,
	.pparse_func = mapper_put_parse_cli
};
