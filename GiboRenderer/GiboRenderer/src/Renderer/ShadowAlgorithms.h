#pragma once
#include <algorithm>

namespace Gibo {
	/* Shadow implementation notes
	For sun shadow I used cascaded shadow maps. I also used SDSM, I used a depth prepass and did a gpu min/max reduction to just get farthest and closest depth z value based off the objects
	in the frustrum to get the perfect fit, then I used logarithmic spacing, this is the best fit you can get for the splits. If I disabled SDSM the spacing is just a mix of logarithmic and 
	uniform. Next I calculate frustrum  world positions and create a camera view matrix starting from frustrum centers and moving back in sun direction looking back at the frustrums center.
	Then I project all sides of frustrum to light space and get the min/max values for x,y,z and create an aabb off that. Thats the best fit I can do unless I actually do an algorithm to look at the geometry
	in the frustrum. If I want to use stabilize you just create sphere around frustrum and create aabb around that. After that you just create orthogonal matrix. Then you render each into a shadow
	atlas changing viewport. Also since I fit the aabb on the z values as well that means some occluders could be culled behind the near plane so in the vertex shader I pancake all occluders to the near plane,
	therefore all occluders will succesfully block light while putting near plane as close as possible. Then in shader you just figure out what cascade and get index into atlas and there you go.
	For this implementation I'm pretty happy using sdsm and pancaking got the best fitting frustrums and a lot of the shadows look super nice mostly not even needing filterling. Of course there
	are still some problems, I didn't really address filterling across cascades or blending between them, and theres still some jittering, as well as with sdsm since its delayed 3 frames theres 
	sometimes shadows popping in on the corners but that could be another bug because 3 frames should'nt be that noticeable.

	For point/spots I did basic shadow mapping for points I rendered shadows on 6 sides, and spots just 1 using perspective projection. I didn't use an perspective anti-aliasing techniques just
	filtering. they are put on an atlas as well. Without any perspective anti-aliasing and lower resolution the shadows look really blocky so its important to make lights have lower range, however
	using filtering helps especially random poisson really helps get rid of blockiness for more better looking noise.

	Theres still so many imporvements and new techniques to try. One is better/faster filterling. I could look at variance shadow maps. Also there are a bunch of ways to get rid of light leaking/
	acne for filtering. Theres also ways to address filtering across cube maps or cascaded maps. Also acne fighting has some more solutions too. Theres also so many more advanced shadow mapping
	technques to look into. I havn't done anything for soft shadows either just some pcf. I can also look into optimizing the atlas'es/etc.
	*/


