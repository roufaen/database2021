# include "query_manager.h"

QueryManager::QueryManager(IndexHandler *indexHandler, SystemManager *systemManager, BufManager *bufManager) {
    this->indexHandler = indexHandler;
    this->systemManager = systemManager;
    this->bufManager = bufManager;
}

QueryManager::~QueryManager() {
}

int QueryManager::exeSelect(vector <string> tableNameList, vector <pair <pair <string, string>, string> > selectorList, vector <Condition> conditionList, bool aggregation, vector <vector <Data> >& resData) {
    vector <Table*> tableList;
    vector <TableHeader> jointHeaderList;
    vector <vector <RID> > ridLists;
    string dbName = this->systemManager->getDbName();
    long long totNum = 1;

    for (int i = 0; i < (int)tableNameList.size(); i++) {
        // 判断 table 是否存在
        if (!this->systemManager->hasTable(tableNameList[i])) {
            cerr << "Table " << tableNameList[i] << " doesn't exist. Operation failed." << endl;
            return -1;
        }
    }

    for (int i = 0; i < (int)selectorList.size(); i++) {
        if (aggregation == true && selectorList[i].second == "COUNT" && selectorList[i].first.first == "*") {
            continue;
        }
        // 判断 table 是否存在
        if (!this->systemManager->hasTable(selectorList[i].first.first)) {
            cerr << "Table " << tableNameList[i] << " doesn't exist. Operation failed." << endl;
            return -1;
        }
        // 判断 header 是否存在
        Table *table = this->systemManager->getTable(selectorList[i].first.first);
        vector <TableHeader> headerList = table->getHeaderList();
        bool headerFind = false;
        for (int j = 0; j < (int)headerList.size(); j++) {
            if (headerList[j].headerName == selectorList[i].first.second) {
                headerFind = true;
            }
        }
        if (headerFind == false) {
            cerr << "Column " << selectorList[i].first.second << " doesn't exist. Operation failed." << endl;
            return -1;
        }
        // 判断 aggregator 是否合法
        if (aggregation == true && selectorList[i].second != "SUM" && selectorList[i].second != "AVG" && selectorList[i].second != "MIN" && selectorList[i].second != "MAX") {
            cerr << "Aggregator " << selectorList[i].second << " doesn't exist. Operation failed." << endl;
            return -1;
        }
    }

    // 解决 INT 和 FLOAT 的区别问题
    for (int i = 0; i < (int)conditionList.size(); i++) {
        Table *leftTable = this->systemManager->getTable(conditionList[i].leftTableName);
        vector <TableHeader> leftTableHeader = leftTable->getHeaderList();
        for (int j = 0; j < (int)leftTableHeader.size(); j++) {
            if (conditionList[i].leftCol == leftTableHeader[j].headerName && conditionList[i].rightType == INT && leftTableHeader[j].varType == FLOAT) {
                conditionList[i].rightType = FLOAT;
                conditionList[i].rightFloatVal = conditionList[i].rightIntVal;
                break;
            }
        }
    }

    // 获取用到的所有 table ，其 header 进行连接
    for (int i = 0; i < (int)tableNameList.size(); i++) {
        Table *table = this->systemManager->getTable(tableNameList[i]);
        tableList.push_back(table);
        vector <TableHeader> headerList = table->getHeaderList();
        jointHeaderList.insert(jointHeaderList.end(), headerList.begin(), headerList.end());
        vector <RID> ridList = table->getRecordList(), conditionRidList;
        for (int j = 0; j < (int)conditionList.size(); j++) {
            // 用索引判断
            for (int k = 0; k < (int)headerList.size(); k++) {
                if (conditionList[j].useColumn == false && conditionList[j].rightNull == false && (headerList[k].isPrimary == true || headerList[k].isUnique == true || headerList[k].hasIndex == true)
                    && conditionList[j].leftTableName == headerList[k].tableName && conditionList[j].leftCol == headerList[k].headerName) {
                    key_ptr key = NULL;
                    char str[MAX_RECORD_LEN];
                    if ((conditionList[j].rightType == INT && headerList[k].varType == INT) || (conditionList[j].rightType == DATE && headerList[k].varType == DATE)) {
                        this->indexHandler->openIndex("index_" + this->systemManager->getDbName() + "_" + headerList[k].tableName, conditionList[j].leftCol, INT);
                        key = (char*)&conditionList[j].rightIntVal;
                    } else if (conditionList[j].rightType == FLOAT && headerList[k].varType == FLOAT) {
                        this->indexHandler->openIndex("index_" + this->systemManager->getDbName() + "_" + headerList[k].tableName, conditionList[j].leftCol, FLOAT);
                        key = (char*)&conditionList[j].rightFloatVal;
                    } else if ((conditionList[j].rightType == CHAR || conditionList[j].rightType == VARCHAR) && (headerList[k].varType == CHAR || headerList[k].varType == VARCHAR)) {
                        this->indexHandler->openIndex("index_" + this->systemManager->getDbName() + "_" + headerList[k].tableName, conditionList[j].leftCol, VARCHAR);
                        memcpy(str, conditionList[j].rightStringVal.c_str(), conditionList[j].rightStringVal.size());
                        key = str;
                    }
                    conditionRidList.clear();
                    if (conditionList[j].condType == EQUAL) {
                        conditionRidList = this->indexHandler->getRIDs(key);
                    } else {
                        if (conditionList[j].condType == LESS_EQUAL) {
                            IndexScan indexScanner = this->indexHandler->greaterBound(key);
                            indexScanner.available() ? indexScanner.previous() : indexScanner.setToEnd();
                            while(indexScanner.available()) {
                                conditionRidList.push_back(indexScanner.getValue());
                                indexScanner.previous();
                            }
                        } else if (conditionList[j].condType == LESS) {
                            IndexScan indexScanner = this->indexHandler->upperBound(key);
                            indexScanner.available() ? indexScanner.previous() : indexScanner.setToEnd();
                            while(indexScanner.available()) {
                                conditionRidList.push_back(indexScanner.getValue());
                                indexScanner.previous();
                            }
                        } else if (conditionList[j].condType == GREATER_EQUAL) {
                            IndexScan indexScanner = this->indexHandler->upperBound(key);
                            while(indexScanner.available()) {
                                conditionRidList.push_back(indexScanner.getValue());
                                indexScanner.next();
                            }
                        } else if (conditionList[j].condType == GREATER) {
                            IndexScan indexScanner = this->indexHandler->greaterBound(key);
                            while(indexScanner.available()) {
                                conditionRidList.push_back(indexScanner.getValue());
                                indexScanner.next();
                            }
                        }
                    }
                    indexHandler->closeIndex();
                    ridList = conditionRidList.size() < ridList.size() ? conditionRidList : ridList;
                }
            }
            if (conditionList[j].leftTableName == tableNameList[i] && (conditionList[j].useColumn == false || conditionList[j].rightTableName == tableNameList[i])) {
                conditionRidList.clear();
                for (int k = 0; k < (int)ridList.size(); k++) {
                    vector <Data> dataList = table->exeSelect(ridList[k]);
                    vector <Condition> conditionContainer;
                    conditionContainer.push_back(conditionList[j]);
                    if (conditionJudge(headerList, dataList, conditionContainer)) {
                        conditionRidList.push_back(ridList[k]);
                    }
                }
                ridList = conditionRidList.size() < ridList.size() ? conditionRidList : ridList;
            }
        }
        ridLists.push_back(ridList);
        totNum *= ridList.size();
    }

    // data 进行连接
    vector <int> selectPos;
    vector <Data> aggregationRes;
    long long counter = 0;
    for (int i = 0; i < (int)selectorList.size(); i++) {
        Data aggregationDataInit = { 0, 0, 0.0, "", INT, false };
        if (aggregation == true && selectorList[i].second == "COUNT" && selectorList[i].first.first == "*") {
            selectPos.push_back(-1);
        } else {
            for (int j = 0; j < (int)jointHeaderList.size(); j++) {
                if (selectorList[i].first.first == jointHeaderList[j].tableName && selectorList[i].first.second == jointHeaderList[j].headerName) {
                    selectPos.push_back(j);
                    if (aggregation == true && jointHeaderList[j].varType == INT) {
                        aggregationDataInit.varType = INT;
                    } else if (aggregation == true && jointHeaderList[j].varType == FLOAT) {
                        aggregationDataInit.varType = FLOAT;
                    } else if (aggregation == true && (jointHeaderList[j].varType == CHAR || jointHeaderList[j].varType == VARCHAR) && (selectorList[i].second != "AVG" || selectorList[i].second != "SUM")) {
                        aggregationDataInit.varType = VARCHAR;
                    } else if (aggregation == true) {
                        cerr << "Data type and aggregator mismatch. Operation failed." << endl;
                        return -1;
                    }
                }
            }
        }
        if (aggregation == true) {
            // 用 refCount 暂存 aggregator 类型
            if (selectorList[i].second == "COUNT") {
                aggregationDataInit.refCount = 0;
                aggregationDataInit.intVal = 0;
            } else if (selectorList[i].second == "AVG") {
                aggregationDataInit.refCount = 1;
                aggregationDataInit.intVal = 0;
            } else if (selectorList[i].second == "SUM") {
                aggregationDataInit.refCount = 2;
                aggregationDataInit.intVal = 0;
            } else if (selectorList[i].second == "MIN") {
                aggregationDataInit.refCount = 3;
                aggregationDataInit.intVal = 2147483647;
                aggregationDataInit.floatVal = DBL_MAX;
                aggregationDataInit.stringVal = "\127";
            } else if (selectorList[i].second == "MAX") {
                aggregationDataInit.refCount = 4;
                aggregationDataInit.intVal = -2147483648;
                aggregationDataInit.floatVal = -DBL_MAX;
                aggregationDataInit.stringVal = "\000";
            }
            aggregationRes.push_back(aggregationDataInit);
        }
    }
    for (long long i = 0; i < totNum; i++) {
        vector <Data> jointData;
        for (long long j = 0, tmp = i; j < (int)ridLists.size(); j++) {
            vector <Data> data = tableList[j]->exeSelect(ridLists[j][tmp % ridLists[j].size()]);
            jointData.insert(jointData.end(), data.begin(), data.end());
            tmp /= ridLists[j].size();
        }
        // 判断是否满足选择条件，若是则选择要求的列输出
        if (conditionJudge(jointHeaderList, jointData, conditionList)) {
            counter++;
            vector <Data> res;
            if (aggregation == false) {
                for (int j = 0; j < (int)selectPos.size(); j++) {
                    res.push_back(jointData[selectPos[j]]);
                }
                resData.push_back(res);
            } else {
                for (int j = 0; j < (int)selectPos.size(); j++) {
                    if (aggregationRes[j].refCount == 0) {
                        if (selectPos[j] == -1) {
                            aggregationRes[j].intVal++;
                        } else {
                            aggregationRes[j].intVal += !jointData[selectPos[j]].isNull;
                        }
                    } else if (aggregationRes[j].refCount == 1 || aggregationRes[j].refCount == 2) {
                        aggregationRes[j].intVal += jointData[selectPos[j]].intVal;
                        aggregationRes[j].floatVal += jointData[selectPos[j]].floatVal;
                    } else if (aggregationRes[j].refCount == 3) {
                        aggregationRes[j].intVal = jointData[selectPos[j]].intVal < aggregationRes[j].intVal ? jointData[selectPos[j]].intVal : aggregationRes[j].intVal;
                        aggregationRes[j].floatVal = jointData[selectPos[j]].floatVal < aggregationRes[j].floatVal ? jointData[selectPos[j]].floatVal : aggregationRes[j].floatVal;
                        aggregationRes[j].stringVal = jointData[selectPos[j]].stringVal < aggregationRes[j].stringVal ? jointData[selectPos[j]].stringVal : aggregationRes[j].stringVal;
                    } else if (aggregationRes[j].refCount == 4) {
                        aggregationRes[j].intVal = jointData[selectPos[j]].intVal > aggregationRes[j].intVal ? jointData[selectPos[j]].intVal : aggregationRes[j].intVal;
                        aggregationRes[j].floatVal = jointData[selectPos[j]].floatVal > aggregationRes[j].floatVal ? jointData[selectPos[j]].floatVal : aggregationRes[j].floatVal;
                        aggregationRes[j].stringVal = jointData[selectPos[j]].stringVal > aggregationRes[j].stringVal ? jointData[selectPos[j]].stringVal : aggregationRes[j].stringVal;
                    }
                }
            }
        }
    }
    if (aggregation == true) {
        for (int i = 0; i < (int)aggregationRes.size(); i++) {
            if (aggregationRes[i].varType == 1) {
                if (aggregationRes[i].varType == INT) {
                    aggregationRes[i].floatVal = aggregationRes[i].intVal * 1.0 / counter;
                } else {
                    aggregationRes[i].floatVal = aggregationRes[i].floatVal / counter;
                }
            }
        }
        resData.push_back(aggregationRes);
    }

    return 0;
}

