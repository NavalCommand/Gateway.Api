#include <NavalCommand/Gateway/Api/Codec.h>
#include <fstream>
#include <iostream>

int main(int argc, char** argv)
{
	using namespace NavalCommand::Gateway::Api;
	if (argc != 3)
	{
		return 2;
	}
	try
	{
		std::ifstream input{argv[1], std::ios::binary};
		Wire::Publication publication;
		if (!publication.ParseFromIstream(&input) || publication.views_size() != 1 || publication.views(0).entries_size() != 4)
		{
			return 3;
		}
		auto& view = *publication.mutable_views(0);
		*view.mutable_entries(0)->mutable_result()->mutable_observation() = Encode(Decode(view.entries(0).result().observation()));
		*view.mutable_entries(1)->mutable_result()->mutable_operations() = Encode(Decode(view.entries(1).result().operations()));
		*view.mutable_entries(2)->mutable_result()->mutable_failure()->mutable_infrastructure() = Encode(Decode(view.entries(2).result().failure().infrastructure()));
		*view.mutable_entries(3)->mutable_result()->mutable_unit() = Encode(Decode(view.entries(3).result().unit()));
		std::ofstream output{argv[2], std::ios::binary};
		return publication.SerializeToOstream(&output) ? 0 : 4;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
