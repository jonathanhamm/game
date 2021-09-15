#ifndef __models_h__
#define __models_h__

#include "glprogram.h"
#include "common/data-structures.h"
#include <cglm/cglm.h>
#include <GL/glew.h>

typedef struct Model Model;
typedef struct Instance Instance;
typedef struct LazyInstance LazyInstance;
typedef struct Range Range;

struct Model {
	GlProgram *program;
	GlTexture *texture;
	GLuint vbo;
	GLuint vao;
	GLenum drawType;
	GLint drawStart;
	GLint drawCount;
};

struct Instance {
	Model *model;
	float mass;
  bool isSubjectToGravity;
  bool isStatic;
	vec3 pos;
	vec3 velocity;
	vec3 acceleration;
  vec3 force;
	vec3 scale;
	vec3 rotation;
	PointerVector *collision_space;
	PointerVector *gravity_space;
  PointerList *impulse;
};

struct LazyInstance {
	Model *model;
	float mass;
  bool isSubjectToGravity;
  bool isStatic;
	char *px;
	char *py;
	char *pz;
	vec3 velocity;
	vec3 acceleration;
  vec3 force;
	char *scalex;
	char *scaley;
	char *scalez;
	vec3 rotation;
	PointerVector *collision_space;
	PointerVector *gravity_space;
  PointerList *impulse;
};

struct Range {
	int steps;
	char var;
	bool cache;
	Range *child;
	PointerVector lazyinstances;
};

extern Model *get_model_test1(void);

extern PointerVector gen_instances_test1(void);

/* Actual functions */
extern void instance_update_position(Instance *i, float dt);
extern void instance_rotate(Instance *i, float x, float y, float z);
extern void instance_get_matrix(Instance *i, mat4 m4);


#endif
