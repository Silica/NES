#ifndef NES_IL_H
#define NES_IL_H

#include <string>
#include <map>
#include <vector>
#include <cstdio>
#include <cstring>

#include "x86.h"

namespace NES{

template<class T>struct Deleter
{
	void operator()(T *t)
	{
		delete t;
	}
};
template<class T, class D = Deleter<T> >class shptr
{
public:
	shptr(){x = NULL;rc = NULL;}
	shptr(T *t){x = t;setrc();}
	shptr(const shptr &s)
	{
		x = s.x;
		inc(s.rc);
	}
	#ifdef __BORLANDC__
	template<class T, class D> friend class shptr;
	#else
	template<class X, class Y> friend class shptr;
	#endif
	template<class X> shptr(const shptr<X> &s)
	{
		x = s.x;
		inc(s.rc);
	}
	shptr &operator=(const shptr &s)
	{
		dec();
		x = s.x;
		inc(s.rc);
		return *this;
	}
	template<class X>shptr &operator=(const shptr<X> &s)
	{
		dec();
		x = s.x;
		inc(s.rc);
		return *this;
	}
	shptr &operator=(T *t)
	{
		dec();
		x = t;
		setrc();
		return *this;
	}
	      T *get()            {return x;}
	const T *get()       const{return x;}
	      T *operator->()     {return x;}
	const T *operator->()const{return x;}
	      T &operator*()      {return *x;}
	const T &operator*() const{return *x;}
	operator       T*()       {return x;}
	operator const T*()  const{return x;}
	operator       T&()       {return *x;}
	operator const T&()  const{return *x;}
	operator bool() const{return x != NULL;}
	bool operator!()const{return x == NULL;}
	~shptr(){dec();}
	void release(){dec();}
private:
	void setrc()
	{
		if (x)
			rc = new int(1);
		else
			rc = NULL;
	}
	void inc(int *r)
	{
		rc = r;
		if (rc)
			++*rc;
	}
	void dec()
	{
		if (rc)
		{
			if (!--*rc)
			{
				delete rc;
				D()(x);
			}
			rc = NULL;
			x = NULL;
		}
	}
	T *x;
	int *rc;
};

namespace IL
{
	using std::string;
	using std::vector;
	class ValueType;
	typedef shptr<ValueType> VType;
	typedef unsigned long dword;
	struct VarInfo
	{
		string name;
		VType type;
		int address;
	};
	struct ValueType
	{
		enum primitive
		{
			Void,
			Bool,
			Int,
			UInt,
			Char,
			Float,
			Pointer,
			Array,
			Function,
			Struct,
			Union,
			Enum,
		};
		virtual ~ValueType(){}
		virtual int getSize(){return 0;}
		virtual bool isP(primitive p){return false;}
		virtual bool isT(ValueType *p){return false;}
		virtual VType get(){return NULL;}
		virtual VType getA(int i){return NULL;}
		virtual int getASize(){return 0;}
		virtual VarInfo getMember(const string &name){return VarInfo();}
	};
	struct Primitive : ValueType
	{
		primitive type;
		Primitive(primitive p)
		{
			type = p;
		}
		int getSize()
		{
			if (type == Void)
				return 0;
			else if (type == Char || type == Bool)
				return 1;
			return 4;
		}
		bool isP(primitive p)		{return type == p;}
		bool isT(ValueType *t)		{return t->isP(type);}
	};
	struct Array : ValueType
	{
		Array(VType t, int s) : type(t), size(s){}
		int getSize(){return type->getSize() * size;}
		bool isP(primitive p){return p == ValueType::Array;}
		bool isT(ValueType *t)
		{
			if (t == NULL)
				return false;
			if (!t->isP(ValueType::Array))
				return false;
			if (getSize() != t->getSize())
				return false;
			return type->isT(t->get());
		}
		VType get(){return type;}
	private:
		VType type;
		int size;
	};
	struct Pointer : ValueType
	{
		Pointer(VType t):type(t){}
		int getSize(){return sizeof(void*);}
		bool isP(primitive p){return p == ValueType::Pointer;}
		bool isT(ValueType *t)
		{
			if (t == NULL)
				return false;
			if (!t->isP(ValueType::Pointer))
				return false;
			return type->isT(t->get());
		}
		VType get(){return type;}
	private:
		VType type;
	};
	struct FuncPtr : ValueType
	{
		FuncPtr(VType r) : ret(r){}
		int getSize(){return sizeof(void*);}
		void add(VType t){args.push_back(t);}
		bool isP(primitive p){return p == ValueType::Function;}
		bool isT(ValueType *t)
		{
			if (t == NULL)
				return false;
			if (!t->isP(ValueType::Function))
				return false;
			if (!ret->isT(t->get()))
				return false;
			if ((int)args.size() != t->getASize())
				return false;

			for (int i = 0; i < (int)args.size(); ++i)
			{
				if (!args[i]->isT(t->getA(i)))
					return false;
			}

			return true;
		}
		VType get(){return ret;}
		VType getA(int i){return args[i];}
		int getASize(){return (int)args.size();}
	private:
		vector<VType> args;
		VType ret;
	};

	struct Struct : ValueType
	{
		Struct() : size(0){}
		int getSize(){return size;}
		void add(const string &name, VType t)
		{
			member[name] = VarInfo();
			member[name].name = name;
			member[name].type = t;

			if (t->getSize() >= 4 &&  (size % 4 != 0))
			{
				size += 4 - size % 4;
			}
			member[name].address = size;
			size += t->getSize();
		}
		void end()
		{
			if (size % 4 != 0)
				size += 4 - size % 4;
		}
		VarInfo getMember(const string &name)
		{
			return member[name];
		}
		bool isP(primitive p){return p == ValueType::Struct;}
		bool isT(ValueType *t){return this == t;}
	private:
		std::map<string, VarInfo> member;
		int size;
	};
	struct Union : ValueType
	{
		Union() : size(0){}
		int getSize(){return size;}
		void add(const string &name, VType t)
		{
			member[name] = VarInfo();
			member[name].name = name;
			member[name].type = t;
			member[name].address = 0;
			if (size < t->getSize())
				size = t->getSize();
		}
		VarInfo getMember(const string &name)
		{
			return member[name];
		}
		bool isP(primitive p){return p == ValueType::Union;}
		bool isT(ValueType *t){return this == t;}
	private:
		std::map<string, VarInfo> member;
		int size;
	};
	struct Enum : ValueType
	{
		int getSize(){return 4;}
		void add(const string &name, int i)
		{
			member[name] = i;
		}
		VarInfo getMember(const string &name)
		{
			VarInfo v;
			v.address = member[name];
			return v;
		}
		bool isP(primitive p){return p == ValueType::Enum;}
		bool isT(ValueType *t){return this == t;}
	private:
		std::map<string, int> member;
	};
	struct ValueInfo
	{
		ValueInfo()
		{
			atype = no;
			address = 0;
		}
		ValueInfo(int a)
		{
			atype = temp;
			address = a;
		}
		ValueInfo(int a, VType t)
		{
			atype = temp;
			address = a;
			type = t;
		}
		enum address_type
		{
			no,
			local,
			temp,
			global,
			memory,
			function,
			NameSpace,
			TypeName,
		} atype;
		int address;
		VType type;
		bool isStack()	{return atype == local || atype == temp;}
		bool isGlobal()	{return atype == global;}
		bool isMemory()	{return atype == memory;}
	};

