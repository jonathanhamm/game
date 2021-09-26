#include "log.h"
#include "game.h"
#include "lazy_instance_engine.h"

#include <stdio.h>

int main(void) {
	log_init(stdout);
	//bob_start();
  lazy_epxression_compute("a+b", 1.0);
	log_end();
	return 0;
}

