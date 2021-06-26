#pragma once
//#define GLFW_INCLUDE_VULKAN
#include <GLFW\glfw3.h>
#include "../pch.h"
#include "../ThirdParty/stb_image.h"


namespace Gibo {
	
	//CALLBACKS
	static void error_callback(int error_code, const char* description)
	{
		Logger::LogGLFW("code: " + std::to_string(error_code) + " - " + std::string(description) + "\n");
	}

	static void monitor_callback(GLFWmonitor* monitor, int event) {
		std::string name = glfwGetMonitorName(monitor);
		if (event == GLFW_CONNECTED)
		{
			Logger::LogGLFW("Monitor connected: " + name + "\n");
		}
		else if (event == GLFW_DISCONNECTED)
		{
			Logger::LogGLFW("Monitor disconnected: " + name + "\n");
		}
	}

	static void window_size_callback(GLFWwindow* window, int width, int height)
	{
		Logger::LogGLFW("Window Resized: " + std::to_string(width) + " " + std::to_string(height) + "\n");
	}

	static void window_focus_callback(GLFWwindow* window, int focused)
	{
		if (focused)
		{
			// The window gained input focus
		}
		else
		{
			// The window lost input focus
			//glfwFocusWindow(window);
		}
	}

	//class that holds contexts, windows, monitors, inputs, everything for glfw
	class glfw_handler {

	public:
		glfw_handler() = default;
		~glfw_handler() = default;

		//FUNCTIONALITY
		void PrintMonitors()
		{
			//GLFWmonitor* primary = glfwGetPrimaryMonitor();

			int count;
			GLFWmonitor** monitors = glfwGetMonitors(&count);

			Gibo::Logger::LogInfo("----Monitor Information----\n");
			for (int i = 0; i < count; i++)
			{
				PrintMonitor(monitors[i]);
			}
		}

		void PrintMonitor(GLFWmonitor* mon)
		{
			std::string name = glfwGetMonitorName(mon);

			const GLFWvidmode* vm = glfwGetVideoMode(mon);

			int mmx = -1;
			int mmy = -1;
			glfwGetMonitorPhysicalSize(mon, &mmx, &mmy);

			int vs = -1;
			const GLFWvidmode* vz = glfwGetVideoModes(mon, &vs);

			for (int j = vs; j < vs; j++) {
				Gibo::Logger::LogInfo("resolutionx: " + std::to_string(vz[j].width) + " resolutiony: " + std::to_string(vz[j].height) + " redbits: " + std::to_string(vz[j].redBits)
					+ " greenbits: " + std::to_string(vz[j].greenBits) + " bluebits: " + std::to_string(vz[j].blueBits) + " refreshrate: " +
					std::to_string(vz[j].refreshRate) + "hz\n");
			}

			Gibo::Logger::LogInfo("name: " + name + "\n" +
				"resolutionx: " + std::to_string(vm->width) + " resolutiony: " + std::to_string(vm->height) + " redbits: " + std::to_string(vm->redBits)
				+ " greenbits: " + std::to_string(vm->greenBits) + " bluebits: " + std::to_string(vm->blueBits) + " refreshrate: " + std::to_string(vm->refreshRate) + "hz\n"
				+ "physical width: " + std::to_string(mmx) + "mm physical height: " + std::to_string(mmy) + "mm\n");
		}

		void SetVSync(bool i) {
			glfwSwapInterval(i);
		}

		void SetFullScreen(int x, int y) {
			GLFWmonitor* monitorz = glfwGetPrimaryMonitor();
			glfwSetWindowMonitor(window, monitorz, 0, 0, x, y, 60);
			//window positions are ignored
		}

		void SetWindowedMode(int px, int py, int rx, int ry) {
			glfwSetWindowMonitor(window, nullptr, px, py, rx, ry, 60);
			//refresh rate ignored
		}

