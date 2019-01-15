default:
	@echo "Try make build, make build-nocache or make push."

build:
	docker build -t oarteam/batsim_ci:latest \
                 -t oarteam/batsim_ci:$$(date --iso-8601) \
                 .

build-nocache:
	docker build -t oarteam/batsim_ci:latest \
                 -t oarteam/batsim_ci:$$(date --iso-8601) \
                 --no-cache \
                 .

push:
	docker push oarteam/batsim_ci
