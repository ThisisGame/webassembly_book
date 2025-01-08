//
// Created by captainchen on 2024/12/19.
//

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "app.h"
#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include <fstream>
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#include <emscripten/emscripten.h>
#include <emscripten/fetch.h>
#else
#include "imgui-filebrowser/imfilebrowser.h"
#endif

#include "app.h"
#include "stream_parser.h"




#ifdef __EMSCRIPTEN__
static bool fetch_file_finished = true;//是否下载完成

// Function to make the HTTP request
void fetchFileAndPassToWasm(const char* url) {
    std::cout << "fetchFileAndPassToWasm: " << url << std::endl;
    if (fetch_file_finished==false) {
        std::cerr << "Previous fetch is still in progress\n";
        return;
    }
    fetch_file_finished = false;

    // Call the JavaScript function to download the file
    EM_ASM_ARGS({
        downloadFile(UTF8ToString($0));
    }, url);
}

// Function to process file bytes in C++
extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void processFileBytes(const uint8_t* data, int length) {
        std::cout << "processFileBytes: " << length << " bytes\n";
        fetch_file_finished = true;

        if(length==0){
            std::cerr << "file length is 0\n";
            return;
        }

        // data是一个文本，以'\n'分割
        std::string str((const char*)data, length);
        std::vector<std::string> lines;
        std::string::size_type pos1, pos2;
        pos2 = str.find('\n');
        pos1 = 0;
        while(std::string::npos != pos2) {
            lines.push_back(str.substr(pos1, pos2-pos1));

            pos1 = pos2 + 1;
            pos2 = str.find('\n', pos1);
        }
        if(pos1 != str.length()) {
            lines.push_back(str.substr(pos1));
        }

        //解析每一行
        App::GetInstance()->ParseLogLines(lines);
    }
}
#endif

App* App::instance_=nullptr;

void App::Init(GLFWwindow* window) {
#ifdef __EMSCRIPTEN__
    // Call downloadFile after main has run
    fetch_file_finished = false;
    emscripten_run_script("downloadFileFromUrlParam()");//从url参数中下载文件，例如http://localhost:7000/index.html?logFileUrl=http://localhost:7000/ue.log
#else
    // (optional) set browser properties
    file_browser.SetTitle("title");
    file_browser.SetTypeFilters({".nprof" });
#endif
}


