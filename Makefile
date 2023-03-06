ROM ?= $(PWD)/pokered.gbc

web: update
	python -m http.server --directory www
update:
	(cd extractor && cargo build --release)
	extractor/target/release/pokanalysis $(ROM) www/out
test: update
	diff -ur www/ref www/out
ref:
	(cd extractor && cargo build --release)
	$(RM) -r www/ref
	extractor/target/release/pokanalysis $(ROM) www/ref
clean:
	$(RM) -r extractor/target
	$(RM) -r www/out
	$(RM) -r www/ref

.PHONY: web update test ref clean
