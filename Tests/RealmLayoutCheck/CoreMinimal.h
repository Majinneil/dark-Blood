// Minimal stand-ins for the Unreal types DBRealmLayout.cpp uses, so the realm layout builds and can be checked without the
// engine. FMath::PerlinNoise2D is a classic Perlin noise here, not the engine's: the terrain has the same kind of shapes,
// not the same ones - results are a plausibility check, the real map comes from -run=DBBuildRealm.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <string>
#include <vector>

typedef uint8_t uint8; typedef int32_t int32; typedef uint32_t uint32; typedef int64_t int64; typedef char TCHAR;
#define TEXT(x) x
#define DARKBLOOD_API

struct FName { std::string S; FName() {} FName(const char* In) : S(In) {} bool operator==(const FName& O) const { return S == O.S; } };

struct FVector2D
{
	double X = 0, Y = 0;
	FVector2D() {}
	FVector2D(double InX, double InY) : X(InX), Y(InY) {}
	FVector2D operator+(const FVector2D& O) const { return {X + O.X, Y + O.Y}; }
	FVector2D operator-(const FVector2D& O) const { return {X - O.X, Y - O.Y}; }
	FVector2D operator*(double S) const { return {X * S, Y * S}; }
	FVector2D& operator+=(const FVector2D& O) { X += O.X; Y += O.Y; return *this; }
	double SizeSquared() const { return X * X + Y * Y; }
	bool IsZero() const { return X == 0 && Y == 0; }
	static double Distance(const FVector2D& A, const FVector2D& B) { return std::sqrt((A - B).SizeSquared()); }
	static double DotProduct(const FVector2D& A, const FVector2D& B) { return A.X * B.X + A.Y * B.Y; }
	static const FVector2D ZeroVector;
};
inline const FVector2D FVector2D::ZeroVector{0, 0};

template <typename T> struct TArray
{
	std::vector<T> V;
	TArray() {}
	TArray(std::initializer_list<T> L) : V(L) {}
	int32 Add(const T& Item) { V.push_back(Item); return int32(V.size()) - 1; }
	T& AddDefaulted_GetRef() { V.emplace_back(); return V.back(); }
	int32 Num() const { return int32(V.size()); }
	bool IsEmpty() const { return V.empty(); }
	T& Last() { return V.back(); }
	const T& Last() const { return V.back(); }
	void SetNum(int32 N) { V.resize(N); }
	T& operator[](int32 I) { return V[I]; }
	const T& operator[](int32 I) const { return V[I]; }
	auto begin() { return V.begin(); } auto end() { return V.end(); }
	auto begin() const { return V.begin(); } auto end() const { return V.end(); }
	template <typename P> const T* FindByPredicate(P Pred) const { for (const T& I : V) if (Pred(I)) return &I; return nullptr; }
};

template <typename T> struct TNumericLimits { static T Max() { return std::numeric_limits<T>::max(); } static T Lowest() { return std::numeric_limits<T>::lowest(); } };

struct FMemory { static void Memzero(void* P, size_t N) { std::memset(P, 0, N); } };

struct FMath
{
	template <typename T> static T Clamp(T V, T Lo, T Hi) { return V < Lo ? Lo : (V < Hi ? V : Hi); } // as the engine: NaN -> Hi
	template <typename T> static T Max(T A, T B) { return A > B ? A : B; }
	template <typename T> static T Min(T A, T B) { return A < B ? A : B; }
	template <typename T> static T Abs(T A) { return A < 0 ? -A : A; }
	template <typename T> static T Square(T A) { return A * A; }
	template <typename T, typename U> static T Lerp(T A, T B, U Alpha) { return A + (B - A) * Alpha; }
	static double Pow(double A, double B) { return std::pow(A, B); }
	static double Sin(double A) { return std::sin(A); }
	static double FloorToDouble(double A) { return std::floor(A); }
	static double Frac(double A) { return A - std::floor(A); }
	static int32 RoundToInt(double A) { return int32(std::floor(A + 0.5)); }
	static int32 FloorToInt(double A) { return int32(std::floor(A)); }
	static int32 CeilToInt(double A) { return int32(std::ceil(A)); }
	static float PerlinNoise2D(const FVector2D& L)
	{
		static int P[512];
		static bool bInit = [] {
			static const int Base[256] = {151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,226,250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,221,153,101,155,167,43,172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,107,49,192,214,31,181,199,106,157,184,84,204,176,115,121,50,45,127,4,150,254,138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180};
			for (int I = 0; I < 512; ++I) P[I] = Base[I & 255];
			return true; }();
		(void)bInit;
		auto Fade = [](double T) { return T * T * T * (T * (T * 6 - 15) + 10); };
		auto Grad = [](int H, double X, double Y) { switch (H & 7) { case 0: return X + Y; case 1: return -X + Y; case 2: return X - Y; case 3: return -X - Y; case 4: return X; case 5: return -X; case 6: return Y; default: return -Y; } };
		const double Fx = std::floor(L.X), Fy = std::floor(L.Y);
		const int Xi = int(int64(Fx) & 255), Yi = int(int64(Fy) & 255);
		const double X = L.X - Fx, Y = L.Y - Fy, U = Fade(X), W = Fade(Y);
		const int AA = P[P[Xi] + Yi], AB = P[P[Xi] + Yi + 1], BA = P[P[Xi + 1] + Yi], BB = P[P[Xi + 1] + Yi + 1];
		const double R = Lerp(Lerp(Grad(AA, X, Y), Grad(BA, X - 1, Y), U), Lerp(Grad(AB, X, Y - 1), Grad(BB, X - 1, Y - 1), U), W);
		return float(R);
	}
};
