#include <iostream>

#include <string>
#include <vector>
#include <fstream>
#include <stdio.h>
#include <string>
#include <time.h>
#include <stdlib.h>
#include <queue>
#include <map>
#include <stack>
#include <math.h>
#include <algorithm>
#include <unordered_map>

using namespace std;

typedef struct {
    int op; // 1 is add 0 is del
    int mode; // 1 is distribute;
    int core;
    int ram;
    int id;
}ReqCmd;

typedef struct {
    int core;
    int ram;
}node;

// running server status 
typedef struct {
    int id;      // server id
    string name; // server name
    node     a,b;  // rest resources 
}running_server;

// choice server
typedef struct {
    string name; // server name
    node a, b;   // rest resources
    int serverCost;
    int powerCost;
}choice_server;

// result -- deploy strategy
typedef struct {
    int id;    // vm id
    int core;  // vm request core
    int ram;   // vm request ram
    int abnode; // 1 is node a 0 is node b 
    int dnode;  // 1 is double 0 is single
}deploy;

// result -- migration strategy
typedef struct{
    int vid;    // vm id
    int sid;    // server id
    int abnode; // 1 is a, 0 is b
    int dnode;  // 1 is distribute, 0 is single
} migration;

// result -- map of purchase id to id 
typedef struct{
    int count; // the number of the same servers
    vector<int> sids; // the id list of the same servers
} purchase;

unordered_map<int,vector<int>> server2vm; //server id(index) to vms

unordered_map<int, deploy> vm2deploy; // vm id to deploy status

unordered_map<int,int> vm2server; //vm id to server index

vector<choice_server> choice_servers; // buy server list

vector<running_server> running_servers; // current server status

vector<vector<ReqCmd>> reqs; // request list

unordered_map<string,vector<int>> vmInfos; // vm message

unordered_map<string, choice_server> serverInfos;// server message

unordered_map<int, int> id2outid;//purchase id to output it

int restDays = 0, vm_core = 0, vm_ram = 0, weight=1.0 ,flag = 0;

void loadServer(string &serverType,string &cpuCores,string &memorySize,
    string &serverCost,string &powerCost){
    string _serverType="";
    for(int i =1;i<serverType.size() -1;i++){
        _serverType += serverType[i];
    }
    int _cpuCores =0,_memorySize=0,_serverCost=0,_powerCost=0;

    for(int i=0;i<cpuCores.size() -1;i++){
        _cpuCores = 10*_cpuCores + cpuCores[i] - '0';
    }
    for(int i=0;i<memorySize.size() -1;i++){
        _memorySize = 10*_memorySize + memorySize[i] - '0';
    }
    for(int i=0;i<serverCost.size() -1;i++){
        _serverCost = 10*_serverCost + serverCost[i] - '0';
    }
    for(int i=0;i<powerCost.size()-1;i++){
        _powerCost = 10*_powerCost + powerCost[i] - '0';
    }

    node a = {_cpuCores / 2,_memorySize / 2};
    node b = {_cpuCores / 2,_memorySize / 2};

    choice_server server = {_serverType,a,b,_serverCost,_powerCost};
    serverInfos[server.name] = server;
    choice_servers.push_back(server);
}


//string vmName,vcpuCores,vmemorySize,vtype;
void loadVM(string &vmName,string &vcpuCores,string &vmemorySize,
    string &vtype){
    string _vmName="";
    for(int i =1;i<vmName.size() -1;i++){
        _vmName += vmName[i];
    }
    int _cpuCores =0,_memorySize=0,_type=0;

    for(int i=0;i<vcpuCores.size() -1;i++){
        _cpuCores = 10*_cpuCores + vcpuCores[i] - '0';
    }
    for(int i=0;i<vmemorySize.size() -1;i++){
        _memorySize = 10*_memorySize + vmemorySize[i] - '0';
    }
	_type = vtype[0] - '0';
    vmInfos[_vmName] = vector<int>{ _type ,_cpuCores ,_memorySize};
}


