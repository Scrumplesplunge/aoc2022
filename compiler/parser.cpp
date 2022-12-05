#include "parser.hpp"

#include "debug_output.hpp"

#include <sstream>
#include <stdexcept>

namespace aoc2022 {
namespace {

bool IsTerm(const Token& token) {
  if (auto* s = std::get_if<Symbol>(&token.value)) {
    return *s == Symbol::kOpenParen || *s == Symbol::kOpenSquare;
  }
  const Token::Type type = Token::Type(token.value.index());
  return type == Token::kIdentifier || type == Token::kInteger ||
         type == Token::kCharacter || type == Token::kString;
}

struct Parser {
  template <typename... Args>
  std::runtime_error Error(const Args&... args) {
    const Location location = cursor[0].location;
    std::ostringstream message;
    message << "input:" << location.line << ":" << location.column << ": ";
    (message << ... << args);
    return std::runtime_error(message.str());
  }

  syntax::Program ParseProgram() {
    syntax::Program program;
    while (true) {
      while (Consume(Space::kNewline)) {}
      if (Consume(Space::kEnd)) break;
      program.definitions.push_back(ParseDefinition());
    }
    return program;
  }

  syntax::Definition ParseDefinition() {
    syntax::Identifier name = ParseIdentifier();
    std::vector<syntax::Identifier> parameters;
    while (!NextIs(Symbol::kEquals)) {
      parameters.push_back(ParseIdentifier());
    }
    const Location location = cursor[0].location;
    Eat(Symbol::kEquals);
    syntax::Expression value = ParseExpression();
    return syntax::Definition(location, std::move(name), std::move(parameters),
                              std::move(value));
  }

  syntax::Identifier ParseIdentifier() {
    if (const auto* i = std::get_if<Identifier>(&cursor[0].value)) {
      const Location location = cursor[0].location;
      cursor = cursor.subspan(1);
      return syntax::Identifier(location, i->value);
    } else {
      throw Error("expected identifier, got ", cursor[0]);
    }
  }

  syntax::Integer ParseInteger() {
    if (const auto* i = std::get_if<Integer>(&cursor[0].value)) {
      const Location location = cursor[0].location;
      cursor = cursor.subspan(1);
      return syntax::Integer(location, i->value);
    } else {
      throw Error("expected integer, got ", cursor[0]);
    }
  }

  syntax::Character ParseCharacter() {
    if (const auto* i = std::get_if<Character>(&cursor[0].value)) {
      const Location location = cursor[0].location;
      cursor = cursor.subspan(1);
      return syntax::Character(location, i->value);
    } else {
      throw Error("expected character, got ", cursor[0]);
    }
  }

  syntax::String ParseString() {
    if (const auto* i = std::get_if<String>(&cursor[0].value)) {
      const Location location = cursor[0].location;
      cursor = cursor.subspan(1);
      return syntax::String(location, i->value);
    } else {
      throw Error("expected string, got ", cursor[0]);
    }
  }

  syntax::List ParseList() {
    const Location location = cursor[0].location;
    Eat(Symbol::kOpenSquare);
    std::vector<syntax::Expression> elements;
    const bool indented = Consume(Space::kIndent);
    while (true) {
      if ((!indented || Consume(Space::kDedent)) &&
          Consume(Symbol::kCloseSquare)) {
        break;
      }
      elements.push_back(ParseExpression());
      if (Consume(Symbol::kComma)) continue;
    }
    return syntax::List(location, std::move(elements));
  }

  syntax::Expression ParseExpression() {
    if (Consume(Space::kIndent)) {
      syntax::Expression result = ParseExpression();
      if (!Consume(Space::kDedent)) throw Error("expected dedent");
      return result;
    } else if (NextIs(Keyword::kCase)) {
      return ParseCase();
    } else if (NextIs(Keyword::kIf)) {
      return ParseIf();
    } else {
      return ParseCons();
    }
  }

