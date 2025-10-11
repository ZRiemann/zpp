#include <zpp/namespace.h>
#include <zpp/json.h>
#include "tests.h"
#include <zpp/spdlog.h>
#include <fstream>
#include <string>

USE_ZPP

static void load_json_file(const char *str, size_t size){
    std::string fname("test.json");
    std::ofstream ofs(fname, std::ios::trunc);
    ofs.write(str, size);
    ofs.close();

    json doc;
    doc.load_file(fname);

    rapidjson::StringBuffer sbuf;
    spd_inf("file object:{}", doc.to_string(sbuf, true));
}

/**
 * @brief 从字符串加载json对象
 */
static void load_json_string(const char *str){
    json doc;
    doc.load_string(str);

    // 输出对象
    rapidjson::StringBuffer sbuf;
    spd_inf("load object:{}", doc.to_string(sbuf, true));
}

/**
 * @brief z::json 封装 Rapidjson 测试
 */
void z::test_json(){
    spd_inf("testing class z::json...");

    json doc; // json Document 对象, 有独立的分配器

    doc.set_int("int_value", 3); //{"int_value": 3}
    int i{0};
    doc.get_int("int_value", i);
    spd_inf("int_value: {}", i);

    doc.set_string("hello", "world"); //{"int_value": 3, "hello": "world"}
    std::string hello{"abc"};
    doc.get_string("hello", hello);
    spd_inf("hello: {}", hello);

    json member(doc); // Document 对象内部的子对象，使用 Docment 的分配器。
    member.set_bool("bool", true);
    doc.set_member("member", member);

    // int 数组
    json array(doc);
    array.push_back(1);
    array.push_back(2);
    array.push_back(3);
    doc.set_member("int_array", array);
    json arr_i(doc);
    doc.get_member("int_array", arr_i);
    for(size_t i = 0; i < arr_i.array_size(); ++i){
        json v(doc);
        arr_i.at(v, i);
        spd_inf("arr_int[{}]: {}", i, v.get_int());
    }

    // 字符串数组
    json str_array(doc);
    str_array.push_string("hello");
    str_array.push_string("world");
    doc.set_member("str_array", str_array);
    json arr_str(doc);
    doc.get_member("str_array", arr_str);
    for(size_t i = 0; i < arr_str.array_size(); ++i){
        json v(doc);
        arr_str.at(v, i);
        std::string str_value;
        spd_inf("arr_str[{}]: {}", i, v.get_string(str_value));
    }

    // 对象数组
    json obj_array(doc);
    json obj1(doc);
    obj1.set_int("int", 1);
    obj_array.push_json(obj1);
    json obj2(doc);
    obj2.set_string("str", "str_value");
    obj_array.push_json(obj2);
    doc.set_member("obj_array", obj_array);
    json arr_obj(doc);
    doc.get_member("obj_array", arr_obj);
    for(size_t i = 0; i < arr_str.array_size(); ++i){
        json v(doc);
        arr_obj.at(v, i);
        rapidjson::StringBuffer buf;
        spd_inf("arr_obj[{}]: {}", i, v.to_string(buf));
    }

    // 输出对象
    rapidjson::StringBuffer sbuf;
    spd_inf("{}", doc.to_string(sbuf, true));

    load_json_string(sbuf.GetString());
    load_json_file(sbuf.GetString(), sbuf.GetSize());
}