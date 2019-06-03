#include "log.h"
#include "game.h"
#include <stdio.h>

int main(void) {
	log_init(stdout);
	bob_start();
	log_end();
	return 0;
}

