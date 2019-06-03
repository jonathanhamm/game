#ifndef __glprogram_h__
#define __glprogram_h__

#include <GL/glew.h>

typedef struct GlShader GlShader;
typedef struct GlTexture GlTexture;
typedef struct GlProgram GlProgram;

struct GlShader {
	const char *name;
	GLuint handle;
};

struct GlTexture {
	const char *name;
	GLuint handle;
};

extern int gl_load_shader(GlShader *shader, GLenum shader_type, const char *src, const char *name);
extern int gl_load_shader_from_file(GlShader *shader, GLenum shader_type, const char *file_path, const char *name);
extern int gl_load_shaders(const char *directory);

extern int gl_load_texture(const char *file_path);


#endif

