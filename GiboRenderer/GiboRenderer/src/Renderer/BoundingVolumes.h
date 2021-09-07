#pragma once
#include <glm/common.hpp>
#include <algorithm>

namespace Gibo {
	/*
	Do polymorphism with bounding volumes its just a 1 leve tree so we can switch bounding volumes with ease. You would probably just override intersect functions like
	intersectfrustrum/intersectsphere/etc. 

	For boundingvolume we could store it once in mesh, then each renderobject just multiplies by its model matrix everytime we want the boundingvolume.
	Or we could make each renderobject just hold its own which is more memory per object but faster in runtime.
	Or we could create an SOA for boundingvolumes but maybe we need to link them quicker

	Right now for bounding volumes the meshcache generates bounding volumes for each mesh when you load it up. Then each renderobject holds a string to the mesh filename so when it wants
	a bounding volume it gets the original from the meshcache then transforms it by its model matrix. The bounding volumes are not stored in renderobjects, but in a map in renderobjectmanager,
	it will hold all the bounding volumes and update them. To update it checks if renderobject moved then regrabs original and transforms it by renderobjects model matrix.
		So now renderobjects won't hold extra data which is nice for cache because bounding volumes aren't really needed in main render pass. Also we don't need to store extra data to update
	bounding volume we just fetch original and remultiply by matrix, and we don't have to just mulyipl by matrix every frame. And when you need volumes they are all in one smaller data 
	structure we can now loop through more quickly.
	*/
	struct Plane {
		float a, b, c, d;
	};

	enum class INTERSECTION { OUTSIDE, INSIDE, INTERSECTING};

	class BoundingVolume
	{
	public:
		BoundingVolume() = default;
		~BoundingVolume() = default;

		virtual INTERSECTION IntersectFrustrum(const std::vector<Plane>& planes, glm::mat4 PV) = 0;
		virtual void Construct(std::vector<float>& vertexdata, int vertexattributelength) = 0;
		virtual std::vector<float> CreatePointMesh(int vertexattributelength) = 0;
		virtual void Transform(glm::mat4 matrix) = 0;
		virtual BoundingVolume* create() = 0;
		virtual BoundingVolume* clone() = 0;
	};

	//AABB
	//Box whose axis's match our basis axis. We hold minium and maximum points
	class AABB : public BoundingVolume
	{
	public:
		AABB() = default;
		~AABB() = default;

		AABB* create() override
		{
			return new AABB();
		}

		AABB* clone() override
		{
			return new AABB(*this);
		}

		bool within(float min, float max, float val)
		{
			return val <= max && val >= min;
		}

		INTERSECTION IntersectFrustrum(const std::vector<Plane>& planes, glm::mat4 PV) override
		{
			//convert all 8 points to clip space. If all points are outside one dimension rejected. if all are in completely inside, else intersecting
			std::vector<glm::vec4> points = {
				PV * glm::vec4(min.x,min.y,min.z, 1),
				PV * glm::vec4(min.x,min.y,max.z, 1),
				PV * glm::vec4(min.x,max.y,min.z, 1),
				PV * glm::vec4(min.x,max.y,max.z, 1),
				PV * glm::vec4(max.x,min.y,min.z, 1),
				PV * glm::vec4(max.x,min.y,max.z, 1),
				PV * glm::vec4(max.x,max.y,min.z, 1),
				PV * glm::vec4(max.x,max.y,max.z, 1)
			};

			//go through every plane, if one point is on negative side break. If all are on positive side we can leave and say its outside. If we never leave it must be inside.
			for (int d = 0; d < 6; d++)
			{
				bool inside = false;
				for (int i = 0; i < points.size(); i++)
				{
					switch (d)
					{
					case 0: if (points[i].x >= -points[i].w) inside = true; break;
					case 1: if (points[i].x <= points[i].w)  inside = true; break;
					case 2: if (points[i].y >= -points[i].w) inside = true; break;
					case 3: if (points[i].y <= points[i].w)  inside = true; break;
					case 4: if (points[i].z >= 0)			 inside = true; break;
					case 5: if (points[i].z <= points[i].w)  inside = true; break;
					}
					if (inside) break;
				}
				if (!inside) return INTERSECTION::OUTSIDE;
			}

			return INTERSECTION::INTERSECTING;
		}

		void Transform(glm::mat4 matrix) override
		{
			min = glm::vec3(matrix * glm::vec4(min, 1));
			min += glm::vec3(matrix[0][3], matrix[1][3], matrix[2][3]);
			
			max = glm::vec3(matrix * glm::vec4(max, 1));
			max += glm::vec3(matrix[0][3], matrix[1][3], matrix[2][3]);
		}

		//construct with a model matrix

