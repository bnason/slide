project(
	'slide', 'cpp', 'c',
	version : '0.1',
	default_options : [
			'warning_level=3',
			'cpp_std=c++20',
			'buildtype=release',
			'default_library=static'
	],
)

add_global_arguments(
	[
		'-Wall',
		'-Wno-pointer-sign',
		'-Wno-deprecated-declarations',
		'-Wno-unused',
		'-Wno-incompatible-pointer-types',
		'-Wno-discarded-qualifiers'
	],
	language: 'c'
)

if get_option('buildtype') == 'debug'
	add_project_arguments('--optimize=g', language: 'c')
endif

wayland_protos = dependency('wayland-protocols')
wlroots = dependency('wlroots', version: '>= 0.17.0-dev')
libinput = dependency('libinput', required: get_option('libinput'))

source_files = []

subdir('src')

executable(
	'slide',
	source_files,
	c_args: '-DWLR_USE_UNSTABLE',
	dependencies: [
		wayland_protos,
		wlroots,
		# libinput,
	],
	install: true,
)
