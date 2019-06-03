#ifndef __data_structures_h__
#define __data_structures_h__

#include <stdlib.h>

#define INIT_CHAR_BUF_SIZE 256
#define MAP_TABLE_SIZE 53

typedef struct CharBuf CharBuf;
typedef struct BobStrMapEntry BobStrMapEntry;
typedef struct BobStrMap BobStrMap;

struct CharBuf {
	size_t size;
	size_t buf_size;
	char *buffer;
};

struct BobStrMapEntry {
	char *key;
	void *val;
	BobStrMapEntry *next;
};

struct BobStrMap {
	int size;
	BobStrMapEntry *table[MAP_TABLE_SIZE];
};

extern int char_buf_init(CharBuf *b);
extern int char_buf_from_file(CharBuf *b, const char *file_path);
extern int char_add_c(CharBuf *b, char c);
extern int char_buf_free(CharBuf *b);

extern void bob_str_map_init(BobStrMap *m);
extern int bob_str_map_insert(BobStrMap *m, char *key, void *val);
extern void *bob_str_map_get(BobStrMap *m, char *key);
extern void *bob_str_map_free(BobStrMap *m);

#endif

