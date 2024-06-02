project(
	'clark',
	'c',
	default_options: [
		'c_std=c11',
		'warning_level=3',
	],
	subproject_dir: 'third-party',
)

bigint_lib = get_option('bigint-lib')
add_project_arguments(
	[
		'-DCLARK_USE_LIBTOMMATH=' + (bigint_lib == 'libtommath').to_string(),
		'-DCLARK_USE_LIBGMP=' + (bigint_lib == 'libgmp').to_string(),
	],
	language: 'c',
)

if host_machine.system() == 'windows'
	add_project_arguments(
		[
			'-DSTARLARK_PUBLIC=__declspec(dllexport)',
		],
		language: 'c',
	)
else
	add_project_arguments(
		[
			'-DSTARLARK_PUBLIC=',
		],
		language: 'c',
	)
endif

srcs = files(
	'src/starlark/common.c',
	'src/starlark/int.c',
	'src/starlark/lex.c',
	'src/starlark/parse.c',
	'src/starlark/strpool.c',
	'src/starlark/util.c',

	'src/utf8/utf8.c',
	'src/util/diff.c',
	'src/util/fmt.c',
	'src/util/fnv-1a.c',
	'src/util/io.c',
	'src/util/lineno.c',
	'src/util/panic.c',

	'src/util/polyfill.c',
)

incdirs = include_directories(
	'include/',
	'src/',
)

install_headers(
	'include/starlark/common.h',
	'include/starlark/lex.h',
	subdir: 'starlark',
)

libtommath = subproject('libtommath-1.2.0')
libtommath_dep = libtommath.get_variable('libtommath_dep')

cc = meson.get_compiler('c')
math_dep = cc.find_library('m', required: false)

starlark = library(
	'starlark',
	srcs,
	include_directories: incdirs,
	install: true,
	dependencies: [libtommath_dep, math_dep],
)

starlark_dep = declare_dependency(
	link_with: starlark,
	include_directories: incdirs,
)

executable(
	'clark',
	files('src/cmd/starlark/main.c'),
	dependencies: starlark_dep,
	install: true,
)

subdir('tests')