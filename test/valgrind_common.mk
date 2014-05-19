SUPPRESSIONS = $(top_srcdir)/test/valgrind.supp

%.valgrind: %
	@$(TESTS_ENVIRONMENT) \
	RUNNING_VALGRIND=yes \
	CK_FORK=no \
	CK_TIMEOUT_MULTIPLIER=10 \
	G_SLICE=always-malloc \
	$(LIBTOOL) --mode=execute \
	valgrind -q \
	$(foreach s,$(SUPPRESSIONS),--suppressions=$(s)) \
	--tool=memcheck --leak-check=full --trace-children=yes \
	--leak-resolution=high --num-callers=30 \
	--error-exitcode=1 \
	./$*

valgrind: $(check_PROGRAMS)
	for t in $(filter-out $(VALGRIND_TESTS_DISABLE),$(check_PROGRAMS)); do \
		$(MAKE) $$t.valgrind; \
	done;


