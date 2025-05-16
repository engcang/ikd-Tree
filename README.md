# ikd-Tree
+ ikd-Tree is an incremental k-d tree for robotic applications.

<br>

## Sphere branch
+ `Downsampling` mechanism is modified for faster mapping and grid-aligned points.
	+ Original: add a point with the shortest distance to the centroid of a voxel, and delete other points in a voxel grid
		+ Searching other points, deleting other points, and comparing distance take more time than changed method
	  + (`Add_by_point` should be changed to set `covered` as existing points are removed)
	+ **Changed**: add a centroid of a voxel where the raw point is laid / if a point already exists in the voxel grid, do nothing
		+ Searching other points (only checking existence), but no deletion nor comparing
		+ `Add_Points` automatically calls `Build`, if not built yet
			+ Input points for `Build` inside `Add_Points` is also modified to use `Downsampling`
			+ Otherwise, the first input points are not grid-aligned as voxels.
	+ Added modification of `Delete_Points` to search whole tree => `Delete_Points_Accurate`
	+ Added `Delete_Points_Downsample` which deletes the all points within the voxel