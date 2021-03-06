#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/mlrutil.h"
#include "lib/mtrand.h"
#include "containers/slls.h"
#include "containers/lhmss.h"
#include "containers/lhmsi.h"
#include "input/lrec_readers.h"
#include "mapping/mappers.h"
#include "mapping/lrec_evaluators.h"
#include "output/lrec_writers.h"
#include "cli/mlrcli.h"
#include "cli/argparse.h"

#ifdef NO_AUTOCONFIG
#include "mlrvers.h"
#else
#include "config.h"
#endif // NO_AUTOCONFIG

// ----------------------------------------------------------------
static mapper_setup_t* mapper_lookup_table[] = {
	&mapper_cat_setup,
	&mapper_check_setup,
	&mapper_count_distinct_setup,
	&mapper_cut_setup,
	&mapper_filter_setup,
	&mapper_group_by_setup,
	&mapper_group_like_setup,
	&mapper_having_fields_setup,
	&mapper_head_setup,
	&mapper_histogram_setup,
	&mapper_join_setup,
	&mapper_label_setup,
	&mapper_put_setup,
	&mapper_regularize_setup,
	&mapper_rename_setup,
	&mapper_reorder_setup,
	&mapper_sort_setup,
	&mapper_stats1_setup,
	&mapper_stats2_setup,
	&mapper_step_setup,
	&mapper_tac_setup,
	&mapper_tail_setup,
	&mapper_top_setup,
	&mapper_uniq_setup,
};
static int mapper_lookup_table_length = sizeof(mapper_lookup_table) / sizeof(mapper_lookup_table[0]);

// ----------------------------------------------------------------
static lhmss_t* singleton_pdesc_to_chars_map = NULL;
static lhmss_t* get_desc_to_chars_map() {
	if (singleton_pdesc_to_chars_map == NULL) {
		singleton_pdesc_to_chars_map = lhmss_alloc();
		lhmss_put(singleton_pdesc_to_chars_map, "cr",        "\r");
		lhmss_put(singleton_pdesc_to_chars_map, "crcr",      "\r\r");
		lhmss_put(singleton_pdesc_to_chars_map, "newline",   "\n");
		lhmss_put(singleton_pdesc_to_chars_map, "lf",        "\n");
		lhmss_put(singleton_pdesc_to_chars_map, "lflf",      "\n\n");
		lhmss_put(singleton_pdesc_to_chars_map, "crlf",      "\r\n");
		lhmss_put(singleton_pdesc_to_chars_map, "crlfcrlf",  "\r\n\r\n");
		lhmss_put(singleton_pdesc_to_chars_map, "tab",       "\t");
		lhmss_put(singleton_pdesc_to_chars_map, "space",     " ");
		lhmss_put(singleton_pdesc_to_chars_map, "comma",     ",");
		lhmss_put(singleton_pdesc_to_chars_map, "newline",   "\n");
		lhmss_put(singleton_pdesc_to_chars_map, "pipe",      "|");
		lhmss_put(singleton_pdesc_to_chars_map, "slash",     "/");
		lhmss_put(singleton_pdesc_to_chars_map, "colon",     ":");
		lhmss_put(singleton_pdesc_to_chars_map, "semicolon", ";");
		lhmss_put(singleton_pdesc_to_chars_map, "equals",    "=");
	}
	return singleton_pdesc_to_chars_map;
}
static char* sep_from_arg(char* arg, char* argv0) {
	char* chars = lhmss_get(get_desc_to_chars_map(), arg);
	if (chars != NULL) // E.g. crlf
		return chars;
	else // E.g. '\r\n'
		return mlr_unbackslash(arg);
}

// ----------------------------------------------------------------
static lhmss_t* singleton_default_rses = NULL;
static lhmss_t* singleton_default_fses = NULL;
static lhmss_t* singleton_default_pses = NULL;
static lhmsi_t* singleton_default_repeat_ifses = NULL;
static lhmsi_t* singleton_default_repeat_ipses = NULL;

