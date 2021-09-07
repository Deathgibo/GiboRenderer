#pragma once
#include "../pch.h"

namespace Gibo {

	struct Cluster {
		glm::vec3 min;
		float padding1;
		glm::vec3 max;
		float padding2;
	};

	void FrustrumLocation(std::vector<glm::vec3>& points, int x, int x_size, int y, int y_size, float z1, float z2, float n, float f, 
		                  const std::vector<glm::vec4>& near_plane, const std::vector<glm::vec4>& far_plane)
	{
		//get percentages 
		float x_percent = x / (float)(x_size);
		float y_percent = y / (float)(y_size);
		float znear_percent = (z1-n) / (f - n);
		float zfar_percent = (z2-n) / (f - n);

		//calculate x and y position on near plane
		float near_x = near_plane[1].x + (near_plane[2].x - near_plane[1].x)*x_percent;
		float near_y = near_plane[0].y + (near_plane[1].y - near_plane[0].y)*y_percent;
		glm::vec3 nearplane_point(near_x, near_y, n);
		//calculate x and y position on far plane
		float far_x = far_plane[1].x + (far_plane[2].x - far_plane[1].x)*x_percent;
		float far_y = far_plane[0].y + (far_plane[1].y - far_plane[0].y)*y_percent;
		glm::vec3 farplane_point(far_x, far_y, f);

		//start at nearplane point, go towards far plane point for a distance based off z distance
		points.push_back(nearplane_point + (farplane_point - nearplane_point)*znear_percent);
		points.push_back(nearplane_point + (farplane_point - nearplane_point)*zfar_percent);
	}

	//creates clusters. This needs to be calculate once per start up and only updated if projection matrix gets updated
	std::vector<Cluster> CreateClusters(float nnear, float ffar, float fov, VkExtent2D proj_extent, int x_size, int y_size, int z_size)
	{
		//std::cout << "total clusters: " << x_size * y_size*z_size << std::endl;

		float tanval = tan(glm::radians(fov) / 2.0f);
		float aspect_ratio = (proj_extent.width / (float)proj_extent.height);
		//NEAR PLANE
		std::vector<glm::vec4> near_plane(4);
		float halfheight = tanval * nnear;
		float halfwidth = halfheight * aspect_ratio;
		//go from eye space to world space
		near_plane[0] = glm::vec4(-halfwidth, halfheight, -nnear, 1);
		near_plane[1] = glm::vec4(-halfwidth, -halfheight, -nnear, 1);
		near_plane[2] = glm::vec4(halfwidth, -halfheight, -nnear, 1);
		near_plane[3] = glm::vec4(halfwidth, halfheight, -nnear, 1);

		//FAR PLANE
		std::vector<glm::vec4> far_plane(4);
		halfheight = tanval * ffar;
		halfwidth = halfheight * aspect_ratio;
		//go from eye space to world space
		far_plane[0] = glm::vec4(-halfwidth, halfheight, -ffar, 1);
		far_plane[1] = glm::vec4(-halfwidth, -halfheight, -ffar, 1);
		far_plane[2] = glm::vec4(halfwidth, -halfheight, -ffar, 1);
		far_plane[3] = glm::vec4(halfwidth, halfheight, -ffar, 1);

		//create a x by y grid on the front of the frustrum. Then based off a logarithmic splitting split by depth values and create AABB's to make intersection easier and so everything overlaps	
		std::vector<Cluster> clusters;
		clusters.reserve(x_size*y_size*z_size);
		for (int z = 0; z < z_size; z++)
		{
			//logarithmic splitting scheme between near and far for z_size slices
			float previous_z = -nnear * std::pow(ffar / nnear, z / (float)(z_size));
			float current_z = -nnear * std::pow(ffar / nnear, (z+1) / (float)(z_size));

			for (int x = 0; x < x_size; x++)
			{
				for (int y = 0; y < y_size; y++)
				{
					//calculate 8 points of current frustrum based off sorrounding 4 points and 2 z values
					std::vector<glm::vec3> corners;
					FrustrumLocation(corners, x,     x_size, y,     y_size, previous_z, current_z, -nnear, -ffar, near_plane, far_plane); //top left
					FrustrumLocation(corners, x,     x_size, y + 1, y_size, previous_z, current_z, -nnear, -ffar, near_plane, far_plane); //bot left
					FrustrumLocation(corners, x + 1, x_size, y + 1, y_size, previous_z, current_z, -nnear, -ffar, near_plane, far_plane); //bot right
					FrustrumLocation(corners, x + 1, x_size, y,     y_size, previous_z, current_z, -nnear, -ffar, near_plane, far_plane); //top right

					//calculate AABB based off 8 corners
					Cluster current_cluster;
					current_cluster.min = corners[0];
					current_cluster.max = corners[0];
					//current_cluster.min = glm::vec3(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
					//current_cluster.max = glm::vec3(std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), std::numeric_limits<float>::min());
					
					for (int i = 1; i < corners.size(); i++)
					{
						current_cluster.min.x = std::min<float>(current_cluster.min.x, corners[i].x);
						current_cluster.min.y = std::min<float>(current_cluster.min.y, corners[i].y);
						current_cluster.min.z = std::min<float>(current_cluster.min.z, corners[i].z);

						current_cluster.max.x = std::max<float>(current_cluster.max.x, corners[i].x);
						current_cluster.max.y = std::max<float>(current_cluster.max.y, corners[i].y);
						current_cluster.max.z = std::max<float>(current_cluster.max.z, corners[i].z);
					}
					//std::cout << current_cluster.min.x << " " << current_cluster.min.y << " " << current_cluster.min.z << "  |  " << current_cluster.max.x << " " << current_cluster.max.y << " " << current_cluster.max.z << std::endl;
					clusters.emplace_back(current_cluster);
				}
			}
		}

		return clusters;
	}

