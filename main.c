/*
	DENV MAIN
*/

#include "denv.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

#define DENV_BIND_PATH				("/.local/share/denv")
#define ARRLEN(X) 					(sizeof(X)/sizeof((X)[0]))
#define BUFF_SIZE 					(1024)
#define STDIN_VAR_BUFFER_LENGTH		(4096)
#define PATH_BUFFER_LENGHT			(4096)

typedef enum {
	UNDEFINED,
	VERSION,
	HELP,
	SET,
	GET,
	DELETE,
	REMOVE,
	LIST,
	STATS,
	CLEANUP,
	SAVE,
	LOAD
}commands_states;

static const struct {
	char*	cmd;
	bool	do_load_table;
	char*	options;
	int     state;
}commands[] = {
	{"-h",			false,	NULL,	HELP},
	{"-v",			false,	NULL,	VERSION},
	{"--help",		false,	NULL,	HELP},
	{"--version",	false,	NULL,	VERSION},
	{"help",		false,	NULL,	HELP},
	{"version",		false,	NULL,	VERSION},
	{"set",			true,	"eb:",	SET},
	{"get",			true,	"b:",	GET},
	{"delete",		true,	"b:",	DELETE},
	{"remove",		false,	"b:",	REMOVE}, // remove does not init table
	{"list",		true,	"b:",	LIST},
	{"stats",		true,	"b:",	STATS},
	{"cleanup",		true,	"b:",	CLEANUP},
	{"save",		true,	"b:",  	SAVE},
	{"load",		true,	"b:",	LOAD}	
};

void print_help(void) {
	printf(
		"Usage: denv [command] [options] <key>, <value>\n"
		"\t-h / --help / help             Display this information.\n"
		"\t-v / --version / version       Display current version.\n"
		"\tset [-b/-e] <key> <value>      Sets the key with the value provided.\n"
		"\tget [-b] <key>                 Gets the value stored in the key.\n"
		"\tdelete [-b] <key>              Deletes the key and value pair.\n"
		"\tlist [-b]                      Lists all keys.\n"
		"\tremove [-b]                    Removes the attached shmem.\n"
		"\tstats [-b]                     Print stats.\n"
		"\tcleanup [-b]                   Clear deleted variables from memory.\n"
		"\tsave [-b] <filename>           Save denv table to a file.\n"
		"\tload [-b] <filename>           Load from a denv save file.\n\n"
		"option -b:        Path to the attached memory.\n"
		"option -e:        Set variable as an envrionment variable.\n"
		"option -f:        Get only the flags of the variables.\n"
	);
}

int make_bind_path(char *path_buf){
	struct stat st = {0};

	if(stat(path_buf, &st) == -1) {
		int err = mkdir(path_buf, 0766);
		if(err) {
			perror("mkdir");
			return -1;
		}		
	}
	
	return 0;
}

