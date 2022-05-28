out:
	cc -pg -fprofile-arcs -ftest-coverage loadlevel.c camera.c lazy_instance_engine.c game.c meshes.c models.c physics.c glprogram.c common/opengl-util.c common/log.c common/data-structures.c main.c -o game -lm -lpng -lglfw -lGL -lGLEW -lpng -lsqlite3 -ggdb -lpthread -pedantic

