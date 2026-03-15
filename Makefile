
clang-tidy:
	find ./asrtc ./asrtl ./asrtr \( -iname "*.h" -o -iname "*.c" \) | xargs clang-tidy -p ./_build/ --warnings-as-errors='*'
