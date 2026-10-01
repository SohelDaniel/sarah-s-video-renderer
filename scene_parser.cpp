#include "scene_parser.h"
#include "shapes2d.h"
#include "math_layout.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <sstream>


std::string parse_message::to_string()const{
	return "line " + std::to_string(line) + ", col " + std::to_string(column) + ": " + text;
}

// ---------------------------------------------------------------------------
//  Did you mean...?
//
//  Edit distance: the fewest single-letter edits (insert a letter, delete
//  one, change one, or swap two neighbours) that turn a into b. Filled in
//  as a table, where d[i][j] = the distance between the first i letters of
//  a and the first j letters of b:
//      d[i][0] = i,  d[0][j] = j           (delete / insert everything)
//      d[i][j] = min( d[i-1][j]   + 1,     delete a's letter
//                     d[i][j-1]   + 1,     insert b's letter
//                     d[i-1][j-1] + (a[i] == b[j] ? 0 : 1),   keep or change
//                     d[i-2][j-2] + 1 if the last two letters are swapped )
//  Without the last line it's Levenshtein distance; with it, swapping two
//  neighbours ("cueb" for "cube", a very common typo) counts as one edit
//  instead of two. (This version is called "optimal string alignment".)
// ---------------------------------------------------------------------------
int edit_distance(const std::string& a,const std::string& b){
	std::vector<std::vector<int>> d(a.size() + 1, std::vector<int>(b.size() + 1, 0));
	for(size_t i = 0;i<=a.size();i++) d[i][0] = int(i);
	for(size_t j = 0;j<=b.size();j++) d[0][j] = int(j);
	for(size_t i = 1;i<=a.size();i++){
		for(size_t j = 1;j<=b.size();j++){
			int change = a[i - 1] == b[j - 1] ? 0 : 1;
			d[i][j] = std::min({d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + change});
			if(i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1]){
				d[i][j] = std::min(d[i][j], d[i - 2][j - 2] + 1);   // two neighbours swapped
			}
		}
	}
	return d[a.size()][b.size()];
}

// The closest known word, if it's close enough to be a likely typo:
// at most 1 edit for short words, about a third of the letters for long ones.
static std::string closest(const std::string& word,const std::vector<std::string>& known){
	std::string best;
	int best_distance = 1000;
	for(const std::string& k : known){
		int dist = edit_distance(word, k);
		if(dist < best_distance){
			best_distance = dist;
			best = k;
		}
	}
	int allowed = std::max(1, int(word.size()) / 3);
	return best_distance <= allowed ? best : "";
}

static std::string did_you_mean(const std::string& word,const std::vector<std::string>& known){
	std::string guess = closest(word, known);
	return guess.empty() ? "" : " (did you mean " + guess + "?)";
}

// ---------------------------------------------------------------------------
//  The words the language knows
// ---------------------------------------------------------------------------

static const std::vector<std::string> shape_words = {
	"cube", "sphere", "cone", "cylinder", "icosahedron", "octahedron", "pyramid", "tetrahedron", "torus", "plane",
};

// the same colors the C++ test scenes use, so a .dan file and its C++
// version give exactly the same scene (checked by engine_test)
static const std::map<std::string, px::Pixel> color_words = {
	{"blue",   px::Pixel( 80, 160, 230)}, {"green",  px::Pixel(120, 200,  90)},
	{"purple", px::Pixel(200, 120, 220)}, {"yellow", px::Pixel(240, 220,  80)},
	{"grey",   px::Pixel(160, 160, 170)}, {"white",  px::Pixel(235, 235, 245)},
	{"red",    px::Pixel(220,  80,  70)}, {"teal",   px::Pixel( 90, 210, 200)},
	{"orange", px::Pixel(230, 130,  60)}, {"gold",   px::Pixel(250, 200,  60)},
};

static const std::map<std::string, size_word> size_words = {
	{"tiny", size_word::tiny}, {"small", size_word::small}, {"normal", size_word::normal},
	{"big", size_word::big}, {"huge", size_word::huge},
};

