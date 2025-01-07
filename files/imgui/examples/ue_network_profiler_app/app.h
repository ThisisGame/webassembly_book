//
// Created by captainchen on 2024/12/19.
//

#ifndef EXAMPLE_PROFILER_APP_APP_H
#define EXAMPLE_PROFILER_APP_APP_H

#define IP_HEADER_SIZE 20
#define UDP_HEADER_SIZE IP_HEADER_SIZE + 8
#define UDP_HEADER_SIZE_BITS UDP_HEADER_SIZE * 8

#include <string>
#include <unordered_map>
#include <map>
#include <sstream>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "GLFW/glfw3.h"
#ifndef __EMSCRIPTEN__
#include "imgui-filebrowser/imfilebrowser.h"
#include "network_stream.h"

#endif

class App {
public:
    void Init(GLFWwindow* window);
    void Update(int window_width, int window_height);
    void ResetData();
    void ParseLogFile(const std::string& file_path);
    void ParseNProf(const std::string& file_path);
    void ParseLogLines(const std::vector<std::string>& lines);
private:
#ifndef __EMSCRIPTEN__
    // create a file browser instance
    ImGui::FileBrowser file_browser;
#endif

    std::vector<std::string> connection_names;//连接名字
    std::map<std::string, std::vector<float>> connection_queued_bits_data_map;//连接名字和带宽余量数据
    std::map<std::string, std::vector<float>> connection_replicate_actor_bits_write_data_map;//属性同步 连接名字和写入数据
    std::map<std::string, std::vector<float>> connection_rpc_bits_write_data_map;//RPC 连接名字和写入数据

    std::vector<float> queued_bits_data;//带宽余量数据
    std::vector<std::string> queued_bits_data_show_tips;//带宽余量数据显示的提示
    float max_queued_bits=FLT_MIN;
    float min_queued_bits=FLT_MAX;

    std::vector<float> replicate_actor_bits_write_data;//属性同步BitsWrite柱状图 数据
    float max_replicate_actor_bits_write=FLT_MIN;
    float min_replicate_actor_bits_write=FLT_MAX;
    std::vector<std::string> replicate_actor_bits_write_data_show_tips;//属性同步BitsWrite柱状图 显示的提示
    std::vector<std::string> replicate_actor_bits_write_data_short_tips;//属性同步BitsWrite柱状图 显示的简要提示


    std::vector<float> rpc_bits_write_data;//RPCBitsWrite柱状图 数据
    float max_rpc_bits_write=FLT_MIN;
    float min_rpc_bits_write=FLT_MAX;
    std::vector<std::string> rpc_bits_write_data_show_tips;//RPCBitsWrite柱状图 显示的提示


    std::map<std::string,std::vector<std::string>> connection_queued_bits_data_show_tips_map;//QueuedBits柱状图 连接名字和显示的提示
    std::map<std::string,std::vector<std::string>> connection_replicate_actor_bits_write_data_show_tips_map;//属性同步BitsWrite柱状图 连接名字和显示的提示
    std::map<std::string,std::vector<std::string>> connection_replicate_actor_bits_write_data_short_tips_map;//属性同步BitsWrite柱状图 连接名字和显示的简要提示
    std::map<std::string,std::vector<std::string>> connection_rpc_bits_write_data_show_tips_map;//RPCBitsWrite柱状图 连接名字和显示的提示

    std::string hover_tip_;//鼠标悬停的提示

    NetworkStream network_stream_;

    bool has_collect_connection_data_=false;//是否已经收集当前Connection的数据
public:
    static App* GetInstance();
private:
    static App* instance_;
};


#endif //EXAMPLE_PROFILER_APP_APP_H
