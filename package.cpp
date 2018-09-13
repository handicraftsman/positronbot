#include <particlepm.hpp>

using namespace PPM::Utils;

PPM_PACKAGE(p) {
  p->name("positronbot");
  p->version(PPM::Version("v0.1.1"));
  p->description("An IRC bot framework");
  p->github(PPM::GitHub("handicraftsman", "positronbot"));
  p->license(PPM::License("MIT", "https://github.com/handicraftsman/positronbot/blob/master/LICENSE.txt"));

  PPM::GitHubPtr
    pdi_d   = p->github_repo("handicraftsman", "particledi"),
    pini_d  = p->github_repo("handicraftsman", "particleini"),
    guosh_d = p->github_repo("handicraftsman", "guosh");
  std::string sqlite3_f = p->pkgconfig("sqlite3");

  std::vector<std::string> lib_src {
    "positronbot/bot.cpp",
    "positronbot/config_service.cpp",
    "positronbot/db_service.cpp",
    "positronbot/event.cpp",
    "positronbot/ircsocket.cpp",
    "positronbot/ircstream.cpp",
    "positronbot/plugin.cpp",
    "positronbot/plugin_service.cpp"
  };

  std::string flags = flagcat({
    ("-L" + PPM::dist_dir),
    
    "-lparticledi",
    "-lparticleini",
    "-lguosh",
    "-ldl",
    "-lpthread",

    "-I.",
    ("-I" + pdi_d->dir()),
    ("-I" + pini_d->dir()),
    ("-I" + guosh_d->dir() + "/src"),

    sqlite3_f
  });

  PPM::TargetPtr lib = p->library("libpositronbot");
  lib->name("positronbot");
  lib->cpp("gnu++17");
  lib->cpp_files(lib_src);
  lib->cpp_flags(flags);

  PPM::TargetPtr core_p = p->library("pb-core");
  core_p->cpp("gnu++17");
  core_p->cpp_files(std::vector<std::string> {
    "plugins/core.cpp"
  });
  core_p->cpp_flags(flags);
  core_p->cpp_flags("-lpositronbot");
  core_p->depends(lib); 

  PPM::TargetPtr exe = p->executable("positronbot");
  exe->cpp("gnu++17");
  exe->cpp_files(std::vector<std::string> {
    "main.cpp"
  });
  exe->cpp_flags(flags);
  exe->cpp_flags("-lpositronbot");
  exe->depends(lib); 
}