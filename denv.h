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
#include <zlib.h>
#include <time.h>
#include <unistd.h>

#define DENV_IPC_RESULT_ERROR 	(-1)

#define DENV_MAX_ELEMENTS 		(1 << 11) // 2048 Bytes
#define DENV_BLOCK_SIZE 		(1 << 20) // 1048576 Bytes

#define DENV_MAJOR_VERSION 		1
#define DENV_MINOR_VERSION 		0
#define DENV_FIX_VERSION 		1

#if UINTPTR_WIDTH == 8
#define DENV_MAGIC 				0x44454e5600000000ULL
#else
#define DENV_MAGIC 				0x44454e56
#endif


#define DENV_CHUNK 				(1 << 19) // 512KiB

#define DENV_ENV_BUFFER			(1 << 11) // 2048 Bytes

#define DENV_COMPRESSION_LEVEL 	Z_DEFAULT_COMPRESSION

#if !defined(DENV_VERSION_A) || !defined(DENV_VERSION_B) || !defined(DENV_VERSION_C)
#error "Missing version defines."
#endif

typedef uintptr_t Word;

typedef enum {
	ELEMENT_IS_USED			= (1 << 0),
	ELEMENT_HAS_COLLISION	= (1 << 1),
	ELEMENT_IS_FREED		= (1 << 2),
	ELEMENT_IS_ENV			= (1 << 3),
	ELEMENT_IS_BEING_READ	= (1 << 4),
	ELEMENT_IS_UPDATED		= (1 << 5)
}DenvElementFlags;

typedef struct {
	Word flags;
	Word data_index;	 	// block index
	Word data_word_size; 	// size in words
	Word collision_next;	// get collision member
}Element;

typedef enum {
	TABLE_IS_INITIALIZED = 	(1 << 0),
	TABLE_IS_BUSY =			(1 << 1)
}DenvTableFlags;

typedef struct {
	Word magic;
	Word flags;
	sem_t denv_sem;
	struct {
		Word used;
		Word collision_used;
		Element array[DENV_MAX_ELEMENTS];
		Element collision_array[DENV_MAX_ELEMENTS];
	}element;
	Word total_size;
	Word current_word_block_offset;
	Word block[DENV_BLOCK_SIZE];
}Table;

typedef struct {
	uint8_t *data;
	Word size;
}Buffer;

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
#if UINTPTR_WIDTH == 8
		new_size |= (new_size >> 32);
#endif
	new_size++;

	return new_size;
}

Table *denv_table_init(void *init_ptr){
	assert(init_ptr != NULL);

	Table *table = init_ptr;

	table->flags |= TABLE_IS_INITIALIZED;

	table->magic = DENV_MAGIC;

	table->element.used = 0;
	table->element.collision_used = 0;

	table->total_size = sizeof(Table);

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

void denv_table_write_slice(void *slice_ptr, char *name, char * value){
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
		name = (char*) &table->block[table->element.collision_array[element_index & (DENV_MAX_ELEMENTS - 1)].data_index];
	} else {
		name = (char*) &table->block[table->element.array[element_index & (DENV_MAX_ELEMENTS - 1)].data_index];
	}
	return name;
}

