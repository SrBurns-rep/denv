# denv (dynamic environment)
Denv creates a shared memory instance for storing variables for general usage, it enables an easy way to communicate between shell processes.

This shared memory instance is globally shared among any call of denv, denv blocks it's call until it's command is complete.

:warning: :construction: **Warning:** Denv is still under development, don't rely on it for anything critical.

## Dependencies
`gcc` `zlib` `a posix shell (bash, zsh, fish, etc...)`

## Supported systems
Any linux/bsd with reasonable support for shared memory is prone to work, tested systems are:
* Arch Linux
* Ubuntu
* Debian
* FreeBSD

## Usage
Set a variable
```
$ denv set "variable_name" "value"
```
Get a variable value
```
$ denv get "variable_name"
```
Delete a variable
```
$ denv delete "variable_name"
```
List variables
```
$ denv list
```
Remove shared memory (Deletes everything)
```
$ denv remove
```
Denv table cleanup (has to be done manually)
```
$ denv cleanup
```
Save denv table to a file
```
$ denv save file-name
```
Load denv from a file (all current variables are going to be overwritten!)
```
$ denv load file-name
```
Print stats
```
$ denv stats
```
Wait until variable changes
```
$ denv await variable
```