	class Environment;
	struct opcode
	{
		virtual ~opcode(){}
		virtual int getSize() = 0;
		virtual int run(Environment *env) = 0;
		virtual void gen(Environment *env) = 0;
	};

	class Environment
	{
	public:
		typedef std::map<string, VarInfo> var_table;
		typedef vector<shptr<opcode> > Code;
		class Function
		{
		public:
			Function(const string &s, VType r) : name(s), ret(r)
			{
				tag = 0;
				localstack = 0;
				tempstack = 0;
				argstack = 8;
				local.push_back(var_table());
				labels = 0;
				maxstack = 0;
			}
			void pushcode(opcode *c)			{code.push_back(c);}
			int getCurrentStack();
			ValueInfo getVariable(const string &name)
			{
				ValueInfo vi;
				if (local.back().count(name))
				{
					vi.atype = ValueInfo::local;
					vi.address = local.back()[name].address;
					vi.type = local.back()[name].type;
				}
				return vi;
			}
			void addLocal(const string &s, VType type)
			{
				local.back()[s].name = s;
				local.back()[s].type = type;
				int size = type->getSize();
				if ((size >= 4) && (localstack % 4 != 0))
				{
					localstack += 4 - localstack % 4;
					//localstack = ((localstack + 3) / 4) * 4;
				}
				localstack += size;
				local.back()[s].address = -localstack;
			}
			void addArg(const string &name, VType type)
			{
				local.back()[name].name = name;
				local.back()[name].type = type;
				int size = type->getSize();
				local.back()[name].address = argstack;
				argstack += size;
				argtype.push_back(type);
			}
			int getTemp()
			{
				if (localstack % 4 != 0)
					localstack += 4 - localstack % 4;
				tempstack += 4;
				int next = localstack + tempstack;
				return -next;
			}
			void newState()
			{
				if (maxstack < tempstack)
					maxstack = tempstack;
				tempstack = 0;
			}
			int getLabel()			{return labels++;}
			void addLabel(int l)	{label[l] = Label_(codesize(), code.size());}
			int Label(int l)		{return label[l].native;}
			int LabelIL(int l)		{return label[l].il;}
			int codesize()
			{
				int size = 1+2+6;
				for (Code::iterator it = code.begin(); it != code.end(); ++it)
					size += (*it)->getSize();
				return size;
				// 現状だとpushする度にsizeにも加算する方法の方がよいが
			}
			void pregen(Environment *env)
			{
				address = env->NCodes(codesize());
			}
			void gen(Environment *env)
			{
				env->EnterFunction(name);
				x86::push_ebp(env->Codes());
				x86::mov_ebp_esp(env->Codes());
				x86::add_esp_int(env->Codes(), -(localstack+maxstack));
				for (Code::iterator it = code.begin(); it != code.end(); ++it)
				{
					(*it)->gen(env);
				}
				env->LeaveFunction();
				env->writeNcode(address);
			}
			void setReturn()	{return_address = codesize();}
			int getReturn()		{return return_address;}
			int getCodePos(opcode *c)
			{
				int size = 1+2+6;
				for (Code::iterator it = code.begin(); it != code.end(); ++it)
				{
					if (it->get() == c)
						return size;
					size += (*it)->getSize();
				}
				return 0;
			}
			int getILPos(opcode *c)
			{
				for (Code::iterator it = code.begin(); it != code.end(); ++it)
				{
					if (it->get() == c)
						return it - code.begin();
				}
				return 0;
			}

			enum Run
			{
				End = 0,
				Start,
				Return,
				Call,
			};
			Run run(Environment *env, int start)
			{
				return env->runCode(code, start);
			}
			void call(Environment *env)
			{
				env->EnterFunction(name);
				env->r.enter(localstack+maxstack);
			}
			VType getFuncPtr()
			{
				FuncPtr *f = new FuncPtr(ret);
				VType t = f;
				for (vector<VType>::iterator it = argtype.begin(); it != argtype.end(); ++it)
					f->add(*it);
				return t;
			}
			VType getReturnType(){return ret;}
			int getAddress(){return address;}
		private:
			int tag;
			string name;
			Code code;
			vector<var_table> local;
			int localstack;
			int tempstack;
			int maxstack;
			int argstack;
			VType ret;
			vector<VType> argtype;
			int labels;
			int address;
			int return_address;
			struct Label_
			{
				Label_(){}
				Label_(int n, int i){native = n;il = i;}
				int native;
				int il;
			};
			std::map<int, Label_> label;
			// これ必ずgetLabelで順番に発行するならvectorでよくない？
		};
		Environment()
		{
			global = new NameSpace();
			globalsize = 0x1000;
			ns_context.push_back(global);
			native = new NativeData();
			native->global.resize(globalsize);
			errors = 0;
		}
		void err(const string &s)
		{
			errors++;
			std::printf("IL error: %s\n", s.c_str());
		}
		int getTemp()		{return function_context.back()->getTemp();}
		ValueInfo getVariable(const string &name)
		{
			ValueInfo vi = function_context.back()->getVariable(name);
			if (vi.address)
				return vi;

			for (vector<shptr<NameSpace> >::reverse_iterator it = ns_context.rbegin(); it != ns_context.rend(); ++it)
			{
				if ((*it)->global.count(name))
				{
					vi.atype = ValueInfo::global;
					vi.type = (*it)->global[name].type;
					vi.address = (*it)->global[name].address;
					return vi;
				}
				else if ((*it)->types.count(name))
				{
					vi.atype = ValueInfo::TypeName;
					vi.type = (*it)->types[name];
					return vi;
				}
				else if ((*it)->function.count(name))
				{
					vi.atype = ValueInfo::function;
					vi.type = (*it)->function[name]->getFuncPtr();
					vi.address = (int)((*it)->function[name].get());
					return vi;
				}
			}

			if (ns_context.back()->ns.count(name))
			{
				vi.atype = ValueInfo::NameSpace;
				// どう返す？何を返す？
				// ここで今すぐEnterNameSpace(name);する？
				// それはちょっとなー
				// 例によってenvにセットするか…
				vi.address = (int)(ns_context.back()->ns[name].get());
				last_ns = ns_context.back()->ns[name];
				return vi;
			}

			err(name + " is undefined");
			return vi;
		}
		private:struct NameSpace;public:
		shptr<NameSpace> last_ns;
		VType getType(const string &s)
		{
			for (vector<shptr<NameSpace> >::reverse_iterator it = ns_context.rbegin(); it != ns_context.rend(); ++it)
				if ((*it)->types.count(s)) return (*it)->types[s];
			return VType(NULL);
		}
		void addType(const string &name, VType type)
		{
			if (ns_context.back()->types.count(name))
			{
				err(name + " is already exists");
				return;
			}
			ns_context.back()->types[name] = type;
		}
		int addGlobal(const string &name, VType type, int val = 0, int address = -1)
		{
			if (ns_context.back()->global.count(name))
			{
				err(name + " is already exists");
				return 0;
			}
			VarInfo vi;
			vi.name = name;
			vi.type = type;
			if (address == -1)
			{
				address = globalsize;
				globalsize += type->getSize();
				native->global.resize(globalsize);
			}
			vi.address = address;
			ns_context.back()->global[name] = vi;
			native->global_address[name] = address;
			*(int*)Global(address) = val;
			return vi.address;
		}
		int addString(const string &s)
		{
			if (!string_table.count(s))
			{
				int o = globalsize;
				string_table[s] = o;
				globalsize += s.length() + 1;

				native->global.resize(globalsize);
				std::memcpy(&native->global[o], s.c_str(), s.length() + 1);
			}
			return string_table[s];
		}
		void EnterNameSpace(const string &s){ns_context.push_back(ns_context.back()->ns[s] = new NameSpace());}
		void LeaveNameSpace()				{ns_context.pop_back();}

