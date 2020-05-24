# Intro

TODO

# Building

You can either build log-analyzer as a command-line application or as a shared library (e.g.: to be used in a UI). You just need to have a C++17 compliant compiler and at least CMake 3.12.4.

For example, to build the x86 console app for Windows, using VStudio 2019, run this in the root folder of the project:

```
mkdir build-windows
cd build-windows
cmake -DBUILD_CONSOLE=ON -G "Visual Studio 16 2019" -A Win32 -T host=x64 ../
```

For Linux (using Ninja):
```
mkdir build-linux
cd build-linux
cmake -DBUILD_CONSOLE=ON -DCMAKE_MAKE_PROGRAM=<Ninja full path> -G "Ninja" ../
```

TODO

# Code guideline

The project is divided into 4 main folders:
* core: most of the log-analyzer source code (file mapping, parsers, commands, etc.)
* console: the command-line application code when building as a console (BUILD_CONSOLE is ON in CMake)
* shared: the C API that exposes every feature when building log-analyzer as a shared library (default CMake build)
* third_party: external dependencies

Most of the time, a developer will simply want to add a new command or even perhaps a new flavor of logs, which means that, usually, it only needs to hack code inside the "core" folder.

## How to add a new flavor

A "flavor" is a type of log files. For example, if you have 4 different applications that produce two distinct types of log files (for example, one is in a text format and the other in binary), you would create two new flavors: MyAppText, MyAppBin. What you do with the data of one type/flavor of log are called commands.

Steps to add a new flavor:
1. Go to the ```FlavorsRepo``` class (in file "flavors_repo.cpp") and add a new entry to the ```Type``` enum
2. Go to the "flavors" folder and add a new .cpp and .hpp files with the appropriate names and following the guidelines of the already existing flavors:
    1. In the header file, add a single class with a single static method with the signature ```static FlavorsRepo::Info genFlavorsRepoInfo();```
    2. In the source file, implement that method which should return an instance of ```FlavorsRepo::Info``` properly configured
3. Finally, go the "flavors_repo.hpp" file and add a new entry to the ```Flavors``` static array with the enum and method of your new flavor.

## How to add a new command

Flavors are just parsers, which means that, after they're done, we just have an index to every line of every log file. We then need to create commands that make use of this index. Examples of commands are:
* extract all errors
* create data views (e.g.: which user did what at what time)
* create summaries / reports

A command can be used with more that one flavor. Using the example above, if MyAppText and MyAppBin index to the same kind of data, then any command used in one can be also used in the other.

To create a new command, go to the "commands" folder and add a new .cpp and .hpp files with the appropriate names and following the guidelines of the already existing commands:
1. In the header file, add a single class with a single static method with the signature ```static CommandsRepo::Registry genCommandsRegistry();```
2. In the source file, implement that method which should return an instance of ```CommandsRepo::Registry``` properly configured

A command registry is called whenever a new lines repository is created, or in other words, each time a new flavor is processed.

Commands must have a valid "tag" associated with them. This allows commands to be grouped according to functionality. For example, and continuing using the examples above, we have 4 applications (AppA, AppB, AppC and AppC) writing to two flavors of log files (MyAppText and MyAppBin). Since both flavors produce the same information, all commands that we create can be used with both flavors. But now, imagine that, for the team responsible for AppA, they need to create several commands to analyze the logs and they aren't useful to the remaining applications. They would then create their commands with the tag "AppA", while the other teams would create other tags.
