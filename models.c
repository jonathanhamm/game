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
	pointer_vector_add(&pv, fragment_shader);
	pointer_vector_add(&pv, vertex_shader);

	return pv;
}

Model *get_model_test1(void) {
	Model *m = malloc(sizeof *m);
	GLint handle;
	PointerVector shaders = get_basic_shaders1();
	GlProgram *program = malloc(sizeof *program);	
	GlTexture *texture = malloc(sizeof *texture);

	gl_create_program(program, shaders);

	m->program = program;

	glGenBuffers(1, &m->vbo);
	glGenVertexArrays(1, &m->vao);

	glBindVertexArray(m->vao);
	glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
	glBufferData(GL_ARRAY_BUFFER, TEST_MESH1_SIZE, test_mesh1, GL_STATIC_DRAW);

	handle = gl_shader_attrib(program, "vert");
	glEnableVertexAttribArray(handle);
	glVertexAttribPointer(handle, 3, GL_FLOAT, GL_FALSE, 5*sizeof(GLfloat), NULL);

	handle = gl_shader_attrib(program, "vertexCoord");
	glEnableVertexAttribArray(handle);
	glVertexAttribPointer(handle, 2, GL_FLOAT, GL_TRUE, 5*sizeof(GLfloat), (const GLvoid *)(3*sizeof(GLfloat)));

	glBindVertexArray(0);

	gl_load_texture(texture, "textures/pge_icon.png");
	m->texture = texture;
	m->drawType = GL_TRIANGLE_STRIP;
	m->drawStart = 0;
	m->drawCount = 6*2*3;

	return m;
}

PointerVector gen_instances_test1(void) {
	int result;
	PointerVector pv;

	Instance *i;

	Model *m1 = get_model_test1();
	result = pointer_vector_init(&pv);

	i = malloc(sizeof *i);
	i->model = m1;
	i->mass = 1.0;
	glm_vec3_copy((vec3){0,0,0}, i->pos);
	glm_vec3_copy((vec3){0,0,0}, i->velocity);
	glm_vec3_copy((vec3){0,0,0}, i->acceleration);
	glm_vec3_copy((vec3){1,1,1}, i->scale);
	glm_vec3_copy((vec3){1,1,0}, i->rotation);
	pointer_vector_add(&pv, i);


	i = malloc(sizeof *i);
	i->model = m1;
	i->mass = 1.0;
	glm_vec3_copy((vec3){-5,0,0}, i->pos);
	glm_vec3_copy((vec3){0,0,0}, i->velocity);
	glm_vec3_copy((vec3){0,0,0}, i->acceleration);
	glm_vec3_copy((vec3){2,1,1}, i->scale);
	glm_vec3_copy((vec3){2,1,0}, i->rotation);
	pointer_vector_add(&pv, i);

	i = malloc(sizeof *i);
	i->model = m1;
	i->mass = 1.0;
	glm_vec3_copy((vec3){5,0,2}, i->pos);
	glm_vec3_copy((vec3){0,0,0}, i->velocity);
	glm_vec3_copy((vec3){0,0,0}, i->acceleration);
	glm_vec3_copy((vec3){2,1,1}, i->scale);
	glm_vec3_copy((vec3){2,1,0}, i->rotation);
	pointer_vector_add(&pv, i);
	return pv;
}

void instance_update_position(Instance *i, float dt) {
	float *p = i->pos;
	float *v = i->velocity;	
	float *a = i->acceleration;

	v[0] += (a[0] / i->mass) * dt;
	v[1] += (a[1] / i->mass) * dt;
	v[2] += (a[2] / i->mass) * dt;
	p[0] += v[0] * dt;
	p[1] += v[1] * dt;
	p[2] += v[2] * dt;
}

void instance_rotate(Instance *i, float x, float y, float z) {	
	float *r = i->rotation;
	r[0] += x;
	r[1] += y;
	r[2] += z;
}

void instance_get_matrix(Instance *i, mat4 m4) {
	glm_mat4_identity(m4);	
	glm_translate(m4, i->pos);
	glm_scale(m4, i->scale);
}

