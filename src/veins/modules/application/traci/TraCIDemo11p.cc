//
// Copyright (C) 2006-2011 Christoph Sommer <christoph.sommer@uibk.ac.at>
//
// Documentation for these modules is at http://veins.car2x.org/
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include "veins/modules/application/traci/TraCIDemo11p.h"

Define_Module(TraCIDemo11p);

void TraCIDemo11p::initialize(int stage) {
    BaseWaveApplLayer::initialize(stage);
    if (stage == 0) {
        sentMessage = false;
        lastDroveAt = simTime();
        currentSubscribedServiceId = -1;
    }
}

void TraCIDemo11p::onBSM(BasicSafetyMessage* bsm) {
    //Your application has received a beacon message from another car or RSU
    //code for handling the message goes here
    std::clock_t start;
    double duration;
    start = std::clock();

    CryptoPP::ECDSA<ECP, CryptoPP::SHA256>::PublicKey publicKey = bsm->getSenderPK();
    const CryptoPP::ECP::Point& q = publicKey.GetPublicElement();
    const CryptoPP::Integer& qx = q.x;
    const CryptoPP::Integer& qy = q.y;

    std::stringstream ss ;
    ss<<std::hex<<q.x<<q.y;
    std::string pk = ss.str();

    for(std::vector<PID_List>::iterator i = valid_PID.begin() ; i != valid_PID.end(); ++i){
        if((system_clock::to_time_t(system_clock::now()) - i->expiry) > 5){
            valid_PID.erase(i);
            //std::cout<<"Expired and deleted"<<std::endl;
        }
    }

    for(std::vector<PID_List>::iterator it = valid_PID.begin() ; it != valid_PID.end(); ++it){
        if(it->PID == bsm->getSenderAddress() && it->pkey == pk){
            //std::cout<<"valid bsm"<<std::endl;
            goto end;
        }
    }
    if(this->isChannelIdle()){
        this->mac->setChannelIdle(false);
        if(this->verifyPID(bsm->getSenderAddress(), pk)) {
            valid_PID.push_back({bsm->getSenderAddress(), pk, system_clock::to_time_t(system_clock::now())});
            //std::cout<<"Channel is not congested"<<std::endl;
        }
        this->mac->setChannelIdle(true);
    }
    else {
        scheduleAt(simTime()+dblrand()*0.1, bsm);
        std::cout<<"Channel is congested, waited for sometime"<<std::endl;
        this->handleSelfMsg(bsm);
        cancelEvent(bsm);
        //std::cout<<"Channel is congested, waited for sometime"<<std::endl;
    }
    end:
    if(this->verifyPKSignature(bsm)) {
    }
    duration = (std::clock()-start)/(double)CLOCKS_PER_SEC*1000;
    totalDuration += duration;
    totalBSMs++;
}

void TraCIDemo11p::onWSA(WaveServiceAdvertisment* wsa) {
    /*if (currentSubscribedServiceId == -1) {
        mac->changeServiceChannel(wsa->getTargetChannel());
        currentSubscribedServiceId = wsa->getPsid();
        if  (currentOfferedServiceId != wsa->getPsid()) {
            stopService();
            startService((Channels::ChannelNumber) wsa->getTargetChannel(), wsa->getPsid(), "Mirrored Traffic Service");
        }
    }*/
    int pnm = wsa->getValidPID();
    std::string pubKey = wsa->getValidPK();
    this->cancelEvent(wsa);
    //std::cout<<"Cancelled the WSA Event."<<std::endl;
    for(std::vector<PID_List>::iterator i = valid_PID.begin() ; i != valid_PID.end(); ++i){
        if(i->PID == pnm){
            return;
        }
    }
    valid_PID.push_back({pnm, pubKey, system_clock::to_time_t(system_clock::now())});
}

void TraCIDemo11p::onWSM(WaveShortMessage* wsm) {
    findHost()->getDisplayString().updateWith("r=16,green");

    if (mobility->getRoadId()[0] != ':') traciVehicle->changeRoute(wsm->getWsmData(), 9999);
    if (!sentMessage) {
        sentMessage = true;
        //repeat the received traffic update once in 2 seconds plus some random delay
        wsm->setSenderAddress(myId);
        wsm->setSerial(3);
        scheduleAt(simTime() + 2 + uniform(0.01,0.2), wsm->dup());
    }
}

void TraCIDemo11p::handleSelfMsg(cMessage* msg) {
    if(BasicSafetyMessage* bsm = dynamic_cast<BasicSafetyMessage*>(msg))
    if(bsm->isSelfMessage()){
        const ECP::Point& q = bsm->getSenderPK().GetPublicElement();
        const CryptoPP::Integer& qx = q.x;
        const CryptoPP::Integer& qy = q.y;

        std::stringstream ss ;
        ss<<std::hex<<q.x<<q.y;
        std::string pk = ss.str();
        if(this->verifyPID(bsm->getSenderAddress(), pk)) {
            valid_PID.push_back({bsm->getSenderAddress(), pk, system_clock::to_time_t(system_clock::now())});
            std::cout<<"Handle Self Msg called and verified PID"<<std::endl;
        }
        if(this->verifyPKSignature(bsm)) {
            std::cout<<"Verified Message Signature"<<std::endl;
        }
        cancelEvent(bsm);
    }
    if (WaveShortMessage* wsm = dynamic_cast<WaveShortMessage*>(msg)) {
        //send this message on the service channel until the counter is 3 or higher.
        //this code only runs when channel switching is enabled
        sendDown(wsm->dup());
        wsm->setSerial(wsm->getSerial() +1);
        if (wsm->getSerial() >= 3) {
            //stop service advertisements
            stopService();
            delete(wsm);
        }
        else {
            scheduleAt(simTime()+1, wsm);
        }
    }
    else {
        BaseWaveApplLayer::handleSelfMsg(msg);
    }
}

void TraCIDemo11p::handlePositionUpdate(cObject* obj) {
    BaseWaveApplLayer::handlePositionUpdate(obj);

    // stopped for for at least 10s?
    if (mobility->getSpeed() < 1) {
        if (simTime() - lastDroveAt >= 10 && sentMessage == false) {
            findHost()->getDisplayString().updateWith("r=16,red");
            sentMessage = true;

            WaveShortMessage* wsm = new WaveShortMessage();
            populateWSM(wsm);
            wsm->setWsmData(mobility->getRoadId().c_str());

            //host is standing still due to crash
            if (dataOnSch) {
                startService(Channels::SCH2, 42, "Traffic Information Service");
                //started service and server advertising, schedule message to self to send later
                scheduleAt(computeAsynchronousSendingTime(1,type_SCH),wsm);
            }
            else {
                //send right away on CCH, because channel switching is disabled
                sendDown(wsm);
            }
        }
    }
    else {
        lastDroveAt = simTime();
    }
}

void TraCIDemo11p::finish(){
    BaseWaveApplLayer::finish();
    BaseWaveApplLayer::printStat();
    TraCIDemoRSU11p::printValues();
}
