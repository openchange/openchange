#include <iostream>

#include <libmapi++/libmapi++.h>

static void dotest() throw(libmapipp::mapi_exception)
{
	throw libmapipp::mapi_exception(MAPI_E_SUCCESS, "mapi_exception test");
}

int main()
{
	try {
		dotest();
	}
	catch (libmapipp::mapi_exception e) {
		std::cout << e.what() << std::endl;
	}
	
	return 0;
}

