project(
  'LEVELKV',
  'CPP',
  'C',
  version: run_command(
    find_program('cat'),
    files('/opt/mellanox/doca/applications/VERSION'),
    check: true,
  ).stdout().strip(),
  license: 'Proprietary',
  default_options: ['buildtype=release', 'cpp_std=c++17', 'optimization=2'],
  meson_version: '>= 0.61.2',
)

add_project_arguments('-D DOCA_ALLOW_EXPERIMENTAL_API', '-O2', language: ['cpp', 'c'])


subdir('src/host')
subdir('src/dpu')