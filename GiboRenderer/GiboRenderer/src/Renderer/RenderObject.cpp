#include "../pch.h"
#include "RenderObject.h"

namespace Gibo {


	void RenderObject::SetTransformation(glm::vec3 position, glm::vec3 scale, ROTATE_DIMENSION dimension, float indegrees)
	{
		model_matrix = glm::scale(glm::mat4(1.0), scale);
	
		glm::vec3 rotatedim = glm::vec3(0, 0, 0);
		switch (dimension)
		{
		case ROTATE_DIMENSION::XANGLE: rotatedim.x = 1; break;
		case ROTATE_DIMENSION::YANGLE: rotatedim.y = 1; break;
		case ROTATE_DIMENSION::ZANGLE: rotatedim.z = 1; break;
		}

		model_matrix = glm::rotate(model_matrix, glm::radians(indegrees), rotatedim);

		model_matrix = glm::translate(model_matrix, position);
	}


}