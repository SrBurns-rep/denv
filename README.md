<img align="left" style="width:260px" src="resources/denv-logo.svg" width="260px">

**Denv creates a shared memory instance for storing variables for general usage.** 

It enables an easy way to communicate between shell processes, this shared memory instance is globally shared among any call of denv, denv blocks it's call until it's command is complete.

:warning: :construction: **Warning:** Denv is still under development, don't rely on it for anything critical.

<br>

---

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

## Logo
 <p xmlns:cc="http://creativecommons.org/ns#" xmlns:dct="http://purl.org/dc/terms/"><a property="dct:title" rel="cc:attributionURL" href="https://github.com/SrBurns-rep/denv/blob/main/resources/denv-logo.svg">Denv Logo</a> by <a rel="cc:attributionURL dct:creator" property="cc:attributionName" href="https://github.com/SrBurns-rep">Caio Burns Lessa</a> is licensed under <a href="https://creativecommons.org/licenses/by-sa/4.0/?ref=chooser-v1" target="_blank" rel="license noopener noreferrer" style="display:inline-block;">CC BY-SA 4.0<img style="height:22px!important;margin-left:3px;vertical-align:text-bottom;" src="https://mirrors.creativecommons.org/presskit/icons/cc.svg?ref=chooser-v1" alt=""><img style="height:22px!important;margin-left:3px;vertical-align:text-bottom;" src="https://mirrors.creativecommons.org/presskit/icons/by.svg?ref=chooser-v1" alt=""><img style="height:22px!important;margin-left:3px;vertical-align:text-bottom;" src="https://mirrors.creativecommons.org/presskit/icons/sa.svg?ref=chooser-v1" alt=""></a></p> 
