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
        int ret = nng_msg_alloc(&msg_, size);
        if(ret != 0){
            spd_err("nng_msg_alloc failed: {}", strerr(ret));
        }
        //spd_inf("msg_ allocated: {}", (void*)msg_);
    }
    ~message(){
        if(owned_ && msg_){
            //spd_inf("msg_{} freed.", (void*)msg_);
            nng_msg_free(msg_);
            //msg_ = nullptr;
        }
    }
    message(const message& duplicate){
        owned_ = true;
        int ret = nng_msg_dup(&msg_, duplicate.msg_);
        if(ret != 0){
            spd_err("nng_msg_dup failed: {}", strerr(ret));
        }
    }

public:
    size_t capacity() const {
        return nng_msg_capacity(msg_);
    }
    int reallocate(size_t size) {
        int ret = nng_msg_realloc(msg_, size);
        if(ret != 0){
            spd_err("nng_msg_realloc failed: {}", strerr(ret));
        }
        return ret;
    }
    int reserve(size_t size) {
        int ret = nng_msg_reserve(msg_, size);
        if(ret != 0){
            spd_err("nng_msg_reserve failed: {}", strerr(ret));
        }
        return ret;
    }
    size_t length() const {
        return nng_msg_len(msg_);
    }
    void *body() const {
        return nng_msg_body(msg_);
    }
    void clear() {
        nng_msg_clear(msg_);
    }

    int append(const void* data, size_t size){
        int ret = nng_msg_append(msg_, data, size);
        if(ret != 0){
            spd_err("nng_msg_append failed: {}", strerr(ret));
        }
        return ret;
    }
    int append_u16(uint16_t v){
        int ret = nng_msg_append_u16(msg_, v);
        if(ret != 0){
            spd_err("nng_msg_append_u16 failed: {}", strerr(ret));
        }
        return ret;
    }
    int append_u32(uint32_t v){
        int ret = nng_msg_append_u32(msg_, v);
        if(ret != 0){
            spd_err("nng_msg_append_u32 failed: {}", strerr(ret));
        }
        return ret;
    }
    int append_u64(uint64_t v){
        int ret = nng_msg_append_u64(msg_, v);
        if(ret != 0){
            spd_err("nng_msg_append_u64 failed: {}", strerr(ret));
        }
        return ret;
    }

    int insert(const void* data, size_t size){
        int ret = nng_msg_insert(msg_, data, size);
        if(ret != 0){
            spd_err("nng_msg_insert failed: {}", strerr(ret));
        }
        return ret;
    }
    int insert_u16(uint16_t v){
        int ret = nng_msg_insert_u16(msg_, v);
        if(ret != 0){
            spd_err("nng_msg_insert_u16 failed: {}", strerr(ret));
        }
        return ret;
    }
    int insert_u32(uint32_t v){
        int ret = nng_msg_insert_u32(msg_, v);
        if(ret != 0){   
            spd_err("nng_msg_insert_u32 failed: {}", strerr(ret));
        }
        return ret;
    }
    int insert_u64(uint64_t v){
        int ret = nng_msg_insert_u64(msg_, v);
        if(ret != 0){
            spd_err("nng_msg_insert_u64 failed: {}", strerr(ret));
        }
        return ret;
    }
    int trim(size_t size){
        int ret = nng_msg_trim(msg_, size);
        if(ret != 0){
            spd_err("nng_msg_trim failed: {}", strerr(ret));
        }
        return ret;
    }
    int trim_u16(uint16_t* v){
        int ret = nng_msg_trim_u16(msg_, v);
        if(ret != 0){
            spd_err("nng_msg_trim_u16 failed: {}", strerr(ret));
        }
        return ret;
    }
    int trim_u32(uint32_t* v){
        int ret = nng_msg_trim_u32(msg_, v);
        if(ret != 0){
            spd_err("nng_msg_trim_u32 failed: {}", strerr(ret));
        }
        return ret;
    }
    int trim_u64(uint64_t* v){
        int ret = nng_msg_trim_u64(msg_, v);
        if(ret != 0){
            spd_err("nng_msg_trim_u64 failed: {}", strerr(ret));
        }
        return ret;
    }
    int chop(size_t size){
        int ret = nng_msg_chop(msg_, size);
        if(ret != 0){
            spd_err("nng_msg_chop failed: {}", strerr(ret));
        }
        return ret;
    }
    int chop_u16(uint16_t* v){
        int ret = nng_msg_chop_u16(msg_, v);
        if(ret != 0){
            spd_err("nng_msg_chop_u16 failed: {}", strerr(ret));
        }
        return ret;
    }
    int chop_u32(uint32_t* v){
        int ret = nng_msg_chop_u32(msg_, v);
        if(ret != 0){
            spd_err("nng_msg_chop_u32 failed: {}", strerr(ret));
        }
        return ret;
    }
    int chop_u64(uint64_t* v){
        int ret = nng_msg_chop_u64(msg_, v);
        if(ret != 0){
            spd_err("nng_msg_chop_u64 failed: {}", strerr(ret));
        }
        return ret;
    }
    // Message Header reserved
    // Message Pipe reserved
public:
    bool owned_{true}; // 当send成功时，nng内部释放msg_失去所有权
    nng_msg* msg_{nullptr};
};
NSE_NNG