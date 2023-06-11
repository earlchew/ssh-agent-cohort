.PHONY:	all
all:	ssh-agent-cohort

.PHONY:	man
man:	ssh-agent-cohort.man

.PHONY:	lib
lib:	library.a

.PHONY:	clean
clean:
	$(RM) lib/*.o
	$(RM) library.a
	$(RM) ssh-agent-cohort

.PHONY:	check
check:	ssh-agent-cohort
	#
	VALGRIND='$(VALGRIND)' test/check

CFLAGS = -Wall -Werror -Wshadow -D_GNU_SOURCE -Ilib/
ssh-agent-cohort:	ssh-agent-cohort.c library.a

LIBOBJS = $(patsubst %.c,%.o,$(wildcard lib/*.c))
ARFLAGS = crvs
library.a:	$(foreach o,$(LIBOBJS),library.a($o))
.SECONDARY:

ssh-agent-cohort.man:	ssh-agent-cohort.1
	nroff -man $< >$@.tmp && mv $@.tmp $@
