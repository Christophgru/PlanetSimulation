#pragma once
#include <gtest/gtest.h>
#include "rendering/diagnostics/FrameProfiler.h"
#include <filesystem>
#include <map>
#include <sstream>
#include <vector>

namespace timing_test {
struct Query { GLuint64 time=0; bool ready=false; };
inline std::map<GLuint,Query> queries;
inline GLuint next=0;
inline GLuint64 clock=0;
inline unsigned reads=0,unavailableReads=0,generated=0,deleted=0;
inline bool failAllocation=false;
inline void GLAPIENTRY gen(GLsizei count,GLuint* ids) {
    for(int i=0;i<count;++i) {
        ids[i]=failAllocation ? 0 : ++next;
        if(ids[i]) {queries[ids[i]]={};++generated;}
    }
}
inline void GLAPIENTRY remove(GLsizei count,const GLuint* ids) {
    for(int i=0;i<count;++i) if(ids[i]) {queries.erase(ids[i]);++deleted;}
}
inline void GLAPIENTRY counter(GLuint id,GLenum target) {
    EXPECT_EQ(target,GLenum(GL_TIMESTAMP));ASSERT_TRUE(queries.contains(id));
    queries[id]={clock+=1000000,false};
}
inline void GLAPIENTRY available(GLuint id,GLenum target,GLint* value) {
    EXPECT_EQ(target,GLenum(GL_QUERY_RESULT_AVAILABLE));ASSERT_TRUE(queries.contains(id));
    *value=queries.at(id).ready ? GL_TRUE : GL_FALSE;
}
inline void GLAPIENTRY result(GLuint id,GLenum target,GLuint64* value) {
    EXPECT_EQ(target,GLenum(GL_QUERY_RESULT));ASSERT_TRUE(queries.contains(id));
    ++reads;if(!queries.at(id).ready) ++unavailableReads;
    *value=queries.at(id).time;
}
inline void ready() {for(auto& [id,q]:queries) q.ready=true;}
struct FakeQueries {
    decltype(__glewGenQueries) oldGen=__glewGenQueries;
    decltype(__glewDeleteQueries) oldDelete=__glewDeleteQueries;
    decltype(__glewQueryCounter) oldCounter=__glewQueryCounter;
    decltype(__glewGetQueryObjectiv) oldAvailable=__glewGetQueryObjectiv;
    decltype(__glewGetQueryObjectui64v) oldResult=__glewGetQueryObjectui64v;
    FakeQueries() {
        queries.clear();next=0;clock=0;reads=unavailableReads=generated=deleted=0;failAllocation=false;
        __glewGenQueries=gen;__glewDeleteQueries=remove;__glewQueryCounter=counter;
        __glewGetQueryObjectiv=available;__glewGetQueryObjectui64v=result;
    }
    ~FakeQueries() {
        EXPECT_EQ(unavailableReads,0u);EXPECT_TRUE(queries.empty());EXPECT_EQ(generated,deleted);
        __glewGenQueries=oldGen;__glewDeleteQueries=oldDelete;__glewQueryCounter=oldCounter;
        __glewGetQueryObjectiv=oldAvailable;__glewGetQueryObjectui64v=oldResult;
    }
};
inline std::string path(const char* name) {
    const auto p=std::filesystem::path(PLANET_TIMING_OUTPUT)/name;
    std::filesystem::create_directories(p.parent_path());return p.string();
}
using Row=std::map<std::string,std::string>;
inline std::vector<std::string> cells(const std::string& line) {
    std::vector<std::string> result;std::istringstream in(line);std::string cell;
    while(std::getline(in,cell,',')) result.push_back(cell);
    if(!line.empty() && line.back()==',') result.emplace_back();return result;
}
inline std::vector<Row> rows(const std::string& file) {
    std::ifstream in(file);std::string line;std::getline(in,line);const auto header=cells(line);
    std::vector<Row> result;
    while(std::getline(in,line)) {
        const auto values=cells(line);EXPECT_EQ(header.size(),values.size());Row row;
        for(std::size_t i=0;i<std::min(values.size(),header.size());++i) row[header[i]]=values[i];
        result.push_back(std::move(row));
    }
    return result;
}
inline rendering::GpuWorkIdentity identity{4,7,2,123,456,1,1,2};
}