void App::Update(int window_width, int window_height) {
    // 1. 选择文件
    {
#ifdef __EMSCRIPTEN__
        //按钮点击打开文件
        ImGui::Begin("Open File");
        {
            if(fetch_file_finished){
                if(ImGui::Button("Open File")) {
                    fetchFileAndPassToWasm("http://localhost:7000/ue.log");
                }
            }else{
                ImGui::Text("Downloading...");
            }
        }
        ImGui::End();
#else
        if(ImGui::Begin("SelectFileWindow")){
            // open file dialog when user clicks this button
            if(ImGui::Button("SelectFile")){
                file_browser.Open();
            }
        }
        ImGui::End();
        file_browser.Display();

        if(file_browser.HasSelected())
        {
            std::cout << "Selected filename" << file_browser.GetSelected().string() << std::endl;

            ParseLogFile(file_browser.GetSelected().string());

            file_browser.ClearSelected();
        }
#endif
    }

    // 2. 性能分析窗口
    {
        ImGui::SetNextWindowSize(ImVec2(window_width-100, 300), ImGuiCond_FirstUseEver);
        ImGui::Begin("ue net profiler");
        {
            //先创建一个Tips窗口，鼠标悬浮到柱形图时刷新这个窗口
            ImGui::SetNextWindowSize(ImVec2(200, window_height-100), ImGuiCond_FirstUseEver);
            ImGui::Begin("Tooltip");
            {
                ImGui::TextUnformatted(hover_tip_.c_str());
            }
            ImGui::End();

            //显示Connection列表
            if(network_stream_.StringAddressArray.size()>0) {
                static int selected_connection_index = 0;
                if(ImGui::BeginCombo("Connection",network_stream_.StringAddressArray[selected_connection_index].c_str())) {
                    for(int i=0;i<network_stream_.StringAddressArray.size();i++) {
                        bool is_selected=(selected_connection_index==i);
                        if(ImGui::Selectable(network_stream_.StringAddressArray[i].c_str(),is_selected)) {
                            selected_connection_index=i;
                            ResetData();
                        }
                        if(is_selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                //构造QueuedBits数据
                CollectData(selected_connection_index);

                //创建Connection下拉框
                const char* connection_name=network_stream_.StringAddressArray[selected_connection_index].c_str();
                ImGui::BeginChild(connection_name,ImVec2(0, 125*4),true,ImGuiWindowFlags_AlwaysHorizontalScrollbar);
                {
                    static float queued_bits_middle_value=0.f;
                    ImGui::SliderFloat("middle value", &queued_bits_middle_value, min_queued_bits, max_queued_bits);

                    int frame_num=network_stream_.Frames.size();
                    float window_width = frame_num * 10;

                    //绘制QueuedBits曲线
                    if(queued_bits_data.size()>0)
                    {
                        ImGui::Text("QueuedBits");
                        ImGui::BeginChild("##QueuedBits PlotLines", ImVec2(window_width, 80.0f));
                        ImGui::PlotLines("##QueuedBits PlotLines", queued_bits_data.data(), frame_num,0,NULL,min_queued_bits,max_queued_bits,ImVec2(window_width,80.0f));
                        ImGui::EndChild();

                        //绘制QueuedBits柱状图
                        ImGui::Text("QueuedBits");
                        ImGui::BeginChild("##QueuedBits PlotHistogram", ImVec2(window_width, 80.0f));
                        ImGui::PlotHistogramWithColor("##QueuedBits PlotHistogram", queued_bits_data.data(), frame_num, 0, NULL, min_queued_bits, max_queued_bits, ImVec2(window_width, 80.0f), sizeof(float), queued_bits_middle_value, ImGui::GetColorU32(ImVec4(1,0,0,1)), ImGui::GetColorU32(ImVec4(0,1,0,1)), queued_bits_data_show_tips,hover_tip_);
                        ImGui::EndChild();
                    }

                    //绘制属性同步BitsWrite柱状图
                    ImGui::Text("ReplicateActor BitsWrite");
                    //按下l键时，切换到详细提示
                    ImGuiIO& io = ImGui::GetIO();
                    if(ImGui::IsKeyDown(ImGuiKey_D)){
                        ImGui::BeginChild("##ReplicateActor PlotHistogram Details", ImVec2(window_width, 80.0f));
                        ImGui::PlotHistogramWithColor("##ReplicateActor PlotHistogram Details", replicate_actor_bits_write_data.data(), replicate_actor_bits_write_data.size(), 0, NULL, min_replicate_actor_bits_write, max_replicate_actor_bits_write, ImVec2(window_width, 80.0f), sizeof(float), 0, ImGui::GetColorU32(ImVec4(1, 0, 0, 1)), ImGui::GetColorU32(ImVec4(0, 1, 0, 1)), replicate_actor_bits_write_data_show_tips,hover_tip_);
                        ImGui::EndChild();
                    }else{
                        ImGui::BeginChild("##ReplicateActor PlotHistogram", ImVec2(window_width, 80.0f));
                        ImGui::PlotHistogramWithColor("##ReplicateActor PlotHistogram", replicate_actor_bits_write_data.data(), replicate_actor_bits_write_data.size(), 0, NULL, min_replicate_actor_bits_write, max_replicate_actor_bits_write, ImVec2(window_width, 80.0f), sizeof(float), 0, ImGui::GetColorU32(ImVec4(1, 0, 0, 1)), ImGui::GetColorU32(ImVec4(0, 1, 0, 1)), replicate_actor_bits_write_data_short_tips,hover_tip_);
                        ImGui::EndChild();
                    }

                    //绘制RPC BitsWrite柱状图
                    ImGui::Text("RPC BitsWrite");
                    ImGui::BeginChild("##RPC PlotHistogram", ImVec2(window_width, 80.0f));
                    ImGui::PlotHistogramWithColor("##RPC PlotHistogram", rpc_bits_write_data.data(), rpc_bits_write_data.size(), 0, NULL, min_rpc_bits_write, max_rpc_bits_write, ImVec2(window_width, 80.0f), sizeof(float), 0, ImGui::GetColorU32(ImVec4(1, 0, 0, 1)), ImGui::GetColorU32(ImVec4(0, 1, 0, 1)), rpc_bits_write_data_show_tips,hover_tip_);
                    ImGui::EndChild();
                }
                ImGui::EndChild();
            }
        }
        ImGui::End();
    }
}

void App::CollectData(int selected_connection_index) {
    if(has_collect_connection_data_==false){
        has_collect_connection_data_=true;

        int frame_num=network_stream_.Frames.size();
        for(int i=0;i<frame_num;i++) {
            float queued_bits=0.f;
            std::string queued_bits_show_tip;

            float replicate_actor_bits=0.f;
            std::string replicate_actor_bits_write_data_show_tip;
            std::string replicate_actor_bits_write_data_short_tip;

            std::unordered_map<int,TokenSendRPC> actor_rpc_map;
            float rpc_bits = 0.f;
            std::string rpc_bits_write_data_show_tip;

            PartialNetworkStream* frame=network_stream_.Frames[i];
            for(const auto& Token:frame->Tokens){
                if(Token->ConnectionIndex==selected_connection_index){
                    ETokenTypes token_type=Token->GetTokenType();
                    if(token_type==ETokenTypes::ConnectionQueuedBits){
                        TokenConnectionQueuedBits* token_connection_queued_bits=static_cast<TokenConnectionQueuedBits*>(Token);
                        if(token_connection_queued_bits->ConnectionIndex==selected_connection_index){
                            queued_bits=token_connection_queued_bits->QueuedBits;
                        }
                    } else if (token_type==ETokenTypes::ReplicateActor) {
                        TokenReplicateActor* token_replicate_actor=static_cast<TokenReplicateActor*>(Token);
                        if(token_replicate_actor->ConnectionIndex==selected_connection_index){
                            replicate_actor_bits+=token_replicate_actor->GetNumReplicatedBits();
                            replicate_actor_bits_write_data_show_tip+=token_replicate_actor->GetDescription();
                            replicate_actor_bits_write_data_short_tip+=token_replicate_actor->GetShortDescription();
                        }
                    } else if (token_type==ETokenTypes::SendRPC) {
                        TokenSendRPC* token_send_rpc=static_cast<TokenSendRPC*>(Token);
                        if(token_send_rpc->ConnectionIndex==selected_connection_index){
                            rpc_bits+=token_send_rpc->GetNumTotalBits();
                            rpc_bits_write_data_show_tip+=token_send_rpc->GetDescription();
                        }
                    }
                }
            }

            //归总QueuedBits数据
            {
                queued_bits_data.push_back(queued_bits);
                //更新最大最小值
                if(queued_bits>max_queued_bits) {
                    max_queued_bits=queued_bits;
                }
                if(queued_bits<min_queued_bits) {
                    min_queued_bits=queued_bits;
                }

                std::string head_str;
                head_str+="queued_bits:"+std::to_string(queued_bits) + "\n";
                head_str+="-----------------------------\n";
                head_str+="total_bits_write:"+std::to_string(replicate_actor_bits + rpc_bits) + "\n";
                head_str+="-----------------------------\n";
                head_str+="udp_head:"+std::to_string(UDP_HEADER_SIZE);
                if(replicate_actor_bits>0){
                    head_str+="\n--------------replicator:" + std::to_string(replicate_actor_bits) + "---------------\n";
                    head_str+= replicate_actor_bits_write_data_short_tip;
                }
                if(rpc_bits>0){
                    head_str+="\n--------------rpc:" + std::to_string(rpc_bits) + "---------------\n";
                    head_str+= rpc_bits_write_data_show_tip;
                }
                queued_bits_data_show_tips.push_back(head_str);
            }

            //归总属性同步数据
            {
                replicate_actor_bits_write_data.push_back(replicate_actor_bits);
                //更新最大最小值
                if(replicate_actor_bits>max_replicate_actor_bits_write) {
                    max_replicate_actor_bits_write=replicate_actor_bits;
                }
                if(replicate_actor_bits<min_replicate_actor_bits_write) {
                    min_replicate_actor_bits_write=replicate_actor_bits;
                }

                std::string head_str;
                head_str+="total_bits_write:"+std::to_string(replicate_actor_bits);
                head_str+="\n-----------------------------\n";
                head_str+="udp_head:"+std::to_string(UDP_HEADER_SIZE);
                head_str+="\n--------------replicator:" + std::to_string(replicate_actor_bits) + "---------------\n";
                replicate_actor_bits_write_data_show_tip = head_str + replicate_actor_bits_write_data_show_tip;
                replicate_actor_bits_write_data_show_tips.push_back(replicate_actor_bits_write_data_show_tip);

                replicate_actor_bits_write_data_short_tip = head_str + replicate_actor_bits_write_data_short_tip;
                replicate_actor_bits_write_data_short_tips.push_back(replicate_actor_bits_write_data_short_tip);
            }

            //归总RPC数据
            {
                rpc_bits_write_data.push_back(rpc_bits);
                //更新最大最小值
                if(rpc_bits>max_rpc_bits_write) {
                    max_rpc_bits_write=rpc_bits;
                }
                if(rpc_bits<min_rpc_bits_write) {
                    min_rpc_bits_write=rpc_bits;
                }

                std::string head_str;
                head_str+="total_bits_write:"+std::to_string(rpc_bits);
                head_str+="\n-----------------------------\n";
                head_str+="udp_head:"+std::to_string(UDP_HEADER_SIZE);
                head_str+="\n--------------rpc:" + std::to_string(rpc_bits) + "---------------\n";
                rpc_bits_write_data_show_tip = head_str + rpc_bits_write_data_show_tip;
                rpc_bits_write_data_show_tips.push_back(rpc_bits_write_data_show_tip);
            }
        }
    }
}

void App::ResetData() {
    queued_bits_data.clear();
    queued_bits_data_show_tips.clear();
    max_queued_bits=FLT_MIN;
    min_queued_bits=FLT_MAX;

    replicate_actor_bits_write_data.clear();
    replicate_actor_bits_write_data_show_tips.clear();
    replicate_actor_bits_write_data_short_tips.clear();
    max_replicate_actor_bits_write=FLT_MIN;
    min_replicate_actor_bits_write=FLT_MAX;

    rpc_bits_write_data.clear();
    rpc_bits_write_data_show_tips.clear();
    max_rpc_bits_write=FLT_MIN;
    min_rpc_bits_write=FLT_MAX;

    has_collect_connection_data_=false;
}

void App::ParseLogFile(const std::string &file_path) {
    ResetData();

    std::ifstream file(file_path);
    ParseNProf(file_path);
}

void App::ParseNProf(const std::string &file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << file_path << std::endl;
        return;
    }
    //输出文件大小
    file.seekg(0, std::ios::end);
    std::cout<<"file size:"<<file.tellg()<<std::endl;
    file.seekg(0, std::ios::beg);

    //输出当前读取位置
//    std::cout<<"tellg:"<<file.tellg()<<std::endl;

    network_stream_ = StreamParser::Parse(file);
}

App *App::GetInstance() {
    if(instance_==nullptr)
    {
        instance_=new App();
    }
    return instance_;
}