int QueryManager::exeInsert(string tableName, vector <Data> dataList) {
    RID rid;
    return exeInsert(tableName, dataList, rid);
}

int QueryManager::exeInsert(string tableName, vector <Data> dataList, RID& rid) {
    // 判断 table 是否存在
    if (!this->systemManager->hasTable(tableName)) {
        cerr << "Table " << tableName << " doesn't exist. Operation failed." << endl;
        return -1;
    }
    Table *table = this->systemManager->getTable(tableName);
    vector <TableHeader> headerList = table->getHeaderList();
    vector <RID> ridList = table->getRecordList();
    vector <vector <Data> > dataLists;
    int maxGroup = -1;
    // 数据长度不符
    if (dataList.size() != headerList.size()) {
        cerr << "Data list size incorrect. Operation failed." << endl;
        return -1;
    }
    for (int i = 0; i < (int)headerList.size(); i++) {
        // 是否非法空值
        if (dataList[i].isNull == true && (headerList[i].permitNull == false || headerList[i].isPrimary == true)) {
            cerr << "Column " << headerList[i].headerName << " illegal NULL. Operation failed." << endl;
            return -1;
        // 是否类型不符
        } else if (dataList[i].isNull == false && dataList[i].varType != headerList[i].varType && !((dataList[i].varType == CHAR || dataList[i].varType == VARCHAR) && (headerList[i].varType == CHAR || headerList[i].varType == VARCHAR))) {
            if (dataList[i].varType == INT && headerList[i].varType == FLOAT) {
                dataList[i].varType = FLOAT;
                dataList[i].floatVal = dataList[i].intVal;
            } else {
                cerr << "Column " << headerList[i].headerName<< " data type mismatch. Operation failed." << endl;
                return -1;
            }
        // 字符串长度是否非法
        } else if ((headerList[i].varType == CHAR || headerList[i].varType == VARCHAR) && (int)dataList[i].stringVal.size() > headerList[i].len) {
            cerr << "String \"" << dataList[i].stringVal << "\" too long. Operation failed." << endl;
            return -1;
        }
        if (headerList[i].isUnique == true) {
            maxGroup = headerList[i].uniqueGroup > maxGroup ? headerList[i].uniqueGroup : maxGroup;
        }
    }
    for (int i = 0; i < (int)headerList.size(); i++) {
        if (headerList[i].isForeign == true && dataList[i].isNull == false) {
            // 外键不能引用不存在的键
            if (foreignKeyExistJudge(headerList[i], dataList[i]) == false) {
                if (headerList[i].varType == INT || headerList[i].varType == DATE) {
                    cerr << "Foreign key " << dataList[i].intVal << " doesn't exist. Operation failed." << endl;
                } else if (headerList[i].varType == FLOAT) {
                    cerr << "Foreign key " << dataList[i].floatVal << " doesn't exist. Operation failed." << endl;
                } else if (headerList[i].varType == CHAR || headerList[i].varType == VARCHAR) {
                    cerr << "Foreign key " << dataList[i].stringVal << " doesn't exist. Operation failed." << endl;
                }
                return -1;
            }
        }
    }
    // 判断是否满足 unique 要求
    vector <string> judgeHeaderList;
    vector <Data> judgeDataList;
    for (int i = 0; i <= maxGroup; i++) {
        for (int j = 0; j < (int)headerList.size(); j++) {
            if (headerList[j].isUnique == true && headerList[j].uniqueGroup == i) {
                judgeHeaderList.push_back(headerList[j].headerName);
                judgeDataList.push_back(dataList[j]);
            }
        }
        if (judgeUnique(tableName, judgeHeaderList, judgeDataList) == false) {
            cerr << "Data don't satisfy unique requirement." << endl;
            return -1;
        }
        judgeHeaderList.clear();  judgeDataList.clear();
    }
    for (int i = 0; i < (int)headerList.size(); i++) {
        if (headerList[i].isPrimary == true) {
            judgeHeaderList.push_back(headerList[i].headerName);
            judgeDataList.push_back(dataList[i]);
        }
    }
    if (judgeUnique(tableName, judgeHeaderList, judgeDataList) == false) {
        cerr << "Data don't satisfy unique requirement." << endl;
        return -1;
    }

    // 插入数据和索引
    rid = this->systemManager->opInsert(tableName, dataList);

    return 0;
}

