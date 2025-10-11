/**
 * @brief Application main entry with uni_server subclass
 */
#include <zpp/error.h>
#ifndef SVR_NAME
#include <zpp/hpx/server.hpp>
#endif

#include <hpx/hpx_init.hpp>
//#include <hpx/iostream.hpp>
#include <hpx/runtime.hpp>

USE_ZPP

int hpx_main(int argc, char* argv[])
{
    err_t err{ERR_FAIL};
    
#ifdef SVR_NAME
    // We can't easily pass tf_cores to constructor unless we change server signature or use global/static config
    // But wait, the server constructor is defined in THIS file above.
    // We can allow the constructor to check hpx::get_os_thread_count() directly.
    auto server_ptr = std::make_unique<SVR_NAME>(argc, argv);
#else
    auto server_ptr = std::make_unique<z::zhpx::server>(argc, argv);
#endif
    do{
        if(ERR_OK != (err = server_ptr->configure()))break;
        if(ERR_OK != (err = server_ptr->run()))break;
        
        server_ptr->loop(); 

        server_ptr->stop();
        server_ptr->wait_stop();
        err = ERR_OK;
    }while(0);

    return hpx::finalize();
}

int main(int argc, char **argv){
    return hpx::init(argc, argv);
}