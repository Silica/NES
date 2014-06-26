#ifndef NES_AST_H
#define NES_AST_H

#include <vector>
#include <string>
#include <map>

#include "il.h"

namespace NES{

namespace AST
{
	using std::string;
	using std::vector;
	using IL::ValueInfo;
	using IL::VType;

	class Environment;
	struct NameElement
	{
		NameElement(const string &s){name = s;generated = false;}
		virtual ~NameElement(){}
		string &getName(){return name;}
		bool isGenerated(){return generated;}
		void gen(Environment *env)
		{
			if (generated)
				return;
			generated = true;
			gene(env);
		}
		virtual void gene(Environment *env) = 0;
	protected:
		string name;
		bool generated;
	};
	typedef shptr<NameElement> element;


	class Environment
	{
	public:
		struct PrimitiveType : NameElement
		{
			PrimitiveType(const string &s, IL::ValueType::primitive p):NameElement(s){t = p;}
			void gene(Environment *env)
			{
				env->addType(name, new IL::Primitive(t));
			}
		private:
			IL::ValueType::primitive t;
		};
		struct PrimitiveFunction : NameElement
		{
			PrimitiveFunction(const string &s, VType t, int v, int a) : NameElement(s), type(t), val(v), address(a){}
			void gene(Environment *env)
			{
				env->addGlobal(name, type, val, address);
			}
		private:
			int val;
			VType type;
			int address;
		};
		struct NameSpace : NameElement
		{
			NameSpace() : NameElement("")
			{
				dic["void"]  = new PrimitiveType("void",  IL::ValueType::Void);
				dic["bool"]  = new PrimitiveType("bool",  IL::ValueType::Bool);
				dic["int"]   = new PrimitiveType("int",   IL::ValueType::Int);
				dic["uint"]  = new PrimitiveType("uint",  IL::ValueType::UInt);
				dic["char"]  = new PrimitiveType("char",  IL::ValueType::Char);
				dic["float"] = new PrimitiveType("float", IL::ValueType::Float);

				IL::FuncPtr *t = new IL::FuncPtr(new IL::Primitive(IL::ValueType::Int));
				t->add(new IL::Pointer(new IL::Primitive(IL::ValueType::Char)));
				dic["puts"] = new PrimitiveFunction("puts", t, (int)&std::puts, 8);

				t = new IL::FuncPtr(new IL::Primitive(IL::ValueType::Int));
				t->add(new IL::Primitive(IL::ValueType::Int));
				dic["putchar"] = new PrimitiveFunction("putchar", t, (int)&std::putchar, 0);

				t = new IL::FuncPtr(new IL::Primitive(IL::ValueType::Int));
				dic["getchar"] = new PrimitiveFunction("getchar", t, (int)&std::getchar, 4);
			}
			NameSpace(const string &s) : NameElement(s){}
			bool Add(element e)
			{
				string name = e->getName();
				if (dic.count(name))
					return true;

				dic[name] = e;
				return false;
			}
			void gene(Environment *env)
			{
				for (std::map<string, element>::iterator it = dic.begin(); it != dic.end(); ++it)
				{
					it->second->gen(env);
				}
			}
			void genSpecific(const string &s, Environment *env)
			{
				if (dic.count(s))
					dic[s]->gen(env);
			}
		private:
			std::map<string, element> dic;
		};
		Environment(shptr<NameSpace> n) : ns(n){errors = 0;}
		void err(const string &s)
		{
			errors++;
			std::printf("AST error: %s\n", s.c_str());
		}
		void genSpecific(const string &s)
		{
			ns->genSpecific(s, this);
		}

		ValueInfo getVariable(const string &name)
		{
			genSpecific(name);
			return ienv->getVariable(name);
		}
		int getTemp()																	{return ienv->getTemp();}
		void addType(const string &name, VType type)									{ienv->addType(name, type);}
		int addGlobal(const string &name, VType type, int val = 0, int address = -1)	{return ienv->addGlobal(name, type, val, address);}
		int addString(const string &s)													{return ienv->addString(s);}
		void initGlobal(const string &name)												{ienv->runInit(name);}
		void EnterFunction(const string &s, VType r)									{ienv->EnterFunction(s, r);}
		void LeaveFunction()															{ienv->LeaveFunction();}
		void EnterLoop(int breakLabel, int continueLabel)								{break_label.push_back(breakLabel);continue_label.push_back(continueLabel);}
		void LeaveLoop()																{break_label.pop_back();continue_label.pop_back();}
		int getBreakLabel()																{return break_label.back();}
		int getContinueLabel()															{return continue_label.back();}
		void addArg(const string &name, VType type)										{ienv->addArg(name, type);}
		void addLocal(const string &name, VType type)									{ienv->addLocal(name, type);}
		void pushcode(IL::opcode *c)													{ienv->pushcode(c);}
		void newState()																	{ienv->newState();}
		void setReturn()																{ienv->setReturn();}
		VType getReturnType()															{return ienv->getReturnType();}
		VType getType(const string &s)
		{
			genSpecific(s);
			VType t = ienv->getType(s);
			if (!t)
				err(s + " is undefined");
			return t;
		}
		int getLabel(){return ienv->getLabel();}
		void addLabel(int label){ienv->addLabel(label);}
		void EnterNameSpace(const string &s){ienv->EnterNameSpace(s);}
		void LeaveNameSpace()				{ienv->LeaveNameSpace();}
		ValueInfo getNVar(const string &s)	{return ienv->last_ns->get(ienv, s);}

		shptr<IL::Environment> gen()
		{
			ienv = new IL::Environment();
			ns->gen(this);
			if (errors)
			{
				std::printf("AST: %d errors occurred\n", errors);
				return NULL;
			}
			return ienv;
		}
	private:
		int errors;
		shptr<NameSpace> ns;
		shptr<IL::Environment> ienv;
		vector<int> break_label;
		vector<int> continue_label;
	};
	typedef Environment::NameSpace NameSpace;

	struct TypeBase
	{
		virtual ~TypeBase(){}
		virtual string getName() = 0;
		virtual VType gen(Environment *env) = 0;
	};
	typedef shptr<TypeBase> Type;

	struct Expression
	{
		virtual ~Expression(){}
		virtual ValueInfo genL(Environment *env)
		{
			env->err("not L-value");
			return ValueInfo();
		}
		virtual ValueInfo genR(Environment *env){return LtoR(env, genL(env));}
		static ValueInfo LtoR(Environment *env, ValueInfo r)
		{
			ValueInfo v = r;
			if (!v.type)
			{
				env->err("^^!\n");
				return v;
			}
			if (v.type->isP(IL::ValueType::Array))
			{
				ValueInfo to(env->getTemp(), new IL::Pointer(v.type->get()));
				env->pushcode(new IL::getLocalPtr(to.address, v.address));
				return to;
			}
			if (v.isStack())
			{
				return v;
			}
			else if (v.isGlobal())
			{
				ValueInfo to(env->getTemp(), v.type);
				env->pushcode(new IL::getGlobal(to.address, v.address));
				return to;
			}
			else if (v.isMemory())
			{
				ValueInfo to(env->getTemp(), v.type);
				env->pushcode(new IL::getMemory(to.address, v.address));
				return to;
			}
			else if (v.atype == ValueInfo::function)
			{
				IL::Function *f = (IL::Function*)v.address;
				ValueInfo to(env->getTemp(), f->getFuncPtr());
				env->pushcode(new IL::getFunction(to.address, f));
				return to;
			}
			else
			{
				env->err("?");
			}
			return v;
		}
	};
	typedef shptr<Expression> Exp;

