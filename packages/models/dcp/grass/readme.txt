model:		grass
		designed for use in the game cube
		created 01.2004 using milkshape 3D
		grass spread adjusted 02.2004 to cover a larger area with less mapmodels
           

creator:	dietmar pier, http://www.dietmarpier.de

stats:		vertices: 171
		polygons: 247
		skins: 1

description:	unzip the tris.md2 and skin.jpg files to cube/packages/models/ using 'full path information' (how is this
		option named exactly in the english version of winzip???)

		add the following lines at the END of cube/data/models.cfg

		loadmodel "grass"
		mapmodel 1 0 0 0

		when editing a map, select a floor cube, hit the console key and type 'newent mapmodel XY' where XY has to
		be replaced by the correspondent number of the new mapmodel in your models.cfg

		hint: to cover an area with grass, be sure you turn your viewing angle when placing one model beside another
		to secure a more random orientation of the grass-blades. caution: don't place too much grass on a map, the
		polygon load will severely increase if you try to recreate a soccer field in cube... ;-)

copyright:	it's free to use for eveyone who likes to. if you want to use it for something else than cube, please drop
		me a line, i just want to know...

credits:	id-software (.tris)

		chumbalum-soft (milkshape3D, it just took 3 days to understand the basics, one more to make my first working
		models for cube)

		aardappel (i love cube! it's so easy/fun to modify things like models, textures, sounds. man, you gave me a
		new chance to make use of my pathologic creativity, since my musical carrier has come to a grinding halt...)
