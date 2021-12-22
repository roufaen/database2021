#include "index_handler.h"
#include <cstdio>
#include <iostream>
unsigned char MyBitMap::ha[]={0};
int main(){
    std::cout << sizeof(BPlusNode) << std::endl;
    MyBitMap::initConst();
    BufManager* bm = new BufManager();
    IndexHandler* ih = new IndexHandler(bm);
    ih->openIndex("db","col",INT);
    printf("Built\n");
    for(int i=0; i<600; i++){
        ih->insert((char*)&i, RID{i,i});
        printf("Inserted %d\n", i);
        //int index = 1;
        //if( i>=1 && ih->greaterCount((char*)&index) != i-1) break;
    }
    ih->closeIndex();
    ih->openIndex("db","col",INT);
    IndexScan indexScan = ih->begin();
    // char* nowdata = new char[MAX_RECORD_LEN];
    // for(;indexScan.available(); indexScan.next())
    // {
    //     indexScan.getKey(nowdata);
    //     cout << *((int*)nowdata) << " ";
    // }
    // cout << endl;
    // delete[] nowdata;
    int index = 1;
    printf("Is there any 1? %d\n", ih->count((char*)&index));
    index = 10;
    printf("Is there any 10? %d\n", ih->has((char*)&index));
    index = 1;
    ih->insert((char*)&index, RID{10,20});
    printf("How many 1? %d\n", ih->count((char*)&index));
    printf("How many bigger than 1? %d\n", ih->greaterCount((char*)&index));
    ih->closeIndex();
    return 0;
}