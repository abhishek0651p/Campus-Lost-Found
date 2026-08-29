#ifndef MATCHER_H
#define MATCHER_H

#include <string>
#include <vector>
#include <map>

// ============================================================================
// Campus Lost & Found — Smart Matching Engine
// ============================================================================
// This header defines the core data structures and matching interface.
// The matcher compares lost and found item reports and computes a similarity
// score (0–100) based on multiple weighted criteria.
// ============================================================================

namespace campus_lf {

// ---- Data Structures -------------------------------------------------------

struct Item {
    std::string id;
    std::string title;
    std::string category;
    std::string description;
    std::string location;
    std::string date;       // ISO format: YYYY-MM-DD
    std::string time;       // HH:MM
    std::string reporter;
    std::string email;
    std::string status;     // "open", "matched", "resolved"
    std::string color;
    std::string brand;
    std::vector<std::string> tags;
};

struct MatchResult {
    std::string match_id;
    std::string lost_id;
    std::string found_id;
    int    score;           // 0–100
    std::vector<std::string> matched_on;  // which criteria contributed
    std::string status;     // "pending", "confirmed", "rejected"
    std::string created_at; // ISO timestamp
};

// ---- Matching Engine -------------------------------------------------------

class Matcher {
public:
    Matcher();

    // Load items from a JSON file into internal vectors
    bool loadItems(const std::string& jsonPath);

    // Run the matching algorithm on all loaded items
    std::vector<MatchResult> computeMatches();

    // Write match results to a separate JSON file
    bool saveResults(const std::string& outputPath,
                     const std::vector<MatchResult>& results);

    // Accessors for loaded item counts
    size_t lostCount() const  { return lostItems_.size(); }
    size_t foundCount() const { return foundItems_.size(); }

private:
    std::vector<Item> lostItems_;
    std::vector<Item> foundItems_;

    // --- Individual scoring functions (each returns 0.0 – 1.0) ---

    // Exact or fuzzy category match
    double scoreCategory(const Item& lost, const Item& found) const;

    // Color similarity
    double scoreColor(const Item& lost, const Item& found) const;

    // Brand match
    double scoreBrand(const Item& lost, const Item& found) const;

    // Location proximity (exact match, same building, same area)
    double scoreLocation(const Item& lost, const Item& found) const;

    // Date proximity (same day, ±1 day, ±3 days, etc.)
    double scoreDate(const Item& lost, const Item& found) const;

    // Keyword overlap in descriptions
    double scoreDescription(const Item& lost, const Item& found) const;

    // Tag overlap (Jaccard similarity)
    double scoreTags(const Item& lost, const Item& found) const;

    // --- Helpers ---
    std::vector<std::string> tokenize(const std::string& text) const;
    std::string toLower(const std::string& s) const;
};

} // namespace campus_lf

#endif // MATCHER_H
