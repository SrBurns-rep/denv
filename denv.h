#ifndef _DENV_H
#define _DENV_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define DENV_IPC_RESULT_ERROR (-1)

#define DENV_MAX_ELEMENTS (1 << 11) // 2048 Bytes
#define DENV_BLOCK_SIZE (1 << 20) // 1048576 Bytes

typedef uintptr_t Word;

typedef enum {
	ELEMENT_IS_USED	=		(1 << 0),
	ELEMENT_HAS_COLISION = 	(1 << 1),
	ELEMENT_IS_FREED = 		(1 << 2)
}DenvElementFlags;

typedef struct {
	Word flags;
	Word data_index;	 // block index
	Word data_word_size; // size in words
	Word colision_next;	 // get colision member
}Element;

typedef enum {
	TABLE_IS_INITIALIZED = 	(1 << 0),
	TABLE_IS_BUSY =			(1 << 1)
}DenvTableFlags;

typedef struct {
	Word flags;
	sem_t denv_sem;
	struct {
		Word used;
		Word colision_used;
		Element array[DENV_MAX_ELEMENTS];
		Element colision_array[DENV_MAX_ELEMENTS];
	}element;
	Word total_size;
	Word current_word_block_offset;
	Word block[DENV_BLOCK_SIZE];
}Table;

Word denv_hash(char *name){
    uint32_t hash = 2166136261u;
    for(size_t i = 0; i < strlen(name); i++){
        hash ^= (uint8_t)name[i];
        hash *= 16777619;
    }
    return (hash & (DENV_MAX_ELEMENTS - 1));
}

Word denv_round_to_word(size_t size){
	Word new_size = size;

	new_size--;
	new_size |= (new_size >> 1);
	new_size |= (new_size >> 2);
	new_size |= (new_size >> 4);
	new_size |= (new_size >> 8);
	new_size |= (new_size >> 16);
	if(sizeof(Word) == sizeof(uint64_t)){
		new_size |= (new_size >> 32);
	}
	new_size++;

	return new_size;
}

Table *denv_table_init(void *init_ptr){
	assert(init_ptr != NULL);

	Table *table = init_ptr;

	table->flags = 0;

	table->element.used = 0;
	table->element.colision_used = 0;

	table->total_size = sizeof(*table);

	table->current_word_block_offset = 0;

	return table;
}

void* denv_table_slice_block(Table *table, size_t size){
	assert(table != NULL);
	size = denv_round_to_word(size);

	assert(size <= DENV_BLOCK_SIZE - table->current_word_block_offset * sizeof(Word) && "Table block is out of memory.");

	void *new_slice = (void*) &table->block[table->current_word_block_offset];

	table->current_word_block_offset += size / sizeof(Word);

	return new_slice;
}

void denv_table_write_slice(void *slice_ptr, char* name, char* value){
	assert(slice_ptr != NULL && name != NULL);

	strcpy(slice_ptr, name);
	if(value != NULL) {
		size_t name_size = strlen(name) + 1;
		strcpy(slice_ptr + name_size, value);
	}
}

char *denv_get_element_name(Table *table, Word element_index){

	char *name;

	if(element_index >= DENV_MAX_ELEMENTS){
		name = (char*) &table->block[table->element.colision_array[element_index].data_index];
	} else {
		name = (char*) &table->block[table->element.array[element_index].data_index];
	}
	return name;
}


/* função que rebe ponteiro pra table, nome e valor e aloca elemento
*/

void denv_table_set_value(Table *table, char* name, char* value){
	assert(table != NULL && name != NULL);
	Word hash = denv_hash(name);
	Element *e = &table->element.array[hash];

	Word storage_size = strlen(name) + strlen(value) + 2;
	Word storage_size_in_words = denv_round_to_word(storage_size);

	if(e->flags & ELEMENT_IS_USED) {

		char *element_name = denv_get_element_name(table, hash);

		if(strcmp(name, element_name) == 0) {
			if(e->data_word_size < (storage_size_in_words / sizeof(Word))){
				// size has grown, allocate new block
				e->data_word_size = storage_size_in_words / sizeof(Word);
				e->data_index = table->current_word_block_offset;
				void *new_data = denv_table_slice_block(table, storage_size);
				denv_table_write_slice(new_data, name, value);
				return;
			} else {
				// rewrite over old data
				void *old_data = (void*) &table->block[e->data_index];
				denv_table_write_slice(old_data, name, value);
				return;
			}
			
		} else {
			if(e->flags & ELEMENT_HAS_COLISION) {
				Element *col_e = &table->element.colision_array[e->colision_next];
				char *col_element_name = (char *) &table->block[col_e->data_index];

				// loop here
				while(strcmp(name, col_element_name)){ 
					if(col_e->flags & ELEMENT_HAS_COLISION){
						col_e = &table->element.colision_array[col_e->colision_next];
						col_element_name = (char *) &table->block[col_e->data_index];
					} else {
						// element has colision now
						col_e->flags |= ELEMENT_HAS_COLISION;
						col_e->colision_next = table->element.colision_used;
						col_e = &table->element.colision_array[table->element.colision_used];
						table->element.colision_used++;

						col_e->flags |= ELEMENT_IS_USED;
						col_e->data_word_size = storage_size_in_words / sizeof(Word);
						col_e->data_index = table->current_word_block_offset;
						void *new_data = denv_table_slice_block(table, storage_size);
						denv_table_write_slice(new_data, name, value);
						return;
					}
				}
				
				if(col_e->data_word_size < (storage_size_in_words / sizeof(Word))){
					col_e->data_word_size = storage_size_in_words / sizeof(Word);
					col_e->data_index = table->current_word_block_offset;
					void *new_data = denv_table_slice_block(table, storage_size);
					denv_table_write_slice(new_data, name, value);
					return;
				} else {
					void *old_data = (void*) &table->block[col_e->data_index];
					denv_table_write_slice(old_data, name, value);
					return;
				}
	
			} else {
				// element has colision now
				e->flags |= ELEMENT_HAS_COLISION;
				e->colision_next = table->element.colision_used;
				Element *col_e = &table->element.colision_array[table->element.colision_used];
				table->element.colision_used++;
				
				col_e->flags |= ELEMENT_IS_USED;
				col_e->data_word_size = storage_size_in_words / sizeof(Word);
				col_e->data_index = table->current_word_block_offset;
				void *new_data = denv_table_slice_block(table, storage_size);
				denv_table_write_slice(new_data, name, value);
				return;				
			}
		}
		
	} else {

		e->flags |= ELEMENT_IS_USED;
		e->data_word_size = storage_size_in_words / sizeof(Word);
		e->data_index = table->current_word_block_offset;

		size_t size = storage_size;
		
		void *new_data = denv_table_slice_block(table, size);
		denv_table_write_slice(new_data, name, value);
	}
}

