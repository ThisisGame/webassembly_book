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
#include <iomanip>
#include "GLFW/glfw3.h"


class LogTrackNetDataOneRPC{
public:
    std::string function_name_;//rpc名字
    float bits_write_;//同步Actor写入的字节数
};

class LogTrackNetDataOneActorReplicator{
public:
    float bits_write_;//同步Actor写入的字节数
    std::map<std::string,float> property_map_;//属性名字->写入的字节数
};

class LogTrackNetDataOneConnection{
public:
    float delta_bits_; //这一帧叠加的带宽余量
    float allowed_lag_;//这一帧最终给予的带宽余量
    float queued_bits_;//这一帧的带宽余量

    std::map<std::string, LogTrackNetDataOneActorReplicator*> actor_replicator_map_;//每个ActorReplicator的数据

    std::map<std::string, std::vector<LogTrackNetDataOneRPC*>> actor_rpc_map_;//每个RPC的数据

    std::vector<std::string> actor_replicator_drop_names_;//属性同步丢弃的Actor名字
public:
    std::string to_string(bool include_delta_bits=true,bool include_allowed_lag=true,bool include_queued_bits=true,
                          bool include_actor_replicator=true,bool include_actor_replicator_detail=true,bool include_actor_rpc=true,bool include_actor_replicator_drop=true){
        std::string str;

        if(include_queued_bits){
            str+="queued_bits:"+std::to_string(queued_bits_);
            str+="\n-----------------------------\n";
        }

        if(include_delta_bits){
            str+="delta_bits:"+std::to_string(delta_bits_);
            str+="\n-----------------------------\n";
        }

        if(include_allowed_lag){
            str+="allowed_lag:"+std::to_string(allowed_lag_);
            str+="\n-----------------------------\n";
        }

        //汇总每个ActorReplicator的数据
        float total_bits_write=0;

        //加上UDP的头部
        total_bits_write+=UDP_HEADER_SIZE;

        //加上每个ActorReplicator的数据
        float total_bits_write_actor_replicator=0;
        for(auto& iter:actor_replicator_map_){
            total_bits_write_actor_replicator+=iter.second->bits_write_;
        }
        total_bits_write+=total_bits_write_actor_replicator;

        //加上每个RPC的数据
        float total_bits_write_rpc=0;
        for(auto& iter:actor_rpc_map_){
            for(auto& rpc:iter.second){
                total_bits_write_rpc+=rpc->bits_write_;
            }
        }
        total_bits_write+=total_bits_write_rpc;

        str+="total_bits_write:"+std::to_string(total_bits_write);
        str+="\n-----------------------------\n";

        //列出UDP的头部
        str+="udp_head:"+std::to_string(UDP_HEADER_SIZE);

        if(include_actor_replicator){
            if(total_bits_write_actor_replicator>0){
                str+="\n--------------replicator:" + std::to_string(total_bits_write_actor_replicator) + "---------------\n";
            }

            //列出每个ActorReplicator的数据
            for(auto& iter:actor_replicator_map_){
                str+=iter.first+" bits_write:"+std::to_string(iter.second->bits_write_)+"\n";
                //列出每个ActorReplicator的属性名字->写入的字节数，并将字节数保留2位小数
                if(include_actor_replicator_detail){
                    for(auto& name:iter.second->property_map_){
                        std::ostringstream oss;
                        oss << std::fixed << std::setprecision(2) << name.second;  // 设置精度为 2

                        str+="  "+name.first+" :" + oss.str() +"\n";
                    }
                }
            }
        }

        if(include_actor_rpc){
            if(actor_rpc_map_.size()>0){
                str+="\n--------------rpc:" + std::to_string(total_bits_write_rpc) + "---------------\n";
            }

            //列出每个RPC的数据
            for(auto& iter:actor_rpc_map_){
                //汇总当前Actor的RPC的数据
                float total_bits_write_rpc=0;
                for(auto& rpc:iter.second){
                    total_bits_write_rpc+=rpc->bits_write_;
                }

                str+=iter.first+": "+std::to_string(total_bits_write_rpc)+"\n";
                for(auto& rpc:iter.second){
                    str+="  "+rpc->function_name_+" bits_write:"+std::to_string(rpc->bits_write_)+"\n";
                }
            }
        }

        if(include_actor_replicator_drop){
            if(actor_replicator_drop_names_.size()>0){
                str+="\n--------------replicator drop---------------\n";
            }

            //列出丢弃的Actor名字
            for(auto& name:actor_replicator_drop_names_){
                str+=name+"\n";
            }
        }


        return str;
    }
};

class LogTrackNetDataOneFrame{
public:
    std::string frame_time_;//这一帧的时间,这里填的是这一帧的开始时间，一帧可能跨越多个时间
    std::map<std::string, LogTrackNetDataOneConnection*> connection_data_map_;//每个Connection的数据
};

class LogTrackNetData{
public:
    std::map<int, LogTrackNetDataOneFrame*> frame_data_map_;//每一帧的网络数据
};

class App {
public:
    void Init(GLFWwindow* window);
    void ResetData();
    void ParseLogFile(const std::string& file_path);
    void ParseLogLines(const std::vector<std::string>& lines);
private:
    LogTrackNetData* log_track_net_data_= nullptr;//网络数据
    std::vector<std::string> connection_names;//连接名字
    std::map<std::string, std::vector<float>> connection_queued_bits_data_map;//连接名字和带宽余量数据
    std::map<std::string, std::vector<float>> connection_replicate_actor_bits_write_data_map;//属性同步 连接名字和写入数据
    std::map<std::string, std::vector<float>> connection_rpc_bits_write_data_map;//RPC 连接名字和写入数据

    float max_queued_bits=FLT_MIN;
    float min_queued_bits=FLT_MAX;

    float max_replicate_actor_bits_write=FLT_MIN;
    float min_replicate_actor_bits_write=FLT_MAX;

    float max_rpc_bits_write=FLT_MIN;
    float min_rpc_bits_write=FLT_MAX;

    std::map<std::string,std::vector<std::string>> connection_queued_bits_data_show_tips_map;//QueuedBits柱状图 连接名字和显示的提示
    std::map<std::string,std::vector<std::string>> connection_replicate_actor_bits_write_data_show_tips_map;//属性同步BitsWrite柱状图 连接名字和显示的提示
    std::map<std::string,std::vector<std::string>> connection_replicate_actor_bits_write_data_short_tips_map;//属性同步BitsWrite柱状图 连接名字和显示的简要提示
    std::map<std::string,std::vector<std::string>> connection_rpc_bits_write_data_show_tips_map;//RPCBitsWrite柱状图 连接名字和显示的提示

    std::string hover_tip_;//鼠标悬停的提示

public:
    static App* GetInstance();
private:
    static App* instance_;
};


#endif //EXAMPLE_PROFILER_APP_APP_H
