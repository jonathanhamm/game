#include "instance_plane.h"

void instance_plane_get_matrix(InstancePlane *ip, mat4 m4) {
	glm_mat4_identity(m4);	
	glm_translate(m4, i->pos);
}

