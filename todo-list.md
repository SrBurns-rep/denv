# Denv to-do list

## To-do for V1.0
- [x] Option to show current version.
- [x] Function to clear freed elements.
- [x] Function to save memory to a file.
- [x] Function to load memory from a file.
- [x] Implement statistics and meta-data.
- [x] Change denv main to use words as commands instead of signgle letter flags.
- [x] Option to load denv in another path.
- [ ] Load from stdin and save to stdout.
- [ ] Function to load environment variables to it.
- [ ] Function to make an file to source environment variables.
- [ ] Function to spawn a new `$SHELL` with saved environment variables.
- [ ] Function to execute a program with saved environment variables.
- [ ] Read lock for each variable, single write lock for the entire thing.
- [ ] Option to watch variable and block until it's changed.
- [ ] Create automated tests.
- [ ] Benchmark.
- [ ] Fix bugs.
- [ ] Make a man page.
- [ ] Implement a cofiguration file in toml.
- [ ] Fix more bugs and prepare for packaging.
- [ ] Make a package for .deb, .rpm, archlinux and freebsd.

## To-do for V2.0
- [ ] Redesign denv to be expandable.
- [ ] Function to expand the memory table.
- [ ] Add support for locales.
- [ ] Add a daemon.
- [ ] Maybe add cryptography and access management.
