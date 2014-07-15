#include <cstdlib>
#include <iostream>
#include <string>

#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include <boost/fiber/all.hpp>

inline
int fn( std::string const& str, int n)
{
	for ( int i = 0; i < n; ++i)
	{
		std::cout << i << ": " << str << std::endl;
		boost::this_fiber::yield();
	}

    return n;
}

void start()
{
    boost::fibers::future< int > fi(
        boost::fibers::async( fn, "abc", 5) );
    fi.wait();
    std::cout << "fn() returned " << fi.get() << std::endl;
}

int main()
{
	try
	{
        boost::fibers::fiber( start).join();
		std::cout << "done." << std::endl;

		return EXIT_SUCCESS;
	}
	catch ( std::exception const& e)
	{ std::cerr << "exception: " << e.what() << std::endl; }
	catch (...)
	{ std::cerr << "unhandled exception" << std::endl; }
	return EXIT_FAILURE;
}
