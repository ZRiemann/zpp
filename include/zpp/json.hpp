#pragma once
/**
 * @file zpp/json.hpp
 * @note
 *  A small RapidJSON wrapper with separate document, value and view
 *  specializations selected by the template argument.
 */

#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#ifndef RAPIDJSON_NOMEMBERITERATORCLASS
#define RAPIDJSON_NOMEMBERITERATORCLASS // disable struct std::iterator is deprecated [-Wdeprecated-declarations]
#endif

#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "namespace.h"
#include "error.h"

NSB_ZPP

template<class V = rapidjson::Document>
class json{
public:
    using value_type = V;
    using allocator_type = rapidjson::Document::AllocatorType;

    static constexpr bool is_document = std::is_same_v<V, rapidjson::Document>;
    static constexpr bool is_value = std::is_same_v<V, rapidjson::Value>;
    static constexpr bool is_view = std::is_same_v<V, rapidjson::Value*>;

    static_assert(is_document || is_value || is_view,
        "z::json only supports rapidjson::Document, rapidjson::Value and rapidjson::Value*.");

    class iterator{
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = json<rapidjson::Value*>;
        using reference = value_type;
        using pointer = void;

        iterator()
            :ptr_(nullptr)
            ,alloc_(nullptr){
        }

        iterator(rapidjson::Value *ptr, allocator_type *alloc)
            :ptr_(ptr)
            ,alloc_(alloc){
        }

        reference operator*() const{
            return value_type(ptr_, alloc_);
        }

        iterator &operator++(){
            if(nullptr != ptr_){
                ++ptr_;
            }
            return *this;
        }

        iterator operator++(int){
            iterator old(*this);
            ++(*this);
            return old;
        }

        friend bool operator==(const iterator &lhs, const iterator &rhs){
            return lhs.ptr_ == rhs.ptr_ && lhs.alloc_ == rhs.alloc_;
        }

        friend bool operator!=(const iterator &lhs, const iterator &rhs){
            return !(lhs == rhs);
        }

    private:
        rapidjson::Value *ptr_;
        allocator_type *alloc_;
    };

    json() requires (is_document)
        :v()
        ,alloc_(&v.GetAllocator()){
    }

    json() requires (is_value)
        :v()
        ,alloc_(nullptr){
    }

    json() requires (is_view)
        :v(nullptr)
        ,alloc_(nullptr){
    }

    template<class OwnerV>
    json(json<OwnerV> &owner) requires (is_value)
        :v()
        ,alloc_(&owner.allocator()){
    }

    json(rapidjson::Value *value, allocator_type *alloc) requires (is_view)
        :v(value)
        ,alloc_(alloc){
    }

    json(const json&) = delete;
    json& operator=(const json&) = delete;

    json(json &&other) noexcept
        :v(std::move(other.v))
        ,alloc_(other.alloc_){
        if constexpr(is_document){
            alloc_ = &v.GetAllocator();
        }else if constexpr(is_view){
            other.v = nullptr;
            other.alloc_ = nullptr;
        }else{
            other.alloc_ = nullptr;
        }
    }

    json& operator=(json&&) = delete;
    ~json() = default;

    bool valid() const{
        if constexpr(is_view){
            return nullptr != v && nullptr != alloc_;
        }else if constexpr(is_value){
            return nullptr != alloc_;
        }else{
            return true;
        }
    }

    err_t load_file(const std::string &fname){
        if constexpr(!is_document){
            return ERR_NOT_SUPPORT;
        }else{
            FILE *fp = nullptr;
#if ZSYS_WINDOWS
            fp = fopen(fname.c_str(), "rb");
#else
            fp = fopen(fname.c_str(), "r");
#endif
            if(!fp){
                return ERR_NOT_EXIST;
            }

            char readBuffer[4096];
            rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
            v.ParseStream(is);
            fclose(fp);
            return v.HasParseError() ? ERR_PARAM_INVALID : ERR_OK;
        }
    }

    err_t load_string(const char *str, size_t len = 0){
        if constexpr(!is_document){
            return ERR_NOT_SUPPORT;
        }else{
            if(nullptr == str){
                return ERR_PARAM_INVALID;
            }
            if(len){
                rapidjson::MemoryStream mem(str, len);
                v.ParseStream(mem);
            }else{
                v.Parse(str);
            }
            return v.HasParseError() ? ERR_PARAM_INVALID : ERR_OK;
        }
    }

    const char *to_string(rapidjson::StringBuffer &buffer, bool is_pretty = false) const{
        if(!valid()){
            return buffer.GetString();
        }
        if(is_pretty){
            rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
            value().Accept(writer);
        }else{
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            value().Accept(writer);
        }
        return buffer.GetString();
    }