ReqCmd loadReqs(int op,string &vmName,string &vmId0){
    string _vmName;
    int _vmID =0,_cmdType=0;
    vector<int> vmInfo;
    
    if(op) {
        for(int i =0;i<vmName.size() -1;i++){ 
            _vmName += vmName[i];
        }
        vmInfo = vmInfos[_vmName];
        // cout<<"name: "<<_vmName<<" vmInfo size"<<vmInfo.size()<<endl;
    }

    for(int i=0;i<vmId0.size() -1;i++){
        _vmID = 10*_vmID + vmId0[i] - '0';
    }

    if(op) {
        // cout<<"op"<<op<<" mode: "<<vmInfo[0]<<" core: "<<vmInfo[1]<<endl;
        return {op ,vmInfo[0] ,vmInfo[1] ,vmInfo[2] ,_vmID};
    }

    return {op ,0 ,0 ,0 ,_vmID};
}


bool cmpRunningServerCoreAndRam(running_server a, running_server b){ // sort rule of migration servers: rest core and ram , more is prior
    return a.a.core + a.b.core + a.a.ram + a.b.ram > b.a.core + b.b.core + b.a.ram + b.b.ram;
}

bool cmpRunningServerCoreAndRamLess(running_server a, running_server b){ // sort rule of migration servers: rest core and ram , more is prior
     return a.a.core + a.b.core + a.a.ram + a.b.ram < b.a.core + b.b.core + b.a.ram + b.b.ram;
}

bool cmpVmsCoreAndRam(int a, int b){
    return vm2deploy[a].core + vm2deploy[a].ram < vm2deploy[b].core + vm2deploy[b].ram;
}

bool cmpRunningServersIds(running_server a , running_server b){
    return a.id < b.id;
}

bool cmpServerCost(choice_server a, choice_server b){ // sort rule of purchase server: serverCost, less is prior 
    return a.serverCost < b.serverCost;
}

bool cmpServerCostPowerCost(choice_server a, choice_server b){ // sort rule of purchase server: serverCost, less is prior 
    return a.serverCost+a.powerCost*restDays < b.serverCost+b.powerCost*restDays;
}

bool cmpServer(choice_server a,choice_server b){
    int x = ((a.a.core * 2 + a.a.ram * 2) / a.serverCost + a.serverCost / a.powerCost);
    int y = ((b.a.core * 2 + b.a.ram * 2) / b.serverCost + b.serverCost / b.powerCost);
    return x > y;
}

bool cmpAddRequests(ReqCmd a, ReqCmd b){ // vm request rest days sort
    return a.core + a.ram > b.core + b.ram;
}

running_server inline new_server_r(int id,choice_server& server) {
    // create a new server and not deploy a vm in the server
    node a = {server.a.core , server.a.ram};
    node b = {server.b.core , server.b.ram};
    return {id,server.name,a,b};
}

running_server inline new_server_r_a(int id,choice_server& server,int vm_core,int vm_ram) {
    // create a new server and deploy a vm in the server node a
    node a = {server.a.core - vm_core, server.a.ram - vm_ram};
    node b = {server.b.core , server.b.ram};
    return {id,server.name,a,b};
}

running_server inline new_server_r_d(int id,choice_server& server,int vm_core,int vm_ram) {
    // create a new server and deploy a vm in the server node b
    node a = {server.a.core - vm_core/2, server.a.ram - vm_ram/2};
    node b = {server.b.core - vm_core/2, server.b.ram - vm_ram/2};
    return {id,server.name,a,b};
}

deploy inline create_vm(ReqCmd& req,int abnode,int dnode) {
    // create a new vm -- mode : 1 is double node 0 is abnode
    return {req.id,req.core,req.ram,abnode,dnode};
}

void inline update_server_a(int sid,ReqCmd& req) {
    // update server node a source
    running_servers[sid].a.core -= req.core;
    running_servers[sid].a.ram  -= req.ram;    
}

void inline update_server_b(int sid,ReqCmd& req) {
    // update server node b source
    running_servers[sid].b.core -= req.core;
    running_servers[sid].b.ram  -= req.ram;    
}

