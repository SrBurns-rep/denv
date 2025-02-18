<img align="left" style="width:260px" src="resources/denv-logo.svg" width="260px">

**Denv - A Shared Memory Environment for general usage.** 

Denv creates a shared memory instance for storing variables for general usage. It enables an easy way to communicate between shell processes. This shared memory instance is globally shared among any call to denv, and denv blocks its call until the command is complete.

:warning: :construction: **Warning:** Denv is still under development, don't rely on it for anything critical.

<br>

---

## Features
* Store and manage environment variables in a shared environment.
* Communicate between shell processes using a global shared memory instance.
* Execute programs with custom environment variables.

## Dependencies
* `gcc`
* `zlib`
* `A POSIX-compatible shell (bash, zsh, fish, etc.)`

## Installation
```shell
$ git clone https://github.com/SrBurns-rep/denv.git
$ cd denv
$ ./build.sh
$ sudo mv denv /usr/local/bin
```

## Supported systems
**Denv** is designed for Linux/BSD systems with shared memory support. Tested systems include:
* Arch Linux
* Void Linux
* Debian

## Usage
Set a variable
```shell
$ denv set "variable_name" "value"
```
Get a variable value
```shell
$ denv get "variable_name"
```
Remove a variable
```shell
$ denv rm "variable_name"
```
List variables
```shell
$ denv ls
```
Drop shared memory (Deletes everything)
```shell
$ denv drop
```
Denv table cleanup (has to be done manually to clear removed variables)
```shell
$ denv cleanup
```
Save denv table to a file
```shell
$ denv save file-name
```
Load denv from a file (all current variables are going to be overwritten!)
```shell
$ denv load file-name
```
Print stats
```shell
$ denv stats
```
Wait until variable changes
```shell
$ denv await variable
```
Execute programs with environment variables stored in denv
```shell
$ denv exec program
```
Run a daemon to save denv at shutdown (if you have a file named `save.denv` at  `$HOME/.local/share/denv` it will be loaded!)
```shell
$ denv daemon
```
## Logo
 <p xmlns:cc="http://creativecommons.org/ns#" xmlns:dct="http://purl.org/dc/terms/"><a property="dct:title" rel="cc:attributionURL" href="https://github.com/SrBurns-rep/denv/blob/main/resources/denv-logo.svg" target="_blank">Denv Logo</a> by <a rel="cc:attributionURL dct:creator" property="cc:attributionName" href="https://github.com/SrBurns-rep" target="_blank" >Caio Burns Lessa</a> is licensed under <a href="https://creativecommons.org/licenses/by-sa/4.0/" target="_blank" rel="license noopener noreferrer" style="display:inline-block;">CC BY-SA 4.0<img style="height:22px!important;margin-left:3px;vertical-align:text-bottom;" src="https://mirrors.creativecommons.org/presskit/icons/cc.svg?ref=chooser-v1" target="_blank" alt=""><img style="height:22px!important;margin-left:3px;vertical-align:text-bottom;" src="https://mirrors.creativecommons.org/presskit/icons/by.svg?ref=chooser-v1" alt=""><img style="height:22px!important;margin-left:3px;vertical-align:text-bottom;" src="https://mirrors.creativecommons.org/presskit/icons/sa.svg?ref=chooser-v1" alt=""></a></p> 

---

Cursed be the man that uses his knowledge for the evil.
