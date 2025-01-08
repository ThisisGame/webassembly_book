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
    file_browser.SetTypeFilters({".log" });
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

            for(auto& key_value_pair:connection_queued_bits_data_map)
            {
                //显示Connection名字
                ImGui::Text(key_value_pair.first.c_str());

                //显示Connection的带宽余量
                ImGui::BeginChild(key_value_pair.first.c_str(),ImVec2(0, 120*4),true,ImGuiWindowFlags_AlwaysHorizontalScrollbar);
                {
                    static float queued_bits_middle_value=0.f;
                    ImGui::SliderFloat("middle value", &queued_bits_middle_value, min_queued_bits, max_queued_bits);

                    //绘制QueuedBits曲线
                    ImGui::Text("QueuedBits");
                    float window_width = key_value_pair.second.size() * 10;
                    ImGui::BeginChild("##QueuedBits PlotLines", ImVec2(window_width, 80.0f));
                    ImGui::PlotLines("##QueuedBits PlotLines", key_value_pair.second.data(), key_value_pair.second.size(),0,NULL,min_queued_bits,max_queued_bits,ImVec2(window_width,80.0f));
                    ImGui::EndChild();

                    //绘制QueuedBits柱状图
                    ImGui::Text("QueuedBits");
                    ImGui::BeginChild("##QueuedBits PlotHistogram", ImVec2(window_width, 80.0f));
                    ImGui::PlotHistogramWithColor("##QueuedBits PlotHistogram", key_value_pair.second.data(), key_value_pair.second.size(), 0, NULL, min_queued_bits, max_queued_bits, ImVec2(window_width, 80.0f), sizeof(float), queued_bits_middle_value, ImGui::GetColorU32(ImVec4(1,0,0,1)), ImGui::GetColorU32(ImVec4(0,1,0,1)), connection_queued_bits_data_show_tips_map[key_value_pair.first],hover_tip_);
                    ImGui::EndChild();

                    //绘制属性同步BitsWrite柱状图
                    ImGui::Text("ReplicateActor BitsWrite");
                    std::vector<float>& bits_write_data=connection_replicate_actor_bits_write_data_map[key_value_pair.first];
                    //按下l键时，切换到详细提示
                    ImGuiIO& io = ImGui::GetIO();
                    if(ImGui::IsKeyDown(ImGuiKey_D)){
                        ImGui::BeginChild("##ReplicateActor PlotHistogram Details", ImVec2(window_width, 80.0f));
                        ImGui::PlotHistogramWithColor("##ReplicateActor PlotHistogram Details", bits_write_data.data(), bits_write_data.size(), 0, NULL, min_replicate_actor_bits_write, max_replicate_actor_bits_write, ImVec2(window_width, 80.0f), sizeof(float), 0, ImGui::GetColorU32(ImVec4(1, 0, 0, 1)), ImGui::GetColorU32(ImVec4(0, 1, 0, 1)), connection_replicate_actor_bits_write_data_show_tips_map[key_value_pair.first],hover_tip_);
                        ImGui::EndChild();
                    }else{
                        ImGui::BeginChild("##ReplicateActor PlotHistogram", ImVec2(window_width, 80.0f));
                        ImGui::PlotHistogramWithColor("##ReplicateActor PlotHistogram", bits_write_data.data(), bits_write_data.size(), 0, NULL, min_replicate_actor_bits_write, max_replicate_actor_bits_write, ImVec2(window_width, 80.0f), sizeof(float), 0, ImGui::GetColorU32(ImVec4(1, 0, 0, 1)), ImGui::GetColorU32(ImVec4(0, 1, 0, 1)), connection_replicate_actor_bits_write_data_short_tips_map[key_value_pair.first],hover_tip_);
                        ImGui::EndChild();
                    }


                    //绘制RPC BitsWrite柱状图
                    ImGui::Text("RPC BitsWrite");
                    std::vector<float>& rpc_bits_write_data=connection_rpc_bits_write_data_map[key_value_pair.first];
                    ImGui::BeginChild("##RPC PlotHistogram", ImVec2(window_width, 80.0f));
                    ImGui::PlotHistogramWithColor("##RPC PlotHistogram", rpc_bits_write_data.data(), rpc_bits_write_data.size(), 0, NULL, min_rpc_bits_write, max_rpc_bits_write, ImVec2(window_width, 80.0f), sizeof(float), 0, ImGui::GetColorU32(ImVec4(1, 0, 0, 1)), ImGui::GetColorU32(ImVec4(0, 1, 0, 1)), connection_rpc_bits_write_data_show_tips_map[key_value_pair.first],hover_tip_);
                    ImGui::EndChild();
                }
                ImGui::EndChild();
            }
        }
        ImGui::End();
    }
}