int QueryManager::exeDelete(string tableName, vector <Condition> conditionList) {
    // 判断 table 是否存在
    if (!this->systemManager->hasTable(tableName)) {
        cerr << "Table " << tableName << " doesn't exist. Operation failed." << endl;
        return -1;
    }
    Table *table = this->systemManager->getTable(tableName);
    vector <TableHeader> headerList = table->getHeaderList();
    vector <RID> ridList = table->getRecordList();

    for (int i = 0; i < (int)conditionList.size(); i++) {
        if (conditionList[i].useColumn == false) {
            Table *leftTable = this->systemManager->getTable(tableName);
            vector <TableHeader> leftTableHeader = leftTable->getHeaderList();
            for (int j = 0; j < (int)leftTableHeader.size(); j++) {
                if (conditionList[i].leftCol == leftTableHeader[j].headerName && conditionList[i].rightType == INT && leftTableHeader[j].varType == FLOAT) {
                    conditionList[i].rightType = FLOAT;
                    conditionList[i].rightFloatVal = conditionList[i].rightIntVal;
                    break;
                }
            }
        }
    }

    // 主键不能被引用，否则无法删除
    for (int i = 0; i < (int)ridList.size(); i++) {
        vector <Data> dataList = table->exeSelect(ridList[i]);
        if (conditionJudge(headerList, dataList, conditionList)) {
            for (int j = 0; j < (int)dataList.size(); j++) {
                if (headerList[j].isPrimary == true && dataList[j].refCount != 0) {
                    cerr << "Column " << headerList[j].headerName << " has elements referenced. Operation failed." << endl;
                    return -1;
                }
            }
        }
    }

    // 删除数据和索引
    for (int i = 0; i < (int)ridList.size(); i++) {
        vector <Data> dataList = table->exeSelect(ridList[i]);
        if (conditionJudge(headerList, dataList, conditionList)) {
            this->systemManager->opDelete(tableName, dataList, ridList[i]);
        }
    }

    return 0;
}

