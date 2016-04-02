
EXPE_FOLDERS := $(wildcard tests/*)
SIMULS_OUT := $(patsubst %,%/out_jobs.csv,$(EXPE_FOLDERS))


all: simuls

simuls: $(SIMULS_OUT)
	cd ..; coverage html -d tests/coverage_html


%/out_jobs.csv:
	cd ..;python launch_expe.py tests/$*/expe.json


generate:
	python generate.py


clean:
	rm tests/*/out_jobs.csv
	cd ..; python -m coverage erase