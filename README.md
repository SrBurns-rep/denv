# denv (Dyamic Environment)
Denv creates a shared memory instance for storing variables for general usage, it enables an easy way to communicate between bash processes.

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
