#pragma once

#include "socket.h"

NSB_NNG
/**
 * @class socket_x
 * @brief nng socket with pipe event notifications
 */
class socket_x : public socket{
public:
    socket_x(): socket(){
    }
    virtual ~socket_x() = 0;
public:
#pragma region Pipe Notifications
    /**
     * @brief Pipe Event Callbacks
     * @note Tip
     * The callback cb may close a pipe for any reason by simply closing it using nng_pipe_close.
     * For example, this might be done to prevent an unauthorized peer from connecting to the socket, 
     * if an authorization check made during NNG_PIPE_EV_ADD_PRE fails.
     */
    virtual void on_add_pre(nng_pipe p) = 0; //{ims_inf("pipe on_add_pre: {}", id());}
    virtual void on_add_post(nng_pipe p) = 0; //{ims_inf("pipe on_add_post: {}", id());}
    virtual void on_remove_pre(nng_pipe p) = 0; //{ims_inf("pipe on_remove_pre: {}", id());}

    /**
     * @brief Pipe Event Callback
     * @note Warning!
     *  The callback cb function must not attempt to perform any accesses to the socket, 
     *  as it is called with a lock on the socket held!
     *  Doing so would thus result in a deadlock.
     */
    static inline void pipe_cb(nng_pipe p, nng_pipe_ev ev, void* arg){
        socket* sock = static_cast<socket*>(arg);
        switch(ev){
            case NNG_PIPE_EV_ADD_PRE:
                sock->on_add_pre(p);
                break;
            case NNG_PIPE_EV_ADD_POST:
                sock->on_add_post(p);
                break;
            case NNG_PIPE_EV_REMOVE_POST:
                sock->on_remove_pre(p);
                break;
            default:
                ims_err("unknown pipe event: {} for pipe {}", (int)ev, nng_pipe_id(p));
                break;
        }
    }
    int pipe_notify(){
        int ret = (int)nng_pipe_notify(_sock, NNG_PIPE_EV_ADD_PRE, &socket::pipe_cb, this);
        int ret1 = (int)nng_pipe_notify(_sock, NNG_PIPE_EV_ADD_POST, &socket::pipe_cb, this);
        int ret2 = (int)nng_pipe_notify(_sock, NNG_PIPE_EV_REMOVE_POST, &socket::pipe_cb, this);
        if(ret != 0 || ret1 != 0 || ret2 != 0){
            spd_err("nng_pipe_notify ADD_PRE failed: {} {} {}",
                strerr(ret), strerr(ret1), strerr(ret2));
            return -1;
        }
        return 0;
    }
#pragma endregion
};
NSE_NNG