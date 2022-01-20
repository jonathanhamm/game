#include "common/data-structures.h"
#include "log.h" 
#include "loadlevel.h" 
#include "models.h"
#include "meshes.h"
#include "common/errcodes.h"
#include "common/constants.h"
#include <assert.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <GL/glew.h>

#define BDB_VERTEXT_BUF 256

struct bob_db_s {
	sqlite3 *db;
	sqlite3_stmt *qambientgravity;
	sqlite3_stmt *qinstance;
	sqlite3_stmt *qrange;
	sqlite3_stmt *qlazyinstance;
	sqlite3_stmt *qmodel;
	sqlite3_stmt *qmesh;
	sqlite3_stmt *qshader;
	sqlite3_stmt *qtexture;
	IntMap models;
	IntMap shaders;
};

const char *level_ambient_gravity_qstr =
"SELECT ambientGravityX, ambientGravityY, ambientGravityZ"
" FROM level"
" WHERE name=?";
const char *instance_qstr = 
"SELECT modelID, levelID, vx, vy, vz, scalex, scaley, scalez, mass, isSubjectToGravity, isStatic"
" FROM level AS l JOIN instance AS i"
" ON l.id=i.levelID"
" WHERE l.name=?";
const char *range_qstr =
"SELECT r.id, steps, var, cache, child"
" FROM level AS l"
" JOIN range AS r ON l.id=r.levelID"
" WHERE l.name=?";
const char *lazy_instance_qstr =
"SELECT id, modelID, vx, vy, vz, scalex, scaley, scalez, mass, isSubjectToGravity, isStatic"
" FROM lazy_instance"
" WHERE rangeID=?";
const char *model_qstr = 
"SELECT meshID, programID, textureID, hasUV"
" FROM model"
" WHERE id=?";
const char *mesh_qstr =
"SELECT name, data"
" FROM mesh"
" WHERE id=?";
const char *shader_qstr = 
"SELECT name, type, src"
" FROM shader AS s JOIN program_xref AS px"
" ON s.id=px.shaderID"
" WHERE px.programID=?";
const char *texture_qstr =
"SELECT path FROM texture AS t"
" WHERE t.id=?";

static sqlite3_stmt *query_level;

static int prepare_queries(bob_db_s *bdb);
static int bob_dbload_ambient_gravity(Level *lvl, bob_db_s *bdb, const char *name);
static int bob_dbload_instances(Level *lvl, bob_db_s *bdb, const char *name);
static int bob_dbload_ranges(Level *lvl, bob_db_s *bdb, const char *name);
static int bob_dbload_lazy_instances(Level *lvl, Range *range, bob_db_s *bdb, 
		int rangeId, PointerVector *pv);
static Model *bob_dbload_model(bob_db_s *bdb, int modelID);
static void bob_dbload_mesh(bob_db_s *bdb, Model *m, int meshID);
static int bob_dbload_program(bob_db_s *bdb, Model *m, int programID);
static int bob_dbload_texture(bob_db_s *bdb, Model *m, int textureID);
static int bob_parse_vertices(FloatBuf *fbuf, const unsigned char *vertext);

/** Range Partitioning **/
static void bob_get_range_roots(PointerVector *ranges, PointerVector *result);
static void bob_visit_range_for_model(PointerVector *rangeRoots, Range *range);
static RangeRoot *ll_range_get_range_root(PointerVector *rangeRoots, Model *model);
static void range_get_path(PointerVector *pv, Range *range);
static void range_add_filtered_instances(Range *newRange, Range *oldRange, Model *m);
static Range *createRangeClone(Range *range, Model *m, Range *parent);
static void range_add_node_range(Range *range, Range *parent, Model *model, 
    PointerVector *path, int index);
static void range_add_node(RangeRoot *rangeRoot, PointerVector *path);
/** **/

/** misc **/
static GLenum to_gl_shader(bob_shader_e shader_type);


