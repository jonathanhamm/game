#include "game.h"
#include "log.h"
#include "meshes.h"
#include "glprogram.h"
#include "models.h"
#include "physics.h"
#include "lazy_instance_engine.h"
#include "loadlevel.h"
#include <cglm/cglm.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <stdio.h>

static void level_render(GLFWwindow *window, Level *level);
static void render_instance_group(InstanceGroup *ig, Camera *camera);

static void render_instance(Instance *instance, Camera *camera);
static void render_instance2(Level *level, Instance *instance, mat4 cmatrix, GLint camera_handle, 
    GLint model_handle, GLint tex_handle);
static void render_range_root(Level *level, RangeRoot *rangeRoot, Camera *camera);
static void render_range(Level *level, Range *range, Camera *camera, Model *m, GLint model_handle, GLint camera_handle, GLint tex_handle, mat4 cmatrix);
static void render_lazy_instance(Level *level, LazyInstance *li, Range *range, mat4 cmatrix, GLint camera_handle,
    GLint model_handle, GLint tex_handle) ;
static void update(GLFWwindow *window, Camera *camera, float secondsElapsed);
static Instance *spawn_instance(Level *level);

static void buffered_render(Level *level, Model *m, vec3 pos, vec3 scale);
static void buffered_render_finalize(Level *level, Model *m);


/** callbacks **/
static void error_callback(int error, const char *description);
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

void bob_start(void) {
	int result;
	GLFWwindow *window;
	GlShader vertex_shader, fragment_shader;
	GlProgram program;
	GLuint vertex_buffer;
	GLint mvp_location, vpos_location, vcol_location;

	static int debounce = 0;


	glfwSetErrorCallback(error_callback);
	if (!glfwInit())
		exit(EXIT_FAILURE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	window = glfwCreateWindow(1000, 1000, "Simple example", NULL, NULL);
	if (!window) {
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetCursorPos(window, 0, 0);
	//glfwSetScrollCallback(window, onScroll);
	glfwMakeContextCurrent(window);

	glfwSetKeyCallback(window, key_callback);
	//gladLoadGL(glfwGetProcAddress);
	//glfwSwapInterval(1);
	// NOTE: OpenGL error checks have been omitted for brevity

	if(glewInit() != GLEW_OK) {
		log_error("Error Initializing glew environment");
	}
	while(glGetError() != GL_NO_ERROR) {}

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL );

	bob_db_s *bdb = bob_loaddb("level/test.db");

	Level *blvl = bob_loadlevel(bdb, "hello");

	Level level = *blvl;
	level.t0 = glfwGetTime();
	PointerVector pvt = gen_instances_test1();

	camera_init(&level.camera);

	GLenum glError = glGetError();
	if (glError != GL_NO_ERROR) {
		log_error("glerror: %d", glError);
	}
	else {
		log_debug("no glerrors");
	}

	while (!glfwWindowShouldClose(window)) {
		double currTime = glfwGetTime();
		float dt = currTime - level.t0;

		glfwPollEvents();
		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS && !debounce) {
			spawn_instance(&level);
			debounce++;
		}
		else if(debounce > 10) {
			debounce = 0;
		}
		else if(debounce) {
			debounce++;
		} 

    if (!debounce)
      log_debug("dt: %f", 1/dt);

		update(window, &level.camera, dt);
		phys_compute_force(&level);
		phys_update_position(&level);

		level_render(window, &level);

		glfwSwapBuffers(window);

		GLenum error = glGetError();
		if (error != GL_NO_ERROR) {
			log_error("OpenGL Error: %d", error);
			exit(1);
		}
		level.t0 = currTime;
	}
	glfwDestroyWindow(window);
	glfwTerminate();
	exit(EXIT_SUCCESS);
}

void level_render(GLFWwindow *window, Level *level) {
	int i;

	for (i = 0; i < level->instances.size; i++) {
		InstanceGroup *ig = level->instances.buffer[i];
    //render_instance_group(ig, &level->camera);
	}

	for (i = 0; i < level->ranges.size; i++) {
    RangeRoot *rangeRoot = level->ranges.buffer[i];
    render_range_root(level, rangeRoot, &level->camera);
	}
}

void render_instance_group(InstanceGroup *ig, Camera *camera) {
  int i;

  mat4 cmatrix;
  Model *m = ig->model;
  GLint program, camera_handle, model_handle, tex_handle;

  program = m->program->handle;

  camera_get_matrix(camera, cmatrix);

  glUseProgram(program);

  model_handle = glGetUniformLocation(program, "model");
  camera_handle = glGetUniformLocation(program, "camera");
  tex_handle = glGetUniformLocation(program, "tex");

	glUniformMatrix4fv(camera_handle, 1, false, (const GLfloat *)cmatrix);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex_handle); 

	glBindVertexArray(m->vao);

  for (i = 0; i < ig->instances.size; i++) {
    Instance *inst = ig->instances.buffer[i];
    //render_instance2(level, inst, cmatrix, camera_handle, model_handle, tex_handle);
  }

  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);

  glUseProgram(0);
}