	struct Term : Expression{};

	struct Variable : Term
	{
		Variable(const string &s)
		{
			name = s;
		}
		ValueInfo genL(Environment *env)
		{
			ValueInfo v = env->getVariable(name);
			return v;
		}
		ValueInfo genR(Environment *env){return LtoR(env, genL(env));}
	private:
		string name;
	};
	struct Int : Term
	{
		Int(int i){x = i;}
		ValueInfo genR(Environment *env)
		{
			ValueInfo to(env->getTemp(), new IL::Primitive(IL::ValueType::Int));
			env->pushcode(new IL::getInt(to.address, x));
			return to;
		}
	private:
		int x;
	};
	struct Float : Term
	{
		Float(float f){x = f;}
		ValueInfo genR(Environment *env)
		{
			ValueInfo to(env->getTemp(), new IL::Primitive(IL::ValueType::Float));
			env->pushcode(new IL::getFloat(to.address, x));
			return to;
		}
	private:
		float x;
	};
	struct Char : Term
	{
		Char(char c){x = c;}
		ValueInfo genR(Environment *env)
		{
			ValueInfo to(env->getTemp(), new IL::Primitive(IL::ValueType::Char));
			env->pushcode(new IL::getChar(to.address, x));
			return to;
		}
	private:
		char x;
	};
	struct String : Term
	{
		String(const string &s):x(s){}
		ValueInfo genR(Environment *env)
		{
			ValueInfo to(env->getTemp(), new IL::Pointer(new IL::Primitive(IL::ValueType::Char)));
			int s = env->addString(x);
			env->pushcode(new IL::getGlobalPtr(to.address, s));
			return to;
		}
	private:
		string x;
	};

	struct UnaryExpression : Expression
	{
		UnaryExpression(Exp x) : e(x){}
	protected:
		Exp e;
	};

	struct Inc : UnaryExpression
	{
		Inc(Exp x) : UnaryExpression(x){}
		static void INC(Environment *env, ValueInfo &li)
		{
			if (li.type->isP(IL::ValueType::Int) || li.type->isP(IL::ValueType::UInt))
			{
				if (li.isStack())
					env->pushcode(new IL::incL(li.address));
				else if (li.isGlobal())
					env->pushcode(new IL::incG(li.address));
				else if (li.isMemory())
					env->pushcode(new IL::incM(li.address));
				else
					env->err("?");
			}
			else if (li.type->isP(IL::ValueType::Char))
			{
				if (li.isStack())
					env->pushcode(new IL::cincL(li.address));
				else if (li.isGlobal())
					env->pushcode(new IL::cincG(li.address));
				else if (li.isMemory())
					env->pushcode(new IL::cincM(li.address));
				else
					env->err("?");
			}
			else if (li.type->isP(IL::ValueType::Pointer))
			{
				int size = li.type->get()->getSize();
				if (li.isStack())
					env->pushcode(new IL::pincL(li.address, size));
				else if (li.isGlobal())
					env->pushcode(new IL::pincG(li.address, size));
				else if (li.isMemory())
					env->pushcode(new IL::pincM(li.address, size));
				else
					env->err("?");
			}
			else
			{
				env->err("this type do not '++' operation");
			}
		}
		ValueInfo genR(Environment *env)
		{
			ValueInfo li = e->genL(env);
			ValueInfo to(env->getTemp(), li.type);
			ValueInfo ri = LtoR(env, li);
			env->pushcode(new IL::assign(to.address, ri.address));
			INC(env, li);
			return to;
		}
	};
	struct PreInc : UnaryExpression
	{
		PreInc(Exp x) : UnaryExpression(x){}
		ValueInfo genL(Environment *env)
		{
			ValueInfo li = e->genL(env);
			Inc::INC(env, li);
			return li;
		}
	};
	struct Dec : UnaryExpression
	{
		Dec(Exp x) : UnaryExpression(x){}
		static void DEC(Environment *env, ValueInfo &li)
		{
			if (li.type->isP(IL::ValueType::Int) || li.type->isP(IL::ValueType::UInt))
			{
				if (li.isStack())
					env->pushcode(new IL::decL(li.address));
				else if (li.isGlobal())
					env->pushcode(new IL::decG(li.address));
				else if (li.isMemory())
					env->pushcode(new IL::decM(li.address));
				else
					env->err("?");
			}
			else if (li.type->isP(IL::ValueType::Char))
			{
				if (li.isStack())
					env->pushcode(new IL::cdecL(li.address));
				else if (li.isGlobal())
					env->pushcode(new IL::cdecG(li.address));
				else if (li.isMemory())
					env->pushcode(new IL::cdecM(li.address));
				else
					env->err("?");
			}
			else if (li.type->isP(IL::ValueType::Pointer))
			{
				int size = li.type->get()->getSize();
				if (li.isStack())
					env->pushcode(new IL::pdecL(li.address, size));
				else if (li.isGlobal())
					env->pushcode(new IL::pdecG(li.address, size));
				else if (li.isMemory())
					env->pushcode(new IL::pdecM(li.address, size));
				else
					env->err("?");
			}
			else
			{
				env->err("this type do not '--' operation");
			}
		}
		ValueInfo genR(Environment *env)
		{
			ValueInfo li = e->genL(env);
			ValueInfo to(env->getTemp(), li.type);
			ValueInfo ri = LtoR(env, li);
			env->pushcode(new IL::assign(to.address, ri.address));
			DEC(env, li);
			return to;
		}
	};
	struct PreDec : UnaryExpression
	{
		PreDec(Exp x) : UnaryExpression(x){}
		ValueInfo genL(Environment *env)
		{
			ValueInfo li = e->genL(env);
			Dec::DEC(env, li);
			return li;
		}
	};
	struct Plus : UnaryExpression
	{
		Plus(Exp x) : UnaryExpression(x){}
		ValueInfo genR(Environment *env)
		{
			return e->genR(env);
		}
	};
	struct Minus : UnaryExpression
	{
		Minus(Exp x) : UnaryExpression(x){}
		ValueInfo genR(Environment *env)
		{
			ValueInfo ri = e->genR(env);
			ValueInfo to(env->getTemp(), ri.type);
			if (ri.type->isP(IL::ValueType::Int))
				env->pushcode(new IL::minus(to.address, ri.address));
			else if (ri.type->isP(IL::ValueType::Float))
				env->pushcode(new IL::fminus(to.address, ri.address));
			else
				env->err("this type do not unary'-' operation");
			return to;
		}
	};
	struct Not : UnaryExpression
	{
		Not(Exp x) : UnaryExpression(x){}
		ValueInfo genR(Environment *env)
		{
			ValueInfo ri = e->genR(env);
			ValueInfo to(env->getTemp(), ri.type);
			if (ri.type->isP(IL::ValueType::Bool))
				env->pushcode(new IL::Not(to.address, ri.address));
			else
				env->err("this type do not '!' operation");
			return to;
		}
	};
	struct Compl : UnaryExpression
	{
		Compl(Exp x) : UnaryExpression(x){}
		ValueInfo genR(Environment *env)
		{
			ValueInfo ri = e->genR(env);
			ValueInfo to(env->getTemp(), ri.type);
			if (ri.type->isP(IL::ValueType::Int) || ri.type->isP(IL::ValueType::UInt))
				env->pushcode(new IL::Compl(to.address, ri.address));
			else
				env->err("this type do not '~' operation");
			return to;
		}
	};
	struct Deref : UnaryExpression
	{
		Deref(Exp x) : UnaryExpression(x){}
		ValueInfo genL(Environment *env)
		{
			ValueInfo v = e->genR(env);
			if (!v.type->isP(IL::ValueType::Pointer))
			{
				env->err("this type don't operate unary'*'");
				return 0;
			}

			int size = v.type->get()->getSize();
			ValueInfo to(v.address, v.type->get());
			to.atype = ValueInfo::memory;
			return to;
		}
	};
	struct Ref : UnaryExpression
	{
		Ref(Exp x) : UnaryExpression(x){}
		ValueInfo genR(Environment *env)
		{
			ValueInfo v = e->genL(env);
			if (v.atype == ValueInfo::local)
			{
				ValueInfo to(env->getTemp(), new IL::Pointer(v.type));
				env->pushcode(new IL::getLocalPtr(to.address, v.address));
				return to;
			}
			else if (v.atype == ValueInfo::global)
			{
				ValueInfo to(env->getTemp(), new IL::Pointer(v.type));
				env->pushcode(new IL::getGlobalPtr(to.address, v.address));
				return to;
			}
			else if (v.atype == ValueInfo::memory)
			{
				// &*ptr みたいなこと？
				ValueInfo to(env->getTemp(), new IL::Pointer(v.type));
			}
			env->err("this type don't operate unary'&'");
			return 0;
		}
	};

