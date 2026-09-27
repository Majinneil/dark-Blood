#include "TestFramework.h"

int main()
{
	int FailedTests = 0;
	for (const DarkBloodTest::FTestCase& Test : DarkBloodTest::Registry())
	{
		const int Before = DarkBloodTest::FailureCount();
		Test.Body();
		const bool bPassed = DarkBloodTest::FailureCount() == Before;
		FailedTests += bPassed ? 0 : 1;
		std::printf("[%s] %s\n", bPassed ? " OK " : "FAIL", Test.Name);
	}
	std::printf("\n%zu tests, %d failed, %d failed checks\n", DarkBloodTest::Registry().size(), FailedTests,
		DarkBloodTest::FailureCount());
	return FailedTests == 0 ? 0 : 1;
}
