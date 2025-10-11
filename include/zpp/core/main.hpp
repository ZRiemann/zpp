/**
 * @brief Application main entry with uni_server subclass
 */
#include <zpp/error.h>
#ifndef SVR_NAME
#include <zpp/core/server.h>
#endif

USE_ZPP

int main(int argc, char **argv){
    err_t err{ERR_FAIL};
#ifdef SVR_NAME
    auto server_ptr = std::make_unique<SVR_NAME>(argc, argv);
#else
    auto server_ptr = std::make_unique<z::server>(argc, argv);
#endif
    do{
        if(ERR_OK != (err = server_ptr->configure()))break;
        if(ERR_OK != (err = server_ptr->run()))break;
        server_ptr->loop();
        server_ptr->stop();
        server_ptr->wait_stop();
        err = ERR_OK;
    }while(0);
    return err;
}