void App::ResetData() {
    if(log_track_net_data_!=nullptr)
    {
        delete log_track_net_data_;
        log_track_net_data_=nullptr;
    }
    connection_names.clear();
    connection_queued_bits_data_map.clear();
    connection_replicate_actor_bits_write_data_map.clear();
    connection_rpc_bits_write_data_map.clear();
    max_queued_bits=FLT_MIN;
    min_queued_bits=FLT_MAX;
    max_replicate_actor_bits_write=FLT_MIN;
    min_replicate_actor_bits_write=FLT_MAX;
    max_rpc_bits_write=FLT_MIN;
    min_rpc_bits_write=FLT_MAX;
    connection_queued_bits_data_show_tips_map.clear();
    connection_replicate_actor_bits_write_data_show_tips_map.clear();
    connection_replicate_actor_bits_write_data_short_tips_map.clear();
    connection_rpc_bits_write_data_show_tips_map.clear();
    hover_tip_.clear();
}

void App::ParseLogFile(const std::string &file_path) {

    std::ifstream file(file_path);
    //将file每一行读取出来存储到Vector中
    std::vector<std::string> lines;
    std::string str;
    while (std::getline(file, str)) {
        lines.push_back(str);
    }

    //解析每一行
    ParseLogLines(lines);
}

