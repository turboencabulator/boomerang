/**
 * \file
 * \ingroup UnitTest
 *
 * Compares selected members of the OPER enum against the operStrings table.
 *
 * To use, remove the binary, then `make checkstrings`.
 *
 * \note Could say "All is correct" when not, if some operators are deleted
 * and the same number added.
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "operator.h"
#include "operstrings.h"

#include <cstdio>
#include <cstring>

#define checkpoint(x) { x, #x }
static const struct {
	size_t index;
	const char *name;
} checklist[] = {
	checkpoint(opFPlusd),
	checkpoint(opSQRTd),
	checkpoint(opGtrEqUns),
	checkpoint(opRotateL),
	checkpoint(opList),
	checkpoint(opMachFtr),
	checkpoint(opFpop),
	checkpoint(opExecute),
	checkpoint(opWildStrConst),
	checkpoint(opTrue),
};

int
main()
{
	if (sizeof operStrings / sizeof *operStrings == opNumOf) {
		printf("All is correct\n");
		return 0;
	}

	for (size_t i = 0; i < sizeof checklist / sizeof *checklist; ++i) {
		if (strcmp(operStrings[checklist[i].index], checklist[i].name) != 0) {
			printf("Error at or before %s\n", checklist[i].name);
			return 1;
		}
	}
	printf("Error near the end\n");
	return 1;
}
