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

#define DENV_BIND_PATH ("/.local/share/denv")
#define DENV_SAVE_PATH ("/save.denv")
#define ARRLEN(X) (sizeof(X) / sizeof((X)[0]))
#define BUFF_SIZE (1024)
#define STDIN_VAR_BUFFER_LENGTH (4096)
#define PATH_BUFFER_LENGHT (4096)
#define POLLING_MILLISECONDS (1000000)
#define POLLING_INTERVAL (100 * POLLING_MILLISECONDS)

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
} commands_states;

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
        "\tawait [-b] <key>               Wait for a change in key value.\n"
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
        "option -x:        Supress environment variable indicator on listing.\n"
        "\n"
        "stats --<format>:\n"
        "\t--csv (default)\n");
}

int make_bind_path(char *path_buf) {
    struct stat st = {0};

    if (stat(path_buf, &st) == -1) {
        int err = mkdir(path_buf, 0766);
        if (err) {
            perror("mkdir");
            return -1;
        }
    }

    return 0;
}

char *get_bind_path(char *path_buf, size_t buf_len) {

    char *home_path = getenv("HOME");
    if (home_path == NULL) {
        fprintf(stderr, "HOME environment variable is not set.\n");
        return NULL;
    }

    if (buf_len <= (strlen(home_path) + strlen(DENV_BIND_PATH))) {
        fprintf(stderr, "Path buffer is too short.\n");
        return NULL;
    }

    memcpy(path_buf, home_path, strlen(home_path) + 1);

    char *denv_path = DENV_BIND_PATH;

    int len = BUFF_SIZE - strlen(home_path);

    strncat(path_buf, denv_path, len);

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
        fprintf(stderr, "Bind path not found.\n");
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
        fprintf(stderr, "Failed to create a shared memory environment.\n");
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
        fprintf(stderr, "Failed to detach shared memory.\n");
    }

    table = NULL;
}

Table *init_on_path(char *path) {
    struct stat st = {0};

    if (stat(path, &st) == -1) {
        fprintf(stderr, "Bind path \"%s\" doesn't exist.\n", path);
        return NULL;
    }

    Table *table = denv_shmem_attach(path, sizeof(Table));

    if (table == NULL) {
        fprintf(stderr, "Failed to create a shared memory environment.\n");
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
        fprintf(stderr, "Failed to create a shared memory environment.\n");
        return NULL;
    }
    return table;
}

