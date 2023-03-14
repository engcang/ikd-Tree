# ikd-Tree
+ ikd-Tree is an incremental k-d tree for robotic applications.

<br>

## Coverage check branch
+ This branch is for checking if the nodes(points) are covered(seen) by the sensor attached to the robots.
+ Additionally, `Downsampling` mechanism is modified for faster mapping and grid-aligned points.
	+ Original: add a point with the shortest distance to the centroid of a voxel, and delete other points in a voxel grid (searching other points takes more time than changed method)
	  + `Add_by_point` should be changed to set `covered`
	+ Changed: add a centroid of a voxel / if a point already exists in the voxel, do nothing