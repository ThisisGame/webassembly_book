//
// Created by captainchen on 2025/1/6.
//

#ifndef EXAMPLE_PROFILER_APP_TREE_NODE_COLLECTION_H
#define EXAMPLE_PROFILER_APP_TREE_NODE_COLLECTION_H


#include <string>
#include <vector>
#include "tree_node.h"

class TreeNodeCollection {
public:
    void Clear() {
        // Clear
    }

    int size() {
        return 0;
    }

    TreeNode* Add(const std::string& Value) {
        // Add node
        return nullptr;
    }

    //重载[]
    TreeNode& operator[](int index) {
        return *Nodes[index];
    }

public:
    std::vector<TreeNode*> Nodes;
};


#endif //EXAMPLE_PROFILER_APP_TREE_NODE_COLLECTION_H
