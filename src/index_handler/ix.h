#ifndef IX_H
#define IX_H

#include "../utils/rid.h"
#include "index_file_handler.h"
#include "../record_handler/record_handler.h"
#include <memory>

typedef char* key_ptr;

const int maxIndexPerPage = 400;
const int maxIndexPerOVP = 1000;

namespace ix{
    enum NodeType {INTERNAL, LEAF, OVRFLOW};
    enum DataType {INT, FLOAT, VARCHAR};
}

struct IndexRecord{
    RID keyPos;
    RID value;  
    int count;
};
    //for leaf page: count>1 -> overflowpage id; count==1 -> RID
    //for intermediate page: pointer right to this page

struct BPlusNode{
    IndexRecord data[maxIndexPerPage];
    ix::NodeType nodeType;
    int pageId;
    int recs;
    int prevPage;
    int nextPage;   //-1 for nonexistent
};

struct BPlusOverflowPage{
    RID data[maxIndexPerOVP];
    ix::NodeType nodeType;
    int pageId;
    int recs;
    int prevPage;
    int nextPage;
    int fatherPage;
};

struct IndexFileHeader{
    int rootPageId;
    int pageCount;
    int firstLeaf;
    int lastLeaf;
    int sum;
};

#endif