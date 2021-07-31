#include "instance_plane.h"

void instance_plane_get_matrix(InstancePlane *ip, mat4 m4) {
	glm_mat4_identity(m4);	
	glm_translate(m4, i->pos);
}

/** TODO: R@D on the effectiveness of tis */
void render_instance_plane(InstancePlane *ip) {
  int i,j;
  for (i = 0; i < ip->n1; i++) {
    for (j = 0; j < ip->n2; j++) {
    }
  }
}

