# Denv to-do list

## To-do for V1.0
- [x] Option to show current version.
- [x] Function to clear freed elements.
- [x] Function to save memory to a file.
- [x] Function to load memory from a file.
- [x] Implement statistics and meta-data.
- [x] Change denv main to use words as commands instead of signgle letter flags.
- [x] Option to load denv in another path.
- [x] Option to watch variable and block until it's changed.
- [x] Function to execute a program with denv environment variables.
- [x] Make a man page.
- [x] Function to clone environment variables.
- [x] Function to make an file to source environment variables.
- [x] Add a daemon.

**Priority:**
- [ ] Use `make` for build instead of `build.sh`.
- [ ] Parse options into a struct in `main.c`.
- [ ] Reduce code coupling in `main` to simplify parsing.
- [ ] Fix the daemon.

**Release:**
- [ ] Make variables able to store binary data
- [ ] Make a package for .deb, .rpm, Arch Linux and FreeBSD.

**Need help for this part:**
- [ ] Test denv
    - [ ] Create automated tests.
    - [ ] Benchmark.
    - [ ] Fix bugs.

## To-do for V2.0
- [ ] Read lock for each variable, single write lock for the entire thing.
- [ ] Redesign denv to be expandable.
    - [ ] Function to expand the memory table.
    - [ ] Implement a cofiguration file in toml.
- [ ] Add support for locales.
- [ ] Maybe add cryptography and access management. (needs feedback from the users)
