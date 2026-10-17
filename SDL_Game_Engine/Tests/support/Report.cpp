#include <catch_amalgamated.hpp>
#include <nlohmann/json.hpp>
#include "Measurements.h"

#include <chrono>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;
namespace fs = std::filesystem;

std::string timestamp() {
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::ostringstream text;
    text << std::put_time(std::gmtime(&now), "%Y-%m-%dT%H:%M:%SZ");
    return text.str();
}
double milliseconds(Clock::time_point since) {
    return std::chrono::duration<double, std::milli>(Clock::now() - since).count();
}
void save(const fs::path& path, const Json& data) {
    if (path.has_parent_path()) fs::create_directories(path.parent_path());
    std::ofstream file(path);
    file << data.dump(2) << '\n';
    if (!file) throw std::runtime_error("Cannot write report: " + path.string());
}
Json counts(const Catch::Counts& value) {
    return {{"total", value.total()}, {"passed", value.passed}, {"failed", value.failed},
            {"skipped", value.skipped}, {"expected_failures", value.failedButOk}};
}
std::string slug(const std::string& name) {
    std::string result;
    for (unsigned char ch : name) {
        if (std::isalnum(ch)) result += static_cast<char>(std::tolower(ch));
        else if (!result.empty() && result.back() != '-') result += '-';
    }
    if (!result.empty() && result.back() == '-') result.pop_back();
    return result.empty() ? "test" : result;
}

class JsonReports : public Catch::EventListenerBase {
    Clock::time_point runStart, caseStart;
    std::string startedAt, caseStartedAt;
    Json tests = Json::array(), failures = Json::array();
public:
    using EventListenerBase::EventListenerBase;
    void testRunStarting(const Catch::TestRunInfo&) override {
        runStart = Clock::now();
        startedAt = timestamp();
    }
    void testCaseStarting(const Catch::TestCaseInfo&) override {
        failures = Json::array();
        test_report::memoryCount = test_report::frameCount = 0;
        test_report::samplesTruncated = false;
        caseStartedAt = timestamp();
        caseStart = Clock::now();
    }
    void assertionEnded(const Catch::AssertionStats& stats) override {
        const auto& result = stats.assertionResult;
        if (result.isOk()) return;
        Json messages = Json::array();
        for (const auto& message : stats.infoMessages) messages.push_back(message.message);
        failures.push_back({{"expression", result.getExpression()},
            {"expanded", result.getExpandedExpression()}, {"message", result.getMessage()},
            {"file", result.getSourceInfo().file}, {"line", result.getSourceInfo().line}, {"context", messages}});
    }
    void testCaseEnded(const Catch::TestCaseStats& stats) override {
        const double duration = milliseconds(caseStart);
        const auto& info = *stats.testInfo;
        const std::string suite = fs::path(info.lineInfo.file).stem().string();
        Json tags = Json::array(), memory = Json::array(), frames = Json::array();
        std::string category;
        for (const auto& tag : info.tags) {
            const std::string name(tag.original.data(), tag.original.size());
            tags.push_back(name);
            if (name == "unit" || name == "integration" || name == "e2e") category = name;
        }
        for (std::size_t i = 0; i < test_report::memoryCount; ++i) {
            const auto& s = test_report::memorySamples[i];
            memory.push_back({{"step", i}, {"phase", s.phase}, {"sdl_baseline", s.sdlBaseline},
                {"sdl_current", s.sdlCurrent}, {"sdl_delta", s.sdlCurrent - s.sdlBaseline},
                {"cpp_baseline", s.cppBaseline}, {"cpp_current", s.cppCurrent}, {"cpp_delta", s.cppCurrent - s.cppBaseline}});
        }
        for (std::size_t i = 0; i < test_report::frameCount; ++i) {
            const auto& s = test_report::frameSamples[i];
            frames.push_back({{"step", i}, {"limit_fps", s.limit}, {"measured_fps", s.fps},
                {"current_fps", s.currentFPS}, {"updates_per_second", s.updatesPerSecond},
                {"duration_seconds", s.seconds}, {"frames", s.frames}, {"updates", s.updates}});
        }
        const std::string status = stats.totals.testCases.failed ? "failed" :
            (stats.totals.testCases.skipped ? "skipped" : "passed");
        Json report = {{"schema_version", 1}, {"framework", "Catch2"},
            {"started_at", caseStartedAt}, {"run_started_at", startedAt},
            {"category", category}, {"suite", suite}, {"name", info.name}, {"tags", tags},
            {"status", status}, {"duration_ms", duration}, {"assertions", counts(stats.totals.assertions)},
            {"source", {{"file", info.lineInfo.file}, {"line", info.lineInfo.line}}}, {"failures", failures},
            {"measurements", {{"memory", memory}, {"frame_timing", frames}, {"truncated", test_report::samplesTruncated}}}};
        const auto path = fs::path(suite) / slug(info.name) / "out.json";
        report["report_path"] = path.generic_string();
        save(path, report);
        tests.push_back(std::move(report));
    }
    void testRunEnded(const Catch::TestRunStats& stats) override {
        save("out.json", {{"schema_version", 1}, {"framework", "Catch2"}, {"started_at", startedAt},
            {"duration_ms", milliseconds(runStart)}, {"summary", counts(stats.totals.testCases)},
            {"assertions", counts(stats.totals.assertions)}, {"tests", tests}});
    }
};

int summarize(const fs::path& root) {
    Json reports = Json::array(), missing = Json::array();
    unsigned total = 0, passed = 0, failed = 0, skipped = 0;
    for (const char* category : {"unit", "integration", "e2e"}) {
        std::ifstream file(root / category / "out.json");
        if (!file) { missing.push_back(category); continue; }
        Json report;
        file >> report;
        const auto& summary = report.at("summary");
        total += summary.at("total").get<unsigned>();
        passed += summary.at("passed").get<unsigned>();
        failed += summary.at("failed").get<unsigned>();
        skipped += summary.at("skipped").get<unsigned>();
        report["category"] = category;
        reports.push_back(std::move(report));
    }
    const bool complete = missing.empty();
    save(root / "out.json", {{"schema_version", 1}, {"framework", "Catch2"}, {"created_at", timestamp()},
        {"complete", complete}, {"missing_categories", missing},
        {"summary", {{"total", total}, {"passed", passed}, {"failed", failed}, {"skipped", skipped}}},
        {"categories", reports}});
    std::cout << "Passed tests: " << passed << " / " << total << "\nFailed tests: " << failed << " / " << total
              << "\nSkipped tests: " << skipped << " / " << total << '\n';
    if (!complete) std::cerr << "Incomplete run; missing categories: " << missing.dump() << '\n';
    return !complete || !total || failed || skipped ? 1 : 0;
}
} // namespace

CATCH_REGISTER_LISTENER(JsonReports)

int main(int argc, char** argv) {
    try {
        if (argc == 3 && std::string(argv[1]) == "--summarize") return summarize(argv[2]);
        return Catch::Session().run(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "Test runner error: " << error.what() << '\n';
        return 2;
    }
}
