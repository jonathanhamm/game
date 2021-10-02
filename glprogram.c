#include "log.h"
#include "glprogram.h"
#include "common/errcodes.h"
#include <stdio.h>
#include <string.h>
#include <setjmp.h>


static void print_png_version(void);
static int load_png(Png *png, const char *file_path);
static GLenum gl_map_color_type(png_byte color_type);


int gl_create_program_t1(GlProgram *program, GlShader *vertex_shader, GlShader *fragment_shader) {
	int phandle;
	GLint gstatus;

	phandle = glCreateProgram();
	if (!phandle) {
		log_error("Failed to create gl program, failed on call to glCreateProgram()");
		return STATUS_GL_ERR;
	} 

	glAttachShader(phandle, vertex_shader->handle);
	glAttachShader(phandle, fragment_shader->handle);
	glLinkProgram(phandle);

	glGetProgramiv(phandle, GL_LINK_STATUS, &gstatus);	
	if (gstatus == GL_FALSE) {
		GLint info_log_length;
		char *err_str;

		glGetProgramiv(phandle, GL_INFO_LOG_LENGTH, &info_log_length);
		err_str = malloc(info_log_length + 1);
		if (!err_str) {
			log_error("Failed to create gl program: glLinkProgram() failed, and out of memory");
			glDeleteProgram(phandle);
			return STATUS_OUT_OF_MEMORY;
		}
		else {
			glGetProgramInfoLog(phandle, info_log_length, NULL, err_str);	
			log_error("Failed to create gl program: glLinkProgram() failed - %s", err_str);
			free(err_str);
			glDeleteProgram(phandle);
			return STATUS_GL_ERR;
		}
	}

	program->handle = phandle;

	return STATUS_OK;
}

int gl_create_program(GlProgram *program, PointerVector shaders) {
	size_t i;
	int phandle;
	GLint gstatus;

	phandle = glCreateProgram();
	if (!phandle) {
		log_error("Failed to create gl program, failed on call to glCreateProgram()");
		return STATUS_GL_ERR;
	} 

	for (i = 0; i < shaders.size; i++) {
		GlShader *shader = shaders.buffer[i];
		glAttachShader(phandle, shader->handle);
	}

	glLinkProgram(phandle);

	glGetProgramiv(phandle, GL_LINK_STATUS, &gstatus);	
	if (gstatus == GL_FALSE) {
		GLint info_log_length;
		char *err_str;

		glGetProgramiv(phandle, GL_INFO_LOG_LENGTH, &info_log_length);
		err_str = malloc(info_log_length + 1);
		if (!err_str) {
			log_error("Failed to create gl program: glLinkProgram() failed, and out of memory");
			glDeleteProgram(phandle);
			return STATUS_OUT_OF_MEMORY;
		}
		else {
			glGetProgramInfoLog(phandle, info_log_length, NULL, err_str);	
			log_error("Failed to create gl program: glLinkProgram() failed - %s", err_str);
			free(err_str);
			glDeleteProgram(phandle);
			return STATUS_GL_ERR;
		}
	}
	program->handle = phandle;
	return STATUS_OK;
}

GLint gl_shader_attrib(GlProgram *program, const GLchar *attrib_name) {
	GLint attrib = glGetAttribLocation(program->handle, attrib_name);
	if (attrib == -1) {
		log_error("Attribute not found: %s", attrib_name);
	}
	return attrib;
}

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
	shader->type = shader_type;
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
	GlTexture texture;
	print_png_version();
	const char *test = "shaders/test.vsh";
	gl_load_shader_from_file(&shader, GL_VERTEX_SHADER, test, test);

	gl_load_texture(&texture, "textures/pge_icon.png");
}

void gl_delete_shader(GlShader *shader) {
	glDeleteShader(shader->handle);
}

void print_png_version(void) {
	log_info("Compiled with libpng %s; using libpng %s", PNG_LIBPNG_VER_STRING, png_libpng_ver);
	//log_info("Compiled with zlib %s; using zlib %s.", ZLIB_VERSION, zlib_version);
};

int gl_load_texture(GlTexture *texture, const char *file_path) {
	int result, handle;

	result = load_png(&texture->png, file_path);
	if (result) {
		log_error("Error loading texture %s", file_path);
		return result;
	}

	glGenTextures(1, &handle);
	glBindTexture(GL_TEXTURE_2D, handle);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	GLenum gl_color_type = gl_map_color_type(texture->png.color_type);
	glTexImage2D(GL_TEXTURE_2D, 0, gl_color_type, texture->png.width, texture->png.height, 0, gl_color_type, GL_UNSIGNED_BYTE, texture->png.data);
	glBindTexture(GL_TEXTURE_2D, 0);
	texture->handle = handle;

	return STATUS_OK;
}

int load_png(Png *png, const char *file_path) {
	enum { PNG_SIG_SIZE = 8 };
	FILE *f;			
	char *data;
	png_byte sig[PNG_SIG_SIZE];
	int result, i;
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
		log_error("Error reading png: %s is not a valid png file", file_path);
		fclose(f);
		return STATUS_PNG_ERR;
	}

	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,
			NULL, NULL);
	if (!png_ptr) {
		log_error("Error reading png: %s - Failed on png_create_read_struct()", file_path);
		fclose(f);
		return STATUS_PNG_ERR;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!png_ptr) {
		log_error("Error reading png: %s - Failed on png_create_info_struct()", file_path);
		fclose(f);
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		return STATUS_PNG_ERR;
	}

	png_infop end_info = png_create_info_struct(png_ptr);
	if (!end_info) {
		log_error("Error reading png: %s - Failed on png_create_info_struct()", file_path);
		fclose(f);
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
		return STATUS_PNG_ERR;
	}

	result = setjmp(png_jmpbuf(png_ptr));
	if (result) {
		log_error("Error reading png: %s - Failed to create longjump buffer 1", file_path);
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
		log_error("Error reading png: %s - Out of Memory", file_path);
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

	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	fclose(f);
	return STATUS_OK;
}

void gl_delete_texture(GlTexture *texture) {
	glDeleteTextures(1, &texture->handle);
	free(texture->png.data);
};

GLenum gl_map_color_type(png_byte color_type) {
	switch(color_type) {
		case PNG_COLOR_TYPE_GRAY:
		case PNG_COLOR_TYPE_GRAY_ALPHA:
		case PNG_COLOR_TYPE_PALETTE:
		case PNG_COLOR_TYPE_RGB:
			return GL_RGB;
		case PNG_COLOR_TYPE_RGB_ALPHA:
			return GL_RGBA;
		default:
			log_error("unknown color type %d", color_type);
			return GL_INVALID_VALUE;
	}
}


