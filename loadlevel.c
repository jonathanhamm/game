#include "log.h"
#include "loadlevel.h"
#include "models.h"
#include <sqlite3.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <GL/glew.h>

#define BDB_VERTEXT_BUF 256

const char *level_qstr = 
  "SELECT modelID, levelID, vx, vy, vz, mass"
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

struct bob_db_s {
  sqlite3 *db;
  sqlite3_stmt *qlevel;
  sqlite3_stmt *qmodel;
  sqlite3_stmt *qmesh;
  IntMap models;
};

static sqlite3_stmt *query_level;

static int prepare_queries(bob_db_s *bdb);
static Model *bob_dbload_model(bob_db_s *bdb, int modelID);
static void bob_dbload_mesh(bob_db_s *bdb, Model *m, int meshID);
static int bob_parse_vertices(FloatBuf *fbuf, const unsigned char *vertext);

bob_db_s *bob_loaddb(const char *path) {
  int rc;

  bob_db_s *bdb = malloc(sizeof *bdb);
  if (!bdb) {
    perror("memory allocation error");
    return NULL;
  }

  bob_int_map_init(&bdb->models);

  rc = sqlite3_open(path, &bdb->db);
  if (rc != SQLITE_OK) {
    perror("error opening database");
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
    perror("failed to allocate memory for while loading level");
    return NULL;
  }

  rc = sqlite3_bind_text(bdb->qlevel, 1, name, -1, NULL);
  if (rc != SQLITE_OK) {
    log_error("failed to bind level name parameter to level query");
    return NULL;
  }

  log_debug("loading level instances");

  int modelID, levelID;
  double vx, vy ,vz, mass;
  while (1) {
    rc = sqlite3_step(bdb->qlevel);
    if (rc == SQLITE_ROW) {
      modelID = sqlite3_column_int(bdb->qlevel, 0);
      levelID = sqlite3_column_int(bdb->qlevel, 1);
      vx = sqlite3_column_double(bdb->qlevel, 2);
      vy = sqlite3_column_double(bdb->qlevel, 3);
      vz = sqlite3_column_double(bdb->qlevel, 4);
      mass = sqlite3_column_double(bdb->qlevel, 5);
      bob_dbload_model(bdb, modelID);
    }
    else if (rc = SQLITE_DONE) {
      break;
    }
    else {
      log_error("Unexpected result from database level query: %d\n", rc);
      return NULL;
    }
  }
  sqlite3_reset(bdb->qlevel);
  return lvl;
}

int prepare_queries(bob_db_s *bdb) {
  int rc;

  rc = sqlite3_prepare_v2(bdb->db, level_qstr, -1, &bdb->qlevel, 0);
  if (rc != SQLITE_OK) {
    perror("failed to prepare level query");
    return -1;
  }
  rc = sqlite3_prepare_v2(bdb->db, model_qstr, -1, &bdb->qmodel, 0);
  if (rc != SQLITE_OK) {
    perror("failed to prepare model query");
    return -1;
  }
  rc = sqlite3_prepare_v2(bdb->db, mesh_qstr, -1, &bdb->qmesh, 0);
  if (rc != SQLITE_OK) {
    perror("failed to prepare mesh query");
    return -1;
  }
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
  m->drawStart = 1;
  
  log_debug("loading model %d", modelID);
  rc = sqlite3_bind_int(bdb->qmodel, 1, modelID);
  if (rc != SQLITE_OK) {
    log_error("failed to bind modelID parameter to model query");
    return NULL;
  }

  int meshID, programID, textureID, hasUV;
  rc = sqlite3_step(bdb->qmodel);
  if (rc = SQLITE_ROW) {
    meshID = sqlite3_column_int(bdb->qmodel, 0);
    programID = sqlite3_column_int(bdb->qmodel, 1);
    textureID = sqlite3_column_int(bdb->qmodel, 2);
    hasUV = sqlite3_column_int(bdb->qmodel, 3);
    bob_dbload_mesh(bdb, m, meshID);
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
  if (rc = SQLITE_ROW) {
    name = sqlite3_column_text(bdb->qmesh, 0);
    data = sqlite3_column_text(bdb->qmesh, 1);
    log_info("loaded mesh of name %s with data %s", name, data);
    bob_parse_vertices(&fbuf, data);

    glGenBuffers(1, &m->vbo);
    glGenVertexArrays(1, &m->vao);

    glBindVertexArray(m->vao);
    glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
    glBufferData(GL_ARRAY_BUFFER, fbuf.size * sizeof(GLfloat), fbuf.buffer, GL_STATIC_DRAW);

  }
  rc = sqlite3_step(bdb->qmesh);
  if (rc != SQLITE_DONE) {
    log_error("Database in invalid format");
    return;
  }
  sqlite3_reset(bdb->qmesh);
  //bob_int_map_insert(&bdb->models, meshID, m);
  char_buf_free(&mbuf);
  float_buf_free(&fbuf);
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

