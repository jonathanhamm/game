out:
	cc loadlevel.c camera.c game.c log.c meshes.c models.c physics.c glprogram.c common/data-structures.c main.c -o game -lm -lpng -lglfw -lGL -lGLEW -lpng -lsqlite3 -ggdb

