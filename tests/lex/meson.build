# The lex tests simply lex a file and compare the output of the token dump to
# the *.txt.expect version of the file.
lex_runner = executable(
	'runner',
	files('runner.c'),
	dependencies: starlark_dep,
)

test(
	'symbols',
	lex_runner,
	args: files('symbols.txt'),
	suite: 'lex',
)

test(
	'no whitespace',
	lex_runner,
	args: files('no_whitespace.txt'),
	suite: 'lex',
)

test(
	'identifiers',
	lex_runner,
	args: files('identifiers.txt'),
	suite: 'lex',
)

test(
	'numbers',
	lex_runner,
	args: files('numbers.txt'),
	suite: 'lex',
)

test(
	'strings',
	lex_runner,
	args: files('strings.txt'),
	suite: 'lex',
)

test(
	'comments',
	lex_runner,
	args: files('comments.txt'),
	suite: 'lex',
)

# DON'T EDIT THIS FILE
# It has invalid UTF-8 in it which some text editors will try to replace with
# U+FFFD
test(
	'invalid utf8',
	lex_runner,
	args: files('invalid_utf8.txt'),
	suite: 'lex',
)