		void EnterFunction(const string &s, VType r = NULL)
		{
			if (!ns_context.back()->function.count(s))ns_context.back()->function[s] = new Function(s, r);
			function_context.push_back(ns_context.back()->function[s]);
		}
		void LeaveFunction()							{function_context.pop_back();}
		void addLocal(const string &name, VType type)	{function_context.back()->addLocal(name, type);}
		void addArg(const string &name, VType type)		{function_context.back()->addArg(name, type);}
		void pushcode(IL::opcode *c)					{function_context.back()->pushcode(c);}
		int getCodePos(opcode *c)						{return function_context.back()->getCodePos(c);}
		int getILPos(opcode *c)							{return function_context.back()->getILPos(c);}
		void newState()									{function_context.back()->newState();}

		int getLabel()									{return function_context.back()->getLabel();}
		void addLabel(int label)						{function_context.back()->addLabel(label);}
		int Label(int label)							{return function_context.back()->Label(label);}
		int LabelIL(int label)							{return function_context.back()->LabelIL(label);}
		void setReturn()								{function_context.back()->setReturn();}
		int getReturn()									{return function_context.back()->getReturn();}
		VType getReturnType()							{return function_context.back()->getReturnType();}
		struct NativeData
		{
			typedef unsigned char byte;
			int global_base;
			int code_base;
			vector<byte> code;
			vector<byte> global;
			std::map<string, int> global_address;
			std::map<string, int> function_address;
			int *get(const string &name)
			{
				if (function_address.count(name))
				{
					return (int*)&code[function_address[name]];
				}
				else if (global_address.count(name))
				{
					return (int*)&global[global_address[name]];
				}
				return NULL;
			}
		};
		typedef shptr<NativeData> Native;
		Native gen(int code_base = 0, int global_base = 0)
		{
			if (!code_base)
			{
				native->code.reserve(getCodeSize());
				native->code.resize(1);
				native->code_base = (int)&native->code[0];
				native->code.resize(0);
			}
			else
			{
				native->code_base = code_base;
			}
			if (!global_base)
			{
				if (globalsize)
				{
					native->global.reserve(globalsize);
					native->global_base = (int)&native->global[0];
				}
			}
			else
			{
				native->global_base = global_base;
			}
			pregen_ns(global);
			gen_ns(global);
			if (errors)
			{
				std::printf("IL: %d errors occurred\n", errors);
				return NULL;
			}
			return native;
		}
		int getCodeSize()		{return codesize(global);}
		int GlobalBase(){return native->global_base;}
		int CodeBase(){return native->code_base;}
		vector<NativeData::byte> tempcode;
		vector<NativeData::byte> &Codes(){return tempcode;}
		int NCodes(int size)
		{
			int address = native->code.size();
			native->code.resize(address + size);
			return address;
		}
		void writeNcode(int address)
		{
			memcpy(&native->code[address], &tempcode[0], tempcode.size());
			tempcode.resize(0);
		}
		int run(int argc = 0, char **argv = 0)
		{
			r.stack.reserve(0x1000);
			r.stack.resize(0x100);
			runFunction("main");
			return r.ret;
		}
		int call(const string &name, int a1 = 0, int a2 = 0, int a3 = 0, int a4 = 0, int a5 = 0, int a6 = 0, int a7 = 0, int a8 = 0,
									int a9 = 0, int a10 = 0, int a11 = 0, int a12 = 0, int a13 = 0, int a14 = 0, int a15 = 0, int a16 = 0)
		{
			r.stack.reserve(0x1000);
			r.stack.resize(0x100);
			*(int*)&r.stack[0x100-4* 1] = a1;
			*(int*)&r.stack[0x100-4* 2] = a2;
			*(int*)&r.stack[0x100-4* 3] = a3;
			*(int*)&r.stack[0x100-4* 4] = a4;
			*(int*)&r.stack[0x100-4* 5] = a5;
			*(int*)&r.stack[0x100-4* 6] = a6;
			*(int*)&r.stack[0x100-4* 7] = a7;
			*(int*)&r.stack[0x100-4* 8] = a8;
			*(int*)&r.stack[0x100-4* 9] = a9;
			*(int*)&r.stack[0x100-4*10] = a10;
			*(int*)&r.stack[0x100-4*11] = a11;
			*(int*)&r.stack[0x100-4*12] = a12;
			*(int*)&r.stack[0x100-4*13] = a13;
			*(int*)&r.stack[0x100-4*14] = a14;
			*(int*)&r.stack[0x100-4*15] = a15;
			*(int*)&r.stack[0x100-4*16] = a16;
			runFunction(name);
			return r.ret;
		}
		int *getGlobal(const string &name){return (int*)&native->global[global->global[name].address];}
		Function::Run status;
		Function::Run runCode(Code &c, int start)
		{
			for (Code::iterator it = c.begin() + start; it != c.end(); ++it)
			{
				int j = (*it)->run(this);
				if (j)
					it += j;
				if (status == Function::Return)
				{
					break;
				}
				if (status == Function::Call)
				{
					pushLine(++it - c.begin());
					status = Function::Start;
					return Function::Call;
				}
			}
			status = Function::Start;
			LeaveFunction();
			r.leave();
			return Function::Return;
		}
		void runInit(const string &name)
		{
			r.stack.reserve(0x1000);
			r.stack.resize(0x100);
			runFunction(name);
			//*Global(ns_context.back()->global[name].address) = r.ret;
			//初期化用関数の最後にset_globalがあるのでいらない、というか、こうするならするで、set_returnが必要
		}
		void runFunction(const string &name)
		{
			ns_context.back()->function[name]->call(this);
			runContext();
		}
		void runContext()
		{
			int l = 0;
			pushLine(-1);
			while (l != -1)
			{
				Function::Run r = function_context.back()->run(this, l);
				if (r == Function::Return)
				{
					l = popLine();
				}
				else if (r == Function::Call)
				{
					l = 0;
				}
			}
		}
		vector<int> linestack;
		void pushLine(int l){linestack.push_back(l);}
		int popLine(){int l = linestack.back();linestack.pop_back();return l;}
		struct
		{
			int ret;
			vector<NativeData::byte> stack;
			vector<int> local_base;
			vector<int> arg_base;
			void enter(int size)
			{
				int s = stack.size() + 4;
				arg_base.push_back(s);
				stack.resize(s + size);
				local_base.push_back(s + size);
			}
			void leave()
			{
				stack.resize(arg_base.back() - 4);
				arg_base.pop_back();
				local_base.pop_back();
			}
			int *Stack(int s)
			{
				if (s < 0)
					return (int*)&stack[local_base.back() + s];
				else
					return (int*)&stack[arg_base.back() - s];
			}
			int &StackTop(int s = 0){return *(int*)&stack[stack.size() - s*4];}
			void Push(int i)		{stack.resize(stack.size() + 4);StackTop(1) = i;}
			void Pop(int i)			{stack.resize(stack.size() - i);}
		} r;
		int *Global(int a)		{return (int*)&native->global[a];}
	private:
		int errors;
		typedef std::map<string, shptr<Function> > Funcs;
		int globalsize;
		std::map<string, int> string_table;

