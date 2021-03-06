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
  sqlite3_stmt *qlevel;
  sqlite3_stmt *qinstanceplanes;
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
const char *instance_plane_qstr =
  "SELECT modelID, px, py, pz, vx, vy, vz, n1, n2"
  " FROM instance_plane"
  " WHERE levelID=?";
const char *level_qstr = 
  "SELECT modelID, levelID, vx, vy, vz, scalex, scaley, scalez, mass, isSubjectToGravity, isStatic"
  " FROM level AS l JOIN instance AS i"
  " ON l.id=i.levelID"
  " WHERE l.name=?";
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
static int bob_dbload_instance_planes(bob_db_s *bdb, Level *lvl, int levelID);
static InstancePlane *bob_dbload_instance_plane(bob_db_s *bdb);
static Model *bob_dbload_model(bob_db_s *bdb, int modelID);
static void bob_dbload_mesh(bob_db_s *bdb, Model *m, int meshID);
static int bob_dbload_program(bob_db_s *bdb, Model *m, int programID);
static int bob_dbload_texture(bob_db_s *bdb, Model *m, int textureID);
static int bob_parse_vertices(FloatBuf *fbuf, const unsigned char *vertext);
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

  rc = bob_dbload_ambient_gravity(lvl, bdb, name);
  if (rc < 0)
    return NULL;

  rc = sqlite3_bind_text(bdb->qlevel, 1, name, -1, NULL);
  if (rc != SQLITE_OK) {
    log_error("failed to bind level name parameter to level query");
    return NULL;
  }

  log_debug("loading level instances");

  int modelID, levelID;
  double vx, vy, vz, scalex, scaley, scalez, mass;
  bool isSubjectToGravity, isStatic;
  Instance *inst;
  Model *model;
  pointer_vector_init(&lvl->instances);
  pointer_vector_init(&lvl->gravityObjects);
  pointer_vector_init(&lvl->instance_planes);
  while (1) {
    rc = sqlite3_step(bdb->qlevel);
    if (rc == SQLITE_ROW) {
      modelID = sqlite3_column_int(bdb->qlevel, 0);
      levelID = sqlite3_column_int(bdb->qlevel, 1);
      vx = sqlite3_column_double(bdb->qlevel, 2);
      vy = sqlite3_column_double(bdb->qlevel, 3);
      vz = sqlite3_column_double(bdb->qlevel, 4);
      scalex = sqlite3_column_double(bdb->qlevel, 5);
      scaley = sqlite3_column_double(bdb->qlevel, 6);
      scalez = sqlite3_column_double(bdb->qlevel, 7);
      mass = sqlite3_column_double(bdb->qlevel, 8);
      isSubjectToGravity = sqlite3_column_int(bdb->qlevel, 9);
      isStatic = sqlite3_column_int(bdb->qlevel, 10);
      model = bob_dbload_model(bdb, modelID);
      inst = calloc(1, sizeof *inst);
      if (!inst) {
        log_error("failed to allocate memory for instance");
        return NULL;
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
      pointer_vector_add(&lvl->instances, inst);
    }
    else if (rc == SQLITE_DONE) {
      break;
    }
    else {
      log_error("Unexpected result from database level query: %d\n", rc);
      return NULL;
    }
  }
  sqlite3_reset(bdb->qlevel);
  bob_dbload_instance_planes(bdb, lvl, levelID);
  return lvl;
}

int bob_dbload_instance_planes(bob_db_s *bdb, Level *lvl, int levelID) {
  int rc;

  log_debug("loading level instances planes");

  rc = sqlite3_bind_int(bdb->qinstanceplanes, 1, levelID);
  if (rc != SQLITE_OK) {
    log_error("failed to bind levelID parameter to instance plane query");
    return -1;
  }

  while (1) {
    rc = sqlite3_step(bdb->qinstanceplanes);
    if (rc == SQLITE_ROW) {
      InstancePlane *ip = bob_dbload_instance_plane(bdb);
      pointer_vector_add(&lvl->instance_planes, ip);
    } 
    else if (rc == SQLITE_DONE) {
      break;
    }
    else {
      log_error("Unexpected result from database instance plane query: %d\n", rc);
      return -1;
    }
  }
  sqlite3_reset(bdb->qinstanceplanes);
  return 0;
}

InstancePlane *bob_dbload_instance_plane(bob_db_s *bdb) {
  int modelID;
  Model *model;
  int n1, n2;
  double px, py, pz, vx, vy, vz;
  InstancePlane *ip = malloc(sizeof *ip); 
  if (!ip) {
    log_error("failed to allocate memory for while loading instance plane");
    return NULL;
  }
  modelID = sqlite3_column_int(bdb->qinstanceplanes, 0);
  px = sqlite3_column_double(bdb->qinstanceplanes, 1);
  py = sqlite3_column_double(bdb->qinstanceplanes, 2);
  pz = sqlite3_column_double(bdb->qinstanceplanes, 3);
  vx = sqlite3_column_int(bdb->qinstanceplanes, 4);
  vy = sqlite3_column_double(bdb->qinstanceplanes, 5);
  vz = sqlite3_column_double(bdb->qinstanceplanes, 6);
  n1 = sqlite3_column_double(bdb->qinstanceplanes, 7);
  n2 = sqlite3_column_int(bdb->qinstanceplanes, 8);
  model = bob_dbload_model(bdb, modelID);
  ip->p[0] = px;
  ip->p[1] = py;
  ip->p[2] = pz;
  ip->v[0] = vx;
  ip->v[1] = vy;
  ip->v[2] = vz;
  ip->n1 = n1;
  ip->n2 = n2;
  log_debug("loaded instance plane: <%f,%f,%f>x%d | <%f,%f,%f>x%d | %p", 
      ip->p[0], ip->p[1], ip->p[2], ip->n1,
      ip->v[0], ip->v[1], ip->v[2], ip->n2,
      ip->model);
  return ip;
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
}

int prepare_queries(bob_db_s *bdb) {
  int rc;

  rc = sqlite3_prepare_v2(bdb->db, level_ambient_gravity_qstr, -1, &bdb->qambientgravity, 0);
  if (rc != SQLITE_OK) {
    log_error("failed to prepare ambientGravity query");
    return -1;
  }
  rc = sqlite3_prepare_v2(bdb->db, level_qstr, -1, &bdb->qlevel, 0);
  if (rc != SQLITE_OK) {
    log_error("failed to prepare level query");
    return -1;
  }
  rc = sqlite3_prepare_v2(bdb->db, instance_plane_qstr, -1, &bdb->qinstanceplanes, 0);
  if (rc != SQLITE_OK) {
    log_error("failed to prepare instance plane query");
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
    glBufferData(GL_ARRAY_BUFFER, fbuf.size * sizeof(GLfloat), fbuf.buffer, GL_STATIC_DRAW);

    handle = gl_shader_attrib(m->program, "vert");
    glEnableVertexAttribArray(handle);
    glVertexAttribPointer(handle, 3, GL_FLOAT, GL_FALSE, 5*sizeof(GLfloat), NULL);

    handle = gl_shader_attrib(m->program, "vertexCoord");
    glEnableVertexAttribArray(handle);
    glVertexAttribPointer(handle, 2, GL_FLOAT, GL_TRUE, 5*sizeof(GLfloat), (const GLvoid *)(3*sizeof(GLfloat)));

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
      src = sqlite3_column_text(bdb->qshader, 2);
      gl_type = to_gl_shader(bob_type);
      shader = malloc(sizeof *shader);
      rc = gl_load_shader(shader, gl_type, src, name);
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
    gl_load_texture(texture, path);
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
          num = atof(buf);
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

