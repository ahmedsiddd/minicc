// minicc: tiny compiler. lexer -> recursive-descent parser -> AST -> const-fold/branch-elim
//         -> three-address-code IR -> dead-code elimination -> interpreter | IR dump | PTX-style asm
#include <bits/stdc++.h>
using namespace std;
using L = long long;
struct N { string k; L v = 0; string s; vector<N*> c; };           // AST node
struct I { string op, d, a, b; };                                    // TAC: op d a b
vector<string> T; size_t p = 0; vector<I> ir; int nt = 0, nl = 0; set<string> def;

// ---------- lexer ----------
void lex(const string& s) {
  for (size_t i = 0, j; i < s.size();) {
    char c = s[i];
    if (isspace(c)) i++;
    else if (c == '#') while (i < s.size() && s[i] != '\n') i++;
    else if (isdigit(c)) { for (j = i; j < s.size() && isdigit(s[j]);) j++; T.push_back(s.substr(i, j - i)); i = j; }
    else if (isalpha(c) || c == '_') { for (j = i; j < s.size() && (isalnum(s[j]) || s[j] == '_');) j++; T.push_back(s.substr(i, j - i)); i = j; }
    else { string t = s.substr(i, 2); if (t == "==" || t == "!=" || t == "<=" || t == ">=") { T.push_back(t); i += 2; } else T.push_back(string(1, s[i++])); }
  }
}

// ---------- parser ----------
string pk() { return p < T.size() ? T[p] : ""; }
string nx() { if (p >= T.size()) throw runtime_error("unexpected end of input"); return T[p++]; }
void ex(const string& t) { if (pk() != t) throw runtime_error("expected '" + t + "' but got '" + pk() + "'"); p++; }
N* mk(string k, string s = "", L v = 0, vector<N*> c = {}) { return new N{k, v, s, c}; }
N* expr(); N* stmt();
N* prim() {
  string t = nx();
  if (t == "(") { N* e = expr(); ex(")"); return e; }
  if (t == "-") return mk("bin", "-", 0, {mk("num"), prim()});
  if (isdigit(t[0])) return mk("num", "", stoll(t));
  if (isalpha(t[0]) || t[0] == '_') return mk("var", t);
  throw runtime_error("unexpected '" + t + "'");
}
N* bin(int lv) {   // precedence climbing: ==,!=  <  <,>,<=,>=  <  +,-  <  *,/,%
  static const vector<vector<string>> P = {{"==", "!="}, {"<", ">", "<=", ">="}, {"+", "-"}, {"*", "/", "%"}};
  if (lv == 4) return prim();
  N* l = bin(lv + 1);
  while (find(P[lv].begin(), P[lv].end(), pk()) != P[lv].end()) { string o = nx(); N* r = bin(lv + 1); l = mk("bin", o, 0, {l, r}); }
  return l;
}
N* expr() { return bin(0); }
N* blk() { ex("{"); N* b = mk("blk"); while (pk() != "}") b->c.push_back(stmt()); ex("}"); return b; }
N* stmt() {
  string t = pk();
  if (t == "print") { p++; N* e = expr(); ex(";"); return mk("print", "", 0, {e}); }
  if (t == "if") { p++; ex("("); N* c = expr(); ex(")"); N* a = blk(); N* b = mk("blk"); if (pk() == "else") { p++; b = blk(); } return mk("if", "", 0, {c, a, b}); }
  if (t == "while") { p++; ex("("); N* c = expr(); ex(")"); N* b = blk(); return mk("while", "", 0, {c, b}); }
  if (t == "let") p++;                                              // 'let x = e;' and 'x = e;' behave the same
  string n = nx(); ex("="); N* e = expr(); ex(";"); return mk("asg", n, 0, {e});
}

