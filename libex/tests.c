
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include "libex.h"

/* Tests:
 * 1. returned void, errno test.
 * 2. returned exc_type test.
 * 3. returned int return code test.
 * 4. returned HRESULT test.
 * 5. returned ok/error int test.
 * 6. returned NULL, errno test.
 */

#define requires(E) *p++; assert(E)
#define mark(p) (*(p))++

static exc_type test_unwind_try(exc_type e, int* p) {
	THROWS(e)
	TRY() {
		mark(p);
		TRY() {
			mark(p);
			THROW(e);
			assert(0);
		} IN {
			assert(0);
		} HANDLE CATCHANY {
			mark(p);
			assert(__CUR_EXC__ == e);
			THROW(EInvalidOp)
			assert(0);
		} FINALLY {
			mark(p);
			assert(__CUR_EXC__ != e);
		}
		ENDTRY;
		assert(0);
	} IN {
		assert(0);
	} HANDLE CATCH(EInvalidOp) {
		mark(p);
		THROW(e)
	} CATCHANY {
		assert(0);
	} FINALLY {
		mark(p);
	}
	DONE;
}

static exc_type test_unwind_in(exc_type e, int* p) {
	THROWS(e)
	TRY() {
		mark(p);
	} IN {
		mark(p);
		THROW(e);
		assert(0);
	} HANDLE CATCHANY {
		assert(0);
	} FINALLY {
		mark(p);
		assert(__CUR_EXC__ == e);
	}
	DONE;
}

static void set_errno(exc_type e) {
	errno = e;
}
static exc_type test_errno(exc_type e) {
	THROWS(e)
	TRY() {
		CHECK(set_errno(e));
	} IN {
		assert(e == ENoError && __CUR_EXC__ == ENoError);
	} HANDLE CATCHANY {
		assert(e != ENoError && __CUR_EXC__ != ENoError);
		RETHROW;
	} FINALLY {
		assert(__CUR_EXC__ == e);
	}
	DONE;
}

#define run_test(E) p = 0; assert(E)

int main(char ** argv, size_t argc) {
	int p;
	run_test(EUnrecoverable == test_unwind_try(EUnrecoverable, &p) && p == 6);
	run_test(EUnrecoverable == test_unwind_in(EUnrecoverable, &p) && p == 3);
	run_test(ENoError == test_errno(ENoError));
	run_test(EUnrecoverable == test_errno(EUnrecoverable));
	return 0;
}
