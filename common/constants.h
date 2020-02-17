#ifndef __constants_h__
#define __constants_h__

typedef enum bob_shader_e bob_shader_e;

enum bob_shader_e {
  BOB_VERTEX_SHADER,
  BOB_TESS_EVAL_SHADER,
  BOB_TESS_CONTROL_SHADER,
  BOB_GEOMETRY_SHADER,
  BOB_FRAGMENT_SHADER,
  BOB_COMPUTE_SHADER
};

#endif

