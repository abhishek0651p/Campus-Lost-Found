#include "matcher.h"
#include <algorithm>
#include <sstream>
#include <cmath>
#include <set>
#include <cctype>
#include <fstream>
#include <iostream>

// ============================================================================
// Campus Lost & Found — Smart Matching Engine  (Implementation)
// ============================================================================
// This file implements the matching algorithm.  Each pair (lost, found) is
// scored on multiple weighted dimensions and a composite 0–100 score is
// produced.  The weights below can be tuned.
// ============================================================================

namespace campus_lf {

// ---- Weight configuration --------------------------------------------------
// Weights should sum to 1.0 for easy percentage interpretation.
static const double W_CATEGORY    = 0.25;
static const double W_COLOR       = 0.10;
static const double W_BRAND       = 0.10;
static const double W_LOCATION    = 0.20;
static const double W_DATE        = 0.10;
static const double W_DESCRIPTION = 0.15;
static const double W_TAGS        = 0.10;

// ---- Constructor -----------------------------------------------------------

Matcher::Matcher() {}

// ---- Helpers ---------------------------------------------------------------

std::string Matcher::toLower(const std::string& s) const {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return out;
}

std::vector<std::string> Matcher::tokenize(const std::string& text) const {
    std::vector<std::string> tokens;
    std::string lower = toLower(text);
    std::string token;

    for (char c : lower) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            token += c;
        } else if (!token.empty()) {
            // Skip very short noise words
            if (token.size() > 2) {
                tokens.push_back(token);
            }
            token.clear();
        }
    }
    if (token.size() > 2) {
        tokens.push_back(token);
    }
    return tokens;
}

// ---- Scoring Functions -----------------------------------------------------

double Matcher::scoreCategory(const Item& lost, const Item& found) const {
    if (toLower(lost.category) == toLower(found.category)) return 1.0;
    return 0.0;
}

double Matcher::scoreColor(const Item& lost, const Item& found) const {
    if (lost.color.empty() || found.color.empty()) return 0.0;
    if (toLower(lost.color) == toLower(found.color)) return 1.0;
    return 0.0;
}

double Matcher::scoreBrand(const Item& lost, const Item& found) const {
    if (lost.brand.empty() || found.brand.empty()) return 0.0;
    if (toLower(lost.brand) == toLower(found.brand)) return 1.0;
    return 0.0;
}

double Matcher::scoreLocation(const Item& lost, const Item& found) const {
    std::string lLoc = toLower(lost.location);
    std::string fLoc = toLower(found.location);

    // Exact match
    if (lLoc == fLoc) return 1.0;

    // Extract building name (text before the first " - ")
    auto getBuilding = [](const std::string& loc) -> std::string {
        size_t pos = loc.find(" - ");
        if (pos != std::string::npos) return loc.substr(0, pos);
        return loc;
    };

    std::string lBuild = getBuilding(lLoc);
    std::string fBuild = getBuilding(fLoc);

    if (lBuild == fBuild) return 0.7;

    // Check if any significant word overlaps (e.g. "library", "canteen")
    auto lTokens = tokenize(lLoc);
    auto fTokens = tokenize(fLoc);
    for (const auto& lt : lTokens) {
        for (const auto& ft : fTokens) {
            if (lt == ft && lt.size() > 3) return 0.4;
        }
    }

    return 0.0;
}

double Matcher::scoreDate(const Item& lost, const Item& found) const {
    // Simple day-difference heuristic (parse YYYY-MM-DD)
    auto parseDays = [](const std::string& d) -> int {
        if (d.size() < 10) return -1;
        int y = std::stoi(d.substr(0, 4));
        int m = std::stoi(d.substr(5, 2));
        int day = std::stoi(d.substr(8, 2));
        return y * 365 + m * 30 + day;  // rough approximation
    };

    int ld = parseDays(lost.date);
    int fd = parseDays(found.date);
    if (ld < 0 || fd < 0) return 0.0;

    int diff = std::abs(ld - fd);
    if (diff == 0) return 1.0;
    if (diff == 1) return 0.8;
    if (diff <= 3) return 0.5;
    if (diff <= 7) return 0.2;
    return 0.0;
}

double Matcher::scoreDescription(const Item& lost, const Item& found) const {
    auto lTokens = tokenize(lost.description);
    auto fTokens = tokenize(found.description);

    if (lTokens.empty() || fTokens.empty()) return 0.0;

    std::set<std::string> fSet(fTokens.begin(), fTokens.end());
    int hits = 0;
    for (const auto& t : lTokens) {
        if (fSet.count(t)) hits++;
    }

    double ratio = static_cast<double>(hits) / lTokens.size();
    return std::min(ratio * 1.5, 1.0);  // boost slightly, cap at 1
}

