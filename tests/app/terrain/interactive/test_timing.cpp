#include "FrameTestSupport.h"
#include <map>
#include <sstream>

TEST(TerrainFrame, TracedPreparationRetainsRequestKeysAndDoesNotWaitForTimingQueries) {
    auto o=options("timing");o.performanceTrace=o.configPath+".frames.csv";
    std::uint64_t readySamples=0;
    {
        rendering::Renderer renderer(o);auto& r=Probe::state(renderer);r.options.renderTestMode=false;
        GlAudit audit;
        const auto tracedTick=[&] {
            r.profiler.beginFrame(20);tick(r);r.profiler.endFrame();
            glFlush(); // Test presentation submits commands, never waits for them.
        };
        ASSERT_TRUE(until(tracedTick,[&]{return idle(r);}));
        for(int i=0;i<4;++i) {
            r.profiler.beginFrame(20);draw(r,false);r.profiler.endFrame();glFlush();
        }
        ASSERT_TRUE(until(tracedTick,[&]{return r.profiler.gpuWork().stats().pending==0;}));
        EXPECT_EQ(r.profiler.gpuWork().stats().dropped,0u);
        readySamples=r.profiler.gpuWork().stats().ready;EXPECT_GE(readySamples,10u);
        EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
        std::ofstream(o.configPath+".timing.json")<<json{{"gl",audit.state()},
            {"renderer",reinterpret_cast<const char*>(glGetString(GL_RENDERER))},
            {"version",reinterpret_cast<const char*>(glGetString(GL_VERSION))},
            {"ready_samples",readySamples},{"dropped_samples",r.profiler.gpuWork().stats().dropped},
            {"peak_pending",r.profiler.gpuWork().stats().peakPending},{"publication",r.terrainPublicationState()}}.dump(2);
    }
    std::ifstream input(o.performanceTrace+".gpu-work.csv");std::string line;ASSERT_TRUE(std::getline(input,line));
    const auto split=[](const std::string& text) {
        std::vector<std::string> fields;std::istringstream stream(text);std::string value;
        while(std::getline(stream,value,',')) fields.push_back(value);
        if(!text.empty() && text.back()==',') fields.emplace_back();return fields;
    };
    const auto header=split(line);std::map<std::string,unsigned> stages;unsigned count=0;
    while(std::getline(input,line)) {
        const auto values=split(line);ASSERT_EQ(values.size(),header.size());std::map<std::string,std::string> row;
        for(std::size_t i=0;i<header.size();++i) row[header[i]]=values[i];
        EXPECT_EQ(row["status"],"ready");EXPECT_EQ(row["identity_valid"],"1");
        EXPECT_GT(std::stoull(row["request_serial"]),0u);EXPECT_EQ(row["epoch"],"1");
        EXPECT_LT(std::stoull(row["body"]),2u);EXPECT_GT(std::stoull(row["field"]),0u);
        EXPECT_GT(std::stoull(row["topology"]),0u);EXPECT_EQ(row["backend"],"2");
        EXPECT_GE(std::stod(row["gpu_ms"]),0);++stages[row["stage"]];++count;
    }
    EXPECT_EQ(count,readySamples);
    for(const char* stage:rendering::gpuWorkStageNames) EXPECT_GT(stages[stage],0u) << stage;
}
