#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <string>

/*
	This is class used to print information througout the program. Its a static class that just calls functions.
	It shouldn't be instantiated as an object. Also has a nice macro to print out vulkan return statements.
	Really its uses is for turning it off on release mode, printing different colors, pretexts, and having vulkan debug printing.
*/

namespace Gibo {

	
	#define TEXT_RED 12
	#define TEXT_YELLOW 14
	#define TEXT_BLACK 1
	#define TEXT_GREEN 10
	#define TEXT_ORANGE 5
	#define TEXT_WHITE 7

	#define VULKAN_CHECK(result, text) Logger::vulkan_check(result, text)

	template <typename T>
	void variadic_print(T v) {
		std::cout << v;
	}
	template <typename T, typename... Types>
	void variadic_print(T var1, Types... var2) {
		std::cout << var1;
		variadic_print(var2...);
	}

	class Logger {
	public:


		static void vulkan_check(int result, std::string text)
		{
#ifndef _DEBUG 
			return;
#endif

			if (result != 0)
			{
				HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
				SetConsoleTextAttribute(hConsole, TEXT_WHITE);
				printf(text.c_str());
				printf(" - ");
				LogVKError(result);
			}
		}

		static void LogVKError(int result)
		{
#ifndef _DEBUG 
			return;
#endif

			HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
			SetConsoleTextAttribute(hConsole, TEXT_RED);
			printf("VULKAN ERROR CODE: ");

			SetConsoleTextAttribute(hConsole, TEXT_WHITE);
			printf(VKErrorToString(result).c_str());
			printf("\n");
		}


		template <typename T, typename... Types>
		static void LogTime(T var1, Types... var2)
		{
#ifndef _DEBUG 
			return;
#endif
			HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
			SetConsoleTextAttribute(hConsole, TEXT_GREEN);
			printf("TIME: ");

			SetConsoleTextAttribute(hConsole, TEXT_WHITE);
			variadic_print(var1, var2...);
		}

		template <typename T, typename... Types>
		static void Log(T var1, Types... var2)
		{
#ifndef _DEBUG 
			return;
#endif

			variadic_print(var1, var2...);
		}

		template <typename T, typename... Types>
		static void LogPerformance(T var1, Types... var2)
		{
#ifndef _DEBUG 
			return;
#endif
			HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
			SetConsoleTextAttribute(hConsole, TEXT_GREEN);

			SetConsoleTextAttribute(hConsole, TEXT_WHITE);
			variadic_print(var1, var2...);
		}

		template <typename T, typename... Types>
		static void LogError(T var1, Types... var2)
		{
#ifndef _DEBUG 
			return;
#endif
			HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
			SetConsoleTextAttribute(hConsole, TEXT_RED);
			printf("ERROR: ");

			SetConsoleTextAttribute(hConsole, TEXT_WHITE);
			variadic_print(var1, var2...);
		}

		template <typename T, typename... Types>
		static void LogWarning(T var1, Types... var2)
		{
#ifndef _DEBUG 
			return;
#endif
			HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
			SetConsoleTextAttribute(hConsole, TEXT_YELLOW);
			printf("WARNING: ");

			SetConsoleTextAttribute(hConsole, TEXT_WHITE);
			variadic_print(var1, var2...);
		}

		template <typename T, typename... Types>
		static void LogInfo(T var1, Types... var2)
		{
#ifndef _DEBUG 
			return;
#endif
			HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
			SetConsoleTextAttribute(hConsole, TEXT_WHITE);
			printf("INFO: ");

			SetConsoleTextAttribute(hConsole, TEXT_WHITE);
			variadic_print(var1, var2...);
		}

		template <typename T, typename... Types>
		static void LogGLFW(T var1, Types... var2)
		{
#ifndef _DEBUG 
			return;
#endif
			HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
			SetConsoleTextAttribute(hConsole, 8);
			printf("GLFW: ");

			SetConsoleTextAttribute(hConsole, TEXT_WHITE);
			variadic_print(var1, var2...);
		}

	private:

		static std::string VKErrorToString(int result)
		{
			switch (result)
			{
			case 0: return "VK_SUCCESS";
			case 1: return "VK_NOT_READY";
			case 2: return "VK_TIMEOUT";
			case 3: return "VK_EVENT_SET";
			case 4: return "VK_EVENT_RESET";
			case 5: return "VK_INCOMPLETE";
			case -1: return "VK_ERROR_OUT_OF_HOST_MEMORY";
			case -2: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
			case -3: return "VK_ERROR_INITIALIZATION_FAILED";
			case -4: return "VK_ERROR_DEVICE_LOST";
			case -5: return "VK_ERROR_MEMORY_MAP_FAILED";
			case -6: return "VK_ERROR_LAYER_NOT_PRESENT";
			case -7: return "VK_ERROR_EXTENSION_NOT_PRESENT";
			case -8: return "VK_ERROR_FEATURE_NOT_PRESENT";
			case -9: return "VK_ERROR_INCOMPATIBLE_DRIVER";
			case -10: return "VK_ERROR_TOO_MANY_OBJECTS";
			case -11: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
			case -12: return "VK_ERROR_FRAGMENTED_POOL";
			case -13: return "VK_ERROR_UNKNOWN";
			case -1000069000: return "VK_ERROR_OUT_OF_POOL_MEMORY";
			case -1000072003: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
			case -1000161000: return "VK_ERROR_FRAGMENTATION";
			case -1000257000: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
			case -1000000000: return "VK_ERROR_SURFACE_LOST_KHR";
			case -1000000001: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
			case 1000001003: return "VK_SUBOPTIMAL_KHR";
			case -1000001004: return "VK_ERROR_OUT_OF_DATE_KHR";
			case -1000003001: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
			case -1000011001: return "VK_ERROR_VALIDATION_FAILED_EXT";
			case -1000012000: return "VK_ERROR_INVALID_SHADER_NV";
			case -1000150000: return "VK_ERROR_INCOMPATIBLE_VERSION_KHR";
			case -1000158000: return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
			case -1000174001: return "VK_ERROR_NOT_PERMITTED_EXT";
			case -1000255000: return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
			case 1000268000: return "VK_THREAD_IDLE_KHR";
			case 1000268001: return "VK_THREAD_DONE_KHR";
			case 1000268002: return "VK_OPERATION_DEFERRED_KHR";
			case 1000268003: return "VK_OPERATION_NOT_DEFERRED_KHR";
			case 1000297000: return "VK_PIPELINE_COMPILE_REQUIRED_EXT";
			default: return "ERROR_CODE_NOT_FOUND";
			}
		}
	};

}