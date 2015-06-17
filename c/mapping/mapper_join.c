#include "lib/mlr_globals.h"
#include "lib/mlrutil.h"
#include "containers/lrec.h"
#include "containers/sllv.h"
#include "containers/lhmslv.h"
#include "containers/mixutil.h"
#include "mapping/mappers.h"
#include "input/lrec_readers.h"
#include "cli/argparse.h"

// xxx comment
#define OPTION_UNSPECIFIED 0xff

typedef struct _mapper_join_opts_t {
	// xxx prefix for left  non-join field names
	// xxx prefix for right non-join field names
	slls_t*  pleft_field_names;
	slls_t*  pright_field_names;
	slls_t*  poutput_field_names;
	int      allow_unsorted_input;
	int      emit_pairables;
	int      emit_left_unpairables;
	int      emit_right_unpairables;

	char*    left_file_name;

	// These allow the joiner to have its own different format/delimiter for
	// the left-file:
	char     irs;
	char     ifs;
	char     ips;
	int      allow_repeat_ifs;
	int      allow_repeat_ips;
	char*    ifmt;
	int      use_mmap_for_read;
} mapper_join_opts_t;

typedef struct _mapper_join_state_t {
	mapper_join_opts_t* popts;

	hss_t*   pleft_field_name_set;
	hss_t*   pright_field_name_set;

	lhmslv_t* pbuckets_by_key_field_names; // For unsorted input

} mapper_join_state_t;

static void ingest_left_file(mapper_join_state_t* pstate);

typedef struct _join_bucket_t {
	sllv_t* precords;
	int was_paired;
} join_bucket_t;

// ----------------------------------------------------------------
static sllv_t* mapper_join_process_unsorted(lrec_t* pright_rec, context_t* pctx, void* pvstate) {
	mapper_join_state_t* pstate = (mapper_join_state_t*)pvstate;

	if (pright_rec == NULL) { // End of input record stream
		if (pstate->popts->emit_left_unpairables) {
			sllv_t* poutrecs = sllv_alloc();
			for (lhmslve_t* pe = pstate->pbuckets_by_key_field_names->phead; pe != NULL; pe = pe->pnext) {
				join_bucket_t* pbucket = pe->pvvalue;
				if (!pbucket->was_paired) {
					for (sllve_t* pf = pbucket->precords->phead; pf != NULL; pf = pf->pnext) {
						lrec_t* pleft_rec = pf->pvdata;
						sllv_add(poutrecs, pleft_rec);
					}
				}
			}
			sllv_add(poutrecs, NULL);
			return poutrecs;
		} else {
			return sllv_single(NULL);
		}
	}

	if (pstate->pbuckets_by_key_field_names == NULL) // First call
		ingest_left_file(pstate);

	slls_t* pright_field_values = mlr_selected_values_from_record(pright_rec, pstate->popts->pright_field_names);
	join_bucket_t* pleft_bucket = lhmslv_get(pstate->pbuckets_by_key_field_names, pright_field_values);
	if (pleft_bucket == NULL) {
		if (pstate->popts->emit_right_unpairables) {
			return sllv_single(pright_rec);
		} else {
			return NULL;
		}
	} else if (pstate->popts->emit_pairables) {
		sllv_t* pout_records = sllv_alloc();
		pleft_bucket->was_paired = TRUE;
		for (sllve_t* pe = pleft_bucket->precords->phead; pe != NULL; pe = pe->pnext) {
			lrec_t* pleft_rec = pe->pvdata;
			lrec_t* pout_rec = lrec_unbacked_alloc();

			// add the joined-on fields
			sllse_t* pg = pstate->popts->pleft_field_names->phead;
			sllse_t* ph = pstate->popts->pright_field_names->phead;
			sllse_t* pi = pstate->popts->poutput_field_names->phead;
			for ( ; pg != NULL && ph != NULL && pi != NULL; pg = pg->pnext, ph = ph->pnext, pi = pi->pnext) {
				char* v = lrec_get(pleft_rec, pg->value);
				lrec_put(pout_rec, pi->value, strdup(v), 0);
			}

			// add the left-record fields not already added
			for (lrece_t* pl = pleft_rec->phead; pl != NULL; pl = pl->pnext) {
				if (!hss_has(pstate->pleft_field_name_set, pl->key))
					lrec_put(pout_rec, strdup(pl->key), strdup(pl->value), LREC_FREE_ENTRY_KEY|LREC_FREE_ENTRY_VALUE);
			}

			// add the right-record fields not already added
			for (lrece_t* pr = pright_rec->phead; pr != NULL; pr = pr->pnext) {
				if (!hss_has(pstate->pright_field_name_set, pr->key))
					lrec_put(pout_rec, strdup(pr->key), strdup(pr->value), LREC_FREE_ENTRY_KEY|LREC_FREE_ENTRY_VALUE);
			}

			sllv_add(pout_records, pout_rec);
		}
		return pout_records;
	} else {
		return NULL;
	}
}