bob_db_s *bob_loaddb(const char *path) {
	int rc;

	bob_db_s *bdb = malloc(sizeof *bdb);
	if (!bdb) {
		log_error("memory allocation error");
		return NULL;
	}

	bob_int_map_init(&bdb->models);
	bob_int_map_init(&bdb->shaders);
	rc = sqlite3_open_v2(path, &bdb->db, SQLITE_OPEN_READONLY, NULL);
	if (rc != SQLITE_OK) {
		log_error("error opening database");
		sqlite3_close(bdb->db);
		return NULL;
	}

	rc = prepare_queries(bdb);
	if (rc) {
		log_error("Failed to prepare queries");
		return NULL;
	}
	return bdb;
}

Level *bob_loadlevel(bob_db_s *bdb, const char *name) {
	int rc, i;

	Level *lvl = malloc(sizeof *lvl);
	if (!lvl) {
		log_error("failed to allocate memory for while loading level");
		return NULL;
	}

  lvl->renderBuffer.pos = 0;

	rc = bob_dbload_ambient_gravity(lvl, bdb, name);
	if (rc < 0)
		return NULL;
	rc = bob_dbload_instances(lvl, bdb, name);
	if (rc < 0)
		return NULL;
	rc = bob_dbload_ranges(lvl, bdb, name);
	if (rc < 0)
		return NULL;
	return lvl;
}

int prepare_queries(bob_db_s *bdb) {
	int rc;

	rc = sqlite3_prepare_v2(bdb->db, level_ambient_gravity_qstr, -1, 
			&bdb->qambientgravity, 0);
	if (rc != SQLITE_OK) {
		log_error("failed to prepare ambientGravity query");
		return -1;
	}
	rc = sqlite3_prepare_v2(bdb->db, instance_qstr, -1, &bdb->qinstance, 0);
	if (rc != SQLITE_OK) {
		log_error("failed to prepare instance query");
		return -1;
	}
	rc = sqlite3_prepare_v2(bdb->db, range_qstr, -1, &bdb->qrange, 0);
	if (rc != SQLITE_OK) {
		log_error("failed to prepare range query");
		return -1;
	}
	rc = sqlite3_prepare_v2(bdb->db, lazy_instance_qstr, -1, &bdb->qlazyinstance, 0);
	if (rc != SQLITE_OK) {
		log_error("failed to prepare lazy instance query");
		return -1;
	}
	rc = sqlite3_prepare_v2(bdb->db, model_qstr, -1, &bdb->qmodel, 0);
	if (rc != SQLITE_OK) {
		log_error("failed to prepare model query");
		return -1;
	}
	rc = sqlite3_prepare_v2(bdb->db, mesh_qstr, -1, &bdb->qmesh, 0);
	if (rc != SQLITE_OK) {
		log_error("failed to prepare mesh query");
		return -1;
	}
	rc = sqlite3_prepare_v2(bdb->db, shader_qstr, -1, &bdb->qshader, 0);
	if (rc != SQLITE_OK) {
		log_error("failed to prepare shader query");
		return -1;
	}
	rc = sqlite3_prepare_v2(bdb->db, texture_qstr, -1, &bdb->qtexture, 0);
	if (rc != SQLITE_OK) {
		log_error("failed to prepare texture query");
		return -1;
	}
	log_info("texture handle: %p", bdb->qtexture);
	return 0;
}

