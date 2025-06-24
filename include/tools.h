//
// Created by cv2 on 6/14/25.
//

#ifndef TOOLS_H
#define TOOLS_H
#include <regex>

class Tools {
public:
                /// A parsed constraint: pkgname, operator (one of "<=",">=","<",">","="), and version
struct Constraint {
    std::string name, op, version;
};

/// Parse "foo>=1.2.3-4" or plain "foo" into its parts
static Constraint parseConstraint(const std::string& s) {
    static const std::vector<std::string> ops = {"<=", ">=", "<", ">", "="};
    for (auto& o : ops) {
        auto pos = s.find(o);
        if (pos != std::string::npos) {
            return { s.substr(0, pos), o, s.substr(pos + o.size()) };
        }
    }
    return { s, "", "" };
}

/// Compare two version strings a and b:
/// - Splits on '.', '-', '+'
/// - Compares numeric segments numerically, other segments lexicographically
/// - Ignores any extra **numeric-only** segments at the end (pkgrel)
///
/// Returns:
///  -1 if a < b
///   0 if a == b (including when one has only a trailing numeric pkgrel)
///  +1 if a > b
static int versionCompare(const std::string& a, const std::string& b) {
    static const std::regex re(R"([\.\-+])");

    auto tokenize = [&](const std::string& v) {
        std::vector<std::string> tok;
        std::sregex_token_iterator it(v.begin(), v.end(), re, -1), end;
        for ( ; it!=end; ++it) {
            tok.push_back(it->str());
        }
        return tok;
    };

    auto ta = tokenize(a);
    auto tb = tokenize(b);
    size_t na = ta.size(), nb = tb.size();
    size_t n  = std::min(na, nb);

    // Compare shared tokens
    for (size_t i = 0; i < n; ++i) {
        const auto& sa = ta[i];
        const auto& sb = tb[i];

        bool na_num = !sa.empty() && std::all_of(sa.begin(), sa.end(), ::isdigit);
        bool nb_num = !sb.empty() && std::all_of(sb.begin(), sb.end(), ::isdigit);

        if (na_num && nb_num) {
            long va = std::stol(sa), vb = std::stol(sb);
            if (va < vb) return -1;
            if (va > vb) return +1;
        } else {
            int cmp = sa.compare(sb);
            if (cmp < 0) return -1;
            if (cmp > 0) return +1;
        }
    }

    // If same length, they’re equal
    if (na == nb) return 0;

    // If a has extra tokens
    if (na > nb) {
        // check if all extra a-tokens are numeric
        for (size_t i = nb; i < na; ++i) {
            const auto& tok = ta[i];
            if (tok.empty() || !std::all_of(tok.begin(), tok.end(), ::isdigit)) {
                return +1;  // non-numeric suffix => a > b
            }
        }
        return 0;  // only numeric suffix => treat equal
    }

    // If b has extra tokens
    if (nb > na) {
        for (size_t i = na; i < nb; ++i) {
            const auto& tok = tb[i];
            if (tok.empty() || !std::all_of(tok.begin(), tok.end(), ::isdigit)) {
                return -1;  // non-numeric suffix => b > a, so a < b
            }
        }
        return 0;  // only numeric suffix => treat equal
    }

    return 0;  // fallback
}


/// Test an installed version vs. a constraint
static bool evalConstraint(const std::string& instVer, const Constraint& c) {
    if (c.op.empty()) return true;
    int cmp = versionCompare(instVer, c.version);
    if      (c.op == "=")  return cmp == 0;
    else if (c.op == "<")  return cmp <  0;
    else if (c.op == "<=") return cmp <= 0;
    else if (c.op == ">")  return cmp >  0;
    else if (c.op == ">=") return cmp >= 0;
    return false;
}
};
#endif //TOOLS_H