static const std::map<std::string, relation_kind> relation_words = {
	{"near", relation_kind::near}, {"left_of", relation_kind::left_of}, {"right_of", relation_kind::right_of},
	{"above", relation_kind::above}, {"below", relation_kind::below},
	{"in_front_of", relation_kind::in_front_of}, {"behind", relation_kind::behind},
};

static const std::map<std::string, rate> rate_words = {
	{"linear", rate::linear}, {"smooth", rate::smooth}, {"sine", rate::sine},
	{"rush_into", rate::rush_into}, {"rush_from", rate::rush_from}, {"there_and_back", rate::there_and_back},
};

static const std::map<std::string, view_word> view_words = {
	{"front", view_word::front}, {"front_above", view_word::front_above},
	{"left_above", view_word::left_above}, {"right_above", view_word::right_above},
};

template<typename T>
static std::vector<std::string> keys_of(const std::map<std::string, T>& m){
	std::vector<std::string> keys;
	for(const auto& entry : m) keys.push_back(entry.first);
	return keys;
}

// ---------------------------------------------------------------------------
//  1. The lexer: text -> tokens.
//
//  "planet = octahedron orbits sun 1 turn 0s-20s" becomes
//      word(planet) equals word(octahedron) word(orbits) word(sun)
//      number(1) word(turn) time(0) dash time(20) end_of_line
//  Every token remembers its line and column, for the error messages.
// ---------------------------------------------------------------------------

enum class token_kind{ word, number, time, text, color, equals, comma, dash, end_of_line, end_of_file, bad };

struct token{
	token_kind kind;
	std::string value;   // the word, the digits, the text between quotes
	int line, column;
};

static std::vector<token> tokenize(const std::string& source){
	std::vector<token> tokens;
	int line = 1, column = 1;
	size_t i = 0;
	auto at = [&](size_t k){ return k < source.size() ? source[k] : '\0'; };
	auto advance = [&](){
		if(source[i] == '\n'){ line++; column = 1; }
		else column++;
		i++;
	};

	while(i < source.size()){
		char c = source[i];
		int start_column = column;

		if(c == '\n'){
			tokens.push_back({token_kind::end_of_line, "", line, column});
			advance();
		}else if(c == ' ' || c == '\t' || c == '\r'){
			advance();
		}else if(c == '#'){
			// "#ff8800" is a color: '#' and exactly 6 hex digits. Anything else
			// after a '#' is a comment, up to the end of the line.
			bool is_color = true;
			for(size_t k = 1;k<=6;k++) if(!std::isxdigit((unsigned char)at(i + k))) is_color = false;
			if(is_color && (std::isalnum((unsigned char)at(i + 7)) || at(i + 7) == '_')) is_color = false;
			if(is_color){
				std::string hex = source.substr(i + 1, 6);
				for(int k = 0;k<7;k++) advance();
				tokens.push_back({token_kind::color, hex, line, start_column});
			}else{
				while(i < source.size() && source[i] != '\n') advance();
			}
		}else if(std::isdigit((unsigned char)c)){
			// 12, 1.5, and 12s (a number followed straight away by 's' is a time)
			std::string digits;
			while(std::isdigit((unsigned char)at(i)) || at(i) == '.'){ digits += source[i]; advance(); }
			if(at(i) == 's' && !std::isalnum((unsigned char)at(i + 1)) && at(i + 1) != '_'){
				advance();
				tokens.push_back({token_kind::time, digits, line, start_column});
			}else{
				tokens.push_back({token_kind::number, digits, line, start_column});
			}
		}else if(std::isalpha((unsigned char)c) || c == '_'){
			std::string word;
			while(std::isalnum((unsigned char)at(i)) || at(i) == '_'){ word += source[i]; advance(); }
			tokens.push_back({token_kind::word, word, line, start_column});
		}else if(c == '"'){
			advance();
			std::string text;
			while(i < source.size() && source[i] != '"' && source[i] != '\n'){ text += source[i]; advance(); }
			if(at(i) == '"') advance();
			tokens.push_back({token_kind::text, text, line, start_column});
		}else if(c == '='){
			tokens.push_back({token_kind::equals, "=", line, column}); advance();
		}else if(c == ','){
			tokens.push_back({token_kind::comma, ",", line, column}); advance();
		}else if(c == '-'){
			tokens.push_back({token_kind::dash, "-", line, column}); advance();
		}else{
			tokens.push_back({token_kind::bad, std::string(1, c), line, column}); advance();
		}
	}
	tokens.push_back({token_kind::end_of_line, "", line, column});
	tokens.push_back({token_kind::end_of_file, "", line, column});
	return tokens;
}

