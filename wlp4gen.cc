#include <exception>
#include <iostream>
#include <istream>
#include <iterator>
#include <sstream>
#include <string>

#include <map>
#include <set>
#include <vector>

using namespace std;

const set<string> TERMINALS = {"BOF", "EOF", "ID", "NUM", "LPAREN", "RPAREN", "LBRACE", "RBRACE", "RETURN", "IF", "ELSE", "WHILE", "PRINTLN", "WAIN", "BECOMES", "INT", "EQ", "NE", "LT", "GT", "LE", "GE", "PLUS", "MINUS", "STAR", "SLASH", "PCT", "COMMA", "SEMI", "NEW", "DELETE", "LBRACK", "RBRACK", "AMP", "NULL"};

class Tree {
public:
    vector<string> rule; // Rule for non-terminal, token for terminal
    string lexeme = "";
    string type = "";
    vector<Tree*> children;

    Tree(string line) {
        stringstream ss(line);
        istream_iterator<string> begin(ss);
        istream_iterator<string> end;
        vector<string> trans(begin, end);
        rule = trans;

        if (TERMINALS.count(rule[0])) {
            lexeme = rule[1];
        }        
    }

    ~Tree() {
        for (int i = 0; i < children.size(); ++i) {
            delete children[i];
        }
    }

    void printTree() {
        cout << "lexeme: " << lexeme << endl;
        for (int i = 0; i < rule.size(); ++i) {
            cout << rule[i] << " ";
        }
        cout << endl;
        cout << "num children: " << children.size() << endl << endl;

        for (int i = 0; i < children.size(); ++i) {
            children[i]->printTree();
        }
        
    }

};

void addChildren(Tree* root) {
    if (!TERMINALS.count(root->rule[0])){
        for (int i = 0; i < root->rule.size()-1; ++i) {
            string line;
            getline(cin, line);

            Tree* child = new Tree(line);
            addChildren(child);

            root->children.push_back(child);
        }
    }
}

typedef map<string, string> VariableTable; // var_name -> type
typedef vector<string> Signature; // Vector of types
typedef map<string, pair<Signature, VariableTable>> ProcedureTable; // proc_name -> (sig, var_table)

string current_procedure;

class SymbolTable {
public:
    ProcedureTable pt;

    string type_dcl(Tree* tree) { // Checks what type a dcl is
        if (tree->children[0]->rule.size() == 2) // rule is "type INT"
            return "int";
        else
            return "int*";
    }

    bool accessibleProcedure(string name) {
        if (pt.count(name) == 0) return false;
        else if (pt[current_procedure].second.count(name) != 0) return false;
        else return true;
    }

    bool accessibleVariable(string name) {
        if (pt[current_procedure].second.count(name) == 0) return false;
        else return true;
    }

    void addProcedure(Tree* tree) {
        string proc_name = tree->children[1]->lexeme; // ID or WAIN
        if (pt.count(proc_name) != 0) {// Duplicate procedure
            cerr << "ERROR: procedure " + proc_name + " declared more than once";
            throw exception();
        }
        current_procedure = proc_name;

        // Get signature
        vector<string> sig;
        if (tree->rule[0] == "main") {
            sig.push_back(type_dcl(tree->children[3]));
            sig.push_back(type_dcl(tree->children[5]));
        }
        else {
            if (tree->children[3]->rule.size() == 2) { // paramlist or empty
                Tree* paramlist = tree->children[3]->children[0];
                while (paramlist->children.size() == 3) { // List continues  
                    sig.push_back(type_dcl(paramlist->children[0]));
                    paramlist = paramlist->children[2];
                }
                sig.push_back(type_dcl(paramlist->children[0]));
            }
        }

        pt.insert(pair<string, pair<Signature, VariableTable>>(proc_name, pair<Signature, VariableTable>(sig, map<string,string>())));
    }

    void addVariable(Tree* tree) {
        string var_name = tree->children[1]->lexeme; // ID

        if (accessibleVariable(var_name)) {
            cerr << "ERROR: variable " + var_name + " declared more than once";
            throw exception();
        }

        string type = type_dcl(tree);
        
        pt.find(current_procedure)->second.second.insert(pair<string,string>(var_name, type));
    }

    string lookupVariable(string name) {
        return pt[current_procedure].second[name];
    }

    vector<string> lookupProcedure(string name) {
        return pt[name].first;
    }

