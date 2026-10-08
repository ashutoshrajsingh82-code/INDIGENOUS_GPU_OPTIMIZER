#include "solver/io/mps_reader.hpp"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <map>
#include <tuple>
#include <vector>
#include <string>

namespace solver {

bool read_mps(const std::string& path, LinearModel& m, std::string& e) {
  std::ifstream in(path);
  if (!in) {
    e = "cannot open MPS file";
    return false;
  }

  enum Sec { NONE, ROWS, COLUMNS, RHS, BOUNDS };
  Sec sec = NONE;

  std::string line;
  std::unordered_map<std::string, int> row;
  std::vector<std::tuple<std::string, std::string, Real>> entries;
  std::unordered_map<std::string, Real> rhs;
  std::unordered_map<std::string, char> sense;
  std::unordered_map<std::string, Real> lo, up;
  std::unordered_map<std::string, bool> haslo, hasup;
  std::vector<std::string> vars;
  std::unordered_map<std::string, int> vi;
  std::string objrow;

  int ln = 0;

  while (std::getline(in, line)) {
    ++ln;

    if (line.empty()) {
      continue;
    }

    // MPS comments are traditionally marked by '*' in column 1. Also accept
    // indented comments and '$' comment cards emitted by some exporters.
    const auto first_non_space = line.find_first_not_of(" \t\r");
    if (first_non_space == std::string::npos) {
      continue;
    }
    if (line[first_non_space] == '*' || line[first_non_space] == '$') {
      continue;
    }

    // Section headers are standalone records. Do not classify an RHS data
    // record such as "RHS 2 132. 3 446." as the RHS section header.
    std::istringstream header(line);
    std::string a, b;
    header >> a;
    header >> b;
    const bool standalone = b.empty() && !header;

    if (standalone) {
      if (a == "ROWS") {
        sec = ROWS;
        continue;
      }
      if (a == "COLUMNS") {
        sec = COLUMNS;
        continue;
      }
      if (a == "RHS") {
        sec = RHS;
        continue;
      }
      if (a == "BOUNDS") {
        sec = BOUNDS;
        continue;
      }
      if (a == "ENDATA") {
        break;
      }
    }

    if (a == "NAME") {
      std::istringstream s(line);
      s >> a >> m.name;
      if (!s) {
        e = "MPS NAME parse error at line " + std::to_string(ln);
        return false;
      }
      continue;
    }

    if (sec == ROWS) {
      std::string typ, name;
      std::istringstream s(line);
      s >> typ >> name;
      if (typ.size() != 1 || name.empty()) {
        e = "MPS ROWS parse error at line " + std::to_string(ln);
        return false;
      }

      const char t = typ[0];
      if (t == 'N') {
        if (objrow.empty()) {
          objrow = name;
        } else {
          row[name] = static_cast<int>(m.constraints.size());
        }
      } else if (t == 'L' || t == 'G' || t == 'E') {
        Constraint c;
        c.name = name;
        if (t == 'L') {
          c.upper_bound = 0;
        } else if (t == 'G') {
          c.lower_bound = 0;
        } else {
          c.lower_bound = c.upper_bound = 0;
        }

        row[name] = static_cast<int>(m.constraints.size());
        sense[name] = t;
        m.constraints.push_back(c);
      } else {
        e = "unsupported ROW type at line " + std::to_string(ln);
        return false;
      }
    } else if (sec == COLUMNS) {
      // Support free-field and classic fixed-field MPS COLUMNS records.
      // Fixed-field positions are:
      //   variable 5-12, row1 15-22, value1 25-36,
      //   row2 40-47, value2 50-61.
      std::string var, r1;
      Real v1 = 0;
      std::istringstream s(line);
      s >> var >> r1;

      if (!s) {
        e = "MPS COLUMNS parse error at line " + std::to_string(ln) +
            ": " + line;
        return false;
      }

      // Integer-section marker records are structural. This solver currently
      // operates on continuous LP models, so INTORG/INTEND markers are ignored.
      if (r1 == "'MARKER'" || r1 == "MARKER") {
        continue;
      }

      if (!(s >> v1)) {
        // Recover value1 from the canonical fixed-field location.
        bool recovered = false;
        if (line.size() >= 36) {
          std::istringstream fs(line.substr(24, 12));
          if (fs >> v1) {
            recovered = true;
          }
        }

        if (!recovered) {
          e = "MPS COLUMNS numeric value parse error at line " +
              std::to_string(ln) + ": " + line;
          return false;
        }
      }

      if (!vi.count(var)) {
        vi[var] = static_cast<int>(vars.size());
        vars.push_back(var);
      }
      entries.emplace_back(var, r1, v1);

      // Free-field second row/value pair.
      std::string rr;
      Real vv = 0;
      bool second_added = false;
      while (s >> rr) {
        if (!(s >> vv)) {
          // A fixed-field record may contain a blank optional second value.
          break;
        }
        entries.emplace_back(var, rr, vv);
        second_added = true;
      }

      // Recover an optional second fixed-field pair if whitespace parsing did
      // not already add it.
      if (!second_added && line.size() >= 61) {
        std::istringstream r2s(line.substr(39, 8));
        std::istringstream v2s(line.substr(49, 12));
        std::string rr2;
        Real vv2 = 0;
        if (r2s >> rr2 && v2s >> vv2) {
          entries.emplace_back(var, rr2, vv2);
        }
      }
    } else if (sec == RHS) {
      std::istringstream s(line);
      std::vector<std::string> tok;
      std::string t;
      while (s >> t) {
        tok.push_back(t);
      }

      if (tok.empty()) {
        continue;
      }

      const bool has_set = (tok.size() % 2) == 1;
      const std::size_t start = has_set ? 1 : 0;

      if (tok.size() < start + 2 ||
          ((tok.size() - start) % 2) != 0) {
        e = "MPS RHS parse error at line " + std::to_string(ln);
        return false;
      }

      for (std::size_t k = start; k < tok.size(); k += 2) {
        Real v = 0;
        std::istringstream vs(tok[k + 1]);
        if (!(vs >> v)) {
          e = "MPS RHS value parse error at line " + std::to_string(ln);
          return false;
        }
        rhs[tok[k]] = v;
      }
    } else if (sec == BOUNDS) {
      // Standard MPS BOUNDS supports:
      //   LO lower bound, UP upper bound, FX fixed value,
      //   FR free, MI minus infinity, PL plus infinity,
      //   BV binary, LI integer lower bound, UI integer upper bound.
      //
      // Both free-field and classic fixed-field records are accepted.
      // Fixed-field positions are:
      //   type 2-3, set 5-12, variable 15-22, value 25-36.
      std::string typ, set, var;
      Real val = 0;
      std::istringstream s(line);
      s >> typ >> set >> var;

      if (!s || typ.empty() || set.empty() || var.empty()) {
        // Recover the three identifying fields from fixed-field columns.
        if (line.size() >= 22) {
          typ = line.substr(1, 2);
          set = line.substr(4, 8);
          var = line.substr(14, 8);

          auto trim = [](std::string value) {
            const auto first = value.find_first_not_of(" \t\r");
            if (first == std::string::npos) return std::string{};
            const auto last = value.find_last_not_of(" \t\r");
            return value.substr(first, last - first + 1);
          };

          typ = trim(typ);
          set = trim(set);
          var = trim(var);
        }
      }

      if (typ.empty() || set.empty() || var.empty()) {
        e = "MPS BOUNDS parse error at line " + std::to_string(ln) +
            ": " + line;
        return false;
      }

      if (typ != "LO" && typ != "UP" && typ != "FX" && typ != "FR" &&
          typ != "MI" && typ != "PL" && typ != "BV" && typ != "LI" &&
          typ != "UI") {
        e = "unsupported BOUNDS type " + typ +
            " at line " + std::to_string(ln);
        return false;
      }

      // FR, MI, PL and BV do not require a numeric value.
      // LI/UI/LO/UP/FX require one.
      const bool value_optional =
          typ == "FR" || typ == "MI" || typ == "PL" || typ == "BV";

      if (!value_optional) {
        if (!(s >> val)) {
          // Recover the numeric value from the canonical fixed-field
          // location when whitespace parsing did not expose it.
          bool recovered = false;
          if (line.size() >= 36) {
            std::istringstream fs(line.substr(24, 12));
            if (fs >> val) {
              recovered = true;
            }
          }

          if (!recovered) {
            e = "missing bound value at line " + std::to_string(ln) +
                ": " + line;
            return false;
          }
        }
      }

      if (!vi.count(var)) {
        vi[var] = static_cast<int>(vars.size());
        vars.push_back(var);
      }

      if (typ == "LO" || typ == "LI") {
        lo[var] = val;
        haslo[var] = true;
      } else if (typ == "UP" || typ == "UI") {
        up[var] = val;
        hasup[var] = true;
      } else if (typ == "FX") {
        lo[var] = up[var] = val;
        haslo[var] = hasup[var] = true;
      } else if (typ == "FR") {
        lo[var] = -kInfinity;
        up[var] = kInfinity;
        haslo[var] = hasup[var] = true;
      } else if (typ == "MI") {
        lo[var] = -kInfinity;
        haslo[var] = true;
      } else if (typ == "PL") {
        up[var] = kInfinity;
        hasup[var] = true;
      } else if (typ == "BV") {
        lo[var] = 0;
        up[var] = 1;
        haslo[var] = hasup[var] = true;
      }
    }
  }

  m.variables.resize(vars.size());
  for (std::size_t i = 0; i < vars.size(); ++i) {
    m.variables[i].name = vars[i];
    if (haslo[vars[i]]) {
      m.variables[i].lower_bound = lo[vars[i]];
    }
    if (hasup[vars[i]]) {
      m.variables[i].upper_bound = up[vars[i]];
    }
  }

  std::vector<Real> val;
  std::vector<Index> ri;
  std::vector<Index> cp(vars.size() + 1, 0);
  std::vector<std::vector<std::pair<Index, Real>>> cols(vars.size());

  for (const auto& entry : entries) {
    const auto& v = std::get<0>(entry);
    const auto& r = std::get<1>(entry);
    const Real x = std::get<2>(entry);
    const auto j = vi[v];

    if (r == objrow) {
      m.variables[j].objective = x;
    } else {
      const auto it = row.find(r);
      if (it != row.end()) {
        cols[j].push_back({it->second, x});
      }
    }
  }

  for (std::size_t j = 0; j < cols.size(); ++j) {
    std::map<Index, Real> aggregated;
    for (const auto& entry : cols[j]) {
      aggregated[entry.first] += entry.second;
    }

    for (const auto& entry : aggregated) {
      if (entry.second != 0) {
        ri.push_back(entry.first);
        val.push_back(entry.second);
      }
    }
    cp[j + 1] = static_cast<Index>(val.size());
  }

  for (std::size_t i = 0; i < m.constraints.size(); ++i) {
    const auto it = rhs.find(m.constraints[i].name);
    if (it == rhs.end()) {
      continue;
    }

    const char st = sense[m.constraints[i].name];
    if (st == 'L') {
      m.constraints[i].upper_bound = it->second;
    } else if (st == 'G') {
      m.constraints[i].lower_bound = it->second;
    } else if (st == 'E') {
      m.constraints[i].lower_bound = it->second;
      m.constraints[i].upper_bound = it->second;
    }
  }

  m.A = CscMatrix(
      m.constraints.size(),
      m.variables.size(),
      std::move(val),
      std::move(ri),
      std::move(cp));

  return m.validate(e);
}

} // namespace solver