    err_t set(const std::string &key, bool data){
        rapidjson::Value val;
        val.SetBool(data);
        return set_value(key, val);
    }

    err_t set(const std::string &key, int data){
        rapidjson::Value val;
        val.SetInt(data);
        return set_value(key, val);
    }

    err_t set(const std::string &key, int64_t data){
        rapidjson::Value val;
        val.SetInt64(data);
        return set_value(key, val);
    }

    err_t set(const std::string &key, double data){
        rapidjson::Value val;
        val.SetDouble(data);
        return set_value(key, val);
    }

    err_t set(const std::string &key, const char *data){
        return set(key, std::string_view(data ? data : ""));
    }

    err_t set(const std::string &key, const std::string &data){
        return set(key, std::string_view(data.data(), data.size()));
    }

    err_t set(const std::string &key, std::string_view data){
        if(!has_allocator()){
            return ERR_NOT_SUPPORT;
        }
        rapidjson::Value val;
        val.SetString(data.data(), (rapidjson::SizeType)data.size(), allocator());
        return set_value(key, val);
    }

    /**
     * @brief Set an object member from another json wrapper by deep-copying its value.
     *
     * This overload always performs a deep copy via RapidJSON `CopyFrom()`, even when
     * `data` refers to a temporary object. Use the rvalue overload when the source json
     * value is no longer needed and you want to avoid the extra copy when allocators match.
     *
     * @tparam JV Source json wrapper value type.
     * @param key Object member name to update or insert.
     * @param data Source json value to copy from.
     * @return ERR_OK on success, or ERR_NOT_SUPPORT when the destination has no allocator
     *         or the source json is invalid.
     */
    template<class JV>
    err_t set(const std::string &key, const json<JV> &data){
        if(!has_allocator() || !data.valid()){
            return ERR_NOT_SUPPORT;
        }
        rapidjson::Value copy;
        copy.CopyFrom(data.value(), allocator());
        return set_value(key, copy);
    }

    /**
     * @brief Set an object member from an rvalue json value, preferring move semantics.
     *
     * When the source and destination share the same allocator, this overload moves the
     * underlying RapidJSON value into the destination member without a deep copy. If the
     * allocators differ, it falls back to a deep copy to keep ownership safe.
     *
     * Prefer this overload over the const-reference overload when performance matters and
     * the source json value is no longer needed after the call.
     *
     * @param key Object member name to update or insert.
     * @param data Source json value to move from.
     * @return ERR_OK on success, or ERR_NOT_SUPPORT when the destination has no allocator
     *         or the source json is invalid.
     */
    err_t set(const std::string &key, json<rapidjson::Value> &&data){
        if(!has_allocator() || !data.valid()){
            return ERR_NOT_SUPPORT;
        }
        if(data.alloc_ == alloc_){
            return set_value(key, data.v.Move());
        }
        rapidjson::Value copy;
        copy.CopyFrom(data.v, allocator());
        return set_value(key, copy);
    }

    err_t get(const std::string &key, bool &out) const{
        const rapidjson::Value *val = find(key);
        if(nullptr == val || !val->IsBool()){
            return ERR_NOT_EXIST;
        }
        out = val->GetBool();
        return ERR_OK;
    }

    err_t get(const std::string &key, int &out) const{
        const rapidjson::Value *val = find(key);
        if(nullptr == val || !val->IsInt()){
            return ERR_NOT_EXIST;
        }
        out = val->GetInt();
        return ERR_OK;
    }

    err_t get(const std::string &key, int64_t &out) const{
        const rapidjson::Value *val = find(key);
        if(nullptr == val || !val->IsInt64()){
            return ERR_NOT_EXIST;
        }
        out = val->GetInt64();
        return ERR_OK;
    }

    err_t get(const std::string &key, double &out) const{
        const rapidjson::Value *val = find(key);
        if(nullptr == val || !val->IsNumber()){
            return ERR_NOT_EXIST;
        }
        out = val->GetDouble();
        return ERR_OK;
    }

    err_t get(const std::string &key, std::string &out) const{
        const rapidjson::Value *val = find(key);
        if(nullptr == val || !val->IsString()){
            return ERR_NOT_EXIST;
        }
        out.assign(val->GetString(), val->GetStringLength());
        return ERR_OK;
    }

    err_t get(bool &out) const{
        if(!valid() || !value().IsBool()){
            return ERR_NOT_EXIST;
        }
        out = value().GetBool();
        return ERR_OK;
    }

    err_t get(int &out) const{
        if(!valid() || !value().IsInt()){
            return ERR_NOT_EXIST;
        }
        out = value().GetInt();
        return ERR_OK;
    }

