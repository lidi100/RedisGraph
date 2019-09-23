/*
* Copyright 2018-2019 Redis Labs Ltd. and Contributors
*
* This file is available under the Redis Labs Source Available License Agreement
*/

#include <float.h>
#include "agg_funcs.h"
#include "aggregate.h"
#include "repository.h"
#include "../value.h"
#include "../util/arr.h"
#include "../util/qsort.h"
#include <assert.h>
#include <math.h>
#include "../datatypes/array.h"


#define ISLT(a,b) ((*a) < (*b))

#define SI_LT(a,b) (SIValue_Compare((*a), (*b), NULL) > 0)

typedef struct {
	size_t num;
	double total;
} __agg_sumCtx;

int __agg_sumStep(AggCtx *ctx, SIValue *argv, int argc) {
	// convert the value of the input sequence to a double if possible
	__agg_sumCtx *ac = Agg_FuncCtx(ctx);

	double n;
	for(int i = 0; i < argc; i ++) {
		if(!SIValue_ToDouble(&argv[i], &n)) {
			if(!SIValue_IsNullPtr(&argv[i])) {
				// not convertible to double!
				return Agg_SetError(ctx,
									"SUM Could not convert upstream value to double");
			} else {
				return AGG_OK;
			}
		}

		ac->num++;
		ac->total += n;
	}

	return AGG_OK;
}

int __agg_sumReduceNext(AggCtx *ctx) {
	__agg_sumCtx *ac = Agg_FuncCtx(ctx);
	Agg_SetResult(ctx, SI_DoubleVal(ac->total));
	return AGG_OK;
}

AggCtx *Agg_SumFunc() {
	__agg_sumCtx *ac = malloc(sizeof(__agg_sumCtx));
	ac->num = 0;
	ac->total = 0;

	return Agg_Reduce(ac, __agg_sumStep, __agg_sumReduceNext);
}

//------------------------------------------------------------------------

typedef struct {
	size_t count;
	double total;
} __agg_avgCtx;

int __agg_avgStep(AggCtx *ctx, SIValue *argv, int argc) {
	// convert the value of the input sequence to a double if possible
	__agg_avgCtx *ac = Agg_FuncCtx(ctx);

	double n;
	for(int i = 0; i < argc; i ++) {
		if(!SIValue_ToDouble(&argv[i], &n)) {
			if(!SIValue_IsNullPtr(&argv[i])) {
				// not convertible to double!
				return Agg_SetError(ctx,
									"AVG Could not convert upstream value to double");
			} else {
				return AGG_OK;
			}
		}


		ac->count++;
		ac->total += n;
	}

	return AGG_OK;
}

int __agg_avgReduceNext(AggCtx *ctx) {
	__agg_avgCtx *ac = Agg_FuncCtx(ctx);

	if(ac->count > 0) {
		Agg_SetResult(ctx, SI_DoubleVal(ac->total / ac->count));
	} else {
		Agg_SetResult(ctx, SI_DoubleVal(0));
	}
	return AGG_OK;
}

AggCtx *Agg_AvgFunc() {
	__agg_avgCtx *ac = malloc(sizeof(__agg_avgCtx));
	ac->count = 0;
	ac->total = 0;

	return Agg_Reduce(ac, __agg_avgStep, __agg_avgReduceNext);
}

//------------------------------------------------------------------------

typedef struct {
	SIValue max;
	bool init;
} __agg_maxCtx;

int __agg_maxStep(AggCtx *ctx, SIValue *argv, int argc) {
	assert(argc == 1);
	__agg_maxCtx *ac = Agg_FuncCtx(ctx);

	// Any null values are excluded from the calculation.
	if(SIValue_IsNull(argv[0])) return AGG_OK;

	if(!ac->init) {
		ac->init = true;
		ac->max = argv[0];
		return AGG_OK;
	}

	if(SIValue_Compare(ac->max, argv[0], NULL) < 0) {
		ac->max = argv[0];
	}

	return AGG_OK;
}