		struct NameSpace
		{
			var_table global;
			std::map<string, VType> types;
			Funcs function;
			std::map<string, shptr<NameSpace> > ns;
			ValueInfo get(Environment *env, const string &name)
			{
				ValueInfo vi;
				if (global.count(name))
				{
					vi.atype = ValueInfo::global;
					vi.type = global[name].type;
					vi.address = global[name].address;
					return vi;
				}
				else if (types.count(name))
				{
					vi.atype = ValueInfo::TypeName;
					vi.type = types[name];
					return vi;
				}
				else if (function.count(name))
				{
					vi.atype = ValueInfo::function;
					vi.type = function[name]->getFuncPtr();
					vi.address = (int)(function[name].get());
					return vi;
				}
				else if (ns.count(name))
				{
					vi.atype = ValueInfo::NameSpace;
					vi.address = (int)(ns[name].get());
					env->last_ns = ns[name];
					return vi;
				}
				else
				{
					env->err(name + " is not find");
				}
				return vi;
			}
		};
		shptr<NameSpace> global;
		vector<shptr<NameSpace> > ns_context;

		int codesize(shptr<NameSpace> ns)
		{
			int size = 0;
			for (Funcs::iterator it = ns->function.begin(); it != ns->function.end(); ++it)
			{
				size += it->second->codesize();
			}

			for (std::map<string, shptr<NameSpace> >::iterator it = ns->ns.begin(); it != ns->ns.end(); ++it)
			{
				size += codesize(it->second);
			}
			return size;
		}
		void pregen_ns(shptr<NameSpace> ns)
		{
			for (Funcs::iterator it = ns->function.begin(); it != ns->function.end(); ++it)
			{
				it->second->pregen(this);
				native->function_address[it->first] = it->second->getAddress();
				// namespaceが絡むとどうなるかというのはあるが
			}

			for (std::map<string, shptr<NameSpace> >::iterator it = ns->ns.begin(); it != ns->ns.end(); ++it)
			{
				pregen_ns(it->second);
			}
		}
		void gen_ns(shptr<NameSpace> ns)
		{
			for (Funcs::iterator it = ns->function.begin(); it != ns->function.end(); ++it)
			{
				it->second->gen(this);
			}

			for (std::map<string, shptr<NameSpace> >::iterator it = ns->ns.begin(); it != ns->ns.end(); ++it)
			{
				gen_ns(it->second);
			}
		}

		vector<shptr<Function> > function_context;

		Native native;
	};
	typedef Environment::Function Function;

	struct binary : opcode
	{
		binary(int t, int a) : to(t), address(a){}
	protected:
		int to;
		int address;
	};
	struct getFunction : opcode
	{
		getFunction(int t, Function *f)
		{
			to = t;
			func = f;
		}
		int getSize(){return 10;}
		void gen(Environment *env)
		{
			x86::mov_stack_int(env->Codes(), to, env->CodeBase() + func->getAddress());
		}
		int run(Environment *env)
		{
			*env->r.Stack(to) = (int)func;
			return 0;
		}
	private:
		int to;
		Function *func;
	};
	struct getGlobal : binary
	{
		getGlobal(int t, int a) : binary(t, a){}
		int getSize(){return 11;}
		void gen(Environment *env)
		{
			x86::mov_eax_mem(env->Codes(), env->GlobalBase() + address);
			x86::mov_stack_eax(env->Codes(), to);
		}
		int run(Environment *env)
		{
			*env->r.Stack(to) = *env->Global(address);
			return 0;
		}
	};
	struct getMemory : binary
	{
		getMemory(int t, int a) : binary(t, a){}
		int getSize(){return 14;}
		void gen(Environment *env)
		{
			x86::mov_ecx_stack(env->Codes(), address);
			x86::mov_eax_ecx(env->Codes());
			x86::mov_stack_eax(env->Codes(), to);
		}
		int run(Environment *env)
		{
			*env->r.Stack(to) = *(int*)(*env->r.Stack(address));
			return 0;
		}
	};
	struct getGlobalPtr : binary
	{
		getGlobalPtr(int t, int a) : binary(t, a){}
		int getSize(){return 12;}
		void gen(Environment *env)
		{
			x86::lea_eax_mem(env->Codes(), env->GlobalBase() + address);
			x86::mov_stack_eax(env->Codes(), to);
		}
		int run(Environment *env)
		{
			*env->r.Stack(to) = (int)env->Global(address);
			return 0;
		}
	};
	struct getLocalPtr : binary
	{
		getLocalPtr(int t, int a) : binary(t, a){}
		int getSize(){return 12;}
		void gen(Environment *env)
		{
			x86::lea_eax_stack(env->Codes(), address);
			x86::mov_stack_eax(env->Codes(), to);
		}
		int run(Environment *env)
		{
			*env->r.Stack(to) = (int)env->r.Stack(address);
			return 0;
		}
	};

	struct getInt : opcode
	{
		getInt(int t, int i) : to(t), x(i){}
		int getSize(){return 10;}
		void gen(Environment *env)
		{
			x86::mov_stack_int(env->Codes(), to, x);
		}
		int run(Environment *env)
		{
			*env->r.Stack(to) = x;
			return 0;
		}
	private:
		int to;
		int x;
	};
	struct getChar : opcode
	{
		getChar(int t, char c) : to(t), x(c){}
		int getSize(){return 7;}
		void gen(Environment *env)
		{
			x86::mov_stack_char(env->Codes(), to, x);
		}
		int run(Environment *env)
		{
			*(char*)env->r.Stack(to) = x;
			return 0;
		}
	private:
		int to;
		char x;
	};
	struct getFloat : opcode
	{
		getFloat(int t, float f) : to(t), x(f){}
		int getSize(){return 10;}
		void gen(Environment *env)
		{
			x86::mov_stack_int(env->Codes(), to, *(int*)&x);
		}
		int run(Environment *env)
		{
			*(float*)env->r.Stack(to) = x;
			return 0;
		}
	private:
		int to;
		float x;
	};