	struct BinaryExpression : Expression
	{
		BinaryExpression(Exp left, Exp right)
		{
			l = left;
			r = right;
		}
		ValueInfo genR(Environment *env)
		{
			ValueInfo li = l->genR(env);
			ValueInfo ri = r->genR(env);
			ValueInfo to = env->getTemp();
			to.type = li.type;
			gen(env, to, li, ri);
			return to;
		}
		virtual void gen(Environment *env, ValueInfo &to, ValueInfo &li, ValueInfo &ri){}
	protected:
		Exp l;
		Exp r;
	};
	struct Add : BinaryExpression
	{
		Add(Exp left, Exp right) : BinaryExpression(left, right){}
		ValueInfo genR(Environment *env)
		{
			ValueInfo li = l->genR(env);
			ValueInfo ri = r->genR(env);
			ValueInfo to(env->getTemp(), li.type);
			gen(env, to, li, ri);
			return to;
		}
		void gen(Environment *env, ValueInfo &to, ValueInfo &li, ValueInfo &ri)
		{
			if (li.type->isP(IL::ValueType::Int))
			{
				if (ri.type->isP(IL::ValueType::Int))
				{
					env->pushcode(new IL::iadd(to.address, li.address, ri.address));
					return;
				}
				else if (ri.type->isP(IL::ValueType::Pointer))
				{
					to.type = ri.type;
					int size = ri.type->get()->getSize();
					env->pushcode(new IL::getInt(to.address, size));
					env->pushcode(new IL::imul(to.address, li.address, to.address));
					env->pushcode(new IL::iadd(to.address, to.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::UInt))
			{
				if (ri.type->isP(IL::ValueType::UInt))
				{
					env->pushcode(new IL::iadd(to.address, li.address, ri.address));
					return;
				}
				else if (ri.type->isP(IL::ValueType::Pointer))
				{
					to.type = ri.type;
					int size = ri.type->get()->getSize();
					env->pushcode(new IL::getInt(to.address, size));
					env->pushcode(new IL::imul(to.address, li.address, to.address));
					env->pushcode(new IL::iadd(to.address, to.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::Float))
			{
				if (ri.type->isP(IL::ValueType::Float))
				{
					env->pushcode(new IL::fadd(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::Char))
			{
				if (ri.type->isP(IL::ValueType::Int) || ri.type->isP(IL::ValueType::UInt))
				{
					env->pushcode(new IL::iadd(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::Pointer))
			{
				if (ri.type->isP(IL::ValueType::Int) || ri.type->isP(IL::ValueType::UInt))
				{
					int size = ri.type->get()->getSize();
					env->pushcode(new IL::getInt(to.address, size));
					env->pushcode(new IL::imul(to.address, ri.address, to.address));
					env->pushcode(new IL::iadd(to.address, to.address, li.address));
					return;
				}
			}
			env->err("this type do not '+' operation");
		}
	};
	struct Sub : BinaryExpression
	{
		Sub(Exp left, Exp right) : BinaryExpression(left, right){}
		void gen(Environment *env, ValueInfo &to, ValueInfo &li, ValueInfo &ri)
		{
			if (li.type->isP(IL::ValueType::Int))
			{
				if (ri.type->isP(IL::ValueType::Int))
				{
					env->pushcode(new IL::isub(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::UInt))
			{
				if (ri.type->isP(IL::ValueType::UInt))
				{
					env->pushcode(new IL::isub(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::Float))
			{
				if (ri.type->isP(IL::ValueType::Float))
				{
					env->pushcode(new IL::fsub(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::Char))
			{
				if (ri.type->isP(IL::ValueType::Int) || ri.type->isP(IL::ValueType::UInt) || ri.type->isP(IL::ValueType::Char))
				{
					env->pushcode(new IL::isub(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::Pointer))
			{
				if (ri.type->isP(IL::ValueType::Int) || ri.type->isP(IL::ValueType::UInt))
				{
					int size = ri.type->get()->getSize();
					env->pushcode(new IL::getInt(to.address, size));
					env->pushcode(new IL::imul(to.address, ri.address, to.address));
					env->pushcode(new IL::isub(to.address, to.address, li.address));
					return;
				}
				else if (ri.type->isP(IL::ValueType::Pointer))
				{
					to.type = new IL::Primitive(IL::ValueType::Int);
					env->pushcode(new IL::isub(to.address, li.address, ri.address));
					return;
				}
			}
			env->err("this type do not '-' operation");
		}
	};
	struct Mul : BinaryExpression
	{
		Mul(Exp left, Exp right) : BinaryExpression(left, right){}
		void gen(Environment *env, ValueInfo &to, ValueInfo &li, ValueInfo &ri)
		{
			if (li.type->isP(IL::ValueType::Int))
			{
				if (ri.type->isP(IL::ValueType::Int))
				{
					env->pushcode(new IL::imul(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::UInt))
			{
				if (ri.type->isP(IL::ValueType::UInt))
				{
					env->pushcode(new IL::imul(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::Float))
			{
				if (ri.type->isP(IL::ValueType::Float))
				{
					env->pushcode(new IL::fmul(to.address, li.address, ri.address));
					return;
				}
			}
			env->err("this type do not '*' operation");
		}
	};
	struct Div : BinaryExpression
	{
		Div(Exp left, Exp right) : BinaryExpression(left, right){}
		void gen(Environment *env, ValueInfo &to, ValueInfo &li, ValueInfo &ri)
		{
			if (li.type->isP(IL::ValueType::Int))
			{
				if (ri.type->isP(IL::ValueType::Int))
				{
					env->pushcode(new IL::idiv(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::UInt))
			{
				if (ri.type->isP(IL::ValueType::UInt))
				{
					env->pushcode(new IL::idiv(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::Float))
			{
				if (ri.type->isP(IL::ValueType::Float))
				{
					env->pushcode(new IL::fdiv(to.address, li.address, ri.address));
					return;
				}
			}
			env->err("this type do not '/' operation");
		}
	};
	struct Mod : BinaryExpression
	{
		Mod(Exp left, Exp right) : BinaryExpression(left, right){}
		void gen(Environment *env, ValueInfo &to, ValueInfo &li, ValueInfo &ri)
		{
			if (li.type->isP(IL::ValueType::Int))
			{
				if (ri.type->isP(IL::ValueType::Int))
				{
					env->pushcode(new IL::imod(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::UInt))
			{
				if (ri.type->isP(IL::ValueType::UInt))
				{
					env->pushcode(new IL::imod(to.address, li.address, ri.address));
					return;
				}
			}
			env->err("this type do not '%' operation");
		}
	};
	struct SHL : BinaryExpression
	{
		SHL(Exp left, Exp right) : BinaryExpression(left, right){}
		void gen(Environment *env, ValueInfo &to, ValueInfo &li, ValueInfo &ri)
		{
			if (li.type->isP(IL::ValueType::Int))
			{
				if (ri.type->isP(IL::ValueType::Int) || ri.type->isP(IL::ValueType::UInt))
				{
					env->pushcode(new IL::ishl(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::UInt))
			{
				if (ri.type->isP(IL::ValueType::Int) || ri.type->isP(IL::ValueType::UInt))
				{
					env->pushcode(new IL::ishl(to.address, li.address, ri.address));
					return;
				}
			}
			env->err("this type do not '<<' operation");
		}
	};
	struct SHR : BinaryExpression
	{
		SHR(Exp left, Exp right) : BinaryExpression(left, right){}
		void gen(Environment *env, ValueInfo &to, ValueInfo &li, ValueInfo &ri)
		{
			if (li.type->isP(IL::ValueType::Int))
			{
				if (ri.type->isP(IL::ValueType::Int) || ri.type->isP(IL::ValueType::UInt))
				{
					env->pushcode(new IL::ishr(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::UInt))
			{
				if (ri.type->isP(IL::ValueType::Int) || ri.type->isP(IL::ValueType::UInt))
				{
					env->pushcode(new IL::ushr(to.address, li.address, ri.address));
					return;
				}
			}
			env->err("this type do not '>>' operation");
		}
	};
	struct AND : BinaryExpression
	{
		AND(Exp left, Exp right) : BinaryExpression(left, right){}
		void gen(Environment *env, ValueInfo &to, ValueInfo &li, ValueInfo &ri)
		{
			if (li.type->isP(IL::ValueType::Int))
			{
				if (ri.type->isP(IL::ValueType::Int))
				{
					env->pushcode(new IL::iand(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::UInt))
			{
				if (ri.type->isP(IL::ValueType::UInt))
				{
					env->pushcode(new IL::iand(to.address, li.address, ri.address));
					return;
				}
			}
			env->err("this type do not '&' operation");
		}
	};
	struct XOR : BinaryExpression
	{
		XOR(Exp left, Exp right) : BinaryExpression(left, right){}
		void gen(Environment *env, ValueInfo &to, ValueInfo &li, ValueInfo &ri)
		{
			if (li.type->isP(IL::ValueType::Int))
			{
				if (ri.type->isP(IL::ValueType::Int))
				{
					env->pushcode(new IL::ixor(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::UInt))
			{
				if (ri.type->isP(IL::ValueType::UInt))
				{
					env->pushcode(new IL::ixor(to.address, li.address, ri.address));
					return;
				}
			}
			env->err("this type do not '^' operation");
		}
	};
	struct OR : BinaryExpression
	{
		OR(Exp left, Exp right) : BinaryExpression(left, right){}
		void gen(Environment *env, ValueInfo &to, ValueInfo &li, ValueInfo &ri)
		{
			if (li.type->isP(IL::ValueType::Int))
			{
				if (ri.type->isP(IL::ValueType::Int))
				{
					env->pushcode(new IL::ior(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::UInt))
			{
				if (ri.type->isP(IL::ValueType::UInt))
				{
					env->pushcode(new IL::ior(to.address, li.address, ri.address));
					return;
				}
			}
			env->err("this type do not '|' operation");
		}
	};
	struct LT : BinaryExpression
	{
		LT(Exp left, Exp right) : BinaryExpression(left, right){}
		void gen(Environment *env, ValueInfo &to, ValueInfo &li, ValueInfo &ri)
		{
			to.type = new IL::Primitive(IL::ValueType::Bool);
			if (li.type->isP(IL::ValueType::Int))
			{
				if (ri.type->isP(IL::ValueType::Int))
				{
					env->pushcode(new IL::ilt(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::UInt))
			{
				if (ri.type->isP(IL::ValueType::UInt))
				{
					env->pushcode(new IL::ult(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::Char))
			{
				if (ri.type->isP(IL::ValueType::Char))
				{
					env->pushcode(new IL::clt(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::Float))
			{
				if (ri.type->isP(IL::ValueType::Float))
				{
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::Pointer))
			{
				if (ri.type->isP(IL::ValueType::Pointer))
				{
					env->pushcode(new IL::ult(to.address, li.address, ri.address));
					return;
				}
			}
			env->err("this type do not '<' operation");
		}
	};
	struct LE : BinaryExpression
	{
		LE(Exp left, Exp right) : BinaryExpression(left, right){}
		void gen(Environment *env, ValueInfo &to, ValueInfo &li, ValueInfo &ri)
		{
			to.type = new IL::Primitive(IL::ValueType::Bool);
			if (li.type->isP(IL::ValueType::Int))
			{
				if (ri.type->isP(IL::ValueType::Int))
				{
					env->pushcode(new IL::ile(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::UInt))
			{
				if (ri.type->isP(IL::ValueType::UInt))
				{
					env->pushcode(new IL::ule(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::Char))
			{
				if (ri.type->isP(IL::ValueType::Char))
				{
					env->pushcode(new IL::cle(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::Float))
			{
				if (ri.type->isP(IL::ValueType::Float))
				{
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::Pointer))
			{
				if (ri.type->isP(IL::ValueType::Pointer))
				{
					env->pushcode(new IL::ule(to.address, li.address, ri.address));
					return;
				}
			}
			env->err("this type do not '<=' operation");
		}
	};
	struct GT : BinaryExpression
	{
		GT(Exp left, Exp right) : BinaryExpression(left, right){}
		void gen(Environment *env, ValueInfo &to, ValueInfo &li, ValueInfo &ri)
		{
			to.type = new IL::Primitive(IL::ValueType::Bool);
			if (li.type->isP(IL::ValueType::Int))
			{
				if (ri.type->isP(IL::ValueType::Int))
				{
					env->pushcode(new IL::igt(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::UInt))
			{
				if (ri.type->isP(IL::ValueType::UInt))
				{
					env->pushcode(new IL::ugt(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::Char))
			{
				if (ri.type->isP(IL::ValueType::Char))
				{
					env->pushcode(new IL::cgt(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::Float))
			{
				if (ri.type->isP(IL::ValueType::Float))
				{
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::Pointer))
			{
				if (ri.type->isP(IL::ValueType::Pointer))
				{
					env->pushcode(new IL::ugt(to.address, li.address, ri.address));
					return;
				}
			}
			env->err("this type do not '>' operation");
		}
	};
	struct GE : BinaryExpression
	{
		GE(Exp left, Exp right) : BinaryExpression(left, right){}
		void gen(Environment *env, ValueInfo &to, ValueInfo &li, ValueInfo &ri)
		{
			to.type = new IL::Primitive(IL::ValueType::Bool);
			if (li.type->isP(IL::ValueType::Int))
			{
				if (ri.type->isP(IL::ValueType::Int))
				{
					env->pushcode(new IL::ige(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::UInt))
			{
				if (ri.type->isP(IL::ValueType::UInt))
				{
					env->pushcode(new IL::uge(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::Char))
			{
				if (ri.type->isP(IL::ValueType::Char))
				{
					env->pushcode(new IL::cge(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::Float))
			{
				if (ri.type->isP(IL::ValueType::Float))
				{
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::Pointer))
			{
				if (ri.type->isP(IL::ValueType::Pointer))
				{
					env->pushcode(new IL::uge(to.address, li.address, ri.address));
					return;
				}
			}
			env->err("this type do not '>=' operation");
		}
	};
	struct EQ : BinaryExpression
	{
		EQ(Exp left, Exp right) : BinaryExpression(left, right){}
		void gen(Environment *env, ValueInfo &to, ValueInfo &li, ValueInfo &ri)
		{
			to.type = new IL::Primitive(IL::ValueType::Bool);
			if (li.type->isP(IL::ValueType::Int))
			{
				if (ri.type->isP(IL::ValueType::Int))
				{
					env->pushcode(new IL::ieq(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::UInt))
			{
				if (ri.type->isP(IL::ValueType::UInt))
				{
					env->pushcode(new IL::ieq(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::Char))
			{
				if (ri.type->isP(IL::ValueType::Char))
				{
					env->pushcode(new IL::ceq(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::Float))
			{
				if (ri.type->isP(IL::ValueType::Float))
				{
					env->pushcode(new IL::ieq(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::Pointer))
			{
				if (ri.type->isP(IL::ValueType::Pointer))
				{
					env->pushcode(new IL::ieq(to.address, li.address, ri.address));
					return;
				}
			}
			env->err("this type do not '==' operation");
		}
	};
	struct NE : BinaryExpression
	{
		NE(Exp left, Exp right) : BinaryExpression(left, right){}
		void gen(Environment *env, ValueInfo &to, ValueInfo &li, ValueInfo &ri)
		{
			to.type = new IL::Primitive(IL::ValueType::Bool);
			if (li.type->isP(IL::ValueType::Int))
			{
				if (ri.type->isP(IL::ValueType::Int))
				{
					env->pushcode(new IL::ine(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::UInt))
			{
				if (ri.type->isP(IL::ValueType::UInt))
				{
					env->pushcode(new IL::ine(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::Char))
			{
				if (ri.type->isP(IL::ValueType::Char))
				{
					env->pushcode(new IL::cne(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::Float))
			{
				if (ri.type->isP(IL::ValueType::Float))
				{
					env->pushcode(new IL::ine(to.address, li.address, ri.address));
					return;
				}
			}
			else if (li.type->isP(IL::ValueType::Pointer))
			{
				if (ri.type->isP(IL::ValueType::Pointer))
				{
					env->pushcode(new IL::ine(to.address, li.address, ri.address));
					return;
				}
			}
			env->err("this type do not '==' operation");
		}
	};
	struct BOOL_AND : BinaryExpression
	{
		BOOL_AND(Exp left, Exp right) : BinaryExpression(left, right){}
		ValueInfo genR(Environment *env)
		{
			ValueInfo li = l->genR(env);
			int l1 = env->getLabel();
			int l2 = env->getLabel();
			ValueInfo to(env->getTemp(), new IL::Primitive(IL::ValueType::Bool));
			env->pushcode(new IL::jump_true(li.address, l1));
			env->pushcode(new IL::assign(to.address, li.address));
			env->pushcode(new IL::jump(l2));
			env->addLabel(l1);
			ValueInfo ri = r->genR(env);
			env->pushcode(new IL::assign(to.address, ri.address));
			env->addLabel(l2);
			return to;
		}
	};
	struct BOOL_OR : BinaryExpression
	{
		BOOL_OR(Exp left, Exp right) : BinaryExpression(left, right){}
		ValueInfo genR(Environment *env)
		{
			ValueInfo li = l->genR(env);
			int l1 = env->getLabel();
			int l2 = env->getLabel();
			ValueInfo to(env->getTemp(), new IL::Primitive(IL::ValueType::Bool));
			env->pushcode(new IL::jump_false(li.address, l1));
			env->pushcode(new IL::assign(to.address, li.address));
			env->pushcode(new IL::jump(l2));
			env->addLabel(l1);
			ValueInfo ri = r->genR(env);
			env->pushcode(new IL::assign(to.address, ri.address));
			env->addLabel(l2);
			return to;
		}
	};
	struct Assign : BinaryExpression
	{
		Assign(Exp left, Exp right) : BinaryExpression(left, right){}
		ValueInfo genL(Environment *env)
		{
			ValueInfo li = l->genL(env);
			ValueInfo ri = r->genR(env);
			assign(env, li, ri);
			return li;
		}
		static void assign(Environment *env, ValueInfo &li, ValueInfo &ri)
		{
			if (!li.type->isT(ri.type))
			{
				env->err("do not '=' different type");
				return;
			}

			if (li.atype == ValueInfo::local)
			{
				if (li.type->getSize() == 4)
					env->pushcode(new IL::assign(li.address, ri.address));
				else if (li.type->getSize() == 1)
					env->pushcode(new IL::cassign(li.address, ri.address));
				else
					env->err("do not = large size type");
			}
			else if (li.atype == ValueInfo::global)
			{
				if (li.type->getSize() == 4)
					env->pushcode(new IL::set_global(li.address, ri.address));
				else if (li.type->getSize() == 1)
					env->pushcode(new IL::cset_global(li.address, ri.address));
				else
					env->err("do not = large size type");
			}
			else if (li.atype == ValueInfo::memory)
			{
				if (li.type->getSize() == 4)
					env->pushcode(new IL::set_memory(li.address, ri.address));
				else if (li.type->getSize() == 1)
					env->pushcode(new IL::cset_memory(li.address, ri.address));
				else
					env->err("do not = large size type");
			}
		}
		ValueInfo genR(Environment *env){return LtoR(env, genL(env));}
	};
	struct AddAssign : Assign
	{
		AddAssign(Exp left, Exp right) : Assign(left, right){}
		ValueInfo genL(Environment *env)
		{
			ValueInfo li = l->genL(env);
			ValueInfo ri = r->genR(env);

			ValueInfo lri = LtoR(env, li);
			ValueInfo to(env->getTemp(), li.type);
			shptr<Add> e = new Add(0, 0);
			e->gen(env, to, lri, ri);
			assign(env, li, to);

			return li;
		}
	};
	struct SubAssign : Assign
	{
		SubAssign(Exp left, Exp right) : Assign(left, right){}
		ValueInfo genL(Environment *env)
		{
			ValueInfo li = l->genL(env);
			ValueInfo ri = r->genR(env);

			ValueInfo lri = LtoR(env, li);
			ValueInfo to(env->getTemp(), li.type);
			shptr<Sub> e = new Sub(0, 0);
			e->gen(env, to, lri, ri);
			assign(env, li, to);

			return li;
		}
	};
	struct MulAssign : Assign
	{
		MulAssign(Exp left, Exp right) : Assign(left, right){}
		ValueInfo genL(Environment *env)
		{
			ValueInfo li = l->genL(env);
			ValueInfo ri = r->genR(env);

			ValueInfo lri = LtoR(env, li);
			ValueInfo to(env->getTemp(), li.type);
			shptr<Mul> e = new Mul(0, 0);
			e->gen(env, to, lri, ri);
			assign(env, li, to);

			return li;
		}
	};
	struct DivAssign : Assign
	{
		DivAssign(Exp left, Exp right) : Assign(left, right){}
		ValueInfo genL(Environment *env)
		{
			ValueInfo li = l->genL(env);
			ValueInfo ri = r->genR(env);

			ValueInfo lri = LtoR(env, li);
			ValueInfo to(env->getTemp(), li.type);
			shptr<Div> e = new Div(0, 0);
			e->gen(env, to, lri, ri);
			assign(env, li, to);

			return li;
		}
	};
	struct ModAssign : Assign
	{
		ModAssign(Exp left, Exp right) : Assign(left, right){}
		ValueInfo genL(Environment *env)
		{
			ValueInfo li = l->genL(env);
			ValueInfo ri = r->genR(env);

			ValueInfo lri = LtoR(env, li);
			ValueInfo to(env->getTemp(), li.type);
			shptr<Mod> e = new Mod(0, 0);
			e->gen(env, to, lri, ri);
			assign(env, li, to);

			return li;
		}
	};
	struct SHLAssign : Assign
	{
		SHLAssign(Exp left, Exp right) : Assign(left, right){}
		ValueInfo genL(Environment *env)
		{
			ValueInfo li = l->genL(env);
			ValueInfo ri = r->genR(env);

			ValueInfo lri = LtoR(env, li);
			ValueInfo to(env->getTemp(), li.type);
			shptr<SHL> e = new SHL(0, 0);
			e->gen(env, to, lri, ri);
			assign(env, li, to);

			return li;
		}
	};
	struct SHRAssign : Assign
	{
		SHRAssign(Exp left, Exp right) : Assign(left, right){}
		ValueInfo genL(Environment *env)
		{
			ValueInfo li = l->genL(env);
			ValueInfo ri = r->genR(env);

			ValueInfo lri = LtoR(env, li);
			ValueInfo to(env->getTemp(), li.type);
			shptr<SHR> e = new SHR(0, 0);
			e->gen(env, to, lri, ri);
			assign(env, li, to);

			return li;
		}
	};
	struct ANDAssign : Assign
	{
		ANDAssign(Exp left, Exp right) : Assign(left, right){}
		ValueInfo genL(Environment *env)
		{
			ValueInfo li = l->genL(env);
			ValueInfo ri = r->genR(env);

			ValueInfo lri = LtoR(env, li);
			ValueInfo to(env->getTemp(), li.type);
			shptr<AND> e = new AND(0, 0);
			e->gen(env, to, lri, ri);
			assign(env, li, to);

			return li;
		}
	};
	struct XORAssign : Assign
	{
		XORAssign(Exp left, Exp right) : Assign(left, right){}
		ValueInfo genL(Environment *env)
		{
			ValueInfo li = l->genL(env);
			ValueInfo ri = r->genR(env);

			ValueInfo lri = LtoR(env, li);
			ValueInfo to(env->getTemp(), li.type);
			shptr<XOR> e = new XOR(0, 0);
			e->gen(env, to, lri, ri);
			assign(env, li, to);

			return li;
		}
	};
	struct ORAssign : Assign
	{
		ORAssign(Exp left, Exp right) : Assign(left, right){}
		ValueInfo genL(Environment *env)
		{
			ValueInfo li = l->genL(env);
			ValueInfo ri = r->genR(env);

			ValueInfo lri = LtoR(env, li);
			ValueInfo to(env->getTemp(), li.type);
			shptr<OR> e = new OR(0, 0);
			e->gen(env, to, lri, ri);
			assign(env, li, to);

			return li;
		}
	};

	struct TernaryOperater : Expression
	{
		TernaryOperater(Exp cond, Exp left, Exp right)
		{
			c = cond;
			l = left;
			r = right;
		}
		ValueInfo genL(Environment *env)
		{
			ValueInfo ci = c->genR(env);
			if (!ci.type->isP(IL::ValueType::Bool))
			{
				env->err("(cond)?: only bool");
			}
			int l1 = env->getLabel();
			int l2 = env->getLabel();
			env->pushcode(new IL::jump_false(ci.address, l1));
			ValueInfo li = l->genR(env);
			ValueInfo to(env->getTemp(), li.type);
			env->pushcode(new IL::assign(to.address, li.address));
			env->pushcode(new IL::jump(l2));
			env->addLabel(l1);
			ValueInfo ri = r->genR(env);
			env->pushcode(new IL::assign(to.address, ri.address));
			env->addLabel(l2);
			return to;
		}
	private:
		Exp c;
		Exp l;
		Exp r;
	};

	struct Call : Expression
	{
		Call(Exp f) : func(f){}
		void Add(Exp a)	{args.push_back(a);}
		ValueInfo genR(Environment *env)
		{
			ValueInfo f = func->genR(env);
			ValueInfo to(env->getTemp(), f.type->get());

			if (!f.type->isP(IL::ValueType::Function))
			{
				env->err("call not function");
				return to;
			}

			int argsize = 0;
			if ((int)args.size() != f.type->getASize())
			{
				env->err("argnum");
				return to;
			}
			int i = 0;
			for (vector<Exp>::reverse_iterator it = args.rbegin(); it != args.rend(); ++it)
			{
				ValueInfo a = (*it)->genR(env);
				env->pushcode(new IL::push(a.address));
				if (!a.type->isT(f.type->getA(i++)))
				{
					env->err("mismatch argument type");
					return to;
				}
				argsize += 4;
			}
			env->pushcode(new IL::call(to.address, f.address));
			env->pushcode(new IL::pop_arg(argsize));
			env->pushcode(new IL::get_return(to.address));
			return to;
		}
	private:
		Exp func;
		vector<Exp> args;
	};
	struct Index : Expression
	{
		Index(Exp e, Exp i) : exp(e), index(i){}
		ValueInfo genL(Environment *env)
		{
			ValueInfo e = exp->genR(env);
			ValueInfo i = index->genR(env);

			if (!e.type->isP(IL::ValueType::Pointer))
			{
				std::printf("%d\n", e.type->isP(IL::ValueType::Char));
				env->err("this type don't operate []");
				return 0;
			}

			int size = e.type->get()->getSize();
			ValueInfo to(env->getTemp(), e.type->get());
			env->pushcode(new IL::getInt(to.address, size));
			env->pushcode(new IL::imul(to.address, i.address, to.address));
			env->pushcode(new IL::iadd(to.address, to.address, e.address));
			to.atype = ValueInfo::memory;
			return to;
		}
	private:
		Exp exp;
		Exp index;
	};
	struct Member : Expression
	{
		Member(Exp e, const string &s) : exp(e), name(s){}
		ValueInfo genL(Environment *env)
		{
			ValueInfo v = exp->genL(env);

			if (v.atype == ValueInfo::NameSpace)
			{
				return env->getNVar(name);
			}
			if (v.atype == ValueInfo::TypeName)
			{
				// 多分enum
				IL::VarInfo en = v.type->getMember(name);
				IL::ValueInfo to(env->getTemp(), new IL::Primitive(IL::ValueType::Int));
				env->pushcode(new IL::getInt(to.address, en.address));
				return to;
			}

			IL::VarInfo m = v.type->getMember(name);
			v.type = m.type;
			if (v.atype == ValueInfo::memory)
			{
				int t = env->getTemp();
				env->pushcode(new IL::getInt(t, m.address));
				env->pushcode(new IL::iadd(t, v.address, t));
				v.address = t;
			}
			else
			{
				v.address += m.address;
			}

			return v;
		}
	private:
		Exp exp;
		string name;
	};

	struct VarInfo : NameElement
	{
		VarInfo(const string &n, Type t, Exp e) : NameElement(n)
		{
			type = t;
			init = e;
		}
		Type getType(){return type;}
		void gene(Environment *env){}
	protected:
		Type type;
		Exp init;
	};
	typedef shptr<VarInfo> Var;

	struct TypeDefBase : NameElement	// これいる？
	{
		TypeDefBase(const string &s) : NameElement(s){}
	};
	struct TypeDef : TypeDefBase
	{
		TypeDef(const string &s) : TypeDefBase(s){}
	private:
		Type type;
	};
	struct Struct : TypeDefBase
	{
		Struct(const string &s) : TypeDefBase(s){}
		void add(Var v)
		{
			member.push_back(v);
		}
		void gene(Environment *env)
		{
			IL::Struct * s = new IL::Struct;
			VType t = s;

			for (vector<Var>::iterator it = member.begin(); it != member.end(); ++it)
				s->add((*it)->getName(), (*it)->getType()->gen(env));
			s->end();

			env->addType(name, t);
		}
	private:
		vector<Var> member;
	};
	struct Union : TypeDefBase
	{
		Union(const string &s) : TypeDefBase(s){}
		void add(Var v)
		{
			member.push_back(v);
		}
		void gene(Environment *env)
		{
			IL::Union *s = new IL::Union;
			VType t = s;

			for (vector<Var>::iterator it = member.begin(); it != member.end(); ++it)
				s->add((*it)->getName(), (*it)->getType()->gen(env));

			env->addType(name, t);
		}
	private:
		vector<Var> member;
	};
	struct Enum : TypeDefBase
	{
		Enum(const string &s) : TypeDefBase(s){m = 0;}
		void add(const string &name, int i)
		{
			member[name] = i;
			m = i + 1;
		}
		void add(const string &name)
		{
			member[name] = m++;
		}
		void gene(Environment *env)
		{
			IL::Enum *e = new IL::Enum;
			VType t = e;

			for (std::map<string, int>::iterator it = member.begin(); it != member.end(); ++it)
				e->add(it->first, it->second);

			env->addType(name, t);
		}
	private:
		std::map<string, int> member;
		int m;
	};

	struct Statement
	{
		virtual ~Statement(){}
		virtual void gen(Environment *env)
		{
			gen_state(env);
			env->newState();
		}
		virtual void gen_state(Environment *env){}
	};
	typedef shptr<Statement> State;

	struct Statements : Statement
	{
		void Add(State s)
		{
			if (s)
				statements.push_back(s);
		}
		void gen_state(Environment *env)
		{
			for (vector<State>::iterator it = statements.begin(); it != statements.end(); ++it)
			{
				(*it)->gen(env);
			}
		}
	private:
		vector<State> statements;
	};

	struct ExpressionStatement : Statement
	{
		ExpressionStatement(Exp e)
		{
			exp = e;
		}
		void gen_state(Environment *env)
		{
			exp->genR(env);
		}
	private:
		Exp exp;
	};

	struct Return : Statement
	{
		Return(Exp e = NULL)
		{
			exp = e;
		}
		void gen_state(Environment *env)
		{
			VType type = env->getReturnType();
			if (exp)
			{
				ValueInfo vi = exp->genR(env);
				if (!vi.type->isT(type))
				{
					env->err("return type mismatch");
				}
				env->pushcode(new IL::set_return(vi.address));
			}
			else
			{
				if (!type->isP(IL::ValueType::Void))
				{
					env->err("function need return value");
				}
			}
			env->pushcode(new IL::Return);
		}
	private:
		Exp exp;
	};
	struct Break : Statement
	{
		void gen_state(Environment *env)
		{
			env->pushcode(new IL::jump(env->getBreakLabel()));
		}
	};
	struct Continue : Statement
	{
		void gen_state(Environment *env)
		{
			env->pushcode(new IL::jump(env->getContinueLabel()));
		}
	};

	struct Declaration : Statement
	{
		Declaration(Var v)					{var = v;}
		void gen_state(Environment *env)	{var->gen(env);}
	private:
		Var var;
	};

	struct If : Statement
	{
		If(Exp c, State i, State e) : cond(c), if_s(i), else_s(e){}
		void gen_state(Environment *env)
		{
			ValueInfo ci = cond->genR(env);
			if (!ci.type->isP(IL::ValueType::Bool))
			{
				env->err("if (cond) only bool");
			}
			int l1 = env->getLabel();
			env->pushcode(new IL::jump_false(ci.address, l1));
			env->newState();
			if_s->gen(env);
			if (else_s)
			{
				int l2 = env->getLabel();
				env->pushcode(new IL::jump(l2));
				env->addLabel(l1);
				else_s->gen(env);
				env->addLabel(l2);
			}
			else
			{
				env->addLabel(l1);
			}
		}
	private:
		Exp cond;
		State if_s;
		State else_s;
	};
	struct While : Statement
	{
		While(Exp c, State s, State e) : cond(c), state(s), else_s(e){}
		void gen_state(Environment *env)
		{
			int l_break = env->getLabel();
			int l_continue = env->getLabel();
			env->addLabel(l_continue);
			ValueInfo ci = cond->genR(env);
			if (!ci.type->isP(IL::ValueType::Bool))
			{
				env->err("while (cond) only bool");
			}
			env->EnterLoop(l_break, l_continue);
			int l_else = else_s ? env->getLabel() : l_break;
			env->pushcode(new IL::jump_false(ci.address, l_else));
			env->newState();
			state->gen(env);
			env->pushcode(new IL::jump(l_continue));
			if (else_s)
			{
				env->addLabel(l_else);
				else_s->gen(env);
			}
			env->addLabel(l_break);
			env->LeaveLoop();
		}
	private:
		Exp cond;
		State state;
		State else_s;
	};

	typedef shptr<vector<Var> > Args;
	struct Function : NameElement
	{
		Function(const string &n, Args a, Type t, State s) : NameElement(n)
		{
			args = a;
			ret = t;
			statement = s;
		}
		void gene(Environment *env)
		{
			VType rtype;
			if (ret)
				rtype = ret->gen(env);
			else
			{
				// returnする値によって自動的に決まればいいな
				rtype = new IL::Primitive(IL::ValueType::Void);
			}
			env->EnterFunction(name, rtype);
			if (args)
			{
				for (vector<Var>::iterator it = args->begin(); it != args->end(); ++it)
				{
					(*it)->gen(env);
				}
			}
			statement->gen(env);
			env->setReturn();
			env->pushcode(new IL::end());
			env->LeaveFunction();
		}
	private:
		Args args;
		Type ret;
		State statement;
	};

	struct TypeName : TypeBase
	{
		TypeName(const string &s){name = s;}
		string getName()
		{
			return name;
		}
		VType gen(Environment *env)
		{
			return env->getType(name);
		}
	private:
		string name;
	};
	struct NameSpaceQualifier : TypeBase
	{
		NameSpaceQualifier(const string &s, Type t){name = s;type = t;}
		string getName()
		{
			return name;
		}
		VType gen(Environment *env)
		{
			return NULL;
		}
	private:
		string name;
		Type type;
	};
	struct Array : TypeBase
	{
		Array(Type t, int s)
		{
			type = t;
			size = s;
		}
		string getName()
		{
			char buf[20];
			std::sprintf(buf, "%d", size);
			return type->getName() + "[" + buf + "]";
		}
		VType gen(Environment *env)
		{
			return new IL::Array(type->gen(env), size);
		}
	private:
		Type type;
		int size;
	};
	struct Pointer : TypeBase
	{
		Pointer(Type t)
		{
			type = t;
		}
		string getName()
		{
			return type->getName() + "*";
		}
		VType gen(Environment *env)
		{
			return type->gen(env);
		}
	private:
		Type type;
	};
	struct FuncPtr : TypeBase
	{
		FuncPtr(Type r, shptr<vector<Type> > a) : ret(r), args(a){}
		string getName()
		{
			string s = "(";
			for (vector<Type>::iterator it = args->begin(); it != args->end(); ++it)
				s += (*it)->getName() + ",";
			return s + "):" + ret->getName();
			// getNameいらなくね？ってのはともかく
		}
		VType gen(Environment *env)
		{
			VType t = ret->gen(env);
			IL::FuncPtr *fp = new IL::FuncPtr(t);
			VType r = fp;
			if (args)
			{
				for (vector<Type>::iterator it = args->begin(); it != args->end(); ++it)
					fp->add((*it)->gen(env));
			}
			return r;
		}
	private:
		shptr<vector<Type> > args;
		Type ret;
	};

	struct GlobalVar : VarInfo
	{
		GlobalVar(const string &n, Type t, Exp e) : VarInfo(n, t, e){}
		void gene(Environment *env)
		{
			if (!type)
			{
				env->err(name + " is no type");
				return;
			}

			VType t = type->gen(env);
			int g = env->addGlobal(name, t);
			if (init)
			{
				env->EnterFunction(name, t);
				ValueInfo v = init->genR(env);
				if (!t->isT(v.type))
				{
					env->err(name + " initial-exp type mismatch");
				}
				env->pushcode(new IL::set_global(g, v.address));
				env->LeaveFunction();
				env->initGlobal(name);
			}
		}
	};
	struct Argument : VarInfo
	{
		Argument(const string &n, Type t, Exp e) : VarInfo(n, t, e){}
		void gene(Environment *env)
		{
			if (!type)
			{
				env->err("argument-type can not omit");
			}
			else
			{
				VType t = type->gen(env);
				env->addArg(name, t);
			}
		}
	};
	struct LocalVar : VarInfo
	{
		LocalVar(const string &n, Type t, Exp e) : VarInfo(n, t, e){}
		void gene(Environment *env)
		{

			/*
			式の戻りが配列や構造体になることはない
			それは確定
			じゃあ初期化構文は？
			var arr : int[10] = {1, 2, 3, 4};
			配列(構造体)初期化構文はパーサレベルで特殊化すべきな気もする
			基本は32bitだけ確保しておけばまあいいか
			*/

			ValueInfo ri;
			if (!type)	// 型省略
			{
				if (!init)
				{
					env->err("variable declaration needs type or init or both");
					return;
				}
				env->getTemp();	// 割り当てる変数用の領域を確保しておく
				ri = init->genR(env);
				env->addLocal(name, ri.type);
			}
			else
			{
				VType t = type->gen(env);
				env->addLocal(name, t);
				if (!init)
				{
					return;
				}
				ri = init->genR(env);
				if (!t->isT(ri.type))
				{
					env->err("variable-type is not same as initial-type");
					return;
				}
			}
			ValueInfo li = env->getVariable(name);
			if (li.atype != ValueInfo::local)
			{
				env->err("?");
			}
			if (li.type->getSize() == 4)
				env->pushcode(new IL::assign(li.address, ri.address));
			else if (li.type->getSize() == 1)
				env->pushcode(new IL::cassign(li.address, ri.address));
			else
				env->err("do not initialize large size type");
		}
	};
}

}
#endif