int bob_dbload_ambient_gravity(Level *lvl, bob_db_s *bdb, const char *name) {
	int rc;
	double agx, agy, agz;

	log_debug("loading ambient gravity");

	rc = sqlite3_bind_text(bdb->qambientgravity, 1, name, -1, NULL);
	if (rc != SQLITE_OK) {
		log_error("failed to bind level name parameter to level query");
		return -1;
	}
	rc = sqlite3_step(bdb->qambientgravity);
	if (rc == SQLITE_ROW) {
		agx = sqlite3_column_double(bdb->qambientgravity, 0);
		agy = sqlite3_column_double(bdb->qambientgravity, 1);
		agz = sqlite3_column_double(bdb->qambientgravity, 2);
		lvl->ambient_gravity[0] = agx;
		lvl->ambient_gravity[1] = agy;
		lvl->ambient_gravity[2] = agz;
	}
	else {
		log_error("Databse error occurred during ambient gravity query.");
		return -1;
	}
	rc = sqlite3_step(bdb->qambientgravity);
	if (rc != SQLITE_DONE) {
		log_error("Database in invalid format at ambient gravity query.");
		return -1;
	}
  return 0;
}

int bob_dbload_instances(Level *lvl, bob_db_s *bdb, const char *name) {
	int rc;

	log_debug("loading level instances");

	rc = sqlite3_bind_text(bdb->qinstance, 1, name, -1, NULL);
	if (rc != SQLITE_OK) {
		log_error("failed to bind level name parameter to level query");
		return -1;
	}

	int modelID, levelID;
	double vx, vy, vz, scalex, scaley, scalez, mass;
	bool isSubjectToGravity, isStatic;
	Instance *inst;
	Model *model;
	pointer_vector_init(&lvl->instances);
	pointer_vector_init(&lvl->gravityObjects);
	while (1) {
		rc = sqlite3_step(bdb->qinstance);
		if (rc == SQLITE_ROW) {
			modelID = sqlite3_column_int(bdb->qinstance, 0);
			levelID = sqlite3_column_int(bdb->qinstance, 1);
			vx = sqlite3_column_double(bdb->qinstance, 2);
			vy = sqlite3_column_double(bdb->qinstance, 3);
			vz = sqlite3_column_double(bdb->qinstance, 4);
			scalex = sqlite3_column_double(bdb->qinstance, 5);
			scaley = sqlite3_column_double(bdb->qinstance, 6);
			scalez = sqlite3_column_double(bdb->qinstance, 7);
			mass = sqlite3_column_double(bdb->qinstance, 8);
			isSubjectToGravity = sqlite3_column_int(bdb->qinstance, 9);
			isStatic = sqlite3_column_int(bdb->qinstance, 10);
			model = bob_dbload_model(bdb, modelID);
			inst = calloc(1, sizeof *inst);
			if (!inst) {
				log_error("failed to allocate memory for instance");
				return -1;
			}
			if (isSubjectToGravity) {
				pointer_vector_add(&lvl->gravityObjects, inst);
			}
			inst->impulse = NULL;
			inst->model = model;
			inst->pos[0] = vx;
			inst->pos[1] = vy;
			inst->pos[2] = vz;
			inst->scale[0] = scalex;
			inst->scale[1] = scaley;
			inst->scale[2] = scalez;
			inst->mass = mass;
			inst->isSubjectToGravity = isSubjectToGravity;
			inst->isStatic = isStatic;
			glm_vec3_zero(inst->velocity);
			glm_vec3_zero(inst->acceleration);
			glm_vec3_zero(inst->force);
			inst->rotation[0] = 2;
			inst->rotation[1] = 1;
			inst->rotation[0] = 0;

			instance_group_add(&lvl->instances, model, inst);
		}
		else if (rc == SQLITE_DONE) {
			break;
		}
		else {
			log_error("Unexpected result from database level query: %d\n", rc);
			return -1;
		}
	}
	sqlite3_reset(bdb->qinstance);
	return 0;
}