// ---------------------------------------------------------------------------
//  2. The parser: tokens -> scene_spec, by recursive descent.
//
//  The grammar (docs/21 has it with examples):
//
//    scene      = { line }
//    line       = [ header | arrow | definition | fact ] end_of_line
//    arrow      = "arrow" name name [ color ] [ time "-" time ]
//    title      = "title" text [ time "-" time ] [ write ]
//    math       = "math" text [ time "-" time ] [ write ] (the text is checked by parse_math, docs/30)
//    write      = "write" [ time ]                        (1 s if no time, docs/38)
//    math       ... { "becomes" text "at" time "-" time }   (formulas only, docs/39)
//    header     = "scene" [ text ] [ "view" view_word ]
//    definition = name "=" shape { property }
//    fact       = name phrase { [","] phrase }
//    property   = size | color | "important" | "filled" | label | phrase | ","
//    (shape: a mesh word, or a flat one: circle square triangle hexagon star, docs/40)
//    label      = "label" [ "math" ] text { "math" | time "-" time | "always" }
//    phrase     = relation_word name
//               | "orbits" name [ number ("turn" | "turns") ] time "-" time
//               | "flies_past" name time "-" time
//               | "hits" name "at" time [ "from" time ] [ "sticks" ]
//    color      = color_word | "#rrggbb"
//
//  Each rule is one function below; a rule calls the rules inside it. That's
//  what "recursive descent" means: the call stack follows the shape of the
//  grammar.
//
//  Facts ("comet hits planet at 12s") can mention objects defined further
//  down, so they're kept and applied after every definition has been read.
// ---------------------------------------------------------------------------

class parser{
public:
	explicit parser(std::vector<token> tokens) : tokens(std::move(tokens)){}

	parse_result run();

private:
	// a mistake on this line: report it, skip to the end of the line, go on
	struct line_error{};

	const token& peek()const{ return tokens[pos]; }
	const token& next(){ return tokens[pos++]; }
	bool at_end_of_line()const{ return peek().kind == token_kind::end_of_line || peek().kind == token_kind::end_of_file; }
	[[noreturn]] void fail(const token& where,const std::string& message);
	void note(const token& where,const std::string& message);
	std::string describe(const token& t)const;

	void line();
	void header();
	void arrow();
	void title();
	void formula();
	void write_in(title_spec& s,const std::string& what);
	void becomes(title_spec& s);
	void check_formula(const token& words);
	void definition();
	void fact();
	void property(object_spec& o);
	void phrase(object_spec& o,const token& first);
	std::string name(const std::string& after);
	rate optional_rate();
	float time(const std::string& after);
	float number(const std::string& after);

	std::vector<token> tokens;
	size_t pos = 0;
	parse_result result;

	// facts wait until every object is defined
	struct waiting_fact{
		size_t first_token;   // where in `tokens` the fact starts
	};
	std::vector<waiting_fact> facts;
	std::vector<std::string> defined;   // names, for "did you mean"
	std::vector<std::pair<std::string, token>> references;   // names used in phrases, checked at the end
};

void parser::fail(const token& where,const std::string& message){
	result.errors.push_back({where.line, where.column, message});
	throw line_error{};
}

void parser::note(const token& where,const std::string& message){
	result.notes.push_back({where.line, where.column, message});
}

std::string parser::describe(const token& t)const{
	switch(t.kind){
		case token_kind::end_of_line:
		case token_kind::end_of_file: return "the end of the line";
		case token_kind::time:        return "'" + t.value + "s'";
		case token_kind::color:       return "'#" + t.value + "'";
		default:                      return "'" + t.value + "'";
	}
}

