.PHONY: reformat
reformat:
	find GPTP/ -iname *.h -o -iname *.cpp | xargs clang-format -i -style=file
