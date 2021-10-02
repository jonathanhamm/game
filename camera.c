#include "camera.h"
#include "common/errcodes.h"

static const float MAX_VERTICAL_ANGLE = 85.0f;

static void normalize_angles(Camera *camera);

int camera_init(Camera *camera) {
  camera->pos[0] = 0.0;
  camera->pos[1] = 0.0;
  camera->pos[2] = 5.0;
  camera->horizontal_angle = 0.0;
  camera->vertical_angle = 0.0;
  camera->field_of_view = 50.0;
  camera->near_plane = 0.01f;
  camera->far_plane = 500.0;
  camera->viewport_aspect_ratio = 4.0f/3.0f;
  camera->gdegrees_rotated = 0.0;
  return STATUS_OK;
}

void camera_offset_position(Camera *camera, vec3 offset) {
  glm_vec3_add(camera->pos, offset, camera->pos);
}

void camera_orientation(Camera *camera, mat4 m) {
  float vertrad = glm_rad(camera->vertical_angle);
  float horizrad = glm_rad(camera->horizontal_angle);
  glm_rotate_make(m, vertrad, (vec3){1.0,0.0,0.0});
  glm_rotate(m, horizrad, (vec3){0.0,1.0,0.0});
}

void camera_offset_orientation(Camera *camera, float upAngle, float rightAngle) {
  camera->horizontal_angle += rightAngle;
  camera->vertical_angle += upAngle;
  normalize_angles(camera); 
}

void camera_lookat(Camera *camera, vec3 position) {
  vec3 direction;	
  float asiny;
  float atanx;

  glm_vec3_sub(position, camera->pos, direction);
  glm_vec3_normalize(direction);
  asiny = asinf(-direction[1]);
  atanx = atan2f(-direction[0], -direction[2]);
  camera->vertical_angle = glm_rad(asiny);
  camera->horizontal_angle = -glm_rad(atanx);
  normalize_angles(camera);
}

void camera_forward(Camera *camera, vec3 result) {
  vec4 forward;
  mat4 orientation;

  camera_orientation(camera, orientation);
  glm_mat4_inv(orientation, orientation);
  glm_mat4_mulv(orientation, (vec4){0,0,-1,1}, forward);
  glm_vec3(forward, result);
}

void camera_right(Camera *camera, vec3 result) {
  vec4 right;
  mat4 orientation;

  camera_orientation(camera, orientation);
  glm_mat4_inv(orientation, orientation);
  glm_mat4_mulv(orientation, (vec4){1,0,0,1}, right);
  glm_vec3(right, result);
}

void camera_up(Camera *camera, vec3 result) {
  vec4 up;
  mat4 orientation;

  camera_orientation(camera, orientation);
  glm_mat4_inv(orientation, orientation);
  glm_mat4_mulv(orientation, (vec4){0,1,0,1}, up);
  glm_vec3(up, result);
}

void camera_pos(Camera *camera, mat4 result) {
  glm_translate_make(result, camera->pos);	
}

void normalize_angles(Camera *camera) {
  float horiz = camera->horizontal_angle;
  float vert =  camera->vertical_angle;

  horiz = fmodf(horiz, 360.0f);
  if (horiz < 0.0f)
    horiz += 360.0f;

  if (vert > MAX_VERTICAL_ANGLE) 
    vert = MAX_VERTICAL_ANGLE;
  else if (vert < -MAX_VERTICAL_ANGLE)
    vert = -MAX_VERTICAL_ANGLE;

  camera->horizontal_angle = horiz;
  camera->vertical_angle = vert;
}


void camera_get_matrix(Camera *camera, mat4 result) {
  mat4 v;
  camera_projection(camera, result);
  camera_view(camera, v);
  glm_mat4_mul(result, v, result);
}

void camera_projection(Camera *camera, mat4 result) {
  glm_perspective(
      glm_rad(camera->field_of_view), 
      camera->viewport_aspect_ratio, 
      camera->near_plane, 
      camera->far_plane, 
      result
      );
}

void camera_view(Camera *camera, mat4 result) {
  vec3 npos;
  mat4 translate;

  npos[0] = -camera->pos[0];
  npos[1] = -camera->pos[1];
  npos[2] = -camera->pos[2];
  camera_orientation(camera, result);
  glm_translate_make(translate, npos);	
  glm_mat4_mul(result, translate, result);
}

