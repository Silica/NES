#ifndef NES_NES_H
#define NES_NES_H

#include "parser.h"

namespace NES
{
	typedef shptr<IL::Environment> Environment;
	typedef IL::Environment::Native Native;
	struct nes
	{
		static Environment compile_IL(const std::string &s)
		{
			Tokenizer t(s);
			Parser p(&t);
			shptr<AST::NameSpace> ns = p.Parse();
			if (!ns)
				return NULL;
			AST::Environment env(ns);
			Environment ienv = env.gen();
			if (!ienv)
				return NULL;
			return ienv;
		};
		static Native compile(const std::string &s)
		{
			Environment ienv = compile_IL(s);
			if (!ienv)
				return NULL;
			IL::Environment::Native na = ienv->gen();
			return na;
		};
	};
}
#endif
