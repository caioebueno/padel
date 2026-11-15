#include "TestSupport.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace Padel::Test
{
	const FBallPhysicsParams& Physics()
	{
		static const FBallPhysicsParams Params = []()
		{
			std::ifstream File(PADEL_REPO_ROOT "/Data/Physics.json");
			std::stringstream Text;
			Text << File.rdbuf();
			FBallPhysicsParams Loaded;
			std::string Error;
			if (!ParseBallPhysicsJson(Text.str(), Loaded, Error))
			{
				std::fprintf(stderr, "Data/Physics.json: %s\n", Error.c_str());
				std::abort();
			}
			return Loaded;
		}();
		return Params;
	}
}
