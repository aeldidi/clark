# The parse tests parse a file and compare the output of the syntax tree to
# the *.txt.expect version of the file.
parse_runner = executable(
	'runner',
	files('runner.c'),
	dependencies: starlark_dep,
)

test(
	'integers',
	parse_runner,
	args: files('integers.txt'),
	suite: 'parse',
)

test(
	'floats',
	parse_runner,
	args: files('floats.txt'),
	suite: 'parse',
)
