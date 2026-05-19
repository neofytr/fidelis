# Session persistence module. Pure JSON serialise / load over std::filesystem.

session_lib = static_library(
  'fidelis_session',
  'session.cpp',
  include_directories : inc_public,
  dependencies : [nlohmann_vendored_dep],
)

session_dep = declare_dependency(
  link_with : session_lib,
  include_directories : inc_public,
  dependencies : [nlohmann_vendored_dep],
)