/*
TODO:
Functions to implement to simplify denv_table_set_value
denv_write_element_data
denv_realloc_element_data
denv_handle_collisions
denv_element_has_collision
*/
/* Function that receives table, variable name and value and allocates the element,
   it also edits the element if the name match
   it also do collision handling
*/
void denv_table_set_value(Table *table, char* name, char* value, Word flags){
	assert(table != NULL && name != NULL);

	sem_wait(&table->denv_sem);
	
	Word hash = denv_hash(name);
	Element *e = &table->element.array[hash];

	Word storage_size = strlen(name) + strlen(value) + 2;
	Word storage_size_in_words = denv_round_to_word(storage_size);

	// Do not let external flags mess up with crucial flags
	flags &= ~(ELEMENT_IS_USED | ELEMENT_IS_BEING_READ | ELEMENT_HAS_COLLISION | ELEMENT_IS_FREED | ELEMENT_IS_UPDATED);

	if(e->flags & ELEMENT_IS_USED) {

		char *element_name = denv_get_element_name(table, hash);

		if(strcmp(name, element_name) == 0) {
			if(e->data_word_size < (storage_size_in_words / sizeof(Word))){
				// size has grown, allocate new block
				e->data_word_size = storage_size_in_words / sizeof(Word);
				e->data_index = table->current_word_block_offset;
				void *new_data = denv_table_slice_block(table, storage_size);
				denv_table_write_slice(new_data, name, value);

				e->flags |= flags | ELEMENT_IS_UPDATED;
				e->flags &= ~(ELEMENT_IS_FREED);

				sem_post(&table->denv_sem);
				return;
			} else {
				// rewrite over old data
				void *old_data = (void*) &table->block[e->data_index];
				denv_table_write_slice(old_data, name, value);

				e->flags |= flags | ELEMENT_IS_UPDATED;
				e->flags &= ~(ELEMENT_IS_FREED);
				
				sem_post(&table->denv_sem);
				return;
			}

		} else {
		
			if(e->flags & ELEMENT_HAS_COLLISION) {
				//check all palces with ->collision_next
				Element *col_e = &table->element.collision_array[e->collision_next & (DENV_MAX_ELEMENTS - 1)];
				char *col_element_name = (char *) &table->block[col_e->data_index];

				// loop here
				while(strcmp(name, col_element_name)){ 
					if(col_e->flags & ELEMENT_HAS_COLLISION){
						col_e = &table->element.collision_array[col_e->collision_next];
						col_element_name = (char *) &table->block[col_e->data_index];
					} else {
						// element has collision now
						col_e->flags |= ELEMENT_HAS_COLLISION;
						col_e->collision_next = table->element.collision_used;
						col_e = &table->element.collision_array[table->element.collision_used];
						table->element.collision_used++;

						col_e->flags |= ELEMENT_IS_USED;
						col_e->data_word_size = storage_size_in_words / sizeof(Word);
						col_e->data_index = table->current_word_block_offset;
						void *new_data = denv_table_slice_block(table, storage_size);
						denv_table_write_slice(new_data, name, value);

						col_e->flags |= flags | ELEMENT_IS_UPDATED;
						col_e->flags &= ~(ELEMENT_IS_FREED);

						sem_post(&table->denv_sem);
						return;
					}
				}
				
				if(col_e->data_word_size < (storage_size_in_words / sizeof(Word))){
					col_e->data_word_size = storage_size_in_words / sizeof(Word);
					col_e->data_index = table->current_word_block_offset;
					void *new_data = denv_table_slice_block(table, storage_size);
					denv_table_write_slice(new_data, name, value);

					col_e->flags |= flags | ELEMENT_IS_UPDATED;
					col_e->flags &= ~(ELEMENT_IS_FREED);

					sem_post(&table->denv_sem);
					return;
				} else {
					void *old_data = (void*) &table->block[col_e->data_index];
					denv_table_write_slice(old_data, name, value);

					col_e->flags |= flags | ELEMENT_IS_UPDATED;
					col_e->flags &= ~(ELEMENT_IS_FREED);

					sem_post(&table->denv_sem);
					return;
				}
	
			} else {
				// element has collision now
				e->flags |= ELEMENT_HAS_COLLISION;
				e->collision_next = table->element.collision_used;
				Element *col_e = &table->element.collision_array[table->element.collision_used];

				assert(table->element.collision_used < DENV_MAX_ELEMENTS);
				
				table->element.collision_used++;
				
				col_e->flags |= ELEMENT_IS_USED;
				col_e->data_word_size = storage_size_in_words / sizeof(Word);
				col_e->data_index = table->current_word_block_offset;
				void *new_data = denv_table_slice_block(table, storage_size);
				denv_table_write_slice(new_data, name, value);

				col_e->flags |= flags | ELEMENT_IS_UPDATED;
				col_e->flags &= ~(ELEMENT_IS_FREED);

				sem_post(&table->denv_sem);
				return;				
			}
		}
		
	} else {

		e->flags |= ELEMENT_IS_USED;
		e->data_word_size = storage_size_in_words / sizeof(Word);
		e->data_index = table->current_word_block_offset;

		table->element.used++;

		size_t size = storage_size;
		
		void *new_data = denv_table_slice_block(table, size);
		denv_table_write_slice(new_data, name, value);

		e->flags |= flags | ELEMENT_IS_UPDATED;
		e->flags &= ~(ELEMENT_IS_FREED);

		sem_post(&table->denv_sem);
	}
}

