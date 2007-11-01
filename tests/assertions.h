//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include "../src/misc.h"

#ifndef assert_false
#define assert_false(o) fail_unless(o == false, "expected to be false")
#endif

#ifndef assert_true
#define assert_true(o) fail_unless(o == true, "expected to be true")
#endif

#ifndef assert_null
#define assert_null(o) fail_unless(o == NULL, "expected object to be null")
#endif

#ifndef assert_not_null
#define assert_not_null(o) fail_unless(o != NULL, "expected object to not be null")
#endif

#ifndef assert_equal
#define assert_equal(expected, actual) fail_unless(expected == actual, "expected %d but got %d", expected, actual)
#endif

#ifndef assert_equal_s
#define assert_equal_s(e, a) fail_unless(strcmp(e,a) == 0, "expected %s but got %d", e, a)
#endif

#ifndef assert_equal_f
#define assert_equal_f(expected, actual) fail_unless(fabs(expected - actual) <= 0.000001, "expected %f but got %f", expected, actual)
#endif

#ifndef assert_between_ex
#define assert_between_ex(bottom, top, actual) \
  fail_unless(bottom < actual && actual < top, "expected %f to be between %f and %f", actual, bottom, top)
#endif
