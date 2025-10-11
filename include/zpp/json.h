#pragma once
/**
 * @file zpp/json.h
 * @note
 *  简化并安全的使用rapidjson，提高开发效率。使用方法参考 @see tests/uni_svr/test_json.cpp
 */
#include <string>

#ifndef RAPIDJSON_NOMEMBERITERATORCLASS
#define RAPIDJSON_NOMEMBERITERATORCLASS // disable struct std::iterator’ is deprecated [-Wdeprecated-declarations]
#endif

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/prettywriter.h>

#include "namespace.h"
#include "error.h"

NSB_ZPP

class json{
public:
    json(){
        doc = new rapidjson::Document;
        alloc = &(doc->GetAllocator());
        pv = doc;
    }

    json(json &j)
        :doc(nullptr)
        ,alloc(j.alloc){
        pv = &v;
    }

    ~json(){
        if(doc != nullptr){
            delete doc;
        }
    }

    err_t load_file(const std::string &fname){
        err_t err = ERR_PARAM_INVALID;
        FILE* fp = NULL;
        do{
            if(nullptr == doc){
                break;
            }
#if ZSYS_WINDOWS
            fp = fopen(fname.c_str(), "rb");
#else
            fp = fopen(fname.c_str(), "r");
#endif
            if(!fp){
                err = ERR_NOT_EXIST;
                break;
            }
            char readBuffer[4096];
            rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
            doc->ParseStream(is);
            err = doc->IsObject() ? ERR_OK : ERR_PARAM_INVALID;
        }while(0);
        if(fp){
            fclose(fp);
        }
        return err;
    }

    err_t load_string(const char *str, size_t len = 0){
        if(nullptr == doc){
            return ERR_NOT_SUPPORT;
        }
        if(len){
            rapidjson::MemoryStream mem(str, len);
            doc->ParseStream(mem);
        }else{
            doc->Parse(str);
        }
        return  doc->IsObject() ? ERR_OK : ERR_PARAM_INVALID;
    }

    const char *to_string(rapidjson::StringBuffer &buffer, bool is_pretty = false){
        if(is_pretty){
            rapidjson::PrettyWriter<rapidjson::StringBuffer>writer(buffer);
            pv->Accept(writer);
        }else{
            rapidjson::Writer<rapidjson::StringBuffer>writer(buffer);
            pv->Accept(writer);
        }
        return buffer.GetString();
    }

    err_t set_bool(const std::string& key, bool value){
        if(!pv->IsObject()){
            pv->SetObject();
        }
        auto itr = pv->FindMember(key.c_str());
        if (itr != pv->MemberEnd()){
            itr->value.SetBool(value);
        }else{
            pv->AddMember(rapidjson::Value(key.c_str(), *alloc).Move(), value, *alloc);
        }
        return ERR_OK;
    }

    err_t get_bool(const std::string& key, bool &value){
        rapidjson::Value::ConstMemberIterator itr = pv->FindMember(key.c_str());
        if (itr != pv->MemberEnd() && itr->value.IsBool()){
            value = itr->value.GetBool();
            return ERR_OK;
        }

        return ERR_NOT_EXIST;
    }

    err_t set_int(const std::string& key, int value){
        if(!pv->IsObject()){
            pv->SetObject();
        }
        auto itr = pv->FindMember(key.c_str());
        if (itr != pv->MemberEnd()){
            itr->value.SetInt(value);
        }else{
            pv->AddMember(rapidjson::Value(key.c_str(), *alloc).Move(), value, *alloc);
        }
        return ERR_OK;
    }

    err_t get_int(const std::string& key, int &value){
        if(!pv->IsObject()){
            return ERR_NOT_EXIST;
        }
        rapidjson::Value::ConstMemberIterator itr = pv->FindMember(key.c_str());
        if (itr != pv->MemberEnd() && itr->value.IsInt()){
            value = itr->value.GetInt();
            return ERR_OK;
        }

        return ERR_NOT_EXIST;
    }

