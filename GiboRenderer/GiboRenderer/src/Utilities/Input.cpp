#include "../pch.h"
#include "Input.h"

namespace Gibo {

	//Call_Backs
	void Input::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		if (action == GLFW_PRESS) {
			SetKeys(key, true);
			SetKeysOnce(key, true);
		}
		else if (action == GLFW_RELEASE) {
			SetKeys(key, false);
			SetKeysOnce(key, false);
		}
		//action  GLFW_PRESS, GLFW_REPEAT, GLFW_RELEASE
	}

	void Input::scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
	{
		Setscroll(yoffset);
	}

	void Input::cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
	{
		Setmousexyv(xpos - Getmousex(), ypos - Getmousey());
		Setmousexy(xpos, ypos);

		mousex = xpos;
		mousey = ypos;
	}

	void Input::mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
	{
		if (button == GLFW_MOUSE_BUTTON_LEFT) {
			if (action == GLFW_PRESS) {
				Setmouseleft(true);
			}
			else if (action == GLFW_RELEASE) {
				Setmouseleft(false);
			}
		}
		if (button == GLFW_MOUSE_BUTTON_RIGHT) {
			if (action == GLFW_PRESS) {
				Setmouseright(true);
			}
			else if (action == GLFW_RELEASE) {
				Setmouseright(false);
			}
		}


		//GLFW_PRESS, GLFW_RELEASE
		//GLFW_MOUSE_BUTTON_LEFT
		//if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
	}

	Input::Input() {
		mousex = 0;
		mousey = 0;
		mousevx = mousex;
		mousevy = mousey;
		leftmouse = false;
		rightmouse = false;
		for (int i = 0; i < 500; i++) {
			Keys[i] = false;
		}
	}

	//get mouse position
	void Input::GetMousePos(double& x, double& y) const 
	{
		x = mousex;
		y = mousey;
		//glfwGetCursorPos(thewindow, &x, &y);
	}

	void Input::GetMouseVelocity(double& x, double& y) const
	{
		x = mousevx;
		y = mousevy;
	}

	void Input::GetMouseButtons(bool& l, bool& r) const
	{
		l = leftmouse;
		r = rightmouse;
	}

	bool Input::GetKeyPress(int i) const
	{
		return Keys[i];
	}

	bool Input::GetKeyPressOnce(int i) 
	{
		bool val = KeysOnce[i];
		KeysOnce[i] = false;
		return val;
	}

	void  Input::PrintInfo()
	{
		//Logger::instance->LogInfo("(" + std::to_string(mousex) + ", " + std::to_string(mousex) + "), (" + std::to_string(mousevx) + ", " + std::to_string(mousevy) + ") " +
			//std::to_string(leftmouse) + "|" + std::to_string(rightmouse) + "  " + std::to_string(scroll) + "ESC: " + std::to_string(Keys[GLFW_KEY_ESCAPE]) + "\n");
	}

}