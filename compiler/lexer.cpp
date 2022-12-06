#include "lexer.hpp"

#include <cassert>
#include <sstream>
#include <stdexcept>
#include <map>

namespace aoc2022 {
namespace {

const std::map<std::string_view, Keyword, std::less<>> keywords = {
  {"case", Keyword::kCase},
  {"of", Keyword::kOf},
  {"let", Keyword::kLet},
  {"in", Keyword::kIn},
  {"if", Keyword::kIf},
  {"then", Keyword::kThen},
  {"else", Keyword::kElse},
};

const std::map<std::string_view, Symbol, std::less<>> operators = {
  {"+", Symbol::kAdd},
  {"&&", Symbol::kAnd},
  {"->", Symbol::kArrow},
  {"||", Symbol::kOr},
  {":", Symbol::kColon},
  {",", Symbol::kComma},
  {"==", Symbol::kCompareEqual},
  {">", Symbol::kCompareGreater},
  {">=", Symbol::kCompareGreaterOrEqual},
  {"<", Symbol::kCompareLess},
  {"<=", Symbol::kCompareLessOrEqual},
  {"!=", Symbol::kCompareNotEqual},
  {"++", Symbol::kConcat},
  {"/", Symbol::kDivide},
  {".", Symbol::kDot},
  {"=", Symbol::kEquals},
  {"%", Symbol::kModulo},
  {"*", Symbol::kMultiply},
  {"||", Symbol::kOr},
  {"-", Symbol::kSubtract},
};

bool IsDigit(char c) { return '0' <= c && c <= '9'; }
bool IsLower(char c) { return 'a' <= c && c <= 'z'; }
bool IsUpper(char c) { return 'A' <= c && c <= 'Z'; }
bool IsAlpha(char c) { return IsLower(c) || IsUpper(c); }

bool IsIdentifier(char c) {
  return IsAlpha(c) || IsDigit(c) || c == '\'';
}

bool IsIdentifierStart(char c) { return IsLower(c); }

bool IsOperator(char c) {
  switch (c) {
    case '!': case '%': case '&': case '*': case '+': case ',': case '-':
    case '.': case '/': case ':': case '<': case '=': case '>': case '|':
      return true;
  }
  return false;
}

struct IndentationLevel {
  Location location;
  int amount;
};

struct Lexer {
  template <typename... Args>
  std::runtime_error Error(const Args&... args) {
    std::ostringstream message;
    message << source.filename << ":" << line << ":" << column << ": ";
    (message << ... << args);
    return std::runtime_error(message.str());
  }

  void SkipWhitespace() {
    const char* const first = cursor.data();
    const char* const end = first + cursor.size();
    const char* i = first;
    while (i != end && *i == ' ') i++;
    Advance(i - first);
  }

  void SkipLineEnd() {
    SkipWhitespace();
    if (cursor.starts_with("--")) {
      const auto end = cursor.find("\n");
      assert(end != cursor.npos);
      Advance(end);
    }
    assert(!cursor.empty() && cursor[0] == '\n');
    Advance(1);
    HandleIndent();
  }

  void SkipToNext() {
    while (true) {
      SkipWhitespace();
      if (!cursor.starts_with("\n") && !cursor.starts_with("--")) return;
      SkipLineEnd();
    }
  }

  void Advance(int n) {
    assert(n <= (int)cursor.size());
    for (char c : cursor.substr(0, n)) {
      if (c == '\n') {
        line++;
        column = 1;
      } else {
        column++;
      }
    }
    cursor.remove_prefix(n);
  }

  void HandleIndent() {
    const Location location(&source, line, column);
    const char* const first = cursor.data();
    const char* const end = first + cursor.size();
    const char* i = first;
    while (i != end && *i == ' ') i++;
    const int new_indent = i - first;
    Advance(i - first);
    if (cursor.starts_with("\n") || cursor.starts_with("--")) {
      return SkipLineEnd();
    }
    if (indentation_levels.back().amount < new_indent) {
      tokens.emplace_back(location, Space::kIndent);
      indentation_levels.push_back({location, new_indent});
    } else if (indentation_levels.back().amount > new_indent) {
      while (indentation_levels.back().amount > new_indent) {
        tokens.emplace_back(location, Space::kDedent);
        indentation_levels.pop_back();
      }
      // The new indentation level must be equal to some previous one, not
      // a previously unseen indentation level.
      if (indentation_levels.back().amount != new_indent) {
        throw Error("bad indentation");
      }
    } else {
      tokens.emplace_back(location, Space::kNewline);
    }
  }

