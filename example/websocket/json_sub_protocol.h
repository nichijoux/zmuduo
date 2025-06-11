#pragma once
#include <zmuduo/net/http/ws/ws_frame.h>

#include "json.hpp"

using JSON = nlohmann::json;

struct Person {
    std::string name;
    std::string address;
    int         age;
};

void to_json(JSON& j, const Person& p) {
    j = JSON{{"name", p.name}, {"address", p.address}, {"age", p.age}};
}

void from_json(const JSON& j, Person& p) {
    j.at("name").get_to(p.name);
    j.at("address").get_to(p.address);
    j.at("age").get_to(p.age);
}

// 定义一个json子协议
class JsonWSSubProtocol : public zmuduo::net::http::ws::WSSubProtocol {
    std::string getName() const override {
        return "json";
    }

    std::any process(const std::string& payload) override {
        auto json = nlohmann::json::parse(payload);
        return json;
    }
};