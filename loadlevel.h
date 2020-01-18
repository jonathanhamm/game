#ifndef __loadlevel_h__
#define __loadlevel_h__

#include "game.h"

typedef struct bob_db_s bob_db_s;

extern bob_db_s *bob_loaddb(const char *path);
extern Level *bob_loadlevel(bob_db_s *bdb, const char *name);

#endif

