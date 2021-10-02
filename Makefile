out:
	cc -pg -fprofile-arcs -ftest-coverage loadlevel.c camera.c lazy_instance_engine.c game.c log.c meshes.c models.c physics.c glprogram.c common/data-structures.c main.c -o game -lm -lpng -lglfw -lGL -lGLEW -lpng -lsqlite3 -ggdb