void inline update_server_d(int sid,ReqCmd& req) {
    // update server double node source
    running_servers[sid].a.core -= req.core/2;
    running_servers[sid].a.ram  -= req.ram/2;    
    running_servers[sid].b.core -= req.core/2;
    running_servers[sid].b.ram  -= req.ram/2;    
}

void inline migrate_server_d(int sIndex, int dIndex, deploy vm){
    // migrate src server double node source to dst server double node source
    running_servers[sIndex].a.core += vm.core/2;
    running_servers[sIndex].b.core += vm.core/2;
    running_servers[sIndex].a.ram += vm.ram/2;
    running_servers[sIndex].b.ram += vm.ram/2;

    running_servers[dIndex].a.core -= vm.core/2;
    running_servers[dIndex].b.core -= vm.core/2;
    running_servers[dIndex].a.ram -= vm.ram/2;
    running_servers[dIndex].b.ram -= vm.ram/2;
}

void inline migrate_server_a(int sIndex , int dIndex, deploy vm){
    // migrate src server node a/b source to dst server node a source
    if(vm.abnode == 1){
        running_servers[sIndex].a.core += vm.core;
        running_servers[sIndex].a.ram += vm.ram;
    }
    else{
        running_servers[sIndex].b.core += vm.core;
        running_servers[sIndex].b.ram += vm.ram;
    }
    running_servers[dIndex].a.core -= vm.core;
    running_servers[dIndex].a.ram -= vm.ram;
}

void inline migrate_server_b(int sIndex , int dIndex, deploy vm){
    // migrate src server node a/b source to dst server node b source
    if(vm.abnode == 1){
        running_servers[sIndex].a.core += vm.core;
        running_servers[sIndex].a.ram += vm.ram;
    }
    else{
        running_servers[sIndex].b.core += vm.core;
        running_servers[sIndex].b.ram += vm.ram;
    }
    running_servers[dIndex].b.core -= vm.core;
    running_servers[dIndex].b.ram -= vm.ram;
}