// ----------------------------------------------------------------
static sllv_t* mapper_join_process_sorted(lrec_t* pright_rec, context_t* pctx, void* pvstate) {
	if (pright_rec == NULL) // End of input record stream
		return sllv_single(NULL);
	//mapper_join_state_t* pstate = (mapper_join_state_t*)pvstate;

	return sllv_single(pright_rec);
}

// ----------------------------------------------------------------
static void mapper_join_free(void* pvstate) {
	mapper_join_state_t* pstate = (mapper_join_state_t*)pvstate;
	if (pstate->popts->pleft_field_names != NULL)
		slls_free(pstate->popts->pleft_field_names);
	if (pstate->popts->pright_field_names != NULL)
		slls_free(pstate->popts->pright_field_names);
	if (pstate->popts->poutput_field_names != NULL)
		slls_free(pstate->popts->poutput_field_names);
}

// ----------------------------------------------------------------
// Format and separator flags are passed to mapper_join in MLR_GLOBALS rather
// than on the stack, since the latter would require complicating the interface
// for all the other mappers which don't do their own file I/O.  (Also, while
// some of the information needed to construct an lrec_reader is available on
// the command line before the mapper-allocators are called, some is not
// available until after.  Hence our obtaining these flags after mapper-alloc.)

static void ingest_left_file(mapper_join_state_t* pstate) {

	cli_opts_t* popts = MLR_GLOBALS.popts;
	lrec_reader_t* plrec_reader = lrec_reader_alloc(popts->ifmt, popts->use_mmap_for_read,
		popts->irs, popts->ifs, popts->allow_repeat_ifs, popts->ips, popts->allow_repeat_ips);

	void* pvhandle = plrec_reader->popen_func(pstate->popts->left_file_name);
	plrec_reader->psof_func(plrec_reader->pvstate);

	context_t ctx = { .nr = 0, .fnr = 0, .filenum = 1, .filename = pstate->popts->left_file_name };
	context_t* pctx = &ctx;

	pstate->pbuckets_by_key_field_names = lhmslv_alloc();

	while (TRUE) {
		lrec_t* pleft_rec = plrec_reader->pprocess_func(pvhandle, plrec_reader->pvstate, pctx);
		if (pleft_rec == NULL)
			break;

		slls_t* pleft_field_values = mlr_selected_values_from_record(pleft_rec, pstate->popts->pleft_field_names);
		join_bucket_t* pbucket = lhmslv_get(pstate->pbuckets_by_key_field_names, pleft_field_values);
		if (pbucket == NULL) { // New key-field-value: new bucket and hash-map entry
			slls_t* pkey_field_values_copy = slls_copy(pleft_field_values);
			join_bucket_t* pbucket = mlr_malloc_or_die(sizeof(join_bucket_t));
			pbucket->precords = sllv_alloc();
			pbucket->was_paired = FALSE;
			sllv_add(pbucket->precords, pleft_rec);
			lhmslv_put(pstate->pbuckets_by_key_field_names, pkey_field_values_copy, pbucket);
		} else { // Previously seen key-field-value: append record to bucket
			sllv_add(pbucket->precords, pleft_rec);
		}
	}

	plrec_reader->pclose_func(pvhandle);
}

// ----------------------------------------------------------------
static mapper_t* mapper_join_alloc(mapper_join_opts_t* popts)
{
	mapper_t* pmapper = mlr_malloc_or_die(sizeof(mapper_t));

	mapper_join_state_t* pstate = mlr_malloc_or_die(sizeof(mapper_join_state_t));
	pstate->popts = popts;
	pstate->pleft_field_name_set        = hss_from_slls(popts->pleft_field_names);
	pstate->pright_field_name_set       = hss_from_slls(popts->pright_field_names);
	pstate->pbuckets_by_key_field_names = NULL;

	pmapper->pvstate = (void*)pstate;
	if (popts->allow_unsorted_input) {
		pmapper->pprocess_func = mapper_join_process_unsorted;
	} else {
		pmapper->pprocess_func = mapper_join_process_sorted;
	}
	pmapper->pfree_func = mapper_join_free;

	return pmapper;
}

