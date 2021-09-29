#include "log.h"
#include "game.h"
#include "lazy_instance_engine.h"

#include <stdio.h>

int main(void) {
	log_init(stdout);
	//bob_start();
  double result = lazy_epxression_compute(NULL, "1+3+(((4+1)+1)+1+3+1.3330)*4/(1+3)");
	printf("result: %f\n", result);
	log_end();
	return 0;
}