		void Construct(std::vector<float>& vertexdata, int vertexattributelength) override
		{
			min = glm::vec3(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
			max = glm::vec3(std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), std::numeric_limits<float>::min());

			for (int i = 0; i < vertexdata.size(); i+= vertexattributelength)
			{
				min.x = std::min<float>(min.x, vertexdata[i]);
				min.y = std::min<float>(min.y, vertexdata[i + 1]);
				min.z = std::min<float>(min.z, vertexdata[i + 2]);

				max.x = std::max<float>(max.x, vertexdata[i]);
				max.y = std::max<float>(max.y, vertexdata[i + 1]);
				max.z = std::max<float>(max.z, vertexdata[i + 2]);
			}
		}
		
		std::vector<float> CreatePointMesh(int vertexattributelength) override
		{
			glm::vec3 center = (min + max) / 2.0f;
			glm::vec3 halfvec = max - center;

			std::vector<float> pointdata;
			pointdata.resize(8 * vertexattributelength);

			std::vector<glm::vec3> points = {
				{center.x - halfvec.x,center.y + halfvec.y, center.z + halfvec.z},   //fronttopleft
				{center.x - halfvec.x,center.y - halfvec.y, center.z + halfvec.z},   //frontbottomleft
				{center.x + halfvec.x,center.y - halfvec.y, center.z + halfvec.z},   //frontbottomright
				{center.x + halfvec.x,center.y + halfvec.y, center.z + halfvec.z},   //fronttopright

				{center.x - halfvec.x,center.y + halfvec.y, center.z - halfvec.z},   //backtopleft
				{center.x - halfvec.x,center.y - halfvec.y, center.z - halfvec.z},   //backbottomleft
				{center.x + halfvec.x,center.y - halfvec.y, center.z - halfvec.z},   //backbottomright
				{center.x + halfvec.x,center.y + halfvec.y, center.z - halfvec.z},   //backtopright
			};

			for (int i = 0; i < pointdata.size(); i += vertexattributelength)
			{
				pointdata[i] = points[i / vertexattributelength].x;
				pointdata[i + 1] = points[i / vertexattributelength].y;
				pointdata[i + 2] = points[i / vertexattributelength].z;
			}

			return pointdata;
		}

	public:
		glm::vec3 min;
		glm::vec3 max;
	};


	//SPHERE
	class Sphere : public BoundingVolume
	{
	public:
		Sphere() = default;
		~Sphere() = default;

		Sphere* create() override
		{
			return new Sphere();
		}

		Sphere* clone() override
		{
			return new Sphere(*this);
		}

		INTERSECTION IntersectFrustrum(const std::vector<Plane>& planes, glm::mat4 PV) override
		{
			//multiply sphere center by each plane. If its completely outside not intersecting, if its not completely outside intsersecvting, else if its inside all 6 planes completely inside
			for (int i = 0; i < planes.size(); i++)
			{
				float distance = glm::dot(glm::vec3(planes[i].a, planes[i].b, planes[i].c), c) + planes[i].d; //if its positive its on side normal is pointing, negative other side, 0 on the plane
				distance = -distance;
	
				if (distance > r) { 
					return INTERSECTION::OUTSIDE;
				}
				else if (distance > -r) {
					return INTERSECTION::INTERSECTING;
				}
			}

			return INTERSECTION::INSIDE;
		}

		//*assumes uniform scale if not there I would ahve to do outer x,y,z points and get max distance
		void Transform(glm::mat4 matrix) override
		{
			//take center and a point on border of sphere then transform both then get distance
			glm::vec3 outer_point = glm::vec3(matrix * (glm::vec4(c + glm::vec3(r, 0, 0), 1))) + glm::vec3(matrix[0][3], matrix[1][3], matrix[2][3]);
			c = glm::vec3(matrix * (glm::vec4(c, 1))) + glm::vec3(matrix[0][3], matrix[1][3], matrix[2][3]);
			r = glm::distance(c, outer_point);
		}

		//using [Ritter90] algorithm described and coded in Real_Time_Collision_Detection 
		void Construct(std::vector<float>& vertexdata, int vertexattributelength) override
		{
			RitterSphere(vertexdata, vertexattributelength);
		}

		std::vector<float> CreatePointMesh(int vertexattributelength) override
		{
			//point will be center +- radius in 3 axis

			std::vector<glm::vec3> points = {
				//x
				{c.x + r, c.y, c.z},
				{c.x - r, c.y, c.z},
				
				//y
				{c.x, c.y + r, c.z},
				{c.x, c.y - r, c.z},

				//z
				{c.x, c.y, c.z + r},
				{c.x, c.y, c.z - r}
			};

			std::vector<float> pointdata;
			pointdata.resize(6 * vertexattributelength);

			for (int i = 0; i < pointdata.size(); i += vertexattributelength)
			{
				pointdata[i] = points[i / vertexattributelength].x;
				pointdata[i + 1] = points[i / vertexattributelength].y;
				pointdata[i + 2] = points[i / vertexattributelength].z;
			}

			return pointdata;
		}
   
