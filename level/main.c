#include "lex.h"
#include "parse.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
	int i;	

	if (argc < 2) {
		fprintf(stderr, "Usage: level <path-to-source-file> [,...]\n");
	}
	else {
		for (i = 1; i < argc; i++) {
			toklist_s tokens = lex(argv[i]);
			parse(&tokens);
			//toklist_print(&tokens);
		}
	}
	
	return 0;
}

