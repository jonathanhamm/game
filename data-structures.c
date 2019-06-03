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

int char_buf_free(CharBuf *b) {
	free(b->buffer);
	b->buffer = NULL;
}

void bob_str_map_init(BobStrMap *m) {
	int i;

	m->size = 0;
	for (i = 0; i < MAP_TABLE_SIZE; i++) {
		m->table[i] = NULL;
	}
}

int bob_str_map_insert(BobStrMap *m, char *key, void *val) {
	unsigned index = pjw_hash(key) % MAP_TABLE_SIZE;
	BobStrMapEntry **pcurr = &m->table[index], *curr = *pcurr;
	BobStrMapEntry *n = malloc(sizeof *n);

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

void *bob_str_map_get(BobStrMap *m, char *key) {
	unsigned index = pjw_hash(key);
	BobStrMapEntry *entry = m->table[index];

	if (entry) {
		while (entry) {
			if (!strcmp(entry->key, key))
				return entry->val;
		}
	}
	return NULL;
}

void *bob_str_map_free(BobStrMap *m) {
	int i;
	BobStrMapEntry *entry, *bck;

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

