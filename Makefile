
clang-tidy:
	find ./asrtc ./asrtl ./asrtr \( -iname "*.h" -o -iname "*.c" \) | xargs clang-tidy -p ./_build/ --warnings-as-errors='*'

# Build the Docker CI image (cached after first run)
docker-image:
	docker build -t assert-ci .

# Run any preset inside the Docker CI environment, e.g. make ci-asan
ci-%: docker-image
	./docker-ci.sh $*

.PHONY: clang-tidy docker-image
