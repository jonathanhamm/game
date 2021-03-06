#include "meshes.h"

GLfloat test_mesh1[] = {
	//  X     Y     Z       U     V
	// bottom
	-1.0f,-1.0f,-1.0f,   0.0f, 0.0f,
	1.0f,-1.0f,-1.0f,   1.0f, 0.0f,
	-1.0f,-1.0f, 1.0f,   0.0f, 1.0f,
	1.0f,-1.0f,-1.0f,   1.0f, 0.0f,
	1.0f,-1.0f, 1.0f,   1.0f, 1.0f,
	-1.0f,-1.0f, 1.0f,   0.0f, 1.0f,

	// top
	-1.0f, 1.0f,-1.0f,   0.0f, 0.0f,
	-1.0f, 1.0f, 1.0f,   0.0f, 1.0f,
	1.0f, 1.0f,-1.0f,   1.0f, 0.0f,
	1.0f, 1.0f,-1.0f,   1.0f, 0.0f,
	-1.0f, 1.0f, 1.0f,   0.0f, 1.0f,
	1.0f, 1.0f, 1.0f,   1.0f, 1.0f,

	// front
	-1.0f,-1.0f, 1.0f,   1.0f, 0.0f,
	1.0f,-1.0f, 1.0f,   0.0f, 0.0f,
	-1.0f, 1.0f, 1.0f,   1.0f, 1.0f,
	1.0f,-1.0f, 1.0f,   0.0f, 0.0f,
	1.0f, 1.0f, 1.0f,   0.0f, 1.0f,
	-1.0f, 1.0f, 1.0f,   1.0f, 1.0f,

	// back
	-1.0f,-1.0f,-1.0f,   0.0f, 0.0f,
	-1.0f, 1.0f,-1.0f,   0.0f, 1.0f,
	1.0f,-1.0f,-1.0f,   1.0f, 0.0f,
	1.0f,-1.0f,-1.0f,   1.0f, 0.0f,
	-1.0f, 1.0f,-1.0f,   0.0f, 1.0f,
	1.0f, 1.0f,-1.0f,   1.0f, 1.0f,

	// left
	-1.0f,-1.0f, 1.0f,   0.0f, 1.0f,
	-1.0f, 1.0f,-1.0f,   1.0f, 0.0f,
	-1.0f,-1.0f,-1.0f,   0.0f, 0.0f,
	-1.0f,-1.0f, 1.0f,   0.0f, 1.0f,
	-1.0f, 1.0f, 1.0f,   1.0f, 1.0f,
	-1.0f, 1.0f,-1.0f,   1.0f, 0.0f,

	// right
	1.0f,-1.0f, 1.0f,   1.0f, 1.0f,
	1.0f,-1.0f,-1.0f,   5.0f, 0.0f,
	1.0f, 1.0f,-1.0f,   0.0f, 0.0f,
	1.0f,-1.0f, 1.0f,   1.0f, 1.0f,
	1.0f, 1.0f,-1.0f,   0.0f, 0.0f,
	1.0f, 1.0f, 1.0f,   0.0f, 1.0f
};

