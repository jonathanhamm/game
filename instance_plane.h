#ifndef __instance_plane_h__
#define __instance_plane_h__

#include "models.h"

struct InstancePlane {
  vec3 p;
  vec3 v;
  int n1;
  int n2;
  Model *model;
};

#endif

