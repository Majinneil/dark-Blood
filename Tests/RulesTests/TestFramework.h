// Minimal dependency-free test harness (no exceptions, matching the rules core constraints).
#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace DarkBloodTest
{
	struct FTestCase
	{
		const char* Name;
		std::function<void()> Body;
	};

	inline std::vector<FTestCase>& Registry()
	{
		static std::vector<FTestCase> Tests;
		return Tests;
	}

	inline int& FailureCount()
	{
		static int Failures = 0;
		return Failures;
	}

	struct FRegistrar
	{
		FRegistrar(const char* Name, std::function<void()> Body) { Registry().push_back({Name, std::move(Body)}); }
	};

	inline void ReportFailure(const char* File, int Line, const std::string& Message)
	{
		++FailureCount();
		std::printf("    FAILED %s:%d: %s\n", File, Line, Message.c_str());
	}
}

#define DB_CONCAT_INNER(A, B) A##B
#define DB_CONCAT(A, B) DB_CONCAT_INNER(A, B)

#define DB_TEST(Name)                                                                                   \
	static void Name();                                                                                 \
	static DarkBloodTest::FRegistrar DB_CONCAT(Registrar_, Name)(#Name, &Name);                         \
	static void Name()

#define DB_CHECK(Condition)                                                                             \
	do                                                                                                  \
	{                                                                                                   \
		if (!(Condition))                                                                               \
		{                                                                                               \
			DarkBloodTest::ReportFailure(__FILE__, __LINE__, #Condition);                               \
		}                                                                                               \
	} while (0)

#define DB_CHECK_EQ(Actual, Expected)                                                                   \
	do                                                                                                  \
	{                                                                                                   \
		const auto& DbActual = (Actual);                                                                \
		const auto& DbExpected = (Expected);                                                            \
		if (!(DbActual == DbExpected))                                                                  \
		{                                                                                               \
			DarkBloodTest::ReportFailure(__FILE__, __LINE__, std::string(#Actual " == " #Expected));     \
		}                                                                                               \
	} while (0)

#define DB_CHECK_NEAR(Actual, Expected, Tolerance)                                                      \
	do                                                                                                  \
	{                                                                                                   \
		const double DbDelta = static_cast<double>(Actual) - static_cast<double>(Expected);             \
		if (DbDelta > (Tolerance) || DbDelta < -(Tolerance))                                            \
		{                                                                                               \
			DarkBloodTest::ReportFailure(__FILE__, __LINE__,                                            \
				std::string(#Actual " ~= " #Expected " (got ") + std::to_string(static_cast<double>(Actual)) + ")"); \
		}                                                                                               \
	} while (0)
