# Changelog 

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

<!-- Version Index -->
* [0.13.10](#01310)
* [0.13.8](#0138)
* [0.13.7](#0137)
* [0.13.6](#0136)
* [0.13.2](#0132)
* [0.13.1](#0131)
* [0.13.0](#0130)
* [0.10.6](#0106)
* [0.10.4](#0104)
* [0.10.3](#0103)
* [0.10.2](#0102)
* [0.10.0](#0100)
* [0.9.2](#092)

<!-- Changelog Description -->
## 0.13.10

### Changed 
* `stats` command now has room for more formats.
* `await` function was moved to `denv.h`.

## 0.13.8

### Changed
* `stats` command now outputs in csv format.

## 0.13.7

### Fixed
* Loading variables from a file with `-f` option.

## 0.13.6

"Unixifying" denv commands

### Changed
* `delete` command is now `rm`.
* `list` command is now `ls`.
* `remove` command is now `drop`.

### Fixed
* Await bug when using `-b` option.

## 0.13.2

### Fixed
* Denv was loading it's default table even when creating other bind paths.

## 0.13.1

### Fixed
* Segfault when awaiting a variable not added yet.

## 0.13.0

### Added
* Flag when element is updated.
* Element on update function.
* Get element function.
* Option to await variable on change.

### Changed
* Denv set function now sets updated flag.
* denv_shmem_destroy no longer needs table, only shmem bind path.

### Fixed
* Removing table no longer loads the table (useful for removing broken tables).

## 0.10.6

### Added
* Flag to force yes to operations that prompts the user.

### Changed
* `-f` flags are now interpreted as "force yes" to operations that prompts the user.

### Fixed
* Bug when loading to a different path.

## 0.10.4

### Fixed
* Bug when trying to get a variable that doesn't exist.

## 0.10.3

### Fixed
* Denv not creating the path for it's default shared environment.

## 0.10.2

### Fixed
* Bug when removing denv instances both from denv default bind path.
* Bug when removing denv from another bind path.

## 0.10.0

### Added
* Contributors file [CONTRIBUTORS.md](CONTRIBUTORS.md).
* Configuration for the gdb debugger ".gdbinit".
* Option to bind denv to another path.

### Changed
* Commands are now full words, options are set as flags.
* License is now GPL-3.

## 0.9.2

### Added
* This changelog file.
* Flag to mark variables that work as environment variables.
* Flag to mark a variable as beeing read.
* A bunch of things to the todo-list for version 1.0.

### Changed
* To-do task of Implementing new instances is moved to version 1.0 as an option to load denv in another path.
* Fix version from 1 to 2.