// ---------- semantics shared by folder and interpreter ----------
L ev(const string& o, L a, L b) {
  using U = unsigned long long;
  if (o == "+") return U(a) + U(b);
  if (o == "-") return U(a) - U(b);
  if (o == "*") return U(a) * U(b);
  if (o == "/" || o == "%") { if (!b) throw runtime_error("division by zero"); if (a == LLONG_MIN && b == -1) return o == "/" ? a : 0; return o == "/" ? a / b : a % b; }
  if (o == "<") return a < b;
  if (o == ">") return a > b;
  if (o == "<=") return a <= b;
  if (o == ">=") return a >= b;
  if (o == "==") return a == b;
  return a != b;
}

// ---------- optimization 1 (AST): constant folding + dead-branch elimination ----------
N* fold(N* n) {
  for (auto& c : n->c) c = fold(c);
  if (n->k == "bin" && n->c[0]->k == "num" && n->c[1]->k == "num" && !((n->s == "/" || n->s == "%") && !n->c[1]->v))
    return mk("num", "", ev(n->s, n->c[0]->v, n->c[1]->v));
  if (n->k == "if" && n->c[0]->k == "num") return n->c[0]->v ? n->c[1] : n->c[2];
  if (n->k == "while" && n->c[0]->k == "num" && !n->c[0]->v) return mk("blk");
  return n;
}

// ---------- AST -> three-address code ----------
const set<string> NOUT = {"print", "label", "jmp", "bz"};          // ops with no destination register
string tmp() { return "%t" + to_string(nt++); }
string lbl() { return "L" + to_string(nl++); }
void emit(string op, string d = "", string a = "", string b = "") { ir.push_back({op, d, a, b}); }
string gen(N* n) {
  if (n->k == "num") return to_string(n->v);
  if (n->k == "var") { if (!def.count(n->s)) throw runtime_error("variable '" + n->s + "' used before assignment"); return n->s; }
  string a = gen(n->c[0]), b = gen(n->c[1]), d = tmp(); emit(n->s, d, a, b); return d;
}
void gs(N* n) {
  if (n->k == "blk") for (auto c : n->c) gs(c);
  else if (n->k == "asg") { string v = gen(n->c[0]); def.insert(n->s); emit("mov", n->s, v); }
  else if (n->k == "print") emit("print", "", gen(n->c[0]));
  else if (n->k == "if") { string c = gen(n->c[0]), e = lbl(), f = lbl(); emit("bz", "", c, e); gs(n->c[1]); emit("jmp", "", f); emit("label", "", e); gs(n->c[2]); emit("label", "", f); }
  else if (n->k == "while") { string t = lbl(), e = lbl(); emit("label", "", t); string c = gen(n->c[0]); emit("bz", "", c, e); gs(n->c[1]); emit("jmp", "", t); emit("label", "", e); }
}

// ---------- optimization 2 (IR): dead-code elimination to a fixpoint ----------
void dce() {
  for (bool ch = true; ch;) {
    ch = false; map<string, int> u;
    for (auto& x : ir) {
      if (!NOUT.count(x.op)) { if (x.op != "mov") u[x.b]++; u[x.a]++; }  // arithmetic reads a,b; mov reads a
      else if (x.op == "print" || x.op == "bz") u[x.a]++;
    }
    vector<I> o;
    for (auto& x : ir) { if (!NOUT.count(x.op) && !u[x.d]) { ch = true; continue; } o.push_back(x); }  // value never read -> drop
    ir = o;
  }
}

