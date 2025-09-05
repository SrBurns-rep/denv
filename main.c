/*
        DENV MAIN
*/

#include "denv.h"
#include <ctype.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>
#include <stdarg.h>

#define DENV_BIND_PATH ("/.local/share/denv")
#define DENV_SAVE_PATH ("/save.denv")
#define ARRLEN(X) (sizeof(X) / sizeof((X)[0]))
#define BUFF_SIZE (1024)
#define STDIN_VAR_BUFFER_LENGTH (4096)
#define PATH_BUFFER_LENGHT (4096)
#define POLLING_MILLISECONDS (1000000)
#define POLLING_INTERVAL (100 * POLLING_MILLISECONDS)

char *strncat_s(char *restrict dst, const char *src, size_t size) {
    size_t len = size - strlen(dst) - 1;
    return strncat(dst, src, len);
}

int print_err(const char * fmt, ...) {

    const int fmt_len = 256;
    char fmt_ap[fmt_len];

    strncpy(fmt_ap, "\x1b[0;1;31m[ERROR] ", fmt_len);
    strncat_s(fmt_ap, fmt, fmt_len);
    strncat_s(fmt_ap, "\x1b[0m", fmt_len);

    va_list ap;
    va_start(ap, fmt);
    va_end(ap);

    return vfprintf(stderr, fmt_ap, ap);
}

typedef enum {
    UNDEFINED,
    VERSION,
    HELP,
    SET,
    GET,
    REMOVE,
    DROP,
    LIST,
    STATS,
    CLEANUP,
    SAVE,
    LOAD,
    AWAIT,
    EXEC,
    CLONE,
    EXPORT,
    DAEMON,
} command_states;

typedef enum {
    CSV = 1,
    XML,
    JSON,
    PRETTY // will be the default when implemented
} print_options;

static const struct {
    char *cmd;
    char *options;
    int state;
} commands[] = {
    {"-h", NULL, HELP},       {"-v", NULL, VERSION},
    {"--help", NULL, HELP},   {"--version", NULL, VERSION},
    {"help", NULL, HELP},     {"version", NULL, VERSION},
    {"set", "eb:", SET},      {"get", "b:", GET},
    {"rm", "b:", REMOVE},  // *
    {"drop", "fb:", DROP}, // *
    {"ls", "xb:", LIST},   // *
    {"stats", "b:", STATS},   {"cleanup", "b:", CLEANUP},
    {"save", "b:", SAVE},     {"load", "fb:", LOAD},
    {"await", "b:", AWAIT},   {"exec", "b:", EXEC},
    {"clone", "b:", CLONE},   {"export", "b:", EXPORT},
    {"daemon", "b:", DAEMON},
};

void print_help(void) {
    printf(
        "Usage: denv [command] [options] <key> <value>\n"
        "\t-h / --help / help             Display this information.\n"
        "\t-v / --version / version       Display current version.\n"
        "\tset [-b/-e] <key> <value>      Sets the key with the value "
        "provided.\n"
        "\tget [-b] <key>                 Gets the value stored in the key.\n"
        "\trm [-b] <key>                  Removes the key and value pair.\n"
        "\tls [-x/-b]                     Lists all keys.\n"
        "\tdrop [-f/-b]                   Deletes everything in the attached "
        "shmem.\n"
        "\tstats [-b] / --<format>        Print stats.\n"
        "\tcleanup [-b]                   Clear deleted variables from "
        "memory.\n"
        "\tsave [-b] <filename>           Save denv table to a file.\n"
        "\tload [-f/-b] <filename>        Load from a denv save file.\n"
        "\tawait [-b] <key>               Wait for change in the value of a "
        "key.\n"
        "\texec [-b] <program> <args>     Executes a program with denv "
        "environment variables.\n"
        "\tclone [-b]                     Clone parent process environment.\n"
        "\texport [-b]                    Export environment variables to a "
        "file.\n"
        "\tdaemon [-b]                    Run a daemon to automatically save "
        "and load.\n"
        "\n"
        "option -b:        Shared memory bind path.\n"
        "option -e:        Set variable as an envrionment variable.\n"
        "option -f:        Force yes to operations that prompts the user.\n"
        "option -x:        Suppress environment variable indicator on listing.\n"
        "\n"
        "stats --<format>:\n"
        "\t--csv (default)\n");
}