	std::vector<float> CreateClusterMesh(int vertexattributelength, const std::vector<Cluster>& clusters , glm::mat4 inveyematrix)
	{
		std::vector<float> pointdata;
		pointdata.resize(clusters.size() * 8 * vertexattributelength);
		for (int i = 0; i < clusters.size(); i++)
		{
			glm::vec3 center = (clusters[i].min + clusters[i].max) / 2.0f;
			glm::vec3 halfvec = clusters[i].max - center;

			std::vector<glm::vec3> points = {
			inveyematrix * glm::vec4(center.x - halfvec.x,center.y + halfvec.y, center.z + halfvec.z, 1),   //fronttopleft
			inveyematrix * glm::vec4(center.x - halfvec.x,center.y - halfvec.y, center.z + halfvec.z, 1),   //frontbottomleft
			inveyematrix * glm::vec4(center.x + halfvec.x,center.y - halfvec.y, center.z + halfvec.z, 1),   //frontbottomright
			inveyematrix * glm::vec4(center.x + halfvec.x,center.y + halfvec.y, center.z + halfvec.z, 1),   //fronttopright
																				 
			inveyematrix * glm::vec4(center.x - halfvec.x,center.y + halfvec.y, center.z - halfvec.z, 1),   //backtopleft
			inveyematrix * glm::vec4(center.x - halfvec.x,center.y - halfvec.y, center.z - halfvec.z, 1),   //backbottomleft
			inveyematrix * glm::vec4(center.x + halfvec.x,center.y - halfvec.y, center.z - halfvec.z, 1),   //backbottomright
			inveyematrix * glm::vec4(center.x + halfvec.x,center.y + halfvec.y, center.z - halfvec.z, 1),   //backtopright
			};

			for (int j = 0; j < points.size()*vertexattributelength; j += vertexattributelength)
			{
				int index = i * 8 * vertexattributelength;

				pointdata[index + j] = points[j / vertexattributelength].x;
				pointdata[index + j + 1] = points[j / vertexattributelength].y;
				pointdata[index + j + 2] = points[j / vertexattributelength].z;
			}
		}

		return pointdata;
	}



}