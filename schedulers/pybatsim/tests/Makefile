
EXPE_FOLDERS := $(wildcard tests/*)
SIMULS_OUT := $(patsubst %,%/out_jobs.csv,$(EXPE_FOLDERS))


all: simuls

simuls: $(SIMULS_OUT)


%/out_jobs.csv:
	cd ..;python launch_expe.py tests/$*/expe.json
	cd ..; coverage html -d tests/coverage_html


generate:
	python generate.py


clean:
	rm tests/*/out_jobs.csv
	cd ..; coverage erase