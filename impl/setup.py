from distutils.core import setup, Extension

module1 = Extension('CutDetect',
					define_macros = [('MAJOR_VERSION', '3'),
									 ('MINOR_VERSION', '2')],
					extra_objects = ['shotbounds.so'],
					extra_compile_args = ['-std=c99'],
					include_dirs = ['include'],
					sources = ['src/shotboundsWrapper.c'])

setup (name = 'CutDetectPkg',
		version = '1.0',
		description = 'Wrapper for cut detection using ffmpeg',
		ext_modules=[module1])
