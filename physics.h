#ifndef __physics_h__
#define __physics_h__

#include "game.h"
#include "models.h"

typedef struct phys_impulse_s phys_impulse_s;

struct phys_impulse_s {
	double dt;
	vec3 force;
};

extern phys_impulse_s *phys_impulse_new(double dt);
extern void phys_add_impulse(Instance *inst, phys_impulse_s *impulse);
extern void phys_compute_force(Level *level);
extern void phys_update_position(Level *level);

#endif