    err_t set_string(const std::string &key, const std::string &value){
        if(!pv->IsObject() && !pv->IsArray()){
            pv->SetObject();
        }
        auto itr = pv->FindMember(key.c_str());
        if (itr != pv->MemberEnd()){
            //pv->EraseMember(itr);
            itr->value.SetString(value.c_str(), value.length(), *alloc);
        }else{
            pv->AddMember(rapidjson::Value(key.c_str(), *alloc).Move(),
                rapidjson::Value(value.c_str(), value.length(), *alloc).Move(),
                *alloc);
        }
        return ERR_OK;
    }

    err_t get_string(const std::string &key, std::string &str){
        if(!pv->IsObject()){
            return ERR_NOT_EXIST;
        }
        rapidjson::Value::ConstMemberIterator itr = pv->FindMember(key.c_str());
        if (itr != pv->MemberEnd() && itr->value.IsString()){
            str.assign(itr->value.GetString(), itr->value.GetStringLength());
            return ERR_OK;
        }
        return ERR_NOT_EXIST;
    }

    std::string &get_string(std::string &self){
        return self.assign(pv->GetString());
    }

    err_t set_member(const std::string &key, json &value){
        if(value.v.IsNull()){
            return ERR_NOT_EXIST;
        }
        if(!pv->IsObject()){
            pv->SetObject();
        }
        auto itr = pv->FindMember(key.c_str());
        if (itr != pv->MemberEnd()){
            itr->value.Swap(value.v);
        }else{
            pv->AddMember(rapidjson::Value(key.c_str(), *alloc).Move(),
                value.pv->Move(), *alloc);
        }
        return ERR_OK;
    }

    err_t get_member(const std::string &key, json &value){
        if(!pv->IsObject()){
            return ERR_NOT_SUPPORT;
        }
        rapidjson::Value::MemberIterator itr = pv->FindMember(key.c_str());
        if (itr != pv->MemberEnd()){
            value.pv = &(itr->value);
            return ERR_OK;
        }
        return ERR_NOT_EXIST;
    }

    template<typename T> err_t push_back(T t){
        if(!pv->IsArray()){
            if(pv->IsNull()){
                pv->SetArray();
            }else{
                return ERR_NOT_SUPPORT;
            }
        }
        pv->PushBack(t, *alloc);
        return ERR_OK;
    }

    err_t push_json(json &js){
        if(!pv->IsArray()){
            if(pv->IsNull()){
                pv->SetArray();
            }else{
                return ERR_NOT_SUPPORT;
            }
        }
        pv->PushBack((rapidjson::Value&)*(js.pv), *alloc);
        return ERR_OK;
    }

    err_t push_string(const std::string &str){
        if(!pv->IsArray()){
            if(pv->IsNull()){
                pv->SetArray();
            }else{
                return ERR_NOT_SUPPORT;
            }
        }
        pv->PushBack(rapidjson::Value(str.c_str(), *alloc).Move(), *alloc);
        return ERR_OK;
    }

    size_t array_size(){
        if(!pv->IsArray()){
            return 0;
        }
        return (size_t)pv->Size();
    }
    err_t at(json &js, size_t pos){
        if(!pv->IsArray()){
            return ERR_NOT_SUPPORT;
        }
        js.pv = &(*pv)[pos];
        return ERR_OK;
    }

    bool is_int(){
        return pv->IsInt();
    }

    int get_int(){
        return pv->GetInt();
    }

    bool is_object(){
        return pv->IsObject();
    }

    bool is_float(){
        return pv->IsFloat();
    }

    float get_float(){
        return pv->GetFloat();
    }
public:
    rapidjson::Document *doc;
    rapidjson::Document::AllocatorType *alloc;
    rapidjson::Value v;
    rapidjson::Value *pv;
};

NSE_ZPP

