#include "rendering/diagnostics/tracing/CpuTrace.h"
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <set>
#include <thread>

using rendering::CpuTrace;
namespace {
const auto tracePath = std::filesystem::path(PLANET_CPU_TRACE_TEST);
nlohmann::json readTrace() { return nlohmann::json::parse(std::ifstream(tracePath)); }
}

TEST(CpuTrace, NestedScopesWorkerBindingAndUnwindingProduceValidJson) {
    {
        CpuTrace trace(tracePath.string());
        CpuTrace::Thread thread(&trace, "main \"thread\"");
        CpuTrace::Scope parent("parent");
        { CpuTrace::Scope child("child"); child.stop(); child.stop(); }
        std::thread worker([&] {
            CpuTrace::Thread binding(&trace, "worker");
            CpuTrace::Scope scope("worker.call");
        });
        worker.join();
        try { CpuTrace::Scope scope("throws"); throw 1; } catch (int) {}
    }
    const auto json = readTrace();
    std::set<unsigned> threads;
    nlohmann::json parent, child;
    int count = 0;
    for (const auto& event : json.at("traceEvents")) {
        threads.insert(event.at("tid").get<unsigned>());
        if (event.at("ph") != "X") continue;
        ++count;
        EXPECT_GE(event.at("dur").get<double>(), 0);
        EXPECT_GE(event.at("ts").get<double>(), 0);
#if defined(__linux__)
        EXPECT_GE(event.at("args").at("thread_cpu_us").get<double>(), 0);
#endif
        if (event.at("name") == "parent") parent = event;
        if (event.at("name") == "child") child = event;
    }
    EXPECT_EQ(threads.size(), 2);
    EXPECT_EQ(count, 4);
    ASSERT_FALSE(parent.is_null() || child.is_null());
    EXPECT_LE(parent.at("ts").get<double>(), child.at("ts").get<double>());
    EXPECT_GE(parent.at("ts").get<double>() + parent.at("dur").get<double>(),
              child.at("ts").get<double>() + child.at("dur").get<double>());
}

TEST(CpuTrace, DisabledNestedBindingRestoresOuterTrace) {
    {
        CpuTrace trace(tracePath.string());
        CpuTrace::Thread outer(&trace, "outer");
        {
            CpuTrace disabled;
            CpuTrace::Thread binding(&disabled, "disabled");
            CpuTrace::Scope ignored("absent");
        }
        CpuTrace::Scope restored("restored");
    }
    const auto events = readTrace().at("traceEvents");
    ASSERT_EQ(events.size(), 2);
    EXPECT_EQ(events.at(1).at("name"), "restored");
    // No stale pointer remains after both owner and binding destruction.
    CpuTrace::Scope unbound("unbound");
}

TEST(CpuTrace, BadOutputPathFailsImmediately) {
    EXPECT_THROW(CpuTrace((tracePath / "missing" / "trace.json").string()), std::runtime_error);
}