	void CSM(const int cascade_count, bool SDSM_ENABLE, bool STABILIZE_ENABLE, float sun_distance, float fov, float near_depth, float far_depth, VkExtent2D proj_extent, glm::mat4 inveyematrix, glm::vec4 light_dir, std::vector<glm::mat4>& cammatrix,
		std::vector<glm::mat4>& projmatrix, std::vector<glm::vec4>& cascade_depths, std::vector<float>& cascade_nears, glm::mat4 cam_matrix, glm::mat4 proj_matrix)
	{
		//calculate splitting depths in view space and convert to world
		glm::mat4 inv_cam_matrix = inveyematrix;
		const int m = cascade_count; //number of cascades
		float a = (SDSM_ENABLE) ? 0.7 : .7; //linear combination variable

		std::vector<glm::vec4> cascade_positions_ws(m + 1);
		std::vector<float> cascade_positions_eyes(m + 1);
		cascade_positions_ws[0] = inv_cam_matrix * glm::vec4(0, 0, -near_depth, 1);
		cascade_positions_ws[m] = inv_cam_matrix * glm::vec4(0, 0, -far_depth, 1);
		cascade_positions_eyes[0] = near_depth;
		cascade_positions_eyes[m] = far_depth;
		for (int i = 1; i < m; i++)
		{
			float log_method = near_depth * pow(far_depth / near_depth, i / (float)m);
			float uniform_method = near_depth + (i / (float)m)*(far_depth - near_depth);
			float depth_value = a * log_method + (1 - a)*uniform_method;

			cascade_positions_eyes[i] = depth_value;
			cascade_positions_ws[i] = inv_cam_matrix * glm::vec4(0, 0, -depth_value, 1);
		}

		//calculate 4 points of each plane of cascade starting from near plane and ending on far plane. topleft,bottomleft,bottomright,topright
		std::vector<std::array<glm::vec4, 4>> cascade_planes_ws(m + 1);
		float tanval = tan(glm::radians(fov) / 2.0f);
		float aspect_ratio = (proj_extent.width / (float)proj_extent.height);
		for (int i = 0; i <= m; i++)
		{
			float depth = cascade_positions_eyes[i];
			float halfheight = tanval * depth;
			float halfwidth = halfheight * aspect_ratio;
			//go from eye space to world space
			cascade_planes_ws[i][0] = inv_cam_matrix * glm::vec4(-halfwidth,   halfheight, -depth, 1);
			cascade_planes_ws[i][1] = inv_cam_matrix * glm::vec4(-halfwidth,  -halfheight, -depth, 1);
			cascade_planes_ws[i][2] = inv_cam_matrix * glm::vec4( halfwidth,  -halfheight, -depth, 1);
			cascade_planes_ws[i][3] = inv_cam_matrix * glm::vec4( halfwidth,   halfheight, -depth, 1);
		}

		//calculate center of each frustrum
		std::vector<glm::vec4> frustrum_center_ws(m);
		for (int i = 0; i < m; i++)
		{
			glm::vec3 total(0, 0, 0);
			for (int j = 0; j < 8; j++)
			{
				if (j < 4)
				{
					total += glm::vec3(cascade_planes_ws[i][j]);
				}
				else
				{
					total += glm::vec3(cascade_planes_ws[i + 1][j - 4]);
				}
			}
			total /= 8.0f;

			frustrum_center_ws[i] = glm::vec4(total,1);
		}

		//create view matrix starting form center and moving back in light direction for very far amount because it doesn't really matter how far away it is if its directional light
		std::vector<glm::mat4> cascade_viewmatrix(m);
		glm::vec3 arbitrary_vector(0,1,0); //just picking this for now because light will probably never be looking up from underground. Can check dot product if its 1 pick 1,0,0
		if (abs(glm::dot(arbitrary_vector, glm::vec3(light_dir))) - 1 < .01) {
			arbitrary_vector = glm::vec3(0, 0, light_dir.y);
		}
		glm::vec3 right_vector = glm::cross(arbitrary_vector, glm::vec3(light_dir));
		glm::vec3 up_vector = glm::cross(glm::vec3(light_dir), right_vector);
		if (STABILIZE_ENABLE)
		{
			up_vector = glm::vec3(0, 1, 0);
		}

		for (int i = 0; i < m; i++)
		{
			glm::vec3 light_pos = glm::vec3(frustrum_center_ws[i]) + glm::vec3(-light_dir) * sun_distance; //make to infinity when finished
			cascade_viewmatrix[i] = glm::lookAt(light_pos, glm::vec3(frustrum_center_ws[i]), up_vector);
		}
		
		//now convert that frustrum to light space using that view matrix. Get the minium and maximum x and y values
		//I think you could also get max z value from here? 
		std::vector<glm::vec4> min_max(m); //x=minx, y=maxx, z=miny, w=maxy
		std::vector<glm::vec2> near_far(m);
		for (int i = 0; i < m; i++)
		{
			float value = 100000;
			min_max[i].x = value;
			min_max[i].y = -value;
			min_max[i].z = value;
			min_max[i].w = -value;
			near_far[i].x = value;
			near_far[i].y = -value;

			for (int j = 0; j < 8; j++)
			{
				glm::vec4 pos_lightspace;
				if (j < 4)
				{
					pos_lightspace = cascade_viewmatrix[i] * cascade_planes_ws[i][j];
				}
				else
				{
					pos_lightspace = cascade_viewmatrix[i] * cascade_planes_ws[i + 1][j-4];
				}
		
				min_max[i].x = std::min<float>(min_max[i].x, pos_lightspace.x);
				min_max[i].y = std::max<float>(min_max[i].y, pos_lightspace.x);
				min_max[i].z = std::min<float>(min_max[i].z, pos_lightspace.y);
				min_max[i].w = std::max<float>(min_max[i].w, pos_lightspace.y);

				//frustrum won't be behind sun because we move sun away from frustrum looking at it so z will always be negative
				near_far[i].x = std::min<float>(near_far[i].x, abs(pos_lightspace.z)); // 1.f;
				near_far[i].y = std::max<float>(near_far[i].y, abs(pos_lightspace.z));
			}
		}

		if (STABILIZE_ENABLE)
		{
			//instead of creating aabb you get radius that is the length of the farthest corner of the frustrum from its center. Then you wrap a box around that radius as new aabb
			//up direction has to be same apparently
			glm::vec3 up(0, 1, 0);
			for (int i = 0; i < m; i++)
			{
				float radius = 0.0f;
				for (int j = 0; j < 8; j++)
				{
					if (j < 4)
					{
						radius = std::max<float>(radius, glm::distance(glm::vec3(frustrum_center_ws[i]), glm::vec3(cascade_planes_ws[i][j])));
					}
					else
					{
						radius = std::max<float>(radius, glm::distance(glm::vec3(frustrum_center_ws[i]), glm::vec3(cascade_planes_ws[i+1][j - 4])));
					}
				}

				radius = std::ceil(radius * 16.0f) / 16.0f;

				min_max[i].x = -radius;
				min_max[i].y = radius;
				min_max[i].z = -radius;
				min_max[i].w = radius;
				near_far[i].x = (sun_distance - radius);
				near_far[i].y = (sun_distance + radius);
			}
		}

		//now with the max/min xyz create the orthographic matrix based on these numbers
		std::vector<glm::mat4> cascade_orthomatrix(m);
		for (int i = 0; i < m; i++)
		{
			if (STABILIZE_ENABLE)
			{
				//move by 1 texel increments
				float cascade_depth = min_max[i].y * 2;// glm::length(cascade_positions_ws[i + 1] - cascade_positions_ws[i]);
				float vWorldUnitsPerTexel = cascade_depth / (1024 * 3);

				min_max[i].x /= vWorldUnitsPerTexel;
				min_max[i].x = std::floor(min_max[i].x);
				min_max[i].x *= vWorldUnitsPerTexel;
				min_max[i].y /= vWorldUnitsPerTexel;
				min_max[i].y = std::floor(min_max[i].y);
				min_max[i].y *= vWorldUnitsPerTexel;
				min_max[i].z /= vWorldUnitsPerTexel;
				min_max[i].z = std::floor(min_max[i].z);
				min_max[i].z *= vWorldUnitsPerTexel;
				min_max[i].w /= vWorldUnitsPerTexel;
				min_max[i].w = std::floor(min_max[i].w);
				min_max[i].w *= vWorldUnitsPerTexel;
			}
			int padding = 1;//1
			int padding_depth = 1;
			cascade_orthomatrix[i] = glm::ortho(min_max[i].x - padding, min_max[i].y + padding, min_max[i].z - padding, min_max[i].w + padding, near_far[i].x - padding_depth, near_far[i].y + padding_depth);

			cascade_nears.push_back(near_far[i].x - padding_depth);
		}

		cammatrix.clear();
		projmatrix.clear();
		
		for (int i = 0; i < m; i++)
		{
			cammatrix.push_back(cascade_viewmatrix[i]);
			projmatrix.push_back(cascade_orthomatrix[i]);
		}

		for (int i = 1; i < m; i++)
		{
			cascade_depths.push_back(glm::vec4(cascade_positions_eyes[i], cascade_positions_eyes[i], cascade_positions_eyes[i], cascade_count));
		}
	}

