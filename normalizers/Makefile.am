AM_CFLAGS =					\
	$(GROONGA_CFLAGS)

AM_LDFLAGS =					\
	-avoid-version				\
	-module					\
	-no-undefined

LIBS =						\
	$(GROONGA_LIBS)

normalizers_plugins_LTLIBRARIES =
normalizers_plugins_LTLIBRARIES += yamysql.la 

normalizers_plugins_SOURCE =			\
	yamysql.c

if WITH_MECAB
yamysql_la_CFLAGS = $(AM_CFLAGS) $(MECAB_CPPFLAGS)
yamysql_la_LIBADD = $(LIBS) $(MECAB_LIBS)
yamysql_la_LDFLAGS = $(AM_LDFLAGS) $(MECAB_LDFLAGS)
endif
