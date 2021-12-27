#ifndef __glprogram_h__
#define __glprogram_h__

#include <GL/glew.h>
#include <png.h>
#include "common/data-structures.h"

typedef struct Png Png;
typedef struct GlShader GlShader;
typedef struct GlTexture GlTexture;
typedef struct GlProgram GlProgram;

struct Png {
	int width;
	int height;
	png_byte bit_depth;
	png_byte color_type;
	char *data;
};

struct GlShader {
	const char *name;
	GLenum type;
	GLuint handle;
};

struct GlTexture {
	Png png;
	const char *name;
	GLuint handle;
};

struct GlProgram {
	GLuint handle;
};

extern int gl_create_program_t1(GlProgram *program, GlShader *vertex_shader, GlShader *fragment_shader); 

extern int gl_create_program(GlProgram *program, PointerVector shaders);

extern GLint gl_shader_attrib(GlProgram *program, const GLchar *attrib_name);

extern int gl_load_shader(GlShader *shader, GLenum shader_type, const char *src, const char *name);
extern int gl_load_shader_from_file(GlShader *shader, GLenum shader_type, const char *file_path, const char *name);
extern void gl_delete_shader(GlShader *shader);

extern int gl_load_texture(GlTexture *texture, const char *file_path);
extern void gl_delete_texture(GlTexture *texture);


#endif
