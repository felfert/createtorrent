bin_PROGRAMS = createtorrent
createtorrent_SOURCES = main.c
createtorrent_LDADD =
createtorrent_CPPFLAGS = -D_FILE_OFFSET_BITS=64
createtorrent_CFLAGS = -Wall -Wextra

MAINTAINERCLEANFILES = -r \
		aclocal.m4 config.h.in* configure Makefile.in \
        missing compile depcomp install-sh \
        config.sub config.guess autom4te.cache .deps

# After checkout from git, run make -f Makefile.am
bootstrap:
	autoreconf -ifs