int bob_dbload_ranges(Level *lvl, bob_db_s *bdb, const char *name) {
	int i, rc;
	int steps, modelId, rangeId, childId;
	const unsigned char *var;
	bool cache;
	Range *range;
	IntMap rangeMap;
	PointerVector loadRanges;
  PointerVector rangeRoots;

	rc = sqlite3_bind_text(bdb->qrange, 1, name, -1, NULL);
	if (rc != SQLITE_OK) {
		log_error("failed to bind level name parameter to range query");
		return -1;
	}

	pointer_vector_init(&loadRanges);
	bob_int_map_init(&rangeMap);

	while (1) {
		rc = sqlite3_step(bdb->qrange);
		if (rc == SQLITE_ROW) {
			rangeId = sqlite3_column_int(bdb->qrange, 0);	
			steps = sqlite3_column_int(bdb->qrange, 1);	
			var = sqlite3_column_text(bdb->qrange, 2);
			cache = sqlite3_column_int(bdb->qrange, 3);
			childId = sqlite3_column_int(bdb->qrange, 4);

			range = calloc(1, sizeof *range);
			if (!range) {
				log_error("failed to allocate memory for range");
				return -1;
			}
      range->id = rangeId;
			range->steps = steps;
			range->var = *var;
			range->cache = cache;
      range->child = NULL;
			range->childId = childId;
			range->currval = 0;
			range->parent = NULL;

			rc = bob_dbload_lazy_instances(lvl, range, bdb, rangeId, 
					&range->lazyinstances);
			if (rc) {
				return -1;
			}

			bob_int_map_insert(&rangeMap, rangeId, range);

			pointer_vector_add(&loadRanges, range);
		}
		else if (rc == SQLITE_DONE) {
			break;	
		}
		else {
			log_error("Unexpected result from database range query: %d\n", rc);
			return -1;
		}
	}

	sqlite3_reset(bdb->qrange);

	/* assign pointers */
	for (i = 0; i < loadRanges.size; i++) {
		Range *curr = loadRanges.buffer[i];
		if (curr->childId) {
			Range *child = bob_int_map_get(&rangeMap, curr->childId);
			curr->child = child;
			child->parent = curr;
		}
	}
	bob_int_map_free(&rangeMap);

	/* Assign root ranges to ranges vector */
  pointer_vector_init(&rangeRoots);
	for (i = 0; i < loadRanges.size; i++) {
		Range *curr = loadRanges.buffer[i];
		if (!curr->parent) {
			pointer_vector_add(&rangeRoots, curr);
		}
	}
	pointer_vector_free(&loadRanges);

  /* Partition Ranges (testing) */
	pointer_vector_init(&lvl->ranges);
  bob_get_range_roots(&rangeRoots, &lvl->ranges);
  pointer_vector_free(&rangeRoots);

  for (i = 0; i < lvl->ranges.size; i++) {
    log_info("range partition: %p", lvl->ranges.buffer[i]);
  }

	return 0;
}

