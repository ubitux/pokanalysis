ifeq ($(ROM),)
    $(error You must set ROM in order to run the tests)
endif

all: tests
tests:
	@./tests.py $(ROM) > results
	@diff refs results
	@echo All tests OK
