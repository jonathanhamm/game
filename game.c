#include "game.h"
#include "log.h"
#include "meshes.h"
#include "glprogram.h"
#include "models.h"
#include "loadlevel.h"
#include <cglm/cglm.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <stdio.h>


static void level_render(Level *level);


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

void level_render(Level *level) {
	int i;
	double currTime = glfwGetTime();
  static float counter = 0.01;
  static int bob = 0;
	float dt = (float)(currTime - level->dt);

  level->camera.pos[2] -= 0.001;
  if (bob <= 100) {
    level->camera.pos[0] += counter;
    level->camera.pos[1] += counter;
  }
  else {
    bob = 0;
    counter = -counter;
  }
  bob++;

	for (i = 0; i < level->instances.size; i++) {
		Instance *instance = level->instances.buffer[i];
		instance_update_position(instance, dt);
    //printf("instance pos: <%f %f %f>\n", instance->pos[0], instance->pos[1], instance->pos[2]);
		render_instance(level->instances.buffer[i], &level->camera);
		//return;
	}

	level->dt = dt;
}

void bob_start(void) {
	int result;
	GLFWwindow *window;
	GlShader vertex_shader, fragment_shader;
	GlProgram program;
	GLuint vertex_buffer;
	GLint mvp_location, vpos_location, vcol_location;

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

	//glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetCursorPos(window, 0, 0);
	//glfwSetScrollCallback(window, onScroll);
	glfwMakeContextCurrent(window);


	glfwSetKeyCallback(window, key_callback);
	//gladLoadGL(glfwGetProcAddress);
	//glfwSwapInterval(1);
	// NOTE: OpenGL error checks have been omitted for brevity

	if(glewInit() != GLEW_OK) {
		puts("Error Initializing glew environment");
	}
	while(glGetError() != GL_NO_ERROR) {}

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					
  bob_db_s *bdb = bob_loaddb("level/test.db");
  Level *blvl = bob_loadlevel(bdb, "hello");

	Level level = *blvl;
	level.dt = glfwGetTime();
	PointerVector pvt = gen_instances_test1();
  
	camera_init(&level.camera);

  int i;
  for (i = 0; i < level.instances.size; i++) {
    Instance *inst = (Instance *)level.instances.buffer[i];
    Model *model = ((Instance *)pvt.buffer[0])->model;
    //inst->model->vbo = model->vbo;
    //inst->model->vao = model->vao;
    printf("inst pos: <%f %f %f>\n", inst->pos[0], inst->pos[1], inst->pos[2]);
    printf("inst scale: <%f %f %f>\n", inst->scale[0], inst->scale[1], inst->scale[2]);
    printf("inst model texture: %d\n", inst->model->texture->handle);
    printf("inst program: %d\n", inst->model->program->handle);
    printf("inst vbo and vao: %d %d\n", inst->model->vbo, inst->model->vao);
    printf("inst velocity: <%f %f %f>\n", inst->velocity[0], inst->velocity[1], inst->velocity[2]);
    printf("inst acceleration: <%f %f %f>\n", inst->acceleration[0], inst->acceleration[1], inst->acceleration[2]);
    printf("inst draw: %d %d %d\n", inst->model->drawCount, inst->model->drawStart, inst->model->drawType);
    printf("inst mass: %f\n", inst->mass);
    puts("--------------------");
  }


  GLenum glError = glGetError();
  if (glError != GL_NO_ERROR) {
    log_error("glerror: %d", glError);
  }
  else {
    log_debug("no glerrors");
  }

	while (!glfwWindowShouldClose(window)) {
		//float ratio;
		//int width, height;
		//mat4 m = GLM_MAT4_IDENTITY_INIT, p, mvp;

		//glfwGetFramebufferSize(window, &width, &height);
		//ratio = width / (float) height;
		//glViewport(0, 100, width, height);
		//glClear(GL_COLOR_BUFFER_BIT);
		//mat4x4_rotate_Z(m, m, (float) glfwGetTime());
		//mat4x4_ortho(p, -ratio, ratio, -1.f, 1.f, 1.f, -1.f);
		//glm_ortho(-1.f, 1.f, 1.f, -1.f, -ratio, ratio, p);
		//mat4x4_mul(mvp, p, m);
	//	glm_mat4_mul(p, m, mvp);
	//	glUseProgram(program.handle);
	//	glUniformMatrix4fv(mvp_location, 1, GL_FALSE, (const GLfloat*) mvp);
	//	glDrawArrays(GL_TRIANGLES, 0, 3);
	//	glfwSwapBuffers(window);
		glfwPollEvents();
		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		level_render(&level);
		glfwSwapBuffers(window);

		GLenum error = glGetError();
		if (error != GL_NO_ERROR) {
			log_error("OpenGL Error: %d %d", error);
			exit(1);
		}
	}
	glfwDestroyWindow(window);
	glfwTerminate();
	exit(EXIT_SUCCESS);
}