int QueryManager::exeUpdate(string tableName, vector <string> updateHeaderNameList, vector <Data> originalUpdateDataList, vector <Condition> conditionList) {
    // 判断 table 是否存在
    if (!this->systemManager->hasTable(tableName)) {
        cerr << "Table " << tableName << " doesn't exist. Operation failed." << endl;
        return -1;
    }
    // 判断输入数据格式是否正确
    if (updateHeaderNameList.size() != originalUpdateDataList.size()) {
        cerr << "Input data format error. Operation failed." << endl;
        return -1;
    }
    Table *table = this->systemManager->getTable(tableName);
    vector <TableHeader> headerList = table->getHeaderList();
    vector <RID> ridList = table->getRecordList();
    vector <int> updatePos;
    vector <Data> updateDataList;

    for (int i = 0; i < (int)conditionList.size(); i++) {
        if (conditionList[i].useColumn == false) {
            Table *leftTable = this->systemManager->getTable(tableName);
            vector <TableHeader> leftTableHeader = leftTable->getHeaderList();
            for (int j = 0; j < (int)leftTableHeader.size(); j++) {
                if (conditionList[i].leftCol == leftTableHeader[j].headerName && conditionList[i].rightType == INT && leftTableHeader[j].varType == FLOAT) {
                    conditionList[i].rightType = FLOAT;
                    conditionList[i].rightFloatVal = conditionList[i].rightIntVal;
                    break;
                }
            }
        }
    }

    // 预处理
    int updateCount = 0;
    for (int i = 0; i < (int)headerList.size(); i++) {
        int updateHere = 0;
        for (int j = 0; j < (int)updateHeaderNameList.size(); j++) {
            if (headerList[i].headerName == updateHeaderNameList[j] && updateHere == 0) {
                updateHere = 1;
                updatePos.push_back(1);
                updateDataList.push_back(originalUpdateDataList[j]);
                updateCount++;
            // 判断是否重复修改某些列的值
            } else if (headerList[i].headerName == updateHeaderNameList[j] && updateHere == 1) {
                cerr << "Input data format error. Operation failed." << endl;
                return -1;
            }
        }
        if (updateHere == 0) {
            updatePos.push_back(0);
            updateDataList.push_back(originalUpdateDataList[0]);
        }
    }
    // 要求修改的某些列不存在
    if (updateCount < (int)updateHeaderNameList.size()) {
        cerr << "Input data format error. Operation failed." << endl;
        return -1;
    }

    // 更新的数据本身是否存在问题
    for (int i = 0; i < (int)headerList.size(); i++) {
        // 是否非法空值
        if (updatePos[i] == 1 && updateDataList[i].isNull == true && (headerList[i].permitNull == false || headerList[i].isPrimary == true)) {
            cerr << "Column " << headerList[i].headerName << " illegal NULL. Operation failed." << endl;
            return -1;
        // 是否类型不符
        } else if (updateDataList[i].isNull == false && updatePos[i] == 1 && updateDataList[i].varType != headerList[i].varType && !((updateDataList[i].varType == CHAR || updateDataList[i].varType == VARCHAR) && (headerList[i].varType == CHAR || headerList[i].varType == VARCHAR))) {
            if (updateDataList[i].varType == INT && headerList[i].varType == FLOAT) {
                updateDataList[i].varType = FLOAT;
                updateDataList[i].floatVal = updateDataList[i].intVal;
            } else {
                cerr << "Column " << headerList[i].headerName<< " data type mismatch. Operation failed." << endl;
                return -1;
            }
        // 字符串长度是否非法
        } else if (updatePos[i] == 1 && headerList[i].varType == CHAR && (int)updateDataList[i].stringVal.size() > headerList[i].len) {
            cerr << "String \"" << updateDataList[i].stringVal << "\" too long. Operation failed." << endl;
            return -1;
        }
    }

    // 寻找符合条件的数据
    vector <vector <Data> > dataLists, originalDataLists;
    vector <RID> updateRidList, updatedRidList;
    for (int i = 0; i < (int)ridList.size(); i++) {
        vector <Data> dataList = table->exeSelect(ridList[i]);
        if (conditionJudge(headerList, dataList, conditionList)) {
            for (int j = 0; j < (int)headerList.size(); j++) {
                if (updatePos[j] == 1 && headerList[j].isPrimary == true && dataList[j].refCount != 0) {
                    cerr << "Column " << headerList[j].headerName << " has elements referenced. Operation failed." << endl;
                    return -1;
                }
            }
            dataLists.push_back(dataList);
            originalDataLists.push_back(dataList);
            updateRidList.push_back(ridList[i]);
        }
    }

    // 更新数据
    for (int i = 0; i < (int)updateRidList.size(); i++) {
        this->systemManager->opDelete(tableName, dataLists[i], updateRidList[i]);
        for (int j = 0; j < (int)updateDataList.size(); j++) {
            if (updatePos[j] == 1) {
                dataLists[i][j] = updateDataList[j];
            }
        }
    }
    for (int i = 0; i < (int)updateRidList.size(); i++) {
        RID rid;
        if (exeInsert(tableName, dataLists[i], rid) == 0) {
            updatedRidList.push_back(rid);
        } else {
            for (int k = 0; k < (int)updatedRidList.size(); k++) {
                this->systemManager->opDelete(tableName, dataLists[k], updatedRidList[k]);
            }
            for (int k = 0; k < (int)updateRidList.size(); k++) {
                this->systemManager->opInsert(tableName, originalDataLists[k]);
            }
            return -1;
        }
    }

    return 0;
}