    err_t get(int64_t &out) const{
        if(!valid() || !value().IsInt64()){
            return ERR_NOT_EXIST;
        }
        out = value().GetInt64();
        return ERR_OK;
    }

    err_t get(double &out) const{
        if(!valid() || !value().IsNumber()){
            return ERR_NOT_EXIST;
        }
        out = value().GetDouble();
        return ERR_OK;
    }

    err_t get(std::string &out) const{
        if(!valid() || !value().IsString()){
            return ERR_NOT_EXIST;
        }
        out.assign(value().GetString(), value().GetStringLength());
        return ERR_OK;
    }

    json<rapidjson::Value*> member(const std::string &key){
        json<rapidjson::Value*> out;
        member(key, out);
        return out;
    }

    err_t member(const std::string &key, json<rapidjson::Value*> &out){
        if(!valid() || !value().IsObject()){
            return ERR_NOT_SUPPORT;
        }
        auto itr = value().FindMember(key.c_str());
        if(itr == value().MemberEnd()){
            return ERR_NOT_EXIST;
        }
        out.bind(itr->value, allocator());
        return ERR_OK;
    }

    json<rapidjson::Value*> at(size_t index){
        json<rapidjson::Value*> out;
        at(index, out);
        return out;
    }

    err_t at(size_t index, json<rapidjson::Value*> &out){
        if(!valid() || !value().IsArray()){
            return ERR_NOT_SUPPORT;
        }
        if(index >= value().Size()){
            return ERR_OUT_OF_RANGE;
        }
        out.bind(value()[(rapidjson::SizeType)index], allocator());
        return ERR_OK;
    }

    err_t push(bool data){
        rapidjson::Value val;
        val.SetBool(data);
        return push_value(val);
    }

    err_t push(int data){
        rapidjson::Value val;
        val.SetInt(data);
        return push_value(val);
    }

    err_t push(int64_t data){
        rapidjson::Value val;
        val.SetInt64(data);
        return push_value(val);
    }

    err_t push(double data){
        rapidjson::Value val;
        val.SetDouble(data);
        return push_value(val);
    }

    err_t push(const char *data){
        return push(std::string_view(data ? data : ""));
    }

    err_t push(const std::string &data){
        return push(std::string_view(data.data(), data.size()));
    }

    err_t push(std::string_view data){
        if(!has_allocator()){
            return ERR_NOT_SUPPORT;
        }
        rapidjson::Value val;
        val.SetString(data.data(), (rapidjson::SizeType)data.size(), allocator());
        return push_value(val);
    }

    /**
     * @brief Append another json wrapper value to the array by deep-copying it.
     *
     * This overload always performs a deep copy via RapidJSON `CopyFrom()`, even when
     * `data` refers to a temporary object. Use the rvalue overload when the source json
     * value is no longer needed and you want to avoid the extra copy when allocators match.
     *
     * @tparam JV Source json wrapper value type.
     * @param data Source json value to copy from.
     * @return ERR_OK on success, or ERR_NOT_SUPPORT when the destination has no allocator
     *         or the source json is invalid.
     */
    template<class JV>
    err_t push(const json<JV> &data){
        if(!has_allocator() || !data.valid()){
            return ERR_NOT_SUPPORT;
        }
        rapidjson::Value copy;
        copy.CopyFrom(data.value(), allocator());
        return push_value(copy);
    }

    /**
     * @brief Append an rvalue json value to the array, preferring move semantics.
     *
     * When the source and destination share the same allocator, this overload moves the
     * underlying RapidJSON value into the array without a deep copy. If the allocators
     * differ, it falls back to a deep copy to keep ownership safe.
     *
     * Prefer this overload over the const-reference overload when performance matters and
     * the source json value is no longer needed after the call.
     *
     * @param data Source json value to move from.
     * @return ERR_OK on success, or ERR_NOT_SUPPORT when the destination has no allocator
     *         or the source json is invalid.
     */
    err_t push(json<rapidjson::Value> &&data){
        if(!has_allocator() || !data.valid()){
            return ERR_NOT_SUPPORT;
        }
        if(data.alloc_ == alloc_){
            return push_value(data.v.Move());
        }
        rapidjson::Value copy;
        copy.CopyFrom(data.v, allocator());
        return push_value(copy);
    }

    err_t remove(const std::string &key){
        if(!valid() || !value().IsObject()){
            return ERR_NOT_SUPPORT;
        }
        auto itr = value().FindMember(key.c_str());
        if(itr == value().MemberEnd()){
            return ERR_NOT_EXIST;
        }
        value().EraseMember(itr);
        return ERR_OK;
    }

