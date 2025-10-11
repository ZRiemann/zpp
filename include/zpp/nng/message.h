#pragma once

#include "defs.h"
#include <zpp/spdlog.h>

NSB_NNG
class message{
public:
    message(){
        // Used by socket recv message
    }
    message(size_t size){
        int ret = nng_msg_alloc(&_msg, size);
        if(ret != 0){
            spd_err("nng_msg_alloc failed: {}", strerr(ret));
        }
        //spd_inf("_msg allocated: {}", (void*)_msg);
    }
    ~message(){
        if(_owned && _msg){
            //spd_inf("_msg{} freed.", (void*)_msg);
            nng_msg_free(_msg);
            //_msg = nullptr;
        }
    }
    message(const message& duplicate){
        _owned = true;
        int ret = nng_msg_dup(&_msg, duplicate._msg);
        if(ret != 0){
            spd_err("nng_msg_dup failed: {}", strerr(ret));
        }
    }

public:
    size_t capacity() const {
        return nng_msg_capacity(_msg);
    }
    int reallocate(size_t size) {
        int ret = nng_msg_realloc(_msg, size);
        if(ret != 0){
            spd_err("nng_msg_realloc failed: {}", strerr(ret));
        }
        return ret;
    }
    int reserve(size_t size) {
        int ret = nng_msg_reserve(_msg, size);
        if(ret != 0){
            spd_err("nng_msg_reserve failed: {}", strerr(ret));
        }
        return ret;
    }
    size_t length() const {
        return nng_msg_len(_msg);
    }
    void *body() const {
        return nng_msg_body(_msg);
    }
    void clear() {
        nng_msg_clear(_msg);
    }

    int append(const void* data, size_t size){
        int ret = nng_msg_append(_msg, data, size);
        if(ret != 0){
            spd_err("nng_msg_append failed: {}", strerr(ret));
        }
        return ret;
    }
    int append_u16(uint16_t v){
        int ret = nng_msg_append_u16(_msg, v);
        if(ret != 0){
            spd_err("nng_msg_append_u16 failed: {}", strerr(ret));
        }
        return ret;
    }
    int append_u32(uint32_t v){
        int ret = nng_msg_append_u32(_msg, v);
        if(ret != 0){
            spd_err("nng_msg_append_u32 failed: {}", strerr(ret));
        }
        return ret;
    }
    int append_u64(uint64_t v){
        int ret = nng_msg_append_u64(_msg, v);
        if(ret != 0){
            spd_err("nng_msg_append_u64 failed: {}", strerr(ret));
        }
        return ret;
    }

    int insert(const void* data, size_t size){
        int ret = nng_msg_insert(_msg, data, size);
        if(ret != 0){
            spd_err("nng_msg_insert failed: {}", strerr(ret));
        }
        return ret;
    }
    int insert_u16(uint16_t v){
        int ret = nng_msg_insert_u16(_msg, v);
        if(ret != 0){
            spd_err("nng_msg_insert_u16 failed: {}", strerr(ret));
        }
        return ret;
    }
    int insert_u32(uint32_t v){
        int ret = nng_msg_insert_u32(_msg, v);
        if(ret != 0){   
            spd_err("nng_msg_insert_u32 failed: {}", strerr(ret));
        }
        return ret;
    }
    int insert_u64(uint64_t v){
        int ret = nng_msg_insert_u64(_msg, v);
        if(ret != 0){
            spd_err("nng_msg_insert_u64 failed: {}", strerr(ret));
        }
        return ret;
    }
    int trim(size_t size){
        int ret = nng_msg_trim(_msg, size);
        if(ret != 0){
            spd_err("nng_msg_trim failed: {}", strerr(ret));
        }
        return ret;
    }
    int trim_u16(uint16_t* v){
        int ret = nng_msg_trim_u16(_msg, v);
        if(ret != 0){
            spd_err("nng_msg_trim_u16 failed: {}", strerr(ret));
        }
        return ret;
    }
    int trim_u32(uint32_t* v){
        int ret = nng_msg_trim_u32(_msg, v);
        if(ret != 0){
            spd_err("nng_msg_trim_u32 failed: {}", strerr(ret));
        }
        return ret;
    }
    int trim_u64(uint64_t* v){
        int ret = nng_msg_trim_u64(_msg, v);
        if(ret != 0){
            spd_err("nng_msg_trim_u64 failed: {}", strerr(ret));
        }
        return ret;
    }
    int chop(size_t size){
        int ret = nng_msg_chop(_msg, size);
        if(ret != 0){
            spd_err("nng_msg_chop failed: {}", strerr(ret));
        }
        return ret;
    }
    int chop_u16(uint16_t* v){
        int ret = nng_msg_chop_u16(_msg, v);
        if(ret != 0){
            spd_err("nng_msg_chop_u16 failed: {}", strerr(ret));
        }
        return ret;
    }
    int chop_u32(uint32_t* v){
        int ret = nng_msg_chop_u32(_msg, v);
        if(ret != 0){
            spd_err("nng_msg_chop_u32 failed: {}", strerr(ret));
        }
        return ret;
    }
    int chop_u64(uint64_t* v){
        int ret = nng_msg_chop_u64(_msg, v);
        if(ret != 0){
            spd_err("nng_msg_chop_u64 failed: {}", strerr(ret));
        }
        return ret;
    }
    // Message Header reserved
    // Message Pipe reserved
public:
    bool _owned{true}; // 当send成功时，nng内部释放_msg失去所有权
    nng_msg* _msg{nullptr};
};
NSE_NNG