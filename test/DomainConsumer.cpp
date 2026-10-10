#include <NavalCommand/Gateway/Api/Codec.h>
#include <NavalCommand/Orders/Api/Request.h>
#include <NavalCommand/Orders/Api/RouteCommand.h>
#include <NavalCommand/World/Api/View.h>

int main()
{
	NavalCommand::Orders::Api::Request request{"session", "request", "unit", 7, NavalCommand::Orders::Api::RouteCommand{61.96, -10.37}};
	const auto roundTrip = NavalCommand::Gateway::Api::Decode(NavalCommand::Gateway::Api::Encode(request));
	NavalCommand::World::Api::View view;
	view.sessionId = roundTrip.sessionId;
	return view.sessionId == "session" && roundTrip.routeCommand->longitudeDeg == -10.37 ? 0 : 1;
}
