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
  {"=", Symbol::kEquals},
  {".", Symbol::kDot},
  {":", Symbol::kColon},
  {"+", Symbol::kPlus},
  {"->", Symbol::kArrow},
  {"<", Symbol::kLess},
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
    case '=':
    case '.':
    case ':':
    case '+':
    case '-':
    case '>':
    case '<':
      return true;
  }
  return false;
}

struct Lexer {
  template <typename... Args>
  std::runtime_error Error(const Args&... args) {
    std::ostringstream message;
    message << "input:" << line << ":" << column << ": ";
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
    const char* const first = cursor.data();
    const char* const end = first + cursor.size();
    const char* i = first;
    int new_indent = 0;
    while (i != end && *i == ' ') new_indent++;
    Advance(i - first);
    if (cursor.starts_with("\n") || cursor.starts_with("--")) {
      SkipLineEnd();
      return HandleIndent();
    }
    if (indentation_levels.back() < new_indent) {
      tokens.push_back(Space::kIndent);
      indentation_levels.push_back(new_indent);
    } else if (indentation_levels.back() > new_indent) {
      while (indentation_levels.back() > new_indent) {
        tokens.push_back(Space::kDedent);
        indentation_levels.pop_back();
      }
      // The new indentation level must be equal to some previous one, not
      // a previously unseen indentation level.
      if (indentation_levels.back() != new_indent) {
        throw Error("bad indentation");
      }
    } else {
      tokens.push_back(Space::kNewline);
    }
  }

  void Lex() {
    while (true) {
      SkipToNext();
      if (cursor.empty()) return;
      const char c = cursor[0];
      if (c == '(') {
        tokens.push_back(Symbol::kOpenParen);
        Advance(1);
      } else if (c == ')') {
        tokens.push_back(Symbol::kCloseParen);
        Advance(1);
      } else if (c == '[') {
        tokens.push_back(Symbol::kOpenSquare);
        Advance(1);
      } else if (c == ']') {
        tokens.push_back(Symbol::kCloseSquare);
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
    tokens.push_back(Character(value));
  }

  void LexString() {
    if (cursor.empty() || cursor[0] != '"') {
      throw Error("expected string literal");
    }
    Advance(1);
    std::string value;
    while (true) {
      if (cursor.empty()) throw Error("unterminated string literal");
      if (ConsumePrefix("\"")) {
        tokens.push_back(String(std::move(value)));
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
    const std::string_view word = PeekWord();
    std::int64_t value = 0;
    for (char c : word) {
      if (!IsDigit(c)) throw Error("bad int");
      value = 10 * value + (c - '0');
    }
    Advance(word.size());
    tokens.push_back(Integer(value));
  }

  void LexIdentifierOrKeyword() {
    const std::string_view word = PeekWord();
    if (word.empty() || !IsIdentifierStart(word[0])) {
      throw Error("bad identifier");
    }
    const auto i = keywords.find(word);
    if (i == keywords.end()) {
      tokens.push_back(Identifier(std::string(word)));
    } else {
      tokens.push_back(i->second);
    }
    Advance(word.size());
  }
  
  void LexOperator() {
    const std::string_view op = PeekSequence<IsOperator>();
    const auto i = operators.find(op);
    if (i == operators.end()) throw Error("bad operator");
    Advance(op.size());
    tokens.push_back(i->second);
  }

  int line = 1;
  int column = 1;
  std::string_view cursor;
  std::vector<int> indentation_levels = {0};
  std::vector<Token> tokens = {};
};

}  // namespace

std::vector<Token> Lex(std::string_view source) {
  assert(!source.empty() && source.back() == '\n');
  Lexer lexer = {.cursor = source};
  lexer.Lex();
  return lexer.tokens;
}

}  // namespace aoc2022
