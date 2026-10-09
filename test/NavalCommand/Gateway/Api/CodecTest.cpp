#include <NavalCommand/Gateway/Api/Codec.h>
#include <gtest/gtest.h>
using namespace NavalCommand;
TEST(Codec, DomainDoesNotRequireTransport) {
  World::Api::View d;
  d.sessionId = "opaque";
  d.ready = true;
  d.units.push_back({"u",
                     "own",
                     {61.96, -10.37, {}},
                     {},
                     42,
                     {{"set_waypoint", Orders::Api::Availability::Executable,
                       "", "append"}}});
  auto w = Gateway::Api::Encode(d);
  auto r = Gateway::Api::Decode(w);
  EXPECT_EQ(r.units[0].id, "u");
  EXPECT_FALSE(r.units[0].position.altitudeMeters);
  EXPECT_FALSE(w.own_units(0).has_speed_mps());
  EXPECT_EQ(r.units[0].orders[0].kind, "set_waypoint");
}
TEST(Codec, OptionalZeroAndUnknownEnum) {
  navalcommand::gateway::v1::OrderResult w;
  w.set_displacement_m(0);
  w.set_stage(static_cast<navalcommand::gateway::v1::Stage>(99));
  auto r = Gateway::Api::Encode(Gateway::Api::Decode(w));
  EXPECT_TRUE(r.has_displacement_m());
  EXPECT_EQ(r.stage(), 99);
}