	struct unary : opcode
	{
		unary(int t) : to(t){}
	protected:
		int to;
	};
	struct incL : unary
	{
		incL(int t) : unary(t){}
		int getSize(){return 6;}
		void gen(Environment *env)
		{
			x86::inc_stack(env->Codes(), to);
		}
		int run(Environment *env)
		{
			++*env->r.Stack(to);
			return 0;
		}
	};
	struct incG : unary
	{
		incG(int t) : unary(t){}
		int getSize(){return 6;}
		void gen(Environment *env)
		{
			x86::inc_mem(env->Codes(), env->GlobalBase() + to);
		}
		int run(Environment *env)
		{
			++*env->Global(to);
			return 0;
		}
	};
	struct incM : unary
	{
		incM(int t) : unary(t){}
		int getSize(){return 8;}
		void gen(Environment *env)
		{
			x86::mov_ecx_stack(env->Codes(), to);
			x86::inc_ecx(env->Codes());
		}
		int run(Environment *env)
		{
			++*(int*)(*env->r.Stack(to));
			return 0;
		}
	};
	struct cincL : unary
	{
		cincL(int t) : unary(t){}
		int getSize(){return 6;}
		void gen(Environment *env)
		{
			x86::inc_byte_stack(env->Codes(), to);
		}
		int run(Environment *env)
		{
			++*(char*)env->r.Stack(to);
			return 0;
		}
	};
	struct cincG : unary
	{
		cincG(int t) : unary(t){}
		int getSize(){return 6;}
		void gen(Environment *env)
		{
			x86::inc_byte_mem(env->Codes(), env->GlobalBase() + to);
		}
		int run(Environment *env)
		{
			++*(char*)env->Global(to);
			return 0;
		}
	};
	struct cincM : unary
	{
		cincM(int t) : unary(t){}
		int getSize(){return 8;}
		void gen(Environment *env)
		{
			x86::mov_ecx_stack(env->Codes(), to);
			x86::inc_byte_ecx(env->Codes());
		}
		int run(Environment *env)
		{
			++*(char*)(*env->r.Stack(to));
			return 0;
		}
	};
	struct pincL : opcode
	{
		pincL(int t, int s){to = t;size = s;}
		int getSize(){return 10;}
		void gen(Environment *env)
		{
			x86::add_stack_int(env->Codes(), to, size);
		}
		int run(Environment *env)
		{
			*env->r.Stack(to) += size;
			return 0;
		}
	private:
		int to;
		int size;
	};
	struct pincG : opcode
	{
		pincG(int t, int s){to = t;size = s;}
		int getSize(){return 10;}
		void gen(Environment *env)
		{
			x86::add_mem_int(env->Codes(), env->GlobalBase() + to, size);
		}
		int run(Environment *env)
		{
			*env->Global(to) += size;
			return 0;
		}
	private:
		int to;
		int size;
	};
	struct pincM : opcode
	{
		pincM(int t, int s){to = t;size = s;}
		int getSize(){return 12;}
		void gen(Environment *env)
		{
			x86::mov_ecx_stack(env->Codes(), to);
			x86::add_ecx_int(env->Codes(), size);
		}
		int run(Environment *env)
		{
			*(int*)(*env->r.Stack(to)) += size;
			return 0;
		}
	private:
		int to;
		int size;
	};

	struct decL : unary
	{
		decL(int t) : unary(t){}
		int getSize(){return 6;}
		void gen(Environment *env)
		{
			x86::dec_stack(env->Codes(), to);
		}
		int run(Environment *env)
		{
			--*env->r.Stack(to);
			return 0;
		}
	};
	struct decG : unary
	{
		decG(int t) : unary(t){}
		int getSize(){return 6;}
		void gen(Environment *env)
		{
			x86::dec_mem(env->Codes(), env->GlobalBase() + to);
		}
		int run(Environment *env)
		{
			--*env->Global(to);
			return 0;
		}
	};
	struct decM : unary
	{
		decM(int t) : unary(t){}
		int getSize(){return 8;}
		void gen(Environment *env)
		{
			x86::mov_ecx_stack(env->Codes(), to);
			x86::dec_ecx(env->Codes());
		}
		int run(Environment *env)
		{
			--*(int*)(*env->r.Stack(to));
			return 0;
		}
	};
	struct cdecL : unary
	{
		cdecL(int t) : unary(t){}
		int getSize(){return 6;}
		void gen(Environment *env)
		{
			x86::dec_byte_stack(env->Codes(), to);
		}
		int run(Environment *env)
		{
			--*(char*)env->r.Stack(to);
			return 0;
		}
	};
	struct cdecG : unary
	{
		cdecG(int t) : unary(t){}
		int getSize(){return 6;}
		void gen(Environment *env)
		{
			x86::dec_byte_mem(env->Codes(), env->GlobalBase() + to);
		}
		int run(Environment *env)
		{
			--*(char*)env->Global(to);
			return 0;
		}
	};
	struct cdecM : unary
	{
		cdecM(int t) : unary(t){}
		int getSize(){return 8;}
		void gen(Environment *env)
		{
			x86::mov_ecx_stack(env->Codes(), to);
			x86::dec_byte_ecx(env->Codes());
		}
		int run(Environment *env)
		{
			--*(char*)(*env->r.Stack(to));
			return 0;
		}
	};
	struct pdecL : opcode
	{
		pdecL(int t, int s){to = t;size = s;}
		int getSize(){return 10;}
		void gen(Environment *env)
		{
			x86::add_stack_int(env->Codes(), to, -size);
			// これ中間言語自体を用意する必要がなくね？
			// pincL等に最初からマイナスで渡せば済む話
			// まず別の命令名にすべきな気がするが
		}
		int run(Environment *env)
		{
			*env->r.Stack(to) -= size;
			return 0;
		}
	private:
		int to;
		int size;
	};
	struct pdecG : opcode
	{
		pdecG(int t, int s){to = t;size = s;}
		int getSize(){return 10;}
		void gen(Environment *env)
		{
			x86::add_mem_int(env->Codes(), env->GlobalBase() + to, -size);
		}
		int run(Environment *env)
		{
			*env->Global(to) -= size;
			return 0;
		}
	private:
		int to;
		int size;
	};
	struct pdecM : opcode
	{
		pdecM(int t, int s){to = t;size = s;}
		int getSize(){return 12;}
		void gen(Environment *env)
		{
			x86::mov_ecx_stack(env->Codes(), to);
			x86::add_ecx_int(env->Codes(), -size);
		}
		int run(Environment *env)
		{
			*(int*)(*env->r.Stack(to)) -= size;
			return 0;
		}
	private:
		int to;
		int size;
	};

	struct minus : binary
	{
		minus(int t, int a) : binary(t, a){}
		int getSize(){return 14;}
		void gen(Environment *env)
		{
			x86::mov_eax_stack(env->Codes(), address);
			x86::neg_eax(env->Codes());
			x86::mov_stack_eax(env->Codes(), to);
		}
		int run(Environment *env)
		{
			*env->r.Stack(to) = -*env->r.Stack(address);
			return 0;
		}
	};
	struct fminus : binary
	{
		fminus(int t, int a) : binary(t, a){}
		int getSize(){return 0;}
		void gen(Environment *env)
		{
		}
		int run(Environment *env)
		{
			*(float*)env->r.Stack(to) = -*(float*)env->r.Stack(address);
			return 0;
		}
	};
	struct Not : binary
	{
		Not(int t, int a) : binary(t, a){}
		int getSize(){return 15;}
		void gen(Environment *env)
		{
			x86::mov_eax_stack(env->Codes(), address);
			x86::xor_eax_1(env->Codes());
			x86::mov_stack_eax(env->Codes(), to);
		}
		int run(Environment *env)
		{
			*env->r.Stack(to) = !*env->r.Stack(address);
			return 0;
		}
	};
	struct Compl : binary
	{
		Compl(int t, int a) : binary(t, a){}
		int getSize(){return 14;}
		void gen(Environment *env)
		{
			x86::mov_eax_stack(env->Codes(), address);
			x86::not_eax(env->Codes());
			x86::mov_stack_eax(env->Codes(), to);
		}
		int run(Environment *env)
		{
			*(dword*)env->r.Stack(to) = ~*(dword*)env->r.Stack(address);
			return 0;
		}
	};

