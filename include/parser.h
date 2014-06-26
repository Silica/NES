#ifndef NES_PARSER_H
#define NES_PARSER_H

#include <string>
#include <algorithm>
#include <cstdio>

#include "ast.h"

namespace NES{

class Tokenizer
{
public:
	typedef std::string string;
	Tokenizer(const string &s) : str(s)
	{
		line = 1;
	}
	bool isEnd()
	{
		skipSpace();
		return str.length() == 0;
	}
	void skipToLineEnd()
	{
		string::size_type pos = str.find_first_of("\n");
		if (pos == string::npos)
			str.resize(0);
		else
		{
			substr(pos);
			if (str.length())
			{
				line++;
				substr(1);
			}
		}
	}
	bool Keyword(const string &s)
	{
		skipSpace();
		bool r = std::equal(s.begin(), s.end(), str.begin());
		if (r)
		{
			char c = str[s.length()];
			if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_') || (c >= '0' && c <= '9'))
				return false;
			str = str.substr(s.length());
		}
		return r;
	}
	bool Operator(const string &s, const string &e = "")
	{
		skipSpace();
		return check(s, e);
	}
	bool isFloat()
	{
		skipSpace();
		if (isEnd())
			return false;
		char *e;
		double d = strtod(str.c_str(), &e);
		if (str.c_str() == e)
			return false;
		for (const char *c = str.c_str(); c < e; ++c)
		{
			if (*c < '0' || *c > '9')
				return true;
		}
		return false;
	}
	float getFloat()
	{
		skipSpace();
		char *e;
		double d = strtod(str.c_str(), &e);
		str = str.substr(e - str.c_str());
		return d;
	}
	bool isInt()
	{
		skipSpace();
		if (isEnd())
			return false;
		char c = str[0];
		return ((c >= '0' && c <= '9'));
	}
	int getInt()
	{
		skipSpace();
		int n = 0;
		int pos = 0;
		char c = str[pos];
		while ((c >= '0' && c <= '9'))
		{
			n = (n * 10) + (c - '0');
			c = str[++pos];
		}
		str = str.substr(pos);
		return n;
	}
	bool isChar()
	{
		skipSpace();
		if (str.length() > 3 && str[0] == '\'' && str[1] == '\\' && str[3] == '\'')
			return true;
		if (str.length() > 2 && str[0] == '\'' && str[2] == '\'')
			return true;
		return false;
	}
	char getChar()
	{
		skipSpace();
		char c;
		if (str.length() > 3 && str[0] == '\'' && str[1] == '\\' && str[3] == '\'')
		{
			c = str[2];
			if (c == '0')
				c = 0;
			else if (c == 'n')
				c = '\n';
			str = str.substr(4);
		}
		else if (str.length() > 2 && str[0] == '\'' && str[2] == '\'')
		{
			c = str[1];
			str = str.substr(3);
		}
		return c;
	}
	bool isString()
	{
		skipSpace();
		if (isEnd())
			return false;
		return str[0] == '"';
	}
	string getString()
	{
		skipSpace();
		if (str[0] != '"')
		{
			return string();
		}
		string::size_type pos = str.find_first_of('"', 1);
		string s = str.substr(1, pos-1);
		str = str.substr(pos+1);
		return s;
	}
	bool isIdentifier()
	{
		skipSpace();
		if (isEnd())
			return false;
		char c = str[0];
		return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_'));
	}
	string getIdentifier()
	{
		skipSpace();
		char c = str[0];
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_')))
		{
			return string();
		}
		string::size_type pos = str.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789");
		string name;
		if (pos == string::npos)
		{
			name = str;
			str.resize(0);
		}
		else
		{
			name = str.substr(0, pos);
			str = str.substr(pos);
		}
		return name;
	}
	int getLine(){return line;}
private:
	string str;
	int line;
	bool check(const string &s, const string &e = "")
	{
		bool r = std::equal(s.begin(), s.end(), str.begin());
		if (r)
		{
			if (e.find_first_of(str[s.length()]) != string::npos)
			{
				return false;
			}
			str = str.substr(s.length());
		}
		return r;
	}
	void substr(string::size_type p)
	{
		for (string::size_type i = 0; i < p; ++i)
		{
			if (str[i] == '\n')
				line++;
		}
		str = str.substr(p);
	}
	void skipSpace()
	{
		string::size_type p = 0;
		while ((p = str.find_first_not_of(" \t\r\n")) != string::npos)
		{
			substr(p);
			if (check("//"))
			{
				p = str.find_first_of("\n");
				if (p == string::npos)
					break;
				p = p + 1;
				substr(p);
				continue;
			}
			else if (check("/*"))
			{
				while ((p = str.find_first_of("*")) != string::npos)
				{
					if (str[p+1] == '/')
					{
						p = p + 2;
						substr(p);
						break;
					}
				}
				if (p == string::npos)
				{
					std::printf("token error: comment is not closed\n");
				}
			}
			else
			{
				break;
			}
		}
		if (p == string::npos)
			substr(str.length());
	}
};
class Parser
{
public:
	Parser(Tokenizer *t_)
	{
		t = t_;
		errors = 0;
	}
	shptr<AST::NameSpace> Parse()
	{
		shptr<AST::NameSpace> ns = new AST::NameSpace();
		while (!t->isEnd())
		{
			ParseGlobal(ns);
		}
		if (errors)
		{
			std::printf("parser: %d errors occurred\n", errors);
			return NULL;
		}
		return ns;
	}
private:
	typedef std::string string;
	typedef AST::Exp Exp;
	typedef AST::State State;
	typedef AST::Var Var;
	typedef AST::Args Args;
	typedef shptr<AST::Function> Function;
	Tokenizer *t;
	int errors;
	void err(const string &s)
	{
		std::printf("parser %d: %s\n", t->getLine(), s.c_str());
		errors++;
	}
	AST::Type ParseTypeName()
	{
		if (!t->isIdentifier())
		{
			err("no type name");
			t->skipToLineEnd();
			return NULL;
		}
		string name = t->getIdentifier();
		if (t->Operator("."))
			return new AST::NameSpaceQualifier(name, ParseTypeName());
		else
			return new AST::TypeName(name);
	}
	AST::Type ParseType()
	{
		if (t->Operator("("/*)*/))
		{
			// func ptr
			shptr<std::vector<AST::Type> > arg = new std::vector<AST::Type>();

			if (!t->Operator(/*(*/")"))
			{
				do
				{
					AST::Type type = ParseType();
					arg->push_back(type);
				} while (t->Operator(","));
				if (!t->Operator(/*(*/")"))
				{
					err("func-ptr(arg) is not closed");
					return NULL;
				}
			}
			else
			{
				// no argument
			}
			if (!t->Operator(":"))
			{
				err("func-ptr need ':' before return type");
				return NULL;
			}
			AST::Type ret = ParseType();
			return new AST::FuncPtr(ret, arg);
		}
		AST::Type type = ParseTypeName();
		while (true)
		{
			if (t->Operator("["))
			{
				t->isInt();
				int n = t->getInt();
				if (!t->Operator("]"))
				{
					err("array[] is not closed");
					t->skipToLineEnd();
					return NULL;
				}
				type = new AST::Array(type, n);
			}
			else if (t->Operator("*"))
			{
				type = new AST::Pointer(type);
			}
			else
			{
				break;
			}
		}
		return type;
	}
	template<class T/* = AST::VarInfo*/>Var ParseDecVar()
	{
		if (!t->isIdentifier())
		{
			err("need identifier for variable name");
			t->skipToLineEnd();
			return NULL;
		}
		string name = t->getIdentifier();
		AST::Type type;
		if (t->Operator(":"))
		{
			type = ParseType();
		}
		Exp e;
		if (t->Operator("="))
		{
			e = ParseExpression();
		}
		return new T(name, type, e);
	}
	State ParseBlock()
	{
		AST::Statements *stm = new AST::Statements();
		State s = stm;
		while (!t->Operator(/*{*/"}"))
		{
			if (t->isEnd())
			{
				err("premature end");
				break;
			}
			stm->Add(ParseStatement());
		}
		return s;
	}
	Args ParseArgumentsList()
	{
		Args args = new std::vector<Var>;
		if (t->Keyword("void"))
		{
			return args;
		}
		if (!t->isIdentifier())
		{
			// 引数無し
			return args;
		}
		do
		{
			if (t->isIdentifier())
			{
				Var v = ParseDecVar<AST::Argument>();
				args->push_back(v);
			}
			else
			{
				err("no identifier");
				break;
			}
		} while (t->Operator(","));
		return args;
	}
	void ParseGlobal(AST::NameSpace &ns)
	{
		if (t->Keyword("def"))
		{
			if (!t->isIdentifier())
			{
				err("need identifier after 'def' keyword");
				return;
			}
			string name = t->getIdentifier();

			Args args;
			if (t->Operator("("/*)*/))
			{
				args = ParseArgumentsList();
				if (!t->Operator(/*(*/")"))
				{
					err("(arguments) is not closed");
					t->skipToLineEnd();
					return;
				}
			}
			else
			{
				// 引数省略、引数なし
			}
			AST::Type type;
			if (t->Operator(":"))
			{
				type = ParseType();
			}
			else
			{
				// 戻り値省略、なし
				// 戻り値型を省略する(auto)場合もあり得るが
				// それは今後の課題としてまずは省略時はvoidであるとして扱う
			}

			State s;
			if (t->Operator("{"/*}*/))
			{
				s = ParseBlock();
			}
			else
			{
				s = ParseStatement();
			}
			AST::element f = new AST::Function(name, args, type, s);
			if (ns.Add(f))
			{
				err(name + " is already exists");
			}
		}
		else if (t->Keyword("namespace"))
		{
			if (!t->isIdentifier())
			{
				err("need identifier after 'namespace' keyword");
				return;
			}
			string name = t->getIdentifier();
			shptr<AST::NameSpace> nns = new AST::NameSpace(name);

			if (!t->Operator("{"/*}*/))
			{
				err("namespace need {"/*}*/);
			}

			while (!t->Operator(/*{*/"}"))
			{
				if (t->isEnd())
				{
					err(name + " namespace: premature end");
					return;
				}
				ParseGlobal(nns);
			}

			ns.Add(nns);
		}
		else if (t->Keyword("typedef"))
		{
			// typedef type identifier;
			// typedef identifier : type; こっちだな
		}
		else if (t->Keyword("struct"))
		{
			if (!t->isIdentifier())
			{
				err("need identifier after 'struct' keyword");
				t->skipToLineEnd();
				return;
			}
			string name = t->getIdentifier();
			AST::Struct *s = new AST::Struct(name);
			AST::element e = s;
			if (!t->Operator("{"/*}*/))
			{
				err("struct need '{'"/*'}'*/);
				t->skipToLineEnd();
				return;
			}
			while (!t->Operator(/*{*/"}"))
			{
				if (t->isEnd())
				{
					err("premature end");
					return;
				}
				Var v = ParseDecVar<AST::VarInfo>();
				s->add(v);
				if (!t->Operator(";"))
				{
					err("no ';' at end of declare member");
				}
			}
			ns.Add(e);
		}
		else if (t->Keyword("union"))
		{
			if (!t->isIdentifier())
			{
				err("need identifier after 'union' keyword");
				t->skipToLineEnd();
				return;
			}
			string name = t->getIdentifier();
			AST::Union *s = new AST::Union(name);
			AST::element e = s;
			if (!t->Operator("{"/*}*/))
			{
				err("union need '{'"/*'}'*/);
				t->skipToLineEnd();
				return;
			}
			while (!t->Operator(/*{*/"}"))
			{
				if (t->isEnd())
				{
					err("premature end");
					return;
				}
				Var v = ParseDecVar<AST::VarInfo>();
				s->add(v);
				if (!t->Operator(";"))
				{
					err("no ';' at end of declare member");
				}
			}
			ns.Add(e);
		}
		else if (t->Keyword("enum"))
		{
			if (!t->isIdentifier())
			{
				err("need identifier after 'enum' keyword");
			}
			string name = t->getIdentifier();
			AST::Enum *s = new AST::Enum(name);
			AST::element e = s;
			if (!t->Operator("{"/*}*/))
			{
				err("enum need '{'"/*'}'*/);
				t->skipToLineEnd();
				return;
			}
			while (!t->Operator(/*{*/"}"))
			{
				if (!t->isIdentifier())
				{
					err("no identifier in enum member");
					return;
				}
				string var_name = t->getIdentifier();
				if (t->Operator("="))
				{
					if (!t->isInt())
					{
						err("enum parameter is only int");
					}
					s->add(var_name, t->getInt());
					// ParseExpression();
				}
				else
				{
					s->add(var_name);
				}
				if (t->Operator(";"));
				else if (t->Operator(","));
				else if (t->Operator(/*{*/"}"))
				{
					break;
				}
				else
				{
					err("no separator in enum");
					return;
				}
			}
			ns.Add(e);
		}
		else if (t->Keyword("var"))
		{
			Var v = ParseDecVar<AST::GlobalVar>();
			if (!t->Operator(";"))
				err("no ';' at end of global declaration");
			if (ns.Add(v))
			{
				err(v->getName() + " is already exists");
			}
		}
		else
		{
			err("unknown global element");
			t->skipToLineEnd();
		}
	}
	State ParseIf()
	{
		if (!t->Operator("("/*)*/))
		{
			err("if '(' cond ')' statement [else statement]");
			t->skipToLineEnd();
			return NULL;
		}
		Exp cond = ParseExpression();
		if (!t->Operator(/*(*/")"))
		{
			err("if '(' cond ')' statement [else statement]");
			t->skipToLineEnd();
			return NULL;
		}
		State if_s = ParseStatement();
		State else_s;
		if (t->Keyword("else"))
		{
			else_s = ParseStatement();
		}
		return new AST::If(cond, if_s, else_s);
	}
	State ParseWhile()
	{
		if (!t->Operator("("/*)*/))
		{
			err("while '(' cond ')' statement [else statement]");
		}
		Exp cond = ParseExpression();
		if (!t->Operator(/*(*/")"))
		{
			err("if '(' cond ')' statement [else statement]");
		}
		State s = ParseStatement();
		State else_s;
		if (t->Keyword("else"))
		{
			else_s = ParseStatement();
		}
		return new AST::While(cond, s, else_s);
	}
	State ParseStatement()
	{
		State s;
		if (t->Operator(";"))
		{
			// empty
		}
		else if (t->Operator("{"/*}*/))
		{
			s = ParseBlock();
		}
		else if (t->Keyword("var"))
		{
			Var v = ParseDecVar<AST::LocalVar>();
			s = new AST::Declaration(v);
			if (!t->Operator(";"))
				err("no ';' at end of local declaration");
		}
		else if (t->Keyword("typedef"))
		{
			// local typedefは可能？
		}
		else if (t->Keyword("if"))
		{
			s = ParseIf();
		}
		else if (t->Keyword("while"))
		{
			s = ParseWhile();
		}
		else if (t->Keyword("return"))
		{
			if (t->Operator(";"))
			{
				s = new AST::Return();
			}
			else
			{
				Exp e = ParseExpression();
				if (!t->Operator(";"))
				{
					err("no ';' at end of return");
				}
				s = new AST::Return(e);
			}
		}
		else if (t->Keyword("continue"))
		{
			if (!t->Operator(";"))
			{
				err("continue need ;");
			}
			s = new AST::Continue();
		}
		else if (t->Keyword("break"))
		{
			if (!t->Operator(";"))
			{
				err("break need ;");
			}
			s = new AST::Break();
		}
		else
		{
			Exp e = ParseExpression();
			if (!t->Operator(";"))
			{
				err("no ';' at end of expression");
				t->skipToLineEnd();
			}
			s = new AST::ExpressionStatement(e);
		}
		return s;
	}
	void ParseArguments(AST::Call *c)
	{
		do
		{
			c->Add(ParseExpression());
		} while (t->Operator(","));
	}
	Exp getTerm()
	{
		Exp l;
		if (t->Operator("("/*)*/))
		{
			l = ParseExpression();
			if (t->Operator(/*(*/")"))
				;
			else
				err(/*(*/"need )");
		}
		#define PRE_OP(o,a) else if (t->Operator(o)) {l = new AST::a(getTerm());l = getTerm();}
		PRE_OP("++", PreInc)
		PRE_OP("--", PreDec)
		PRE_OP("+",  Plus)
		PRE_OP("-",  Minus)
		PRE_OP("!",  Not)
		PRE_OP("~",  Compl)
		PRE_OP("*",  Deref)
		PRE_OP("&",  Ref)
		#undef PRE_OP
		else if (t->isIdentifier())
		{
			string name = t->getIdentifier();
			l = new AST::Variable(name);
		}
		else if (t->isFloat())
		{
			float f = t->getFloat();
			l = new AST::Float(f);
		}
		else if (t->isInt())
		{
			int n = t->getInt();
			l = new AST::Int(n);
		}
		else if (t->isChar())
		{
			char c = t->getChar();
			l = new AST::Char(c);
		}
		else if (t->isString())
		{
			string s = t->getString();
			l = new  AST::String(s);
		}
		else
		{
			err("unknown term");
			t->skipToLineEnd();
			return NULL;
		}

		while (!t->isEnd())
		{
			if (t->Operator("++"))
			{
				l = new AST::Inc(l);
			}
			else if (t->Operator("--"))
			{
				l = new AST::Dec(l);
			}
			else if (t->Operator("("/*)*/))
			{
				AST::Call *c = new AST::Call(l);
				l = c;
				if (t->Operator(/*(*/")"))
				{
					// no argument
				}
				else
				{
					ParseArguments(c);
					if (!t->Operator(/*(*/")"))
					{
						err("call() is not closed");
						return NULL;
					}
				}
			}
			else if (t->Operator("["))
			{
				Exp e = ParseExpression();
				if (!t->Operator("]"))
				{
					err("exp[] is not closed");
					return NULL;
				}
				l = new AST::Index(l, e);
			}
			else if (t->Operator("."))
			{
				if (!t->isIdentifier())
				{
					err("no member name after '.'");
					return NULL;
				}
				string name = t->getIdentifier();
				l = new AST::Member(l, name);
			}
			else
				break;
		}
		return l;
	}
	Exp getexp1()
	{
		Exp l = getTerm();
		while (!t->isEnd())
		{
			if (t->Operator("*", "="))
			{
				Exp r = getTerm();
				l = new AST::Mul(l, r);
			}
			else if (t->Operator("/", "="))
			{
				Exp r = getTerm();
				l = new AST::Div(l, r);
			}
			else if (t->Operator("%", "="))
			{
				Exp r = getTerm();
				l = new AST::Mod(l, r);
			}
			else
				break;
		}
		return l;
	}
	Exp getexp2()
	{
		Exp l = getexp1();
		while (!t->isEnd())
		{
			if (t->Operator("+", "="))
			{
				Exp r = getexp1();
				l = new AST::Add(l, r);
			}
			else if (t->Operator("-", "="))
			{
				Exp r = getexp1();
				l = new AST::Sub(l, r);
			}
			else
				break;
		}
		return l;
	}
	Exp getexp3()
	{
		Exp l = getexp2();
		while (!t->isEnd())
		{
			if (t->Operator("<<", "="))
			{
				Exp r = getexp2();
				l = new AST::SHL(l, r);
			}
			else if (t->Operator(">>", "="))
			{
				Exp r = getexp2();
				l = new AST::SHR(l, r);
			}
			else
				break;
		}
		return l;
	}
	Exp getexp4()
	{
		Exp l = getexp3();
		while (!t->isEnd())
		{
			if (t->Operator("&", "&="))
			{
				Exp r = getexp3();
				l = new AST::AND(l, r);
			}
			else
				break;
		}
		return l;
	}
	Exp getexp5()
	{
		Exp l = getexp4();
		while (!t->isEnd())
		{
			if (t->Operator("^", "="))
			{
				Exp r = getexp4();
				l = new AST::XOR(l, r);
			}
			else
				break;
		}
		return l;
	}
	Exp getexp6()
	{
		Exp l = getexp5();
		while (!t->isEnd())
		{
			if (t->Operator("|", "|="))
			{
				Exp r = getexp5();
				l = new AST::OR(l, r);
			}
			else
				break;
		}
		return l;
	}
	Exp getexp7()
	{
		Exp l = getexp6();
		while (!t->isEnd())
		{
			if (t->Operator("<="))
			{
				Exp r = getexp6();
				l = new AST::LE(l, r);
			}
			else if (t->Operator("<"))
			{
				Exp r = getexp6();
				l = new AST::LT(l, r);
			}
			else if (t->Operator(">="))
			{
				Exp r = getexp6();
				l = new AST::GE(l, r);
			}
			else if (t->Operator(">"))
			{
				Exp r = getexp6();
				l = new AST::GT(l, r);
			}
			else
				break;
		}
		return l;
	}
	Exp getexp8()
	{
		Exp l = getexp7();
		while (!t->isEnd())
		{
			if (t->Operator("=="))
			{
				Exp r = getexp7();
				l = new AST::EQ(l, r);
			}
			else if (t->Operator("!="))
			{
				Exp r = getexp7();
				l = new AST::NE(l, r);
			}
			else
				break;
		}
		return l;
	}
	Exp getexp9()
	{
		Exp l = getexp8();
		while (!t->isEnd())
		{
			if (t->Operator("&&"))
			{
				Exp r = getexp8();
				l = new AST::BOOL_AND(l, r);
			}
			else
				break;
		}
		return l;
	}
	Exp getexp10()
	{
		Exp l = getexp9();
		while (!t->isEnd())
		{
			if (t->Operator("||"))
			{
				Exp r = getexp9();
				l = new AST::BOOL_OR(l, r);
			}
			else
				break;
		}
		return l;
	}
	Exp getexp11()
	{
		Exp l = getexp10();
		while (!t->isEnd())
		{
			if (t->Operator("?"))
			{
				Exp r1 = getexp11();
				if (!t->Operator(":"))
				{
					err("need :");
				}
				Exp r2 = getexp11();
				l = new AST::TernaryOperater(l, r1, r2);
			}
			else
				break;
		}
		return l;
	}
	Exp getexp12()
	{
		Exp l = getexp11();
		while (!t->isEnd())
		{
			if (t->Operator("="))
			{
				Exp r = getexp12();
				l = new AST::Assign(l, r);
			}
			else if (t->Operator("+="))
			{
				Exp r = getexp12();
				l = new AST::AddAssign(l, r);
			}
			else if (t->Operator("-="))
			{
				Exp r = getexp12();
				l = new AST::SubAssign(l, r);
			}
			else if (t->Operator("*="))
			{
				Exp r = getexp12();
				l = new AST::MulAssign(l, r);
			}
			else if (t->Operator("/="))
			{
				Exp r = getexp12();
				l = new AST::DivAssign(l, r);
			}
			else if (t->Operator("%="))
			{
				Exp r = getexp12();
				l = new AST::ModAssign(l, r);
			}
			else if (t->Operator("<<="))
			{
				Exp r = getexp12();
				l = new AST::SHLAssign(l, r);
			}
			else if (t->Operator(">>="))
			{
				Exp r = getexp12();
				l = new AST::SHRAssign(l, r);
			}
			else if (t->Operator("&="))
			{
				Exp r = getexp12();
				l = new AST::ANDAssign(l, r);
			}
			else if (t->Operator("^="))
			{
				Exp r = getexp12();
				l = new AST::XORAssign(l, r);
			}
			else if (t->Operator("|="))
			{
				Exp r = getexp12();
				l = new AST::ORAssign(l, r);
			}
			else
				break;
		}
		return l;
	}
	Exp ParseExpression()
	{
		Exp e = getexp12();
		return e;
	}
};

}
#endif
