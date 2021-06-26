#include "pch.h"
#include "TestApplication.h"

int main()
{

	TestApplication mainapp;
	try {
		mainapp.run();
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}