// ---------- backends ----------
bool imm(const string& s) { return isdigit(s[0]) || s[0] == '-'; }
void dump() {
  for (auto& x : ir) {
    if (x.op == "label") cout << x.a << ":\n";
    else if (x.op == "jmp") cout << "  jmp " << x.a << "\n";
    else if (x.op == "bz") cout << "  bz " << x.a << ", " << x.b << "\n";
    else if (x.op == "print") cout << "  print " << x.a << "\n";
    else if (x.op == "mov") cout << "  " << x.d << " = " << x.a << "\n";
    else cout << "  " << x.d << " = " << x.a << " " << x.op << " " << x.b << "\n";
  }
}
void run() {
  map<string, L> m; map<string, size_t> lb;
  for (size_t i = 0; i < ir.size(); i++) if (ir[i].op == "label") lb[ir[i].a] = i;
  auto v = [&](const string& s) -> L { return imm(s) ? stoll(s) : m[s]; };
  for (size_t i = 0; i < ir.size(); i++) {
    auto& x = ir[i];
    if (x.op == "mov") m[x.d] = v(x.a);
    else if (x.op == "print") cout << v(x.a) << "\n";
    else if (x.op == "jmp") i = lb[x.a];
    else if (x.op == "bz") { if (!v(x.a)) i = lb[x.b]; }
    else if (x.op != "label") m[x.d] = ev(x.op, v(x.a), v(x.b));
  }
}
void ptx() {   // PTX-style output: every variable/temp gets a 64-bit virtual register. Not validated with ptxas.
  static const map<string, string> A = {{"+", "add.s64"}, {"-", "sub.s64"}, {"*", "mul.lo.s64"}, {"/", "div.s64"}, {"%", "rem.s64"}};
  static const map<string, string> C = {{"<", "lt"}, {">", "gt"}, {"<=", "le"}, {">=", "ge"}, {"==", "eq"}, {"!=", "ne"}};
  map<string, int> r; ostringstream o;
  auto R = [&](const string& s) -> string { if (imm(s)) return s; if (!r.count(s)) { int k = r.size(); r[s] = k; } return "%rd" + to_string(r[s]); };
  auto RA = [&](const string& s) -> string { if (!imm(s)) return R(s); o << "    mov.s64 %rs, " << s << ";\n"; return "%rs"; };  // first operand -> register
  for (auto& x : ir) {
    if (x.op == "label") o << "$" << x.a << ":\n";
    else if (x.op == "jmp") o << "    bra $" << x.a << ";\n";
    else if (x.op == "bz") { string a = RA(x.a); o << "    setp.eq.s64 %p, " << a << ", 0;\n    @%p bra $" << x.b << ";\n"; }
    else if (x.op == "print") o << "    // print " << R(x.a) << "\n";
    else if (x.op == "mov") o << "    mov.s64 " << R(x.d) << ", " << R(x.a) << ";\n";
    else if (A.count(x.op)) { string a = RA(x.a); o << "    " << A.at(x.op) << " " << R(x.d) << ", " << a << ", " << R(x.b) << ";\n"; }
    else { string a = RA(x.a); o << "    setp." << C.at(x.op) << ".s64 %p, " << a << ", " << R(x.b) << ";\n    selp.s64 " << R(x.d) << ", 1, 0, %p;\n"; }
  }
  cout << ".version 7.0\n.target sm_70\n.address_size 64\n\n.visible .entry kernel()\n{\n    .reg .s64 %rd<" << max<size_t>(r.size(), 1) << ">;\n    .reg .s64 %rs;\n    .reg .pred %p;\n"
       << o.str() << "    ret;\n}\n";
}

int main(int ac, char** av) {
  try {
    if (ac < 3) { cerr << "usage: minicc <run|ir|ptx> file.mc [--no-opt]\n"; return 2; }
    ifstream f(av[2]); if (!f) throw runtime_error("cannot open " + string(av[2]));
    stringstream ss; ss << f.rdbuf(); lex(ss.str());
    N* root = mk("blk"); while (p < T.size()) root->c.push_back(stmt());
    bool opt = !(ac > 3 && string(av[3]) == "--no-opt");
    if (opt) root = fold(root);
    gs(root); if (opt) dce();
    string m = av[1]; if (m == "ir") dump(); else if (m == "ptx") ptx(); else run();
  } catch (exception& e) { cerr << "error: " << e.what() << "\n"; return 1; }
}
