#ifndef __camera_h__
#define __camera_h__

#include <cglm/cglm.h>

typedef struct Camera Camera;

struct Camera {
	vec3 pos;
	float horizontal_angle;
	float vertical_angle;
	float field_of_view;
	float near_plane;
	float far_plane;
	float viewport_aspect_ratio;
	float gdegrees_rotated;
	float scrolly;
};

extern int camera_init(Camera *camera);
extern void camera_offset_position(Camera *camera, vec3 offset);
extern void camera_orientation(Camera *camera, mat4 m);
extern void camera_offset_orientation(Camera *camera, float upAngle, float rightAngle);
extern void camera_lookat(Camera *camera, vec3 position);
extern void camera_forward(Camera *camera, vec3 result);
extern void camera_right(Camera *camera, vec3 result);
extern void camera_up(Camera *camera, vec3 result);
extern void camera_pos(Camera *camera, mat4 result);
extern void camera_get_matrix(Camera *camera, mat4 result);
extern void camera_projection(Camera *camera, mat4 result);
extern void camera_view(Camera *camera, mat4 result);

#endif

