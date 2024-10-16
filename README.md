# denv (dynamic environment)
Denv creates a shared memory instance for storing variables for general usage, it enables an easy way to communicate between shell processes.

:warning: :construction: **Warning:** Denv is still under development, don't rely on it for anything critical.

## Dependencies
`gcc` `zlib` `a posix shell (bash, zsh, fish, etc...)`

## Usage
Set a variable
```
$ denv sed "variable_name" "value"
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
Load denv from a file
```
$ denv list file-name
```
Print stats
```
$ denv stats
```
