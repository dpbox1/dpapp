git ls-files '*.c' '*.h' '*.hh' '*.cc' | xargs -r clang-format -style=file -i