double Matcher::scoreTags(const Item& lost, const Item& found) const {
    if (lost.tags.empty() || found.tags.empty()) return 0.0;

    std::set<std::string> lSet, fSet;
    for (const auto& t : lost.tags)  lSet.insert(toLower(t));
    for (const auto& t : found.tags) fSet.insert(toLower(t));

    int intersection = 0;
    for (const auto& t : lSet) {
        if (fSet.count(t)) intersection++;
    }

    int unionSize = static_cast<int>(lSet.size() + fSet.size()) - intersection;
    if (unionSize == 0) return 0.0;

    return static_cast<double>(intersection) / unionSize;  // Jaccard
}

// ---- Main Matching ---------------------------------------------------------

std::vector<MatchResult> Matcher::computeMatches() {
    std::vector<MatchResult> results;
    int matchCounter = 1;

    for (const auto& lost : lostItems_) {
        for (const auto& found : foundItems_) {
            // Compute individual scores
            double sCat   = scoreCategory(lost, found);
            double sCol   = scoreColor(lost, found);
            double sBrand = scoreBrand(lost, found);
            double sLoc   = scoreLocation(lost, found);
            double sDate  = scoreDate(lost, found);
            double sDesc  = scoreDescription(lost, found);
            double sTags  = scoreTags(lost, found);

            // Weighted composite
            double composite = sCat   * W_CATEGORY
                             + sCol   * W_COLOR
                             + sBrand * W_BRAND
                             + sLoc   * W_LOCATION
                             + sDate  * W_DATE
                             + sDesc  * W_DESCRIPTION
                             + sTags  * W_TAGS;

            int score = static_cast<int>(std::round(composite * 100));

            // Only keep matches above a minimum threshold
            if (score < 30) continue;

            MatchResult mr;
            mr.match_id = "M" + std::to_string(matchCounter++);
            mr.lost_id  = lost.id;
            mr.found_id = found.id;
            mr.score    = score;
            mr.status   = "pending";

            // Record which criteria contributed
            if (sCat   > 0.5) mr.matched_on.push_back("category");
            if (sCol   > 0.5) mr.matched_on.push_back("color");
            if (sBrand > 0.5) mr.matched_on.push_back("brand");
            if (sLoc   > 0.3) mr.matched_on.push_back("location_proximity");
            if (sDate  > 0.3) mr.matched_on.push_back("date_proximity");
            if (sDesc  > 0.2) mr.matched_on.push_back("description_keywords");
            if (sTags  > 0.2) mr.matched_on.push_back("tags_overlap");

            results.push_back(mr);
        }
    }

    // Sort by score descending
    std::sort(results.begin(), results.end(),
              [](const MatchResult& a, const MatchResult& b) {
                  return a.score > b.score;
              });

    return results;
}

// ---- JSON I/O (hand-rolled, no external dependencies) ---------------------
// Parses the known structure of items.json.  This is NOT a general-purpose
// JSON parser — it is tailored to our specific schema so it stays compact
// and dependency-free for a college project.

// ---------- Tiny helpers for JSON parsing -----------------------------------

// Skip whitespace characters in the string starting at position pos.
static size_t skipWS(const std::string& s, size_t pos) {
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\n' ||
           s[pos] == '\r' || s[pos] == '\t'))
        ++pos;
    return pos;
}

// Parse a JSON-quoted string starting at pos (which must point at the opening
// quote).  Returns the unescaped content and advances pos past the closing
// quote.
static std::string parseString(const std::string& s, size_t& pos) {
    if (pos >= s.size() || s[pos] != '"') return "";
    ++pos; // skip opening "
    std::string result;
    while (pos < s.size() && s[pos] != '"') {
        if (s[pos] == '\\' && pos + 1 < s.size()) {
            ++pos;
            switch (s[pos]) {
                case '"':  result += '"';  break;
                case '\\': result += '\\'; break;
                case '/':  result += '/';  break;
                case 'n':  result += '\n'; break;
                case 't':  result += '\t'; break;
                case 'r':  result += '\r'; break;
                default:   result += s[pos]; break;
            }
        } else {
            result += s[pos];
        }
        ++pos;
    }
    if (pos < s.size()) ++pos; // skip closing "
    return result;
}

// Parse a JSON array of strings: ["a", "b", "c"]
static std::vector<std::string> parseStringArray(const std::string& s,
                                                  size_t& pos) {
    std::vector<std::string> arr;
    pos = skipWS(s, pos);
    if (pos >= s.size() || s[pos] != '[') return arr;
    ++pos; // skip '['
    while (pos < s.size()) {
        pos = skipWS(s, pos);
        if (s[pos] == ']') { ++pos; break; }
        if (s[pos] == ',') { ++pos; continue; }
        if (s[pos] == '"') {
            arr.push_back(parseString(s, pos));
        } else {
            ++pos; // skip unexpected char
        }
    }
    return arr;
}

