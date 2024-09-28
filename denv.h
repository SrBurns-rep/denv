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

#define DENV_IPC_RESULT_ERROR (-1)

#define DENV_MAX_ELEMENTS (1 << 11) // 2048 Bytes
#define DENV_BLOCK_SIZE (1 << 20) // 1048576 Bytes

#define DENV_MAJOR_VERSION 0
#define DENV_MINOR_VERSION 9
#define DENV_FIX_VERSION 1

#define DENV_MAGIC 0x44454e5600000000ULL

#define DENV_CHUNK (1 << 19) //512KiB

#define DENV_COMPRESSION_LEVEL Z_DEFAULT_COMPRESSION

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
	Word magic; // added it now
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
	if(sizeof(Word) == sizeof(uint64_t)){
		new_size |= (new_size >> 32);
	}
	new_size++;

	return new_size;
}

Table *denv_table_init(void *init_ptr){
	assert(init_ptr != NULL);

	Table *table = init_ptr;

	table->flags |= TABLE_IS_INITIALIZED;

	table->magic = DENV_MAGIC;

	table->element.used = 0;
	table->element.colision_used = 0;

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
		name = (char*) &table->block[table->element.colision_array[element_index].data_index];
	} else {
		name = (char*) &table->block[table->element.array[element_index].data_index];
	}
	return name;
}


/* Function that receives table, variable name and value and allocates the element,
   it also edits the element if the name match
   it also do collision handling
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
				//check all palces with ->colision_next
				Element *col_e = &table->element.colision_array[e->colision_next & (DENV_MAX_ELEMENTS - 1)];
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

				assert(table->element.colision_used < DENV_MAX_ELEMENTS);
				
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

		table->element.used++;

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

			if(e->flags & ELEMENT_IS_FREED) return NULL;
		
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

				if(col_e->flags & ELEMENT_IS_FREED) return NULL;

				if(col_e->flags & ELEMENT_IS_USED){
					char *data = (char*) &table->block[col_e->data_index];
					value = data + strlen(data) + 1;				
				}
			}
		}	
	}

	assert(value); // it should return a value by here

	return value;
}

void denv_table_delete_value(Table *table, char *name){
	assert(table != NULL && name != NULL);
	Word hash = denv_hash(name);
	Element *e = &table->element.array[hash];

	char *element_name = denv_get_element_name(table, hash);

	if(strcmp(name, element_name) == 0){
		e->flags |= ELEMENT_IS_FREED;

		table->element.used--;
		
	} else {
		if(e->flags & ELEMENT_HAS_COLISION){
			Element *col_e = &table->element.colision_array[e->colision_next & (DENV_MAX_ELEMENTS - 1)];
			char *col_element_name = (char *) &table->block[col_e->data_index];

			while(strcmp(name, col_element_name)){
				if(col_e->flags & ELEMENT_HAS_COLISION){
					col_e = &table->element.colision_array[col_e->colision_next];
					col_element_name = (char *) &table->block[col_e->data_index];
				} else {
					break; //return;
				}
			}
			col_e->flags |= ELEMENT_IS_FREED;
		}
	}
}

void denv_table_list_values(Table *table) {
	for(int i = 0; i < DENV_MAX_ELEMENTS; i++){

		Word flags = table->element.array[i].flags;

		if((flags & (ELEMENT_IS_USED | ELEMENT_IS_FREED)) == ELEMENT_IS_USED) {
			printf("%s\n", (char *) &table->block[table->element.array[i].data_index]);
		}
		
		Word col_flags = table->element.colision_array[i].flags;
		
		if((col_flags & (ELEMENT_IS_USED | ELEMENT_IS_FREED)) == ELEMENT_IS_USED) {
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

void denv_print_version(void){
	struct {
		int a;
		int b;
		int c;
		int d;
	}version;

	version.a = DENV_MAJOR_VERSION;
	version.b = DENV_MINOR_VERSION;
	version.c = DENV_FIX_VERSION;

	char *date_time = __DATE__ __TIME__;

	version.d = denv_hash(date_time);	// disciminator for compiled versions on the same day

	printf("denv %d.%d.%d.%d\n", version.a, version.b, version.c, version.d);
}

void denv_print_stats(Table *table){
	printf(
		"Table total size:                     %lu bytes\n"
		"Current data block offset:            %lu\n"
		"Number of elements on hash table:     %lu\n"
		"Number of elements on colision table: %lu\n",
		 table->total_size,
		 table->current_word_block_offset,
		 table->element.used,
		 table->element.colision_used
	);
}

int denv_clear_freed(Table *table){
	Table *clean_table = malloc(table->total_size);
	if(!clean_table){
		fprintf(stderr, "%s: Could not allocate memory to clean the table\n", __FUNCTION__);
		return -1;
	}

	clean_table->flags = table->flags;
	clean_table->denv_sem = table->denv_sem;
	clean_table->element.used = 0;
	clean_table->element.colision_used = 0;
	clean_table->total_size = table->total_size;
	clean_table->current_word_block_offset = 0;

	// this section has room for optimizations
	for(int i = 0; i < DENV_MAX_ELEMENTS; i++){
		
		Element *e = &table->element.array[i];
		Element *coll_e = &table->element.colision_array[i];
		
		if((e->flags & (ELEMENT_IS_USED | ELEMENT_IS_FREED)) == ELEMENT_IS_USED){
			char *name = (char*)&table->block[e->data_index];
			char *value = denv_table_get_value(table, name);
			if(!value) break;
			denv_table_set_value(clean_table, name, value);
		}
		if((coll_e->flags & (ELEMENT_IS_USED | ELEMENT_IS_FREED)) == ELEMENT_IS_USED){
			char *name = (char*)&table->block[coll_e->data_index];
			char *value = denv_table_get_value(table, name);
			if(!value) break;
			denv_table_set_value(clean_table, name, value);
		}
	}

	size_t table_size = table->total_size;

	memcpy(table, clean_table, table_size);

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

void denv_save_to_file(Table *table, char *pathname){
	assert(table && pathname);

	sem_post(&table->denv_sem); // must do this to avoid blocking the table!!

	FILE *table_file = fmemopen(table, table->total_size, "r");
	if(ferror(table_file)){
		perror("fmemopen");
		return;
	}

	FILE *dst_file = fopen(pathname, "w");
	if(ferror(dst_file)){
		perror("fopen");
		return;
	}

	int ret = denv_compress(table_file, dst_file, DENV_COMPRESSION_LEVEL);
	if(ret != Z_OK){
		fprintf(stderr, "%s: Failed to compress table.\n", __FUNCTION__);
	}
	fclose(table_file);
	fclose(dst_file);
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

/* TODO:
int denv_expand_table(Table *table){
	
}


int denv_load_config_file(char *filename){
	

}
*/

#endif /* _DENV_H */
