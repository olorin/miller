AC_PREREQ([2.60])
# Manually increment on updates to https://github.com/johnkerl/miller/releases
AC_INIT([mlr],[2.2.0])
AC_CONFIG_SRCDIR([c/mlrmain.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([autotools])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE

AC_CANONICAL_HOST
AC_PROG_CC
AM_PROG_AR
AM_PROG_LEX
AC_EXEEXT
LT_INIT


# TODO: better source handling for lemon sources?
# perhaps lemon can be improved to survive being called from the build dir
AC_CONFIG_LINKS([c/dsls/filter_dsl_parse.y:c/dsls/filter_dsl_parse.y])
AC_CONFIG_LINKS([c/dsls/lempar.c:c/dsls/lempar.c])
AC_CONFIG_LINKS([c/dsls/put_dsl_parse.y:c/dsls/put_dsl_parse.y])

AC_CONFIG_FILES([Makefile
		c/cli/Makefile
		c/containers/Makefile
		c/dsls/Makefile
		c/experimental/Makefile
		c/input/Makefile
		c/lib/Makefile
		c/mapping/Makefile
		c/output/Makefile
		c/stream/Makefile
		c/test/Makefile
		c/test/expected/Makefile
		c/test/input/Makefile
		c/test/input/rfc-csv/Makefile
		c/tools/Makefile
		c/Makefile
		doc/Makefile])
AC_OUTPUT
