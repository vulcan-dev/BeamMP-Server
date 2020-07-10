///
/// Created by Anonymous275 on 4/2/2020
///
#include <iostream>
#include "Client.hpp"
#include "../logger.h"
#include "../Settings.hpp"
#include "../Lua System/LuaSystem.hpp"

void SendToAll(Client*c, const std::string& Data, bool Self, bool Rel);
void Respond(Client*c, const std::string& MSG, bool Rel);
void UpdatePlayers();
int TriggerLuaEvent(const std::string& Event,bool local,Lua*Caller,LuaArg* arg);

int FC(const std::string& s,const std::string& p,int n) {
    std::string::size_type i = s.find(p);
    int j;
    for (j = 1; j < n && i != std::string::npos; ++j)i = s.find(p, i+1);
    if (j == n)return(i);
    else return(-1);
}
int Handle(EXCEPTION_POINTERS *ep,char* Origin);

void Apply(Client*c,int VID,const std::string& pckt){
    std::string Packet = pckt;
    std::string VD = c->GetCarData(VID);
    Packet = Packet.substr(FC(Packet, ",", 2) + 1);
    Packet = VD.substr(0, FC(VD, ",", 2) + 1) +
             Packet.substr(0, Packet.find_last_of('"') + 1) +
             VD.substr(FC(VD, ",\"", 7));
    c->SetCarData(VID, Packet);
}
void UpdateCarData(Client*c,int VID,const std::string& Packet){
    __try{
        Apply(c,VID,Packet);
    }__except(Handle(GetExceptionInformation(),(char*)"Car Data Updater")){}
}
void VehicleParser(Client*c,const std::string& Pckt){
    std::string Packet = Pckt;
    char Code = Packet.at(1);
    int PID = -1;
    int VID = -1;
    std::string Data = Packet.substr(3),pid,vid;
    switch(Code){ //Spawned Destroyed Switched/Moved NotFound Reset
        case 's':
            if(Data.at(0) == '0'){
                int CarID = c->GetOpenCarID();
                std::cout << c->GetName() << " CarID : " << CarID << std::endl;
                Packet = "Os:"+c->GetRole()+":"+c->GetName()+":"+std::to_string(c->GetID())+"-"+std::to_string(CarID)+Packet.substr(4);
                if(c->GetCarCount() >= MaxCars ||
                TriggerLuaEvent("onVehicleSpawn",false,nullptr,
                                   new LuaArg{{c->GetID(),CarID,Packet.substr(3)}})){
                    Respond(c,Packet,true);
                    std::string Destroy = "Od:" + std::to_string(c->GetID())+"-"+std::to_string(CarID);
                    Respond(c,Destroy,true);
                }else{
                    c->AddNewCar(CarID,Packet);
                    SendToAll(nullptr, Packet,true,true);
                }
            }
            break;
        case 'c':
            pid = Data.substr(0,Data.find('-'));
            vid = Data.substr(Data.find('-')+1,Data.find(':',1)-Data.find('-')-1);
            if(pid.find_first_not_of("0123456789") == std::string::npos && vid.find_first_not_of("0123456789") == std::string::npos){
                PID = stoi(pid);
                VID = stoi(vid);
            }
            if(PID != -1 && VID != -1 && PID == c->GetID()){
                if(!TriggerLuaEvent("onVehicleEdited",false,nullptr,
                        new LuaArg{{c->GetID(),VID,Packet.substr(3)}})) {
                    SendToAll(c, Packet, false, true);
                    UpdateCarData(c,VID,Packet);
                }else{
                    std::string Destroy = "Od:" + std::to_string(c->GetID())+"-"+std::to_string(VID);
                    Respond(c,Destroy,true);
                    c->DeleteCar(VID);
                }

            }
            break;
        case 'd':
            pid = Data.substr(0,Data.find('-'));
            vid = Data.substr(Data.find('-')+1);
            if(pid.find_first_not_of("0123456789") == std::string::npos && vid.find_first_not_of("0123456789") == std::string::npos){
               PID = stoi(pid);
               VID = stoi(vid);
            }
            if(PID != -1 && VID != -1 && PID == c->GetID()){
                SendToAll(nullptr,Packet,true,true);
                TriggerLuaEvent("onVehicleDeleted",false,nullptr,
                                new LuaArg{{c->GetID(),VID}});
                c->DeleteCar(VID);
            }
            break;
        case 'r':
            SendToAll(c,Packet,false,true);
            break;
        //case 'm':
          //  break;
        default:
            break;
    }
    Data.clear();
    Packet.clear();
}
void SyncVehicles(Client*c){
    Respond(c,"Sn"+c->GetName(),true);
    SendToAll(c,"JWelcome "+c->GetName()+"!",false,true);
    TriggerLuaEvent("onPlayerJoin",false,nullptr,new LuaArg{{c->GetID()}});
    for (Client*client : Clients) {
        if (client != c) {
            for(const std::pair<int,std::string>&a : client->GetAllCars()){
                Respond(c,a.second,true);
            }
        }
    }
}

extern int PPS;

void ParseVeh(Client*c, const std::string&Packet){
    __try{
            VehicleParser(c,Packet);
    }__except(Handle(GetExceptionInformation(),(char*)"Vehicle Handler")){}
}

void GlobalParser(Client*c, const std::string&Packet){
    if(Packet.empty())return;
    if(Packet.find("TEST")!=std::string::npos)SyncVehicles(c);
    std::string pct;
    char Code = Packet.at(0);
    switch (Code) {
        case 'P':
            Respond(c, "P" + std::to_string(c->GetID()),true);
            return;
        case 'p':
            Respond(c,"p",false);
            UpdatePlayers();
            return;
        case 'O':
            if(Packet.length() > 1000) {
                std::cout << "Received data from: " << c->GetName() << " Size: " << Packet.length() << std::endl;
            }
            ParseVeh(c,Packet);
            return;
        case 'J':
            SendToAll(c,Packet,false,true);
            break;
        case 'C':
            pct = "C:" + c->GetName() + Packet.substr(Packet.find(':', 3));
            if (TriggerLuaEvent("onChatMessage", false, nullptr,
            new LuaArg{{ c->GetID(), c->GetName(), pct.substr(pct.find(':', 3) + 1) }}))break;
            SendToAll(nullptr, pct, true, true);
            pct.clear();
            break;
        case 'E':
            SendToAll(nullptr,Packet,true,true);
            break;
        default:
            break;
    }
    //V to Z
    if(Code <= 90 && Code >= 86){
        PPS++;
        SendToAll(c,Packet,false,false);
    }
    if(Debug)debug("Vehicle Data Received from " + c->GetName());
}