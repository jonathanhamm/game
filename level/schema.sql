CREATE TABLE shader (
	id INTEGER,
	type TINYINT,
	name VARCHAR(16),
	src TEXT,
	PRIMARY KEY(id)
);

CREATE TABLE program_xref (
	id INTEGER,
	shaderID INTEGER,
	programID INTEGER,
	PRIMARY KEY(id),
	FOREIGN KEY(shaderID) REFERENCES shader(id),
	FOREIGN KEY(programID) REFERENCES program(id)
);	

CREATE TABLE program (
	id INTEGER,
  PRIMARY KEY(id)
);

CREATE TABLE texture (
	id INTEGER,
	path TEXT,
	PRIMARY KEY(id)
);

CREATE TABLE mesh (
	id INTEGER,
	name VARCHAR(16),
	data TEXT,
	PRIMARY KEY(id)
);

CREATE TABLE model (
	id INTEGER,
	meshID INTEGER, 
	programID INTEGER,
	textureID INTEGER,
  hasUV TINYINT,
	PRIMARY KEY(id),
	FOREIGN KEY(meshID) REFERENCES mesh(id),
	FOREIGN KEY(programID) REFERENCES program(id),
	FOREIGN KEY(textureID) REFERENCES texture(id)
);

CREATE TABLE instance (
	modelID INTEGER,
	levelID INTEGER,
	vx FLOAT,
	vy FLOAT,
	vz FLOAT,
  scalex FLOAT,
  scaley FLOAT,
  scalez FLOAT,
	mass FLOAT,
  isSubjectToGravity TINYINT,
  isStatic TINYINT,
	FOREIGN KEY(modelID) REFERENCES model (id),
	FOREIGN KEY(levelID) REFERENCES level (id)
);

CREATE TABLE instance_plane (
  modelID INTEGER,
  levelID INTEGER,
  px FLOAT,
  py FLOAT,
  pz FLOAT,
  v1x FLOAT,
  v1y FLOAT,
  v1z FLOAT,
  v2x FLOAT,
  v2y FLOAT,
  v2z FLOAT,
  n1 INTEGER,
  n2 INTEGER,
  FOREIGN KEY(modelID) REFERENCES model (id),
	FOREIGN KEY(levelID) REFERENCES level (id)
);

CREATE TABLE level (
	id INTEGER,
	name TEXT,
  ambientGravityX FLOAT,
  ambientGravityY FLOAT,
  ambientGravityZ FLOAT,
	PRIMARY KEY(id)
);