	struct ternary : opcode
	{
		ternary(int t, int l, int r)
		{
			to = t;
			left = l;
			right = r;
		}
		int getSize(){return 18;}
		void gen(Environment *env)
		{
			x86::mov_eax_stack(env->Codes(), left);
			x86::mov_ecx_stack(env->Codes(), right);
			gen_calc(env);
			x86::mov_stack_eax(env->Codes(), to);
		}
		virtual void gen_calc(Environment *env){}
	protected:
		int to;
		int left;
		int right;
	};
	struct iadd : ternary
	{
		iadd(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 20;}
		void gen_calc(Environment *env)
		{
			x86::add_eax_ecx(env->Codes());
		}
		int run(Environment *env)
		{
			*env->r.Stack(to) = *env->r.Stack(left) + *env->r.Stack(right);
			return 0;
		}
	};
	struct fadd : ternary
	{
		fadd(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 18;}
		void gen(Environment *env)
		{
			x86::fld_stack(env->Codes(), left);
			x86::fadd_stack(env->Codes(), right);
			x86::fstp_stack(env->Codes(), to);
		}
		int run(Environment *env)
		{
			*(float*)env->r.Stack(to) = *(float*)env->r.Stack(left) + *(float*)env->r.Stack(right);
			return 0;
		}
	};
	struct isub : ternary
	{
		isub(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 20;}
		void gen_calc(Environment *env)
		{
			x86::sub_eax_ecx(env->Codes());
		}
		int run(Environment *env)
		{
			*env->r.Stack(to) = *env->r.Stack(left) - *env->r.Stack(right);
			return 0;
		}
	};
	struct fsub : ternary
	{
		fsub(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 18;}
		void gen(Environment *env)
		{
			x86::fld_stack(env->Codes(), left);
			x86::fsub_stack(env->Codes(), right);
			x86::fstp_stack(env->Codes(), to);
		}
		int run(Environment *env)
		{
			*(float*)env->r.Stack(to) = *(float*)env->r.Stack(left) - *(float*)env->r.Stack(right);
			return 0;
		}
	};
	struct imul : ternary
	{
		imul(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 20;}
		void gen_calc(Environment *env)
		{
			x86::mul_ecx(env->Codes());
		}
		int run(Environment *env)
		{
			*env->r.Stack(to) = *env->r.Stack(left) * *env->r.Stack(right);
			return 0;
		}
	};
	struct fmul : ternary
	{
		fmul(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 18;}
		void gen(Environment *env)
		{
			x86::fld_stack(env->Codes(), left);
			x86::fmul_stack(env->Codes(), right);
			x86::fstp_stack(env->Codes(), to);
		}
		int run(Environment *env)
		{
			*(float*)env->r.Stack(to) = *(float*)env->r.Stack(left) * *(float*)env->r.Stack(right);
			return 0;
		}
	};
	struct idiv : ternary
	{
		idiv(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 22;}
		void gen_calc(Environment *env)
		{
			x86::xor_edx_edx(env->Codes());
			x86::div_ecx(env->Codes());
		}
		int run(Environment *env)
		{
			*env->r.Stack(to) = *env->r.Stack(left) / *env->r.Stack(right);
			return 0;
		}
	};
	struct fdiv : ternary
	{
		fdiv(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 18;}
		void gen(Environment *env)
		{
			x86::fld_stack(env->Codes(), left);
			x86::fdiv_stack(env->Codes(), right);
			x86::fstp_stack(env->Codes(), to);
		}
		int run(Environment *env)
		{
			*(float*)env->r.Stack(to) = *(float*)env->r.Stack(left) / *(float*)env->r.Stack(right);
			return 0;
		}
	};
	struct imod : ternary
	{
		imod(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 22;}
		void gen(Environment *env)
		{
			x86::mov_eax_stack(env->Codes(), left);
			x86::mov_ecx_stack(env->Codes(), right);
			x86::xor_edx_edx(env->Codes());
			x86::div_ecx(env->Codes());
			x86::mov_stack_edx(env->Codes(), to);
		}
		int run(Environment *env)
		{
			*env->r.Stack(to) = *env->r.Stack(left) % *env->r.Stack(right);
			return 0;
		}
	};
	struct ishl : ternary
	{
		ishl(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 20;}
		void gen_calc(Environment *env)
		{
			x86::shl_eax_ecx(env->Codes());
		}
		int run(Environment *env)
		{
			*env->r.Stack(to) = *env->r.Stack(left) << *env->r.Stack(right);
			return 0;
		}
	};
	struct ishr : ternary
	{
		ishr(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 20;}
		void gen_calc(Environment *env)
		{
			x86::sar_eax_ecx(env->Codes());
		}
		int run(Environment *env)
		{
			*env->r.Stack(to) = *env->r.Stack(left) >> *env->r.Stack(right);
			return 0;
		}
	};
	struct ushr : ternary
	{
		ushr(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 20;}
		void gen_calc(Environment *env)
		{
			x86::shr_eax_ecx(env->Codes());
		}
		int run(Environment *env)
		{
			*(dword*)env->r.Stack(to) = *(dword*)env->r.Stack(left) >> *(dword*)env->r.Stack(right);
			return 0;
		}
	};
	struct iand : ternary
	{
		iand(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 20;}
		void gen_calc(Environment *env)
		{
			x86::and_eax_ecx(env->Codes());
		}
		int run(Environment *env)
		{
			*(dword*)env->r.Stack(to) = *(dword*)env->r.Stack(left) & *(dword*)env->r.Stack(right);
			return 0;
		}
	};
	struct ior : ternary
	{
		ior(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 20;}
		void gen_calc(Environment *env)
		{
			x86::or_eax_ecx(env->Codes());
		}
		int run(Environment *env)
		{
			*(dword*)env->r.Stack(to) = *(dword*)env->r.Stack(left) | *(dword*)env->r.Stack(right);
			return 0;
		}
	};
	struct ixor : ternary
	{
		ixor(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 20;}
		void gen_calc(Environment *env)
		{
			x86::xor_eax_ecx(env->Codes());
		}
		int run(Environment *env)
		{
			*(dword*)env->r.Stack(to) = *(dword*)env->r.Stack(left) ^ *(dword*)env->r.Stack(right);
			return 0;
		}
	};
	struct ilt : ternary
	{
		ilt(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 32;}
		void gen_calc(Environment *env)
		{
			x86::xor_edx_edx(env->Codes());
			x86::cmp_eax_ecx(env->Codes());
			x86::jl3(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::mov_eax_edx(env->Codes());
		}
		int run(Environment *env)
		{
			*env->r.Stack(to) = *env->r.Stack(left) < *env->r.Stack(right);
			return 0;
		}
	};
	struct ult : ternary
	{
		ult(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 32;}
		void gen_calc(Environment *env)
		{
			x86::xor_edx_edx(env->Codes());
			x86::cmp_eax_ecx(env->Codes());
			x86::jb3(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::mov_eax_edx(env->Codes());
		}
		int run(Environment *env)
		{
			*(dword*)env->r.Stack(to) = *(dword*)env->r.Stack(left) < *(dword*)env->r.Stack(right);
			return 0;
		}
	};
	struct clt : ternary
	{
		clt(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 32;}
		void gen_calc(Environment *env)
		{
			x86::xor_edx_edx(env->Codes());
			x86::cmp_al_cl(env->Codes());
			x86::jb3(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::mov_eax_edx(env->Codes());
		}
		int run(Environment *env)
		{
			*(char*)env->r.Stack(to) = *(char*)env->r.Stack(left) < *(char*)env->r.Stack(right);
			return 0;
		}
	};
	struct ile : ternary
	{
		ile(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 32;}
		void gen_calc(Environment *env)
		{
			x86::xor_edx_edx(env->Codes());
			x86::cmp_eax_ecx(env->Codes());
			x86::jle3(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::mov_eax_edx(env->Codes());
		}
		int run(Environment *env)
		{
			*env->r.Stack(to) = *env->r.Stack(left) <= *env->r.Stack(right);
			return 0;
		}
	};
	struct ule : ternary
	{
		ule(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 32;}
		void gen_calc(Environment *env)
		{
			x86::xor_edx_edx(env->Codes());
			x86::cmp_eax_ecx(env->Codes());
			x86::jbe3(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::mov_eax_edx(env->Codes());
		}
		int run(Environment *env)
		{
			*(dword*)env->r.Stack(to) = *(dword*)env->r.Stack(left) <= *(dword*)env->r.Stack(right);
			return 0;
		}
	};
	struct cle : ternary
	{
		cle(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 32;}
		void gen_calc(Environment *env)
		{
			x86::xor_edx_edx(env->Codes());
			x86::cmp_al_cl(env->Codes());
			x86::jbe3(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::mov_eax_edx(env->Codes());
		}
		int run(Environment *env)
		{
			*(char*)env->r.Stack(to) = *(char*)env->r.Stack(left) <= *(char*)env->r.Stack(right);
			return 0;
		}
	};
	struct igt : ternary
	{
		igt(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 32;}
		void gen_calc(Environment *env)
		{
			x86::xor_edx_edx(env->Codes());
			x86::cmp_eax_ecx(env->Codes());
			x86::jg3(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::mov_eax_edx(env->Codes());
		}
		int run(Environment *env)
		{
			*env->r.Stack(to) = *env->r.Stack(left) > *env->r.Stack(right);
			return 0;
		}
	};
	struct ugt : ternary
	{
		ugt(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 32;}
		void gen_calc(Environment *env)
		{
			x86::xor_edx_edx(env->Codes());
			x86::cmp_eax_ecx(env->Codes());
			x86::ja3(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::mov_eax_edx(env->Codes());
		}
		int run(Environment *env)
		{
			*(dword*)env->r.Stack(to) = *(dword*)env->r.Stack(left) > *(dword*)env->r.Stack(right);
			return 0;
		}
	};
	struct cgt : ternary
	{
		cgt(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 32;}
		void gen_calc(Environment *env)
		{
			x86::xor_edx_edx(env->Codes());
			x86::cmp_al_cl(env->Codes());
			x86::ja3(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::mov_eax_edx(env->Codes());
		}
		int run(Environment *env)
		{
			*(char*)env->r.Stack(to) = *(char*)env->r.Stack(left) > *(char*)env->r.Stack(right);
			return 0;
		}
	};
	struct ige : ternary
	{
		ige(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 32;}
		void gen_calc(Environment *env)
		{
			x86::xor_edx_edx(env->Codes());
			x86::cmp_eax_ecx(env->Codes());
			x86::jge3(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::mov_eax_edx(env->Codes());
		}
		int run(Environment *env)
		{
			*env->r.Stack(to) = *env->r.Stack(left) >= *env->r.Stack(right);
			return 0;
		}
	};
	struct uge : ternary
	{
		uge(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 32;}
		void gen_calc(Environment *env)
		{
			x86::xor_edx_edx(env->Codes());
			x86::cmp_eax_ecx(env->Codes());
			x86::jae3(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::mov_eax_edx(env->Codes());
		}
		int run(Environment *env)
		{
			*(dword*)env->r.Stack(to) = *(dword*)env->r.Stack(left) >= *(dword*)env->r.Stack(right);
			return 0;
		}
	};
	struct cge : ternary
	{
		cge(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 32;}
		void gen_calc(Environment *env)
		{
			x86::xor_edx_edx(env->Codes());
			x86::cmp_al_cl(env->Codes());
			x86::jae3(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::mov_eax_edx(env->Codes());
		}
		int run(Environment *env)
		{
			*(char*)env->r.Stack(to) = *(char*)env->r.Stack(left) >= *(char*)env->r.Stack(right);
			return 0;
		}
	};
	struct ieq : ternary
	{
		ieq(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 32;}
		void gen_calc(Environment *env)
		{
			x86::xor_edx_edx(env->Codes());
			x86::cmp_eax_ecx(env->Codes());
			x86::je3(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::mov_eax_edx(env->Codes());
		}
		int run(Environment *env)
		{
			*env->r.Stack(to) = *env->r.Stack(left) == *env->r.Stack(right);
			return 0;
		}
	};
	struct ceq : ternary
	{
		ceq(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 32;}
		void gen_calc(Environment *env)
		{
			x86::xor_edx_edx(env->Codes());
			x86::cmp_al_cl(env->Codes());
			x86::je3(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::mov_eax_edx(env->Codes());
		}
		int run(Environment *env)
		{
			*(char*)env->r.Stack(to) = *(char*)env->r.Stack(left) == *(char*)env->r.Stack(right);
			return 0;
		}
	};
	struct ine : ternary
	{
		ine(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 32;}
		void gen_calc(Environment *env)
		{
			x86::xor_edx_edx(env->Codes());
			x86::cmp_eax_ecx(env->Codes());
			x86::jnz3(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::mov_eax_edx(env->Codes());
		}
		int run(Environment *env)
		{
			*env->r.Stack(to) = *env->r.Stack(left) != *env->r.Stack(right);
			return 0;
		}
	};
	struct cne : ternary
	{
		cne(int t, int l, int r) : ternary(t, l, r){}
		int getSize(){return 32;}
		void gen_calc(Environment *env)
		{
			x86::xor_edx_edx(env->Codes());
			x86::cmp_al_cl(env->Codes());
			x86::jnz3(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::xor_edx_1(env->Codes());
			x86::mov_eax_edx(env->Codes());
		}
		int run(Environment *env)
		{
			*(char*)env->r.Stack(to) = *(char*)env->r.Stack(left) != *(char*)env->r.Stack(right);
			return 0;
		}
	};

