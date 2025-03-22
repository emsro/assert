
clang-tidy:
	find ./asrt* \( -iname "*.h" -o -iname "*.c" -o -iname "*.hpp" -o -iname "*.cpp" \) | grep -v oof | xargs clang-tidy -p ./_build/
