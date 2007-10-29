//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

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