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
  default_options: ['buildtype=debug', 'cpp_std=c++17'],
  meson_version: '>= 0.61.2',
)

add_project_arguments('-D DOCA_ALLOW_EXPERIMENTAL_API', language: ['cpp', 'c'])


subdir('src/host')
subdir('src/dpu')