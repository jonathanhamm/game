#include "log.h"
#include "glprogram.h"
#include "errcodes.h"
#include "data-structures.h"
#include <png.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>

typedef struct Png Png;

struct Png {
	int width;
	int height;
	png_byte bit_depth;
	png_byte color_type;
	char *data;
};

static void print_png_version(void);
static int load_png(Png *png, const char *file_path);

int gl_load_shader(GlShader *shader, GLenum shader_type, const char *src, const char *name) {
	int handle;
	GLint status;

	handle = glCreateShader(shader_type);
	if (!handle) {
		log_error("Failed to load shader: %s; GL error on call to glCreateShader", name);
		return STATUS_GL_ERR;
	}

	glShaderSource(handle, 1, (const GLchar **)&src, NULL);	
	glCompileShader(handle);
	glGetShaderiv(handle, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		GLint info_len;
		char *error_msg;

		glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &info_len);
		error_msg = malloc(info_len + 1);
		if (!error_msg) {
			log_error("Failed to load shader: %s; Failed to compile source and not enough memory to get errorm essage", name);
			glDeleteShader(handle);
			return STATUS_OUT_OF_MEMORY;
		}
		glGetShaderInfoLog(handle, info_len, NULL, error_msg);
		log_error("Failed to load shader: %s; Failed to compile source: %s", name, error_msg);
		glDeleteShader(handle);
		free(error_msg);
		return STATUS_GL_ERR;
	}
	shader->name = name;
	shader->handle = handle;
	return STATUS_OK;
}

int gl_load_shader_from_file(GlShader *shader, GLenum shader_type, const char *file_path, const char *name) {
	int c, result;
	CharBuf char_buf;

	result = char_buf_from_file(&char_buf, file_path);
	if (result)
		return result;
	result = gl_load_shader(shader, shader_type, char_buf.buffer, name);	
	char_buf_free(&char_buf);	
	return result;
}

int gl_load_shaders(const char *directory) {
	GlShader shader; 
	print_png_version();
	const char *test = "shaders/test.vsh";
	gl_load_shader_from_file(&shader, GL_VERTEX_SHADER, test, test);

	gl_load_texture("textures/pge_icon.png");
}

void print_png_version(void) {
	log_info("Compiled with libpng %s; using libpng %s", PNG_LIBPNG_VER_STRING, png_libpng_ver);
	//log_info("Compiled with zlib %s; using zlib %s.", ZLIB_VERSION, zlib_version);
};

int gl_load_texture(const char *file_path) {
	int result;
	Png png;

	result = load_png(&png, file_path);
}

int load_png(Png *png, const char *file_path) {
	enum { PNG_SIG_SIZE = 8 };
	FILE *f;			
	char *data;
	png_byte sig[PNG_SIG_SIZE];
	int result, npasses, i;
	size_t len, row_size;
	long width, height;
	png_byte bit_depth;
	png_byte color_type;
	png_bytep *row_ptrs;

	log_info("Attempting to load texture: %s", file_path);

	f = fopen(file_path, "r");
	if (!f) {
		log_error("Error on fread for file %s: Failed to open file.", file_path);
		return STATUS_FILE_IO_ERR;
	}

	len = fread(sig, 1, PNG_SIG_SIZE, f);
	if (len != PNG_SIG_SIZE) {
		log_error("Error on fread for file %s: Failed to read specified number of items.", file_path);
		fclose(f);
		return STATUS_FILE_IO_ERR;
	}

	result = png_check_sig(sig, PNG_SIG_SIZE);
	if (!result) {
		log_error("Error reading texture: %s is not a valid png file", file_path);
		fclose(f);
		return STATUS_PNG_ERR;
	}
	
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,
		NULL, NULL);
	if (!png_ptr) {
		log_error("Error reading texture: %s - Failed on png_create_read_struct()", file_path);
		fclose(f);
		return STATUS_PNG_ERR;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!png_ptr) {
		log_error("Error reading texture: %s - Failed on png_create_info_struct()", file_path);
		fclose(f);
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		return STATUS_PNG_ERR;
	}

	png_infop end_info = png_create_info_struct(png_ptr);
	if (!end_info) {
		log_error("Error reading texture: %s - Failed on png_create_info_struct()", file_path);
		fclose(f);
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
		return STATUS_PNG_ERR;
	}
	
	result = setjmp(png_jmpbuf(png_ptr));
	if (result) {
		log_error("Error reading texture: %s - Failed to create longjump buffer 1", file_path);
		fclose(f);
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		return STATUS_PNG_ERR;
	}

	png_init_io(png_ptr, f);
	png_set_sig_bytes(png_ptr, PNG_SIG_SIZE);

	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
	
	width = png_get_image_width(png_ptr, info_ptr);
	height = png_get_image_height(png_ptr, info_ptr);
	color_type = png_get_color_type(png_ptr, info_ptr);	
	bit_depth = png_get_bit_depth(png_ptr, info_ptr);
	row_ptrs = png_get_rows(png_ptr, info_ptr);
	row_size = png_get_rowbytes(png_ptr, info_ptr);
	
	data = malloc(row_size * height);
	if (!data) {
		log_error("Error reading texture: %s - Out of Memory", file_path);
		fclose(f);
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		return STATUS_OUT_OF_MEMORY;
	}

	for (i = 0; i < height; i++) {
		memcpy(&data[row_size*i], row_ptrs[i], row_size);
	}

	png->width = width;
	png->height = height;
	png->bit_depth = bit_depth;
	png->color_type = color_type;
	png->data = data;

	log_info("read png with width: %d, height: %d, color_type: %d", width, height, color_type);

	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	fclose(f);
	return STATUS_OK;
}

