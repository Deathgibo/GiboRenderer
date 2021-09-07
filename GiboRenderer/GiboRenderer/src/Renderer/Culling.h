#pragma once
#include "Renderobject.h"

namespace Gibo {
	
	void NormalizePlane(Plane& plane)
	{
		float mag;
		mag = sqrt(plane.a * plane.a + plane.b * plane.b + plane.c * plane.c);
		plane.a = plane.a / mag;
		plane.b = plane.b / mag;
		plane.c = plane.c / mag;
		plane.d = plane.d / mag;
	}

	float DistanceToPoint(Plane p, glm::vec3 pt)
	{
		return p.a*pt.x + p.b*pt.y + p.c*pt.z + p.d;
	}

	//takes in the frustrums camera and projection matrix. Returns a vector for each plane that you dot product with another vector for plane equation
	std::vector<Plane> CalculatePlanes(glm::mat4 PV)
	{
		std::vector<Plane> p_planes(6);

		// Left clipping plane
		p_planes[0].a = PV[0][3] + PV[0][0];
		p_planes[0].b = PV[1][3] + PV[1][0];
		p_planes[0].c = PV[2][3] + PV[2][0];
		p_planes[0].d = PV[3][3] + PV[3][0];
		// Right clipping plane
		p_planes[1].a = PV[0][3] - PV[0][0];
		p_planes[1].b = PV[1][3] - PV[1][0];
		p_planes[1].c = PV[2][3] - PV[2][0];
		p_planes[1].d = PV[3][3] - PV[3][0];
		// Top clipping plane
		p_planes[2].a = PV[0][3] - PV[0][1];
		p_planes[2].b = PV[1][3] - PV[1][1];
		p_planes[2].c = PV[2][3] - PV[2][1];
		p_planes[2].d = PV[3][3] - PV[3][1];
		// Bottom clipping plane
		p_planes[3].a = PV[0][3] + PV[0][1];
		p_planes[3].b = PV[1][3] + PV[1][1];
		p_planes[3].c = PV[2][3] + PV[2][1];
		p_planes[3].d = PV[3][3] + PV[3][1];
		// Near clipping plane
		p_planes[4].a = PV[0][2]; //PV[0][3] + PV[0][2];
		p_planes[4].b = PV[1][2]; //PV[1][3] + PV[1][2];
		p_planes[4].c = PV[2][2]; //PV[2][3] + PV[2][2];
		p_planes[4].d = PV[3][2]; //PV[3][3] + PV[3][2];
		// Far clipping plane
		p_planes[5].a = PV[0][3] - PV[0][2];
		p_planes[5].b = PV[1][3] - PV[1][2];
		p_planes[5].c = PV[2][3] - PV[2][2];
		p_planes[5].d = PV[3][3] - PV[3][2];

		
		// Normalize the plane equations
		NormalizePlane(p_planes[0]);
		NormalizePlane(p_planes[1]);
		NormalizePlane(p_planes[2]);
		NormalizePlane(p_planes[3]);
		NormalizePlane(p_planes[4]);
		NormalizePlane(p_planes[5]);

		return p_planes;
	}

	//it loops through every object and every object not culled it stores its index into the array. That way you just loop through easily and call draw calls
	//pass in 6 planes and list of renderobjects
	std::vector<uint32_t> FrustrumIntersection(const std::vector<Plane>& planes, std::vector<RenderObject*>& renderobjects, std::unordered_map<uint32_t, BoundingVolume*> bvs, glm::mat4 PV)
	{
		//for each render object call its frustrum intersection if so add its index into the list
		std::vector<uint32_t> indices;
		indices.reserve(renderobjects.size() / 2);

		for (int i = 0; i < renderobjects.size(); i++)
		{
			if (bvs[renderobjects[i]->GetId()]->IntersectFrustrum(planes, PV) != INTERSECTION::OUTSIDE)
			{
				indices.push_back(i);
			}
		}

		return indices;
	}

	std::vector<float> CreateFrustrumMesh(int vertexattributelength, float fov, float near_depth, float far_depth, VkExtent2D proj_extent, glm::mat4 inveyematrix)
	{
		//calculate 8 corners of matrix
		std::vector<glm::vec4> points(8);
		float tanval = tan(glm::radians(fov) / 2.0f);
		float aspect_ratio = (proj_extent.width / (float)proj_extent.height);
		for (int i = 0; i < 2; i++)
		{
			float depth = (i == 0) ? near_depth : far_depth;
			float halfheight = tanval * depth;
			float halfwidth = halfheight * aspect_ratio;
			//go from eye space to world space
			points[i * 4 + 0] = inveyematrix * glm::vec4(-halfwidth, halfheight, -depth, 1);
			points[i * 4 + 1] = inveyematrix * glm::vec4(-halfwidth, -halfheight, -depth, 1);
			points[i * 4 + 2] = inveyematrix * glm::vec4(halfwidth, -halfheight, -depth, 1);
			points[i * 4 + 3] = inveyematrix * glm::vec4(halfwidth, halfheight, -depth, 1);
		}

		std::vector<glm::vec4> points2 = {
			points[0],points[1],points[0],points[3],points[0],points[4],
			points[1],points[2],points[1],points[5],
			points[2],points[3],points[2],points[6],
			points[3],points[7],
			points[4],points[5],points[4],points[7],
			points[6],points[7],points[6],points[5]
			//24
		};

		std::vector<float> pointdata;
		pointdata.resize(points2.size() * vertexattributelength);
		for (int i = 0; i < pointdata.size(); i += vertexattributelength)
		{
			pointdata[i] = points2[i / vertexattributelength].x;
			pointdata[i + 1] = points2[i / vertexattributelength].y;
			pointdata[i + 2] = points2[i / vertexattributelength].z;
		}

		return pointdata;
	}

	std::vector<float> CreateOrthoFrustrumMesh(int vertexattributelength, float minx, float maxx, float miny, float maxy, float minz, float maxz, glm::mat4 inveyematrix)
	{
		//calculate 8 corners of matrix
		std::vector<glm::vec4> points = {
			inveyematrix * glm::vec4(minx, maxy, -minz, 1),
			inveyematrix * glm::vec4(minx, miny, -minz, 1),
			inveyematrix * glm::vec4(maxx, miny, -minz, 1),
			inveyematrix * glm::vec4(maxx, maxy, -minz, 1),
			inveyematrix * glm::vec4(minx, maxy, -maxz, 1),
			inveyematrix * glm::vec4(minx, miny, -maxz, 1),
			inveyematrix * glm::vec4(maxx, miny, -maxz, 1),
			inveyematrix * glm::vec4(maxx, maxy, -maxz, 1),
		};

		std::vector<glm::vec4> points2 = {
			points[0],points[1],points[0],points[3],points[0],points[4],
			points[1],points[2],points[1],points[5],
			points[2],points[3],points[2],points[6],
			points[3],points[7],
			points[4],points[5],points[4],points[7],
			points[6],points[7],points[6],points[5]
			//24
		};

		std::vector<float> pointdata;
		pointdata.resize(points2.size() * vertexattributelength);
		for (int i = 0; i < pointdata.size(); i += vertexattributelength)
		{
			pointdata[i] = points2[i / vertexattributelength].x;
			pointdata[i + 1] = points2[i / vertexattributelength].y;
			pointdata[i + 2] = points2[i / vertexattributelength].z;
		}

		return pointdata;
	}
	
}