void render_instance(Instance *instance, Camera *camera) {
	mat4 mmatrix;	
	mat4 cmatrix;	
	GLint model_handle, camera_handle, tex_handle;
	GLint program;
	Model *m = instance->model;

	program = m->program->handle;
	instance_get_matrix(instance, mmatrix);
	camera_get_matrix(camera, cmatrix);

	glUseProgram(m->program->handle);
	model_handle = glGetUniformLocation(program, "model");
	camera_handle = glGetUniformLocation(program, "camera");
	tex_handle = glGetUniformLocation(program, "tex");

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m->texture->handle); 

	glUniformMatrix4fv(model_handle, 1, false, (const GLfloat *)mmatrix);
	glUniformMatrix4fv(camera_handle, 1, false, (const GLfloat *)cmatrix);
	glUniform1i(tex_handle, 0);

	glBindVertexArray(m->vao);
	glDrawArrays(m->drawType, m->drawStart, m->drawCount);

	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0); 

	glUseProgram(0);
}

void render_instance2(Level *level, Instance *instance, mat4 cmatrix, GLint camera_handle, 
    GLint model_handle, GLint tex_handle) {
  Model *m = instance->model;

	glUniform1i(tex_handle, 0);

  buffered_render(level, m, instance->pos, instance->scale);
}

void render_range_root(Level *level, RangeRoot *rangeRoot, Camera *camera) {
  int i;
  Model *m = rangeRoot->m;
  mat4 cmatrix;
  GLint program = m->program->handle, camera_handle, model_handle, tex_handle;

  camera_get_matrix(camera, cmatrix);
  glUseProgram(program);

  camera_handle = glGetUniformLocation(program, "camera");
  model_handle = glGetUniformLocation(program, "model");
  tex_handle = glGetUniformLocation(program, "tex");

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex_handle); 

  glBindVertexArray(m->vao);

	glUniformMatrix4fv(camera_handle, 1, false, (const GLfloat *)cmatrix);

  for (i = 0; i < rangeRoot->ranges.size; i++) {
    Range *currRange = rangeRoot->ranges.buffer[i]; 
    render_range(level, currRange, camera, m, model_handle, camera_handle, tex_handle, cmatrix);
  }

  buffered_render_finalize(level, m);

  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glUseProgram(0);
}

void render_range(Level *level, Range *range, Camera *camera, Model *m, GLint model_handle, GLint camera_handle, GLint tex_handle, mat4 cmatrix) {
  int i;

  for (range->currval = 0; range->currval < range->steps; range->currval++) {
    for (i = 0; i < range->lazyinstances.size; i++) {
      LazyInstance *li = range->lazyinstances.buffer[i];
      render_lazy_instance(level, li, range, cmatrix, camera_handle, model_handle, tex_handle);
    }
    if (range->child) {
      render_range(level, range->child, camera, m, model_handle, camera_handle, tex_handle, cmatrix);
    }
  }
}

void render_lazy_instance(Level *level, LazyInstance *li, Range *range, mat4 cmatrix, GLint camera_handle,
    GLint model_handle, GLint tex_handle) {
	Instance instance;	

	instance.model = li->model;
	instance.isSubjectToGravity = li->isSubjectToGravity;
	instance.isStatic = li->isStatic;
	instance.collision_space = li->collision_space;
	instance.mass = li->mass;
	instance.gravity_space = li->gravity_space;
	instance.impulse = li->impulse;

	float posx = lazy_epxression_compute(range, li->px);
	float posy = lazy_epxression_compute(range, li->py);
	float posz = lazy_epxression_compute(range, li->pz);

  //log_info("posx posy posz: %s %s %s -> %f %f %f", li->px, li->py, li->pz, posx, posy, posz);

	instance.pos[0] = posx;
	instance.pos[1] = posy;
	instance.pos[2] = posz;


  float scalex = lazy_epxression_compute(range, li->scalex);
	float scaley = lazy_epxression_compute(range, li->scaley);
	float scalez = lazy_epxression_compute(range, li->scalez);

  //log_info("values: %f %f %f", scalex, scaley, scalez);

	instance.scale[0] = scalex;
	instance.scale[1] = scaley;
	instance.scale[2] = scalez;

	render_instance2(level, &instance, cmatrix, camera_handle, model_handle, tex_handle);
}

