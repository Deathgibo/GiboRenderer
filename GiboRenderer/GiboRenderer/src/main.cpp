#include "pch.h"
#include "TestApplication.h"
#include "TestApplication2.h"

#define TESTAPPLICATION_1 1
#define TESTAPPLICATION_2 0

int main()
{
#if TESTAPPLICATION_1
	TestApplication mainapp;
#elif TESTAPPLICATION_2
	TestApplication2 mainapp;
#endif

	try {
		mainapp.run();
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}