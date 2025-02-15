
clang-tidy:
	find ./asrt* \( -iname "*.h" -o -iname "*.c" \) | xargs clang-tidy -p ./_build/
