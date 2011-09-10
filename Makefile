all:
	python setup.py build_ext --inplace
clean:
	$(RM) -r build
distclean: clean
	$(RM) pokerom.so
re: distclean all