char *_denv_table_get_value(Table *table, char* name){
	assert(table != NULL && name != NULL);
	
	Word hash = denv_hash(name);
	Element *e = &table->element.array[hash];

	char *element_name = denv_get_element_name(table, hash);

	char *value = NULL;

	if(e->flags & ELEMENT_IS_USED){
		if(strcmp(name, element_name) == 0){

			if(e->flags & ELEMENT_IS_FREED) return NULL;
		
			char *data = (char*) &table->block[e->data_index];
			value = data + strlen(data) + 1;
		} else {
			if(e->flags & ELEMENT_HAS_COLLISION){
				Element *col_e = &table->element.collision_array[e->collision_next];
				char *col_element_name = (char *) &table->block[col_e->data_index];

				while(strcmp(name, col_element_name)){
					if(col_e->flags & ELEMENT_HAS_COLLISION){
						col_e = &table->element.collision_array[col_e->collision_next];
						col_element_name = (char *) &table->block[col_e->data_index];
					} else {
						//debug if
						if(col_e->flags != 0){
							fprintf(stderr, "%s: error in get loop.\n", __FUNCTION__);
						}
						return NULL;
					}
				}

				if(col_e->flags & ELEMENT_IS_FREED) return NULL;

				if(col_e->flags & ELEMENT_IS_USED){
					char *data = (char*) &table->block[col_e->data_index];
					value = data + strlen(data) + 1;				
				}
			}
		}	
	}

	return value;
}

char *denv_table_get_value(Table *table, char* name) {
	assert((table != NULL) && (name != NULL));

	sem_wait(&table->denv_sem);

	char *aux = _denv_table_get_value(table, name);

	sem_post(&table->denv_sem);

	return aux;
}

Element *denv_table_get_element(Table *table, char *name){
	assert(table != NULL && name != NULL);
	
	Word hash = denv_hash(name);
	Element *e = &table->element.array[hash];

	char *element_name = denv_get_element_name(table, hash);

	if(e->flags & ELEMENT_IS_USED){
		if(strcmp(name, element_name) == 0){

			if(e->flags & ELEMENT_IS_FREED) return NULL;

			return e;
			
		} else {
			if(e->flags & ELEMENT_HAS_COLLISION){
				Element *col_e = &table->element.collision_array[e->collision_next];
				char *col_element_name = (char *) &table->block[col_e->data_index];

				while(strcmp(name, col_element_name)){
					if(col_e->flags & ELEMENT_HAS_COLLISION){
						col_e = &table->element.collision_array[col_e->collision_next];
						col_element_name = (char *) &table->block[col_e->data_index];
					} else {
						//debug if
						if(col_e->flags != 0){
							fprintf(stderr, "%s: error in get loop.\n", __FUNCTION__);
						}
						return NULL;
					}
				}

				if(col_e->flags & ELEMENT_IS_FREED) return NULL;

				if(col_e->flags & ELEMENT_IS_USED){
					return col_e;				
				}
			}
		}	
	}

	return NULL;	
}

bool denv_element_on_update(Table *table, Element *element) {
	
	if(element->flags & ELEMENT_IS_UPDATED) {
		sem_wait(&table->denv_sem);
		element->flags &= ~(ELEMENT_IS_UPDATED);
		sem_post(&table->denv_sem);
		return true;
	}

	return false;
}

// Doing polling is considered bad, I'm looking for a better way to do this
bool denv_await_element(Table *table, char *name, time_t nsec){
	Element *e = denv_table_get_element(table, name);

    if(e == NULL) return false;
	
	struct timespec ts = {};
	ts.tv_sec = 0;
	ts.tv_nsec = nsec;

	while (e == NULL) {
		nanosleep(&ts, NULL);
		e = denv_table_get_element(table, name);
	}
	
	while (denv_element_on_update(table, e) == false){
		nanosleep(&ts, NULL);
	}

    return true;
}