int make_bind_path(char *path_buf) {
    struct stat st = {0};

    if (stat(path_buf, &st) == -1) {
        int err = mkdir(path_buf, 0766);
        if (err) {
            char *str_err = strerror(err);
            print_err(
                "Could not create bind path for denv: %s\n"
                "Suggestion, create path using: mkdir -p $HOME/.local/share/denv\n",
                str_err
            );
            return -1;
        }
    }

    return 0;
}

char *get_bind_path(char *path_buf, size_t buf_len) {

    char *home_path = getenv("HOME");
    if (home_path == NULL) {
        print_err("HOME environment variable is not set.\n");
        return NULL;
    }

    if (buf_len <= (strlen(home_path) + strlen(DENV_BIND_PATH))) {
        print_err("Path buffer is too short.\n");
        return NULL;
    }

    memcpy(path_buf, home_path, strlen(home_path) + 1);

    char *denv_path = DENV_BIND_PATH;

    strncat_s(path_buf, denv_path, BUFF_SIZE);

    make_bind_path(path_buf);

    return path_buf;
}

bool check_path(const char *path) {
    struct stat st = {0};
    return (stat(path, &st) != -1);
}

bool check_bind_path(const char *path) {
    struct stat st = {0};

    if (stat(path, &st) == -1) {
        print_err("Bind path not found.\n");
        return false;
    }

    return true;
}

char g_path_buffer[PATH_BUFFER_LENGHT] = {0};

char *load_path() {
    char *file_name = get_bind_path(g_path_buffer, PATH_BUFFER_LENGHT);
    return file_name;
}

bool is_env_char(char e) {
    if (e >= '0' && e <= '9')
        return true;
    if (e >= 'a' && e <= 'z')
        return true;
    if (e >= 'A' && e <= 'Z')
        return true;
    if (e == '_')
        return true;
    return false;
}

bool is_env_var_name(char *name) {
    for (size_t i = 0; i < strlen(name); i++) {
        if (i == 0) {
            if (name[i] >= '0' && name[i] <= '9')
                return false;
        }
        if (is_env_char(name[i]) == false)
            return false;
    }
    return true;
}

Table *init() {

    char *file_name = load_path();
    if (file_name == NULL)
        return NULL;

    // attach memory block
    Table *table = denv_shmem_attach(file_name, sizeof(Table));

    if (table == NULL) {
        print_err("Failed to create a shared memory environment.\n");
        return NULL;
    }

    if ((table->flags & TABLE_IS_INITIALIZED) == 0) {
        int sem_ret = sem_init(&table->denv_sem, 1, 1);
        if (sem_ret < 0) {
            perror("sem_init");
            return NULL;
        }
        denv_table_init(table);
    }

    return table;
}

void deinit(Table *table) {
    assert(table);

    if (denv_shmem_detach(table) == false) {
        print_err("Failed to detach shared memory.\n");
    }

    table = NULL;
}

Table *init_on_path(char *path) {
    struct stat st = {0};

    if (stat(path, &st) == -1) {
        print_err("Bind path \"%s\" doesn't exist.\n", path);
        return NULL;
    }

    Table *table = denv_shmem_attach(path, sizeof(Table));

    if (table == NULL) {
        print_err("Failed to create a shared memory environment.\n");
        return NULL;
    }

    if ((table->flags & TABLE_IS_INITIALIZED) == 0) {
        int sem_ret = sem_init(&table->denv_sem, 1, 1);
        if (sem_ret < 0) {
            perror("sem_init");
            return NULL;
        }
        denv_table_init(table);
    }

    return table;
}

Table *init_only_table(char *file_name) {
    Table *table = denv_shmem_attach(file_name, sizeof(Table));

    if (table == NULL) {
        print_err("Failed to create a shared memory environment.\n");
        return NULL;
    }
    return table;
}

typedef struct {
    char *bind_path;
    char *name;
    char *value;
    char *save_path;
    char *exec_command;
    char **exec_command_args;
    int print_option;
    int error;
    command_states state;
    bool is_env;
    bool force;
    bool suppress;
    bool is_stdin;
    bool is_stdout;
} CmdLine;

