#include "game.h"
#include "log.h"
#include "meshes.h"
#include "glprogram.h"
#include "models.h"
#include "physics.h"
#include "loadlevel.h"
#include <cglm/cglm.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <stdio.h>


static void level_render(GLFWwindow *window, Level *level);
static void update(GLFWwindow *window, Camera *camera, float secondsElapsed);
static Instance *spawn_instance(Level *level);

static void error_callback(int error, const char* description)
{
	fprintf(stderr, "Error: %s\n", description);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GLFW_TRUE);
}

static void render_instance(Instance *instance, Camera *camera) {
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
	
	glUniformMatrix4fv(model_handle, 1, false, (const GLfloat *)mmatrix);
	glUniformMatrix4fv(camera_handle, 1, false, (const GLfloat *)cmatrix);
	glUniform1i(tex_handle, 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m->texture->handle); 

	glBindVertexArray(m->vao);
	glDrawArrays(m->drawType, m->drawStart, m->drawCount);

	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0); 

	glUseProgram(0);
}

void level_render(GLFWwindow *window, Level *level) {
	int i;
  
	for (i = 0; i < level->instances.size; i++) {
		Instance *instance = level->instances.buffer[i];
		render_instance(level->instances.buffer[i], &level->camera);
	}

}

void update(GLFWwindow *window, Camera *camera, float secondsElapsed) {
  const GLfloat degreesPerSecond = 180.0f;
  camera->gdegrees_rotated += secondsElapsed * degreesPerSecond;
  while(camera->gdegrees_rotated > 360.0f) camera->gdegrees_rotated -= 360.0f;
  vec3 result;
  const float moveSpeed = 5; //units per second
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

  pointer_vector_add(pv, inst);
  pointer_vector_add(&level->gravityObjects, inst);
  return inst;
}

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
  //glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

  bob_db_s *bdb = bob_loaddb("level/test.db");

  perror("start 1");
  Level *blvl = bob_loadlevel(bdb, "level1");

	Level level = *blvl;
	level.t0 = glfwGetTime();
	//PointerVector pvt = gen_instances_test1();
  
	camera_init(&level.camera);

  GLenum glError = glGetError();
  if (glError != GL_NO_ERROR) {
    log_error("glerror: %d", glError);
  }
  else {
    log_debug("no glerrors");
  }

	while (!glfwWindowShouldClose(window)) {
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

    double currTime = glfwGetTime();
    float dt = currTime - level.t0;

    update(window, &level.camera, dt);
    phys_compute_force(&level);
    phys_update_position(&level);

		level_render(window, &level);
		glfwSwapBuffers(window);

		GLenum error = glGetError();
		if (error != GL_NO_ERROR) {
			log_error("OpenGL Error: %d %d", error);
			exit(1);
		}

    level.t0 = currTime;
	}
	glfwDestroyWindow(window);
	glfwTerminate();
	exit(EXIT_SUCCESS);
}


