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
/// returns -1 if a<b, 0 if a==b, +1 if a>b.
/// Splits on non-alphanumeric boundaries, compares numeric segments numerically.
static int versionCompare(const std::string& a, const std::string& b) {
    std::regex re(R"([\.\-+])");
    std::sregex_token_iterator ai(a.begin(), a.end(), re, -1), ae;
    std::sregex_token_iterator bi(b.begin(), b.end(), re, -1), be;
    for (; ai != ae && bi != be; ++ai, ++bi) {
        const std::string& sa = *ai, & sb = *bi;
        // numeric?
        if (std::all_of(sa.begin(), sa.end(), ::isdigit) &&
            std::all_of(sb.begin(), sb.end(), ::isdigit))
        {
            long na = std::stol(sa), nb = std::stol(sb);
            if (na < nb) return -1;
            if (na > nb) return +1;
        } else {
            int cmp = sa.compare(sb);
            if (cmp < 0) return -1;
            if (cmp > 0) return +1;
        }
    }
    // one string longer?
    if (ai != ae) return +1;
    if (bi != be) return -1;
    return 0;
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
