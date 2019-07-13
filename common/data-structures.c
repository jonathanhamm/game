#include "data-structures.h"
#include "errcodes.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned pjw_hash(const char *key);

int char_buf_init(CharBuf *b) {
	b->size = 0;
	b->buf_size = INIT_CHAR_BUF_SIZE;
	
	void *buffer = malloc(INIT_CHAR_BUF_SIZE);
	if (!buffer)
		return STATUS_OUT_OF_MEMORY;
	b->buffer = buffer;

	return STATUS_OK;
}

int char_buf_from_file(CharBuf *b, const char *file_path) {
	FILE *f;	
	int c, result;
	
	f = fopen(file_path, "r");
	if (!f)
		return STATUS_FILE_IO_ERR;

	result = char_buf_init(b);
	if (result) {
		fclose(f);
		return result;
	}

	while ((c = fgetc(f)) != EOF) {
		result = char_add_c(b, c);
		if (result) {
			fclose(f);
			char_buf_free(b);
			return result;
		}
	}
	result = char_add_c(b, '\0');
	if (result) 
		char_buf_free(b);
	fclose(f);
	return result;
}

int char_add_c(CharBuf *b, char c) {
	size_t buf_size = b->buf_size;
	char *buffer = b->buffer;

	if (b->size == buf_size) {
		buf_size *= 2;
		buffer = realloc(buffer, buf_size);
		if (!buffer)
			return STATUS_OUT_OF_MEMORY;
		b->buf_size = buf_size;
		b->buffer = buffer;
	}
	buffer[b->size++] = c; 
	return STATUS_OK;
}

void char_buf_free(CharBuf *b) {
	free(b->buffer);
	b->buffer = NULL;
}

int pointer_vector_init(PointerVector *vp) {
	vp->size = 0;
	vp->buf_size = INIT_VECTOR_BUF_SIZE;
	
	void **buffer = malloc(INIT_VECTOR_BUF_SIZE * sizeof(*buffer));
	if (!buffer)
		return STATUS_OUT_OF_MEMORY;
	vp->buffer = buffer;

	return STATUS_OK;
}

int pointer_vector_add(PointerVector *vp, void *p) {
	size_t buf_size = vp->buf_size;
	void **buffer = vp->buffer;

	if (vp->size == buf_size) {
		buf_size *= 2;
		buffer = realloc(buffer, buf_size * sizeof(*buffer));
		if (!buffer)
			return STATUS_OUT_OF_MEMORY;
		vp->buf_size = buf_size;
		vp->buffer = buffer;
	}
	buffer[vp->size++] = p;
	return STATUS_OK;
}

void pointer_vector_free(PointerVector *vp) {
	free(vp->buffer);
	vp->buffer = NULL;
}

void bob_str_map_init(StrMap *m) {
	int i;

	m->size = 0;
	for (i = 0; i < MAP_TABLE_SIZE; i++) {
		m->table[i] = NULL;
	}
}

int bob_str_map_insert(StrMap *m, char *key, void *val) {
	unsigned index = pjw_hash(key) % MAP_TABLE_SIZE;
	StrMapEntry **pcurr = &m->table[index], *curr = *pcurr;
	StrMapEntry *n = malloc(sizeof *n);

	if (!n) 
		return STATUS_OUT_OF_MEMORY;
	n->key = key;
	n->val = val;
	n->next = NULL;

	if (curr) {
		while (curr->next)
			curr = curr->next;
		curr->next = n;
	} 
	else {
		*pcurr = n;	
	}
	m->size++;
	return STATUS_OK;
}

void *bob_str_map_get(StrMap *m, char *key) {
	unsigned index = pjw_hash(key);
	StrMapEntry *entry = m->table[index];

	if (entry) {
		while (entry) {
			if (!strcmp(entry->key, key))
				return entry->val;
		}
	}
	return NULL;
}

void *bob_str_map_free(StrMap *m) {
	int i;
	StrMapEntry *entry, *bck;

	for (i = 0; i < MAP_TABLE_SIZE; i++) {
		entry = m->table[i];
		while (entry) {
			bck = entry->next;	
			free(entry);
			entry = bck;
		}
	}
}

unsigned pjw_hash(const char *key) {
	unsigned h = 0, g;
	while (*key) {
		h = (h << 4) + *key++;
		if((g = h & (unsigned)0xF0000000) != 0) {
			h = (h ^ (g >> 4)) ^g;
		}
	}
	return (unsigned)(h % MAP_TABLE_SIZE);
}