// ----------------------------------------------------------------
static void mapper_join_usage(char* argv0, char* verb) {
	fprintf(stdout, "Usage: %s %s [options]\n", argv0, verb);
	fprintf(stdout, "xxx write me up.\n");
	fprintf(stdout, "-f {left file name}\n");
	fprintf(stdout, "-l {a,b,c}\n");
	fprintf(stdout, "-r {a,b,c}\n");
	fprintf(stdout, "-o {a,b,c}\n");
	fprintf(stdout, "--np\n");
	fprintf(stdout, "--ul\n");
	fprintf(stdout, "--ur\n");
	fprintf(stdout, "-u\n");
	fprintf(stdout, "-e EMPTY\n");
}

//  --irs x
//  --ifs x
//  --ips x
//  --idkvp
//  --inidx
//  --icsv
//  --ipprint
//  --ixtab
//  --use-mmap
//  --no-mmap

//	char     irs;
//	char     ifs;
//	char     ips;
//	int      allow_repeat_ifs;
//	int      allow_repeat_ips;
//	char*    ifmt;
//	int      use_mmap_for_read;

// ----------------------------------------------------------------
static mapper_t* mapper_join_parse_cli(int* pargi, int argc, char** argv) {
	mapper_join_opts_t* popts = mlr_malloc_or_die(sizeof(mapper_join_opts_t));
	popts->left_file_name          = NULL;
	popts->pleft_field_names       = NULL;
	popts->pright_field_names      = NULL;
	popts->poutput_field_names     = NULL;
	popts->allow_unsorted_input    = FALSE;
	popts->emit_pairables          = TRUE;
	popts->emit_left_unpairables   = FALSE;
	popts->emit_right_unpairables  = FALSE;

	char* verb = argv[(*pargi)++];

	ap_state_t* pstate = ap_alloc();
	ap_define_string_flag(pstate,      "-f",   &popts->left_file_name);
	ap_define_string_list_flag(pstate, "-l",   &popts->pleft_field_names);
	ap_define_string_list_flag(pstate, "-r",   &popts->pright_field_names);
	ap_define_string_list_flag(pstate, "-o",   &popts->poutput_field_names);
	ap_define_false_flag(pstate,       "--np", &popts->emit_pairables);
	ap_define_true_flag(pstate,        "--ul", &popts->emit_left_unpairables);
	ap_define_true_flag(pstate,        "--ur", &popts->emit_right_unpairables);
	ap_define_true_flag(pstate,        "-u",   &popts->allow_unsorted_input);

	if (!ap_parse(pstate, verb, pargi, argc, argv)) {
		mapper_join_usage(argv[0], verb);
		return NULL;
	}

	if (popts->left_file_name == NULL) {
		fprintf(stderr, "%s %s: need left file name\n", MLR_GLOBALS.argv0, verb);
		mapper_join_usage(argv[0], verb);
		return NULL;
	}
	if (popts->pleft_field_names == NULL) {
		fprintf(stderr, "%s %s: need left field names\n", MLR_GLOBALS.argv0, verb);
		mapper_join_usage(argv[0], verb);
		return NULL;
	}

	if (!popts->emit_pairables && !popts->emit_left_unpairables && !popts->emit_right_unpairables) {
		fprintf(stderr, "%s %s: all emit flags are unset; no output is possible.\n",
			MLR_GLOBALS.argv0, verb);
		mapper_join_usage(argv[0], verb);
		return NULL;
	}

	if (popts->pright_field_names == NULL)
		popts->pright_field_names = slls_copy(popts->pleft_field_names);
	if (popts->poutput_field_names == NULL)
		popts->poutput_field_names = slls_copy(popts->pleft_field_names);

	int llen = popts->pleft_field_names->length;
	int rlen = popts->pright_field_names->length;
	int olen = popts->poutput_field_names->length;
	if (llen != rlen || llen != olen) {
		fprintf(stderr,
			"%s %s: must have equal left,right,output field-name lists; got lengths %d,%d,%d.\n",
			MLR_GLOBALS.argv0, verb, llen, rlen, olen);
		exit(1);
	}

	return mapper_join_alloc(popts);
}

// ----------------------------------------------------------------
mapper_setup_t mapper_join_setup = {
	.verb = "join",
	.pusage_func = mapper_join_usage,
	.pparse_func = mapper_join_parse_cli,
};
