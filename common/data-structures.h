#ifndef __data_structures_h__
#define __data_structures_h__

#include <GL/glew.h>
#include <stdlib.h>

#define INIT_CHAR_BUF_SIZE 256
#define INIT_FLOAT_BUF_SIZE 128
#define INIT_VECTOR_BUF_SIZE 16
#define MAP_TABLE_SIZE 53

typedef struct CharBuf CharBuf;
typedef struct FloatBuf FloatBuf;
typedef struct PointerVector PointerVector;
typedef struct PointerList PointerList;
typedef struct StrMapEntry StrMapEntry;
typedef struct StrMap StrMap;
typedef struct IntMapEntry IntMapEntry;
typedef struct IntMap IntMap;

struct CharBuf {
	size_t size;
	size_t buf_size;
	char *buffer;
};

struct FloatBuf {
  size_t size;
  size_t buf_size;
  GLfloat *buffer;
};

struct PointerVector {
	size_t size;
	size_t buf_size;
	void **buffer;
};

struct PointerList {
  void *ptr;
  PointerList *next;
};

struct StrMapEntry {
	const char *key;
	void *val;
	StrMapEntry *next;
};

struct StrMap {
	int size;
	StrMapEntry *table[MAP_TABLE_SIZE];
};

struct IntMapEntry {
  int key;
  void *val;
  IntMapEntry *next;
};

struct IntMap {
  int size;
  IntMapEntry *table[MAP_TABLE_SIZE];
};

extern int char_buf_init(CharBuf *b);
extern int char_buf_from_file(CharBuf *b, const char *file_path);
extern int char_add_c(CharBuf *b, char c);
extern int char_add_s(CharBuf *b, const char *s);
extern int char_add_i(CharBuf *b, int i);
extern int char_add_d(CharBuf *b, double d);
extern char char_popback_c(CharBuf *b);
extern void char_buf_free(CharBuf *b);

extern int float_buf_init(FloatBuf *b);
extern int float_add_f(FloatBuf *b, GLfloat f);
extern void float_buf_free(FloatBuf *b);

extern int pointer_vector_init(PointerVector *pv);
extern int pointer_vector_add(PointerVector *pv, void *p);
extern int pointer_vector_add_if_not_exists(PointerVector *pv, void *p);
extern void pointer_vector_free(PointerVector *pv);

extern void bob_str_map_init(StrMap *m);
extern int bob_str_map_insert(StrMap *m, const char *key, void *val);
extern int bob_str_map_update(StrMap *m, const char *key, void *val);
extern void *bob_str_map_get(StrMap *m, const char *key);
extern void *bob_str_map_free(StrMap *m);

extern void bob_int_map_init(IntMap *m);
extern int bob_int_map_insert(IntMap *m, int key, void *val);
extern int bob_int_map_update(IntMap *m, int key, void *val);
extern void *bob_int_map_get(IntMap *m, int key);
extern void *bob_int_map_free(IntMap *m);

extern CharBuf pad_quotes(const char *src);

#endif