	public:
		glm::vec3 c;
		float r;

	private:
		//we just pick the most 2 distance points out of extremes in x,y,z dimenions
		void MostSeperatedPointOnAABB(glm::vec3& min, glm::vec3& max, std::vector<float>& vertexdata, int vertexattributelength)
		{
			int minx = 0, maxx = 0, miny = 1, maxy = 1, minz = 2, maxz = 2;
			for (int i = vertexattributelength; i < vertexdata.size(); i += vertexattributelength)
			{
				if (vertexdata[i] < vertexdata[minx]) minx = i;
				if (vertexdata[i] > vertexdata[maxx]) maxx = i;
				if (vertexdata[i + 1] < vertexdata[miny]) miny = i;
				if (vertexdata[i + 1] > vertexdata[maxy]) maxy = i;
				if (vertexdata[i + 2] < vertexdata[minz]) minz = i;
				if (vertexdata[i + 2] > vertexdata[maxz]) maxz = i;
			}

			float dist2x = glm::dot(glm::vec3(vertexdata[maxx], vertexdata[maxx + 1], vertexdata[maxx + 2]) - glm::vec3(vertexdata[minx], vertexdata[minx + 1], vertexdata[minx + 2]), glm::vec3(vertexdata[maxx], vertexdata[maxx + 1], vertexdata[maxx + 2]) - glm::vec3(vertexdata[minx], vertexdata[minx + 1], vertexdata[minx + 2]));
			float dist2y = glm::dot(glm::vec3(vertexdata[maxy - 1], vertexdata[maxy], vertexdata[maxy + 1]) - glm::vec3(vertexdata[miny - 1], vertexdata[miny], vertexdata[miny + 1]), glm::vec3(vertexdata[maxy - 1], vertexdata[maxy], vertexdata[maxy + 1]) - glm::vec3(vertexdata[miny - 1], vertexdata[miny], vertexdata[miny + 1]));
			float dist2z = glm::dot(glm::vec3(vertexdata[maxz - 2], vertexdata[maxz - 1], vertexdata[maxz]) - glm::vec3(vertexdata[minz - 1], vertexdata[minz - 2], vertexdata[minz]), glm::vec3(vertexdata[maxz - 2], vertexdata[maxz - 1], vertexdata[maxz]) - glm::vec3(vertexdata[minz - 1], vertexdata[minz - 2], vertexdata[minz]));

			min = glm::vec3(vertexdata[minx], vertexdata[minx + 1], vertexdata[minx + 2]);
			max = glm::vec3(vertexdata[maxx], vertexdata[maxx + 1], vertexdata[maxx + 2]);
			if (dist2y > dist2x && dist2y > dist2z) {
				max = glm::vec3(vertexdata[maxy - 1], vertexdata[maxy], vertexdata[maxy + 1]);
				min = glm::vec3(vertexdata[miny - 1], vertexdata[miny], vertexdata[miny + 1]);
			}

			if (dist2z > dist2x && dist2z > dist2y) {
				max = glm::vec3(vertexdata[maxz - 2], vertexdata[maxz - 1], vertexdata[maxz]);
				min = glm::vec3(vertexdata[minz - 1], vertexdata[minz - 2], vertexdata[minz]);
			}
		}

		//calculate circle from 2 distance points
		void SphereFromDistancePoints(std::vector<float>& vertexdata, int vertexattributelength)
		{
			glm::vec3 min, max;
			MostSeperatedPointOnAABB(min, max, vertexdata, vertexattributelength);
			
			c = (min + max) * 0.5f;
			r = glm::dot(max - c, max - c);
			r = std::sqrt(r);
		}
		
		//with a current sphere and new point. Move sphere towards new point and increase radius so it contains new point and all previous points
		void SphereofSphereAndPt(glm::vec3 p, std::vector<float>& vertexdata, int vertexattributelength)
		{
			glm::vec3 d = p - c;
			float dist2 = glm::dot(d, d);

			if (dist2 > r * r)
			{
				float dist = std::sqrt(dist2);
				float newRadius = (r + dist) * 0.5f;
				float k = (newRadius - r) / dist;
				r = newRadius;
				c += d * k;
			}
		}

