// Stage 1 check: JSON + Logger + your existing concurrency engine.
#include <iostream>
#include <memory>
#include <string>
#include "concurrency/Scheduler.h"
#include "concurrency/TaskQueue.h"
#include "concurrency/ThreadPool.h"
#include "utils/Json.h"
#include "utils/Logger.h"

using namespace uniconnect;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::cout << "  FAIL at line " << __LINE__ << "\n"; ++failures; } } while (0)

int main() {
    auto& log = utils::Logger::getInstance();
    log.info("Stage1", "Testing JSON...");

    // build + serialize
    auto o = utils::JsonValue::object();
    o["name"] = "Rahul \"R\" Sharma";
    o["id"] = 1;
    o["ok"] = true;
    o["gpa"] = 8.5;
    o["big"] = static_cast<size_t>(42);
    auto arr = utils::JsonValue::array();
    arr.push_back(1); arr.push_back("two");
    o["items"] = arr;
    std::string s = o.serialize(false);
    std::cout << "  " << s << "\n";
    const std::string expected =
        "{\"name\":\"Rahul \\\"R\\\" Sharma\",\"id\":1,\"ok\":true,\"gpa\":8.5,\"big\":42,\"items\":[1,\"two\"]}";
    CHECK(s == expected);

    // parse (what the HTTP layer will do with request bodies)
    utils::JsonValue p;
    const std::string input =
        "{\"email\":\"a@b.com\",\"section_id\":3,\"n\":\"7\",\"t\":\"caf\\u00e9 \\ud83d\\ude00\"}";
    const bool parsedOk = utils::JsonValue::tryParse(input, p);
    CHECK(parsedOk);
    CHECK(p["email"].asString() == "a@b.com");
    CHECK(p["section_id"].asString() == "3");     // number read as string (ids)
    CHECK(p["n"].asInt() == 7);                   // string read as int
    CHECK(p["missing"].asString("dflt") == "dflt");
    CHECK(p["priority"].asInt(5) == 5);
    CHECK(p.contains("email") && !p.contains("nope"));
    const std::string wantUtf8 = "caf\xc3\xa9 \xf0\x9f\x98\x80";
    const std::string gotUtf8 = p["t"].asString();
    CHECK(gotUtf8 == wantUtf8);

    // bad input must fail cleanly
    utils::JsonValue bad;
    const bool bad1 = utils::JsonValue::tryParse("{\"a\":", bad);
    const bool bad2 = utils::JsonValue::tryParse("{\"a\":1} trailing", bad);
    const bool bad3 = utils::JsonValue::tryParse("", bad);
    CHECK(!bad1);
    CHECK(!bad2);
    CHECK(!bad3);

    // round trip
    utils::JsonValue rt;
    const bool rtOk = utils::JsonValue::tryParse(o.serialize(true), rt);
    CHECK(rtOk);
    CHECK(rt.serialize() == s);

    log.info("Stage1", "Testing your concurrency engine...");
    auto scheduler = std::make_shared<concurrency::Scheduler>(concurrency::SchedulingPolicy::ROUND_ROBIN, 25);
    auto queue = std::make_shared<concurrency::TaskQueue>(1000);
    auto pool = std::make_shared<concurrency::ThreadPool>(4, scheduler, queue);
    pool->start();
    auto metrics = pool->getMetricsJson();
    std::cout << metrics.serialize(true) << "\n";
    CHECK(metrics["num_workers"].asInt() == 4);
    pool->stop();

    if (failures == 0) std::cout << "\nSTAGE 1 OK\n";
    else std::cout << "\nSTAGE 1 FAILED (" << failures << ")\n";
    return failures == 0 ? 0 : 1;
}
