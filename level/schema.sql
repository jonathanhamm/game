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

CREATE TABLE lazy_instance (
	id INTEGER,
	modelID INTEGER,
  rangeID INTEGER,
	vx VARCHAR(64),
	vy VARCHAR(64),
	vz VARCHAR(64),
  scalex VARCHAR(64),
  scaley VARCHAR(64),
  scalez VARCHAR(64),
	mass VARCHAR(64),
  isSubjectToGravity TINYINT,
  isStatic TINYINT,
	PRIMARY KEY(id),
	FOREIGN KEY(modelID) REFERENCES model (id),
  FOREIGN KEY(rangeID) REFERENCES range (id)
);

CREATE TABLE range (
	id INTEGER,
	levelID INTEGER,
  steps INTEGER,
  var VARCHAR(1),
  cache TINYINT,
  child INTEGER,
  FOREIGN KEY(child) REFERENCES range(id),
	FOREIGN KEY(levelID) REFERENCES level (id),
	PRIMARY KEY(id)
);

CREATE TABLE level (
	id INTEGER,
	name TEXT,
  ambientGravityX FLOAT,
  ambientGravityY FLOAT,
  ambientGravityZ FLOAT,
	PRIMARY KEY(id)
);