parse_result parser::run(){
	// pass 1: headers and definitions; facts are only remembered
	while(peek().kind != token_kind::end_of_file){
		try{
			line();
		}catch(const line_error&){
			// error recovery: skip the rest of this line, keep going
			while(!at_end_of_line()) pos++;
		}
		if(peek().kind == token_kind::end_of_line) pos++;
	}

	// pass 2: the facts, now that every name is defined
	for(const waiting_fact& f : facts){
		pos = f.first_token;
		try{
			fact();
		}catch(const line_error&){}
	}

	// names used in relations and motions that nothing is called: the solver
	// reports these too, but here we can say what was probably meant
	for(const auto& r : references){
		if(std::find(defined.begin(), defined.end(), r.first) == defined.end()){
			note(r.second, "there is no object called '" + r.first + "'" + did_you_mean(r.first, defined));
		}
	}
	return result;
}

// line = [ header | definition | fact ]
void parser::line(){
	if(at_end_of_line()) return;                       // empty line or only a comment
	const token& first = peek();
	if(first.kind != token_kind::word) fail(first, "a line should start with a name, got " + describe(first));
	if(first.value == "scene"){ header(); return; }
	if(first.value == "arrow" && tokens[pos + 1].kind != token_kind::equals){ arrow(); return; }
	if(first.value == "title" && tokens[pos + 1].kind != token_kind::equals){ title(); return; }
	if(first.value == "math" && tokens[pos + 1].kind != token_kind::equals){ formula(); return; }
	if(tokens[pos + 1].kind == token_kind::equals){ definition(); return; }

	// a fact: remember where it is, skip it for now
	facts.push_back({pos});
	while(!at_end_of_line()) pos++;
}

// header = "scene" [ text ] [ "view" view_word ]
void parser::header(){
	next();   // "scene"
	if(peek().kind == token_kind::text) next();        // the scene's name: just for people reading it
	if(peek().kind == token_kind::word && peek().value == "view"){
		next();
		const token& v = next();
		auto it = view_words.find(v.value);
		if(v.kind != token_kind::word || it == view_words.end()){
			fail(v, "unknown view " + describe(v) + did_you_mean(v.value, keys_of(view_words))
			        + "; try front, front_above, left_above or right_above");
		}
		result.spec.view = it->second;
	}
	if(!at_end_of_line()) fail(peek(), "unexpected " + describe(peek()) + " after the scene line");
}

// arrow = "arrow" name name [ color ] [ time "-" time ]      (docs/26)
void parser::arrow(){
	next();   // "arrow"
	std::string from = name("for where the arrow starts, like: arrow comet planet");
	std::string to = name("for where the arrow points, like: arrow comet planet");
	arrow_spec& a = result.spec.add_arrow(from, to);
	while(!at_end_of_line()){
		const token& t = next();
		if(t.kind == token_kind::color){
			unsigned value = std::stoul(t.value, nullptr, 16);
			a.color = px::Pixel(uint8_t(value >> 16), uint8_t(value >> 8), uint8_t(value));
		}else if(t.kind == token_kind::word && color_words.count(t.value)){
			a.color = color_words.at(t.value);
		}else if(t.kind == token_kind::time){
			a.start = std::stof(t.value);
			const token& dash = next();
			if(dash.kind != token_kind::dash) fail(dash, "expected '-' between the start and end times, like 2s-8s");
			a.end = time("for when the arrow goes away, like 8s");
		}else{
			fail(t, "unexpected " + describe(t) + " in an arrow; it takes a color and a time range, like: arrow comet planet red 2s-8s");
		}
	}
}

// {"a", "b", "c"} -> "a, b, c"
static std::string join(const std::vector<std::string>& words){
	std::string out;
	for(size_t i = 0;i<words.size();i++) out += (i ? ", " : "") + words[i];
	return out;
}

// 2.5 -> "2.5s", 3 -> "3s"
static std::string format_seconds(float seconds){
	std::ostringstream out;
	out << seconds << "s";
	return out.str();
}

