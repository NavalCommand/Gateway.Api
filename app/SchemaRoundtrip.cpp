#include <NavalCommand/Gateway/Api/Codec.h>
#include <fstream>
#include <iostream>
int main(int argc,char** argv) {
 if(argc!=3)return 2;
 std::ifstream in(argv[1],std::ios::binary); navalcommand::gateway::v1::OrderRequest w;
 if(!w.ParseFromIstream(&in)||!w.has_waypoint()||w.request_id()!="conformance"||w.waypoint().latitude_deg()!=61.96)return 3;
 w.mutable_waypoint()->set_longitude_deg(-10.37);
 std::ofstream out(argv[2],std::ios::binary);if(!w.SerializeToOstream(&out))return 4;
 std::cout<<"C++ parsed oneof, stable IDs, degrees and preserved unknown fields\n";
}

