#include "matcher.h"
#include <iostream>
#include <string>
#include <ctime>
#include <iomanip>

// ============================================================================
// Campus Lost & Found — CLI Entry Point
// ============================================================================
// Usage:
//   campus_matcher.exe [path-to-items.json] [path-to-matches.json]
//
// Defaults:
//   items.json   = ../data/items.json
//   matches.json = ../data/matches.json
//
// This program loads the items database, runs the smart matching algorithm,
// and writes results to a separate matches.json file.  The frontend reads
// both files to display items and their match scores.
// ============================================================================

// Get current ISO timestamp (used for match created_at)
static std::string nowISO() {
    std::time_t t = std::time(nullptr);
    std::tm* tm = std::localtime(&t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", tm);
    return std::string(buf);
}

int main(int argc, char* argv[]) {
    std::cout << "========================================\n";
    std::cout << "  Campus Lost & Found - Smart Matcher\n";
    std::cout << "========================================\n\n";

    std::string itemsPath   = "../data/items.json";
    std::string matchesPath = "../data/matches.json";

    if (argc > 1) itemsPath   = argv[1];
    if (argc > 2) matchesPath = argv[2];

    campus_lf::Matcher matcher;

    // 1. Load items
    std::cout << "[1/3] Loading items from: " << itemsPath << "\n";
    if (!matcher.loadItems(itemsPath)) {
        std::cerr << "Error: Failed to load items.\n";
        return 1;
    }

    // 2. Compute matches
    std::cout << "\n[2/3] Running smart matching algorithm...\n";
    std::string timestamp = nowISO();
    auto results = matcher.computeMatches();

    // Set timestamps on all matches
    for (auto& m : results) {
        m.created_at = timestamp;
    }

    std::cout << "       Compared " << matcher.lostCount() << " lost x "
              << matcher.foundCount() << " found = "
              << (matcher.lostCount() * matcher.foundCount())
              << " pairs.\n";
    std::cout << "       Found " << results.size() << " match(es) above threshold.\n";

    // 3. Save results
    std::cout << "\n[3/3] Saving results to: " << matchesPath << "\n";
    if (!matcher.saveResults(matchesPath, results)) {
        std::cerr << "Error: Failed to save results.\n";
        return 1;
    }

    // 4. Print summary
    std::cout << "\n========================================\n";
    std::cout << "  RESULTS SUMMARY\n";
    std::cout << "========================================\n";
    std::cout << "  Lost items loaded:   " << matcher.lostCount()  << "\n";
    std::cout << "  Found items loaded:  " << matcher.foundCount() << "\n";
    std::cout << "  Matches generated:   " << results.size()       << "\n";

    if (!results.empty()) {
        std::cout << "  Highest score:       " << results[0].score << "%"
                  << " (" << results[0].lost_id << " <-> "
                  << results[0].found_id << ")\n";
        std::cout << "  Lowest score:        " << results.back().score << "%\n";
        std::cout << "\n  Top matches:\n";

        int shown = 0;
        for (const auto& m : results) {
            if (shown >= 5) break;
            std::cout << "    " << std::setw(3) << m.score << "% | "
                      << m.lost_id << " <-> " << m.found_id << " | ";
            for (size_t j = 0; j < m.matched_on.size(); ++j) {
                std::cout << m.matched_on[j];
                if (j + 1 < m.matched_on.size()) std::cout << ", ";
            }
            std::cout << "\n";
            ++shown;
        }
    }

    std::cout << "\n  Output file: " << matchesPath << "\n";
    std::cout << "========================================\n";
    std::cout << "Done!\n";
    return 0;
}
