#pragma once
#include <GLFW\glfw3.h>

namespace Gibo {

	class Input {
	public:
		Input();
		~Input() = default;

		//no copying/moving should be allowed from this class
		// disallow copy and assignment
		Input(Input const&) = delete;
		Input(Input&&) = delete;
		Input& operator=(Input const&) = delete;
		Input& operator=(Input&&) = delete;

		//callbacks
		void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
		void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
		void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
		void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

		void PrintInfo();

		void GetMousePos(double& x, double& y) const;
		void GetMouseVelocity(double& x, double& y) const;
		void GetMouseButtons(bool& l, bool& r) const;
		bool GetKeyPress(int i) const;
		bool GetKeyPressOnce(int i);
		int GetScroll() const { return scroll; }
		double Getmousex() const  { return mousex; }
		double Getmousey() const  { return mousey; }
		bool* GetKeys()  { return &Keys[0]; }

		void Setmousexy(double x, double y) { mousex = x; mousey = y; }
		void Setmousexyv(double x, double y) { mousevx = x; mousevy = y; }
		void Setmouseleft(bool l) { leftmouse = l; }
		void Setmouseright(bool r) { rightmouse = r; }
		void SetKeys(int i, bool val) { Keys[i] = val; }
		void SetKeysOnce(int i, bool val) { KeysOnce[i] = val; }
		void Setscroll(int i) { scroll = i; }
	private:
		double mousex;
		double mousey;
		double mousevx, mousevy; //the difference between the mouse movement in the last 2 mouse position callbacks. MouseCallback1.pos - MouseCallBack1.pos
		int scroll;
		bool Keys[500];
		bool KeysOnce[500];
		bool leftmouse;
		bool rightmouse;
	};

}