#include "log.h"
#include "models.h"
#include "meshes.h"
#include <GL/glew.h>

static PointerVector get_basic_shaders1(void);

PointerVector get_basic_shaders1(void) {
	int result;
	PointerVector pv;

	GlShader *vertex_shader = malloc(sizeof *vertex_shader);
	GlShader *fragment_shader = malloc(sizeof *fragment_shader);

	result = gl_load_shader_from_file(vertex_shader, GL_VERTEX_SHADER, "shaders/vertex.vsh", "test1");
	result = gl_load_shader_from_file(fragment_shader, GL_FRAGMENT_SHADER, "shaders/fragment.fsh", "test2");
	
	pointer_vector_init(&pv);
	pointer_vector_add(&pv, vertex_shader);
	pointer_vector_add(&pv, fragment_shader);

	return pv;
}

Asset *get_asset_test1(void) {
	Asset *a = malloc(sizeof *a);
	GLint handle;
	PointerVector shaders = get_basic_shaders1();
	GlProgram *program = malloc(sizeof *program);	
	GlTexture *texture = malloc(sizeof *texture);

	gl_create_program(program, shaders);

	a->program = program;

	glGenBuffers(1, &a->vbo);
	glGenVertexArrays(1, &a->vao);

	glBindVertexArray(a->vao);
	glBindBuffer(GL_ARRAY_BUFFER, a->vbo);
	glBufferData(GL_ARRAY_BUFFER, TEST_MESH1_SIZE, test_mesh1, GL_STATIC_DRAW);


	handle = gl_shader_attrib(program, "vert");
	glEnableVertexAttribArray(handle);
	glVertexAttribPointer(handle, 3, GL_FLOAT, GL_FALSE, 5*sizeof(GLfloat), NULL);

	handle = gl_shader_attrib(program, "vertexCoord");
	glEnableVertexAttribArray(handle);
	glVertexAttribPointer(handle, 2, GL_FLOAT, GL_TRUE, 5*sizeof(GLfloat), (const GLvoid *)(3*sizeof(GLfloat)));

	glBindVertexArray(0);

	gl_load_texture(texture, "textures/pge_icon.png");
	a->texture = texture;
	a->drawType = GL_TRIANGLE_STRIP;
	a->drawStart = 1;
	a->drawCount = 7*2*3;

	return a;
}

PointerVector gen_models_test1(void) {
	int result;
	PointerVector pv;

	Model *m;

	Asset *a1 = get_asset_test1();
	result = pointer_vector_init(&pv);

	m = malloc(sizeof *m);
	m->asset = a1;
	m->mass = 1.0;
	glm_vec3_copy((vec3){0,0,0}, m->pos);
	glm_vec3_copy((vec3){0,0,0}, m->velocity);
	glm_vec3_copy((vec3){0,0,0}, m->acceleration);
	glm_vec3_copy((vec3){2,1,0}, m->scale);
	glm_vec3_copy((vec3){2,1,0}, m->rotation);
	pointer_vector_add(&pv, m);


	m = malloc(sizeof *m);
	m->asset = a1;
	m->mass = 1.0;
	glm_vec3_copy((vec3){1,0,0}, m->pos);
	glm_vec3_copy((vec3){0,0,0}, m->velocity);
	glm_vec3_copy((vec3){0,0,0}, m->acceleration);
	glm_vec3_copy((vec3){2,1,0}, m->scale);
	glm_vec3_copy((vec3){2,1,0}, m->rotation);
	pointer_vector_add(&pv, m);

	m = malloc(sizeof *m);
	m->asset = a1;
	m->mass = 1.0;
	glm_vec3_copy((vec3){0,0,2}, m->pos);
	glm_vec3_copy((vec3){0,0,0}, m->velocity);
	glm_vec3_copy((vec3){0,0,0}, m->acceleration);
	glm_vec3_copy((vec3){2,1,0}, m->scale);
	glm_vec3_copy((vec3){2,1,0}, m->rotation);
	pointer_vector_add(&pv, m);
	return pv;
}

void model_update_position(Model *m, float dt) {
	float *p = m->pos;
	float *v = m->velocity;	
	float *a = m->acceleration;

	v[0] += (a[0] / m->mass) * dt;
	v[1] += (a[1] / m->mass) * dt;
	v[2] += (a[2] / m->mass) * dt;
	p[0] += v[0] * dt;
	p[1] += v[1] * dt;
	p[2] += v[2] * dt;
}

void model_rotate(Model *m, float x, float y, float z) {	
	float *r = m->rotation;
	r[0] += x;
	r[1] += y;
	r[2] += z;
}

void model_get_matrix(Model *m, mat4 m4) {
	glm_mat4_identity(m4);	
	glm_translate(m4, m->pos);
	glm_scale(m4, m->scale);
}

