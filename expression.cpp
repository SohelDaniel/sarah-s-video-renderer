#include "expression.h"
#include "scene_parser.h"   // edit_distance, for "did you mean"

#include <algorithm>
#include <cctype>
#include <cmath>


const std::vector<std::string>& expression_functions(){
	static const std::vector<std::string> names = {"sin", "cos", "tan", "exp", "log", "sqrt", "abs"};
	return names;
}

namespace{

// One pass over the text, one function per grammar rule (see the header).
class expression_parser{
public:
	explicit expression_parser(const std::string& text) : text(text){}

	expression_result run(){
		expression_result r;
		std::unique_ptr<expr_node> tree = sum();
		skip_spaces();
		if(error.empty() && pos < text.size()){
			bool juxtaposed = std::isalpha((unsigned char)text[pos]) || text[pos] == '(';
			fail("unexpected '" + std::string(1, text[pos]) + "'" + (juxtaposed ? " (to multiply, write *, like 2*x)" : ""), pos);
		}
		if(error.empty()) r.tree = std::move(tree);
		r.error = error;
		r.column = int(error_at) + 1;
		return r;
	}

private:
	const std::string& text;
	size_t pos = 0;
	std::string error;
	size_t error_at = 0;

	void fail(const std::string& message,size_t at){
		if(error.empty()){ error = message; error_at = at; }
	}
	void skip_spaces(){
		while(pos < text.size() && std::isspace((unsigned char)text[pos])) pos++;
	}
	bool take(char c){
		skip_spaces();
		if(pos < text.size() && text[pos] == c){ pos++; return true; }
		return false;
	}
	static std::unique_ptr<expr_node> make(expr_node::kind what,std::unique_ptr<expr_node> left,std::unique_ptr<expr_node> right = nullptr){
		auto n = std::make_unique<expr_node>();
		n->what = what;
		n->left = std::move(left);
		n->right = std::move(right);
		return n;
	}

	// sum = product { ("+" | "-") product }
	std::unique_ptr<expr_node> sum(){
		std::unique_ptr<expr_node> left = product();
		while(error.empty()){
			if(take('+'))      left = make(expr_node::kind::add, std::move(left), product());
			else if(take('-')) left = make(expr_node::kind::subtract, std::move(left), product());
			else break;
		}
		return left;
	}
	// product = unary { ("*" | "/") unary }
	std::unique_ptr<expr_node> product(){
		std::unique_ptr<expr_node> left = unary();
		while(error.empty()){
			if(take('*'))      left = make(expr_node::kind::multiply, std::move(left), unary());
			else if(take('/')) left = make(expr_node::kind::divide, std::move(left), unary());
			else break;
		}
		return left;
	}
	// unary = "-" unary | power
	std::unique_ptr<expr_node> unary(){
		if(take('-')) return make(expr_node::kind::negate, unary());
		return power();
	}
	// power = atom [ "^" unary ]      (the right side is a unary, so 2^3^2 = 2^(3^2))
	std::unique_ptr<expr_node> power(){
		std::unique_ptr<expr_node> base = atom();
		if(error.empty() && take('^')) return make(expr_node::kind::power, std::move(base), unary());
		return base;
	}
	// atom = number | name | function "(" sum ")" | "(" sum ")"
	std::unique_ptr<expr_node> atom(){
		skip_spaces();
		if(pos >= text.size()){ fail("something is missing at the end", pos); return nullptr; }
		size_t at = pos;
		char c = text[pos];
		if(c == '('){
			pos++;
			std::unique_ptr<expr_node> inside = sum();
			if(!take(')')) fail("this '(' is never closed with ')'", at);
			return inside;
		}
		if(std::isdigit((unsigned char)c) || c == '.'){
			size_t used = 0;
			float v = std::stof(text.substr(pos), &used);
			pos += used;
			auto n = std::make_unique<expr_node>();
			n->value = v;
			return n;
		}
		if(std::isalpha((unsigned char)c)){
			std::string name;
			while(pos < text.size() && std::isalpha((unsigned char)text[pos])) name += text[pos++];
			if(name == "x"){
				auto n = std::make_unique<expr_node>();
				n->what = expr_node::kind::variable;
				return n;
			}
			if(name == "pi" || name == "e"){
				auto n = std::make_unique<expr_node>();
				n->value = name == "pi" ? 3.14159265f : 2.71828183f;
				return n;
			}
			for(const std::string& f : expression_functions()){
				if(f != name) continue;
				if(!take('(')){ fail(name + " needs its input in brackets, like " + name + "(x)", pos); return nullptr; }
				auto n = make(expr_node::kind::function, sum());
				n->name = name;
				if(!take(')')) fail("this '(' is never closed with ')'", at + name.size());
				return n;
			}
			// did you mean (docs/21): the closest known name
			std::vector<std::string> known = expression_functions();
			known.insert(known.end(), {"x", "pi", "e"});
			std::string best;
			int best_distance = 1000;
			for(const std::string& k : known){
				int d = edit_distance(name, k);
				if(d < best_distance){ best_distance = d; best = k; }
			}
			std::string hint = best_distance <= std::max(1, int(name.size()) / 3) ? " (did you mean " + best + "?)" : "";
			fail("unknown name '" + name + "'" + hint, at);
			return nullptr;
		}
		fail("unexpected '" + std::string(1, c) + "'", at);
		return nullptr;
	}
};

}

expression_result parse_expression(const std::string& text){
	return expression_parser(text).run();
}

// Walk the tree: work out the parts, then combine them.
float evaluate(const expr_node& n,float x){
	switch(n.what){
		case expr_node::kind::number:   return n.value;
		case expr_node::kind::variable: return x;
		case expr_node::kind::negate:   return -evaluate(*n.left, x);
		case expr_node::kind::add:      return evaluate(*n.left, x) + evaluate(*n.right, x);
		case expr_node::kind::subtract: return evaluate(*n.left, x) - evaluate(*n.right, x);
		case expr_node::kind::multiply: return evaluate(*n.left, x) * evaluate(*n.right, x);
		case expr_node::kind::divide:   return evaluate(*n.left, x) / evaluate(*n.right, x);
		case expr_node::kind::power:    return std::pow(evaluate(*n.left, x), evaluate(*n.right, x));
		case expr_node::kind::function:{
			float v = evaluate(*n.left, x);
			if(n.name == "sin")  return std::sin(v);
			if(n.name == "cos")  return std::cos(v);
			if(n.name == "tan")  return std::tan(v);
			if(n.name == "exp")  return std::exp(v);
			if(n.name == "log")  return std::log(v);      // NaN below 0, -infinity at 0
			if(n.name == "sqrt") return std::sqrt(v);     // NaN below 0
			if(n.name == "abs")  return std::fabs(v);
			return NAN;
		}
	}
	return NAN;
}
