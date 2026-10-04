#include "solver/io/mps_reader.hpp"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <map>
#include <cctype>
namespace solver {
bool read_mps(const std::string& path,LinearModel& m,std::string& e){std::ifstream in(path);if(!in){e="cannot open MPS file";return false;}enum Sec{NONE,ROWS,COLUMNS,RHS,BOUNDS};Sec sec=NONE;std::string line;std::unordered_map<std::string,int> row;std::vector<std::tuple<std::string,std::string,Real>> entries;std::unordered_map<std::string,Real> rhs; std::unordered_map<std::string,char> sense;std::unordered_map<std::string,Real> lo,up;std::unordered_map<std::string,bool> haslo,hasup;std::vector<std::string> vars;std::unordered_map<std::string,int> vi;std::string objrow;
 int ln=0;while(std::getline(in,line)){
  ++ln;
  if(line.empty()||line[0]=='*') continue;

  // Section headers are standalone records. In particular, Netlib RHS data
  // records commonly begin with the RHS vector name itself (e.g.
  // "RHS 2 132. 3 446."), so testing only the first token would incorrectly
  // discard every RHS data record.
  std::istringstream header(line);
  std::string a,b;
  header>>a;
  header>>b;
  const bool standalone=(b.empty() && !header);
  if(standalone){
    if(a=="NAME"){e="MPS NAME header missing problem name at line "+std::to_string(ln);return false;}
    if(a=="ROWS"){sec=ROWS;continue;}
    if(a=="COLUMNS"){sec=COLUMNS;continue;}
    if(a=="RHS"){sec=RHS;continue;}
    if(a=="BOUNDS"){sec=BOUNDS;continue;}
    if(a=="ENDATA") break;
  }
  if(a=="NAME"){
    std::istringstream s(line);
    s>>a>>m.name;
    if(!s){e="MPS NAME parse error at line "+std::to_string(ln);return false;}
    continue;
  }
 if(sec==ROWS){std::string typ,name;std::istringstream s(line);s>>typ>>name;if(typ.size()!=1){e="MPS ROWS parse error at line "+std::to_string(ln);return false;}char t=typ[0];if(t=='N'){if(objrow.empty())objrow=name;else row[name]=(int)m.constraints.size();}else if(t=='L'||t=='G'||t=='E'){Constraint c;c.name=name;if(t=='L')c.upper_bound=0;else if(t=='G')c.lower_bound=0;else c.lower_bound=c.upper_bound=0;row[name]=(int)m.constraints.size();sense[name]=t;m.constraints.push_back(c);}else{e="unsupported ROW type at line "+std::to_string(ln);return false;}}
 else if(sec==COLUMNS){std::string var,r1;Real v1;std::istringstream s(line);s>>var>>r1>>v1;if(!s){e="MPS COLUMNS parse error at line "+std::to_string(ln);return false;}if(!vi.count(var)){vi[var]=(int)vars.size();vars.push_back(var);}entries.emplace_back(var,r1,v1);std::string rr;Real vv;while(s>>rr>>vv)entries.emplace_back(var,rr,vv);}
 else if(sec==RHS){std::istringstream s(line);std::string first,second;Real value;if(!(s>>first)) continue;if(s>>value){rhs[first]=value;std::string rr;Real vv;while(s>>rr>>vv)rhs[rr]=vv;}else{std::istringstream t(line);std::string set,r1;Real v1;t>>set>>r1>>v1;if(!t){e="MPS RHS parse error at line "+std::to_string(ln);return false;}rhs[r1]=v1;std::string rr;Real vv;while(t>>rr>>vv)rhs[rr]=vv;}}
 else if(sec==BOUNDS){std::string typ,set,var;Real val=0;std::istringstream s(line);s>>typ>>set>>var;if(!s){e="MPS BOUNDS parse error at line "+std::to_string(ln);return false;}if(typ!="LO"&&typ!="UP"&&typ!="FX"&&typ!="FR"){e="unsupported BOUNDS type "+typ;return false;}if(typ!="FR"&&! (s>>val)){e="missing bound value at line "+std::to_string(ln);return false;}if(!vi.count(var)){vi[var]=(int)vars.size();vars.push_back(var);}if(typ=="LO"){lo[var]=val;haslo[var]=true;}else if(typ=="UP"){up[var]=val;hasup[var]=true;}else if(typ=="FX"){lo[var]=up[var]=val;haslo[var]=hasup[var]=true;}else{lo[var]=-kInfinity;up[var]=kInfinity;haslo[var]=hasup[var]=true;}}
 }
m.variables.resize(vars.size());for(std::size_t i=0;i<vars.size();++i){m.variables[i].name=vars[i];if(haslo[vars[i]])m.variables[i].lower_bound=lo[vars[i]];if(hasup[vars[i]])m.variables[i].upper_bound=up[vars[i]];}
std::vector<Real> val;std::vector<Index> ri,cp(vars.size()+1,0);std::vector<std::vector<std::pair<Index,Real>>> cols(vars.size());for(auto [v,r,x]:entries){auto j=vi[v];if(r==objrow){m.variables[j].objective=x;}else {auto it=row.find(r);if(it!=row.end()){auto i=it->second;cols[j].push_back({i,x});}}}for(std::size_t j=0;j<cols.size();++j){
 std::map<Index,Real> aggregated;
 for(auto [i,x]:cols[j]) aggregated[i]+=x;
 for(auto [i,x]:aggregated) if(x!=0){ri.push_back(i);val.push_back(x);}
 cp[j+1]=val.size();
}for(std::size_t i=0;i<m.constraints.size();++i){auto it=rhs.find(m.constraints[i].name);if(it!=rhs.end()){char st=sense[m.constraints[i].name]; if(st=='L') m.constraints[i].upper_bound=it->second; else if(st=='G') m.constraints[i].lower_bound=it->second; else if(st=='E') m.constraints[i].lower_bound=m.constraints[i].upper_bound=it->second;}}m.A=CscMatrix(m.constraints.size(),m.variables.size(),std::move(val),std::move(ri),std::move(cp));return m.validate(e);}
}