static lhmss_t* get_default_rses() {
	if (singleton_default_rses == NULL) {
		singleton_default_rses = lhmss_alloc();
		lhmss_put(singleton_default_rses, "dkvp",    "\n");
		lhmss_put(singleton_default_rses, "csv",     "\r\n");
		lhmss_put(singleton_default_rses, "csvlite", "\n");
		lhmss_put(singleton_default_rses, "nidx",    "\n");
		lhmss_put(singleton_default_rses, "xtab",    "(N/A)");
		lhmss_put(singleton_default_rses, "pprint",  "\n");
	}
	return singleton_default_rses;
}

static lhmss_t* get_default_fses() {
	if (singleton_default_fses == NULL) {
		singleton_default_fses = lhmss_alloc();
		lhmss_put(singleton_default_fses, "dkvp",    ",");
		lhmss_put(singleton_default_fses, "csv",     ",");
		lhmss_put(singleton_default_fses, "csvlite", ",");
		lhmss_put(singleton_default_fses, "nidx",    " ");
		lhmss_put(singleton_default_fses, "xtab",    "\n");
		lhmss_put(singleton_default_fses, "pprint",  " ");
	}
	return singleton_default_fses;
}

static lhmss_t* get_default_pses() {
	if (singleton_default_pses == NULL) {
		singleton_default_pses = lhmss_alloc();
		lhmss_put(singleton_default_pses, "dkvp",    "=");
		lhmss_put(singleton_default_pses, "csv",     "(N/A)");
		lhmss_put(singleton_default_pses, "csvlite", "(N/A)");
		lhmss_put(singleton_default_pses, "nidx",    "(N/A)");
		lhmss_put(singleton_default_pses, "xtab",    " ");
		lhmss_put(singleton_default_pses, "pprint",  "(N/A)");
	}
	return singleton_default_pses;
}

static lhmsi_t* get_default_repeat_ifses() {
	if (singleton_default_repeat_ifses == NULL) {
		singleton_default_repeat_ifses = lhmsi_alloc();
		lhmsi_put(singleton_default_repeat_ifses, "dkvp",    FALSE);
		lhmsi_put(singleton_default_repeat_ifses, "csv",     FALSE);
		lhmsi_put(singleton_default_repeat_ifses, "csvlite", FALSE);
		lhmsi_put(singleton_default_repeat_ifses, "nidx",    FALSE);
		lhmsi_put(singleton_default_repeat_ifses, "xtab",    FALSE);
		lhmsi_put(singleton_default_repeat_ifses, "pprint",  TRUE);
	}
	return singleton_default_repeat_ifses;
}

static lhmsi_t* get_default_repeat_ipses() {
	if (singleton_default_repeat_ipses == NULL) {
		singleton_default_repeat_ipses = lhmsi_alloc();
		lhmsi_put(singleton_default_repeat_ipses, "dkvp",    FALSE);
		lhmsi_put(singleton_default_repeat_ipses, "csv",     FALSE);
		lhmsi_put(singleton_default_repeat_ipses, "csvlite", FALSE);
		lhmsi_put(singleton_default_repeat_ipses, "nidx",    FALSE);
		lhmsi_put(singleton_default_repeat_ipses, "xtab",    TRUE);
		lhmsi_put(singleton_default_repeat_ipses, "pprint",  FALSE);
	}
	return singleton_default_repeat_ipses;
}

// For displaying the default separators in on-line help
static char* rebackslash(char* sep) {
	if (streq(sep, "\r"))
		return "\\r";
	else if (streq(sep, "\n"))
		return "\\n";
	else if (streq(sep, "\r\n"))
		return "\\r\\n";
	else if (streq(sep, "\t"))
		return "\\t";
	else if (streq(sep, " "))
		return "space";
	else
		return sep;
}

// ----------------------------------------------------------------
#define DEFAULT_OFMT "%lf"

#define DEFAULT_OQUOTING QUOTE_MINIMAL