char *denv_table_get_value(Table *table, char* name){
	assert(table != NULL && name != NULL);
	Word hash = denv_hash(name);
	Element *e = &table->element.array[hash];

	char *element_name = denv_get_element_name(table, hash);

	char *value = NULL;

	if(e->flags & ELEMENT_IS_USED){
		if(strcmp(name, element_name) == 0){
			char *data = (char*) &table->block[e->data_index];
			value = data + strlen(data) + 1;
		} else {
			if(e->flags & ELEMENT_HAS_COLISION){
				Element *col_e = &table->element.colision_array[e->colision_next];
				char *col_element_name = (char *) &table->block[col_e->data_index];

				while(strcmp(name, col_element_name)){
					if(col_e->flags & ELEMENT_HAS_COLISION){
						col_e = &table->element.colision_array[col_e->colision_next];
						col_element_name = (char *) &table->block[col_e->data_index];
					} else {
						return NULL;
					}
				}

				if(col_e->flags & ELEMENT_IS_USED){
					char *data = (char*) &table->block[col_e->data_index];
					value = data + strlen(data) + 1;				
				}
			}
		}	
	}

	return value;
}

void denv_table_delete_value(Table *table, char *name){
	assert(table != NULL && name != NULL);
	Word hash = denv_hash(name);
	Element *e = &table->element.array[hash];

	char *element_name = denv_get_element_name(table, hash);

	if(strcmp(name, element_name) == 0){
		e->flags &= ~(ELEMENT_IS_USED);
	} else {
		if(e->flags & ELEMENT_HAS_COLISION){
			Element *col_e = &table->element.colision_array[e->colision_next];
			char *col_element_name = (char *) &table->block[col_e->data_index];

			while(strcmp(name, col_element_name)){
				if(col_e->flags & ELEMENT_HAS_COLISION){
					col_e = &table->element.colision_array[col_e->colision_next];
					col_element_name = (char *) &table->block[col_e->data_index];
				} else {
					return;
				}
			}
			col_e->flags &= ~(ELEMENT_IS_USED);
		}
	}
}

void denv_table_list_values(Table *table){
	for(int i = 0; i < DENV_MAX_ELEMENTS; i++){
		if(table->element.array[i].flags & ELEMENT_IS_USED){
			printf("%s\n", (char *) &table->block[table->element.array[i].data_index]);
		}
	}
	for(int i = 0; i < DENV_MAX_ELEMENTS; i++){
		if(table->element.colision_array[i].flags & ELEMENT_IS_USED){
			printf("%s\n", (char *) &table->block[table->element.colision_array[i].data_index]);
		}
	}
}

int denv_get_shid(char *file_name, size_t size) {
	key_t key = ftok(file_name, 'D');
	if (key == DENV_IPC_RESULT_ERROR) {
		return DENV_IPC_RESULT_ERROR;
	}

	return shmget(key, size, 0644 | IPC_CREAT);
}

void *denv_shmem_attach(char *file_name, size_t size){
	int shared_block_id = denv_get_shid(file_name, size);

	if (shared_block_id == DENV_IPC_RESULT_ERROR) {
		fprintf(stderr, "%s: %s at line %i\n", strerror(errno), __FUNCTION__, __LINE__);
		return NULL;
	}

	void *result = shmat(shared_block_id, NULL, 0);
	if(result == (char*)DENV_IPC_RESULT_ERROR) {
		fprintf(stderr, "%s: %s at line %i\n", strerror(errno), __FUNCTION__, __LINE__);
		return NULL;
	}
	return result;
}

bool denv_shmem_detach(void *attached_shmem){
	return (shmdt(attached_shmem) != DENV_IPC_RESULT_ERROR);
}

bool denv_shmem_destroy(char *filename){
	int shared_block_id = denv_get_shid(filename, 0);

	if (shared_block_id == DENV_IPC_RESULT_ERROR) {
		fprintf(stderr, "%s: %s at line %i\n", strerror(errno), __FUNCTION__, __LINE__);
		return NULL;
	}
	return (shmctl(shared_block_id, IPC_RMID, NULL) != DENV_IPC_RESULT_ERROR);
}

#endif /* _DENV_H */
