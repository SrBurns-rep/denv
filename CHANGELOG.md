# Changelog 

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

<!-- Version Index -->
* [0.10.6](#0.10.6)
* [0.10.4](#0.10.4)
* [0.10.3](#0.10.3)
* [0.10.2](#0.10.2)
* [0.10.0](#0.10.0)
* [0.9.2](#0.9.2)

<!-- Changelog Description -->
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
