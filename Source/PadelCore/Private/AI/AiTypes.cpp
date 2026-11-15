#include "AI/AiTypes.h"

namespace Padel::AI
{
const char* AiShotName(EAiShot Shot)
{
	switch (Shot)
	{
	case EAiShot::Drive: return "drive";
	case EAiShot::Volley: return "volley";
	case EAiShot::Lob: return "lob";
	case EAiShot::Bandeja: return "bandeja";
	case EAiShot::Vibora: return "vibora";
	case EAiShot::Smash: return "smash";
	case EAiShot::Chiquita: return "chiquita";
	case EAiShot::ServeFlat: return "serve_flat";
	case EAiShot::ServeKick: return "serve_kick";
	}
	return "unknown";
}

bool IsOverhead(EAiShot Shot)
{
	return Shot == EAiShot::Bandeja || Shot == EAiShot::Vibora || Shot == EAiShot::Smash;
}

bool IsServe(EAiShot Shot)
{
	return Shot == EAiShot::ServeFlat || Shot == EAiShot::ServeKick;
}
}
