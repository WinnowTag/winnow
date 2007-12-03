SUBDIRS = src . tests
EXTRA_DIST = lib

dist-hook:
	rm -rf `find $(distdir) -name .svn`
	rm -rf $(distdir)/tests/fixtures/classifier_test.sql.gz