    void printTable() {
        for (auto const& x : pt) {
            cerr << x.first << " ";

            vector<string> signature = x.second.first;
            for (int i = 0; i < signature.size(); ++i) {
                cout << signature[i] << " ";
            }
            cout << endl;

            VariableTable iter = x.second.second;
            
            for (auto const& y : iter) {
                cerr << y.first << " " << y.second << endl;
            }
            cout << endl;
        }
    }

};

SymbolTable st;

void buildST(Tree* root) {
    if (root->rule[0] == "procedure" || root->rule[0] == "main") {
        st.addProcedure(root);
    }
    else if (root->rule[0] == "dcl") {
        st.addVariable(root);
    }
    else if (root->rule.size() == 2 && root->rule[1] == "ID" && ((root->rule[0] == "factor") || (root->rule[0] == "lvalue"))) {
        string var_name = root->children[0]->lexeme;
        if (!st.accessibleVariable(var_name))
        {
            cerr << "ERROR: variable " + var_name + " used before declaration in this scope";
            throw exception();
        }
    }
    else if (root->rule[0] == "factor" && root->rule[1] == "ID" && root->rule[2] == "LPAREN") {
        string proc_name = root->children[0]->lexeme;
        if (!st.accessibleProcedure(proc_name)) {
            cerr << "ERROR: procedure " + proc_name + " used in context where it is unavailable";
            throw exception();
        }
    }
    for (int i = 0; i < root->children.size(); ++i) {
        buildST(root->children[i]);
    }
}

string getType(Tree* tree) {
    if (tree->type != "") return tree->type;
    string type = "none";

    if (tree->rule[0] == "ID") {
        type = st.lookupVariable(tree->lexeme);
    }
    else if (tree->rule[0] == "NUM") {
        type = "int";
    }
    else if (tree->rule[0] == "NULL") {
        type = "int*";
    }
    else if (tree->rule[0] == "dcl") {
        type = getType(tree->children[1]);
    }
    else if (tree->rule[0] == "factor") { 
        if (tree->rule.size() == 2) { // Factor derives ID, NUM, or NULL
            type = getType(tree->children[0]);
        }
        else if (tree->rule[1] == "LPAREN") { // Factor derives LPAREN expr RPAREN
            type = getType(tree->children[1]);
        }
        else if (tree->rule[1] == "AMP") { // Factor derives AMP lvalue
            if (getType(tree->children[1]) != "int") {
                cerr << "ERROR: operator & expects int operand";
                throw exception();
            }
            type = "int*";
        }
        else if (tree->rule[1] == "STAR") { // Factor derives STAR factor
            if (getType(tree->children[1]) != "int*") {
                cerr << "ERROR: operator * expects int* operand";
                throw exception();
            }
            type = "int";
        }
        else if (tree->rule[1] == "NEW") { // NEW INT LBRACK expr RBRACK
            if (getType(tree->children[3]) != "int") {
                cerr << "ERROR: operator new int[] expects int operand";
                throw exception();
            }
            type = "int";
        }
        else if (tree->rule[1] == "ID") { // Factor derives ID LPAREN RPAREN or ID LPAREN arglist RPAREN
            // Type of the function
        }
    }
    else if (tree->rule[0] == "lvalue") {
        if (tree->rule[1] == "ID") { // lvalue ID
            return getType(tree->children[0]);
        }
        else if (tree->rule[1] == "STAR") { // lvalue STAR factor
            if (getType(tree->children[1]) != "int*") {
                cerr << "ERROR: operator * expects int* operand";
                throw exception();
            }
            return "int";
        }
        else if (tree->rule[1] == "LPAREN") { // lvalue LPAREN lvalue RPAREN
            return getType(tree->children[1]);
        }
    }
    else if (tree->rule[0] == "term") {
        if (tree->rule[1] == "factor") { // term factor
            return getType(tree->children[0]);
        }
        else { // term term STAR/SLASH/PCT factor
            if (getType(tree->children[0]) != "int" || getType(tree->children[2]) != "int") {
                cerr << "ERROR: operator * or / or % requires two int operands";
                throw exception();
            }

            return "int";
        }
    }
    else if (tree->rule[0] == "expr") {
        if (tree->rule[1] == "term") { // expr term
            return getType(tree->children[0]);
        }
        else if (tree->rule[2] == "PLUS") { // expr expr PLUS term
            if (getType(tree->children[0]) == "int" && getType(tree->children[2]) == "int") {
                return "int";
            }
            else if (getType(tree->children[0]) != getType(tree->children[2])) {
                return "int*";
            }
            else {
                cerr << "ERROR: invalid type operands to operator +";
                throw exception();
            }
        }
        else if (tree->rule[2] == "MINUS") { // expr expr MINUS term
            if (getType(tree->children[0]) == "int" && getType(tree->children[2]) == "int") {
                return "int";
            }
            else if (getType(tree->children[0]) == "int*" && getType(tree->children[2]) == "int*") {
                return "int";
            }
            else if (getType(tree->children[0]) == "int*" && getType(tree->children[2]) == "int") {
                return "int*";
            }
            else {
                cerr << "ERROR: invalid type operands to operator -";
                throw exception();
            }
        }
    }

    tree->type = type;

    //cout << tree->rule[0] << " " << type << endl;

    return type;
}