	void PointSpotShadows(LightManager* lightmanager, std::vector<glm::mat4>& cam_matrixes, std::vector<glm::mat4>& projmatrixes, 
		                  std::vector<glm::mat4>& spotcam_matrixes, std::vector<glm::mat4>& spotprojmatrixes)
	{
		//get vector of all light id's that need shadow maps
		int point_count = 0;
		int spot_count = 0;
		int max_point_count = 0;
		std::vector<int> light_indices = lightmanager->GetShadow_Casts();
		for (int i = 0; i < light_indices.size(); i++)
		{
			if (Light::convert_float_to_type(lightmanager->GetLightFromMap(light_indices[i])->type) == Light::light_type::POINT) max_point_count++;
		}
		for (int i = 0; i < light_indices.size(); i++)
		{
			Light::lightparams* light_info = lightmanager->GetLightFromMap(light_indices[i]);
			if (light_info != nullptr)
			{
				Light::light_type type = Light::convert_float_to_type(light_info->type);
				float aspectratio = 1.0f;
				float near_plane = 1.0f;
				if (type == Light::light_type::POINT)
				{
					float point_range = light_info->falloff;
					glm::vec3 point_pos = glm::vec3(light_info->position);

					cam_matrixes.push_back(glm::lookAt(point_pos, point_pos + glm::vec3(1, 0, 0), glm::vec3(0, 1, 0))); //right
					cam_matrixes.push_back(glm::lookAt(point_pos, point_pos + glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0))); //left
					cam_matrixes.push_back(glm::lookAt(point_pos, point_pos + glm::vec3(0, 1, 0), glm::vec3(0, 0, 1))); //top
					cam_matrixes.push_back(glm::lookAt(point_pos, point_pos + glm::vec3(0, -1, 0), glm::vec3(0, 0, -1))); //bottom
					cam_matrixes.push_back(glm::lookAt(point_pos, point_pos + glm::vec3(0, 0, 1), glm::vec3(0, 1, 0))); //back
					cam_matrixes.push_back(glm::lookAt(point_pos, point_pos + glm::vec3(0, 0, -1), glm::vec3(0, 1, 0))); //front

					glm::mat4 proj_mat = glm::perspective(glm::radians(90.0f), aspectratio, near_plane, point_range);
					for (int j = 0; j < 6; j++)
					{
						projmatrixes.push_back(proj_mat);
					}
					//point lights come first in atlas map
					light_info->atlas_index = point_count * 6;
					point_count++;
				}
				else if (type == Light::light_type::SPOT || type == Light::light_type::FOCUSED_SPOT)
				{
					float point_range = light_info->falloff;
					glm::vec3 point_pos = glm::vec3(light_info->position);
					glm::vec3 dir = glm::vec3(light_info->direction);
					float fov = light_info->outerangle * 2;

					glm::vec3 up(0, 1, 0);
					if (abs(glm::dot(up, dir)) - 1 < .01)
						up = glm::vec3(0, 0, dir.y);

					spotcam_matrixes.push_back(glm::lookAt(point_pos, point_pos + dir, up));
					spotprojmatrixes.push_back(glm::perspective(fov, aspectratio, near_plane, point_range));

					//spot lights come after spot lights in atlas
					light_info->atlas_index = max_point_count * 6 + spot_count;
					spot_count++;
				}
				else
				{
					//
				}
			}
		}
	}

}