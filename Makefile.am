ACLOCAL_AMFLAGS = -I m4
SUBDIRS = src . tests schema bin

dist-hook:
	rm -rf `find $(distdir) -name .svn`
	rm -rf $(distdir)/tests/fixtures/classifier_test.sql.gz
