#ifndef __instance_plane_h__
#define __instance_plane_h__

#include "models.h"

struct InstancePlane {
  vec3 v1;
  int n1;
  vec3 v2;
  int n2;
  Model *model;
};

#endif