void App::ParseLogLines(const std::vector<std::string> &lines) {
    std::cout<<"ParseLogLines lines.size():"<<lines.size()<<std::endl;

    ResetData();

    log_track_net_data_=new LogTrackNetData();

    std::string last_frame_index_str;
    int frame_index=0;
    bool in_net_driver_tick_flush=false;

    LogTrackNetDataOneFrame* current_frame_data=nullptr;
    LogTrackNetDataOneConnection* current_connection_data=nullptr;
    bool in_replication_graph_server_replicate_actors_loop_connection=false;

    std::map<std::string,float> replicate_property_map;

    for(auto& str:lines)
    {
        std::string_view str_view(str);

        //Log示例：[2024.11.30-23.49.00:165][ 11]LogNetTrack: UNetConnection::Tick:
        //仅处理'LogNetTrack:'的log
        size_t log_tag_pos=str_view.find("LogNetTrack:");
        if(log_tag_pos==std::string_view ::npos)
        {
            continue;
        }

        //提取时间[2024.11.30-23.49.00:165]
        size_t pos_time_start=str_view.find('[');
        size_t pos_time_end=str_view.find(']');
        std::string time_str = std::string(str_view.substr(pos_time_start + 1, pos_time_end - pos_time_start - 1));

        //提取帧数[ 11]
        size_t pos_frame_index_start=str_view.find('[',pos_time_end) + 1;
        size_t pos_frame_index_end=str_view.find(']',pos_frame_index_start);
        std::string frame_index_str = std::string(str_view.substr(pos_frame_index_start, pos_frame_index_end - pos_frame_index_start));

        //如果帧数发生变化，就增加帧数
        if(frame_index_str!=last_frame_index_str)
        {
            frame_index++;
            last_frame_index_str=frame_index_str;

            //创建帧数据
            if(log_track_net_data_->frame_data_map_.find(frame_index)==log_track_net_data_->frame_data_map_.end())
            {
                LogTrackNetDataOneFrame* frame_data=new LogTrackNetDataOneFrame();
                frame_data->frame_time_=time_str;
                log_track_net_data_->frame_data_map_[frame_index]=frame_data;

                current_frame_data=frame_data;
            }
        }

        //注意RPC是在Actor::Tick中处理的，在NetDriver::Tick之前执行，所以这里先收集RPC的数据
        //Log示例：[2024.12.02-16.25.45:875][ 65]LogNetTrack: UNetDriver::ProcessRemoteFunctionForChannel: Describe:[UNetConnection] RemoteAddr: 127.0.0.1:51843, Name: GPNetConnection_1, Driver: GameNetDriver GPNetDriver_0, IsServer: YES, PC: BP_PlayerController_C_0, Owner: BP_PlayerController_C_0, UniqueId: INVALID, ActorName: ScoreTips, Function: SetScoreInfo, BitsWritten: 13
        {
            //先判断log是否包含'LogNetTrack: UNetDriver::ProcessRemoteFunctionForChannel: Describe:'
            if(str_view.find("LogNetTrack: UNetDriver::ProcessRemoteFunctionForChannel: Describe:")!=std::string_view ::npos)
            {
                //解析RemoteAddr
                size_t pos_start=str_view.find("RemoteAddr: ")+12;
                size_t pos_end=str_view.find(',',pos_start);
                std::string remote_addr=std::string(str_view.substr(pos_start,pos_end-pos_start));

                //创建Connection数据
                if(current_frame_data->connection_data_map_.find(remote_addr)==current_frame_data->connection_data_map_.end())
                {
                    LogTrackNetDataOneConnection* connection_data=new LogTrackNetDataOneConnection();
                    current_frame_data->connection_data_map_[remote_addr]=connection_data;
                }

                //解析ActorName
                pos_start=str_view.find("ActorName: ")+11;
                pos_end=str_view.find(',',pos_start);
                std::string actor_name=std::string(str_view.substr(pos_start,pos_end-pos_start));

                //解析Function
                pos_start=str_view.find("Function: ")+10;
                pos_end=str_view.find(',',pos_start);
                std::string function_name=std::string(str_view.substr(pos_start,pos_end-pos_start));

                //解析BitsWritten
                pos_start=str_view.find("BitsWritten: ")+13;
                std::string bits_written_str=std::string(str_view.substr(pos_start));
                float bits_written=std::stof(bits_written_str);

                //创建RPC数据
                LogTrackNetDataOneRPC* rpc_data=new LogTrackNetDataOneRPC();
                rpc_data->function_name_=function_name;
                rpc_data->bits_write_=bits_written;

                current_frame_data->connection_data_map_[remote_addr]->actor_rpc_map_[actor_name].push_back(rpc_data);
            }
        }

        //然后进入NetDriver::Tick大循环，驱动ReplicationGraph
        //先判断log是否包含UNetDriver::TickFlush:，如果不包含，就跳过
        if(in_net_driver_tick_flush==false)
        {
            if(str_view.find("LogNetTrack: UNetDriver::TickFlush: Begin")==std::string_view ::npos)
            {
                continue;
            }
            in_net_driver_tick_flush=true;
        }

        if(in_replication_graph_server_replicate_actors_loop_connection==false)
        {
            if(str_view.find("LogNetTrack: UReplicationGraph::ServerReplicateActors: Loop Connection Begin: Describe:")!=std::string_view ::npos)
            {
                in_replication_graph_server_replicate_actors_loop_connection=true;

                //解析RemoteAddr
                size_t pos_start=str_view.find("RemoteAddr: ")+12;
                size_t pos_end=str_view.find(',',pos_start);
                std::string remote_addr=std::string(str_view.substr(pos_start,pos_end-pos_start));

                //创建Connection数据
                if(current_frame_data->connection_data_map_.find(remote_addr)==current_frame_data->connection_data_map_.end())
                {
                    LogTrackNetDataOneConnection* connection_data=new LogTrackNetDataOneConnection();
                    current_frame_data->connection_data_map_[remote_addr]=connection_data;
                }
                current_connection_data=current_frame_data->connection_data_map_[remote_addr];
            }
        }
        if(in_replication_graph_server_replicate_actors_loop_connection)
        {
            //进入单个Actor同步

            //解析变化的属性，Log示例 LogNetTrack: CompareProperties_r: PropertiesAreIdentical Class:xxx  Property:xxx
            if(str_view.find("LogNetTrack: FRepLayout::SendProperties_r: Cmd: ")!=std::string_view ::npos)
            {
                //解析Property
                size_t pos_start=str_view.find("Cmd: ")+5;
                size_t pos_end=str_view.find(',',pos_start);
                std::string property_name=std::string(str_view.substr(pos_start,pos_end-pos_start));

                //解析BitsWritten
                pos_start=str_view.find("BitsWritten: ")+12;
                std::string bits_written_str=std::string(str_view.substr(pos_start));
                float bits_written=std::stof(bits_written_str);

                //先判断replicate_property_map是否已经存在这个属性，如果不存在，就添加，如果存在就叠加bits_written
                if(replicate_property_map.find(property_name)==replicate_property_map.end())
                {
                    replicate_property_map[property_name]=bits_written;
                }
                else
                {
                    replicate_property_map[property_name]+=bits_written;
                }
            }

            //解析单个Actor字节数，Log示例 LogNetTrack: ReplicationGraph::ReplicateSingleActor: ReplicateSingleActor ActorName:xx, ActorClass:xx, BitsWritten:111

            //先判断log是否包含LogNetTrack: ReplicationGraph::ReplicateSingleActor: ReplicateSingleActor ActorName:
            if(str_view.find("LogNetTrack: UReplicationGraph::ReplicateSingleActor: ActorName:")!=std::string_view ::npos)
            {
                //解析ActorName
                size_t pos_start=str_view.find("ActorName:")+10;
                size_t pos_end=str_view.find(',',pos_start);
                std::string actor_name=std::string(str_view.substr(pos_start,pos_end-pos_start));

                //解析ActorClass
                pos_start=str_view.find("ActorClass:",pos_end)+11;
                pos_end=str_view.find(',',pos_start);
                std::string actor_class=std::string(str_view.substr(pos_start,pos_end-pos_start));

                //解析BitsWritten
                pos_start=str_view.find("BitsWritten:",pos_end)+12;
                std::string bits_written_str=std::string(str_view.substr(pos_start));
                float bits_written=std::stof(bits_written_str);

                //创建ActorReplicator数据
                if(current_connection_data->actor_replicator_map_.find(actor_name)==current_connection_data->actor_replicator_map_.end())
                {
                    LogTrackNetDataOneActorReplicator* actor_replicator=new LogTrackNetDataOneActorReplicator();
                    actor_replicator->bits_write_=bits_written;
                    actor_replicator->property_map_=std::move(replicate_property_map);
                    current_connection_data->actor_replicator_map_[actor_name]=actor_replicator;

                    replicate_property_map.clear();
                }
            }

            //查找带宽占满了丢弃的Actor，Log示例：LogNetTrack: UReplicationGraph::ReplicateActorListsForConnections_Default: Network Saturate DropActor: xxx
            if(str_view.find("LogNetTrack: UReplicationGraph::ReplicateActorListsForConnections_Default: Network Saturate DropActor:")!=std::string_view ::npos)
            {
                //解析ActorName
                size_t pos_start=str_view.find("DropActor: ")+11;
                size_t pos_end=str_view.find(',',pos_start);
                std::string actor_name=std::string(str_view.substr(pos_start,pos_end-pos_start));

                current_connection_data->actor_replicator_drop_names_.push_back(actor_name);
            }

            if(str_view.find("LogNetTrack: UReplicationGraph::ServerReplicateActors: Loop Connection End: Describe:")!=std::string_view ::npos) {
                in_replication_graph_server_replicate_actors_loop_connection = false;
            }
        }


        //先判断log是否包含'UNetDriver::TickFlush: End'，如果包含则结束一次Tick
        if(in_net_driver_tick_flush)
        {
            //Log示例：[2024.11.30-23.49.00:165][ 11]LogNetTrack: UNetConnection::Tick: Describe:[UNetConnection] RemoteAddr: 127.0.0.1:50125, Name: GPNetConnection_5, Driver: GameNetDriver GPNetDriver_4, IsServer: YES, PC: BP_PlayerController_C_0, Owner: BP_PlayerController_C_0, UniqueId: INVALID, QueuedBits: -1310.250000, DeltaBits: 1000.000061, AllowedLag: 2000.000122, DeltaTime: 0.061672, BandwidthDeltaTime: 0.033333


            //提取QueuedBits，先定位到', QueuedBits: '的位置，然后以这个为开始，往右边找到',',然后截取中间的数值就是QueuedBits，然后转换成float
            std::string_view queued_bits_str=", QueuedBits: ";
            size_t pos=str_view.find(queued_bits_str);
            if(pos==std::string_view::npos)
            {
                continue;
            }
            pos+=queued_bits_str.size();
            size_t pos_end=str_view.find(',',pos);
            if(pos_end==std::string_view::npos)
            {
                continue;
            }
            float queued_bits_value = std::stof(std::string(str_view.substr(pos, pos_end - pos)));

            //提取DeltaBits
            std::string_view delta_bits_str=", DeltaBits: ";
            pos=str_view.find(delta_bits_str);
            if(pos==std::string_view::npos)
            {
                continue;
            }
            pos+=delta_bits_str.size();
            pos_end=str_view.find(',',pos);
            if(pos_end==std::string_view::npos)
            {
                continue;
            }
            float delta_bits_value = std::stof(std::string(str_view.substr(pos, pos_end - pos)));

            //提取AllowedLag
            std::string_view allowed_lag_str=", AllowedLag: ";
            pos=str_view.find(allowed_lag_str);
            if(pos==std::string_view::npos)
            {
                continue;
            }
            pos+=allowed_lag_str.size();
            pos_end=str_view.find(',',pos);
            if(pos_end==std::string_view::npos)
            {
                continue;
            }
            float allowed_lag_value = std::stof(std::string(str_view.substr(pos, pos_end - pos)));


            //解析RemoteAddr
            pos=str_view.find("RemoteAddr: ")+12;
            pos_end=str_view.find(',',pos);
            std::string remote_addr=std::string(str_view.substr(pos,pos_end-pos));

            //创建Connection数据
            if(current_frame_data->connection_data_map_.find(remote_addr)==current_frame_data->connection_data_map_.end())
            {
                LogTrackNetDataOneConnection* connection_data=new LogTrackNetDataOneConnection();
                current_frame_data->connection_data_map_[remote_addr]=connection_data;

                current_connection_data=connection_data;
            }

            current_connection_data->queued_bits_=queued_bits_value;
            current_connection_data->delta_bits_=delta_bits_value;
            current_connection_data->allowed_lag_=allowed_lag_value;

            if(str_view.find("LogNetTrack: UNetDriver::TickFlush: End")==std::string_view ::npos)
            {
                continue;
            }
            in_net_driver_tick_flush=false;
        }
    }

    //提取出所有的Connection名字
    for(auto& frame_data_pair:log_track_net_data_->frame_data_map_)
    {
        LogTrackNetDataOneFrame* frame_data=frame_data_pair.second;
        for(auto& connection_data_pair:frame_data->connection_data_map_)
        {
            if(connection_names.size()==0||std::find(connection_names.begin(),connection_names.end(),connection_data_pair.first)==connection_names.end()) {
                connection_names.push_back(connection_data_pair.first);
            }
        }
    }

    //提取出每个Connection的QueuedBits数据
    for(auto& frame_data_pair:log_track_net_data_->frame_data_map_)
    {
        LogTrackNetDataOneFrame* frame_data=frame_data_pair.second;

        for(auto& connection_name:connection_names)
        {
            if(frame_data->connection_data_map_.find(connection_name)==frame_data->connection_data_map_.end())//这一帧没有这个Connection，就填充0
            {
                connection_queued_bits_data_map[connection_name].push_back(0);
            }
            else
            {
                connection_queued_bits_data_map[connection_name].push_back(frame_data->connection_data_map_[connection_name]->queued_bits_);
            }
        }
    }

    //计算QueuedBits最大值和最小值
    for(auto& connection_name:connection_names)
    {
        for(auto& queued_bits:connection_queued_bits_data_map[connection_name])
        {
            if(queued_bits>max_queued_bits)
            {
                max_queued_bits=queued_bits;
            }
            if(queued_bits<min_queued_bits)
            {
                min_queued_bits=queued_bits;
            }
        }
    }

    //提取出属性同步每个Connection的BitsWrite数据
    for(auto& frame_data_pair:log_track_net_data_->frame_data_map_)
    {
        LogTrackNetDataOneFrame* frame_data=frame_data_pair.second;

        for(auto& connection_name:connection_names)
        {
            if(frame_data->connection_data_map_.find(connection_name)==frame_data->connection_data_map_.end())//这一帧没有这个Connection，就填充0
            {
                connection_replicate_actor_bits_write_data_map[connection_name].push_back(0);
            }
            else
            {
                float bits_write=0;
                for(auto& actor_replicator_pair:frame_data->connection_data_map_[connection_name]->actor_replicator_map_)
                {
                    bits_write+=actor_replicator_pair.second->bits_write_;
                }
                connection_replicate_actor_bits_write_data_map[connection_name].push_back(bits_write);
            }
        }
    }

    //计算属性同步BitsWrite最大值和最小值
    for(auto& connection_name:connection_names)
    {
        for(auto& bits_write:connection_replicate_actor_bits_write_data_map[connection_name])
        {
            if(bits_write > max_replicate_actor_bits_write)
            {
                max_replicate_actor_bits_write=bits_write;
            }
            if(bits_write < min_replicate_actor_bits_write)
            {
                min_replicate_actor_bits_write=bits_write;
            }
        }
    }

    //提取出RPC每个Connection的BitsWrite数据
    for(auto& frame_data_pair:log_track_net_data_->frame_data_map_)
    {
        LogTrackNetDataOneFrame* frame_data=frame_data_pair.second;

        for(auto& connection_name:connection_names)
        {
            if(frame_data->connection_data_map_.find(connection_name)==frame_data->connection_data_map_.end())//这一帧没有这个Connection，就填充0
            {
                connection_rpc_bits_write_data_map[connection_name].push_back(0);
            }
            else
            {
                float bits_write=0;
                for(auto& actor_rpc_pair:frame_data->connection_data_map_[connection_name]->actor_rpc_map_)
                {
                    for(auto& rpc_data:actor_rpc_pair.second)
                    {
                        bits_write+=rpc_data->bits_write_;
                    }
                }
                connection_rpc_bits_write_data_map[connection_name].push_back(bits_write);
            }
        }
    }

    //计算RPC BitsWrite最大值和最小值
    for(auto& connection_name:connection_names)
    {
        for(auto& bits_write:connection_rpc_bits_write_data_map[connection_name])
        {
            if(bits_write > max_rpc_bits_write)
            {
                max_rpc_bits_write=bits_write;
            }
            if(bits_write < min_rpc_bits_write)
            {
                min_rpc_bits_write=bits_write;
            }
        }
    }
    //---------------------------------------

    //提取出QueuedBits柱状图 连接名字和显示的提示
    for(auto& frame_data_pair:log_track_net_data_->frame_data_map_)
    {
        LogTrackNetDataOneFrame* frame_data=frame_data_pair.second;

        for(auto& connection_name:connection_names)
        {
            if(frame_data->connection_data_map_.find(connection_name)==frame_data->connection_data_map_.end())//这一帧没有这个Connection，就填充
            {
                connection_queued_bits_data_show_tips_map[connection_name].push_back(frame_data->frame_time_ + "\n\n" + "connection not exist in this frame");
            }
            else//这一帧有这个Connection，就填充
            {
                const std::string& show_tip=frame_data->frame_time_ + "\n\n" + frame_data->connection_data_map_[connection_name]->to_string(true,true,true,false,false,false,true);
                connection_queued_bits_data_show_tips_map[connection_name].push_back(show_tip);
            }
        }
    }

    //提取出属性同步BitsWrite柱状图 连接名字和显示的提示
    for(auto& frame_data_pair:log_track_net_data_->frame_data_map_)
    {
        LogTrackNetDataOneFrame* frame_data=frame_data_pair.second;

        for(auto& connection_name:connection_names)
        {
            if(frame_data->connection_data_map_.find(connection_name)==frame_data->connection_data_map_.end())//这一帧没有这个Connection，就填充
            {
                connection_replicate_actor_bits_write_data_show_tips_map[connection_name].push_back(frame_data->frame_time_ + "\n\n" + "connection not exist in this frame");
            }
            else//这一帧有这个Connection，就填充
            {
                const std::string& show_tip=frame_data->frame_time_ + "\n\n" + frame_data->connection_data_map_[connection_name]->to_string(false,false,false,true,true,false,true);
                connection_replicate_actor_bits_write_data_show_tips_map[connection_name].push_back(show_tip);
            }
        }
    }

    //提取出属性同步BitsWrite柱状图 连接名字和显示的简要提示
    for(auto& frame_data_pair:log_track_net_data_->frame_data_map_)
    {
        LogTrackNetDataOneFrame* frame_data=frame_data_pair.second;

        for(auto& connection_name:connection_names)
        {
            if(frame_data->connection_data_map_.find(connection_name)==frame_data->connection_data_map_.end())//这一帧没有这个Connection，就填充
            {
                connection_replicate_actor_bits_write_data_short_tips_map[connection_name].push_back(frame_data->frame_time_ + "\n\n" + "connection not exist in this frame");
            }
            else//这一帧有这个Connection，就填充
            {
                const std::string& show_tip=frame_data->frame_time_ + "\n\n" + frame_data->connection_data_map_[connection_name]->to_string(false,false,false,true,false,false,true);
                connection_replicate_actor_bits_write_data_short_tips_map[connection_name].push_back(show_tip);
            }
        }
    }

    //提取出RPCBitsWrite柱状图 连接名字和显示的提示
    for(auto& frame_data_pair:log_track_net_data_->frame_data_map_)
    {
        LogTrackNetDataOneFrame* frame_data=frame_data_pair.second;

        for(auto& connection_name:connection_names)
        {
            if(frame_data->connection_data_map_.find(connection_name)==frame_data->connection_data_map_.end())//这一帧没有这个Connection，就填充
            {
                connection_rpc_bits_write_data_show_tips_map[connection_name].push_back(frame_data->frame_time_ + "\n\n" + "connection not exist in this frame");
            }
            else//这一帧有这个Connection，就填充
            {
                const std::string& show_tip=frame_data->frame_time_ + "\n\n" + frame_data->connection_data_map_[connection_name]->to_string(false,false,true,false,false,true,false);
                connection_rpc_bits_write_data_show_tips_map[connection_name].push_back(show_tip);
            }
        }
    }
}

App *App::GetInstance() {
    if(instance_==nullptr)
    {
        instance_=new App();
    }
    return instance_;
}



