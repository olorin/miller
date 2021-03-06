// ================================================================
// Data structure for mlr top: just a decorated array.
// ================================================================

#ifndef TOP_KEEPER_H
#define TOP_KEEPER_H
#include "containers/lrec.h"

typedef struct _top_keeper_t {
	double*  top_values;
	lrec_t** top_precords;
	int     size;
	int     capacity;
} top_keeper_t;

top_keeper_t* top_keeper_alloc(int capacity);
void top_keeper_free(top_keeper_t* ptop_keeper);
void top_keeper_add(top_keeper_t* ptop_keeper, double value, lrec_t* prec);

// For debug/test
void top_keeper_print(top_keeper_t* ptop_keeper);

#endif // TOP_KEEPER_H
