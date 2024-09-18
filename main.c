/*
	DENV MAIN
*/

#include "denv.h"
#include <stdio.h>

void print_help(void) {
	printf(
		"Usage: denv [options] <key>, <value>\n"
		"-h                     Display this information.\n"
		"-v                     Display current version.\n"
		"-s <key> <value>       Sets the key with the value provided\n"
		"-g <key>               Gets the value stored in the key\n"
		"-d <key>               Deletes the key and value pair\n"
		"-l <key>               Lists all keys\n"
		"-r                     Removes the attached shmem\n"
		"-t                     Print stats\n"
		"-c                     Clear deleted variables from memory\n"
	);
}

int main(int argc, char* argv[]){

	char input_buffer[200] = {0};

	if(argc < 2){
		// print error
		fprintf(stderr, "Not enough arguments.\n");
		print_help();
		return -1;
	}

	char *file_name = getenv("HOME");
	if(file_name == NULL){
		fprintf(stderr, "HOME environment variable is not set.\n");
		return -1;
	}

	//attach memory block
	Table *table = denv_shmem_attach(file_name, sizeof(Table));

	if(table == NULL){
		fprintf(stderr, "Failed to create a shared memory environment.\n");
		return -1;
	}

	if((table->flags & TABLE_IS_INITIALIZED) == 0){
		int sem_ret = sem_init(&table->denv_sem, 1, 1);
		if(sem_ret < 0){
			perror("sem_init");
			return -1;
		}
		denv_table_init(table);
	}

	sem_wait(&table->denv_sem);
	
	if(argv[1][0] == '-'){
		switch(argv[1][1]){
			case 'h':
				print_help();
				break;

			case 'v':
				{
					denv_print_version();
				}
				break;

			case 's':
				{
					if(argc < 3){
						fprintf(stderr, "Missing key and value.\n");
						goto error;
					}

					if(argc < 4){
						fprintf(stderr, "Missing value.\n");
						goto error;						
					}

					char *key_name = argv[2];
					char *key_value = NULL;
					if(argc >= 4){
						key_value = argv[3];
					}

					denv_table_set_value(table, key_name, key_value);
				}
				break;

			case 'g':
				{
					if(argc < 3){
						fprintf(stderr, "Missing key name.\n");
						goto error;
					}

					char *key_name = argv[2];

					char *key_value = denv_table_get_value(table, key_name);

					if(key_value == NULL) goto error;
					
					printf("%s\n", key_value);
				}
				break;

			case 'd':
				if(argc < 3){
					fprintf(stderr, "Missing key name.\n");
					goto error;
				}

				denv_table_delete_value(table, argv[2]);
				break;

			case 'l':
				denv_table_list_values(table);
				break;

			case 'r':

				printf("Are you sure you want to destroy the shared memory environment? [N/y]\n");

				fgets(input_buffer, sizeof(input_buffer), stdin);

				if(input_buffer[0] != 'y' && input_buffer[0] != 'Y') break;

				if(sem_destroy(&table->denv_sem) == -1){
					perror("sem_destroy");
					goto error;
				}
			
				if(denv_shmem_destroy(file_name) == false){
					fprintf(stderr, "Failed to destroy shared memory environment.\n");
					goto error;
				} else {
					printf("Shared memory environment destroyed successfully.\n");
				}
			
				break;

			case 't':
				denv_print_stats(table);
				break;

			case 'c':
				int err = denv_clear_freed(table);
				if(err){
					fprintf(stderr, "Failed to clear table: %i\n", err);
				}
				break;
		}
	}

	sem_post(&table->denv_sem);
	return 0;

	error:;
	sem_post(&table->denv_sem);
	return -1;
}
