/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */
#ifndef FIXTURES_H_
#define FIXTURES_H_

static char cwd[1024];

static void setup_fixture_path(void) {
  getcwd(cwd, 1024);
  char *srcdir = getenv("srcdir");
  chdir(srcdir);
}

static void teardown_fixture_path(void) {
  chdir(cwd);
}

#endif /*FIXTURES_H_*/
