# PositronBot

This bot is mostly copied ParticleBot2, repackaged into a ParticlePM package.

It has slight modifications though, which include possibility to add a plugin directory, renaming pb2 into pb, etc.

## Building

1. Install [ParticlePM](https://github.com/handicraftsman/particlepm) using CMake.
2. Create a package.cpp with content like below:
  ```cpp
  #include <particlepm.hpp>

  using namespace PPM::Utils;

  PPM_PACKAGE(p) {
    p->name("pb-instance");
    p->version(PPM::Version("v0.1.0"));
    p->description("A PositronBot instance");
    p->github(PPM::GitHub("vpupkin", "pb-instance"));
    p->license(PPM::License("UNLICENSED", ""));

    p->github_repo("handicraftsman", "positronbot");
    // you can include plugins using p->github_repo function
  }
  ```
3. Run `particlepm fetch` to download positronbot and included plugins.
4. Run `particlepm build` to build positronbot and included plugins. You may need to prefix this command with `CC=gcc-8 CXX=g++-8` on Ubuntu.
5. Built libraries and executables can be found in the `dist/` directory.

## Configuring

An example config can be found in the [positronbot.sample.ini](https://github.com/handicraftsman/positronbot/blob/master/positronbot.sample.ini) file.

## Starting

You can start the bot with the command shown below:

```bash
$ /path/to/dist/positronbot -p /path/to/dist/ -c /path/to/positronbot.ini -d /path/to/positronbot.db -l debug
```

1. `-p` option adds a folder to the plugin search directory list. It should be the `dist/` directory in particlepm projects.
2. `-c` option sets config path.
3. `-d` option sets database path. If given file does not exist, it will be created automatically.
4. `-l` option sets log level. Available log levels: `debug, [io | irc], info, warning, error, important, critical`.

## Developing plugins

Thanks to particlepm, you can easily create and distribute plugins.

The only thing you need to do is to create a package manifest which describes your plugin:

```cpp
#include <particlepm.hpp>

using namespace PPM::Utils;

PPM_PACKAGE(p) {
  p->name("pb-helloworld");
  p->version(PPM::Version("v0.1.0"));
  p->description("A 'Hello, World!' plugin for PositronBot");
  p->github(PPM::GitHub("vpupkin", "pb-helloworld"));
  p->license(PPM::License("YourLicense", "https://github.com/vpupkin/pb-helloworld/blob/master/LICENSE.txt"));

  PPM::GitHubPtr 
    pb_d    = p->github_repo("handicraftsman", "positronbot"),
    pdi_d   = p->github_repo("handicraftsman", "particledi"),
    pini_d  = p->github_repo("handicraftsman", "particleini"),
    guosh_d = p->github_repo("handicraftsman", "guosh");
  // you can include any other libraries here

  std::vector<std::string> src {
    "main.cpp"
  };

  std::string flags = flagcat({
    ("-L" + PPM::dist_dir),

    "-lpositronbot",
    "-lparticledi",
    "-lparticleini",
    "-lguosh",

    "-I.",
    ("-I" + pb_d->dir()),
    ("-I" + pdi_d->dir()),
    ("-I" + pini_d->dir()),
    ("-I" + guosh_d->dir() + "/src")
  });

  PPM::TargetPtr plugin = p->library("pb-helloworld");
  plugin->cpp("gnu++17");
  plugin->cpp_files(src);
  plugin->cpp_flags(flags);
}
```

Since plugin development workspace also includes all packages required to run the bot,
you can simply start the bot as described above.