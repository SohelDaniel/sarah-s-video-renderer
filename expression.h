#pragma once
#include <memory>
#include <string>
#include <vector>

// ============================================================================
//  Expressions (docs/42): the function a graph draws, like "2*sin(x) + 1".
//
//  Parsed by recursive descent into a tree (like the dan language, 21, and
//  formulas, 30), then worked out for any x by walking the tree.
//
//    sum     = product { ("+" | "-") product }
//    product = unary { ("*" | "/") unary }
//    unary   = "-" unary | power
//    power   = atom [ "^" unary ]                  (right to left: 2^3^2 = 2^9)
//    atom    = number | "x" | "pi" | "e" | function "(" sum ")" | "(" sum ")"
//    function: sin cos tan exp log sqrt abs
//
//  Because unary is above power, -x^2 is -(x^2), as in maths.
// ============================================================================

struct expr_node{
	enum class kind{ number, variable, negate, add, subtract, multiply, divide, power, function };
	kind what = kind::number;
	float value = 0.0f;            // a number
	std::string name;              // a function's name
	std::unique_ptr<expr_node> left, right;   // the parts; a function or negate has only `left`
};

struct expression_result{
	std::unique_ptr<expr_node> tree;   // nullptr if there's a mistake
	std::string error;
	int column = 0;                    // where in the text (1 = the first character)
};

expression_result parse_expression(const std::string& text);

// The value at x. Not a number (NaN) or infinite where the function isn't
// defined there: log of a negative, 1/0, ...
float evaluate(const expr_node& node,float x);

// The function names it knows (for did-you-mean).
const std::vector<std::string>& expression_functions();
