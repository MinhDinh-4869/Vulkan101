#include <iostream>
#include <cstdlib>
#include "TriangleApplication.h"

//Create an application to draw a triagle

int main()
{
	//Init the application
	TriangleApplication app;

	try
	{
		app.run();
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}