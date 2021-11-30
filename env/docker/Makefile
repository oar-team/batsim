default:
	@echo "Try make build, make build-nocache or make push."

image: default.nix
	nix-build default.nix -A batsim-ci-image -o image

build: image
	docker load -i ./image && docker tag oarteam/batsim_ci:latest oarteam/batsim_ci:$$(date --iso-8601)

push:
	docker push oarteam/batsim_ci:$$(date --iso-8601) && docker push oarteam/batsim_ci:latest