int __agg_maxReduceNext(AggCtx *ctx) {
	__agg_maxCtx *ac = Agg_FuncCtx(ctx);
	Agg_SetResult(ctx, ac->max);
	return AGG_OK;
}

AggCtx *Agg_MaxFunc() {
	__agg_maxCtx *ac = malloc(sizeof(__agg_maxCtx));
	// ac->max = SI_DoubleVal(DBL_MIN);
	ac->init = false;

	return Agg_Reduce(ac, __agg_maxStep, __agg_maxReduceNext);
}

//------------------------------------------------------------------------

typedef struct {
	SIValue min;
	bool init;
} __agg_minCtx;

int __agg_minStep(AggCtx *ctx, SIValue *argv, int argc) {
	assert(argc == 1);
	__agg_minCtx *ac = Agg_FuncCtx(ctx);

	// Any null values are excluded from the calculation.
	if(SIValue_IsNull(argv[0])) return AGG_OK;

	if(!ac->init) {
		ac->init = true;
		ac->min = argv[0];
		return AGG_OK;
	}

	if(SIValue_Compare(ac->min, argv[0], NULL) > 0) {
		ac->min = argv[0];
	}

	return AGG_OK;
}

int __agg_minReduceNext(AggCtx *ctx) {
	__agg_minCtx *ac = Agg_FuncCtx(ctx);
	Agg_SetResult(ctx, ac->min);
	return AGG_OK;
}

AggCtx *Agg_MinFunc() {
	__agg_minCtx *ac = malloc(sizeof(__agg_minCtx));
	// ac->min = SI_DoubleVal(DBL_MAX);
	ac->init = false;

	return Agg_Reduce(ac, __agg_minStep, __agg_minReduceNext);
}

//------------------------------------------------------------------------

typedef struct {
	size_t count;
} __agg_countCtx;

int __agg_countStep(AggCtx *ctx, SIValue *argv, int argc) {
	__agg_countCtx *ac = Agg_FuncCtx(ctx);
	// Batch size to this function is always one, so
	// we only need to check the first argument
	if(!SIValue_IsNullPtr(argv)) ac->count ++;

	return AGG_OK;
}

int __agg_countReduceNext(AggCtx *ctx) {
	__agg_countCtx *ac = Agg_FuncCtx(ctx);
	Agg_SetResult(ctx, SI_LongVal(ac->count));
	return AGG_OK;
}

AggCtx *Agg_CountFunc() {
	__agg_countCtx *ac = malloc(sizeof(__agg_countCtx));
	ac->count = 0;

	return Agg_Reduce(ac, __agg_countStep, __agg_countReduceNext);
}

//------------------------------------------------------------------------

typedef struct {
	SIValue *values;
} __agg_countDistinctCtx;

int __agg_countDistinctStep(AggCtx *ctx, SIValue *argv, int argc) {
	assert(argc == 1);
	__agg_countDistinctCtx *ac = Agg_FuncCtx(ctx);
	if(SIValue_IsNull(argv[0])) return AGG_OK;

	ac->values = array_append(ac->values, SI_CloneValue(argv[0]));
	return AGG_OK;
}

int __agg_countDistinctReduceNext(AggCtx *ctx) {
	__agg_countDistinctCtx *ac = Agg_FuncCtx(ctx);

	uint32_t item_count = array_len(ac->values);
	if(item_count <= 1) {
		Agg_SetResult(ctx, SI_LongVal(item_count));
		return AGG_OK;
	}

	/* Sort array, don't count duplicates */
	QSORT(SIValue, ac->values, item_count, SI_LT);

	uint32_t distinct = 1;  // We've got atleast one item.
	SIValue a = ac->values[0];
	SIValue b = SI_NullVal();
	for(uint32_t i = 1; i < item_count; i++) {
		b = ac->values[i];
		if(SIValue_Compare(a, b, NULL) != 0) distinct++;
		SIValue_Free(&a);
		a = b;
	}
	if(!SIValue_IsNull(b)) SIValue_Free(&b);

	Agg_SetResult(ctx, SI_LongVal(distinct));

	array_free(ac->values);
	return AGG_OK;
}

