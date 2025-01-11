NAME = 'host'

dependencies = [
  dependency('doca-common'), 
  dependency('doca-dma'),
  dependency('doca-comch'),
  dependency('threads'),
]

srcs = [
  'main.cpp',
  'host.cpp',
  'dma_server.cpp',
  '../common/dma_common.cpp',
  '../common/comch_common.cpp',
]

inc_dirs = [
  include_directories('..'),
  include_directories('../common'),
  include_directories('/opt/mellanox/doca/include'),
]

executable(
  NAME,
  srcs,
  c_args: '-Wno-missing-braces',
  dependencies: dependencies,
  include_directories: inc_dirs,
  install: false,
)