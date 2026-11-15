#include "Data/PadelPracticeBallRow.h"

Padel::Shots::FPracticeBallParams FPadelPracticeBallRow::ToCore() const
{
	Padel::Shots::FPracticeBallParams Out;
	Out.GravityMps2 = GravityMps2;
	Out.RadiusM = RadiusM;
	Out.GroundRestitution = GroundRestitution;
	Out.GroundTangentKeep = GroundTangentKeep;
	Out.GlassRestitution = GlassRestitution;
	Out.GlassTangentKeep = GlassTangentKeep;
	Out.MeshRestitution = MeshRestitution;
	Out.MeshTangentKeep = MeshTangentKeep;
	Out.NetRestitution = NetRestitution;
	Out.DeadLingerS = DeadLingerS;
	Out.StepS = StepS;
	return Out;
}