AggCtx *Agg_CountDistinctFunc() {
	__agg_countDistinctCtx *ac = malloc(sizeof(__agg_countCtx));
	ac->values = array_new(SIValue, 0);

	return Agg_Reduce(ac, __agg_countDistinctStep, __agg_countDistinctReduceNext);
}

//------------------------------------------------------------------------

typedef struct {
	double percentile;
	double *values;
	size_t count;
	size_t values_allocated;
} __agg_percCtx;

// This function is agnostic as to percentile method
int __agg_percStep(AggCtx *ctx, SIValue *argv, int argc) {
	__agg_percCtx *ac = Agg_FuncCtx(ctx);

	// The last argument is the requested percentile, which we only
	// need to apply on the first function invocation (at which time
	// _agg_percCtx->percentile will be -1)
	if(ac->percentile < 0) {
		if(!SIValue_ToDouble(&argv[argc - 1], &ac->percentile)) {
			return Agg_SetError(ctx,
								"PERC_DISC Could not convert percentile argument to double");
		}
		if(ac->percentile < 0 || ac->percentile > 1) {
			return Agg_SetError(ctx,
								"PERC_DISC Invalid input for percentile is not a valid argument, must be a number in the range 0.0 to 1.0");
		}
	}

	if(ac->count + argc - 1 > ac->values_allocated) {
		ac->values_allocated *= 2;
		ac->values = realloc(ac->values, sizeof(double) * ac->values_allocated);
	}

	double n;
	for(int i = 0; i < argc - 1; i ++) {
		if(!SIValue_ToDouble(&argv[i], &n)) {
			if(!SIValue_IsNullPtr(&argv[i])) {
				// not convertible to double!
				return Agg_SetError(ctx,
									"PERC_DISC Could not convert upstream value to double");
			} else {
				return AGG_OK;
			}
		}
		ac->values[ac->count] = n;
		ac->count++;
	}

	return AGG_OK;
}

int __agg_percDiscReduceNext(AggCtx *ctx) {
	__agg_percCtx *ac = Agg_FuncCtx(ctx);

	QSORT(double, ac->values, ac->count, ISLT);

	// If ac->percentile == 0, employing this formula would give an index of -1
	int idx = ac->percentile > 0 ? ceil(ac->percentile * ac->count) - 1 : 0;
	double n = ac->values[idx];
	Agg_SetResult(ctx, SI_DoubleVal(n));

	free(ac->values);
	return AGG_OK;
}

int __agg_percContReduceNext(AggCtx *ctx) {
	__agg_percCtx *ac = Agg_FuncCtx(ctx);

	QSORT(double, ac->values, ac->count, ISLT);

	if(ac->percentile == 1.0 || ac->count == 1) {
		Agg_SetResult(ctx, SI_DoubleVal(ac->values[ac->count - 1]));
		free(ac->values);
		return AGG_OK;
	}

	double int_val, fraction_val;
	double float_idx = ac->percentile * (ac->count - 1);
	// Split the temp value into its integer and fractional values
	fraction_val = modf(float_idx, &int_val);
	int index = int_val; // Casting the integral part of the value to an int for convenience

	if(!fraction_val) {
		// A valid index was requested, so we can directly return a value
		Agg_SetResult(ctx, SI_DoubleVal(ac->values[index]));
		free(ac->values);
		return AGG_OK;
	}

	double lhs, rhs;
	lhs = ac->values[index] * (1 - fraction_val);
	rhs = ac->values[index + 1] * fraction_val;

	Agg_SetResult(ctx, SI_DoubleVal(lhs + rhs));

	free(ac->values);
	return AGG_OK;
}