#define PRE_CHARGE_DAYS 3
#define PRE_LEAST_DAYS  10
#define SERVER_CHARGE_WEIGHT 2.0
int main()
{
	// TODO:read standard input
   
    freopen(".\\training-1.txt","rb",stdin);
    // freopen(".\\training-2.txt","rb",stdin);
    freopen(".\\test.txt","wb+",stdout);

    int serverNum,vmNum,days,days_k,req_Num,migrationNum=0; 
    string serverType,cpuCores,memorySize,serverCost,powerCost;
	string vmName,vcpuCores,vmemorySize,vtype;
    string reqCmd,reqVm,vmId;

    int outids_index = 0;
    int serverids_index = 0;
    
    scanf("%d",&serverNum);
    
    for(int i =0;i<serverNum;i++){
        cin>>serverType>>cpuCores>>memorySize>>serverCost>>powerCost;
        loadServer(serverType,cpuCores,memorySize,serverCost,powerCost);
    }	
	scanf("%d",&vmNum);
    for(int i =0;i<vmNum;i++){
        cin>>vmName>>vcpuCores>>vmemorySize>>vtype;
        loadVM(vmName,vcpuCores,vmemorySize,vtype);
    }		
   
    scanf("%d",&days);
    scanf("%d", &days_k);
    for(int i =0;i<days_k;i++){
        scanf("%d",&req_Num);
        vector<ReqCmd> reqds;
        for(int j=0;j<req_Num;j++) {
            cin>>reqCmd;
            if(reqCmd == "(add,") {
                cin>>reqVm>>vmId;
                ReqCmd reqc = loadReqs(1,reqVm,vmId);
                reqds.push_back(reqc);
            }
            else {
                cin>>vmId;
                ReqCmd reqc = loadReqs(0,reqVm,vmId);
                reqds.push_back(reqc);
            }
        }
        reqs.push_back(reqds);
    }

    // TODO:process
    int id = 0; // running servers global id(index)  
    int global_output_id = 0;

    for (int day = 0; day < days; day++){
        restDays = days - day;

        if(day < days/PRE_CHARGE_DAYS){
            sort(choice_servers.begin(), choice_servers.end(), cmpServerCostPowerCost);
        }else if(day == days/PRE_CHARGE_DAYS){
            sort(choice_servers.begin(), choice_servers.end(), cmpServerCost);
        }

        vector<ReqCmd> org_request = reqs[day];
        sort(reqs[day].begin(), reqs[day].end(), cmpAddRequests);

        unordered_map<string, purchase> purs;           // result -- buy server in one day
        vector<migration> migs;                         // result -- migrations in one day
        unordered_map<int, int> serverId2index;         // running_servers id to running_servers index

        // magic one: prepare purchase 
        if(day == 0){
            for (int i = 0; i < PRE_LEAST_DAYS && i < choice_servers.size(); i++){
                choice_server server = choice_servers[i];
                running_server pre_server = new_server_r(id, serverInfos[server.name]);
                serverId2index[pre_server.id] = id;
                id++;
                running_servers.push_back(pre_server);
                if(purs.find(pre_server.name) == purs.end()){
                    vector<int> serverids;
                    int count = 1;
                    serverids.push_back(pre_server.id);
                    purchase pur = {count, serverids};
                    purs[pre_server.name] = pur;
                }else{
                    purs[pre_server.name].count = purs[pre_server.name].count+1;
                    purs[pre_server.name].sids.push_back(pre_server.id);
                }
            }
        }


        // TODO：migration server
        int runningServersNum = running_servers.size();
        int migrationMax = floor(vm2server.size()*3.0/100);        // max migration times
        migrationNum += migrationMax;
        int i = 1;
        int st = 0;
        int ed = runningServersNum;
        while(i >= 0) {
            sort(running_servers.begin(), running_servers.end(), cmpRunningServerCoreAndRam);
            for (int sIndex = st; sIndex < ed; sIndex++){

                if(migrationMax == 0){
                    break;
                }
                // iter src servers
                vector<int> vms = server2vm[running_servers[sIndex].id];
                int vmsNums = vms.size();
                
                // sort the vm of the src server according to rest sources
                sort(vms.begin(), vms.end(), cmpVmsCoreAndRam);

                int dIndex = ed-1;
                for (int vIndex = 0; vIndex < vmsNums; vIndex++){
                    deploy vm = vm2deploy[vms[vIndex]]; 
                    // for (dIndex = runningServersNum - 1; dIndex > sIndex; dIndex--){
                    while(dIndex > sIndex){
                        if(vm.dnode == 1){ // double node
                            if( running_servers[dIndex].a.core >= vm.core/2 &&
                                running_servers[dIndex].b.core >= vm.core/2 && 
                                running_servers[dIndex].a.ram  >= vm.ram/2  &&
                                running_servers[dIndex].b.ram  >= vm.ram/2 ){
                                migrate_server_d(sIndex, dIndex, vm);
                                // update the deploy message of vm
                                vm2deploy[vm.id].abnode = 0;
                                vm2deploy[vm.id].dnode = 1;

                                // update vm id to server id
                                vm2server[vm.id] = running_servers[dIndex].id;

                                // add the vm to new server 
                                server2vm[running_servers[dIndex].id].push_back(vm.id);

                                // del the vm in old server
                                int src_vm_index = 0;
                                for (int i = 0; i < server2vm[running_servers[sIndex].id].size();i++){
                                    if(server2vm[running_servers[sIndex].id][i] == vm.id){
                                        src_vm_index = i;
                                        break;
                                    }
                                }
                                server2vm[running_servers[sIndex].id].erase(server2vm[running_servers[sIndex].id].begin()+src_vm_index);
                                migrationMax--;
                                // record the migration message 
                                migration mig = {vm.id, running_servers[dIndex].id , 0, 1};
                                migs.push_back(mig);
                                break;
                            }
                            else{
                                dIndex--;
                            }
                        }
                        else if(vm.dnode == 0){ // single node
                            if(running_servers[dIndex].a.core >= vm.core && 
                                running_servers[dIndex].a.ram >= vm.ram){ // dst server node a
                                migrate_server_a(sIndex, dIndex, vm);
                                vm2deploy[vm.id].abnode = 1;
                                vm2deploy[vm.id].dnode = 0;
                                vm2server[vm.id] = running_servers[dIndex].id;
                                server2vm[running_servers[dIndex].id].push_back(vm.id);

                                int src_vm_index = 0;
                                for (int i = 0; i < server2vm[running_servers[sIndex].id].size();i++){
                                    if(server2vm[running_servers[sIndex].id][i] == vm.id){
                                        src_vm_index = i;
                                        break;
                                    }
                                }
                                server2vm[running_servers[sIndex].id].erase(server2vm[running_servers[sIndex].id].begin()+src_vm_index);
                                
                                migrationMax--;
                                migration mig = {vm.id, running_servers[dIndex].id , 1, 0}; // dst server single node a
                                migs.push_back(mig);
                                break;
                            }
                            else if(running_servers[dIndex].b.core >= vm.core && 
                                    running_servers[dIndex].b.ram >= vm.ram){ // dst server node b
                                migrate_server_b(sIndex, dIndex, vm);
                                vm2deploy[vm.id].abnode = 0;
                                vm2deploy[vm.id].dnode = 0;
                                vm2server[vm.id] = running_servers[dIndex].id;
                                server2vm[running_servers[dIndex].id].push_back(vm.id);
                                int src_vm_index = 0;
                                for (int i = 0; i < server2vm[running_servers[sIndex].id].size();i++){
                                    if(server2vm[running_servers[sIndex].id][i] == vm.id){
                                        src_vm_index = i;
                                        break;
                                    }
                                }
                                server2vm[running_servers[sIndex].id].erase(server2vm[running_servers[sIndex].id].begin()+src_vm_index);
                                migrationMax--;
                                migration mig = {vm.id, running_servers[dIndex].id , 0, 0}; // dst server single node b
                                migs.push_back(mig);
                                break;
                            }
                            else{
                                dIndex--;
                            }
                        }
                    }
                    if(migrationMax == 0){
                        break;
                    }
                }
            }
            migrationNum -= migrationMax;

            st = 0.1 * runningServersNum;
            ed = runningServersNum - 0.1 * runningServersNum;            
            i -- ;
        }
        sort(running_servers.begin(), running_servers.end(), cmpRunningServerCoreAndRam);
        
        // recover 
        double migCores = 0.0, migRams = 0.0;
        for (int i = 0; i < runningServersNum;i++){
            double migCoreNums = 0, migRamNums = 0, migCorePer = 1.0, migRamPer = 1.0;
            migCoreNums = (running_servers[i].a.core + running_servers[i].b.core);
            migRamNums = (running_servers[i].a.ram + running_servers[i].b.ram);
            migCorePer = migCoreNums / serverInfos[running_servers[i].name].a.core * 2;
            migRamPer = migRamNums / serverInfos[running_servers[i].name].a.ram * 2;
            if(migCorePer > 0.7 ){
                migCores++;
            }
            if(migCorePer > 0.7 ){
                migRams++;
            }
        }

        if(flag == 0 && migCores / runningServersNum > 0.23 && migCores / runningServersNum > 0.23){
            int ts = 0;
            int start = 0;
            int end = runningServersNum;
            flag = 1;
            while(ts < 3){
                sort(running_servers.begin(), running_servers.end(), cmpRunningServerCoreAndRam);
                for (int sIndex = start; sIndex < end; sIndex++){
                    vector<int> vms = server2vm[running_servers[sIndex].id];
                    int vmsNums = vms.size();
                    sort(vms.begin(), vms.end(), cmpVmsCoreAndRam);
                    int dIndex = end;
                    for (int vIndex = 0; vIndex < vmsNums; vIndex++){
                        deploy vm = vm2deploy[vms[vIndex]]; 
                        for (dIndex = end - 1; dIndex > sIndex; dIndex--){
                            if(vm.dnode == 1){ // double node
                                if( running_servers[dIndex].a.core >= vm.core/2 &&
                                    running_servers[dIndex].b.core >= vm.core/2 && 
                                    running_servers[dIndex].a.ram  >= vm.ram/2  &&
                                    running_servers[dIndex].b.ram  >= vm.ram/2 ){
                                    migrate_server_d(sIndex, dIndex, vm);
                                    vm2deploy[vm.id].abnode = 0;
                                    vm2deploy[vm.id].dnode = 1;
                                    vm2server[vm.id] = running_servers[dIndex].id;
                                    server2vm[running_servers[dIndex].id].push_back(vm.id);
                                    int src_vm_index = 0;
                                    for (int i = 0; i < server2vm[running_servers[sIndex].id].size();i++){
                                        if(server2vm[running_servers[sIndex].id][i] == vm.id){
                                            src_vm_index = i;
                                            break;
                                        }
                                    }
                                    server2vm[running_servers[sIndex].id].erase(server2vm[running_servers[sIndex].id].begin()+src_vm_index);
                                    migration mig = {vm.id, running_servers[dIndex].id , 0, 1};
                                    migs.push_back(mig);
                                    break;
                                }
                            }
                            else if(vm.dnode == 0){ // single node
                                if(running_servers[dIndex].a.core >= vm.core && 
                                    running_servers[dIndex].a.ram >= vm.ram){ // dst server node a
                                    migrate_server_a(sIndex, dIndex, vm);
                                    vm2deploy[vm.id].abnode = 1;
                                    vm2deploy[vm.id].dnode = 0;
                                    vm2server[vm.id] = running_servers[dIndex].id;
                                    server2vm[running_servers[dIndex].id].push_back(vm.id);

                                    int src_vm_index = 0;
                                    for (int i = 0; i < server2vm[running_servers[sIndex].id].size();i++){
                                        if(server2vm[running_servers[sIndex].id][i] == vm.id){
                                            src_vm_index = i;
                                            break;
                                        }
                                    }
                                    server2vm[running_servers[sIndex].id].erase(server2vm[running_servers[sIndex].id].begin()+src_vm_index);
                                    
                                    migration mig = {vm.id, running_servers[dIndex].id , 1, 0}; // dst server node a, single
                                    migs.push_back(mig);
                                    break;
                                }
                                else if(running_servers[dIndex].b.core >= vm.core && 
                                        running_servers[dIndex].b.ram >= vm.ram){ // dst server node b
                                    migrate_server_b(sIndex, dIndex, vm);
                                    vm2deploy[vm.id].abnode = 0;
                                    vm2deploy[vm.id].dnode = 0;
                                    vm2server[vm.id] = running_servers[dIndex].id;
                                    server2vm[running_servers[dIndex].id].push_back(vm.id);
                                    int src_vm_index = 0;
                                    for (int i = 0; i < server2vm[running_servers[sIndex].id].size();i++){
                                        if(server2vm[running_servers[sIndex].id][i] == vm.id){
                                            src_vm_index = i;
                                            break;
                                        }
                                    }
                                    server2vm[running_servers[sIndex].id].erase(server2vm[running_servers[sIndex].id].begin()+src_vm_index);
                                    migration mig = {vm.id, running_servers[dIndex].id , 0, 0}; // dst server node b, single
                                    migs.push_back(mig);
                                    break;
                                }
                            }
                        }
                    }
                }
                // update start and end
                ts++;
                start = 0.1 * ts * runningServersNum;
                end = runningServersNum - 0.1 * ts*runningServersNum;
            }
        }

        // TODO：puchase and deploy
        int rlen = reqs[day].size();
        sort(running_servers.begin(), running_servers.end(), cmpRunningServerCoreAndRamLess);

        // update the map of server id to server index
        for (int index = 0; index < runningServersNum; index++){
            serverId2index[running_servers[index].id] = index;
        }

        for (int rid = 0; rid < rlen; rid++){
            if (reqs[day][rid].op == 1){ // add 
                int sid = 0;
                int slen = running_servers.size();
                for (sid = 0; sid < slen; sid++){
                    if(reqs[day][rid].mode == 0){ // single node
                        if(running_servers[sid].a.core >= reqs[day][rid].core && 
                            running_servers[sid].a.ram >= reqs[day][rid].ram){ 
                            update_server_a(sid,reqs[day][rid]);
                            deploy vm = create_vm(reqs[day][rid],1,0);
                            vm2server[vm.id] = running_servers[sid].id;
                            vm2deploy[vm.id] = vm;
                            server2vm[running_servers[sid].id].push_back(vm.id);

                            break;
                        }
                        else if(running_servers[sid].b.core >= reqs[day][rid].core && 
                            running_servers[sid].b.ram >= reqs[day][rid].ram){ 
                            update_server_b(sid,reqs[day][rid]);
                            deploy vm = create_vm(reqs[day][rid],0,0);
                            vm2server[vm.id] = running_servers[sid].id;
                            vm2deploy[vm.id] = vm;
                            server2vm[running_servers[sid].id].push_back(vm.id);
                            break;
                        }
                    }
                    else if(reqs[day][rid].mode == 1){ // double node 
                        if(running_servers[sid].a.core >= reqs[day][rid].core/2 && 
                            running_servers[sid].a.ram >= reqs[day][rid].ram/2 && 
                            running_servers[sid].b.core >= reqs[day][rid].core/2 && 
                            running_servers[sid].b.ram >= reqs[day][rid].ram/2){
                            update_server_d(sid,reqs[day][rid]);
                            deploy vm = create_vm(reqs[day][rid],0,1);
                            vm2server[vm.id] = running_servers[sid].id;
                            vm2deploy[vm.id] = vm;
                            server2vm[running_servers[sid].id].push_back(vm.id);
                            break;
                        }
                    }
                }
                
                if(sid == slen){ // purchase server
                    if(reqs[day][rid].mode == 0){ // single node   --- purchase server
                        for (int i=0; i < choice_servers.size();i++){
                            if(choice_servers[i].a.core >= SERVER_CHARGE_WEIGHT * reqs[day][rid].core && 
                                choice_servers[i].a.ram >= SERVER_CHARGE_WEIGHT * reqs[day][rid].ram){
                                deploy vm = create_vm(reqs[day][rid],1,0);
                                vm2server[vm.id] = id;
                                vm2deploy[vm.id] = vm;
                                running_server server = new_server_r_a(id,choice_servers[i],reqs[day][rid].core,reqs[day][rid].ram);
                                vector<int> vms;
                                vms.push_back(vm.id);
                                server2vm[server.id] = vms;
                                serverId2index[server.id] = id;
                                id++;
                                running_servers.push_back(server);
                                if(purs.find(server.name) == purs.end()) {
                                    vector<int> serverids;
                                    int count = 1;
                                    serverids.push_back(server.id);
                                    purchase pur = {count, serverids};
                                    purs[server.name] = pur;
                                }
                                else{
                                    purs[server.name].count = purs[server.name].count + 1;
                                    purs[server.name].sids.push_back(server.id); // VA -- 0,1
                                }
                                break;
                            }
                        }
                    }
                    else if(reqs[day][rid].mode == 1){ // double node  --- purchase server
                        for (int i=0; i < choice_servers.size();i++){
                            if(choice_servers[i].a.core >= reqs[day][rid].core / 2 
                                && choice_servers[i].b.core >= reqs[day][rid].core / 2 
                                && choice_servers[i].a.ram >= reqs[day][rid].ram / 2
                                && choice_servers[i].b.ram >= reqs[day][rid].ram / 2){
                                
                                deploy vm = create_vm(reqs[day][rid],0,1); 
                                vm2server[vm.id] = id;
                                vm2deploy[vm.id] = vm;
                                running_server server = new_server_r_d(id,choice_servers[i],reqs[day][rid].core,reqs[day][rid].ram);
                                vector<int> vms;
                                vms.push_back(vm.id);
                                server2vm[server.id] = vms;
                                serverId2index[server.id] = id;
                                id++;
                                running_servers.push_back(server);
                                if(purs.find(server.name) == purs.end()) {
                                    vector<int> serverids;
                                    int count = 1;
                                    serverids.push_back(server.id);
                                    purchase pur = {count, serverids};
                                    purs[server.name] = pur;
                                }
                                else{
                                    purs[server.name].count = purs[server.name].count + 1;
                                    purs[server.name].sids.push_back(server.id); // VA -- 0,1
                                }
                                break;
                            }
                        }
                    }
                }
            }
            
            if(reqs[day][rid].op == 0){ // del
                running_server& server = running_servers[serverId2index[vm2server[reqs[day][rid].id]]];
                deploy vm = vm2deploy[reqs[day][rid].id];
                if(vm.dnode == 1){  // double node
                    server.a.core += (vm.core / 2);
                    server.b.core += (vm.core / 2);
                    server.a.ram += (vm.ram / 2);
                    server.b.ram += (vm.ram / 2);
                }
                else{ //single node
                    if(vm.abnode == 1){ 
                        server.a.core += vm.core;
                        server.a.ram += vm.ram;
                    }
                    else {
                        server.b.core += vm.core;
                        server.b.ram += vm.ram;
                    }
                }

                // del the map of server id to vm list
                int vm_index = 0;
                for (int i = 0; i < server2vm[server.id].size();i++){
                    if(server2vm[server.id][i] == vm.id){
                        vm_index = i;
                        break;
                    }
                }
                server2vm[server.id].erase(server2vm[server.id].begin() + vm_index);
            }
        }
        
        // TODO：write standard output

        // 1. output purchase information
        cout << "(purchase, " << purs.size() << ")" << endl;
        for (unordered_map<string, purchase>::iterator it = purs.begin(); it != purs.end();it++){
            string servername = it->first;
            purchase p = it->second;
            vector<int> v = p.sids;
            cout << "(" << servername << ", " << p.count << ")" << endl;
            // map id
            for (int j = 0; j < v.size();j++){
                id2outid[v[j]] = global_output_id;
                global_output_id++;
            }
        }
        // 2. output migration information
        cout << "(migration, " << migs.size() << ")"<< endl;
        for (int mid = 0; mid < migs.size(); mid++){
            int sid = migs[mid].sid;
            int vid = migs[mid].vid;
            if(migs[mid].dnode == 1){ // double node
                cout << "(" << vid << ", " << id2outid[sid] << ")" << endl;
            }
            else{ // single node
                char migration_node = 'A';
                if(migs[mid].abnode == 1){
                    migration_node = 'A';
                }
                else{
                    migration_node = 'B';
                }
                cout << "(" << vid << ", " << id2outid[sid] << ", " << migration_node << ")" << endl;
            }
        }
        // 3. output deploy information
        for (int rid = 0; rid < org_request.size(); rid++){
            if(org_request[rid].op == 1){
                int vid = org_request[rid].id;
                int sid = vm2server[vid];
                deploy vm = vm2deploy[vid];
                if(vm.dnode == 0){ // single node
                    char deploy_node = 'A';
                    if(vm.abnode == 1){
                        deploy_node = 'A';
                    }
                    if(vm.abnode == 0){
                        deploy_node = 'B';
                    }
                    cout << "(" << id2outid[sid] << ", " << deploy_node<< ")" << endl;
                }
                else{ // double node
                    cout << "(" << id2outid[sid] << ")" << endl;
                }
            }else{
                // del the map of vm id to server id 
                vm2server.erase(org_request[rid].id);
                // del the map of vm deploy message 
                vm2deploy.erase(org_request[rid].id);
            }
        }
        fflush(stdout);
        // TODO：input another day's request information
        if(reqs.size()<days){
            scanf("%d", &req_Num);
            vector<ReqCmd> reqds;
            for (int j = 0; j < req_Num;j++){
                cin >> reqCmd;
                if(reqCmd == "(add,") {
                    cin>>reqVm>>vmId;
                    ReqCmd reqc = loadReqs(1,reqVm,vmId);
                    reqds.push_back(reqc);
                }
                else {
                    cin>>vmId;
                    ReqCmd reqc = loadReqs(0,reqVm,vmId);
                    reqds.push_back(reqc);
                }
            }
            reqs.push_back(reqds);
        }
    }
    
    // cout<<"server Num "<< id <<endl;
    // cout << "migrationNum " << migrationNum << endl;
    return 0;
}
