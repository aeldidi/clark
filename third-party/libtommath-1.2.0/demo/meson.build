# SPDX-License-Identifier: Unlicense
test_exe = executable(
	'libtommath_test',
	[
		files(
			'shared.c',
			'test.c',
		),
		srcs,
	],
	include_directories: incdirs,
)

test(
	'test',
	test_exe,
        timeout: 60,
)

timing_exe = executable(
	'timing',
	files(
		'shared.c',
		'timing.c',
	),
	dependencies: libtommath_dep,
)

mtest_opponent_exe = executable(
	'mtest_opponent',
	files(
		'shared.c',
		'mtest_opponent.c',
	),
	dependencies: libtommath_dep,
)
