#ifndef INDEX_HANDLER_H
#define INDEX_HANDLER_H

#include "ix.h"
#include "../utils/var_type.h"
#include <cstring>
#include <vector>

class IndexScan;

//使用IndexHandler时请注意
//及时关闭索引，如果在BufManager进行close()后使用必须重新打开
class IndexHandler{ 
public:
    IndexHandler(BufManager* _bm) { 
        bm = _bm; 
        keyFile = new RecordHandler(bm); 
        treeFile = new IndexFileHandler(bm);
        nowdata = new char[MAX_RECORD_LEN];
    }
    ~IndexHandler() {
        delete[] nowdata;
        delete keyFile;
        delete treeFile;
    }
    void openIndex(std::string tableName, std::string colName, VarType type, int len=0);
    void insert(key_ptr key, RID rid);
    void remove(key_ptr key, RID rid);
    bool has(key_ptr key);
    int count(key_ptr key);
    IndexScan begin();
    IndexScan lesserBound(key_ptr key); //Strictly
    IndexScan greaterBound(key_ptr key); //Strictly
    IndexScan lowerBound(key_ptr key); //Weakly
    IndexScan upperBound(key_ptr key); //Weakly
    std::vector<RID> getRIDs(key_ptr key);
    int totalCount();
    void closeIndex();
    void removeIndex(std::string tableName, std::string colName);
    void removeIndex();
    // void debug();
    friend class IndexScan;

private:
    string tableName, colName;
    VarType type;

    IndexFileHandler* treeFile;
    RecordHandler* keyFile;
    BufManager* bm;
    char* nowdata;

    void insertIntoNonFullPage(key_ptr key, RID rid, int pageID); 
    void splitPage(BPlusNode* node, int index); 
    void insertIntoOverflowPage(key_ptr key, RID rid, BPlusNode* fatherPage, int x);

    void deleteFromLegalPage(key_ptr key, RID rid, int pageID);
    void mergePage(BPlusNode* node, int index);  //合并node上index和index+1号节点
    void borrowFromBackward(BPlusNode* node, int index);
    void borrowFromForward(BPlusNode* node, int index);
    void deleteFromOverflowPage(key_ptr key, RID rid, BPlusNode* fatherPage, int x);

    int getCountIn(int pageID, key_ptr key);
    IndexScan getLowerBound(int pageID, key_ptr key);

    inline std::string getKeyFilename() {return tableName + colName + ".tree";}
    inline std::string getTreeFilename() {return tableName + colName + ".key";}

};

class IndexScan{
public:
    IndexScan(IndexHandler *ih):tree(ih),currentNodeId(0){
        currentCumulation = 0;
        currentOverflowPage = nullptr;
        currentOverflowPageId = 0;
        currentNode = nullptr;
    }
    IndexScan(IndexHandler *ih, BPlusNode* bn, int keyn, int valn):tree(ih), currentNode(bn), currentKeyPos(keyn), currentValuePos(valn)
    {
        currentNodeId = bn->pageId;
        currentOverflowPageId = 0;
        currentOverflowPage = nullptr;
    }

    void revaildate();
    int getKey(char*);
    RID getValue();
    void next();
    void previous();
    void setToBegin();
    void setToEnd();
    bool equals(const IndexScan &other);
    inline bool available(){return currentNodeId>0;}
    void nextKey();
    void previousKey();

private:
    IndexHandler* tree;
    BPlusNode* currentNode;
    BPlusOverflowPage* currentOverflowPage;
    int currentNodeId;
    int currentOverflowPageId;
    int currentKeyPos, currentValuePos, currentCumulation;
};

#endif
