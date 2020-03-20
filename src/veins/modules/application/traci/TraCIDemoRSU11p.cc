//
// Copyright (C) 2016 David Eckhoff <david.eckhoff@fau.de>
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

#include "veins/modules/application/traci/TraCIDemoRSU11p.h"

Define_Module(TraCIDemoRSU11p);
int TraCIDemoRSU11p::lastPID = 0;
long TraCIDemoRSU11p::totalRSURequests = 0;
long TraCIDemoRSU11p::totalWSAs = 0;

void TraCIDemoRSU11p::onWSA(WaveServiceAdvertisment* wsa) {
    //if this RSU receives a WSA for service 42, it will tune to the chan
    if (wsa->getPsid() == 42) {
        mac->changeServiceChannel(wsa->getTargetChannel());
    }
    //std::cout<<"Receiving WSA"<<std::endl;
}

void TraCIDemoRSU11p::onWSM(WaveShortMessage* wsm) {
    //this rsu repeats the received traffic update in 2 seconds plus some random delay
    //wsm->setSenderAddress(myId);
    //if(this->verifyPID(wsm->getSenderAddress()) && this->verifyPKSignature(wsm)){
        sendDelayedDown(wsm->dup(), 2 + uniform(0.01,0.2));
    //}
}

bool TraCIDemoRSU11p::verifyPID(int senderPID, std::string pk) {
    this->totalRSURequests++;
    if(senderPID == this->lastPID) {
        //std::cout<<"Previously validated this vehicle's PID and send WSA. Returned"<<std::endl;
        return true;
    }
    std::string pnm = to_string(senderPID);
    http_client client(std::string("http://localhost:3000/api/queries/selectAllValidVehicles?PIDParam=")+pnm);
    http_response response = client.request(methods::GET).get();
    if(response.status_code() == status_codes::OK)
    {
        utility::string_t body = response.extract_string().get();
        if(body.compare("[]")==0)
            return false;
        string_t value1 = body.substr(49, 8);
        string_t value2 = body.substr(64, 130);
        if(pnm.compare(value1)==0 && pk.compare(value2)){
            //std::cout<<"Verified PK: "<<value2<<std::endl;
            WaveServiceAdvertisment* wsa = new WaveServiceAdvertisment();
            this->populateWSM(wsa);
            wsa->setValidPID(senderPID);
            wsa->setValidPK(pk);
            //this->handleSelfMsg(wsa);
            //sendDown(wsa);
            this->totalWSAs++;
            scheduleAt(simTime(), wsa);
            //std::cout<<"Created and send WSA for: "<<pnm<<std::endl;
            this->lastPID = senderPID;
            return true;
        }

    }
    return false;
}

void TraCIDemoRSU11p::printValues() {
    std::cout<<"Total WSAs: "<<this->totalWSAs<<std::endl;
    std::cout<<"Total RSU Requests: "<<this->totalRSURequests<<std::endl;
}