typedef enum {
    PARSE_ERROR_NONE,
    PARSE_ERROR_UNKNOWN_OPTION,
    PARSE_ERROR_MISSING_NAME,
    PARSE_ERROR_MISSING_PATH,
    PARSE_ERROR_MISSING_PATH_OR_VALUE,
    PARSE_ERROR_MISSING_PATH_OR_MANY_ARGS,
    PARSE_ERROR_MISSING_OPTION_OR_EXTRA_ARGUMENT,
    PARSE_ERROR_INVALID_NAME,
    PARSE_ERROR_NOT_ENOUGH_ARGUMENTS,
    PARSE_ERROR_TOO_MANY_ARGUMENTS,
    PARSE_ERROR_TOO_MANY_ARGUMENTS_OR_MISSING_NAME,
    PARSE_ERROR_UNIMPLEMENTED
} CommandParseErrors;

CmdLine parse_commands(int argc, char **argv) {
    int state = UNDEFINED;

    CmdLine cmd = {0};

    for (size_t i = 0; i < ARRLEN(commands); i++) {
        if (argv[1][0] == commands[i].cmd[0]) {
            if (strcmp(argv[1], commands[i].cmd) == 0) {
                state = commands[i].state;
            }
        }
    }

    cmd.state = state;

    switch(state) {
        case SET:
            if (argc == 4) {
                // denv set "name" "value"                  4
                cmd.name = argv[2];

                if (strcmp(argv[3], "-") == 0) {
                    cmd.is_stdin = true;
                } else {
                    cmd.value = argv[3];
                }
            } else if (argc == 5) {
                // denv set -e "name" "value"               5
                if (strcmp(argv[2], "-e") != 0) {
                    cmd.error = PARSE_ERROR_UNKNOWN_OPTION;
                }
                if (strcmp(argv[2], "-b") == 0) {
                    cmd.error = PARSE_ERROR_MISSING_PATH_OR_VALUE;
                    break;
                }
                if (is_env_var_name(argv[3]) == false) {
                    cmd.name = argv[3];
                    cmd.error = PARSE_ERROR_INVALID_NAME;
                }
                if (strcmp(argv[4], "-") == 0) {
                    cmd.is_stdin = true;
                }
                cmd.is_env = true;
                cmd.name = argv[3];
                cmd.value = argv[4];
            } else if (argc == 6) {
                // denv set -b some/path "name" "value"     6
                // denv set -be some/path "name" "value"    6
                // denv set -eb some/path "name" "value"    6
                if ((strcmp(argv[2], "-eb") && strcmp(argv[2], "-be")) == 0) {
                    cmd.is_env = true;
                } else if (strcmp(argv[2], "-b") != 0) {
                    cmd.error = PARSE_ERROR_UNKNOWN_OPTION;
                    if (strcmp(argv[2], "-e") == 0) {
                        cmd.error = PARSE_ERROR_MISSING_OPTION_OR_EXTRA_ARGUMENT;
                    }
                    break;
                }
                if (is_env_var_name(argv[4]) == false) {
                    cmd.name = argv[4];
                    cmd.error = PARSE_ERROR_INVALID_NAME;
                    break;
                }
                if (strcmp(argv[5], "-") == 0) {
                    cmd.is_stdin = true;
                } else {
                    cmd.value = argv[5];
                }
                cmd.name = argv[4];
                cmd.bind_path = argv[3];
            } else if (argc < 4) {
                cmd.error = PARSE_ERROR_NOT_ENOUGH_ARGUMENTS;
            } else {
                cmd.error = PARSE_ERROR_TOO_MANY_ARGUMENTS;
            }
            break;
        case GET:
            if (argc == 3) {
                // denv get var_name				3
                cmd.name = argv[2];
            } else if (argc < 3) {
                cmd.error = PARSE_ERROR_MISSING_NAME;
                break;
            } else if (argc == 5) {
                // denv get -b bind/path var_name	5
                if (strcmp(argv[2], "-b") != 0) {
                    cmd.error = PARSE_ERROR_UNKNOWN_OPTION;
                    break;
                }
                cmd.name = argv[4];
                cmd.bind_path = argv[3];
            } else if (argc == 4) {
                cmd.error = PARSE_ERROR_TOO_MANY_ARGUMENTS_OR_MISSING_NAME;
                break;
            } else if (argc > 5) {
                cmd.error = PARSE_ERROR_TOO_MANY_ARGUMENTS;
                break;
            }
            break;
        case REMOVE:
            if (argc < 3) {
                cmd.error = PARSE_ERROR_MISSING_NAME;
                break;
            } else if (argc == 3) {
                // denv rm var_name					3
                cmd.name = argv[2];
            } else if (argc == 4) {
                cmd.error = PARSE_ERROR_TOO_MANY_ARGUMENTS_OR_MISSING_NAME;
                break;
            } else if (argc == 5) {
                // denv rm -b bind/path var_name	5
                if (strcmp(argv[2], "-b") != 0) {
                    cmd.error = PARSE_ERROR_UNKNOWN_OPTION;
                    break;
                }
                cmd.name = argv[4];
                cmd.bind_path = argv[3];
            } else if (argc > 5) {
                cmd.error = PARSE_ERROR_TOO_MANY_ARGUMENTS;
                break;
            }
            break;
        case DROP:
            if(argc == 3) {
                if (strcmp(argv[2], "-f") != 0) {
                    cmd.error = PARSE_ERROR_UNKNOWN_OPTION;
                    break;
                }
                cmd.force = true;
            } else if (argc == 4) {
                // denv drop -b bind/path		4
                // denv drop -bf bind/path 		4
                if ((strcmp(argv[2], "-bf") && strcmp(argv[2], "-fb")) == 0) {
                    cmd.force = true;
                } else if (strcmp(argv[2], "-b") != 0) {
                    cmd.error = PARSE_ERROR_UNKNOWN_OPTION;
                    break;
                }
                cmd.bind_path = argv[3];
            } else if (argc > 4) {
                cmd.error = PARSE_ERROR_TOO_MANY_ARGUMENTS;
                break;
            }
            break;
        case LIST:
            if(argc == 3) {
                // denv ls -x 				3
                if (strcmp(argv[2], "-x") != 0) {
                    cmd.error = PARSE_ERROR_UNKNOWN_OPTION;
                    break;
                }
                cmd.suppress = true;
            } else if (argc == 4) {
                // denv ls -b bind/path 	4
                // denv ls -xb bind/path 	4
                // denv ls -bx bind/path 	4
                if (strcmp(argv[2], "-b") != 0) {
                    if ((strcmp(argv[2], "-xb") && strcmp(argv[2], "-bx")) == 0) {
                        cmd.suppress = true;
                    } else {
                        cmd.error = PARSE_ERROR_UNKNOWN_OPTION;
                        break;
                    }
                }
                cmd.bind_path = argv[3];
            } else if (argc > 4) {
                cmd.error = PARSE_ERROR_TOO_MANY_ARGUMENTS;
                break;
            }
            break;
        case STATS:
        
            cmd.print_option = CSV;

            if (argc == 3) {
                // denv stats --csv/xml/json/pretty                 3
                if (strcmp(argv[2], "--xml") == 0) {
                    cmd.print_option = XML;
                    cmd.error = PARSE_ERROR_UNIMPLEMENTED;
                    break;
                }
                if (strcmp(argv[2], "--json") == 0) {
                    cmd.print_option = JSON;
                    cmd.error = PARSE_ERROR_UNIMPLEMENTED;
                    break;
                }
                if (strcmp(argv[2], "--pretty") == 0) {
                    cmd.print_option = PRETTY;
                    cmd.error = PARSE_ERROR_UNIMPLEMENTED;
                    break;
                }
                if (cmd.print_option == UNDEFINED) {
                    cmd.error = PARSE_ERROR_UNKNOWN_OPTION;
                    break;
                }
            } else if (argc == 4) {
                // denv stats -b bind/path                          4
                if (strcmp(argv[2], "-b") != 0) {
                    cmd.error = PARSE_ERROR_UNKNOWN_OPTION;
                    break;
                }
                cmd.bind_path = argv[3];
            } else if (argc == 5) {
                // denv stats --csv/xml/json/pretty -b bind/path    5
                if (strcmp(argv[3], "-b") != 0) {
                    cmd.error = PARSE_ERROR_UNKNOWN_OPTION;
                    break;
                }

                if (strcmp(argv[2], "--xml") == 0) {
                    cmd.print_option = XML;
                    cmd.error = PARSE_ERROR_UNIMPLEMENTED;
                    break;
                }
                if (strcmp(argv[2], "--json") == 0) {
                    cmd.print_option = JSON;
                    cmd.error = PARSE_ERROR_UNIMPLEMENTED;
                    break;
                }
                if (strcmp(argv[2], "--pretty") == 0) {
                    cmd.print_option = PRETTY;
                    cmd.error = PARSE_ERROR_UNIMPLEMENTED;
                    break;
                }
                if (cmd.print_option == UNDEFINED) {
                    cmd.error = PARSE_ERROR_UNKNOWN_OPTION;
                    break;
                }
                cmd.bind_path = argv[4];
            }
            break;
        case CLEANUP:
            if (argc == 3) {
                if(strcmp(argv[2], "-b") != 0) {
                    cmd.error = PARSE_ERROR_UNKNOWN_OPTION;
                    break;
                }
                cmd.error = PARSE_ERROR_MISSING_PATH;
                break;                
            } else if(argc == 4) {
                // denv cleanup -b bind/path
                if(strcmp(argv[2], "-b") != 0) {
                    cmd.error = PARSE_ERROR_UNKNOWN_OPTION;
                    break;
                }
            } else if (argc > 4) {
                cmd.error = PARSE_ERROR_TOO_MANY_ARGUMENTS;
                break;
            }
            break;
        case SAVE:
            if(argc == 2) {
                cmd.error = PARSE_ERROR_MISSING_PATH;
            } else if (argc == 3) {
                // denv save path/to/save_file					3
                cmd.save_path = argv[2];
            } else if (argc == 4) {
                cmd.error = PARSE_ERROR_MISSING_PATH_OR_MANY_ARGS;
            } else if (argc == 5) {
                // denv save -b bind/path path/to/save_file		5
                if(strcmp(argv[2], "-b") != 0) {
                    cmd.error = PARSE_ERROR_UNKNOWN_OPTION;
                    break;
                }
                cmd.bind_path = argv[3];
                cmd.save_path = argv[4];
            } else if (argc > 5) {
                cmd.error = PARSE_ERROR_TOO_MANY_ARGUMENTS;
            }
            break;
        case LOAD:
            if(argc == 2) {
                cmd.error = PARSE_ERROR_MISSING_PATH;
            } else if (argc == 3) {
                // denv load path/so/save_file					3
                cmd.save_path = argv[2];
            } else if (argc == 4) {
                // denv load -f path/to/save_file				4
                if (strcmp(argv[2], "-f") != 0) {
                    cmd.error = PARSE_ERROR_UNKNOWN_OPTION;
                    break;
                }
                cmd.force = true;
                cmd.save_path = argv[3];
            } else if (argc == 5) {
                // denv load -b/bf/fb bind/path path/to/save_file		5
                if (strcmp(argv[2], "-b") != 0) {
                    if (strcmp(argv[2], "-bf") && strcmp(argv[2], "-fb") != 0) {
                        cmd.error = PARSE_ERROR_UNKNOWN_OPTION;
                        break;
                    } else {
                        cmd.force = true;
                    }
                }
                cmd.bind_path = argv[3];
                cmd.save_path = argv[4];
            } else if (argc > 5) {
                cmd.error = PARSE_ERROR_TOO_MANY_ARGUMENTS;
            }
            break;
        case AWAIT:
            if (argc == 2) {
                cmd.error = PARSE_ERROR_MISSING_NAME;
            } else if (argc == 3) {
                // denv await variable
                cmd.name = argv[2];
            } else if (argc == 4) {
                cmd.error = PARSE_ERROR_MISSING_PATH_OR_MANY_ARGS;
            } else if (argc == 5) {
                // denv await -b some/path variable
                if (strcmp(argv[2], "-b") != 0) {
                    cmd.error = PARSE_ERROR_UNKNOWN_OPTION;
                    break;
                }
                cmd.bind_path = argv[3];
                cmd.name = argv[4];
            } else if (argc > 5) {
                cmd.error = PARSE_ERROR_TOO_MANY_ARGUMENTS;
            }
            break;
        case EXEC:
            // denv exec printenv arg1 arg2 arg3 arg4 ...
            // denv exec -b bind/path printenv arg1 arg2 arg3 arg4 ...
            if (argc == 2) {
                cmd.error = PARSE_ERROR_MISSING_PATH;
            } else {
                if (strcmp(argv[2], "-b") == 0) {
                    if (argc == 3 || argc == 4) {
                        cmd.error = PARSE_ERROR_MISSING_PATH;
                        break;
                    }
                    cmd.bind_path = argv[3];
                    cmd.exec_command = argv[4];
                    cmd.exec_command_args = &argv[5];
                } else {
                    cmd.exec_command = argv[2];
                    if(argc > 3) {
                        cmd.exec_command_args = &argv[3];
                    }
                }
            }
            break;
        case CLONE:
            // denv clone
            // denv clone -b bind/path
            if (argc == 3) {
                cmd.error = PARSE_ERROR_MISSING_PATH_OR_MANY_ARGS;
            } else if (argc == 4) {
                if (strcmp(argv[2], "-b") != 0) {
                    cmd.error = PARSE_ERROR_UNKNOWN_OPTION;
                    break;
                }
                cmd.bind_path = argv[3];
            } else if (argc > 4) {
                cmd.error = PARSE_ERROR_TOO_MANY_ARGUMENTS;
            }
            break;
        case EXPORT:
            if (argc == 2) {
                cmd.error = PARSE_ERROR_MISSING_PATH;
            } else if (argc == 3) {
                // denv export -						3
                // denv export file/path				3
                if (argv[2][0] == '-') {
                    if (argv[2][1] == '\0') {
                        cmd.is_stdout = true;
                    } else {
                        cmd.error = PARSE_ERROR_UNKNOWN_OPTION;
                        break;
                    }
                } else {
                    cmd.save_path = argv[2];
                }
            } else if (argc == 4) {
                cmd.error = PARSE_ERROR_MISSING_PATH_OR_MANY_ARGS;
            } else if (argc == 5) {
                // denv export -b bind/path -			5
                // denv export -b bind/path file/path   5
                if (strcmp(argv[2], "-b") != 0) {
                    cmd.error = PARSE_ERROR_UNKNOWN_OPTION;
                    break;
                }
                if (argv[4][0] == '-') {
                    if (argv[4][1] == '\0') {
                        cmd.is_stdout = true;
                    } else {
                        cmd.error = PARSE_ERROR_UNKNOWN_OPTION;
                        break;
                    }
                } else {
                    cmd.save_path = argv[4];
                }
                cmd.bind_path = argv[3];
            } else if (argc > 5) {
                cmd.error = PARSE_ERROR_TOO_MANY_ARGUMENTS;
            }
            break;
        case DAEMON:
            if (argc == 3 || argc == 4) {
                cmd.error = PARSE_ERROR_NOT_ENOUGH_ARGUMENTS;
                break;
            } else if (argc == 5) {
                // denv daemon -b bind/path save-file   5
                if (strcmp(argv[2], "-b") != 0) {
                    cmd.error = PARSE_ERROR_UNKNOWN_OPTION;
                    break;
                }
                cmd.bind_path = argv[3];
                cmd.save_path = argv[4];
            } else if (argc > 5) {
                cmd.error = PARSE_ERROR_TOO_MANY_ARGUMENTS;
            }
            break;
        default:    /* UNDEFINED, HELP or VERSION */
            break;
    }

    return cmd;
}

