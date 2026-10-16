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
		if (!publication.ParseFromIstream(&input) || publication.views_size() != 1 || publication.views(0).entries_size() != 9)
		{
			return 3;
		}
		auto& view = *publication.mutable_views(0);
		for (int index = 0; index < 5; ++index)
		{
			auto* result = view.mutable_entries(index)->mutable_result();
			auto domainObservation = Decode(result->observation());
			*result->mutable_observation() = Encode(domainObservation);
		}
		*view.mutable_entries(5)->mutable_result()->mutable_operations() = Encode(Decode(view.entries(5).result().operations()));
		*view.mutable_entries(6)->mutable_result()->mutable_failure()->mutable_infrastructure() = Encode(Decode(view.entries(6).result().failure().infrastructure()));
		*view.mutable_entries(7)->mutable_result()->mutable_unit() = Encode(Decode(view.entries(7).result().unit()));
		*view.mutable_entries(8)->mutable_result()->mutable_unit_type() = Encode(Decode(view.entries(8).result().unit_type()));
		std::ofstream output{argv[2], std::ios::binary};
		return publication.SerializeToOstream(&output) ? 0 : 4;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