    err_t remove(size_t index){
        if(!valid() || !value().IsArray()){
            return ERR_NOT_SUPPORT;
        }
        if(index >= value().Size()){
            return ERR_OUT_OF_RANGE;
        }
        value().Erase(value().Begin() + (rapidjson::SizeType)index);
        return ERR_OK;
    }

    size_t size() const{
        if(!valid()){
            return 0;
        }
        if(value().IsArray()){
            return (size_t)value().Size();
        }
        if(value().IsObject()){
            return (size_t)value().MemberCount();
        }
        return 0;
    }

    bool empty() const{
        return 0 == size();
    }

    iterator begin(){
        if(!valid() || !has_allocator() || !value().IsArray()){
            return iterator();
        }
        return iterator(value().Begin(), &allocator());
    }

    iterator end(){
        if(!valid() || !has_allocator() || !value().IsArray()){
            return iterator();
        }
        return iterator(value().End(), &allocator());
    }

    iterator begin_array(){
        return begin();
    }

    iterator end_array(){
        return end();
    }

    bool is_null() const{
        return valid() && value().IsNull();
    }

    bool is_object() const{
        return valid() && value().IsObject();
    }

    bool is_array() const{
        return valid() && value().IsArray();
    }

    bool is_string() const{
        return valid() && value().IsString();
    }

    bool is_bool() const{
        return valid() && value().IsBool();
    }

    bool is_int() const{
        return valid() && value().IsInt();
    }

    bool is_int64() const{
        return valid() && value().IsInt64();
    }

    bool is_double() const{
        return valid() && value().IsDouble();
    }

    bool is_number() const{
        return valid() && value().IsNumber();
    }

    rapidjson::Value &raw(){
        return value();
    }

    const rapidjson::Value &raw() const{
        return value();
    }

    allocator_type &allocator(){
        if constexpr(is_document){
            return v.GetAllocator();
        }else{
            return *alloc_;
        }
    }

    const allocator_type &allocator() const{
        if constexpr(is_document){
            return v.GetAllocator();
        }else{
            return *alloc_;
        }
    }

    V v;

private:
    template<class>
    friend class json;

    bool has_allocator() const{
        if constexpr(is_document){
            return true;
        }else{
            return nullptr != alloc_;
        }
    }

    rapidjson::Value &value(){
        if constexpr(is_view){
            return *v;
        }else{
            return v;
        }
    }

    const rapidjson::Value &value() const{
        if constexpr(is_view){
            return *v;
        }else{
            return v;
        }
    }

    void bind(rapidjson::Value &value, allocator_type &alloc) requires (is_view){
        v = &value;
        alloc_ = &alloc;
    }

    err_t ensure_object(){
        if(!valid()){
            return ERR_NOT_SUPPORT;
        }
        rapidjson::Value &self = value();
        if(self.IsObject()){
            return ERR_OK;
        }
        if(self.IsNull()){
            self.SetObject();
            return ERR_OK;
        }
        return ERR_NOT_SUPPORT;
    }

    err_t ensure_array(){
        if(!valid()){
            return ERR_NOT_SUPPORT;
        }
        rapidjson::Value &self = value();
        if(self.IsArray()){
            return ERR_OK;
        }
        if(self.IsNull()){
            self.SetArray();
            return ERR_OK;
        }
        return ERR_NOT_SUPPORT;
    }

    const rapidjson::Value *find(const std::string &key) const{
        if(!valid() || !value().IsObject()){
            return nullptr;
        }
        auto itr = value().FindMember(key.c_str());
        if(itr == value().MemberEnd()){
            return nullptr;
        }
        return &itr->value;
    }

    err_t set_value(const std::string &key, rapidjson::Value &data){
        if(!has_allocator()){
            return ERR_NOT_SUPPORT;
        }
        err_t err = ensure_object();
        if(ERR_OK != err){
            return err;
        }
        auto itr = value().FindMember(key.c_str());
        if(itr != value().MemberEnd()){
            itr->value = data.Move();
        }else{
            value().AddMember(
                rapidjson::Value(key.c_str(), (rapidjson::SizeType)key.size(), allocator()).Move(),
                data.Move(),
                allocator());
        }
        return ERR_OK;
    }

    err_t push_value(rapidjson::Value &data){
        if(!has_allocator()){
            return ERR_NOT_SUPPORT;
        }
        err_t err = ensure_array();
        if(ERR_OK != err){
            return err;
        }
        value().PushBack(data.Move(), allocator());
        return ERR_OK;
    }

    allocator_type *alloc_;
};

template<class OwnerV>
json(json<OwnerV>&) -> json<rapidjson::Value>;

using json_doc = json<rapidjson::Document>;
using json_val = json<rapidjson::Value>;
using json_view = json<rapidjson::Value*>;

NSE_ZPP