// A formula in quotes, checked by the math parser (30). An error points at
// the exact character: the text starts one column after its quote.
void parser::check_formula(const token& words){
	math_parse_result check = parse_math(words.value);
	if(!check.error.empty()){
		token at = words;
		at.column = words.column + check.column;
		fail(at, "in the formula: " + check.error);
	}
}

// { "becomes" text "at" time "-" time }      (docs/39)
// Each change has to happen while the formula is shown, after the writing
// and after the change before it.
void parser::becomes(title_spec& s){
	float earliest = s.start + s.write;
	while(peek().kind == token_kind::word && peek().value == "becomes"){
		next();   // "becomes"
		const token& words = next();
		if(words.kind != token_kind::text) fail(words, "expected the new formula in quotes, like: becomes \"E = mc^2\" at 5s-6s");
		check_formula(words);
		const token& at = next();
		if(!(at.kind == token_kind::word && at.value == "at")) fail(at, "expected 'at' and when it changes, like: at 5s-6s");
		const token& from = peek();
		becomes_step step{words.value, time("for when the change starts, like 5s"), 0.0f};
		const token& dash = next();
		if(dash.kind != token_kind::dash) fail(dash, "expected '-' between the start and end times, like 5s-6s");
		step.end = time("for when the change is done, like 6s");
		if(step.end <= step.start) fail(from, "the change has to end after it starts");
		if(step.start < earliest){
			fail(from, "the change can't start before " + format_seconds(earliest)
			           + (s.becomes.empty() ? (s.write > 0.0f ? " (when the writing is done)" : " (when the formula appears)")
			                                : " (when the change before it is done)"));
		}
		if(s.end >= 0.0f && step.end > s.end) fail(from, "the change has to be done by " + format_seconds(s.end) + ", when the formula goes away");
		s.becomes.push_back(step);
		earliest = step.end;
	}
}

// write = "write" [ time ]      (docs/38): drawn in over that long (1 s if
// no time is given), from the start of its time range.
void parser::write_in(title_spec& s,const std::string& what){
	if(!(peek().kind == token_kind::word && peek().value == "write")) return;
	next();   // "write"
	s.write = 1.0f;
	if(peek().kind == token_kind::dash) fail(peek(), "writing can't take a negative time; write a time like 2s");
	if(peek().kind == token_kind::time){
		const token& t = peek();
		s.write = time("");
		if(s.write <= 0.0f) fail(t, "writing has to take some time, like write 2s");
		if(s.end >= 0.0f && s.start + s.write > s.end){
			fail(t, "the " + what + " is only shown for " + format_seconds(s.end - s.start)
			        + ", so writing it can't take " + format_seconds(s.write));
		}
	}
}

// title = "title" text [ time "-" time ] [ write ]      (docs/27, 38)
void parser::title(){
	next();   // "title"
	const token& words = next();
	if(words.kind != token_kind::text) fail(words, "expected the title's words in quotes, like: title \"Orbits\" 0s-5s");
	title_spec s{words.value, 0.0f, -1.0f};
	if(peek().kind == token_kind::time){
		s.start = time("");
		const token& dash = next();
		if(dash.kind != token_kind::dash) fail(dash, "expected '-' between the start and end times, like 0s-5s");
		s.end = time("for when the title goes away, like 5s");
	}
	write_in(s, "title");
	if(!at_end_of_line()) fail(peek(), "unexpected " + describe(peek()) + " after the title");
	result.spec.titles.push_back(s);
}

// math = "math" text [ time "-" time ] [ write ]      (docs/30, 38)
void parser::formula(){
	next();   // "math"
	const token& words = next();
	if(words.kind != token_kind::text) fail(words, "expected the formula in quotes, like: math \"E = mc^2\" 0s-5s");
	check_formula(words);
	title_spec s{words.value, 0.0f, -1.0f};
	if(peek().kind == token_kind::time){
		s.start = time("");
		const token& dash = next();
		if(dash.kind != token_kind::dash) fail(dash, "expected '-' between the start and end times, like 0s-5s");
		s.end = time("for when the formula goes away, like 5s");
	}
	write_in(s, "formula");
	becomes(s);
	if(!at_end_of_line()) fail(peek(), "unexpected " + describe(peek()) + " after the formula");
	result.spec.maths.push_back(s);
}

