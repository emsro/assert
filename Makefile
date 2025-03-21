
clang-tidy:
	find ./asrt* \( -iname "*.h" -o -iname "*.c" \) | grep -v oof | xargs clang-tidy -p ./_build/
