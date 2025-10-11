#include "defs.h"
#include "message.h"
#include "aio.h"
#include <zpp/spdlog.h>

NSB_NNG
/**
 * @class socket
 * @brief nng socket wrapper
 */
class socket{
public:
    socket() = default;
    virtual ~socket() = default;
#pragma region Sending and Receiving Messages
    inline int send(message &msg, int flags = NNG_FLAG_NONBLOCK){
        int ret = nng_sendmsg(_sock, msg._msg, flags);
        if(ret != 0){
            // ret = NNG_EAGAIN is normal in non-blocking mode
            spd_err("nng_send failed: {}", strerr(ret));
        }else{
            msg._owned = false;
        }
        return ret;
    }
    inline void send(aio& io){
        nng_socket_send(_sock, io._aio);
    }

    inline int recv(message &msg, int flags = 0){
        int ret = nng_recvmsg(_sock, &msg._msg, flags);
        if(ret != 0){
            // flags = 0 block and wait for message arrive
            spd_err("nng_recv failed: {}", strerr(ret));
            msg._owned = false;
        }
        return ret;
    }
    inline void recv(aio& io){
        nng_socket_recv(_sock, io._aio);
    }
#pragma endregion
#pragma region Socket Identifier
    int id() const {
        return nng_socket_id(_sock);
    }
    int raw(bool *raw){
        return nng_socket_raw(_sock, raw);
    }
    int proto_id(uint16_t *idp){
        return nng_socket_proto_id(_sock, idp);
    }
    int peer_id(uint16_t *idp){
        return nng_socket_peer_id(_sock, idp);
    }
    int proto_name(const char **name){
        return nng_socket_proto_name(_sock, name);
    }
    int peer_name(const char **name){
        return nng_socket_peer_name(_sock, name);
    }
#pragma endregion
#pragma region Opening a Socket
    int bus0_open(){
        int ret = nng_bus0_open(&_sock);
        spd_inf("nng_bus0_open[{}] {}", id(), strerr(ret));
        return ret;
    }
    int pub0_open(){
        int ret = nng_pub0_open(&_sock);
        spd_inf("nng_pub0_open[{}] {}", id(), strerr(ret));
        return ret;
    }
    int sub0_open(){
        int ret = nng_sub0_open(&_sock);
        spd_inf("nng_sub0_open[{}] {}", id(), strerr(ret));
        return ret;
    }
    int pull0_open(){
        int ret = nng_pull0_open(&_sock);
        spd_inf("nng_pull0_open[{}] {}", id(), strerr(ret));
        return ret;
    }
    int push0_open(){
        int ret = nng_push0_open(&_sock);
        spd_inf("nng_push0_open[{}] {}", id(), strerr(ret));
        return ret;
    }
    int rep0_open(){
        int ret = nng_rep0_open(&_sock);
        spd_inf("nng_rep0_open[{}] {}", id(), strerr(ret));
        return ret;
    }
    int req0_open(){
        int ret = nng_req0_open(&_sock);
        spd_inf("nng_req0_open[{}] {}", id(), strerr(ret));
        return ret;
    }
    int respondent0_open(){
        int ret = nng_respondent0_open(&_sock);
        spd_inf("nng_respondent0_open[{}] {}", id(), strerr(ret));
        return ret;
    }
    int surveyor0_open(){
        int ret = nng_surveyor0_open(&_sock);
        spd_inf("nng_surveyor0_open[{}] {}", id(), strerr(ret));
        return ret;
    }
    int pare0_open(){
        int ret = nng_pair0_open(&_sock);
        spd_inf("nng_pair0_open[{}] {}", id(), strerr(ret));
        return ret;
    }
    int pair1_open(){
        int ret = nng_pair1_open(&_sock);
        spd_inf("nng_pair1_open[{}] {}", id(), strerr(ret));
        return ret;
    }
    int pare1_open_poly(){
        int ret = nng_pair1_open_poly(&_sock);
        spd_inf("nng_pair1_open_poly[{}] {}", id(), strerr(ret));
        return ret;
    }
#pragma endregion
#pragma region Socket Options
    /**
     * the option should be set on the dialer or listener instead of the socket.
     * NNG_OPT_MAXTTL	int	Maximum number of traversals across an nng_device device, to prevent forwarding loops. May be 1-255, inclusive. Normally defaults to 8.
     * NNG_OPT_RECONNMAXT	nng_duration	Maximum time dialers will delay before trying after failing to connect.
     * NNG_OPT_RECONNMINT	nng_duration	Minimum time dialers will delay before trying after failing to connect.
     * NNG_OPT_RECVBUF	int	Maximum number of messages (0-8192) to buffer locally when receiving.
     * NNG_OPT_RECVMAXSZ	size_t	Maximum message size acceptable for receiving. Zero means unlimited. Intended to prevent remote abuse. Can be tuned independently on dialers and listeners.
     * NNG_OPT_RECVTIMEO	nng_duration	Default timeout (ms) for receiving messages.
     * NNG_OPT_SENDBUF	int	Maximum number of messages (0-8192) to buffer when sending messages.
     * NNG_OPT_SENDTIMEO	nng_duration	Default timeout (ms) for sending messages.
     */
    int set_bool(const char* name, bool v){
        int ret = nng_socket_set_bool(_sock, name, v);
        if(ret != 0){
            spd_err("nng_socket_set_bool failed: {}", strerr(ret));
        }
        return ret;
    }
    int get_bool(const char* name, bool* v){
        int ret = nng_socket_get_bool(_sock, name, v);
        if(ret != 0){
            spd_err("nng_socket_get_bool failed: {}", strerr(ret));
        }
        return ret;
    }
    int set_int(const char* name, int v){
        int ret = nng_socket_set_int(_sock, name, v);
        if(ret != 0){
            spd_err("nng_socket_set_int failed: {}", strerr(ret));
        }
        return ret;
    }
    int get_int(const char* name, int* v){
        int ret = nng_socket_get_int(_sock, name, v);
        if(ret != 0){
            spd_err("nng_socket_get_int failed: {}", strerr(ret));
        }
        return ret;
    }
    int set_size(const char* name, size_t v){
        int ret = nng_socket_set_size(_sock, name, v);
        if(ret != 0){
            spd_err("nng_socket_set_size failed: {}", strerr(ret));
        }
        return ret;
    }
    int get_size(const char* name, size_t* v){
        int ret = nng_socket_get_size(_sock, name, v);
        if(ret != 0){
            spd_err("nng_socket_get_size failed: {}", strerr(ret));
        }
        return ret;
    }
    int set_ms(const char* name, nng_duration v){
        int ret = nng_socket_set_ms(_sock, name, v);
        if(ret != 0){
            spd_err("nng_socket_set_ms failed: {}", strerr(ret));
        }
        return ret;
    }
    int get_ms(const char* name, nng_duration* v){
        int ret = nng_socket_get_ms(_sock, name, v);
        if(ret != 0){
            spd_err("nng_socket_get_ms failed: {}", strerr(ret));
        }
        return ret;
    }
public: // advanced APIs
    int listen(){
        return 0;
    
    }
    int dial(){
        return 0;
    }
#pragma endregion
// raw sockets reserved
// Pollig Socket Events reserved
public:
    nng_socket _sock{NNG_SOCKET_INITIALIZER};
};
NSE_NNG
