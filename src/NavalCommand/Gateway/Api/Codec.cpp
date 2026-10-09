#include <NavalCommand/Gateway/Api/Codec.h>
namespace NavalCommand::Gateway::Api {
static void position(Wire::GeoPoint &p, const World::Api::Position &d) {
  p.set_latitude_deg(d.latitudeDeg);
  p.set_longitude_deg(d.longitudeDeg);
  if (d.altitudeMeters)
    p.set_altitude_m(*d.altitudeMeters);
}
static World::Api::Position position(const Wire::GeoPoint &p) {
  return {p.latitude_deg(), p.longitude_deg(),
          p.has_altitude_m() ? std::optional<double>{p.altitude_m()}
                             : std::nullopt};
}
Wire::WorldSnapshot Encode(const World::Api::View &d) {
  Wire::WorldSnapshot w;
  auto s = w.mutable_session();
  s->set_schema_version(1);
  s->set_session_id(d.sessionId);
  s->set_observer_id(d.observerId);
  s->set_ready(d.ready);
  w.set_observation_version(d.version);
  w.set_operational_seconds(d.operationalSeconds);
  w.set_truncated(d.truncated);
  for (auto &u : d.units) {
    auto x = w.add_own_units();
    x->set_id(u.id);
    x->set_name(u.name);
    position(*x->mutable_position(), u.position);
    if (u.speedMetersPerSecond)
      x->set_speed_mps(*u.speedMetersPerSecond);
    x->set_sampled_at_seconds(u.sampledAtSeconds);
    for (auto &e : u.orders) {
      auto a = x->add_orders();
      a->set_kind(e.kind);
      a->set_availability(static_cast<Wire::Availability>(e.availability));
      a->set_reason(e.reason);
      a->set_argument_semantics(e.arguments);
    }
  }
  for (auto &t : d.tracks) {
    auto x = w.add_tracks();
    x->set_id(t.id);
    position(*x->mutable_detected_position(), t.detectedPosition);
    x->set_sampled_at_seconds(t.sampledAtSeconds);
    x->set_provenance(t.provenance);
  }
  for (auto &f : d.facilities) {
    auto x = w.add_facilities();
    x->set_entity_id(f.entityId);
    x->set_pending_operations(f.pendingOperations);
    x->set_sampled_at_seconds(f.sampledAtSeconds);
    for (auto &k : f.stocks) {
      auto a = x->add_stocks();
      a->set_class_key(k.classKey);
      a->set_squadron(k.squadron);
      a->set_unreserved_count(k.unreservedCount);
    }
  }
  return w;
}
World::Api::View Decode(const Wire::WorldSnapshot &w) {
  World::Api::View d;
  d.sessionId = w.session().session_id();
  d.observerId = w.session().observer_id();
  d.ready = w.session().ready();
  d.version = w.observation_version();
  d.operationalSeconds = w.operational_seconds();
  d.truncated = w.truncated();
  for (auto &x : w.own_units()) {
    World::Api::Unit u;
    u.id = x.id();
    u.name = x.name();
    u.position = position(x.position());
    if (x.has_speed_mps())
      u.speedMetersPerSecond = x.speed_mps();
    u.sampledAtSeconds = x.sampled_at_seconds();
    for (auto &e : x.orders())
      u.orders.push_back(
          {e.kind(), static_cast<Orders::Api::Availability>(e.availability()),
           e.reason(), e.argument_semantics()});
    d.units.push_back(std::move(u));
  }
  for (auto &x : w.tracks())
    d.tracks.push_back({x.id(), position(x.detected_position()),
                        x.sampled_at_seconds(), x.provenance()});
  for (auto &x : w.facilities()) {
    World::Api::Facility f;
    f.entityId = x.entity_id();
    f.pendingOperations = x.pending_operations();
    f.sampledAtSeconds = x.sampled_at_seconds();
    for (auto &k : x.stocks())
      f.stocks.push_back({k.class_key(), k.squadron(), k.unreserved_count()});
    d.facilities.push_back(std::move(f));
  }
  return d;
}
Wire::OrderRequest Encode(const Orders::Api::Request &d) {
  Wire::OrderRequest w;
  w.set_session_id(d.sessionId);
  w.set_request_id(d.requestId);
  w.set_entity_id(d.entityId);
  w.set_based_on_version(d.basedOnVersion);
  if (d.waypoint) {
    w.mutable_waypoint()->set_latitude_deg(d.waypoint->latitudeDeg);
    w.mutable_waypoint()->set_longitude_deg(d.waypoint->longitudeDeg);
  }
  return w;
}
Orders::Api::Request Decode(const Wire::OrderRequest &w) {
  Orders::Api::Request d{w.session_id(), w.request_id(), w.entity_id(),
                         w.based_on_version()};
  if (w.has_waypoint())
    d.waypoint = Orders::Api::Waypoint{w.waypoint().latitude_deg(),
                                       w.waypoint().longitude_deg()};
  return d;
}
Wire::OrderResult Encode(const Orders::Api::Result &d) {
  Wire::OrderResult w;
  w.set_session_id(d.sessionId);
  w.set_request_id(d.requestId);
  w.set_stage(static_cast<Wire::Stage>(d.stage));
  w.set_reason(d.reason);
  w.set_sampled_at_seconds(d.sampledAtSeconds);
  if (d.displacementMeters)
    w.set_displacement_m(*d.displacementMeters);
  return w;
}
Orders::Api::Result Decode(const Wire::OrderResult &w) {
  Orders::Api::Result d{w.session_id(), w.request_id(),
                        static_cast<Orders::Api::Stage>(w.stage()), w.reason(),
                        w.sampled_at_seconds()};
  if (w.has_displacement_m())
    d.displacementMeters = w.displacement_m();
  return d;
}
} // namespace NavalCommand::Gateway::Api