// Parse a single Item object from the JSON string.  pos must point at '{'.
static Item parseItemObject(const std::string& s, size_t& pos) {
    Item item;
    pos = skipWS(s, pos);
    if (pos >= s.size() || s[pos] != '{') return item;
    ++pos; // skip '{'

    while (pos < s.size()) {
        pos = skipWS(s, pos);
        if (s[pos] == '}') { ++pos; break; }
        if (s[pos] == ',') { ++pos; continue; }

        // Parse key
        std::string key = parseString(s, pos);
        pos = skipWS(s, pos);
        if (pos < s.size() && s[pos] == ':') ++pos; // skip ':'
        pos = skipWS(s, pos);

        // Parse value based on key
        if (key == "tags") {
            item.tags = parseStringArray(s, pos);
        } else if (pos < s.size() && s[pos] == '"') {
            std::string val = parseString(s, pos);
            if      (key == "id")          item.id = val;
            else if (key == "title")       item.title = val;
            else if (key == "category")    item.category = val;
            else if (key == "description") item.description = val;
            else if (key == "location")    item.location = val;
            else if (key == "date")        item.date = val;
            else if (key == "time")        item.time = val;
            else if (key == "reporter")    item.reporter = val;
            else if (key == "email")       item.email = val;
            else if (key == "status")      item.status = val;
            else if (key == "color")       item.color = val;
            else if (key == "brand")       item.brand = val;
            // else: skip unknown string fields
        } else {
            // Skip non-string, non-array values (numbers, booleans, etc.)
            while (pos < s.size() && s[pos] != ',' && s[pos] != '}')
                ++pos;
        }
    }
    return item;
}

// Parse an array of Item objects.  pos must point at '['.
static std::vector<Item> parseItemArray(const std::string& s, size_t& pos) {
    std::vector<Item> items;
    pos = skipWS(s, pos);
    if (pos >= s.size() || s[pos] != '[') return items;
    ++pos; // skip '['

    while (pos < s.size()) {
        pos = skipWS(s, pos);
        if (s[pos] == ']') { ++pos; break; }
        if (s[pos] == ',') { ++pos; continue; }
        if (s[pos] == '{') {
            items.push_back(parseItemObject(s, pos));
        } else {
            ++pos;
        }
    }
    return items;
}

// ---------- loadItems -------------------------------------------------------

bool Matcher::loadItems(const std::string& jsonPath) {
    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        std::cerr << "[Matcher] Could not open: " << jsonPath << std::endl;
        return false;
    }

    // Read entire file into a string
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    if (content.empty()) {
        std::cerr << "[Matcher] File is empty: " << jsonPath << std::endl;
        return false;
    }

    // Find and parse "lost_items" array
    size_t lostPos = content.find("\"lost_items\"");
    if (lostPos != std::string::npos) {
        lostPos = content.find('[', lostPos);
        if (lostPos != std::string::npos) {
            lostItems_ = parseItemArray(content, lostPos);
        }
    }

    // Find and parse "found_items" array
    size_t foundPos = content.find("\"found_items\"");
    if (foundPos != std::string::npos) {
        foundPos = content.find('[', foundPos);
        if (foundPos != std::string::npos) {
            foundItems_ = parseItemArray(content, foundPos);
        }
    }

    std::cout << "[Matcher] Loaded " << lostItems_.size()
              << " lost item(s) and " << foundItems_.size()
              << " found item(s)." << std::endl;

    return (!lostItems_.empty() || !foundItems_.empty());
}

// ---------- saveResults -----------------------------------------------------
// Writes matches to a separate matches.json file that the frontend reads.

static std::string escapeJSON(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 10);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

bool Matcher::saveResults(const std::string& outputPath,
                          const std::vector<MatchResult>& results) {
    std::ofstream out(outputPath);
    if (!out.is_open()) {
        std::cerr << "[Matcher] Could not write to: " << outputPath << std::endl;
        return false;
    }

    out << "{\n  \"matches\": [\n";
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& m = results[i];
        out << "    {\n";
        out << "      \"id\": \""         << escapeJSON(m.match_id)   << "\",\n";
        out << "      \"lost_id\": \""    << escapeJSON(m.lost_id)    << "\",\n";
        out << "      \"found_id\": \""   << escapeJSON(m.found_id)   << "\",\n";
        out << "      \"score\": "        << m.score                  << ",\n";

        // matched_on array
        out << "      \"matched_on\": [";
        for (size_t j = 0; j < m.matched_on.size(); ++j) {
            out << "\"" << escapeJSON(m.matched_on[j]) << "\"";
            if (j + 1 < m.matched_on.size()) out << ", ";
        }
        out << "],\n";

        out << "      \"status\": \""     << escapeJSON(m.status)     << "\",\n";
        out << "      \"created_at\": \"" << escapeJSON(m.created_at) << "\"\n";
        out << "    }";
        if (i + 1 < results.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n}\n";

    out.close();
    std::cout << "[Matcher] Saved " << results.size()
              << " match(es) to: " << outputPath << std::endl;
    return true;
}

} // namespace campus_lf