	struct assign : opcode
	{
		assign(int l, int r)
		{
			left = l;
			right = r;
		}
		int getSize(){return 12;}
		void gen(Environment *env)
		{
			x86::mov_eax_stack(env->Codes(), right);
			x86::mov_stack_eax(env->Codes(), left);
		}
		int run(Environment *env)
		{
			*env->r.Stack(left) = *env->r.Stack(right);
			return 0;
		}
	protected:
		int left;
		int right;
	};
	struct cassign : assign
	{
		cassign(int l, int r) : assign(l, r){}
		int getSize(){return 12;}
		void gen(Environment *env)
		{
			x86::mov_al_stack(env->Codes(), right);
			x86::mov_stack_al(env->Codes(), left);
		}
		int run(Environment *env)
		{
			*(char*)env->r.Stack(left) = *(char*)env->r.Stack(right);
			return 0;
		}
	};

	struct set_global : assign
	{
		set_global(int l, int r) : assign(l, r){}
		int getSize(){return 11;}
		void gen(Environment *env)
		{
			x86::mov_eax_stack(env->Codes(), right);
			x86::mov_mem_eax(env->Codes(), env->GlobalBase() + left);
		}
		int run(Environment *env)
		{
			*env->Global(left) = *env->r.Stack(right);
			return 0;
		}
	};
	struct cset_global : assign
	{
		cset_global(int l, int r) : assign(l, r){}
		int getSize(){return 11;}
		void gen(Environment *env)
		{
			x86::mov_al_stack(env->Codes(), right);
			x86::mov_mem_al(env->Codes(), env->GlobalBase() + left);
		}
		int run(Environment *env)
		{
			*(char*)env->Global(left) = *(char*)env->r.Stack(right);
			return 0;
		}
	};