int bob_dbload_lazy_instances(Level *lvl, Range *range, bob_db_s *bdb, 
		int rangeID, PointerVector *pv) {
	int rc, id, modelID;
	const unsigned char *vx, *vy, *vz, *scalex, *scaley, *scalez;
	float mass;
	bool isSubjectToGravity, isStatic;
	Model *model;
	LazyInstance *li;

	pointer_vector_init(&range->lazyinstances);

	rc = sqlite3_bind_int(bdb->qlazyinstance, 1, rangeID);
	if (rc != SQLITE_OK) {
		log_error(
				"failed to bind rangeId parameter to lazy instance query: errno %d", 
				rc);
		return -1;
	}
	while (1) {
		rc = sqlite3_step(bdb->qlazyinstance);
		if (rc == SQLITE_ROW) {
      id = sqlite3_column_int(bdb->qlazyinstance, 0);
			modelID = sqlite3_column_int(bdb->qlazyinstance, 1);
			vx = sqlite3_column_text(bdb->qlazyinstance, 2);
			vy = sqlite3_column_text(bdb->qlazyinstance, 3);
			vz = sqlite3_column_text(bdb->qlazyinstance, 4);
			scalex = sqlite3_column_text(bdb->qlazyinstance, 5);
			scaley = sqlite3_column_text(bdb->qlazyinstance, 6);
			scalez = sqlite3_column_text(bdb->qlazyinstance, 7);
			mass = sqlite3_column_double(bdb->qlazyinstance, 8);
			isSubjectToGravity = sqlite3_column_int(bdb->qlazyinstance, 9);
			isStatic = sqlite3_column_int(bdb->qlazyinstance, 10);

			li = malloc(sizeof *li);
			if (!li) {
				log_error("memory allocation error for new lazy instance");
				return -1;
			}
      li->id = id;
			li->px = bob_dup_str((const char *)vx);
			li->py = bob_dup_str((const char *)vy);
			li->pz = bob_dup_str((const char *)vz);
			li->scalex = bob_dup_str((const char *)scalex);
			li->scaley = bob_dup_str((const char *)scaley);
			li->scalez = bob_dup_str((const char *)scalez);
			li->mass = mass;
			li->isSubjectToGravity = isSubjectToGravity;
			li->isStatic = isStatic;
			model = bob_dbload_model(bdb, modelID);
			li->model = model;
			glm_vec3_zero(li->velocity);
			glm_vec3_zero(li->acceleration);
			glm_vec3_zero(li->force);
			li->rotation[0] = 0;
			li->rotation[1] = 0;
			li->rotation[2] = 0;

      log_info("added %s", li->px);
      pointer_vector_add(&range->lazyinstances, li);
		}
		else if (rc == SQLITE_DONE) {
			break;
		}
		else {
			log_error("Unexpected result from database lazy instance query: %d\n", 
					rc);
			return -1;
		}
	}
	sqlite3_reset(bdb->qlazyinstance);
	return 0;
}

Model *bob_dbload_model(bob_db_s *bdb, int modelID) {
	int rc;
	Model *m;

	m = bob_int_map_get(&bdb->models, modelID);
	if (m)
		return m;
	m = malloc(sizeof *m);
	if (!m) {
		log_error("Error allocating memory for model");
		return NULL;
	}

	m->drawType = GL_TRIANGLE_STRIP;
	//m->drawType = GL_LINE_LOOP;
	m->drawStart = 0;
	m->drawCount = 6*2*3;
	log_debug("loading model %d", modelID);
	rc = sqlite3_bind_int(bdb->qmodel, 1, modelID);
	if (rc != SQLITE_OK) {
		log_error("failed to bind modelID parameter to model query");
		return NULL;
	}

	int meshID, programID, textureID, hasUV;
	rc = sqlite3_step(bdb->qmodel);
	if (rc == SQLITE_ROW) {
		meshID = sqlite3_column_int(bdb->qmodel, 0);
		programID = sqlite3_column_int(bdb->qmodel, 1);
		textureID = sqlite3_column_int(bdb->qmodel, 2);
		hasUV = sqlite3_column_int(bdb->qmodel, 3);
		bob_dbload_program(bdb, m, programID);
		bob_dbload_mesh(bdb, m, meshID);
		bob_dbload_texture(bdb, m, textureID);
	}
	rc = sqlite3_step(bdb->qmodel);
	if (rc != SQLITE_DONE) {
		log_error("Database in invalid format");
		return NULL;
	}
	sqlite3_reset(bdb->qmodel);
	bob_int_map_insert(&bdb->models, modelID, m);
	return m;
}

