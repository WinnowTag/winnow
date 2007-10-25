// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include <stdlib.h>
#include <math.h>
#include "classifier.h"

/* Returns prob(chisq >= x2, with v degrees of freedom)
 *
 * Algorithm taken from http://spambayes.cvs.sourceforge.net/spambayes/spambayes/spambayes/chi2.py?view=markup
 */
double chi2Q(double x2, int v) {
  double chi2;
  
  if (v <= 0 || v % 2 != 0) {
    chi2 = -1.0;
  } else {
    int i;
    int max_i;
    double m;
    double sum;
    double term;
    
    m = x2 / 2;
    max_i = v / 2;
    sum = exp(-m);
    term = sum;

    for (i = 1; i <= max_i; i++) {
      term *= m / i;
      sum += term;
    }

    if (sum > 1.0) {
      sum = 1.0;
    }
    
    chi2 = sum;
  }
  
  return chi2;
}