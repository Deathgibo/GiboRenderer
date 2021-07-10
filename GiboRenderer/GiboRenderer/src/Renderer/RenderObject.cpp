#include "../pch.h"
#include "RenderObject.h"

namespace Gibo {


	void RenderObject::SetTransformation(glm::vec3 position, glm::vec3 scale, ROTATE_DIMENSION dimension, float indegrees)
	{
		glm::mat4 scalez = glm::scale(glm::mat4(1.0), scale);

		glm::vec3 rotatedim = glm::vec3(0, 0, 0);
		switch (dimension)
		{
		case ROTATE_DIMENSION::XANGLE: rotatedim.x = 1; break;
		case ROTATE_DIMENSION::YANGLE: rotatedim.y = 1; break;
		case ROTATE_DIMENSION::ZANGLE: rotatedim.z = 1; break;
		}

		glm::mat4 rotate = glm::rotate(glm::mat4(1.0), glm::radians(indegrees), rotatedim);

		glm::mat4 translate = glm::translate(glm::mat4(1.0), position);

		internal_matrix = translate * rotate * scalez;

		NotifyUpdate();
	}

	void RenderObject::Update(int framecount)
	{
		if (needs_updated)
		{
			model_matrix[framecount] = internal_matrix;

			frames_updated++;
			if (frames_updated >= model_matrix.size())
			{
				frames_updated = 0;
				needs_updated = false;
			}
		}
	}


}