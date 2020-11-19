#include "physics.h"
#include "log.h"
#include <math.h>
#include <assert.h>
#include <GLFW/glfw3.h>

const double GRAV_G = 6.67430E-11;

static void s_phys_compute_point_gravity_instances(Level *level);
static void s_phys_compute_point_gravity(vec3 result, Instance *i1, Instance *i2);
static void s_phys_compute_impulse(Level *level);

phys_impulse_s *phys_impulse_new(double dt) {
  phys_impulse_s *imp = malloc(sizeof *imp);
  if (!imp) {
    log_error("error allocating impulse object");
    exit(1);
  }
  imp->dt = dt;
  return imp;
}

void phys_add_impulse(Instance *inst, phys_impulse_s *impulse) {
  PointerList *pl = malloc(sizeof *pl);
  if (pl == NULL) {
    log_error("memory allocation error");
    exit(1);
  }
  pl->ptr = impulse;
  pl->next = inst->impulse;
  inst->impulse = pl;
}

void phys_compute_force(Level *level) {
  s_phys_compute_point_gravity_instances(level);
  s_phys_compute_impulse(level);
}

void s_phys_compute_point_gravity_instances(Level *level) {
  size_t i, j;
  vec3 gvec;
  PointerVector *pv = &level->gravityObjects;

  for (i = 0; i < pv->size; i++) {
    Instance *i1 = pv->buffer[i];
    for (j = i + 1; j < pv->size; j++) {
      Instance *i2 = pv->buffer[j];
      s_phys_compute_point_gravity(gvec, i1, i2);
      glm_vec3_add(i1->force, gvec, i1->force);
      glm_vec3_negate(gvec);
      glm_vec3_add(i2->force, gvec, i2->force);
    }
  }
}

void s_phys_compute_impulse(Level *level) {
  int i;
  double currtime = glfwGetTime();
  double dt = currtime - level->t0;

  for (i = 0; i < level->instances.size; i++) {
    Instance *inst = level->instances.buffer[i];
    PointerList *prev = inst->impulse, *curr = prev, *bck;
    while (curr) {
      phys_impulse_s *imp = curr->ptr;
      glm_vec3_add(inst->force, imp->force, inst->force);
      imp->dt -= dt;
      if (imp->dt <= 0.0) {
        bck = curr;
        if (curr == prev) {
          prev = curr->next;
          inst->impulse = prev;
        } else {
          prev->next = curr->next;
        }
        curr = curr->next;
        free(bck);
      } else {
        prev = curr;
        curr = curr->next;
      }
    }
  }
}

void phys_update_position(Level *level) {
  size_t i;
  double currtime = glfwGetTime();
  double dt = currtime - level->t0;
  vec3 accum;
  PointerVector *pv = &level->instances;

  for (i = 0; i < pv->size; i++) {
    Instance *inst = pv->buffer[i];
    if (!inst->isStatic) {
      glm_vec3_divs(inst->force, inst->mass, inst->acceleration);
      glm_vec3_zero(inst->force);
      glm_vec3_add(inst->acceleration, level->ambient_gravity, inst->acceleration);
      glm_vec3_scale(inst->acceleration, dt, accum);
      glm_vec3_add(inst->velocity, accum, inst->velocity);
      glm_vec3_scale(inst->velocity, dt, accum);
      glm_vec3_add(inst->pos, accum, inst->pos);
    }
  }
}

void s_phys_compute_point_gravity(vec3 result, Instance *i1, Instance *i2) {
  vec3 diff;
  double r12 = glm_vec3_distance(i2->pos, i1->pos);
  double r122 = r12*r12;
  glm_vec3_sub(i2->pos, i1->pos, diff);
  glm_vec3_divs(diff, r12, result);
  double coeff = GRAV_G * i1->mass * i2->mass / r122;
  glm_vec3_scale(result, coeff, result);
}