void update(GLFWwindow *window, Camera *camera, float secondsElapsed) {
	const GLfloat degreesPerSecond = 180.0f;
	camera->gdegrees_rotated += secondsElapsed * degreesPerSecond;
	while(camera->gdegrees_rotated > 360.0f) camera->gdegrees_rotated -= 360.0f;
	vec3 result;
	const float moveSpeed = 100; //units per second
	if(glfwGetKey(window, 'S')){
		camera_forward(camera, result);
		glm_vec3_scale(result, -secondsElapsed * moveSpeed, result);
		camera_offset_position(camera, result);
	} 
	else if(glfwGetKey(window, 'W')){
		camera_forward(camera, result);
		glm_vec3_scale(result, secondsElapsed * moveSpeed, result);
		camera_offset_position(camera, result);
	}
	if(glfwGetKey(window, 'A')){
		camera_right(camera, result);
		glm_vec3_scale(result, -secondsElapsed * moveSpeed, result);
		camera_offset_position(camera, result);
	} 
	else if(glfwGetKey(window, 'D')){
		camera_right(camera, result);
		glm_vec3_scale(result, secondsElapsed * moveSpeed, result);
		camera_offset_position(camera, result);
	}
	const float mouseSensitivity = 0.1f;
	double mouseX, mouseY;

	glfwGetCursorPos(window, &mouseX, &mouseY);
	camera_offset_orientation(camera, mouseSensitivity * (float)mouseY, mouseSensitivity * (float)mouseX);
	glfwSetCursorPos(window, 0, 0); //reset the mouse, so it doesn't go out of the window

}

Instance *spawn_instance(Level *level) {
	Instance *inst = malloc(sizeof *inst);
	if (!inst) {
		log_error("memory error");
		exit(1);
	}
	PointerVector *pv = &level->instances;
	Instance *template = pv->buffer[0];

	inst->pos[0] = level->camera.pos[0];
	inst->pos[1] = level->camera.pos[1];
	inst->pos[2] = level->camera.pos[2];

	inst->mass = 10;
	inst->scale[0] = 1.0;
	inst->scale[1] = 1.0;
	inst->scale[2] = 1.0;
	inst->rotation[0] = 2;
	inst->rotation[1] = 0;
	inst->rotation[2] = 0;
	inst->isSubjectToGravity = true;
	inst->isStatic = false;
	inst->model = template->model;
	inst->impulse = NULL;

	glm_vec3_zero(inst->velocity);
	glm_vec3_zero(inst->acceleration);
	glm_vec3_zero(inst->force);

	phys_impulse_s *imp = phys_impulse_new(0.01);
	camera_forward(&level->camera, imp->force);
	glm_vec3_scale(imp->force, 1E4, imp->force);
	phys_add_impulse(inst, imp);

	instance_group_add(pv, template->model, inst);
	pointer_vector_add(&level->gravityObjects, inst);
	return inst;
}

void buffered_render(Level *level, Model *m, vec3 pos, vec3 scale) {
  RenderBuffer *rb = &level->renderBuffer;

  glm_vec3_copy(pos, rb->buffer[rb->pos]);
  glm_vec3_copy(scale, rb->buffer[RENDER_BUFFER_SIZE + rb->pos]);

  rb->pos++;

  log_info("rendering pos %f %f %f", pos[0], pos[1], pos[2]);

  if (rb->pos == RENDER_BUFFER_SIZE ) {
    log_info("flushing buffer");
    glBindBuffer(GL_ARRAY_BUFFER, m->pvbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(rb->buffer), &rb->buffer[0], GL_DYNAMIC_COPY);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDrawArraysInstanced(m->drawType, m->drawStart, m->drawCount, RENDER_BUFFER_SIZE);
    rb->pos = 0;
  }

  GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    log_error("in OpenGL Error: %d", error);
    exit(1);
  }
}

void buffered_render_finalize(Level *level, Model *m) {
  RenderBuffer *rb = &level->renderBuffer;



  glBindBuffer(GL_ARRAY_BUFFER, m->pvbo);

  log_info("buffer size: %lu and portion %lu and %lu, and %d", 
      sizeof(rb->buffer), sizeof(*rb->buffer)*rb->pos, sizeof(*rb->buffer), GL_ARRAY_BUFFER);

  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(*rb->buffer)*rb->pos, &rb->buffer[0]);

  GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    log_error("1 in final OpenGL Error: %d", error);
    exit(1);
  }

  glBufferSubData(GL_ARRAY_BUFFER, sizeof(*rb->buffer)*RENDER_BUFFER_SIZE, sizeof(*rb->buffer)*rb->pos, 
      &rb->buffer[RENDER_BUFFER_SIZE]);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDrawArraysInstanced(m->drawType, m->drawStart, m->drawCount, rb->pos);
  rb->pos = 0;

  log_info("finalize render");

  error = glGetError();
  if (error != GL_NO_ERROR) {
    log_error("2 in final OpenGL Error: %d", error);
    exit(1);
  }
}

void error_callback(int error, const char* description)
{
	fprintf(stderr, "Error: %s\n", description);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GLFW_TRUE);
}