// definition = name "=" shape { property }
void parser::definition(){
	const token& n = next();
	next();   // "="
	// the name counts as defined even if the rest of the line is wrong, so
	// one mistake doesn't cause a pile of "no object called ..." after it
	defined.push_back(n.value);
	const token& s = next();
	const std::vector<std::string>& flats = flat_shape_words();
	bool is_flat = std::find(flats.begin(), flats.end(), s.value) != flats.end();
	if(s.kind != token_kind::word || (!is_flat && std::find(shape_words.begin(), shape_words.end(), s.value) == shape_words.end())){
		std::vector<std::string> all = shape_words;
		all.insert(all.end(), flats.begin(), flats.end());
		fail(s, "expected a shape after '=', got " + describe(s) + did_you_mean(s.value, all));
	}
	// a flat shape (docs/40) has no mesh file
	object_spec& o = result.spec.add(n.value, is_flat ? "" : "shapes/" + s.value + ".obj", px::Pixel(200, 200, 200));
	if(is_flat) o.flat = s.value;
	while(!at_end_of_line()) property(o);
}

// property = size | color | "important" | phrase | ","
void parser::property(object_spec& o){
	const token& t = next();
	if(t.kind == token_kind::comma) return;
	if(t.kind == token_kind::color){
		unsigned value = std::stoul(t.value, nullptr, 16);
		o.color = px::Pixel(uint8_t(value >> 16), uint8_t(value >> 8), uint8_t(value));
		return;
	}
	if(t.kind != token_kind::word) fail(t, "unexpected " + describe(t));

	if(auto size = size_words.find(t.value); size != size_words.end()){ o.size = size->second; return; }
	if(auto color = color_words.find(t.value); color != color_words.end()){ o.color = color->second; return; }
	if(t.value == "important"){ o.importance = 10; return; }
	if(t.value == "filled"){
		if(o.flat.empty()) fail(t, "only flat shapes (" + join(flat_shape_words()) + ") can be filled");
		o.filled = true;
		return;
	}
	phrase(o, t);
}