// The percentile initializers are identical save for the ReduceNext function they specify
AggCtx *Agg_PercDiscFunc() {
	__agg_percCtx *ac = malloc(sizeof(__agg_percCtx));
	ac->count = 0;
	ac->values = malloc(1024 * sizeof(double));
	ac->values_allocated = 1024;
	// Percentile will be updated by the first call to Step
	ac->percentile = -1;
	return Agg_Reduce(ac, __agg_percStep, __agg_percDiscReduceNext);
}

AggCtx *Agg_PercContFunc() {
	__agg_percCtx *ac = malloc(sizeof(__agg_percCtx));
	ac->count = 0;
	ac->values = malloc(1024 * sizeof(double));
	ac->values_allocated = 1024;
	// Percentile will be updated by the first call to Step
	ac->percentile = -1;
	return Agg_Reduce(ac, __agg_percStep, __agg_percContReduceNext);
}

//------------------------------------------------------------------------

typedef struct {
	double *values;
	double total;
	size_t count;
	size_t values_allocated;
	int is_sampled;
} __agg_stdevCtx;

int __agg_StdevStep(AggCtx *ctx, SIValue *argv, int argc) {
	__agg_stdevCtx *ac = Agg_FuncCtx(ctx);

	if(ac->count + argc > ac->values_allocated) {
		ac->values_allocated *= 2;
		ac->values = realloc(ac->values, sizeof(double) * ac->values_allocated);
	}

	double n;
	for(int i = 0; i < argc; i ++) {
		if(!SIValue_ToDouble(&argv[i], &n)) {
			if(!SIValue_IsNullPtr(&argv[i])) {
				// not convertible to double!
				return Agg_SetError(ctx,
									"STDEV Could not convert upstream value to double");
			} else {
				return AGG_OK;
			}
		}
		ac->values[ac->count] = n;
		ac->total += n;
		ac->count++;
	}

	return AGG_OK;
}

int __agg_StdevReduceNext(AggCtx *ctx) {
	__agg_stdevCtx *ac = Agg_FuncCtx(ctx);

	if(ac->count < 2) {
		Agg_SetResult(ctx, SI_DoubleVal(0));
		free(ac->values);
		return AGG_OK;
	}

	double mean = ac->total / ac->count;
	long double sum = 0;
	for(int i = 0; i < ac->count; i ++) {
		sum += (ac->values[i] - mean) * (ac->values[i] + mean);
	}
	// is_sampled will be equal to 1 in the Stdev case and 0 in the StdevP case
	double variance = sum / (ac->count - ac->is_sampled);
	double stdev = sqrt(variance);

	Agg_SetResult(ctx, SI_DoubleVal(stdev));

	free(ac->values);
	return AGG_OK;
}

AggCtx *Agg_StdevFunc() {
	__agg_stdevCtx *ac = malloc(sizeof(__agg_stdevCtx));
	ac->is_sampled = 1;
	ac->count = 0;
	ac->total = 0;
	ac->values = malloc(1024 * sizeof(double));
	ac->values_allocated = 1024;
	return Agg_Reduce(ac, __agg_StdevStep, __agg_StdevReduceNext);
}

// StdevP is identical to Stdev save for an altered value we can check for with a bool
AggCtx *Agg_StdevPFunc() {
	AggCtx *func = Agg_StdevFunc();
	__agg_stdevCtx *ac = Agg_FuncCtx(func);
	ac->is_sampled = 0;
	return func;
}

//------------------------------------------------------------------------

typedef struct {
	SIValue list;
} __agg_collectCtx;

int __agg_collectStep(AggCtx *ctx, SIValue *argv, int argc) {
	// convert multiple values to array

	assert(argc >= 0);
	__agg_collectCtx *ac = Agg_FuncCtx(ctx);

	if(ac->list.type == T_NULL) ac->list = SI_Array(argc);
	for(int i = 0; i < argc; i ++) {
		SIValue value = argv[i];
		if(value.type == T_NULL) continue;
		SIArray_Append(&ac->list, value);
	}
	return AGG_OK;
}

