// Net hit reaction, the body of a Custom material node in M_PadelNet and M_PadelNetSolid
// (built by import_assets.py). Returns the offset along the net's local X, in cm.
//
// This is the GPU copy of Padel::Presentation::FNetReaction::Displacement
// (Source/Padel/Private/Presentation/Model/PadelNetReaction.cpp), which the native tests
// check. Change both together.
//
// Inputs (all from MPC_NetReaction, written each frame by UPadelNetReactionSubsystem):
//   P       float3  vertex position in the mesh's local space, cm (the net is built in court space)
//   Impact0 float4  (Y cm, Z cm, Amplitude cm, Age s); Amplitude 0 = no hit
//   Impact1 float4  same, second slot
//   Shape   float4  (BulgeRadius cm, WaveSpeed cm/s, WaveRadius cm, RippleShare)
//   Motion  float4  (Frequency Hz, Damping 1/s, Duration s, TapeShare)
//   Net     float4  (HalfWidth cm, NetHeightCentre cm, NetHeightPosts cm, PostFade cm)
//   Extra   float4  (GroundFade cm, CableFade cm, MaxDisplacement cm, PostInset cm)

float4 Impacts[2] = { Impact0, Impact1 };
float T = saturate(abs(P.y) / Net.x);
float Top = Net.y + (Net.z - Net.y) * T * T;
float Edges = smoothstep(Extra.w, Extra.w + Net.w, Net.x - abs(P.y));
float Ground = smoothstep(0.0, Extra.x, P.z);
float Cable = lerp(1.0, Motion.w, smoothstep(Top - Extra.y, Top, P.z));
float Sum = 0.0;
for (int Index = 0; Index < 2; ++Index)
{
	float4 I = Impacts[Index];
	if (I.z == 0.0 || I.w >= Motion.z)
	{
		continue;
	}
	float Age = I.w;
	float Time = exp(-Motion.y * Age) * sin(6.28318530718 * Motion.x * Age) * (1.0 - smoothstep(0.75 * Motion.z, Motion.z, Age));
	float DY = P.y - I.x;
	float DZ = P.z - I.y;
	float Bulge = exp(-(DY * DY + DZ * DZ) / (2.0 * Shape.x * Shape.x));
	float Travel = Shape.y * Age;
	float W2 = 2.0 * Shape.z * Shape.z;
	float Ripple = 0.5 * (exp(-(DY - Travel) * (DY - Travel) / W2) + exp(-(DY + Travel) * (DY + Travel) / W2)) * exp(-DZ * DZ / W2);
	Sum += clamp(I.z * Time * Edges * Ground * Cable * (Bulge + Shape.w * Ripple), -Extra.z, Extra.z);
}
return clamp(Sum, -Extra.z, Extra.z);