bool QueryManager::compare(int lInt, double lFloat, string lString, int rInt, double rFloat, string rString, ConditionType cond) {
    bool intEqual = (lInt == rInt), floatEqual = (lFloat == rFloat), stringEqual = (lString == rString);
    bool intNotEqual = (lInt != rInt), floatNotEqual = (lFloat != rFloat), stringNotEqual = (lString != rString);
    bool intLess = (lInt < rInt), floatLess = (lFloat < rFloat), stringLess = (lString < rString);
    bool intLessEqual = (lInt <= rInt), floatLessEqual = (lFloat <= rFloat), stringLessEqual = (lString <= rString);
    bool intGreater = (lInt > rInt), floatGreater = (lFloat > rFloat), stringGreater = (lString > rString);
    bool intGreaterEqual = (lInt >= rInt), floatGreaterEqual = (lFloat >= rFloat), stringGreaterEqual = (lString >= rString);
    // 对其中一种数据类型判断时，输入的其余数据均相等
    if (cond == EQUAL) {
        return intEqual && floatEqual && stringEqual;
    } else if (cond == NOT_EQUAL) {
        return intNotEqual || floatNotEqual || stringNotEqual;
    } else if (cond == LESS) {
        return intLess || floatLess || stringLess;
    } else if (cond == LESS_EQUAL) {
        return intLessEqual && floatLessEqual && stringLessEqual;
    } else if (cond == GREATER) {
        return intGreater || floatGreater || stringGreater;
    } else if (cond == GREATER_EQUAL) {
        return intGreaterEqual && floatGreaterEqual && stringGreaterEqual;
    }
    return false;
}

