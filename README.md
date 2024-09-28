# denv (dynamic environment)
Denv creates a shared memory instance for storing variables for general usage, it enables an easy way to communicate between shell processes.

:warning: :construction: **Warning:** Denv is still under development, don't rely on it for anything critical.

## Usage
Set a variable
```
$ denv -s "variable_name" "value"
```
Get a variable value
```
$ denv -g "variable_name"
```
Delete a variable
```
$ denv -d "variable_name"
```
List variables
```
$ denv -l
```
Remove shared memory (Deletes everything)
```
$ denv -r
```
Denv table cleanup (has to be done manually)
```
$ denv -c
```
Save denv table to a file
```
$ denv -S file-name
```
Load denv from a file
```
$ denv -L file-name
```
Print stats
```
$ denv -t
```
