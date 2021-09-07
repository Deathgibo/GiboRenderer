# GibRenderer

Vulkan Rendering Engine.

#include <iostream>
#include <vector>
#include <algorithm>	

struct vec3
{
	float x;
	float y;
	float z;

	vec3()
	{

	}
	vec3(float xx, float yy, float zz) : x(xx), y(yy), z(zz)
	{
	}
};

//Box whose axis's match our basis axis. We hold minium and maximum points
struct AABB
{
	vec3 min;
	vec3 max;
};

AABB CalculateAABB(std::vector<vec3>& vertices)
{
	AABB box;
	box.min = vec3(10000, 10000, 10000);
	box.max = vec3(-10000, -10000, -10000);

	for (int i = 0; i < vertices.size(); i++)
	{
		box.min.x = std::min(box.min.x, vertices[i].x);
		box.min.y = std::min(box.min.y, vertices[i].y);
		box.min.z = std::min(box.min.z, vertices[i].z);

		box.max.x = std::max(box.max.x, vertices[i].x);
		box.max.y = std::max(box.max.y, vertices[i].y);
		box.max.z = std::max(box.max.z, vertices[i].z);
	}
	
	return box;
}

//OBB is a rectangle that can be in any orientation. The 3 axis have to be normalized, and half are the lengths from center to respective faces
struct OBB {
	vec3 center;

	vec3 uaxis;
	vec3 vaxis;
	vec3 waxis;
	
	float halfu;
	float halfv;
	float halfw;
};

OBB CalculateOBB(std::vector<vec3>& vertices)
{
	/*
	First get k extreme points. This is similar to k-dops. You calculate k/2 normals  (usually based symettrical around the direction space) Then
	you just project all points and get min/max point pair for each normal. These are your k extreme points held in set S

	Next create large base triangle. Get the pair with the longest distance apart as first 2 vertices. Then get the point farthest away from plane made from connecting these 2.
	Those 3 points make up the large base triangle. Then for each edge of triangle calculate 3 basis axes given by the equation:
	u0=normalize(edge)
	u1 = normalize(triangle_normal)
	u2 = corss(u0,u1)

	Now you have 3 pairs of axis and you need to approximate the size of the OBB from these axis by projeting all points onto it. Not completely sure how to do this.

	Next with the large base triangle get the maximum and minimum points from that triangle plane and create the ditetrahedron with it. Really just connect the dots and now you
	have 7 triangles. 3 created on top, 3 created on bottom, and the large base triangle we already calculated. For each triangle you generate OBB's and pick best ones which would have
	smallest surface area.

	Once you have the best axis we can find the OBB. You do one final pass like AABB just finding the minimum and maxmimum corners of the box by projecting. Then with that you
	can easily find the midpoint and halflengths of all sides. Then make sure its smaller than AABB and your done.

	The only problem is how can I project points to 3 axis? I have to do this in order to find minimum/maximum corners for final OBB and for finding the best axis.
	Really we just have 3 axis so based on all points of S we calculate the OBB based off these axis, then you get surface area to compare which is better.
	
	*So really the only quesiton is given 3 axis and a set of points how do I determine the minimum and maximum corners? Do I project each point to each axis? probably

	
	
	*/


}

//k-dops are a set of 2 planes that fit on the object. a aabb is a 6-dop because you have 6 planes (each with a parrele counterpart) bounding the object
//The k-dop is the intersection of all the slabs 
struct kdop
{
	struct slab {
		vec3 n;
		float min;
		float max;

		slab() {}
	};

	std::vector<slab> slabs;
};

float dot(vec3 A, vec3 B)
{
	return A.x*B.x + A.y*B.y + A.z*B.z;
}

//normals have to be given or I could just spread them out across 360 degrees
kdop CalculateKDOP(std::vector<vec3>& vertices, std::vector<vec3>& normals)
{
	//for every normals go through every vertex and find distance to plane and get the min/max distances
	for (int n = 0; n < normals.size(); n++)
	{
		vec3 norm = normals[n];
		float min = 100000;
		float max = -100000;
		for (int i = 0; i < vertices.size(); i++)
		{
			//project vertex to plane, get distance from point to plane
			//distance = -dot(N,V) N is normalized, V isn't
			float distance = -dot(norm, vertices[i]);
			min = std::min(min, distance);
			max = std::max(max, distance);
		}
	}
}

struct sphere {
	vec3  center;
	float radius;
};

//1 easy method is aabb
//easy method is aabb then relooknig at vertices
//last one is harder, there is a paper. Its in real time collision detection. And elberly has code on it
sphere CalculateSphere(std::vector<vec3>& vertices)
{
	/*
	0 or 1 points circle is just point or nothing
	2 points is the circle whose center is midpoint between 2 points
	3 points the circle is the circumcircle of the triangle described  by the 3 points
	4 points required calculating the circumsphere of a tetrahedron
	
	Paper uses facts that the minimum sphere is unique around the points, if you have a minimum sphere and take a point out and the minimum sphere is same you know its 
	not on boundary and vice versa, and the fact that d+1 vertices or less are what you need to uniquely determine a minium sphere, so in 2d 3 points 3d 4 points. Anything
	more its not clear what the minimum sphere is. Besides this he used some linear interpolation of spheres for proofs and whatever.
	The algorithm is recursive.
	*/

}

/* Quick Hull
 This is algorithm isn't too difficult to understand, it uses the same ideas from incremental. If you have a convex hull and
 a new point all faces that are visible to the new point and not part of the convex hull and are going to be replaced with new faces. The new faces have
 the new point as vertices the the points of previous hull where one edge isn't visible to point and one is. 
 The other faces that are invisible to p are not touched

 Thats pretty much the algorithm and you start by creating a tetrahedron then partition points based on visibilty of all the faces.
 Then you just do above step, make sure partitions are updated and old faces are deleted and repeat.
*/

//BSP Tree
//Traditional bsp trees need lines or triangles which is fine for 2d but not really useful for 3D. So a k-d tree is usually used in 3D settings

//K-d tree
//K-d tree is good for rough sorting which can be used for transparency, and actually for frustrum becuase you just check if frustrum collides with plane if not skip all its children
//also could you use this for visiblity? in 3D maybe you have a steradian buffer? or spherical coordinates 180*360 buffer.

//OCT-Tree

//Volume Heirarchy?

//Intersections

int main()
{




	return 0;
}