bool QueryManager::conditionJudge(vector <TableHeader> headerList, vector <Data> dataList, vector <Condition> conditionList) {
    Data leftData, rightData;
    bool leftDataFind = false, rightDataFind = false;
    // 对每个条件检查
    for (int i = 0; i < (int)conditionList.size(); i++) {
        // 寻找要求的列
        for (int j = 0; j < (int)headerList.size(); j++) {
            if (headerList[j].tableName == conditionList[i].leftTableName && headerList[j].headerName == conditionList[i].leftCol) {
                leftData = dataList[j];
                leftDataFind = true;
            }
            if (headerList[j].tableName == conditionList[i].rightTableName && headerList[j].headerName == conditionList[i].rightCol) {
                rightData = dataList[j];
                rightDataFind = true;
            }
        }

        // 未找到要求的列，判断失败
        if (leftDataFind == false) {
            return false;
        }

        // char 和 varchar 看作同一种类型
        leftData.varType = leftData.varType == VARCHAR ? CHAR : leftData.varType;
        if (conditionList[i].useColumn == false) {
            rightData.varType = conditionList[i].rightType == VARCHAR ? CHAR : conditionList[i].rightType;
            rightData.intVal = conditionList[i].rightIntVal;
            rightData.floatVal = conditionList[i].rightFloatVal;
            rightData.stringVal = conditionList[i].rightStringVal;
            rightData.isNull = conditionList[i].rightNull;
        } else {
            // 等号右侧的要求的列未找到，判断失败
            if (rightDataFind == false) {
                return false;
            }
            rightData.varType = rightData.varType == VARCHAR ? CHAR : rightData.varType;
        }

        // 两列均为空，只能判等不能比较
        if (leftData.isNull == 1 && rightData.isNull == 1) {
            if (conditionList[i].condType == EQUAL) {
                continue;
            } else {
                return false;
            }
        // 其中一列为空，只能判等不能比较
        } else if ((leftData.isNull ^ rightData.isNull) == 1) {
            if (conditionList[i].condType == NOT_EQUAL) {
                continue;
            } else {
                return false;
            }
        // 分不同的类型比较
        } else if ((leftData.varType == INT && rightData.varType == INT) || (leftData.varType == DATE && rightData.varType == DATE)) {
            if (!compare(leftData.intVal, 0, "", rightData.intVal, 0, "", conditionList[i].condType)) {
                return false;
            }
        } else if (leftData.varType == FLOAT && rightData.varType == FLOAT) {
            if (!compare(0, leftData.floatVal, "", 0, rightData.floatVal, "", conditionList[i].condType)) {
                return false;
            }
        } else if (leftData.varType == CHAR && rightData.varType == CHAR) {
            if (!compare(0, 0, leftData.stringVal, 0, 0, rightData.stringVal, conditionList[i].condType)) {
                return false;
            }
        // 类型不同，判断失败
        } else {
            return false;
        }
    }

    // 通过所有条件，判断成功
    return true;
}

