// Regression test: run the real quietroom_cli on a fixed noisy clip and check that the SNR
// improvement stays above a loose floor. The floor is deliberately far below what the default
// settings achieve (see docs/results), so it catches breakage, not small tuning changes.
#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "quietroom/wav.hpp"

#ifndef _WIN32
#include <sys/wait.h>
#endif

#ifndef QUIETROOM_CLI_PATH
#error "QUIETROOM_CLI_PATH must be defined by CMake"
#endif
#ifndef QUIETROOM_DATA_DIR
#error "QUIETROOM_DATA_DIR must be defined by CMake"
#endif

namespace {

constexpr double kSnrImprovementFloorDb = 3.0;

// Runs the CLI with `args` (already quoted) and returns its exit code.
int run_cli(const std::string& args) {
    std::string command = std::string("\"") + QUIETROOM_CLI_PATH + "\" " + args;
#ifdef _WIN32
    // cmd /C strips the outer quotes of a command that has several quoted parts; add a pair.
    command = "\"" + command + "\"";
    return std::system(command.c_str());
#else
    const int status = std::system(command.c_str());
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

double snr_db(const std::vector<float>& clean, const std::vector<float>& x) {
    double num = 0, den = 0;
    for (std::size_t i = 0; i < clean.size(); ++i) {
        const double c = clean[i], e = x[i] - clean[i];
        num += c * c;
        den += e * e;
    }
    return 10.0 * std::log10(num / den);
}

}  // namespace

TEST(CliRegression, ImprovesSnrOnFixedNoisyClip) {
    const std::string data = QUIETROOM_DATA_DIR;
    const std::string noisy_path = data + "/regression/noisy_white_5db.wav";
    const std::string clean_path = data + "/speech/3000-15664-0000.wav";
    const std::string out_path = "quietroom_cli_regression_out.wav";

    const std::string args = "\"" + noisy_path + "\" \"" + out_path + "\" > quietroom_cli_regression_log.txt";
    ASSERT_EQ(run_cli(args), 0) << "CLI failed with args: " << args;

    quietroom::WavData clean, noisy, out;
    std::string error;
    ASSERT_TRUE(quietroom::read_wav(clean_path, clean, error)) << error;
    ASSERT_TRUE(quietroom::read_wav(noisy_path, noisy, error)) << error;
    ASSERT_TRUE(quietroom::read_wav(out_path, out, error)) << error;
    ASSERT_EQ(out.samples.size(), clean.samples.size());  // CLI output stays aligned and same length

    const double improvement = snr_db(clean.samples, out.samples) - snr_db(clean.samples, noisy.samples);
    RecordProperty("snr_improvement_db", std::to_string(improvement));
    EXPECT_GT(improvement, kSnrImprovementFloorDb);

    std::remove(out_path.c_str());
    std::remove("quietroom_cli_regression_log.txt");
}

TEST(CliRegression, MissingInputFailsWithNonZeroExit) {
    // Exit code 1 is the CLI's "could not read input" code; 2 would mean bad usage, and a
    // shell/quoting failure would give yet another code, so this cannot pass by accident.
    EXPECT_EQ(run_cli("does_not_exist.wav quietroom_cli_missing_out.wav > quietroom_cli_missing_log.txt 2>&1"), 1);
    std::remove("quietroom_cli_missing_log.txt");
}