void denv_table_delete_value(Table *table, char *name){
	assert(table != NULL && name != NULL);

	sem_wait(&table->denv_sem);
	
	Word hash = denv_hash(name);
	Element *e = &table->element.array[hash];

	char *element_name = denv_get_element_name(table, hash);

	if(strcmp(name, element_name) == 0){
		e->flags |= ELEMENT_IS_FREED;

		table->element.used--;
		
	} else {
		if(e->flags & ELEMENT_HAS_COLLISION){
			Element *col_e = &table->element.collision_array[e->collision_next & (DENV_MAX_ELEMENTS - 1)];
			char *col_element_name = (char *) &table->block[col_e->data_index];

			while(strcmp(name, col_element_name)){
				if(col_e->flags & ELEMENT_HAS_COLLISION){
					col_e = &table->element.collision_array[col_e->collision_next];
					col_element_name = (char *) &table->block[col_e->data_index];
				} else {
					break; //return;
				}
			}
			col_e->flags |= ELEMENT_IS_FREED;
		}
	}

	sem_post(&table->denv_sem);
}

void denv_table_list_values(Table *table, bool list_env) {
	for(int i = 0; i < DENV_MAX_ELEMENTS; i++){

		Word flags = table->element.array[i].flags;

		if((flags & (ELEMENT_IS_USED | ELEMENT_IS_FREED)) == ELEMENT_IS_USED) {

			if(list_env && (flags & ELEMENT_IS_ENV)) {
				printf("%-20s (ENV)\n", (char *) &table->block[table->element.array[i].data_index]);
			} else {
				printf("%s\n", (char *) &table->block[table->element.array[i].data_index]);
			}
		
		}
		
		Word col_flags = table->element.collision_array[i].flags;
		
		if((col_flags & (ELEMENT_IS_USED | ELEMENT_IS_FREED)) == ELEMENT_IS_USED) {

			if(list_env && (col_flags & ELEMENT_IS_ENV)) {
				printf("%-20s (ENV)\n", (char *) &table->block[table->element.collision_array[i].data_index]);
			} else {
				printf("%s\n", (char *) &table->block[table->element.collision_array[i].data_index]);
			}
		
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
		return false;
	}
	return (shmctl(shared_block_id, IPC_RMID, NULL) != DENV_IPC_RESULT_ERROR);
}

void denv_print_version(void){

    // disciminator for compiled versions on the same day
	int disc = denv_hash(__DATE__ __TIME__);

	disc ^= 9733; // xor to generate a bigger number

#ifdef DEBUG_ON
	printf("denv %d.%d.%d.%04d (DEBUG)\n", DENV_VERSION_A, DENV_VERSION_B, DENV_VERSION_C, disc);
	return;
#endif
	printf("denv %d.%d.%d.%04d\n", DENV_VERSION_A, DENV_VERSION_B, DENV_VERSION_C, disc);
}

void denv_print_stats_csv(Table *table){
	Word used = table->element.used;
	Word col_used = table->element.collision_used;
	Word total = used + col_used;

	printf(
		"total_size_bytes,data_offset,used_hash,used_collision,used_total\n"
		"%lu,%lu,%lu,%lu,%lu\n",
		 table->total_size,
		 table->current_word_block_offset,
		 used,
		 col_used,
		 total
	);
}

int denv_clear_freed(Table *table){
	Table *clean_table = malloc(table->total_size);
	if(!clean_table){
		fprintf(stderr, "%s: Could not allocate memory to clean the table\n", __FUNCTION__);
		return -1;
	}

	// Initialize the clean table with the source table attributes
	clean_table->magic = table->magic;
	clean_table->flags = table->flags;
	clean_table->denv_sem = table->denv_sem;
	clean_table->element.used = 0;
	clean_table->element.collision_used = 0;
	clean_table->total_size = table->total_size;
	clean_table->current_word_block_offset = 0;

	for(int i = 0; i < DENV_MAX_ELEMENTS; i++){
		
		Element *e = &table->element.array[i];
		Element *coll_e = &table->element.collision_array[i];
		
		if((e->flags & (ELEMENT_IS_USED | ELEMENT_IS_FREED)) == ELEMENT_IS_USED){
			char *name = (char*)&table->block[e->data_index];
			char *value = denv_table_get_value(table, name);
			if(value != NULL) {
				denv_table_set_value(clean_table, name, value, e->flags);
			}
		}
		if((coll_e->flags & (ELEMENT_IS_USED | ELEMENT_IS_FREED)) == ELEMENT_IS_USED){
			char *name = (char*)&table->block[coll_e->data_index];
			char *value = denv_table_get_value(table, name);
			if(value != NULL) {
				denv_table_set_value(clean_table, name, value, coll_e->flags);
			}
		}
	}

	memcpy(table, clean_table, table->total_size);

	free(clean_table);
	
	return 0;
}

int denv_compress(FILE *source, FILE *dest, int level)
{
    int ret, flush;
    unsigned have;
    z_stream strm;
    uint8_t *in = malloc(DENV_CHUNK);
    uint8_t *out = malloc(DENV_CHUNK);
    if(!in || !out) {
    	fprintf(stderr, "%s: Failed to allocate buffers\n", __FUNCTION__);
    	return -1;
    }

    /* allocate deflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = deflateInit(&strm, level);
    if (ret != Z_OK)
        return ret;

    /* compress until end of file */
    do {
        strm.avail_in = fread(in, 1, DENV_CHUNK, source);
        if (ferror(source)) {
            (void)deflateEnd(&strm);
            return Z_ERRNO;
        }
        flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
        strm.next_in = in;

        /* run deflate() on input until output buffer not full, finish
           compression if all of source has been read in */
        do {
            strm.avail_out = DENV_CHUNK;
            strm.next_out = out;
            ret = deflate(&strm, flush);    /* no bad return value */
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            have = DENV_CHUNK - strm.avail_out;
            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                (void)deflateEnd(&strm);
                return Z_ERRNO;
            }
        } while (strm.avail_out == 0);
        assert(strm.avail_in == 0);     /* all input will be used */

        /* done when last data in file processed */
    } while (flush != Z_FINISH);
    assert(ret == Z_STREAM_END);        /* stream will be complete */

    /* clean up and return */
    (void)deflateEnd(&strm);

	free(in);
	free(out);
    
    return Z_OK;
}