bool QueryManager::foreignKeyExistJudge(TableHeader header, Data data) {
    Table *foreignTable = this->systemManager->getTable(header.foreignTableName);
    vector <TableHeader> foreignHeaderList = foreignTable->getHeaderList();
    vector <RID> foreignRidList = foreignTable->getRecordList();
    int refPos = -1;
    for (int j = 0; j < (int)foreignHeaderList.size(); j++) {
        if (header.foreignHeaderName == foreignHeaderList[j].headerName) {
            refPos = j;
        }
    }

    for (int j = 0; j < (int)foreignRidList.size(); j++) {
        vector <Data> foreignDataList = foreignTable->exeSelect(foreignRidList[j]);
        if ((header.varType == INT || header.varType == DATE) && data.intVal == foreignDataList[refPos].intVal && foreignDataList[refPos].isNull == false) {
            return true;
        } else if (header.varType == FLOAT && data.floatVal == foreignDataList[refPos].floatVal && foreignDataList[refPos].isNull == false) {
            return true;
        } else if ((header.varType == CHAR || header.varType == VARCHAR) && data.stringVal == foreignDataList[refPos].stringVal && foreignDataList[refPos].isNull == false) {
            return true;
        }
    }

    return false;
}

bool QueryManager::judgeUnique(string tableName, vector <string> judgeHeaderList, vector <Data> judgeDataList) {
    Table *table = this->systemManager->getTable(tableName);
    vector <TableHeader> headerList = table->getHeaderList();
    map <int, int> ridMap;
    for (int i = 0; i < (int)judgeHeaderList.size(); i++) {
        for (int j = 0; j < (int)headerList.size(); j++) {
            if (judgeHeaderList[i] == headerList[j].headerName) {
                VarType indexType = headerList[j].varType == DATE ? INT : (headerList[j].varType == CHAR ? VARCHAR : headerList[j].varType);
                indexHandler->openIndex("index_" + this->systemManager->getDbName() + "_" + tableName, headerList[j].headerName, indexType);
                vector <RID> ridList;
                if (indexType == INT) {
                    ridList = indexHandler->getRIDs((char*)&judgeDataList[i].intVal);
                } else if (indexType == FLOAT) {
                    ridList = indexHandler->getRIDs((char*)&judgeDataList[i].floatVal);
                } else {
                    char str[MAX_RECORD_LEN];
                    memcpy(str, judgeDataList[i].stringVal.c_str(), judgeDataList[i].stringVal.size());
                    ridList = indexHandler->getRIDs(str);
                }
                indexHandler->closeIndex();
                for (int k = 0; k < (int)ridList.size(); k++) {
                    if (ridMap.count(ridList[k].pageID * MAX_RECORD_LEN + ridList[k].slotID) == 0) {
                        ridMap[ridList[k].pageID * MAX_RECORD_LEN + ridList[k].slotID] = 1;
                    } else {
                        ridMap[ridList[k].pageID * MAX_RECORD_LEN + ridList[k].slotID]++;
                    }
                    if (ridMap[ridList[k].pageID * MAX_RECORD_LEN + ridList[k].slotID] == (int)judgeHeaderList.size()) {
                        return false;
                    }
                }
            }
        }
    }

    return true;
}
