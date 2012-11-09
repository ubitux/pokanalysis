all:
	python2 setup.py build_ext --inplace
clean:
	$(RM) -r build
distclean: clean
	$(RM) pokerom.so
re: distclean all
tests: all
	$(MAKE) -C tests
