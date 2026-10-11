#include <NavalCommand/Gateway/Api/Codec.h>
#include <NavalCommand/Orders/Api/Request.h>
#include <NavalCommand/Orders/Api/RouteCommand.h>
#include <NavalCommand/World/Api/ObserverSnapshot.h>

int main() {
	NavalCommand::Orders::Api::Request request;
	request.SessionId = "session";
	request.RequestId = "request";
	request.Observer = {"observer"};
	request.Observation = {"contact"};
	request.BasedOnVersion = 7;
	request.Payload = NavalCommand::Orders::Api::RouteCommand{{61.96}, {-10.37},
		NavalCommand::Orders::Api::RouteMode::AppendWaypoint};

	const auto roundTrip = NavalCommand::Gateway::Api::Decode(NavalCommand::Gateway::Api::Encode(request));
	NavalCommand::World::Api::ObserverSnapshot snapshot;
	snapshot.SessionId = roundTrip.SessionId;
	snapshot.Observer = roundTrip.Observer;
	const auto observerView = NavalCommand::Gateway::Api::Decode(NavalCommand::Gateway::Api::Encode(snapshot));
	const auto& route = std::get<NavalCommand::Orders::Api::RouteCommand>(roundTrip.Payload);
	return observerView.SessionId == "session" && route.Longitude.Value == -10.37 &&
		roundTrip.Observation.Value == "contact" ? 0 : 1;
}