int __agg_collectReduceNext(AggCtx *ctx) {
	__agg_collectCtx *ac = Agg_FuncCtx(ctx);

	Agg_SetResult(ctx, ac->list);
	return AGG_OK;
}

AggCtx *Agg_CollectFunc() {
	__agg_collectCtx *ac = malloc(sizeof(__agg_collectCtx));
	ac->list = SI_NullVal();

	return Agg_Reduce(ac, __agg_collectStep, __agg_collectReduceNext);
}

//------------------------------------------------------------------------

typedef struct {
	SIValue list;
} __agg_collectDistinctCtx;

int __agg_collectDistinctStep(AggCtx *ctx, SIValue *argv, int argc) {
	// convert multiple values to array

	assert(argc >= 0);
	__agg_collectDistinctCtx *ac = Agg_FuncCtx(ctx);

	// TODO: can this be migrated to Agg_CollectDistinctFunc?
	if(ac->list.type == T_NULL) ac->list = SI_Array(argc);
	for(int i = 0; i < argc; i ++) {
		SIValue value = argv[i];
		if(value.type == T_NULL) continue;
		SIArray_Append(&ac->list, value);
	}
	return AGG_OK;
}

int __agg_collectDistinctReduceNext(AggCtx *ctx) {
	__agg_collectDistinctCtx *ac = Agg_FuncCtx(ctx);

	if(SIValue_IsNull(ac->list)) {
		Agg_SetResult(ctx, ac->list);
		return AGG_OK;
	}

	uint32_t item_count = SIArray_Length(ac->list);
	if(item_count <= 1) {
		Agg_SetResult(ctx, ac->list);
		return AGG_OK;
	}

	SIValue *items = ac->list.array;
	SIValue distinct_array = SI_Array(item_count);

	/* Sort array, don't count duplicates */
	QSORT(SIValue, items, item_count, SI_LT);

	SIValue a = items[0];
	SIValue b = SI_NullVal();

	// Append first element.
	SIArray_Append(&distinct_array, SI_CloneValue(a));

	for(uint32_t i = 1; i < item_count; i++) {
		b = items[i];
		if(SIValue_Compare(a, b, NULL) != 0)  {
			SIArray_Append(&distinct_array, SI_CloneValue(b));
		}
		a = b;
	}
	Agg_SetResult(ctx, distinct_array);
	SIValue_Free(&ac->list);
	return AGG_OK;
}

AggCtx *Agg_CollectDistinctFunc() {
	__agg_collectDistinctCtx *ac = malloc(sizeof(__agg_collectDistinctCtx));
	ac->list = SI_NullVal();

	return Agg_Reduce(ac, __agg_collectDistinctStep, __agg_collectDistinctReduceNext);
}

//------------------------------------------------------------------------

void Agg_RegisterFuncs() {
	Agg_RegisterFunc("sum", Agg_SumFunc);
	Agg_RegisterFunc("avg", Agg_AvgFunc);
	Agg_RegisterFunc("max", Agg_MaxFunc);
	Agg_RegisterFunc("min", Agg_MinFunc);
	Agg_RegisterFunc("count", Agg_CountFunc);
	Agg_RegisterFunc("countDistinct", Agg_CountDistinctFunc);
	Agg_RegisterFunc("percentileDisc", Agg_PercDiscFunc);
	Agg_RegisterFunc("percentileCont", Agg_PercContFunc);
	Agg_RegisterFunc("stDev", Agg_StdevFunc);
	Agg_RegisterFunc("stDevP", Agg_StdevPFunc);
	Agg_RegisterFunc("collect", Agg_CollectFunc);
	Agg_RegisterFunc("collectDistinct", Agg_CollectDistinctFunc);
}