		void RitterSphere(std::vector<float>& vertexdata, int vertexattributelength)
		{
			SphereFromDistancePoints(vertexdata, vertexattributelength);

			for (int i = 0; i < vertexdata.size(); i += vertexattributelength)
			{
				SphereofSphereAndPt(glm::vec3(vertexdata[i], vertexdata[i + 1], vertexdata[i + 2]), vertexdata, vertexattributelength);
			}
		}

	};

	//K-DOP5
	//SPHERE - weltlz algorithm
	//OBB 
	//quickhull?

	//BVH top-down

	class BVH
	{
	public:
		int MAX_OBJECTS_PER_LEAF = 1;

		enum class TYPE : uint8_t { NODE, LEAF };

		struct Node {
			uint32_t numObjects;
			int* objects;
			Node* left;
			Node* right;
			TYPE type;
			int BV;
		};

	public:
		BVH() : root(nullptr) {}
		~BVH() = default;


		//Top down are most common, easiest to make, but aren't the best trees
		void StartTopDownBVTree(int objects[], int numobjects)
		{
			TopDownBVTree(&root, objects, numobjects);
		}
		//You start with all objects and recursively split them into groups of 2 and calculate bounding volumes
		//bounding volume takes in a bunch of objects and just finds a good bounding volume which contains all of their bounding volumes
		//partition just splits objects in 2, probably by a plane and sort them in terms of distance to plane 
		void TopDownBVTree(Node** node, int objects[], int numobjects)
		{
			if (numobjects == 0)
			{
				std::cout << "numobjects is 0 in topdown call error\n";
				return;
			}

			Node* newnode = new Node;
			*node = newnode;

			newnode->BV = 0; //CalculateBoundingVolume(objects,numobjects)
			//depending on bounding volume type of objects you would just create one that contains them all. Spheres you create largest sphere of spheres. Box you would just get min max of every box, etc.

			if (numobjects <= MAX_OBJECTS_PER_LEAF)
			{
				newnode->type = TYPE::LEAF;
				newnode->objects = objects;
				newnode->numObjects = numobjects;
			}
			else
			{
				newnode->type = TYPE::NODE;

				int k = 1;// partition(objects, numobjects);
				//for partition you pick an axis. This could be x,y,z. The obb axis. Axis with greatest variance.
				//Once you have an axis pick a point. usually the median or mean of centroids. Or the median of the parent bounding volume.

				TopDownBVTree(&newnode->left, &objects[0], k);
				TopDownBVTree(&newnode->right, &objects[k], numobjects - k);
			}
		}

		void StartBottomUpBVTree(int object[], int numObjects)
		{
			root = BottomUpBVTree(object, numObjects);
		}
		//we start with all objects in bounding volumes then have to merge them together and go up the tree. This is harder to do but produces better trees.
		//grab 2 to merge, create a parent node to hold them with new bounding volume. clear two processed nodes from list and add this new one.
		Node* BottomUpBVTree(int object[], int numObjects)
		{
			if (numObjects == 0)
			{
				std::cout << "numobjects is 0 in botdown call error\n";
			}

			int i, j;

			//create a list of node*. This represent all the objects as leafs at bottom of tree
			Node** nodelist = new Node*[numObjects];
			for (int i = 0; i < numObjects; i++)
			{
				nodelist[i] = new Node;
				nodelist[i]->type = TYPE::LEAF;
				nodelist[i]->objects = &object[i];
			}

			while (numObjects > 1)
			{
				//get 2 children nodes to merge
				//FindNodesToMerge(&nodelist[0], numObjects, &i, &j);
				//You can do brute force O(n^3), which is just loop through every pair and compute bounding volume and pick ones with minimum bounding volume
				//Another method uses a priority queue, to find minimum volume quicker
				//Another method is to actually find nearest neighbor using k-d tree. 
				//However since my number of objects is low probably best to stick with brute force approach

				//create new parent node that holds the 2 children nodes we are merging
				Node* pair = new Node;
				pair->type = TYPE::NODE;
				pair->left = nodelist[i];
				pair->right = nodelist[j];
				pair->BV = 0;//compute boundingvolume on nodelist[i]->object and nodelist[j]->object

				//now remove the 2 children we proccessed from nodelist and add the new one to the list
				int min = i;
				int max = j;
				if (i > j) min = j, max = i;

				nodelist[min] = pair;
				nodelist[max] = nodelist[numObjects - 1];
				numObjects--;
			}
			//free the nodelist we made. We don't need the array that holds the node pointers. We still need all the leafs and nodes we created with new though.
			Node* root = nodelist[0];
			delete nodelist;
			return root;
		}

		//optimizing - store trees in arrays, store trees in cache-friendly trees, get rid of recursion, if theres specific arrange tree so things come first that will be needed so it can leave early
		//caching - store data from previous frstrum cull which could be used

	public:

		Node* root;
	};

}