	struct set_memory : assign
	{
		set_memory(int l, int r) : assign(l, r){}
		int getSize(){return 14;}
		void gen(Environment *env)
		{
			x86::mov_eax_stack(env->Codes(), right);
			x86::mov_ecx_stack(env->Codes(), left);
			x86::mov_ecx_eax(env->Codes());
		}
		int run(Environment *env)
		{
			*(int*)(*env->r.Stack(left)) = *env->r.Stack(right);
			return 0;
		}
	};
	struct cset_memory : assign
	{
		cset_memory(int l, int r) : assign(l, r){}
		int getSize(){return 14;}
		void gen(Environment *env)
		{
			x86::mov_eax_stack(env->Codes(), right);
			x86::mov_ecx_stack(env->Codes(), left);
			x86::mov_ecx_al(env->Codes());
		}
		int run(Environment *env)
		{
			*(char*)(*env->r.Stack(left)) = *(char*)env->r.Stack(right);
			return 0;
		}
	};

	struct set_return : opcode
	{
		set_return(int i){r = i;}
		int getSize(){return 6;}
		void gen(Environment *env)
		{
			x86::mov_eax_stack(env->Codes(), r);
		}
		int run(Environment *env)
		{
			env->r.ret = *env->r.Stack(r);
			return 0;
		}
	private:
		int r;
	};
	struct Return : opcode
	{
		int getSize(){return 5;}
		void gen(Environment *env)
		{
			int pos = env->getCodePos(this) + getSize();
			int j = env->getReturn() - pos;
			x86::jmp(env->Codes(), j);
		}
		int run(Environment *env)
		{
			env->status = Function::Return;
			return 0;
		}
	};

	struct push : opcode
	{
		push(int s) : stack(s){}
		int getSize(){return 6;}
		void gen(Environment *env)
		{
			x86::push_stack(env->Codes(), stack);
		}
		int run(Environment *env)
		{
			env->r.Push(*env->r.Stack(stack));
			return 0;
		}
	private:
		int stack;
	};
	struct call : opcode
	{
		call(int t, int f)
		{
			to = t;
			func = f;
		}
		int getSize(){return 8;}
		void gen(Environment *env)
		{
			x86::mov_eax_stack(env->Codes(), func);
			x86::call_eax(env->Codes());
		}
		int run(Environment *env)
		{
			int *p = (int*)*(int*)(env->r.Stack(func));
			if (*p)
			{
				typedef int (*fun)(int,int,int,int,int,int,int,int,int,int,int,int,int,int,int,int);
				fun f = (fun)p;
				*env->r.Stack(to) = f(env->r.StackTop(1),env->r.StackTop(2),env->r.StackTop(3),env->r.StackTop(4),
					env->r.StackTop(5),env->r.StackTop(6),env->r.StackTop(7),env->r.StackTop(8),
					env->r.StackTop(9),env->r.StackTop(10),env->r.StackTop(11),env->r.StackTop(12),
					env->r.StackTop(13),env->r.StackTop(14),env->r.StackTop(15),env->r.StackTop(16)
				);
			}
			else
			{
				Function *f = (Function*)p;
				f->call(env);
				env->status = Environment::Function::Call;
			}
			return 0;
		}
	private:
		int func;
		int to;
	};
	struct pop_arg : opcode
	{
		pop_arg(int a) : argsize(a){}
		int getSize(){return 6;}
		void gen(Environment *env)
		{
			x86::add_esp_int(env->Codes(), argsize);
		}
		int run(Environment *env)
		{
			env->r.Pop(argsize);
			return 0;
		}
	private:
		int argsize;
	};
	struct get_return : opcode
	{
		get_return(int t) : to(t){}
		int getSize(){return 6;}
		void gen(Environment *env)
		{
			x86::mov_stack_eax(env->Codes(), to);
		}
		int run(Environment *env)
		{
			*env->r.Stack(to) = env->r.ret;
			return 0;
		}
	private:
		int to;
	};
	struct jump_true : opcode
	{
		jump_true(int s, int l){stack = s;label = l;}
		int getSize(){return 14;}
		void gen(Environment *env)
		{
			x86::mov_eax_stack(env->Codes(), stack);
			x86::test_al_al(env->Codes());
			int l = env->Label(label);
			int pos = env->getCodePos(this) + getSize();
			int j = l - pos;
			x86::jnz(env->Codes(), j);
		}
		int run(Environment *env)
		{
			if (*env->r.Stack(stack))
			{
				int l = env->LabelIL(label);
				int pos = env->getILPos(this) + 1;
				int j = l - pos;
				return j;
			}
			return 0;
		}
	private:
		int stack;
		int label;
	};
	struct jump_false : opcode
	{
		jump_false(int s, int l){stack = s;label = l;}
		int getSize(){return 14;}
		void gen(Environment *env)
		{
			x86::mov_eax_stack(env->Codes(), stack);
			x86::test_al_al(env->Codes());
			int l = env->Label(label);
			int pos = env->getCodePos(this) + getSize();
			int j = l - pos;
			x86::je(env->Codes(), j);
		}
		int run(Environment *env)
		{
			if (!*env->r.Stack(stack))
			{
				int l = env->LabelIL(label);
				int pos = env->getILPos(this) + 1;
				int j = l - pos;
				return j;
			}
			return 0;
		}
	private:
		int stack;
		int label;
	};
	struct jump : opcode
	{
		jump(int l){label = l;}
		int getSize(){return 5;}
		void gen(Environment *env)
		{
			int l = env->Label(label);
			int pos = env->getCodePos(this) + getSize();
			int j = l - pos;
			x86::jmp(env->Codes(), j);
		}
		int run(Environment *env)
		{
			int l = env->LabelIL(label);
			int pos = env->getILPos(this) + 1;
			int j = l - pos;
			return j;
		}
	private:
		int label;
	};
	struct end : opcode
	{
		int getSize(){return 4;}
		void gen(Environment *env)
		{
			x86::mov_esp_ebp(env->Codes());
			x86::pop_ebp(env->Codes());
			x86::retn(env->Codes());
		}
		int run(Environment *env)
		{
			env->status = Function::Return;
			return 0;
		}
	};
}

}
#endif