// ----------------------------------------------------------------
// xxx cmt stdout/err & 0/1
static void main_usage(char* argv0, int exit_code) {
	FILE* o = exit_code == 0 ? stdout : stderr;
	fprintf(o, "Usage: %s [I/O options] {verb} [verb-dependent options ...] {file names}\n", argv0);
	fprintf(o, "Verbs:\n");
	int linelen = 0;
	for (int i = 0; i < mapper_lookup_table_length; i++) {
		linelen += 1 + strlen(mapper_lookup_table[i]->verb);
		if (linelen > 80) {
			fprintf(o, "\n");
			linelen = 0;
		}
		if ((i > 0) && (linelen > 0))
			fprintf(o, " ");
		else
			fprintf(o, "   ");
		fprintf(o, "%s", mapper_lookup_table[i]->verb);
	}
	fprintf(o, "\n");
	fprintf(o, "Example: %s --csv --rs lf --fs tab cut -f hostname,uptime file1.csv file2.csv\n", argv0);
	fprintf(o, "Please use \"%s {verb name} --help\" for verb-specific help.\n", argv0);
	fprintf(o, "Please use \"%s --help-all-verbs\" for help on all verbs.\n", argv0);

	fprintf(o, "\n");
	lrec_evaluator_list_functions(o);
	fprintf(o, "Please use \"%s --help-function {function name}\" for function-specific help.\n", argv0);
	fprintf(o, "Please use \"%s --help-all-functions\" or \"%s -f\" for help on all functions.\n", argv0, argv0);
	fprintf(o, "\n");

	fprintf(o, "Data-format options, for input, output, or both:\n");
	fprintf(o, "  --dkvp    --idkvp   --odkvp            Delimited key-value pairs, e.g \"a=1,b=2\" (default)\n");
	fprintf(o, "  --nidx    --inidx   --onidx            Implicitly-integer-indexed fields (Unix-toolkit style)\n");
	fprintf(o, "  --csv     --icsv    --ocsv             Comma-separated value (or tab-separated with --fs tab, etc.)\n");
	fprintf(o, "  --pprint  --ipprint --opprint --right  Pretty-printed tabular (produces no output until all input is in)\n");
	fprintf(o, "  --xtab    --ixtab   --oxtab            Pretty-printed vertical-tabular\n");
	fprintf(o, "  -p is a keystroke-saver for --nidx --fs space --repifs\n");
	fprintf(o, "Separator options, for input, output, or both:\n");
	fprintf(o, "  --rs      --irs     --ors              Record separators, e.g. 'lf' or '\\r\\n'\n");
	fprintf(o, "  --fs      --ifs     --ofs    --repifs  Field  separators, e.g. comma\n");
	fprintf(o, "  --ps      --ips     --ops              Pair   separators, e.g. equals sign\n");
	fprintf(o, "  Notes:\n");
	fprintf(o, "  * IPS/OPS are only used for DKVP and XTAB formats, since only in these formats do key-value pairs appear juxtaposed.\n");
	fprintf(o, "  * IRS/ORS are ignored for XTAB format. Nominally IFS and OFS are newlines; XTAB records are separated by\n");
	fprintf(o, "    two or more consecutive IFS/OFS -- i.e. a blank line.\n");
	fprintf(o, "  * OPS must be single-character for XTAB format, and OFS must be single-character for PPRINT format.\n");
	fprintf(o, "    This is because they are used with repetition for alignment; multi-character separators\n");
	fprintf(o, "    would make alignment impossible.\n");
	fprintf(o, "  * DKVP, NIDX, CSVLITE, PPRINT, and XTAB formats are intended to handle platform-native text data.\n");
	fprintf(o, "    In particular, this means LF line-terminators by default on Linux/OSX.\n");
	fprintf(o, "    You can use \"--dkvp --rs crlf\" for CRLF-terminated DKVP files, and so on.\n");
	fprintf(o, "  * CSV is intended to handle RFC-4180-compliant data.\n");
	fprintf(o, "    In particular, this means it uses CRLF line-terminators by default.\n");
	fprintf(o, "    You can use \"--csv --rs lf\" for Linux-native CSV files.\n");
	fprintf(o, "  * You can specify separators in any of the following ways, shown by example:\n");
	fprintf(o, "    - Type them out, quoting as necessary for shell escapes, e.g. \"--fs '|' --ips :\"\n");
	fprintf(o, "    - C-style escape sequences, e.g. \"--rs '\\r\\n' --fs '\\t'\".\n");
	fprintf(o, "    - To avoid backslashing, you can use any of the following names:\n");
	fprintf(o, "     ");
	lhmss_t* pmap = get_desc_to_chars_map();
	for (lhmsse_t* pe = pmap->phead; pe != NULL; pe = pe->pnext) {
		fprintf(o, " %s", pe->key);
	}
	fprintf(o, "\n");
	fprintf(o, "  * Default separators by format:\n");
	fprintf(o, "      %-12s %-8s %-8s %s\n", "File format", "RS", "FS", "PS");
	lhmss_t* default_rses = get_default_rses();
	lhmss_t* default_fses = get_default_fses();
	lhmss_t* default_pses = get_default_pses();
	for (lhmsse_t* pe = default_rses->phead; pe != NULL; pe = pe->pnext) {
		char* filefmt = pe->key;
		char* rs = pe->value;
		char* fs = lhmss_get(default_fses, filefmt);
		char* ps = lhmss_get(default_pses, filefmt);
		fprintf(o, "      %-12s %-8s %-8s %s\n", filefmt, rebackslash(rs), rebackslash(fs), rebackslash(ps));
	}
	fprintf(o, "Double-quoting for CSV output:\n");
	fprintf(o, "  --quote-all        Wrap all fields in double quotes\n");
	fprintf(o, "  --quote-none       Do not wrap any fields in double quotes, even if they have OFS or ORS in them\n");
	fprintf(o, "  --quote-minimal    Wrap fields in double quotes only if they have OFS or ORS in them (default)\n");
	fprintf(o, "  --quote-numeric    Wrap fields in double quotes only if they have numbers in them\n");
	fprintf(o, "Numerical formatting:\n");
	fprintf(o, "  --ofmt {format}    E.g. %%.18lf, %%.0lf. Please use sprintf-style codes for double-precision.\n");
	fprintf(o, "                     Applies to verbs which compute new values, e.g. put, stats1, stats2.\n");
	fprintf(o, "                     See also the fmtnum function within mlr put (mlr --help-all-functions).\n");
	fprintf(o, "Other options:\n");
	fprintf(o, "  --seed {n} with n of the form 12345678 or 0xcafefeed. For put/filter urand().\n");
	fprintf(o, "Output of one verb may be chained as input to another using \"then\", e.g.\n");
	fprintf(o, "  %s stats1 -a min,mean,max -f flag,u,v -g color then sort -f color\n", argv0);
	fprintf(o, "Please see http://johnkerl.org/miller/doc and/or http://github.com/johnkerl/miller for more information.\n");
#ifdef NO_AUTOCONFIG
	fprintf(o, "This is Miller version >= %s.\n", MLR_VERSION);
#else
	fprintf(o, "This is Miller version >= %s.\n", PACKAGE_VERSION);
#endif // NO_AUTOCONFIG

	exit(exit_code);
}

