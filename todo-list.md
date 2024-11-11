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
- [ ] Add a daemon.
- [ ] Tidy up for release.
- [ ] Make a package for .deb, .rpm, archlinux and freebsd.

## Need help for this part
- [ ] Create automated tests.
- [ ] Benchmark.
- [ ] Fix bugs.

## To-do for V2.0
- [ ] Read lock for each variable, single write lock for the entire thing.
- [ ] Implement a cofiguration file in toml.
- [ ] Redesign denv to be expandable.
- [ ] Function to expand the memory table.
- [ ] Add support for locales.
- [ ] Maybe add cryptography and access management.