int denv_decompress(FILE *source, FILE *dest)
{
    int ret;
    unsigned have;
    z_stream strm;
    uint8_t *in = malloc(DENV_CHUNK);
    uint8_t *out = malloc(DENV_CHUNK);
    if(!in || !out) {
    	fprintf(stderr, "%s: Failed to allocate buffers\n", __FUNCTION__);
    	return -1;
    }

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit(&strm);
    if (ret != Z_OK)
        return ret;

    /* decompress until deflate stream ends or end of file */
    do {
        strm.avail_in = fread(in, 1, DENV_CHUNK, source);
        if (ferror(source)) {
            (void)inflateEnd(&strm);
            return Z_ERRNO;
        }
        if (strm.avail_in == 0)
            break;
        strm.next_in = in;

        /* run inflate() on input until output buffer not full */
        do {
            strm.avail_out = DENV_CHUNK;
            strm.next_out = out;
            ret = inflate(&strm, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            switch (ret) {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;     /* and fall through */
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                (void)inflateEnd(&strm);
                return ret;
            }
            have = DENV_CHUNK - strm.avail_out;
            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                (void)inflateEnd(&strm);
                return Z_ERRNO;
            }
        } while (strm.avail_out == 0);

        /* done when inflate() says it's done */
    } while (ret != Z_STREAM_END);

    /* clean up and return */
    (void)inflateEnd(&strm);

	free(in);
	free(out);
    
    return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

int denv_save_to_file(Table *table, char *pathname){
	assert(table && pathname);

	sem_post(&table->denv_sem); // must do this to avoid blocking the table!!

	FILE *table_file = fmemopen(table, table->total_size, "r");
	if(ferror(table_file)){
		perror("fmemopen");
		return -1;
	}

	FILE *dst_file = fopen(pathname, "w");
	if(ferror(dst_file)){
		perror("fopen");
		return -1;
	}

	int ret = denv_compress(table_file, dst_file, DENV_COMPRESSION_LEVEL);
	if(ret != Z_OK){
		fprintf(stderr, "%s: Failed to compress table.\n", __FUNCTION__);
	}
	fclose(table_file);
	fclose(dst_file);

	if(ret != Z_OK) return -1;

	return 0;
}

Table *denv_load_from_file(Table *table, char *pathname){
	assert(table && pathname);

	FILE *table_file = fmemopen(table, table->total_size, "w");
	if(ferror(table_file)){
		perror("fmemopen");
		return NULL;
	}

	FILE *src_file = fopen(pathname, "r");
	if(!src_file){
		perror("fopen");
		fclose(table_file);

		sem_post(&table->denv_sem); // must do this to avoid blocking the table
		
		return NULL;
	}

	int ret = denv_decompress(src_file, table_file);
	if(ret != Z_OK){
		fprintf(stderr, "%s: Failed to decompress table.\n", __FUNCTION__);

		fclose(table_file);
		fclose(src_file);
		return NULL;
	}

	fclose(table_file);
	fclose(src_file);
	return table;
}

int denv_exec(Table *table, char *program_path, char **argv) {

	sem_wait(&table->denv_sem);

	for(int i = 0; i < DENV_MAX_ELEMENTS; i++) {
		Element *e = &table->element.array[i];
		Element *coll_e = &table->element.collision_array[i];
		
		if((e->flags & (ELEMENT_IS_USED | ELEMENT_IS_FREED | ELEMENT_IS_ENV)) == (ELEMENT_IS_USED | ELEMENT_IS_ENV)) {
			char *name = (char*)&table->block[e->data_index];
			char *value = denv_table_get_value(table, name);
			
			if(name[0] && value) {
				if(setenv(name, value, 1) != 0) {
					perror("setenv");
					return -1;
				}
			}
		}

		if((coll_e->flags & (ELEMENT_IS_USED | ELEMENT_IS_FREED | ELEMENT_IS_ENV)) == (ELEMENT_IS_USED | ELEMENT_IS_ENV)) {
			char *name = (char*)&table->block[coll_e->data_index];
			char *value = denv_table_get_value(table, name);
			
			if(name[0] && value) {
				if(setenv(name, value, 1) != 0) {
					perror("setenv");
					return -1;
				}
			}
		}
	}

	sem_post(&table->denv_sem);
	
	if(execvp(program_path, argv) == -1) {
		perror("execvp");
		return -1;
	}
	return 0;
}

int denv_clone_env(Table *table, char **envp) {
	assert(envp && envp[0]);
	// calculate env size
	size_t env_size = 0;
	size_t env_lines = 0;

	Word used = table->element.used;
	Word col_used = table->element.collision_used;

	for(int i = 0; envp[i]; i++) {
		env_size += strlen(envp[i]);
		env_lines++;
	}

	if (env_lines > ((2 * DENV_MAX_ELEMENTS) - (used + col_used))) {
		fprintf(stderr, "Not enough space to store environment variables.\n");
		return -1;
	}

	if (env_size > ((DENV_BLOCK_SIZE - table->current_word_block_offset) * (sizeof(Word)))) {
		fprintf(stderr, "Not enough space to store environment variables.\n");
		return -1;
	}

	char *name = NULL;
	char *value = NULL;

	for(int i = 0; envp[i]; i++) {
		name = &envp[i][0];
		for(size_t j = 0; j <= strlen(envp[i]); j++) {
			if(envp[i][j] == '=') {
				envp[i][j] = '\0';
				if(envp[i][j + 1]) {
					value = &envp[i][j + 1];
					denv_table_set_value(table, name, value, ELEMENT_IS_ENV);
				}
				break;
			}
		}
	}
	return 0;
}
// Function to make a text file for using with the "source" bash command
void denv_make_env_save_file(Table *table, FILE *file) {

	assert((table != NULL) && (file != NULL));

	for(int i = 0; i < DENV_MAX_ELEMENTS; i++) {
		Element *e = &table->element.array[i];
		Element *coll_e = &table->element.collision_array[i];
		
		if((e->flags & (ELEMENT_IS_USED | ELEMENT_IS_FREED | ELEMENT_IS_ENV)) == (ELEMENT_IS_USED | ELEMENT_IS_ENV)) {
			char *name = (char*)&table->block[e->data_index];
			char *value = denv_table_get_value(table, name);
			
			if(name[0] && value) {
				fprintf(file, "export %s=%s\n", name, value);
			}
		}

		if((coll_e->flags & (ELEMENT_IS_USED | ELEMENT_IS_FREED | ELEMENT_IS_ENV)) == (ELEMENT_IS_USED | ELEMENT_IS_ENV)) {
			char *name = (char*)&table->block[coll_e->data_index];
			char *value = denv_table_get_value(table, name);
			
			if(name[0] && value) {
				fprintf(file, "export %s=%s\n", name, value);
			}
		}
	}
}

#endif /* _DENV_H */
