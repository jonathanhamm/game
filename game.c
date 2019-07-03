#include "game.h"
#include "log.h"
#include "camera.h"
#include "meshes.h"
#include "glprogram.h"
#include "models.h"
#include <cglm/cglm.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <stdio.h>

struct Level {
	float dt;
	Camera camera;
	PointerVector models;
};

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

static void render_model(Model *model, Camera *camera) {
	mat4 mmatrix;	
	mat4 cmatrix;	
	GLint model_handle, camera_handle, tex_handle;
	GLint program;
	Asset *a = model->asset;

	program = a->program->handle;
	model_get_matrix(model, mmatrix);
	camera_get_matrix(camera, cmatrix);

	glUseProgram(a->program->handle);
	model_handle = glGetUniformLocation(program, "model");
	camera_handle = glGetUniformLocation(program, "camera");
	tex_handle = glGetUniformLocation(program, "tex");
	
	glUniformMatrix4fv(model_handle, 1, false, (const GLfloat *)mmatrix);
	glUniformMatrix4fv(camera_handle, 1, false, (const GLfloat *)cmatrix);
	glUniform1i(tex_handle, 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, a->texture->handle); 

	glBindVertexArray(a->vao);
	glDrawArrays(a->drawType, a->drawStart, a->drawCount);

	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0); 

	glUseProgram(0);
}

void level_render(Level *level) {
	int i;
	double currTime = glfwGetTime();
	float dt = (float)(currTime - level->dt);

	for (i = 0; i < level->models.size; i++) {
		Model *model = level->models.buffer[i];
		model_update_position(model, dt);
		render_model(level->models.buffer[i], &level->camera);
		return;
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
	window = glfwCreateWindow(640, 480, "Simple example", NULL, NULL);
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
					
	Level level;
	level.dt = glfwGetTime();
	level.models = gen_models_test1();
	camera_init(&level.camera);


	while (!glfwWindowShouldClose(window))
	{
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
		level.camera.pos[2] += 0.01;
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


