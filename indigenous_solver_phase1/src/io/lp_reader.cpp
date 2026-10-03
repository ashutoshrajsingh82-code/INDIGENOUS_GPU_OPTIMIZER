#include "solver/io/lp_reader.hpp"
#include <fstream>
#include <sstream>
#include <regex>
#include <unordered_map>
#include <algorithm>
#include <cctype>
namespace solver {
static std::string lower(std::string s){for(char& c:s)c=(char)std::tolower((unsigned char)c);return s;}
static std::string clean(std::string s){s.erase(std::remove(s.begin(),s.end(),'\r'),s.end());return s;}
static bool parse_expr(const std::string& s,std::unordered_map<std::string,Real>& coeff,Real& constant){
 std::string t; for(char ch:s) if(!std::isspace((unsigned char)ch) && ch!='*') t+=ch;
 if(t.empty()) return true;
 size_t i=0;
 while(i<t.size()){
   int sign=1; if(t[i]=='+'){++i;} else if(t[i]=='-'){sign=-1;++i;}
   size_t num=i; bool has_num=false; while(i<t.size()&&(std::isdigit((unsigned char)t[i])||t[i]=='.'||t[i]=='e'||t[i]=='E'||((t[i]=='+'||t[i]=='-')&&i>num&&(t[i-1]=='e'||t[i-1]=='E')))){++i;has_num=true;}
   Real magnitude=1; if(has_num){try{magnitude=std::stod(t.substr(num,i-num));}catch(...){return false;}}
   if(i<t.size()&&(std::isalpha((unsigned char)t[i])||t[i]=='_')){size_t st=i++;while(i<t.size()&&(std::isalnum((unsigned char)t[i])||t[i]=='_'||t[i]=='.'))++i;coeff[t.substr(st,i-st)]+=sign*magnitude;}
   else if(has_num){constant+=sign*magnitude;}
   else return false;
 }
 return true;
}
bool read_lp(const std::string& path,LinearModel& m,std::string& e){std::ifstream in(path);if(!in){e="cannot open LP file";return false;}enum Sec{NONE,OBJ,CONS,BOUNDS};Sec sec=NONE;std::string line;std::unordered_map<std::string,Real> obj;std::vector<std::pair<std::string,std::string>> rawcons;struct B{std::string var;bool lo=false,up=false;Real l=0,u=0;};std::vector<B> bounds;int ln=0;while(std::getline(in,line)){++ln;line=clean(line);auto pos=line.find('\r');if(pos!=std::string::npos)line.erase(pos);std::string l=lower(line);if(l.empty()||l[0]=='\\')continue;if(l.find("minimize")==0||l=="minimize"){m.minimize=true;sec=OBJ;continue;}if(l.find("maximize")==0||l=="maximize"){m.minimize=false;sec=OBJ;continue;}if(l.find("subject to")==0||l=="such that"||l=="st"){sec=CONS;continue;}if(l.find("bounds")==0){sec=BOUNDS;continue;}if(l=="end")break;if(sec==OBJ){auto cpos=line.find(':');if(cpos!=std::string::npos)line=line.substr(cpos+1);std::unordered_map<std::string,Real> c;Real k;if(!parse_expr(line,c,k)){e="LP objective parse error at line "+std::to_string(ln);return false;}for(auto [v,x]:c)obj[v]+=x;}
else if(sec==CONS){auto cpos=line.find(':');std::string name="c"+std::to_string(rawcons.size()+1);if(cpos!=std::string::npos){name=line.substr(0,cpos);line=line.substr(cpos+1);}rawcons.push_back({name,line});}
else if(sec==BOUNDS){
 std::smatch bm;
 std::string t=line;
 std::regex triple(R"(^\s*([+-]?(?:\d*\.?\d+))\s*<=\s*([A-Za-z_][A-Za-z0-9_.]*)\s*<=\s*([+-]?(?:\d*\.?\d+))\s*$)");
 std::regex low(R"(^\s*([A-Za-z_][A-Za-z0-9_.]*)\s*>=\s*([+-]?(?:\d*\.?\d+))\s*$)");
 std::regex up(R"(^\s*([A-Za-z_][A-Za-z0-9_.]*)\s*<=\s*([+-]?(?:\d*\.?\d+))\s*$)");
 std::regex freev(R"(^\s*([A-Za-z_][A-Za-z0-9_.]*)\s+free\s*$)");
 if(std::regex_match(t,bm,triple)){bounds.push_back({bm[2],true,true,std::stod(bm[1]),std::stod(bm[3])});}
 else if(std::regex_match(t,bm,low)){bounds.push_back({bm[1],true,false,std::stod(bm[2]),0});}
 else if(std::regex_match(t,bm,up)){bounds.push_back({bm[1],false,true,0,std::stod(bm[2])});}
 else if(std::regex_match(t,bm,freev)){bounds.push_back({bm[1],true,true,-kInfinity,kInfinity});}
 else {e="unsupported Bounds syntax at line "+std::to_string(ln);return false;}
}
}

std::unordered_map<std::string,int> vi;auto addvar=[&](const std::string& v){if(!vi.count(v)){vi[v]=(int)m.variables.size();m.variables.push_back({v,0,kInfinity,0,false});}};for(auto& [v,x]:obj){addvar(v);m.variables[vi[v]].objective=x;}std::vector<std::unordered_map<int,Real>> rows;for(auto& rc:rawcons){std::string expr=rc.second;std::string op;size_t p=expr.find("<=");if(p==std::string::npos)p=expr.find(">=");if(p==std::string::npos)p=expr.find('=');if(p==std::string::npos){e="constraint missing relation at line "+std::to_string(ln);return false;}if(expr[p]=='<')op="<=";else if(expr[p]=='>')op=">=";else op="=";size_t opstart=p;std::string left=expr.substr(0,opstart), right=expr.substr(p+(op.size()==2?2:1));std::unordered_map<std::string,Real> c;Real k;if(!parse_expr(left,c,k)){e="constraint parse error";return false;}Real rhs;try{rhs=std::stod(right);}catch(...){e="constraint RHS parse error";return false;}std::unordered_map<int,Real> r;for(auto [v,x]:c){addvar(v);r[vi[v]]+=x;}rows.push_back(r);Constraint con;con.name=rc.first;if(op=="<=")con.upper_bound=rhs;else if(op==">=")con.lower_bound=rhs;else con.lower_bound=con.upper_bound=rhs;m.constraints.push_back(con);}for(auto& b:bounds){addvar(b.var);auto& v=m.variables[vi[b.var]];if(b.lo)v.lower_bound=b.l;if(b.up)v.upper_bound=b.u;}std::vector<Real> vals;std::vector<Index> ri,cp(m.variables.size()+1,0);for(size_t j=0;j<m.variables.size();++j){for(size_t i=0;i<rows.size();++i){auto it=rows[i].find((int)j);if(it!=rows[i].end()&&it->second!=0){ri.push_back((Index)i);vals.push_back(it->second);}}cp[j+1]=vals.size();}m.A=CscMatrix(m.constraints.size(),m.variables.size(),std::move(vals),std::move(ri),std::move(cp));return m.validate(e);}
}