int main(int argc, char **argv, char **envp) {

    if (argc < 2) {
        print_err("Not enough arguments.\n");
        print_help();
        return -1;
    }

    CmdLine cmd = parse_commands(argc, argv);

    if(cmd.error) {
        fflush(stderr);
        fflush(stdout);

        switch(cmd.error) {
            case PARSE_ERROR_UNKNOWN_OPTION:
                    print_err("Unknown option \"%s\".\n", argv[2]);
                break;
            case PARSE_ERROR_MISSING_NAME:
                    print_err("Missing variable name.\n");
                break;
            case PARSE_ERROR_MISSING_PATH:
                    print_err("Missing path.\n");
                break;
            case PARSE_ERROR_MISSING_PATH_OR_VALUE:
                    print_err("Missing path or value.\n");
                break;
            case PARSE_ERROR_MISSING_PATH_OR_MANY_ARGS:
                    print_err("Missing path or too many arguments.\n");
                break;
            case PARSE_ERROR_MISSING_OPTION_OR_EXTRA_ARGUMENT:
                    print_err("Missing option or too many arguments.\n");
                break;
            case PARSE_ERROR_INVALID_NAME:
                    print_err("Invalid environment variable name \"%s\".\n", cmd.name);
                break;
            case PARSE_ERROR_NOT_ENOUGH_ARGUMENTS:
                    print_err("Not enough arguments.\n");
                break;
            case PARSE_ERROR_TOO_MANY_ARGUMENTS:
                    print_err("Too many arguments.\n");
                break;
            case PARSE_ERROR_TOO_MANY_ARGUMENTS_OR_MISSING_NAME:
                    print_err("Too many arguments or missing name.\n");
                break;
            case PARSE_ERROR_UNIMPLEMENTED:
                    print_err("Feature not implemented yet.\n");
                break;
        }

        return 1;
    }

    #ifdef DEBUG_ON
        // Print cmd line:

        const char* t[] = {"false", "true"};
        
        fprintf(stderr,
            "\x1b[32mcmd = {\n"
            "   .bind_path=\"%s\",\n"
            "   .name=\"%s\",\n"
            "   .value=\"%s\",\n"
            "   .save_path=\"%s\",\n"
            "   .exec_command=\"%s\",\n"
            "   .exec_command_args=\"%s\",\n"
            "   .print_option=%i,\n"
            "   .error=%i,\n"
            "   .state=%i,\n"
            "   .is_env=%s;\n"
            "   .force=%s;\n"
            "   .suppress=%s,\n"
            "   .is_stdin=%s,\n"
            "   .is_stdout=%s\n"
            "};\x1b[0m\n",
            cmd.bind_path ? cmd.bind_path : "(nil)",
            cmd.name ? cmd.name : "(nil)",
            cmd.value ? cmd.value : "(nil)",
            cmd.save_path ? cmd.save_path : "(nil)",
            cmd.exec_command ? cmd.exec_command : "(nil)",
            cmd.exec_command_args ? cmd.exec_command_args[0] : "(nil)",
            cmd.print_option,
            cmd.error,
            cmd.state,
            t[cmd.is_env],
            t[cmd.force],
            t[cmd.suppress],
            t[cmd.is_stdin],
            t[cmd.is_stdout]
        );

    #endif

    switch(cmd.state) {
        case UNDEFINED:
            print_err("Unknown command \"%s\".\n", argv[1]);
            return -1;
        case VERSION:
            denv_print_version();
            return 0;
        case HELP:
            print_help();
            return 0;
        default:
    }

    // Initialize table
    Table *table = NULL;
    char *path = NULL;

    if(cmd.bind_path) {
        path = cmd.bind_path;
    } else {
        path = load_path();
    }

    table = init_on_path(path);
    if(!table) return -1;    

    int error = 0;
    char input_buffer[BUFF_SIZE] = {0};
    char *name = cmd.name;
    char *value = cmd.value;


    switch(cmd.state) {
        case SET: {

            size_t flags = 0;
            if (cmd.is_env) {
                flags = ELEMENT_IS_ENV;
            }
            
            if (cmd.is_stdin) {
                char *buffer = calloc(BUFF_SIZE, sizeof(char));
                if (!buffer) {
                    print_err("Couldn't allocate more memory.\n");
                    error = -1;
                    break;
                }
                size_t len = 0, acc = 0;
                do {
                    if(acc) {
                        buffer = realloc(buffer, acc + BUFF_SIZE);
                        if (!buffer) {
                            print_err("Couldn't reallocate more memory.\n");
                            error = -1;
                            break;
                        }               
                    }
                    len = fread(buffer + acc, sizeof(char), BUFF_SIZE, stdin);
                    if(ferror(stdin) != 0) {
                        print_err("Couldn't read from stdin.\n");
                        free(buffer);
                        error = -1;
                        break;
                    }
                    acc += len;
                } while(len == BUFF_SIZE);

                denv_table_set_value(table, name, buffer, flags);

                free(buffer);
            } else {
                denv_table_set_value(table, name, value, flags);
            }            
        } break;
        
        case GET:
            value = denv_table_get_value(table, name);

            if (value) {
                printf("%s\n", value);
            }
            break;
        case REMOVE:

            denv_table_delete_value(table, name);
        
            break;
            
        case DROP:

            // Ask if you are sure
            if (cmd.force == false) {
                printf("Are you sure you want to destroy the shared memory "
                       "environment? [N/y]\n");
                fgets(input_buffer, BUFF_SIZE, stdin);
                if (input_buffer[0] != 'y' && input_buffer[0] != 'Y') break;
            }    

            if (denv_shmem_destroy(path) == false) {
                print_err("Failed to destroy the shared memory environment.\n");
                error = -1;
                break;
            }
        
            break;

        case LIST:
                denv_table_list_values(table, !cmd.suppress);
            break;
            
        case STATS: {
                switch(cmd.print_option) {
                    case CSV:
                        denv_print_stats_csv(table);
                        break;
                 /* case XML:           UNIMPLEMENTED
                        break;
                    case JSON:          UNIMPLEMENTED
                        break;
                    case PRETTY:        UNIMPLEMENTED
                        break;
                    default:
                        print_err("Unknown option.\n");
                        error = -1; */
                }
            } break;
            
        case CLEANUP:
            denv_clear_freed(table);
            break;
            
        case SAVE:
            denv_save_to_file(table, cmd.save_path);
            break;

        case LOAD:

            // Ask if you are sure
            if (cmd.force == false) {
            printf("This will overwrite current variables, Are you sure you "
                   "want to load \"%s\" to denv? [N/y]\n",
                   cmd.save_path);
                   
                fgets(input_buffer, BUFF_SIZE, stdin);
                if (input_buffer[0] != 'y' && input_buffer[0] != 'Y') break;
            }

            table = denv_load_from_file(table, cmd.save_path);
            if (table == NULL) {
               print_err("Failed to load from file.\n");
               error = -1; 
            }
            break;
            
        case AWAIT:
            denv_await_element(table, name, POLLING_INTERVAL);
            break;

        case EXEC: {
        
                char *aux = NULL;
                if (cmd.exec_command_args == NULL) {
                    cmd.exec_command_args = &aux;
                }
            
                if (denv_exec(table, cmd.exec_command, cmd.exec_command_args) != 0) {
                    print_err("Error while trying to execute command \"%s\".\n", cmd.exec_command);
                    error = -1;
                }
            } break;
            
        case CLONE:
            denv_clone_env(table, envp);
            break;

        case EXPORT:

            FILE *save_file = fopen(cmd.save_path, "w");
            if (save_file == NULL) {
                print_err("Failed to open \"%s\".\n", cmd.save_path);
                error = -1;
                break;
            }

            denv_make_env_save_file(table, save_file);

            error = fclose(save_file);

            break;

        case DAEMON: {
            char *save_file_path = strncat_s(path, DENV_SAVE_PATH, PATH_BUFFER_LENGHT);
            if (save_file_path == NULL) {
                print_err("Failed to strcat, path too big.\n");
                error = -1;
                break;
            }
            if (check_path(save_file_path)) {
                table = denv_load_from_file(table, save_file_path);
                if(table == NULL) {
                    error = -1;
                    break;
                }
            }
            // hang-up until SIGTERM
            sigset_t set;
            int sig;

            sigemptyset(&set);
            sigaddset(&set, SIGTERM);
            sigaddset(&set, SIGHUP);
            sigaddset(&set, SIGINT);

            sigprocmask(SIG_BLOCK, &set, NULL);

            int pid = getpid();

            printf("PID: %i Waiting until SIGTERM...\n", pid);

            openlog("DENV", LOG_PID | LOG_CONS, LOG_USER);

            if (sigwait(&set, &sig) == 0) {
                // Check if file exists, move to .old and then save new file
                if (check_path(save_file_path)) {
                    char new_path[PATH_BUFFER_LENGHT] = {0};
                    strncpy(new_path, save_file_path, PATH_BUFFER_LENGHT);
                    strncat_s(new_path, ".old", PATH_BUFFER_LENGHT);
                    int ren_err = rename(save_file_path, new_path);
                    if (ren_err == -1) {
                        syslog(LOG_ERR,
                               "Couldn't move current save to old save.");
                        closelog();
                        error = -1;
                        break;
                    }
                }

                if (denv_save_to_file(table, save_file_path) != 0) {
                    error = -1;
                    break;   
                }
            } else {
                syslog(LOG_ERR, "Daemon was interrupted abruptly, couldn't "
                                "save it's state.");
                error = -1;
                closelog();
                break;    
            }
            closelog();


        } break;
        default:

    }

    if(table != NULL) {
        deinit(table);
    }
    
    return error;
}
