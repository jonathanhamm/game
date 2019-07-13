#ifndef __models_h__
#define __models_h__

#include "glprogram.h"
#include "common/data-structures.h"
#include <cglm/cglm.h>
#include <GL/glew.h>

typedef struct Asset Asset;
typedef struct Model Model;

struct Asset {
	GlProgram *program;
	GlTexture *texture;
	GLuint vbo;
	GLuint vao;
	GLenum drawType;
	GLint drawStart;
	GLint drawCount;
};

struct Model {
	Asset *asset;
	float mass;
	vec3 pos;
	vec3 velocity;
	vec3 acceleration;
	vec3 scale;
	vec3 rotation;
	PointerVector *collision_space;
	PointerVector *gravity_space;
};

extern Asset *get_asset_test1(void);

extern PointerVector gen_models_test1(void);

/* Actual functions */
extern void model_update_position(Model *m, float dt);
extern void model_rotate(Model *m, float x, float y, float z);
extern void model_get_matrix(Model *m, mat4 m4);


#endif
