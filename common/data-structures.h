#ifndef __data_structures_h__
#define __data_structures_h__

#include <stdlib.h>

#define INIT_CHAR_BUF_SIZE 256
#define INIT_VECTOR_BUF_SIZE 16
#define MAP_TABLE_SIZE 53

typedef struct CharBuf CharBuf;
typedef struct PointerVector PointerVector;
typedef struct StrMapEntry StrMapEntry;
typedef struct StrMap StrMap;

struct CharBuf {
	size_t size;
	size_t buf_size;
	char *buffer;
};

struct PointerVector {
	size_t size;
	size_t buf_size;
	void **buffer;
};

struct StrMapEntry {
	char *key;
	void *val;
	StrMapEntry *next;
};

struct StrMap {
	int size;
	StrMapEntry *table[MAP_TABLE_SIZE];
};

extern int char_buf_init(CharBuf *b);
extern int char_buf_from_file(CharBuf *b, const char *file_path);
extern int char_add_c(CharBuf *b, char c);
extern void char_buf_free(CharBuf *b);

extern int pointer_vector_init(PointerVector *pv);
extern int pointer_vector_add(PointerVector *pv, void *p);
extern void pointer_vector_free(PointerVector *pv);

extern void bob_str_map_init(StrMap *m);
extern int bob_str_map_insert(StrMap *m, char *key, void *val);
extern void *bob_str_map_get(StrMap *m, char *key);
extern void *bob_str_map_free(StrMap *m);

#endif