// phrase = relation_word name | "orbits" ... | "flies_past" ... | "hits" ...
void parser::phrase(object_spec& o,const token& first){
	if(auto rel = relation_words.find(first.value); rel != relation_words.end()){
		o.relations.push_back({rel->second, name("after '" + first.value + "'")});
		return;
	}
	if(first.value == "orbits"){
		std::string other = name("after 'orbits'");
		float turns = 1.0f;
		if(peek().kind == token_kind::number){
			turns = number("");
			const token& unit = next();
			if(unit.value != "turn" && unit.value != "turns") fail(unit, "expected 'turns' after the number, got " + describe(unit));
		}
		float start = time("for when the orbit starts, like 0s");
		const token& dash = next();
		if(dash.kind != token_kind::dash) fail(dash, "expected '-' between the start and end times, like 0s-20s");
		float end = time("for when the orbit ends, like 20s");
		if(peek().kind == token_kind::word && rate_words.count(peek().value)){
			fail(peek(), "orbits always go at a steady speed, so a loop has no jump (leave out '" + peek().value + "')");
		}
		o.orbits(other, turns, start, end);
		return;
	}
	if(first.value == "flies_past"){
		std::string other = name("after 'flies_past'");
		float start = time("for when it starts, like 4s");
		const token& dash = next();
		if(dash.kind != token_kind::dash) fail(dash, "expected '-' between the start and end times, like 4s-10s");
		float end = time("for when it ends, like 10s");
		o.flies_past(other, start, end, optional_rate());
		return;
	}
	if(first.value == "label"){
		if(peek().kind == token_kind::word && peek().value == "math"){ next(); o.label_math = true; }
		const token& words = next();
		if(words.kind != token_kind::text) fail(words, "expected the label's words in quotes, like: label \"the sun\"");
		o.label = words.value;
		// what may follow the words, in any order
		while(true){
			if(peek().kind == token_kind::word && peek().value == "math"){ next(); o.label_math = true; continue; }
			if(peek().kind == token_kind::word && peek().value == "always"){ next(); o.label_always = true; continue; }
			if(peek().kind == token_kind::time){
				o.label_start = time("");
				const token& dash = next();
				if(dash.kind != token_kind::dash) fail(dash, "expected '-' between the label's start and end times, like 2s-8s");
				o.label_end = time("for when the label goes away, like 8s");
				continue;
			}
			break;
		}
		if(o.label_math){
			math_parse_result check = parse_math(o.label);
			if(!check.error.empty()){
				token at = words;
				at.column = words.column + check.column;
				fail(at, "in the label's formula: " + check.error);
			}
		}
		return;
	}
	if(first.value == "fades_in" || first.value == "fades_out"){
		float start = time("for when the fade starts, like 0s");
		const token& dash = next();
		if(dash.kind != token_kind::dash) fail(dash, "expected '-' between the start and end times, like 0s-2s");
		float end = time("for when the fade ends, like 2s");
		if(first.value == "fades_in") o.fades_in(start, end, optional_rate());
		else                          o.fades_out(start, end, optional_rate());
		return;
	}
	if(first.value == "hits"){
		std::string other = name("after 'hits'");
		const token& at = next();
		if(at.value != "at") fail(at, "expected 'at' and a time after 'hits " + other + "', like: hits " + other + " at 12s");
		float when = time("after 'at', like 12s");
		float approach = 4.0f;
		if(peek().kind == token_kind::word && peek().value == "from"){
			next();
			approach = when - time("after 'from', like 8s");
		}
		rate how = optional_rate();
		if(peek().kind == token_kind::word && peek().value == "sticks") next();   // sticking is what hits do
		o.hits(other, when, approach, how);
		return;
	}

	std::vector<std::string> known = {"important", "orbits", "flies_past", "hits", "fades_in", "fades_out", "label", "filled"};
	for(const auto& m : {keys_of(size_words), keys_of(color_words), keys_of(relation_words)}){
		known.insert(known.end(), m.begin(), m.end());
	}
	fail(first, "unknown word " + describe(first) + did_you_mean(first.value, known));
}

// fact = name phrase { [","] phrase }
void parser::fact(){
	const token& n = next();
	object_spec* o = nullptr;
	for(object_spec& candidate : result.spec.objects){
		if(candidate.name == n.value){
			o = &candidate;
			break;
		}
	}
	if(!o) fail(n, "there is no object called '" + n.value + "' to say this about" + did_you_mean(n.value, defined));
	while(!at_end_of_line()){
		const token& t = next();
		if(t.kind == token_kind::comma) continue;
		if(t.kind != token_kind::word) fail(t, "unexpected " + describe(t));
		phrase(*o, t);
	}
}

// an easing word, if there is one (docs/23): linear when there isn't
rate parser::optional_rate(){
	if(peek().kind == token_kind::word){
		auto it = rate_words.find(peek().value);
		if(it != rate_words.end()){
			next();
			return it->second;
		}
	}
	return rate::linear;
}

std::string parser::name(const std::string& after){
	const token& t = next();
	if(t.kind != token_kind::word) fail(t, "expected a name " + after + ", got " + describe(t));
	references.push_back({t.value, t});
	return t.value;
}

float parser::time(const std::string& what){
	const token& t = next();
	if(t.kind == token_kind::number) fail(t, "times need an 's': write " + t.value + "s instead of " + t.value);
	if(t.kind != token_kind::time) fail(t, "expected a time " + what + ", got " + describe(t));
	return std::stof(t.value);
}

float parser::number(const std::string& what){
	const token& t = next();
	if(t.kind != token_kind::number) fail(t, "expected a number " + what + ", got " + describe(t));
	return std::stof(t.value);
}

parse_result parse_scene(const std::string& text){
	parser p(tokenize(text));
	return p.run();
}

parse_result parse_scene_file(const std::string& filename){
	std::ifstream in(filename);
	if(!in){
		parse_result r;
		r.errors.push_back({0, 0, "could not open " + filename});
		return r;
	}
	std::stringstream buffer;
	buffer << in.rdbuf();
	return parse_scene(buffer.str());
}