void bob_dbload_mesh(bob_db_s *bdb, Model *m, int meshID) {
	int rc;
	GLint handle;
	CharBuf mbuf;   
	FloatBuf fbuf;

	log_debug("Loading mesh %d", meshID);
	char_buf_init(&mbuf);

	rc = sqlite3_bind_int(bdb->qmesh, 1, meshID);
	if (rc != SQLITE_OK) {
		log_error("failed to bind meshID parameter to mesh query");
		return;
	}
	const unsigned char *name, *data;
	rc = sqlite3_step(bdb->qmesh);
	if (rc == SQLITE_ROW) {
		name = sqlite3_column_text(bdb->qmesh, 0);
		data = sqlite3_column_text(bdb->qmesh, 1);
		log_info("loaded mesh of name %s with data %s", name, data);
		bob_parse_vertices(&fbuf, data);

		glGenBuffers(1, &m->vbo);
		glGenVertexArrays(1, &m->vao);

		glBindVertexArray(m->vao);
		glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
		glBufferData(GL_ARRAY_BUFFER, fbuf.size * sizeof(GLfloat), fbuf.buffer, 
				GL_STATIC_DRAW);

		handle = gl_shader_attrib(m->program, "vert");
		glEnableVertexAttribArray(handle);
		glVertexAttribPointer(handle, 3, GL_FLOAT, GL_FALSE, 5*sizeof(GLfloat), 
				NULL);

		handle = gl_shader_attrib(m->program, "vertexCoord");
		glEnableVertexAttribArray(handle);
		glVertexAttribPointer(handle, 2, GL_FLOAT, GL_TRUE, 5*sizeof(GLfloat), 
				(const GLvoid *)(3*sizeof(GLfloat)));


    glGenBuffers(1, &m->pvbo);
    glBindBuffer(GL_ARRAY_BUFFER, m->pvbo);
    handle = gl_shader_attrib(m->program, "pos");
    glEnableVertexAttribArray(handle);
    glVertexAttribPointer(handle, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glVertexAttribDivisor(handle, 1);

    handle = gl_shader_attrib(m->program, "scale");
    glEnableVertexAttribArray(handle);
    glVertexAttribPointer(handle, 3, GL_FLOAT, GL_FALSE, 0, (void *)(RENDER_BUFFER_SIZE * sizeof(vec3)));
    glVertexAttribDivisor(handle, 1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
	}
	rc = sqlite3_step(bdb->qmesh);
	if (rc != SQLITE_DONE) {
		log_error("Database in invalid format for reading meshes");
		return;
	}
	sqlite3_reset(bdb->qmesh);
	//bob_int_map_insert(&bdb->models, meshID, m);
	char_buf_free(&mbuf);
	float_buf_free(&fbuf);
}

//TODO: deal with memory leaks
int bob_dbload_program(bob_db_s *bdb, Model *m, int programID) {
	int rc;
	GlShader *shader;
	GlProgram *program = malloc(sizeof *program);
	PointerVector pv;

	rc = sqlite3_bind_int(bdb->qshader, 1, programID);
	if (rc != SQLITE_OK) {
		log_error("failed to bind shaderID parameter to shader query");
		return -1;
	}
	const unsigned char *name;
	GLenum gl_type;
	bob_shader_e bob_type;
	const GLchar *src;

	pointer_vector_init(&pv);
	while (1) {
		rc = sqlite3_step(bdb->qshader);
		if (rc == SQLITE_ROW) {
			name = sqlite3_column_text(bdb->qshader, 0);
			bob_type = sqlite3_column_int(bdb->qshader, 1);
			src = (GLchar *)sqlite3_column_text(bdb->qshader, 2);
			gl_type = to_gl_shader(bob_type);
			shader = malloc(sizeof *shader);
      if (!shader) {
        log_error("failed to allocate memory for shader");
        exit(1);
      }
			rc = gl_load_shader(shader, gl_type, src, (const char *)name);
			if (rc != STATUS_OK) {
				log_error("failed to load shader: %s.", name);
				return -1;
			}
			log_info("ready %s - %d - %s", name, gl_type, src);
			pointer_vector_add(&pv, shader);
		}
		else if (rc == SQLITE_DONE) {
			gl_create_program(program, pv);
			m->program = program;
			break;
		}
		else {
			log_error("Unexpected result from database shader query: %d\n", rc);
			return -1;
		}
	}
	sqlite3_reset(bdb->qshader);
	pointer_vector_free(&pv);
	return 0;
}


//TODO: deal with memory leaks
int bob_dbload_texture(bob_db_s *bdb, Model *m, int textureID) {
	int rc;
	GlTexture *texture;
	const unsigned char *path;

	rc = sqlite3_bind_int(bdb->qtexture, 1, textureID);
	if (rc != SQLITE_OK) {
		log_error("failed to bind textureID parameter to texture query %d", rc);
		return -1;
	}
	rc = sqlite3_step(bdb->qtexture);
	if (rc == SQLITE_ROW) {
		path = sqlite3_column_text(bdb->qtexture, 0);
		log_info("selected path: %s using id: %d", path, textureID);
		texture = malloc(sizeof *texture);
		if (!texture) {
			log_error("memory allocation error");
		}
		gl_load_texture(texture, (const char *)path);
		m->texture = texture;
	}
	else {
		log_error("unexpected result from texture query: %d", rc);
		return -1;
	}
	rc = sqlite3_step(bdb->qtexture);
	if (rc != SQLITE_DONE) {
		log_error("Database in invalid format");
		return -1;
	}
	sqlite3_reset(bdb->qtexture);
	return 0;
}

int bob_parse_vertices(FloatBuf *fbuf, const unsigned char *vertext) {
	unsigned char buf[BDB_VERTEXT_BUF], 
								*bptr = buf;
	const unsigned char *fptr = vertext;
	GLfloat num;

	float_buf_init(fbuf);
	while (*fptr) {
		switch (*fptr) {
			case ' ':
			case '\t':
			case '\n':
			case '\r':
				fptr++;
				break;
			case ',':
				fptr++;
				break;
			default:
				if (*fptr == '-')
					*bptr++ = *fptr++;
				if (isdigit(*fptr)) {
					do {
						if (bptr - buf < BDB_VERTEXT_BUF - 1) {
							*bptr++ = *fptr++;
						}
						else {
							log_error("Number in mesh vertices too long");
							return -1;
						}
					} while (isdigit(*fptr));

					if (*fptr == '.') {
						if (bptr - buf < BDB_VERTEXT_BUF - 1) {
							*bptr++ = *fptr++;
							if (isdigit(*fptr)) {
								do {
									if (bptr - buf < BDB_VERTEXT_BUF - 1) {
										*bptr++ = *fptr++;
									}
									else {
										log_error("Number in mesh vertices too long");
										return -1;
									}
								} while (isdigit(*fptr));
							}
						}
						else {
							log_error("Number in mesh vertices too long");
							return -1;
						}
					}
					*bptr = '\0';
					num = atof((const char*)buf);
					float_add_f(fbuf, num);
					bptr = buf;
				}
				else {
					log_error("invalid character in mesh data: %c", *fptr);
					fptr++;
				}
				break;
		}
	}
	return 0;
}

void bob_get_range_roots(PointerVector *ranges, PointerVector *result) {
  int i, j;
  for (i = 0; i < ranges->size; i++) {
    Range *currRange = ranges->buffer[i];
	  bob_visit_range_for_model(result, currRange);
  }
}

void bob_visit_range_for_model(PointerVector *rangeRoots, Range *range) {
	size_t i;
	for (i = 0; i < range->lazyinstances.size; i++) {
		LazyInstance *li = range->lazyinstances.buffer[i];
    Model *model = li->model;
    PointerVector rangePath;
    RangeRoot *rangeRoot = ll_range_get_range_root(rangeRoots, li->model);
    if (!rangeRoot) {
      log_debug("creating new range root for model: %p", li->model);
      rangeRoot = malloc(sizeof *rangeRoot);
      if (!rangeRoot) {
        log_error("error allocating memory for Rangeroot");
        return;
      }
      rangeRoot->m = model;
      pointer_vector_init(&rangeRoot->ranges);
      pointer_vector_add(rangeRoots, rangeRoot);
    } 	
    pointer_vector_init(&rangePath);
    range_get_path(&rangePath, range);
    range_add_node(rangeRoot, &rangePath);
    pointer_vector_free(&rangePath);
  }
	if (range->child) {
		bob_visit_range_for_model(rangeRoots, range->child);
	}
}

RangeRoot *ll_range_get_range_root(PointerVector *rangeRoots, Model *model) {
	size_t i;
	for (i = 0; i < rangeRoots->size; i++) {
		RangeRoot *rangeRoot = rangeRoots->buffer[i];
		if (rangeRoot->m == model)
			return rangeRoot;
	}
	return NULL;
}

/** TODO: reconsider the need for this **/
void range_get_path(PointerVector *pv, Range *range) {
  while (range) {
    pointer_vector_add(pv, range);
    range = range->parent;
  }
}

void range_add_filtered_instances(Range *newRange, Range *oldRange, Model *m) {
  int i;

  pointer_vector_init(&newRange->lazyinstances);
  for (i = 0; i < oldRange->lazyinstances.size; i++) {
    LazyInstance *li = oldRange->lazyinstances.buffer[i];
    if (m == li->model) {
      pointer_vector_add(&newRange->lazyinstances, li);
    }
  }
}

Range *createRangeClone(Range *range, Model *m, Range *parent) {
  Range *rangeClone = malloc(sizeof *rangeClone);
  if (!rangeClone) {
    log_error("memory allocation error while cloning range");
    return NULL;
  }
  rangeClone->id = range->id;
  rangeClone->parent = parent;
  if (parent) {
    parent->child = rangeClone;
  }
  rangeClone->steps = range->steps;
  rangeClone->currval = range->currval;
  rangeClone->var = range->var;
  rangeClone->cache = range->cache;
  range_add_filtered_instances(rangeClone, range, m);
  rangeClone->child = NULL;
  return rangeClone;
}

void range_add_node_range(Range *range, Range *parent, Model *model, PointerVector *path, int index) {
  if (index < 0)
    return;
  Range *currPathRange = path->buffer[index];
  if (!range) {
    range = createRangeClone(currPathRange, model, parent);
    range_add_node_range(range->child, range, model, path, index - 1);
  }
  if (range->id == currPathRange->id) {
    range_add_node_range(range->child, range, model, path, index - 1); 
  } else {
    log_error("Invalid State: got range id of %d, but expected %d", range->id, currPathRange->id);
  }
}

void range_add_node(RangeRoot *rangeRoot, PointerVector *path) {
  int i;
  int j = path->size - 1;
  Range *currPathRange = path->buffer[j];
  Model *model = rangeRoot->m;

  for (i = 0; i < rangeRoot->ranges.size; i++) {
    Range *currRange = rangeRoot->ranges.buffer[i];
    if (currRange->id == currPathRange->id) {
      range_add_node_range(currRange->child, currRange, model, path, j - 1);
      return;
    }
  }
  if (i == rangeRoot->ranges.size) {
    Range *newRange = createRangeClone(currPathRange, rangeRoot->m, NULL);
    range_add_node_range(newRange->child, newRange, model, path, j - 1);
    pointer_vector_add(&rangeRoot->ranges, newRange);
  }
}   

GLenum to_gl_shader(bob_shader_e shader_type) {
	switch(shader_type) {
		case BOB_VERTEX_SHADER:
			return GL_VERTEX_SHADER;
		case BOB_TESS_EVAL_SHADER:
			return GL_TESS_EVALUATION_SHADER;
		case BOB_TESS_CONTROL_SHADER:
			return GL_TESS_CONTROL_SHADER;
		case BOB_GEOMETRY_SHADER:
			return GL_GEOMETRY_SHADER;
		case BOB_FRAGMENT_SHADER:
			return GL_FRAGMENT_SHADER;
		case BOB_COMPUTE_SHADER:
			return GL_COMPUTE_SHADER;
		default:
			log_error("unknown shader type: %d", shader_type);
			break;
	}
	return -1;
}