int main(int argc, char **argv, char **envp) {

    if (argc < 2) {
        fprintf(stderr, "Not enough arguments.\n");
        print_help();
        return -1;
    }

    int state = UNDEFINED;

    for (size_t i = 0; i < ARRLEN(commands); i++) {
        if (argv[1][0] == commands[i].cmd[0]) {
            if (strcmp(argv[1], commands[i].cmd) == 0) {
                state = commands[i].state;
            }
        }
    }

    Table *table = NULL;

    char *file_name;

    char stdin_buffer[STDIN_VAR_BUFFER_LENGTH] = {0};
    char input_buffer[200] = {0};

    switch (state) {
    case UNDEFINED:
        fprintf(stderr, "Unknown command.\n");
        print_help();
        return -1;
        break;

    case HELP:
        print_help();
        return 0;
        break;

    case VERSION:
        denv_print_version();
        return 0;
        break;

    case SET: {

        // denv set -b some/path "name" "value"     6
        // denv set -be some/path "name" "value"    6
        // denv set -eb some/path "name" "value"    6
        // denv set -e "name" "value"               5
        // denv set "name" "value"                  4
        // 3 to 6 args

        Word flag = 0;

        if (argc == 4) {

            table = init();
            if (table == NULL)
                goto error;

            if (strcmp(argv[3], "-") == 0) {
                // read from stdin
                fread(stdin_buffer, sizeof(stdin_buffer) - 1, 1, stdin);
                denv_table_set_value(table, argv[2], stdin_buffer, 0);
            } else {
                denv_table_set_value(table, argv[2], argv[3], 0);
            }

        } else if (argc == 5) {

            table = init();
            if (table == NULL)
                goto error;

            // set -e "name" "value"
            if (strcmp(argv[2], "-e") != 0) {
                fprintf(stderr, "Unknwon option \"%s\".\n", argv[2]);
                goto error;
            }

            if (strcmp(argv[2], "-b") == 0) {
                fprintf(stderr, "Missing path or variable value.\n");
                goto error;
            }

            if (is_env_var_name(argv[3]) == false) {
                fprintf(stderr,
                        "\"%s\" is not a valid environment variable name.\n",
                        argv[3]);
                goto error;
            }

            if (strcmp(argv[4], "-") == 0) {
                // read from stdin
                fread(stdin_buffer, sizeof(stdin_buffer) - 1, 1, stdin);
                denv_table_set_value(table, argv[3], stdin_buffer,
                                     ELEMENT_IS_ENV);
            } else {
                denv_table_set_value(table, argv[3], argv[4], ELEMENT_IS_ENV);
            }

        } else if (argc == 6) {
            // -eb or -be then path

            if ((strcmp(argv[2], "-eb") && strcmp(argv[2], "-be")) == 0) {
                flag = ELEMENT_IS_ENV;
            } else if (strcmp(argv[2], "-b") != 0) {
                if (strcmp(argv[2], "-e") == 0) {
                    fprintf(stderr, "Missing -b option for bind path, or extra "
                                    "argument given.\n");
                    goto error;
                }
                fprintf(stderr, "Unknwon option \"%s\".\n", argv[2]);
                goto error;
            }

            if (is_env_var_name(argv[4]) == false) {
                fprintf(stderr,
                        "\"%s\" is not a valid environment variable name.\n",
                        argv[4]);
                goto error;
            }

            // init table on path
            table = init_on_path(argv[3]);
            if (table == NULL) {
                goto error;
            }

            if (strcmp(argv[5], "-") == 0) {
                // read from stdin
                fread(stdin_buffer, sizeof(stdin_buffer) - 1, 1, stdin);
                denv_table_set_value(table, argv[4], stdin_buffer, flag);
            } else {
                denv_table_set_value(table, argv[4], argv[5], flag);
            }

        } else if (argc < 4) {
            fprintf(stderr, "Not enough arguments for the set command.\n");
            goto error;
        } else {
            fprintf(stderr, "Too many arguments.\n");
            goto error;
        }

    } break;

    case GET: {
        // denv get var_name				3
        // denv get -b bind/path var_name	5

        char *value;

        if (argc == 3) {

            table = init();
            if (table == NULL)
                goto error;

            value = denv_table_get_value(table, argv[2]);

        } else if (argc == 5) {
            if (strcmp(argv[2], "-b") != 0) {
                fprintf(stderr, "Unknown option \"%s\".\n", argv[2]);
                goto error;
            }

            // init table on path
            table = init_on_path(argv[3]);
            if (table == NULL) {
                goto error;
            }

            value = denv_table_get_value(table, argv[4]);

        } else if (argc < 3) {
            fprintf(stderr, "Missing variable name.\n");
            goto error;
        } else if (argc == 4) {
            fprintf(stderr, "Too many arguments or missing variable name.\n");
            goto error;
        } else if (argc > 5) {
            fprintf(stderr, "Too many arguments.\n");
            goto error;
        }

        if (value) {
            printf("%s\n", value);
        }

    } break;

    case REMOVE: {
        // -b
        // denv rm var_name					3
        // denv rm -b bind/path var_name	5

        if (argc == 3) {

            table = init();
            if (table == NULL)
                goto error;

            denv_table_delete_value(table, argv[2]);
        } else if (argc == 5) {
            if (strcmp(argv[2], "-b") != 0) {
                fprintf(stderr, "Unknown option \"%s\".\n", argv[2]);
                goto error;
            }

            // init table on path
            table = init_on_path(argv[3]);
            if (table == NULL) {
                goto error;
            }

            denv_table_delete_value(table, argv[4]);
        } else if (argc < 3) {
            fprintf(stderr, "Missing variable name.\n");
            goto error;
        } else if (argc == 4) {
            fprintf(stderr, "Too many arguments or missing variable name.\n");
            goto error;
        } else if (argc > 5) {
            fprintf(stderr, "Too many arguments.\n");
            goto error;
        }

    } break;

    case DROP: {
        // -b
        // denv drop					2
        // denv drop -f 				3
        // denv drop -b bind/path		4
        // denv drop -bf bind/path 		4

        if (argc == 2) {
            file_name = get_bind_path(g_path_buffer, PATH_BUFFER_LENGHT);

            printf("Are you sure you want to destroy the shared memory "
                   "environment? [N/y]\n");

            fgets(input_buffer, sizeof(input_buffer), stdin);

            if (input_buffer[0] != 'y' && input_buffer[0] != 'Y')
                break;

            if (denv_shmem_destroy(file_name) == false) {
                fprintf(stderr,
                        "Failed to destroy shared memory environment.\n");
                goto error;
            } else {
                printf("Shared memory environment destroyed successfully.\n");
            }
        } else if (argc == 4) {

            bool force = false;

            if ((strcmp(argv[2], "-bf") && strcmp(argv[2], "-fb")) == 0) {
                force = true;
            }

            if ((strcmp(argv[2], "-b") && strcmp(argv[2], "-bf") &&
                 strcmp(argv[2], "-fb")) != 0) {
                fprintf(stderr, "Unknwon option \"%s\".\n", argv[2]);
                goto error;
            }

            if (force == false) {
                printf("Are you sure you want to destroy the shared memory "
                       "environment? [N/y]\n");

                fgets(input_buffer, sizeof(input_buffer), stdin);

            } else {
                input_buffer[0] = 'y';
            }

            if (input_buffer[0] != 'y' && input_buffer[0] != 'Y')
                break;

            if (denv_shmem_destroy(argv[3]) == false) {
                fprintf(stderr,
                        "Failed to destroy shared memory environment.\n");
                goto error;
            } else {
                printf("Shared memory environment destroyed successfully.\n");
            }
        } else if (argc == 3) {
            if (strcmp(argv[2], "-f") != 0) {
                fprintf(stderr, "Unknwon option \"%s\".\n", argv[2]);
                goto error;
            }

            file_name = get_bind_path(g_path_buffer, PATH_BUFFER_LENGHT);

            if (denv_shmem_destroy(file_name) == false) {
                fprintf(stderr,
                        "Failed to destroy shared memory environment.\n");
                goto error;
            } else {
                printf("Shared memory environment destroyed successfully.\n");
            }
        } else if (argc > 4) {
            fprintf(stderr, "Too many arguments.\n");
            goto error;
        }

    } break;

    case LIST: {
        // -b
        // denv ls					2
        // denv ls -x 				3
        // denv ls -b bind/path 	4
        // denv ls -xb bind/path 	4
        // denv ls -bx bind/path 	4

        bool env_ind = false;

        if (argc == 2) {

            table = init();
            if (table == NULL)
                goto error;

            denv_table_list_values(table, true);
        } else if (argc == 4) {

            if (strcmp(argv[2], "-b") != 0) {
                if ((strcmp(argv[2], "-xb") && strcmp(argv[2], "-bx")) == 0) {
                    env_ind = true;
                } else {
                    fprintf(stderr, "Unknown option \"%s\".\n", argv[2]);
                    goto error;
                }
            }

            table = init_on_path(argv[3]);
            if (table == NULL)
                goto error;

            denv_table_list_values(table, env_ind);

        } else if (argc == 3) {

            if (strcmp(argv[2], "-x") != 0) {
                fprintf(stderr, "Unknown option \"%s\".\n", argv[2]);
                goto error;
            }

            denv_table_list_values(table, false);

        } else if (argc > 4) {
            fprintf(stderr, "Too many arguments.\n");
            goto error;
        }

    } break;

    case STATS: {
        // arg
        // 0 	1 	  2 		3			4
        // total denv stats
        // 2 denv stats --csv
        // 3
        //			  --json
        //			  --xml
        //			  --pretty (default)
        //
        // denv stats -b 		bind/path 4 denv stats -b
        // --csv 		bind/path 	5 denv stats --csv 	-b
        // bind/path 	5

        if (argc == 2) {

            table = init();
            if (table == NULL)
                goto error;

            denv_print_stats_csv(table); // denv_print_stats_pretty
        } else if (argc == 4) {

            if (strcmp(argv[2], "-b") != 0) {
                fprintf(stderr, "Unknown option \"%s\".\n", argv[2]);
                goto error;
            }

            table = init_on_path(argv[3]);
            if (table == NULL)
                goto error;

            denv_print_stats_csv(table); // denv_print_stats_pretty

        } else if (argc == 3) {

            int opt = UNDEFINED;
            if (strcmp(argv[2], "--csv") == 0)
                opt = CSV;
            if (strcmp(argv[2], "--xml") == 0)
                opt = XML;
            if (strcmp(argv[2], "--json") == 0)
                opt = JSON;
            if (strcmp(argv[2], "--pretty") == 0)
                opt = PRETTY;
            if (opt == UNDEFINED) {
                fprintf(stderr, "Unknown option \"%s\".\n", argv[2]);
                goto error;
            }

            table = init();
            if (table == NULL)
                goto error;

            switch (opt) {
            case CSV:
                denv_print_stats_csv(table);
                break;

            case XML:
                fprintf(stderr, "Feature not implemented.\n");
                goto error;
                break;

            case JSON:
                fprintf(stderr, "Feature not implemented.\n");
                goto error;
                break;

            case PRETTY:
                fprintf(stderr, "Feature not implemented.\n");
                goto error;
                break;
            }

        } else if (argc == 5) {
            int opt = UNDEFINED;
            char *arg = argv[2];
            if (strcmp(argv[2], "-b") == 0) {
                arg = argv[3];
            } else {
                if (strcmp(argv[3], "-b") != 0) {
                    fprintf(stderr, "Unknown option \"%s\".\n", argv[3]);
                    goto error;
                }
            }

            if (strcmp(arg, "--csv") == 0)
                opt = CSV;
            if (strcmp(arg, "--xml") == 0)
                opt = XML;
            if (strcmp(arg, "--json") == 0)
                opt = JSON;
            if (strcmp(arg, "--pretty") == 0)
                opt = PRETTY;

            table = init_on_path(argv[4]);
            if (table == NULL)
                goto error;

            switch (opt) {
            case UNDEFINED:
                fprintf(stderr, "Unknown option \"%s\".\n", arg);
                goto error;
                break;

            case CSV:
                denv_print_stats_csv(table);
                break;

            case XML:
                fprintf(stderr, "Feature not implemented.\n");
                goto error;
                break;

            case JSON:
                fprintf(stderr, "Feature not implemented.\n");
                goto error;
                break;

            case PRETTY:
                fprintf(stderr, "Feature not implemented.\n");
                goto error;
                break;
            }

        } else if (argc > 5) {
            fprintf(stderr, "Too many arguments.\n");
            goto error;
        }

    } break;

    case CLEANUP: {
        // -b
        // denv cleanup
        // denv cleanup -b bind/path

        if (argc == 2) {

            table = init();
            if (table == NULL)
                goto error;

            denv_clear_freed(table);
        } else if (argc == 4) {

            if (strcmp(argv[2], "-b") != 0) {
                fprintf(stderr, "Unknown option \"%s\".\n", argv[2]);
                goto error;
            }

            table = init_on_path(argv[3]);
            if (table == NULL)
                goto error;

            denv_clear_freed(table);

        } else if (argc == 3) {
            fprintf(stderr, "Missing path or too many arguments.\n");
            goto error;
        } else if (argc > 4) {
            fprintf(stderr, "Too many arguments.\n");
            goto error;
        }

    } break;

    case SAVE: {
        // -b
        // denv save path/to/save_file					3
        // denv save -b bind/path path/to/save_file		5

        int err = 0;

        if (argc == 3) {

            table = init();
            if (table == NULL)
                goto error;

            err = denv_save_to_file(table, argv[2]);
            if (err)
                goto error;

        } else if (argc == 5) {

            if (strcmp(argv[2], "-b") != 0) {
                fprintf(stderr, "Unknown option \"%s\".\n", argv[2]);
                goto error;
            }

            table = init_on_path(argv[3]);
            if (table == NULL)
                goto error;

            err = denv_save_to_file(table, argv[4]);
            if (err)
                goto error;

        } else if (argc == 4) {
            fprintf(stderr, "Missing path or too many arguments.\n");
            goto error;
        } else if (argc < 3) {
            fprintf(stderr, "Missing path to save file.\n");
            goto error;
        } else if (argc > 5) {
            fprintf(stderr, "Too many arguments.\n");
            goto error;
        }

    } break;

    case LOAD: {
        // -b
        // denv load path/so/save_file					3
        // denv load -f path/to/save_file				4
        // denv load -b bind/path path/to/save_file		5
        // denv load -bf bind/path path/to/save_file 	5

        if (argc == 3) {

            table = init();
            if (table == NULL)
                goto error;

            printf("This will overwrite current variables, Are you sure you "
                   "want to load \"%s\" to denv? [N/y]\n",
                   argv[2]);

            fgets(input_buffer, sizeof(input_buffer), stdin);

            if (input_buffer[0] != 'y' && input_buffer[0] != 'Y')
                break;

            table = denv_load_from_file(table, argv[2]);
            if (table == NULL)
                goto error;

        } else if (argc == 5) {

            bool force = false;

            if ((strcmp(argv[2], "-bf") && strcmp(argv[2], "-fb")) == 0) {
                force = true;
            }

            if ((strcmp(argv[2], "-b") && strcmp(argv[2], "-bf") &&
                 strcmp(argv[2], "-fb")) != 0) {
                fprintf(stderr, "Unknown option \"%s\".\n", argv[2]);
                goto error;
            }

            if (force == false) {
                printf("This will overwrite current variables, Are you sure "
                       "you want to load \"%s\" to denv at \"%s\"? [N/y]\n",
                       argv[4], argv[3]);

                fgets(input_buffer, sizeof(input_buffer), stdin);
            } else {
                input_buffer[0] = 'y';
            }

            if (input_buffer[0] != 'y' && input_buffer[0] != 'Y')
                break;

            table = init_on_path(argv[3]);
            if (table == NULL)
                goto error;

            table = denv_load_from_file(table, argv[4]);
            if (table == NULL)
                goto error;

        } else if (argc == 4) {
            if (strcmp(argv[2], "-f") != 0) {
                fprintf(stderr, "Unknown option \"%s\".\n", argv[2]);
                goto error;
            }

            table = init();
            if (table == NULL)
                goto error;

            table = denv_load_from_file(table, argv[3]);
            if (table == NULL)
                goto error;

        } else if (argc < 3) {
            fprintf(stderr, "Missing path to save file.\n");
            goto error;
        } else if (argc > 5) {
            fprintf(stderr, "Too many arguments.\n");
            goto error;
        }

    } break;

    case AWAIT: {
        // denv await variable
        // denv await -b some/path variable
        file_name = get_bind_path(g_path_buffer, PATH_BUFFER_LENGHT);
        if (file_name == NULL)
            goto error;

        if (argc == 3) {

            table = init_only_table(file_name);
            if (table == NULL) {
                fprintf(stderr, "Failed to init table.\n");
                goto error;
            }

            denv_await_element(table, argv[2], POLLING_INTERVAL);

        } else if (argc == 5) {

            if (strcmp(argv[2], "-b") != 0) {
                fprintf(stderr, "Unknown option \"%s\".\n", argv[2]);
                goto error;
            }

            table = init_only_table(argv[3]);
            if (table == NULL) {
                fprintf(stderr, "Failed to init table.\n");
                goto error;
            }

            denv_await_element(table, argv[4], POLLING_INTERVAL);

        } else if (argc == 4) {
            fprintf(stderr, "Missing path or too many arguments.\n");
            goto error;
        } else if (argc < 3) {
            fprintf(stderr, "Missing variable name.\n");
            goto error;
        } else if (argc > 5) {
            fprintf(stderr, "Too many arguments.\n");
            goto error;
        }

    } break;

    case EXEC: {
        //			 arg0 (&argv[2])
        // denv exec printenv arg1 arg2 arg3 arg4 ...
        //						  arg0 (&argv[4])
        // denv exec -b bind/path printenv arg1 arg2 arg3 arg4 ...
        if (argc == 2) {
            printf("Not enough arguments!\n");
            goto error;
        }
        if ((strcmp(argv[2], "-b")) == 0) {

            if (argc == 3) {
                fprintf(stderr, "Missing bind path.\n");
                goto error;
            }
            if (argc == 4) {
                fprintf(stderr, "Missing program.\n");
                goto error;
            }

            table = init_only_table(argv[3]);
            if (table == NULL)
                goto error;

            if (denv_exec(table, argv[4], &argv[4]) == -1)
                goto error;

        } else {
            file_name = get_bind_path(g_path_buffer, PATH_BUFFER_LENGHT);
            if (file_name == NULL)
                goto error;

            table = init_only_table(file_name);
            if (table == NULL)
                goto error;

            if (denv_exec(table, argv[2], &argv[2]) == -1)
                goto error;
        }

    } break;

    case CLONE: {
        // denv clone
        // denv clone -b bind/path
        if (argc == 2) {
            table = init();
            denv_clone_env(table, envp);
        } else if (argc == 4) {
            if (strcmp(argv[2], "-b") != 0) {
                fprintf(stderr, "Unknown option \"%s\".\n", argv[2]);
                goto error;
            }

            table = init_on_path(argv[3]);
            if (table == NULL)
                goto error;

            denv_clone_env(table, envp);

        } else if (argc == 3) {
            fprintf(stderr, "Missing path or too many arguments.\n");
            goto error;
        } else if (argc > 4) {
            fprintf(stderr, "Too many arguments!\n");
            goto error;
        }
    } break;

    case EXPORT: {
        // denv export -						3
        // denv export filepath					3
        // denv export -b bind/path -			5
        // denv export -b bind/path filepath	5

        FILE *save_env = NULL;

        if (argc == 2) {
            fprintf(stderr, "Missing file path or \"-\".\n");
            goto error;
        } else if (argc == 3) {
            if (argv[2][0] == '-') {
                save_env = stdout;
            } else {
                save_env = fopen(argv[2], "w+");
                if (save_env == NULL) {
                    perror("fopen");
                    goto error;
                }
            }

            table = init();

            denv_make_env_save_file(table, save_env);

        } else if (argc == 4) {
            fprintf(stderr, "Missing path or too many arguments.\n");
            goto error;
        } else if (argc == 5) {
            if (strcmp(argv[2], "-b") != 0) {
                fprintf(stderr, "Unknown option \"%s\".\n", argv[2]);
                goto error;
            }

            table = init_on_path(argv[3]);
            if (table == NULL)
                goto error;

            if (argv[4][0] == '-') {
                save_env = stdout;
            } else {
                save_env = fopen(argv[4], "w+");
                if (save_env == NULL) {
                    perror("fopen");
                    goto error;
                }
            }

            denv_make_env_save_file(table, save_env);

        } else if (argc > 5) {
            fprintf(stderr, "Too many arguments!\n");
            goto error;
        }

        if (save_env) {
            fclose(save_env);
        }
    } break;

    case DAEMON: {
        // denv daemon        2
        // denv daemon -b bind/path save-file 5

        if (argc == 2) {
            table = init();

            if (table == NULL)
                goto error;

            char *save_file_path =
                get_bind_path(g_path_buffer, PATH_BUFFER_LENGHT);
            if (save_file_path == NULL)
                goto error;

            save_file_path = strcat(save_file_path, DENV_SAVE_PATH);

            if (check_path(save_file_path)) {
                table = denv_load_from_file(table, save_file_path);
                if (table == NULL)
                    goto error;
            }

            // hang-up until SIGTERM
            sigset_t set;
            int sig;

            sigemptyset(&set);
            sigaddset(&set, SIGTERM);

            sigprocmask(SIG_BLOCK, &set, NULL);

            int pid = getpid();

            printf("PID: %i Waiting until SIGTERM...\n", pid);

            openlog("DENV", LOG_PID | LOG_CONS, LOG_USER);

            if (sigwait(&set, &sig) == 0) {
                // Check if file exists, move to .old and then save new file
                if (check_path(save_file_path)) {
                    char new_path[PATH_BUFFER_LENGHT] = {0};
                    strcpy(new_path, save_file_path);
                    strcat(new_path, ".old");
                    int ren_err = rename(save_file_path, new_path);
                    if (ren_err == -1) {
                        syslog(LOG_ERR,
                               "Couldn't move current save to old save.");
                        goto error;
                    }
                }

                int err = denv_save_to_file(table, save_file_path);
                if (err)
                    goto error;
            } else {
                syslog(LOG_ERR, "Daemon was interrupted abruptly, couldn't "
                                "save it's state.");
                goto error;
            }

            closelog();

        } else if ((argc == 3) || (argc == 4)) {
            fprintf(
                stderr,
                "Follow this pattern to use denv daemon with a binding path:\n"
                "\tdenv daemon -b binding/path save-file-name\n");
            goto error;
        } else if (argc == 5) {

            if (strcmp(argv[2], "-b") != 0) {
                fprintf(stderr, "Unknown option \"%s\".\n", argv[2]);
                goto error;
            }

            table = init_on_path(argv[3]);
            if (table == NULL)
                goto error;

            char *save_file_path = argv[4];

            if (check_path(save_file_path)) {
                table = denv_load_from_file(table, save_file_path);
                if (table == NULL)
                    goto error;
            }

            // hang-up until SIGTERM
            sigset_t set;
            int sig;

            sigemptyset(&set);
            sigaddset(&set, SIGTERM);

            sigprocmask(SIG_BLOCK, &set, NULL);

            printf("Waiting until SIGTERM...\n");

            openlog("DENV", LOG_PID | LOG_CONS, LOG_USER);

            if (sigwait(&set, &sig) == 0) {
                // Check if file exists, move to .old and then save new file
                if (check_path(save_file_path)) {
                    char new_path[PATH_BUFFER_LENGHT] = {0};
                    strcpy(new_path, save_file_path);
                    strcat(new_path, ".old");
                    int ren_err = rename(save_file_path, new_path);
                    if (ren_err == -1) {
                        syslog(LOG_ERR,
                               "Couldn't move current save to old save.");
                        goto error;
                    }
                }

                int err = denv_save_to_file(table, save_file_path);
                if (err)
                    goto error;
            } else {
                syslog(LOG_ERR,
                       "Daemon was interrupted abruptly, couldn't save it's "
                       "state for \"%s\".",
                       save_file_path);
                goto error;
            }

            closelog();

        } else {
            fprintf(stderr, "Too many arguments.\n");
            goto error;
        }
    }
    }

    if (table) {
        deinit(table);
    }

    return 0;

error:;
    if (table) {
        deinit(table);
    }

    return -1;
}
