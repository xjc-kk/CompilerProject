#include "parse.h"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <stack>
#include <cassert>
#include <algorithm>
Type data_type;

string getstr(const char* i, const char* j)
{
	while (*i    ==' ') i++;
	while (*(j-1)==' ') j--;
	if (i>=j) return "";
	return string(i, j);
}
vector<string> parse_id(const char* pt)
{
	vector<string> vs;
	const char* j=pt;
	for (const char* i=pt;*i != '\0';i++)
		if (*i == ',' || *i == ']')
		{
			string s = getstr(j+1, i-1);
			if (s.size() >= 1)
				vs.push_back(s);
			j = i;
			if (*i == ']')
				break;
		}
		else if (*i == '[')
			j = i;

	return vs;
}
const char* parse_int(const char* i, int& x)
{
	x = 0;
	while (*i >= '0' && *i <= '9')
	{
		x = x*10 + *i -'0';
		i++;
	}
	return i-1;
}
inline int isop(char c)
{
	return c == '+' || c == '-' || c == '*' || c == '/' ||
		c == '(' || c == ')' || 
		c == '=' || c == ';' ;
}
inline int pri(char c)
{
	switch (c){
		case ';': return -3;
		case '=': return -2;
		case ')': return -1;
		case '(': return 0;
		case '+': return 1;
		case '-': return 1;
		case '*': return 2;
		case '/': return 2;
		default : assert(1==0);
	}
}
	
vector<Stmt> ret;

Expr compute(Expr& a, Expr& b, char c)
{
	switch (c){
		case '+' : return Binary::make(data_type, BinaryOpType::Add, a, b);
		case '-' : return Binary::make(data_type, BinaryOpType::Sub, a, b);
		case '*' : return Binary::make(data_type, BinaryOpType::Mul, a, b);
		case '/' : return Binary::make(data_type, BinaryOpType::Div, a, b);
		default	 : assert(1==0);
	}
}

void putop(char c, stack<Expr>& id, stack<char>& op)
{
	// c = +-*();
	while (!op.empty() && pri(op.top()) >= pri(c))
	{
		if (c == '(') break;
		char oper   = op.top();	op.pop();
	//	printf("%c\n", oper);
		// A = A + alpha * (B*C)
		if (c == ')' && oper == '(') break;
		
		Expr second = id.top(); id.pop();
		Expr first  = id.top(); id.pop();
		

		
		if (oper == '=')
		{
			ret.push_back(Move::make(first, second, MoveType::MemToMem));
			continue;
		}
		Expr ret = compute(first, second, oper);
		id.push(ret);
	}
	if (c == ')') return;
	if (c == ';')
	{
		assert(id.size() == 0);
		assert(op.size() == 0);
		return;
	}
	op.push(c);
}
const char* parse_av(const char* pt, vector<Expr>& av)
{
	// [i+j, j*k, k-j]
	stack<Expr> id;
	stack<char> op;
	const Type index_type = Type::int_scalar(32);

//	printf("parse_av: ");
	for (const char* i=pt;; i++)
	{
		if (*i == ' ') continue;
	//	printf("%c ", *i);
		fflush(stdout);

		if (*i == ',' || *i == ']')
		{
			putop(')', id, op);
			assert(id.size() == 1);
			assert(op.size() == 0);
			av.push_back(id.top());
			id.pop();

			if (*i == ']') 
				return i;
			continue;
		}
		if (isop(*i))
		{
			putop(*i, id, op);
		}
		else if (*i >= '0' && *i <= '9')
		{
			int x;
			i = parse_int(i, x);
			id.push(Expr(x));
		}
		else
		{
    		Expr dom = Dom::make(index_type, -1, -1);
			string name(i, i+1);		// assume one length index
    		Expr idx = Index::make(index_type, name, dom, IndexType::Spatial);
			id.push(idx);
		}
	}
}
vector<Stmt> parse_kernel(const char* pt)
{
	stack<Expr> id;
	stack<char> op;
	ret.resize(0);

	for (const char* i=pt;*i != '\0'; i++)
	{
		if (*i == ' ') continue;
		if (*i == '\n') continue;
		if (*i == '\r') continue;

		if (isop(*i)) 
		{
			putop(*i, id, op);
		}
		else if (*i >= '0' && *i <= '9')
		{
			int x;
			i = parse_int(i, x);
			id.push(Expr(x));
		}
		else
		{
			// A<20, 20>[i+j, k-i]
			vector<Expr> av;
			vector<size_t> cv;
			const char* j = i;
			while (*j != '<') j++;
			string name(i, j);
			j++;

			int x = 0;
			for (;;j++)		// get cv
			{
				if (*j == ' ') continue;
				if (*j == ',' || *j == '>')
				{
					cv.push_back(x);
					x = 0;
					if (*j == '>')
						break;
				}
				if (*j >= '0' && *j <= '9')
					x = x*10 + (*j) - '0';
			}
			assert(*j == '>');
			if (*(j+1) == '[')		// get av
				j = parse_av(j+2, av);
			id.push(Var::make(data_type, name, av, cv));

			i = j;
		}
	}
	return ret;
}
void parse(FILE* f, record& js)
{
	char buf[1000];
	char type[1000];
	char content[1000];

	while (!feof(f))
	{
		fgets(buf, 1000, f);
		int len = strlen(buf);
	//	printf(":: %s\n", buf);
		
		replace(buf, buf+len, '"', ' ');
		char* pt = find(buf, buf+len, ':') ;
		if (*pt == '\0') continue;
		*pt = '\0';

		sscanf(buf , "%s", type);
		sscanf(pt+1, "%s", content);

	//	printf("tp: %s\ncont: %s\n", type, content);

		if (strcmp(type, "data_type")==0)
		{
			js.type = content;
			if (strcmp(content, "float")==0)	data_type = Type::float_scalar(32);
			if (strcmp(content, "int"  )==0)	data_type = Type::int_scalar(32);
		}
		if (strcmp(type, "name"		)==0)	js.name = content;
		if (strcmp(type, "ins"		)==0)	js.in   = parse_id(pt+1);
		if (strcmp(type, "outs"		)==0)	js.out  = parse_id(pt+1);
		if (strcmp(type, "kernel"	)==0)	js.vs   = parse_kernel(pt+1);
	}
}