  void Lex() {
    while (true) {
      SkipToNext();
      const Location location(&source, line, column);
      if (cursor.empty()) {
        tokens.emplace_back(location, Space::kEnd);
        return;
      }
      const char c = cursor[0];
      if (c == '(') {
        tokens.emplace_back(location, Symbol::kOpenParen);
        Advance(1);
      } else if (c == ')') {
        tokens.emplace_back(location, Symbol::kCloseParen);
        Advance(1);
      } else if (c == '[') {
        tokens.emplace_back(location, Symbol::kOpenSquare);
        Advance(1);
      } else if (c == ']') {
        tokens.emplace_back(location, Symbol::kCloseSquare);
        Advance(1);
      } else if (c == ',') {
        tokens.emplace_back(location, Symbol::kComma);
        Advance(1);
      } else if (c == '\'') {
        LexCharacter();
      } else if (c == '\"') {
        LexString();
      } else if (IsDigit(cursor[0])) {
        LexInteger();
      } else if (IsIdentifierStart(cursor[0])) {
        LexIdentifierOrKeyword();
      } else if (IsOperator(cursor[0])) {
        LexOperator();
      } else {
        throw Error("illegal token");
      }
    }
  }

  template <auto Predicate>
  std::string_view PeekSequence() {
    const char* const first = cursor.data();
    const char* const last = first + cursor.size();
    const char* i = first;
    while (i != last && Predicate(*i)) i++;
    return std::string_view(first, i - first);
  }

  std::string_view PeekWord() { return PeekSequence<IsIdentifier>(); }

  bool ConsumePrefix(std::string_view value) {
    if (!cursor.starts_with(value)) return false;
    Advance(value.size());
    return true;
  }

  void LexCharacter() {
    const Location location(&source, line, column);
    if (!ConsumePrefix("'")) throw Error("bad character literal");
    if (cursor.size() < 2) throw Error("unterminated character literal");
    if (cursor[0] == '\'') throw Error("empty character literal");
    char value;
    if (cursor[0] == '\\') {
      // Complex case with an escape sequence.
      switch (cursor[1]) {
        case '\\': value = '\\'; break;
        case '\'': value = '\''; break;
        case '\"': value = '\"'; break;
        case 'n': value = '\n'; break;
        case 'r': value = '\r'; break;
        case 't': value = '\t'; break;
        default: throw Error("unrecognised escape sequence");
      }
      Advance(2);
    } else {
      value = cursor[0];
      Advance(1);
    }
    if (!ConsumePrefix("'")) throw Error("expected '\\''");
    tokens.emplace_back(location, Character(value));
  }

  void LexString() {
    const Location location(&source, line, column);
    if (cursor.empty() || cursor[0] != '"') {
      throw Error("expected string literal");
    }
    Advance(1);
    std::string value;
    while (true) {
      if (cursor.empty()) throw Error("unterminated string literal");
      if (ConsumePrefix("\"")) {
        tokens.emplace_back(location, String(std::move(value)));
        return;
      }
      if (cursor[0] == '\\') {
        if (cursor.size() < 2) throw Error("unterminated string literal");
        // Complex case with an escape sequence.
        switch (cursor[1]) {
          case '\\': value.push_back('\\'); break;
          case '\'': value.push_back('\''); break;
          case '\"': value.push_back('\"'); break;
          case 'n':  value.push_back('\n'); break;
          case 'r':  value.push_back('\r'); break;
          case 't':  value.push_back('\t'); break;
          default: throw Error("unrecognised escape sequence");
        }
        Advance(2);
      } else {
        value.push_back(cursor[0]);
        Advance(1);
      }
    }
  }

  void LexInteger() {
    const Location location(&source, line, column);
    const std::string_view word = PeekWord();
    std::int64_t value = 0;
    for (char c : word) {
      if (!IsDigit(c)) throw Error("bad int");
      value = 10 * value + (c - '0');
    }
    Advance(word.size());
    tokens.emplace_back(location, Integer(value));
  }

  void LexIdentifierOrKeyword() {
    const Location location(&source, line, column);
    const std::string_view word = PeekWord();
    if (word.empty() || !IsIdentifierStart(word[0])) {
      throw Error("bad identifier");
    }
    const auto i = keywords.find(word);
    if (i == keywords.end()) {
      tokens.emplace_back(location, Identifier(std::string(word)));
    } else {
      tokens.emplace_back(location, i->second);
    }
    Advance(word.size());
  }

  void LexOperator() {
    const Location location(&source, line, column);
    const std::string_view op = PeekSequence<IsOperator>();
    const auto i = operators.find(op);
    if (i == operators.end()) throw Error("bad operator");
    Advance(op.size());
    tokens.emplace_back(location, i->second);
  }

  int line = 1;
  int column = 1;
  const Source& source;
  std::string_view cursor;
  std::vector<IndentationLevel> indentation_levels = {{
      .location = {.source = &source, .line = 1, .column = 1}, .amount = 0}};
  std::vector<Token> tokens = {};
};

}  // namespace

std::vector<Token> Lex(const Source& source) {
  assert(!source.contents.empty() && source.contents.back() == '\n');
  Lexer lexer = {.source = source, .cursor = source.contents};
  lexer.Lex();
  return lexer.tokens;
}

}  // namespace aoc2022
