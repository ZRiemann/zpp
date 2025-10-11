#include <hpx/hpx_init.hpp>
#include <hpx/iostream.hpp>

int hpx_main(int, char*[])
{
    // Say hello to the world!
    hpx::cout << "Hello World!\n" << std::flush;
    return hpx::finalize();
}

int main(int argc, char* argv[])
{
    return hpx::init(argc, argv);
}