char *get_bind_path(char *path_buf, size_t buf_len){

	char *home_path = getenv("HOME");
	if(home_path == NULL) {
		fprintf(stderr, "HOME environment variable is not set.\n");
		return NULL;
	}

	if(buf_len <= (strlen(home_path) + strlen(DENV_BIND_PATH))) {
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

bool check_bind_path(const char *path) {
	struct stat st = {0};

	if(stat(path, &st) == -1) {
		fprintf(stderr, "Bind path not found.\n");
		return false;
	}

	return true;
}

char g_path_buffer[PATH_BUFFER_LENGHT] = {0};

char *load_path(){
	char *file_name = get_bind_path(g_path_buffer, PATH_BUFFER_LENGHT);
	return file_name;
}

bool is_env_char(char e){
	if(e >= '0' && e <= '9') return true;
	if(e >= 'a' && e <= 'z') return true;
	if(e >= 'A' && e <= 'Z') return true;
	if(e == '_') return true;
	return false;
}

bool is_env_var_name(char *name){
	for(size_t i = 0; i < strlen(name); i++){
		if(i == 0){
			if(name[i] >= '0' && name[i] <= '9') return false;
		}
		if(is_env_char(name[i]) == false) return false;
	}
	return true;
}

Table  *init(){

	char *file_name = load_path();
	if(file_name == NULL) return NULL;

	//attach memory block
	Table *table = denv_shmem_attach(file_name, sizeof(Table));

	if(table == NULL){
		fprintf(stderr, "Failed to create a shared memory environment.\n");
		return NULL;
	}

	if((table->flags & TABLE_IS_INITIALIZED) == 0){
		int sem_ret = sem_init(&table->denv_sem, 1, 1);
		if(sem_ret < 0){
			perror("sem_init");
			return NULL;
		}
		denv_table_init(table);
	}

	sem_wait(&table->denv_sem);

	return table;
}

void deinit(Table *table){
	assert(table);

	sem_post(&table->denv_sem);

	if(denv_shmem_detach(table) == false){
		fprintf(stderr, "Failed to detach shared memory.\n");
	}

	table = NULL;
}

Table *init_on_path(char *path) {
	struct stat st = {0};

	if(stat(path, &st) == -1) {
		fprintf(stderr, "Bind path \"%s\" doesn't exist.\n", path);
		return NULL;
	}

	Table *table = denv_shmem_attach(path, sizeof(Table));

	if(table == NULL){
		fprintf(stderr, "Failed to create a shared memory environment.\n");
		return NULL;
	}

	if((table->flags & TABLE_IS_INITIALIZED) == 0){
		int sem_ret = sem_init(&table->denv_sem, 1, 1);
		if(sem_ret < 0){
			perror("sem_init");
			return NULL;
		}
		denv_table_init(table);
	}

	sem_wait(&table->denv_sem);

	return table;
}

Table *init_only_table(char *file_name) {
	Table *table = denv_shmem_attach(file_name, sizeof(Table));

	if(table == NULL){
		fprintf(stderr, "Failed to create a shared memory environment.\n");
		return NULL;
	}
	return table;	
}

int main(int argc, char* argv[]){

	if(argc < 2){
		fprintf(stderr, "Not enough arguments.\n");
		print_help();
		return -1;
	}

	int state = UNDEFINED;
	// char *options = NULL;
	bool load_table = false;

	for(size_t i = 0; i < ARRLEN(commands); i++){
		if(argv[1][0] == commands[i].cmd[0]){
			if(strcmp(argv[1], commands[i].cmd) == 0){
				state = commands[i].state;
				load_table = commands[i].do_load_table;
			}
		}
	}
	
	Table *table = NULL;

	if(load_table){
		table = init();
		if(!table) return -1;
	}

	char *file_name;
	
	char stdin_buffer[STDIN_VAR_BUFFER_LENGTH] = {0};
	char input_buffer[200] = {0};
	
	switch(state){
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

				if(strcmp(argv[3], "-") == 0) {
					// read from stdin
					fread(stdin_buffer, sizeof(stdin_buffer) - 1, 1, stdin);
					denv_table_set_value(table, argv[2], stdin_buffer, 0);
				} else {
					denv_table_set_value(table, argv[2], argv[3], 0);
				}

			} else if (argc == 5) {
				// set -e "name" "value"
				if(strcmp(argv[2], "-e") != 0) {
					fprintf(stderr, "Unknwon option \"%s\".\n", argv[2]);
					goto error;
				}

				if(strcmp(argv[2], "-b") == 0) {
					fprintf(stderr, "Missing path or variable value.\n");
					goto error;
				}

				if(is_env_var_name(argv[3]) == false) {
					fprintf(stderr, "\"%s\" is not a valid environment variable name.\n", argv[3]);
					goto error;
				}

				if(strcmp(argv[4], "-") == 0) {
					// read from stdin
					fread(stdin_buffer, sizeof(stdin_buffer) - 1, 1, stdin);
					denv_table_set_value(table, argv[3], stdin_buffer, ELEMENT_IS_ENV);
				} else {
					denv_table_set_value(table, argv[3], argv[4], ELEMENT_IS_ENV);
				}
				
			} else if (argc == 6) {
				// -eb or -be then path

				if((strcmp(argv[2], "-eb") && strcmp(argv[2], "-be")) == 0) {
					flag = ELEMENT_IS_ENV;
				} else if (strcmp(argv[2], "-b") != 0) {
					if(strcmp(argv[2], "-e") == 0){
						fprintf(stderr, "Missing -b option for bind path, or extra argument given.\n");
						goto error;
					}
					fprintf(stderr, "Unknwon option \"%s\".\n", argv[2]);
					goto error;
				}

				if(is_env_var_name(argv[4]) == false) {
					fprintf(stderr, "\"%s\" is not a valid environment variable name.\n", argv[4]);
					goto error;
				}

				deinit(table);

				// init table on path
				table = init_on_path(argv[3]);
				if(table == NULL){
					goto error;
				}

				if(strcmp(argv[5], "-") == 0) {
					// read from stdin
					fread(stdin_buffer, sizeof(stdin_buffer) - 1, 1, stdin);
					denv_table_set_value(table, argv[4], stdin_buffer, flag);
				} else {
					denv_table_set_value(table, argv[4], argv[5], flag);
				}
				
			} else if (argc < 4){
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

			if(argc == 3){
				value = denv_table_get_value(table, argv[2]);
				
			} else if (argc == 5) {
				if(strcmp(argv[2], "-b") != 0){
					fprintf(stderr, "Unknown option \"%s\".\n", argv[2]);
					goto error;
				}

				deinit(table);

				// init table on path
				table = init_on_path(argv[3]);
				if(table == NULL){
					goto error;
				}

				value = denv_table_get_value(table, argv[4]);
				
			} else if (argc < 3 ) {
				fprintf(stderr, "Missing variable name.\n");
				goto error;
			} else if (argc == 4) {
				fprintf(stderr, "Too many arguments or missing variable name.\n");
				goto error;
			} else if (argc > 5) {
				fprintf(stderr, "Too many arguments.\n");
				goto error;
			}

			if(value){
				printf("%s\n", value);
			}

		} break;

		case DELETE: {
			// -b
			// denv delete var_name					3
			// denv delete -b bind/path var_name	5

			if(argc == 3) {
				denv_table_delete_value(table, argv[2]);
			} else if (argc == 5) {
				if(strcmp(argv[2], "-b") != 0) {
					fprintf(stderr, "Unknown option \"%s\".\n", argv[2]);
					goto error;					
				}

				deinit(table);
				
				// init table on path
				table = init_on_path(argv[3]);
				if(table == NULL){
					goto error;
				}

				denv_table_delete_value(table, argv[4]);
			} else if (argc < 3 ) {
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

		case REMOVE: {
			// -b
			// denv remove					2
			// denv remove -b bind/path		4

			if(argc == 2) {
				file_name = get_bind_path(g_path_buffer, PATH_BUFFER_LENGHT);

				table = init_only_table(file_name);
				if(!table) return -1;
				
				printf("Are you sure you want to destroy the shared memory environment? [N/y]\n");

				fgets(input_buffer, sizeof(input_buffer), stdin);

				if(input_buffer[0] != 'y' && input_buffer[0] != 'Y') break;
			
				if(denv_shmem_destroy(table, file_name) == false){
					fprintf(stderr, "Failed to destroy shared memory environment.\n");
					goto error;
				} else {
					printf("Shared memory environment destroyed successfully.\n");
				}
			} else if (argc == 4) {
				if(strcmp(argv[2], "-b") != 0){
					fprintf(stderr, "Unknwon option \"%s\".\n", argv[2]);
					goto error;
				}

				table = init_only_table(argv[3]);
				if(!table) return -1;
				
				printf("Are you sure you want to destroy the shared memory environment? [N/y]\n");

				fgets(input_buffer, sizeof(input_buffer), stdin);

				if(input_buffer[0] != 'y' && input_buffer[0] != 'Y') break;
			
				if(denv_shmem_destroy(table, argv[3]) == false){
					fprintf(stderr, "Failed to destroy shared memory environment.\n");
					goto error;
				} else {
					printf("Shared memory environment destroyed successfully.\n");
				}
			} else if (argc == 3) {
				fprintf(stderr, "Bind path not given or extra argument given.\n");
				goto error;
			} else if (argc > 4) {
				fprintf(stderr, "Too many arguments.\n");
				goto error;
			}

		} break;
		
		case LIST: {
			// -b
			// denv list					2
			// denv list -b bind/path 		4

			if(argc == 2) {
				denv_table_list_values(table);
			} else if (argc == 4) {

				if(strcmp(argv[2], "-b") != 0) {
					fprintf(stderr, "Unknown option \"%s\".\n", argv[2]);
					goto error;
				}
			
				deinit(table);

				table = init_on_path(argv[3]);
				if(table == NULL) goto error;

				denv_table_list_values(table);
			} else if (argc == 3) {
				fprintf(stderr, "Missing path or too many arguments.\n");
				goto error;
			} else if (argc > 4) {
				fprintf(stderr, "Too many arguments.\n");
				goto error;
			}
			
		} break;

		case STATS: {
			// -b
			// denv stats					2
			// denv stats -b bind/path		4

			if(argc == 2) {
				denv_print_stats(table);
			} else if (argc == 4) {

				if(strcmp(argv[2], "-b") != 0) {
					fprintf(stderr, "Unknown option \"%s\".\n", argv[2]);
					goto error;
				}

				deinit(table);

				table = init_on_path(argv[3]);
				if(table == NULL) goto error;

				denv_print_stats(table);
				
			} else if (argc == 3) {
				fprintf(stderr, "Missing path or too many arguments.\n");
				goto error;
			} else if (argc > 4) {
				fprintf(stderr, "Too many arguments.\n");
				goto error;
			}
			
		} break;
		
		case CLEANUP: {
			// -b
			// denv cleanup
			// denv cleanup -b bind/path
			
			if(argc == 2) {
				denv_clear_freed(table);
			} else if (argc == 4) {

				if(strcmp(argv[2], "-b") != 0) {
					fprintf(stderr, "Unknown option \"%s\".\n", argv[2]);
					goto error;
				}

				deinit(table);

				table = init_on_path(argv[3]);
				if(table == NULL) goto error;

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
			
			if(argc == 3) {

				err = denv_save_to_file(table, argv[2]);
				if(err) goto error;
				
			} else if (argc == 5) {

				if(strcmp(argv[2], "-b") != 0) {
					fprintf(stderr, "Unknown option \"%s\".\n", argv[2]);
					goto error;
				}

				deinit(table);

				err = denv_save_to_file(table, argv[4]);
				if(err) goto error;
				
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
			// denv load -b bind/path path/to/save_file		5

			if(argc == 3) {

				table = denv_load_from_file(table, argv[2]);
				if(table == NULL) goto error;
				
			} else if (argc == 5) {

				if(strcmp(argv[2], "-b") != 0) {
					fprintf(stderr, "Unknown option \"%s\".\n", argv[2]);
					goto error;
				}

				deinit(table);

				table = denv_load_from_file(table, argv[4]);
				if(table == NULL) goto error;

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
		
	}
	
	if(table){
		deinit(table);
	}

	return 0;

	error:;
	if(table){
		deinit(table);
	}

	return -1;
}
