#ifndef __instance_plane_h__
#define __instance_plane_h__

#include "models.h"
#include <cglm/cglm.h>


typedef struct InstancePlane InstancePlane;

struct InstancePlane {
  vec3 p;
  vec3 v1;
  vec3 v2;
  int n1;
  int n2;
  Model *model;
};

extern void instance_plane_get_matrix(InstancePlane *ip, mat4 m4);

#endif