  syntax::Expression ParseTerm() {
    if (Consume(Symbol::kOpenParen)) {
      syntax::Expression result = ParseExpression();
      if (!Consume(Symbol::kCloseParen)) throw Error("expected ')'");
      return result;
    } else if (NextIs(Symbol::kOpenSquare)) {
      return ParseList();
    } else if (cursor[0].value.index() == Token::kIdentifier) {
      return ParseIdentifier();
    } else if (cursor[0].value.index() == Token::kInteger) {
      return ParseInteger();
    } else if (cursor[0].value.index() == Token::kCharacter) {
      return ParseCharacter();
    } else if (cursor[0].value.index() == Token::kString) {
      return ParseString();
    } else {
      throw Error("expected term");
    }
  }

  syntax::Expression ParseApply() {
    syntax::Expression result = ParseTerm();
    while (IsTerm(cursor[0])) {
      const Location location = cursor[0].location;
      result = syntax::Apply(location, std::move(result), ParseTerm());
    }
    return result;
  }

  syntax::Expression ParseSum() {
    syntax::Expression result = ParseApply();
    while (true) {
      const Location location = cursor[0].location;
      if (Consume(Symbol::kPlus)) {
        result = syntax::Add(location, std::move(result), ParseApply());
      } else {
        return result;
      }
    }
  }

  syntax::Expression ParseCompare() {
    syntax::Expression result = ParseSum();
    const Location location = cursor[0].location;
    if (Consume(Symbol::kLess)) {
      return syntax::LessThan(location, std::move(result), ParseSum());
    } else {
      return result;
    }
  }

  syntax::Expression ParseCompose() {
    syntax::Expression result = ParseCompare();
    const Location location = cursor[0].location;
    if (!Consume(Symbol::kDot)) return result;
    return syntax::Compose(location, std::move(result), ParseCompose());
  }

  syntax::Expression ParseCons() {
    syntax::Expression result = ParseCompose();
    const Location location = cursor[0].location;
    if (!Consume(Symbol::kColon)) return result;
    return syntax::Cons(location, std::move(result), ParseCons());
  }

  syntax::Case ParseCase() {
    const Location location = cursor[0].location;
    Eat(Keyword::kCase);
    syntax::Expression value = ParseExpression();
    Eat(Keyword::kOf);
    Eat(Space::kIndent);
    std::vector<syntax::Alternative> alternatives;
    while (true) {
      syntax::Expression pattern = ParseExpression();
      const Location location = cursor[0].location;
      Eat(Symbol::kArrow);
      syntax::Expression value = ParseExpression();
      alternatives.emplace_back(location, std::move(pattern), std::move(value));
      if (Consume(Space::kDedent)) break;
      Eat(Space::kNewline);
    }
    return syntax::Case(location, std::move(value), std::move(alternatives));
  }

  syntax::If ParseIf() {
    const Location location = cursor[0].location;
    Eat(Keyword::kIf);
    syntax::Expression condition = ParseExpression();
    Eat(Keyword::kThen);
    syntax::Expression then_branch = ParseExpression();
    Eat(Keyword::kElse);
    syntax::Expression else_branch = ParseExpression();
    return syntax::If(location, std::move(condition), std::move(then_branch),
                      std::move(else_branch));
  }

  template <typename T>
  bool NextIs(const T& s) {
    const auto* c = std::get_if<T>(&cursor[0].value);
    return c && *c == s;
  }

  template <typename T>
  bool Consume(const T& s) {
    if (NextIs(s)) {
      cursor = cursor.subspan(1);
      return true;
    }
    return false;
  }

  template <typename T>
  void Eat(const T& s) {
    if (!Consume(s)) {
      throw Error(cursor[0].location, "expected ", s, ", got ", cursor[0]);
    }
  }

  std::span<const Token> cursor;
};

}  // namespace

syntax::Program Parse(std::span<const Token> tokens) {
  Parser parser{.cursor = tokens};
  return parser.ParseProgram();
}

}  // namespace aoc2022