static void usage_all_verbs(char* argv0) {
	char* separator = "================================================================";

	for (int i = 0; i < mapper_lookup_table_length; i++) {
		fprintf(stdout, "%s\n", separator);
		mapper_lookup_table[i]->pusage_func(argv0, mapper_lookup_table[i]->verb);
		fprintf(stdout, "\n");
	}
	fprintf(stdout, "%s\n", separator);
	exit(0);
}

static void nusage(char* argv0, char* arg) {
	fprintf(stdout, "%s: option \"%s\" not recognized.\n", argv0, arg);
	fprintf(stdout, "\n");
	main_usage(argv0, 1);
}

static void check_arg_count(char** argv, int argi, int argc, int n) {
	if ((argc - argi) < n) {
		main_usage(argv[0], 1);
	}
}

static mapper_setup_t* look_up_mapper_setup(char* verb) {
	mapper_setup_t* pmapper_setup = NULL;
	for (int i = 0; i < mapper_lookup_table_length; i++) {
		if (streq(mapper_lookup_table[i]->verb, verb))
			return mapper_lookup_table[i];
	}

	return pmapper_setup;
}

// ----------------------------------------------------------------
cli_opts_t* parse_command_line(int argc, char** argv) {
	cli_opts_t* popts = mlr_malloc_or_die(sizeof(cli_opts_t));
	memset(popts, 0, sizeof(*popts));

	popts->irs               = NULL;
	popts->ifs               = NULL;
	popts->ips               = NULL;
	popts->allow_repeat_ifs  = NEITHER_TRUE_NOR_FALSE;
	popts->allow_repeat_ips  = NEITHER_TRUE_NOR_FALSE;

	popts->ors               = NULL;
	popts->ofs               = NULL;
	popts->ops               = NULL;
	popts->ofmt              = DEFAULT_OFMT;
	popts->oquoting          = DEFAULT_OQUOTING;

	popts->plrec_reader      = NULL;
	popts->plrec_writer      = NULL;
	popts->filenames         = NULL;

	popts->ifile_fmt         = "dkvp";
	popts->ofile_fmt          = "dkvp";

	popts->use_mmap_for_read = TRUE;
	int left_align_pprint    = TRUE;

	int have_rand_seed       = FALSE;
	unsigned rand_seed       = 0;

	int argi = 1;
	for (; argi < argc; argi++) {
		if (argv[argi][0] != '-')
			break;

		else if (streq(argv[argi], "-h"))
			main_usage(argv[0], 0);
		else if (streq(argv[argi], "--help"))
			main_usage(argv[0], 0);
		else if (streq(argv[argi], "--help-all-verbs"))
			usage_all_verbs(argv[0]);
		else if (streq(argv[argi], "--help-all-functions") || streq(argv[argi], "-f")) {
			lrec_evaluator_function_usage(stdout, NULL);
			exit(0);
		}

		else if (streq(argv[argi], "--help-function") || streq(argv[argi], "--hf")) {
			check_arg_count(argv, argi, argc, 2);
			lrec_evaluator_function_usage(stdout, argv[argi+1]);
			exit(0);
		}

		else if (streq(argv[argi], "--rs")) {
			check_arg_count(argv, argi, argc, 2);
			popts->ors = sep_from_arg(argv[argi+1], argv[0]);
			popts->irs = sep_from_arg(argv[argi+1], argv[0]);
			argi++;
		}
		else if (streq(argv[argi], "--irs")) {
			check_arg_count(argv, argi, argc, 2);
			popts->irs = sep_from_arg(argv[argi+1], argv[0]);
			argi++;
		}
		else if (streq(argv[argi], "--ors")) {
			check_arg_count(argv, argi, argc, 2);
			popts->ors = sep_from_arg(argv[argi+1], argv[0]);
			argi++;
		}

		else if (streq(argv[argi], "--fs")) {
			check_arg_count(argv, argi, argc, 2);
			popts->ofs = sep_from_arg(argv[argi+1], argv[0]);
			popts->ifs = sep_from_arg(argv[argi+1], argv[0]);
			argi++;
		}
		else if (streq(argv[argi], "--ifs")) {
			check_arg_count(argv, argi, argc, 2);
			popts->ifs = sep_from_arg(argv[argi+1], argv[0]);
			argi++;
		}
		else if (streq(argv[argi], "--ofs")) {
			check_arg_count(argv, argi, argc, 2);
			popts->ofs = sep_from_arg(argv[argi+1], argv[0]);
			argi++;
		}
		else if (streq(argv[argi], "--repifs")) {
			popts->allow_repeat_ifs = TRUE;
		}

		else if (streq(argv[argi], "-p")) {
			popts->ifile_fmt = "nidx";
			popts->ofile_fmt = "nidx";
			popts->ifs = " ";
			popts->ofs = " ";
			popts->allow_repeat_ifs = TRUE;
		}

		else if (streq(argv[argi], "--ps")) {
			check_arg_count(argv, argi, argc, 2);
			popts->ops = sep_from_arg(argv[argi+1], argv[0]);
			popts->ips = sep_from_arg(argv[argi+1], argv[0]);
			argi++;
		}
		else if (streq(argv[argi], "--ips")) {
			check_arg_count(argv, argi, argc, 2);
			popts->ips = sep_from_arg(argv[argi+1], argv[0]);
			argi++;
		}
		else if (streq(argv[argi], "--ops")) {
			check_arg_count(argv, argi, argc, 2);
			popts->ops = sep_from_arg(argv[argi+1], argv[0]);
			argi++;
		}

		else if (streq(argv[argi], "--csv"))      { popts->ifile_fmt = popts->ofile_fmt = "csv";  }
		else if (streq(argv[argi], "--icsv"))     { popts->ifile_fmt = "csv";  }
		else if (streq(argv[argi], "--ocsv"))     { popts->ofile_fmt = "csv";  }

		else if (streq(argv[argi], "--csvlite"))  { popts->ifile_fmt = popts->ofile_fmt = "csvlite";  }
		else if (streq(argv[argi], "--icsvlite")) { popts->ifile_fmt = "csvlite";  }
		else if (streq(argv[argi], "--ocsvlite")) { popts->ofile_fmt = "csvlite";  }

		else if (streq(argv[argi], "--dkvp"))     { popts->ifile_fmt = popts->ofile_fmt = "dkvp"; }
		else if (streq(argv[argi], "--idkvp"))    { popts->ifile_fmt = "dkvp"; }
		else if (streq(argv[argi], "--odkvp"))    { popts->ofile_fmt = "dkvp"; }

		else if (streq(argv[argi], "--nidx"))     { popts->ifile_fmt = popts->ofile_fmt = "nidx"; }
		else if (streq(argv[argi], "--inidx"))    { popts->ifile_fmt = "nidx"; }
		else if (streq(argv[argi], "--onidx"))    { popts->ofile_fmt = "nidx"; }

		else if (streq(argv[argi], "--xtab"))     { popts->ifile_fmt = popts->ofile_fmt = "xtab"; }
		else if (streq(argv[argi], "--ixtab"))    { popts->ifile_fmt = "xtab"; }
		else if (streq(argv[argi], "--oxtab"))    { popts->ofile_fmt = "xtab"; }

		else if (streq(argv[argi], "--ipprint")) {
			popts->ifile_fmt        = "csvlite";
			popts->ifs              = " ";
			popts->allow_repeat_ifs = TRUE;

		}
		else if (streq(argv[argi], "--opprint")) {
			popts->ofile_fmt = "pprint";
		}
		else if (streq(argv[argi], "--pprint")) {
			popts->ifile_fmt        = "csvlite";
			popts->ifs              = " ";
			popts->allow_repeat_ifs = TRUE;
			popts->ofile_fmt        = "pprint";
		}
		else if (streq(argv[argi], "--right"))   {
			left_align_pprint = FALSE;
		}

		else if (streq(argv[argi], "--ofmt")) {
			check_arg_count(argv, argi, argc, 2);
			popts->ofile_fmt = argv[argi+1];
			argi++;
		}

		else if (streq(argv[argi], "--quote-all"))     { popts->oquoting = QUOTE_ALL;     }
		else if (streq(argv[argi], "--quote-none"))    { popts->oquoting = QUOTE_NONE;    }
		else if (streq(argv[argi], "--quote-minimal")) { popts->oquoting = QUOTE_MINIMAL; }
		else if (streq(argv[argi], "--quote-numeric")) { popts->oquoting = QUOTE_NUMERIC; }

		// xxx put into online help.
		else if (streq(argv[argi], "--mmap")) {
			popts->use_mmap_for_read = TRUE;
		}
		else if (streq(argv[argi], "--no-mmap")) {
			popts->use_mmap_for_read = FALSE;
		}
		else if (streq(argv[argi], "--seed")) {
			check_arg_count(argv, argi, argc, 2);
			if (sscanf(argv[argi+1], "0x%x", &rand_seed) == 1) {
				have_rand_seed = TRUE;
			} else if (sscanf(argv[argi+1], "%u", &rand_seed) == 1) {
				have_rand_seed = TRUE;
			} else {
				main_usage(argv[0], 1);
			}
			argi++;
		}

		else
			nusage(argv[0], argv[argi]);
	}

	lhmss_t* default_rses = get_default_rses();
	lhmss_t* default_fses = get_default_fses();
	lhmss_t* default_pses = get_default_pses();
	lhmsi_t* default_repeat_ifses = get_default_repeat_ifses();
	lhmsi_t* default_repeat_ipses = get_default_repeat_ipses();

	if (popts->irs == NULL)
		popts->irs = lhmss_get(default_rses, popts->ifile_fmt);
	if (popts->ifs == NULL)
		popts->ifs = lhmss_get(default_fses, popts->ifile_fmt);
	if (popts->ips == NULL)
		popts->ips = lhmss_get(default_pses, popts->ifile_fmt);

	if (popts->allow_repeat_ifs == NEITHER_TRUE_NOR_FALSE)
		popts->allow_repeat_ifs = lhmsi_get(default_repeat_ifses, popts->ifile_fmt);
	if (popts->allow_repeat_ips == NEITHER_TRUE_NOR_FALSE)
		popts->allow_repeat_ips = lhmsi_get(default_repeat_ipses, popts->ifile_fmt);

	if (popts->ors == NULL)
		popts->ors = lhmss_get(default_rses, popts->ofile_fmt);
	if (popts->ofs == NULL)
		popts->ofs = lhmss_get(default_fses, popts->ofile_fmt);
	if (popts->ops == NULL)
		popts->ops = lhmss_get(default_pses, popts->ofile_fmt);

	if (popts->irs == NULL) {
		fprintf(stderr, "%s: internal coding error detected in file %s at line %d.\n", argv[0], __FILE__, __LINE__);
		exit(1);
	}
	if (popts->ifs == NULL) {
		fprintf(stderr, "%s: internal coding error detected in file %s at line %d.\n", argv[0], __FILE__, __LINE__);
		exit(1);
	}
	if (popts->ips == NULL) {
		fprintf(stderr, "%s: internal coding error detected in file %s at line %d.\n", argv[0], __FILE__, __LINE__);
		exit(1);
	}

	if (popts->allow_repeat_ifs == NEITHER_TRUE_NOR_FALSE) {
		fprintf(stderr, "%s: internal coding error detected in file %s at line %d.\n", argv[0], __FILE__, __LINE__);
		exit(1);
	}
	if (popts->allow_repeat_ips == NEITHER_TRUE_NOR_FALSE) {
		fprintf(stderr, "%s: internal coding error detected in file %s at line %d.\n", argv[0], __FILE__, __LINE__);
		exit(1);
	}

	if (popts->ors == NULL) {
		fprintf(stderr, "%s: internal coding error detected in file %s at line %d.\n", argv[0], __FILE__, __LINE__);
		exit(1);
	}
	if (popts->ofs == NULL) {
		fprintf(stderr, "%s: internal coding error detected in file %s at line %d.\n", argv[0], __FILE__, __LINE__);
		exit(1);
	}
	if (popts->ops == NULL) {
		fprintf(stderr, "%s: internal coding error detected in file %s at line %d.\n", argv[0], __FILE__, __LINE__);
		exit(1);
	}

	if (streq(popts->ofile_fmt, "xtab") && strlen(popts->ops) != 1) {
		fprintf(stderr, "%s: OPS for XTAB format must be single-character; got \"%s\".\n",
			argv[0], popts->ops);
		return NULL;
	}
	if (streq(popts->ofile_fmt, "pprint") && strlen(popts->ofs) != 1) {
		fprintf(stderr, "%s: OFS for PPRINT format must be single-character; got \"%s\".\n",
			argv[0], popts->ofs);
		return NULL;
	}
	if      (streq(popts->ofile_fmt, "dkvp"))
		popts->plrec_writer = lrec_writer_dkvp_alloc(popts->ors, popts->ofs, popts->ops);
	else if (streq(popts->ofile_fmt, "csv"))
		popts->plrec_writer = lrec_writer_csv_alloc(popts->ors, popts->ofs, popts->oquoting);
	else if (streq(popts->ofile_fmt, "csvlite"))
		popts->plrec_writer = lrec_writer_csvlite_alloc(popts->ors, popts->ofs);
	else if (streq(popts->ofile_fmt, "nidx"))
		popts->plrec_writer = lrec_writer_nidx_alloc(popts->ors, popts->ofs);
	else if (streq(popts->ofile_fmt, "xtab"))
		popts->plrec_writer = lrec_writer_xtab_alloc(popts->ofs, popts->ops);
	else if (streq(popts->ofile_fmt, "pprint"))
		popts->plrec_writer = lrec_writer_pprint_alloc(popts->ors, popts->ofs[0], left_align_pprint);
	else {
		main_usage(argv[0], 1);
	}

	if ((argc - argi) < 1) {
		main_usage(argv[0], 1);
	}

	popts->pmapper_list = sllv_alloc();
	while (TRUE) {
		check_arg_count(argv, argi, argc, 1);
		char* verb = argv[argi];

		mapper_setup_t* pmapper_setup = look_up_mapper_setup(verb);
		if (pmapper_setup == NULL) {
			fprintf(stderr, "%s: verb \"%s\" not found. Please use \"%s --help\" for a list.\n",
				argv[0], verb, argv[0]);
			exit(1);
		}

		if ((argc - argi) >= 2) {
			if (streq(argv[argi+1], "-h") || streq(argv[argi+1], "--help")) {
				pmapper_setup->pusage_func(argv[0], verb);
				exit(0);
			}
		}

		// It's up to the parse func to print its usage on CLI-parse failure.
		mapper_t* pmapper = pmapper_setup->pparse_func(&argi, argc, argv);
		if (pmapper == NULL) {
			exit(1);
		}
		sllv_add(popts->pmapper_list, pmapper);

		// xxx cmt
		if (argi >= argc || !streq(argv[argi], "then"))
			break;
		argi++;
	}

	popts->filenames = &argv[argi];

	// No filenames means read from standard input, and standard input cannot be mmapped.
	if (argi == argc)
		popts->use_mmap_for_read = FALSE;

	popts->plrec_reader = lrec_reader_alloc(popts->ifile_fmt, popts->use_mmap_for_read,
		popts->irs, popts->ifs, popts->allow_repeat_ifs, popts->ips, popts->allow_repeat_ips);
	if (popts->plrec_reader == NULL)
		main_usage(argv[0], 1);

	if (have_rand_seed) {
		mtrand_init(rand_seed);
	} else {
		mtrand_init_default();
	}

	return popts;
}

// ----------------------------------------------------------------
void cli_opts_free(cli_opts_t* popts) {
	if (popts == NULL)
		return;
	if (popts->plrec_reader->pfree_func != NULL)
		popts->plrec_reader->pfree_func(popts->plrec_reader->pvstate);

	for (sllve_t* pe = popts->pmapper_list->phead; pe != NULL; pe = pe->pnext) {
		mapper_t* pmapper = pe->pvdata;
		pmapper->pfree_func(pmapper->pvstate);
	}
	sllv_free(popts->pmapper_list);

	popts->plrec_writer->pfree_func(popts->plrec_writer->pvstate);
	free(popts);
}