void typeCheck(Tree* root) {
    if (root->rule[0] == "procedure") { // get the expr
        if (getType(root->children[9]) != "int") {
            cerr << "ERROR: procedure return expression must be of type int";
            throw exception();
        }
    }
    else if (root->rule[0] == "main") {
        if (getType(root->children[5]) != "int") { // Get the second dcl
            cerr << "ERROR: second parameter of main must be int";
            throw exception();
        }
        else if (getType(root->children[11]) != "int") {
            cerr << "ERROR: wain return expression must be of type int";
            throw exception();
        }
    }
    else if (root->rule[0] == "expr" || root->rule[0] == "lvalue") {
        getType(root);
    }
    else if (root->rule[0] == "statement") {
        if (root->rule[1] == "lvalue") { // statement lvalue BECOMES expr SEMI
            if (getType(root->children[0]) != getType(root->children[2])) {
                cerr << "ERROR: mismatched types in becomes statement";
                throw exception();
            }
        }
        else if (root->rule[1] == "PRINTLN") { // statement PRINTLN LPAREN expr RPAREN SEMI
            if (getType(root->children[2]) != "int") {
                cerr << "ERROR: println argument must be int";
                throw exception();
            }
        }
        else if (root->rule[1] == "DELETE") { // statement DELETE LBRACK RBRACK expr SEMI
            if (getType(root->children[3]) != "int*") {
                cerr << "ERROR: println argument must be int*";
                throw exception();
            }
        }
    }
    else if (root->rule[0] == "test") { // test expr COMPARISON expr
        if (getType(root->children[0]) != getType(root->children[2])) {
                cerr << "ERROR: mismatched types in conditional test";
                throw exception();
            }
    }
    else if (root->rule[0] == "dcls" && root->rule.size() > 1 && root->rule[2] == "dcl") { // dcls dcls dcl BECOMES NUM/NULL SEMI
        if (root->rule[4] == "NUM" && getType(root->children[1]) != "int") {
            cerr << "ERROR: try to declare non-int type as NUM";
            throw exception();
        }
        else if (root->rule[4] == "NULL" && getType(root->children[1]) != "int*") {
            cerr << "ERROR: try to declare int type as NULL";
            throw exception();
        }
    }
    else if (root->rule[0] == "factor" && root->rule[1] == "ID" && root->rule.size() > 2 && root->rule[2] == "LPAREN") { // fn call has no params
        string proc_name = root->children[0]->lexeme;
        // Check parameters
        if (root->rule[3] == "RPAREN") { // Called with no params, signature must be empty
            if (!st.lookupProcedure(proc_name).empty()) {
                cerr << "ERROR: procedure " + proc_name + " needs a signature";
                throw exception();
            } 
        }
        else { // Called with arglist
            Tree* arglist = root->children[2];
            vector<string> given_args;
            while (arglist->children.size() > 1) {
                given_args.push_back(getType(arglist->children[0]));
            }
            given_args.push_back(getType(arglist->children[0]));

            vector<string> sig_args = st.lookupProcedure(root->children[0]->lexeme);
            if (given_args != sig_args) { // Given arguments dont match signature
                cerr << "ERROR: procedure " + proc_name + " given arguments don't match signature";
                throw exception();
            }
        }

    }

    for (int i = 0; i < root->children.size(); ++i) {
        cout << root->rule[0] << " " << endl;
        typeCheck(root->children[i]);
    }
}

int main() {

    string line;
    getline(cin, line);

    Tree* root = new Tree(line);
    addChildren(root);

    //root->printTree();
    try {
        buildST(root);
        typeCheck(root);
    } catch (exception &e) {
        delete root;
        return 1;
    }
    
    //st.printTable();

    delete root;

    return 0;
}