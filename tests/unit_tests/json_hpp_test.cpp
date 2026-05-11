#include <gtest/gtest.h>

#include <fstream>
#include <string>

#include <zpp/json.hpp>

namespace {

TEST(JsonHppTest, ParseReadAndSerialize) {
    z::json conf;
    ASSERT_EQ(z::ERR_OK, conf.load_string(
        "{\"server\":{\"spd\":{\"max_size\":32}},\"fronts\":[\"tcp://a\",\"tcp://b\"]}"));

    auto spd = conf.member("server").member("spd");
    int max_size = 0;
    ASSERT_EQ(z::ERR_OK, spd.get("max_size", max_size));
    EXPECT_EQ(32, max_size);

    auto fronts = conf.member("fronts");
    EXPECT_TRUE(fronts.is_array());
    EXPECT_EQ(2u, fronts.size());

    std::string front;
    ASSERT_EQ(z::ERR_OK, fronts.at(1).get(front));
    EXPECT_EQ("tcp://b", front);

    rapidjson::StringBuffer buffer;
    EXPECT_NE(nullptr, conf.to_string(buffer, true));
    EXPECT_GT(buffer.GetSize(), 0u);
}

TEST(JsonHppTest, BuildObjectArrayAndNestedValues) {
    z::json doc;

    z::json obj(doc);
    ASSERT_EQ(z::ERR_OK, obj.set("name", "abc"));
    ASSERT_EQ(z::ERR_OK, obj.set("enabled", true));
    ASSERT_EQ(z::ERR_OK, obj.set("count", 7));

    z::json arr(doc);
    ASSERT_EQ(z::ERR_OK, arr.push(1));
    ASSERT_EQ(z::ERR_OK, arr.push("x"));
    ASSERT_EQ(z::ERR_OK, arr.push(obj)); // Deep copy obj into arr

    ASSERT_EQ(z::ERR_OK, doc.set("obj", obj)); // obj is still valid after being copied into arr
    ASSERT_EQ(z::ERR_OK, arr.push(std::move(obj))); // Move obj into arr, obj becomes invalid
    ASSERT_EQ(z::ERR_OK, doc.set("arr", arr));

    std::string name;
    ASSERT_EQ(z::ERR_OK, doc.member("obj").get("name", name));
    EXPECT_EQ("abc", name);

    bool enabled = false;
    ASSERT_EQ(z::ERR_OK, doc.member("obj").get("enabled", enabled));
    EXPECT_TRUE(enabled);

    EXPECT_EQ(4u, doc.member("arr").size());
    ASSERT_EQ(z::ERR_OK, doc.member("arr").at(2).get("name", name));
    EXPECT_EQ("abc", name);
    ASSERT_EQ(z::ERR_OK, doc.member("arr").at(3).get("name", name));
    EXPECT_EQ("abc", name);
}

TEST(JsonHppTest, ViewErrorsAndMoveInsert) {
    z::json doc;
    z::json_view view;
    int value = 0;

    EXPECT_FALSE(view.valid());
    EXPECT_EQ(z::ERR_NOT_EXIST, view.get("missing", value));

    z::json arr(doc);
    ASSERT_EQ(z::ERR_OK, arr.push(1));
    EXPECT_EQ(z::ERR_OUT_OF_RANGE, arr.at(3, view));
    EXPECT_FALSE(view.valid());

    ASSERT_EQ(z::ERR_OK, arr.at(0, view));
    EXPECT_TRUE(view.valid());
    ASSERT_EQ(z::ERR_OK, view.get(value));
    EXPECT_EQ(1, value);

    z::json moved(doc);
    ASSERT_EQ(z::ERR_OK, moved.set("n", 42));
    ASSERT_EQ(z::ERR_OK, doc.set("moved", std::move(moved)));
    ASSERT_EQ(z::ERR_OK, doc.member("moved").get("n", value));
    EXPECT_EQ(42, value);
}

TEST(JsonHppTest, IteratesArraysWithRangeFor) {
    z::json doc;

    z::json numbers(doc);
    ASSERT_EQ(z::ERR_OK, numbers.push(1));
    ASSERT_EQ(z::ERR_OK, numbers.push(2));
    ASSERT_EQ(z::ERR_OK, numbers.push(3));
    ASSERT_EQ(z::ERR_OK, doc.set("numbers", numbers));

    int sum = 0;
    for(auto item : doc.member("numbers")){
        int value = 0;
        ASSERT_EQ(z::ERR_OK, item.get(value));
        sum += value;
    }
    EXPECT_EQ(6, sum);
    EXPECT_EQ(3u, doc.member("numbers").size());
    EXPECT_FALSE(doc.member("numbers").empty());

    z::json items(doc);
    for(int i = 0; i < 3; ++i){
        z::json item(doc);
        ASSERT_EQ(z::ERR_OK, item.set("id", i));
        ASSERT_EQ(z::ERR_OK, items.push(item));
    }
    ASSERT_EQ(z::ERR_OK, doc.set("items", items));

    int count = 0;
    for(auto item : doc.member("items")){
        int id = -1;
        ASSERT_EQ(z::ERR_OK, item.get("id", id));
        EXPECT_EQ(count, id);
        ASSERT_EQ(z::ERR_OK, item.set("seen", true));
        ++count;
    }
    EXPECT_EQ(3, count);

    for(auto item : doc.member("items")){
        bool seen = false;
        ASSERT_EQ(z::ERR_OK, item.get("seen", seen));
        EXPECT_TRUE(seen);
    }

    auto missing = doc.member("missing");
    EXPECT_EQ(missing.begin(), missing.end());
    EXPECT_TRUE(missing.empty());
    count = 0;
    for(auto item : missing){
        (void)item;
        ++count;
    }
    EXPECT_EQ(0, count);

    auto non_array = doc.member("items").at(0);
    EXPECT_EQ(non_array.begin_array(), non_array.end_array());
}

TEST(JsonHppTest, RemovesObjectMembersAndArrayItems) {
    z::json doc;

    ASSERT_EQ(z::ERR_OK, doc.set("name", "abc"));
    ASSERT_EQ(z::ERR_OK, doc.set("count", 2));
    EXPECT_EQ(2u, doc.size());
    EXPECT_EQ(z::ERR_OK, doc.remove("count"));
    EXPECT_EQ(1u, doc.size());

    int count = 0;
    EXPECT_EQ(z::ERR_NOT_EXIST, doc.get("count", count));
    EXPECT_EQ(z::ERR_NOT_EXIST, doc.remove("missing"));

    z::json arr(doc);
    ASSERT_EQ(z::ERR_OK, arr.push(1));
    ASSERT_EQ(z::ERR_OK, arr.push(2));
    ASSERT_EQ(z::ERR_OK, arr.push(3));
    ASSERT_EQ(z::ERR_OK, doc.set("numbers", arr));

    auto numbers = doc.member("numbers");
    ASSERT_EQ(z::ERR_OK, numbers.remove(1));
    EXPECT_EQ(2u, numbers.size());

    int value = 0;
    ASSERT_EQ(z::ERR_OK, numbers.at(0).get(value));
    EXPECT_EQ(1, value);
    ASSERT_EQ(z::ERR_OK, numbers.at(1).get(value));
    EXPECT_EQ(3, value);
    EXPECT_EQ(z::ERR_OUT_OF_RANGE, numbers.remove(2));
    EXPECT_EQ(z::ERR_NOT_SUPPORT, doc.remove(0));
    EXPECT_EQ(z::ERR_NOT_SUPPORT, numbers.remove("name"));
}

TEST(JsonHppTest, LoadFileSupportsNonObjectJson) {
    const char *path = "/tmp/zpp_json_hpp_test.json";
    {
        std::ofstream ofs(path, std::ios::trunc);
        ofs << "[1,2,3]";
    }

    z::json doc;
    ASSERT_EQ(z::ERR_OK, doc.load_file(path));
    EXPECT_TRUE(doc.is_array());
    EXPECT_EQ(3u, doc.size());
}

} // namespace