		void SetWindowSize_or_Resolution(int x, int y) {
			glfwSetWindowSize(window, x, y); //windowed mode sets size, fullscreen mode sets resolution
		}

		void getwindowframebuffersize(int& w, int& h) {
			glfwGetFramebufferSize(window, &w, &h);
			//window size is in screen position, this will get more accurate frame size
			Gibo::Logger::LogGLFW("WindowFrameSize: (" + std::to_string(w) + "," + std::to_string(h) + ")\n");
		}

		void ChangeAspectRatio(int x, int y) {
			glfwSetWindowAspectRatio(window, x, y);
		}

		void SetWindowTitle(std::string title) const {
			glfwSetWindowTitle(window, title.c_str());
		}

		void SetWindowIcon() {
			GLFWimage images[1];
			int bitDepth = -1;
			images[0].pixels = stbi_load("Images/logos/Logo2.png", &images[0].width, &images[0].height, &bitDepth, 0);

			glfwSetWindowIcon(window, 1, images);
		}

		void SetCursorIcon() {
			GLFWimage images;
			int bitDepth = -1;
			images.pixels = stbi_load("Images/Logo2.png", &images.width, &images.height, &bitDepth, 0);

			GLFWcursor* cursor = glfwCreateCursor(&images, 0, 0);
			glfwSetCursor(window, cursor);
			//glfwDestroyCursor(cursor);
		}

		void CursorEnable(bool b) {
			if (b) {
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			}
			else {
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			}
		}

		void PingWindow() {
			glfwRequestWindowAttention(window);
		}

		void FocusWindow() {
			glfwFocusWindow(window);
		}

		bool CreateNewWindowVulkan(int width, int height, std::string title)
		{
			//set window parameters
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
			glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
			glfwWindowHint(GLFW_REFRESH_RATE, GLFW_DONT_CARE);

			window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);

			if (glfwRawMouseMotionSupported()) {
				glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE); //only allowed when cursor is disabled
			}

			monitor = glfwGetWindowMonitor(window);

			if (!window)
			{
				glfwTerminate();
				Logger::LogError("failed to initialize vulkan window!");
				return false;
			}
			return true;
		}

		void BindContext() {
			if (window != nullptr) {
				glfwMakeContextCurrent(window);
			}
			else {
				Logger::LogError("couldnt make context current. Window null!\n");
			}
		}

		void UnBindContext() {
			glfwMakeContextCurrent(0);
		}

		bool InitializeVulkan(int thewidth, int theheight, GLFWframebuffersizefun framebuffer_size_callback, GLFWkeyfun key_callback, GLFWcursorposfun cursor_callback, GLFWmousebuttonfun mouse_callback,
								GLFWscrollfun scroll_callback)
		{
			if (!glfwInit()) {
				Logger::LogError("failed to initialize glfw!");
				return false;
			}

			if (!glfwVulkanSupported())
			{
				Logger::LogError("glfw vulkan not supported!");
				return false;
			}

			glfwSetErrorCallback(error_callback);
			glfwSetMonitorCallback(monitor_callback);

			if (!CreateNewWindowVulkan(thewidth, theheight, "GiboRenderer (Rasterizer)"))
			{
				return false;
			}

			//set callbacks
			glfwSetWindowSizeCallback(window, window_size_callback);
			glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
			glfwSetWindowFocusCallback(window, window_focus_callback);

			glfwSetKeyCallback(window, key_callback);
			glfwSetCursorPosCallback(window, cursor_callback);
			glfwSetMouseButtonCallback(window, mouse_callback);
			glfwSetScrollCallback(window, scroll_callback);

			SetWindowIcon();

			return true;
		}

		void Terminate() {
			GLFWwindow* lol;
			glfwDestroyWindow(window);
			glfwTerminate();
		}

		GLFWwindow* Getwindow() const {
			return window;
		}
	private:
		GLFWwindow* window;
		GLFWmonitor* monitor;
	};

}