#include "MyVisitor.h"
#include <sstream>
#include <math.h>

VarType getVarType(SQLParser::Type_Context* tc, int& len){
    int type = tc->getAltNumber();
    switch(type){
        case 1: {
            len=4;
            return INT;
        }
        case 2: {
            len=getValue<int>(tc->Integer()->getText());
            return VARCHAR;
        }
        case 3: {
            len=4;
            return FLOAT;
        }
        case 4: {
            len=4;
            return DATE;
        }
    }
    len=0;
    return VARCHAR;
}

ConditionType getCondType(SQLParser::OperateContext* oc){
    if(oc->EqualOrAssign()) return ConditionType::EQUAL;
    if(oc->NotEqual()) return ConditionType::NOT_EQUAL;
    if(oc->Less()) return ConditionType::LESS;
    if(oc->LessEqual()) return ConditionType::LESS_EQUAL;
    if(oc->Greater()) return ConditionType::GREATER;
    if(oc->GreaterEqual()) return ConditionType::GREATER_EQUAL;
    if(oc->Is()) return ConditionType::IS;
    if(oc->IsNot()) return ConditionType::ISNOT;
    return ConditionType::IN;
}

template <class Type>  
Type getValue(const string& str){
    std::istringstream iss(str);  
    Type num;  
    iss >> num;  
    return num;  
}

bool getFromValue(Data& dt, SQLParser::ValueContext* data){
    dt.isNull = false;
    if(data->Float())
    { 
        dt.floatVal = getValue<float>(data->Float()->getText());
        dt.varType = FLOAT;
        return true;
    }
    if(data->Integer())
    { 
        dt.intVal = getValue<int>(data->Integer()->getText());
        dt.varType = INT;
        return true;
    }
    if(data->Date())
    {
        if(isDate(data->Date()->getText(), dt.intVal)) dt.varType = DATE;
        else {
            dt.stringVal = data->Date()->getText();
            dt.varType = VARCHAR;
        }
        return true;
    }
    if(data->String())
    { 
        dt.stringVal = data->String()->getText();
        int len = dt.stringVal.length();
        dt.stringVal = dt.stringVal.substr(1, len-2);
        dt.varType = VARCHAR;
        return true;
    }
    if(data->Null())
    { 
        dt.isNull = true;
        return true;
    }
    return false;
}

inline int max(int a, int b) {return a>b?a:b; }

inline int getNumLen(int a) {
    int sign = (a<0);
    int b = abs(a);
    if(b<10) return sign + 1;
    if(b<100) return sign + 2;
    if(b<1000) return sign + 3;
    if(b<10000) return sign + 4;
    if(b<100000) return sign + 5;
    if(b<1000000) return sign + 6;
    if(b<10000000) return sign + 7;
    if(b<100000000) return sign + 8;
    if(b<1000000000) return sign + 9;
    return sign + 10;
}

void print(const std::vector<std::pair<std::pair<std::string, std::string>, std::string>>& colList, const vector<vector<Data>>& data){
    vector<int> len;
    vector<std::string> colName;
    colName.clear();
    for(auto i:colList){
        std::string col = i.first.first + "." + i.first.second;
        if(col == "*.*") col = "*";
        if(i.second.length()) col = i.second + "(" + col + ")";
        len.push_back(col.length());
        colName.push_back(col);
    }
    for(auto dt:data){
        int index = 0;
        for(auto d:dt){
            if(d.isNull) len[index] = max(len[index], 4);
            else
            switch (d.varType)
            {
            case INT:
                len[index] = max(len[index], getNumLen(d.intVal));
                break;
            case FLOAT:
                {
                    double ftval = (d.floatVal < 0) ? -d.floatVal : d.floatVal;
                    len[index] = max(len[index], (ftval>999999?getNumLen(ftval):7)+(d.floatVal<0));
                }
                break;
            case VARCHAR:
            case CHAR:
                len[index] = max(len[index], d.stringVal.length());
                break;
            case DATE:
                len[index] = max(len[index],10);
                break;
            default:
                std::cerr << "ERROR TYPE IN PRINTING" << std::endl;
                break;
            }
            index++;
        }
    }
    int num_of_col = len.size();
    std::cout<<"+";
    for(int i=0; i<num_of_col; i++)
    {
        for(int j=0; j<len[i]; j++)
            std::cout<<"-";
       std::cout<<"+";
    }
    std::cout<<std::endl;
    std::cout<<"|";
    for(int i=0; i<num_of_col; i++)
    {
        std::cout << setw(len[i]) << setiosflags(ios::left) << colName[i];
        std::cout<<"|";
    }
    std::cout << std::endl;
    std::cout<<"+";
    for(int i=0; i<num_of_col; i++)
    {
        for(int j=0; j<len[i]; j++)
            std::cout<<"-";
       std::cout<<"+";
    }
    std::cout<<std::endl;
    for(auto dt:data){
        std::cout << "|";
        int index = 0;
        for(auto d:dt){
            if(d.isNull) std::cout << setw(len[index]) << setiosflags(ios::left) << "NULL";
            else
            switch (d.varType)
            {
            case INT:
                std::cout << setw(len[index]) << setiosflags(ios::left) << d.intVal;
                break;
            case DATE:
                {
                    string str = std::to_string(d.intVal/10000) + "-" ;
                    int mt = (d.intVal/100)%100;
                    if(mt<10) str += "0";
                    str += std::to_string((d.intVal/100)%100) + "-";
                    int dt = d.intVal % 100;
                    if(dt<10) str += "0";
                    str += std::to_string(d.intVal%100);
                    std::cout << setw(len[index]) << setiosflags(ios::left) << str;
                }
                break;
            case FLOAT:
                std::cout << setw(len[index]) << setiosflags(ios::left) << d.floatVal;
                break;
            case VARCHAR:
            case CHAR:
                std::cout << setw(len[index]) << setiosflags(ios::left) << d.stringVal;
                break;
            default:
                std::cerr << "ERROR TYPE IN PRINTING" << std::endl;
                break;
            }
            index++;
            std::cout << "|";
        }
        std::cout << std::endl;
    }
    std::cout<<"+";
    for(int i=0; i<num_of_col; i++)
    {
        for(int j=0; j<len[i]; j++)
            std::cout<<"-";
       std::cout<<"+";
    }
    std::cout<<std::endl;
}

