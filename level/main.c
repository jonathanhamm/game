#include "lex.h"
#include "parse.h"
#include "tree.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
	int i;	
	p_context_s context;

	if (argc < 2) {
		fprintf(stderr, "Usage: level <path-to-source-file> [,...]\n");
	}
	else {
		for (i = 1; i < argc; i++) {
			toklist_s tokens = lex(argv[i]);
			if (tokens.head) {
				context = parse(&tokens);
				tree_walk(&context);
			}
		}
	}
	
	return 0;
}