void print(const vector<string>& tableName,  const vector<vector<string>>& data){
    auto t_i = tableName.begin();
    vector<int> len;
    while(t_i!=tableName.end()){
        len.push_back((*t_i).length());
        t_i++;
    }
    for(auto dt:data){
        int index = 0;
        for(auto d:dt){ 
            len[index] = max(len[index], d.length());
            index++;
        }
    }
    int num_of_col = tableName.size();
    std::cout<<"+";
    for(int i=0; i<num_of_col; i++)
    {
        for(int j=0; j<len[i]; j++)
            std::cout<<"-";
       std::cout<<"+";
    }
    std::cout<<std::endl;
    std::cout<<"|";
    for(int i=0; i<num_of_col; i++)
    {
        std::cout << setw(len[i]) << setiosflags(ios::left) <<tableName[i];
        std::cout<<"|";
    }
    std::cout << std::endl;
    std::cout<<"+";
    for(int i=0; i<num_of_col; i++)
    {
        for(int j=0; j<len[i]; j++)
            std::cout<<"-";
       std::cout<<"+";
    }
    std::cout<<std::endl;
    for(auto dt:data){
        std::cout << "|";
        int index = 0;
        for(auto d:dt){
            std::cout << setw(len[index]) << setiosflags(ios::left) << d;
            index++;
            std::cout << "|";
        }
        std::cout << std::endl;
    }
    std::cout<<"+";
    for(int i=0; i<num_of_col; i++)
    {
        for(int j=0; j<len[i]; j++)
            std::cout<<"-";
       std::cout<<"+";
    }
    std::cout<<std::endl;
}

void print(const string tableName, const vector<string>& data){
    if(!data.size()) {
        std::cout << "Sorry, but we found no " << tableName << "!" << std::endl;
        return;
    }
    int len = tableName.length();
    for(auto dt:data){ 
        len = max(len, dt.length());
    }
    std::cout<<"+";
    for(int j=0; j<len; j++)
        std::cout<<"-";
    std::cout<<"+";
    std::cout<<std::endl;
    std::cout<<"|";
    std::cout<<setw(len)<<(tableName);
    std::cout<<"|";
    std::cout << std::endl;
    std::cout<<"+";
    for(int j=0; j<len; j++)
        std::cout<<"-";
    std::cout<<"+";
    std::cout<<std::endl;
    for(auto dt:data){
        std::cout << "|" << setw(len) << setiosflags(ios::left) << dt;
        std::cout << "|" << std::endl;;
    }
    std::cout<<"+";
    for(int j=0; j<len; j++)
        std::cout<<"-";
    std::cout<<"+";
    std::cout<<std::endl;
}


bool isDate(string dateStr, int& date){
    int yr = getValue<int>(dateStr.substr(0, 4));
    int mt = getValue<int>(dateStr.substr(5, 2));
    int dt = getValue<int>(dateStr.substr(8, 2));
    bool isLeap = (yr % 4 == 0) && ( (yr % 100 != 0) || ( yr % 400 == 0));
    if(dt == 0) return false;
    switch (mt)
    {
        case 1:
        case 3:
        case 5:
        case 7:
        case 8:
        case 10:
        case 12:
            if(dt>31) return false;
            break;
        case 4:
        case 6:
        case 9:
        case 11:
            if(dt>30) return false;
            break;
        case 2:
            if(isLeap && dt>29) return false;
            if(!isLeap && dt>28) return false;
            break;
        default:
            return false;
    }
    date = yr*10000+mt*100+dt;
    return true;
}

std::string getAggType(SQLParser::AggregatorContext *agg){
    if(agg->Count()) return "COUNT";
    if(agg->Average()) return "AVG";
    if(agg->Sum()) return "SUM";
    if(agg->Min()) return "MIN";
    return "MAX";
}
