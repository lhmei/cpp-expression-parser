// Source: http://www.daniweb.com/software-development/cpp/code/427500/calculator-using-shunting-yard-algorithm#
// Author: Jesse Brown
// Modifications: Brandon Amos, redpois0n

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <exception>
#include <math.h>

#include "shunting-yard.h"
#include "shunting-yard-exceptions.h"

OppMap_t calculator::buildOpPrecedence() {
  OppMap_t opp;

  // Create the operator precedence map based on C++ default
  // precedence order as described on cppreference website:
  // http://en.cppreference.com/w/cpp/language/operator_precedence
  opp["[]"] = 2; opp["."] = 2;
  opp["^"]  = 3;
  opp["*"]  = 5; opp["/"]  = 5; opp["%"] = 5;
  opp["+"]  = 6; opp["-"]  = 6;
  opp["<<"] = 7; opp[">>"] = 7;
  opp["<"]  = 8; opp["<="] = 8; opp[">="] = 8; opp[">"] = 8;
  opp["=="] = 9; opp["!="] = 9;
  opp["&&"] = 13;
  opp["||"] = 14;
  opp["("]  = 17; opp["["] = 17;

  return opp;
}
// Builds the opPrecedence map only once:
OppMap_t calculator::_opPrecedence = calculator::buildOpPrecedence();

// Check for unary operators and "convert" them to binary:
void calculator::handle_unary(const std::string& str,
    TokenQueue_t& rpnQueue, bool& lastTokenWasOp,
    OppMap_t& opPrecedence) {
  if (lastTokenWasOp) {
    // Convert unary operators to binary in the RPN.
    if (!str.compare("-") || !str.compare("+")) {
      rpnQueue.push(new Token<double>(0, NUM));
    } else {
      throw std::domain_error(
          "Unrecognized unary operator: '" + str + "'.");
    }
  }
}

// Consume operators with precedence >= than op then add op
void calculator::handle_op(const std::string& str,
    TokenQueue_t& rpnQueue,
    std::stack<std::string>& operatorStack,
    OppMap_t& opPrecedence) {
  while (!operatorStack.empty() &&
      opPrecedence[str] >= opPrecedence[operatorStack.top()]) {
    rpnQueue.push(new Token<std::string>(operatorStack.top(), OP));
    operatorStack.pop();
  }
  operatorStack.push(str);
}

#define isvariablechar(c) (isalpha(c) || c == '_')
TokenQueue_t calculator::toRPN(const char* expr,
    Scope scope, OppMap_t opPrecedence) {
  TokenQueue_t rpnQueue; std::stack<std::string> operatorStack;
  bool lastTokenWasOp = true;

  // In one pass, ignore whitespace and parse the expression into RPN
  // using Dijkstra's Shunting-yard algorithm.
  while (*expr && isspace(*expr )) ++expr;
  while (*expr ) {
    if (isdigit(*expr )) {
      // If the token is a number, add it to the output queue.
      char* nextChar = 0;
      double digit = strtod(expr , &nextChar);
      rpnQueue.push(new Token<double>(digit, NUM));
      expr = nextChar;
      lastTokenWasOp = false;
    } else if(isvariablechar(*expr) && lastTokenWasOp
      && operatorStack.size() > 0 && !operatorStack.top().compare(".")) {
      // If it is a member access key (e.g. `map.name`)
      // parse it and add to the output queue.
      std::stringstream ss;
      while( isvariablechar(*expr ) || isdigit(*expr) ) {
        ss << *expr;
        ++expr;
      }

      rpnQueue.push(new Token<std::string>(ss.str(), STR));
      lastTokenWasOp = false;
    } else if (isvariablechar(*expr )) {
      // If the function is a variable, resolve it and
      // add the parsed number to the output queue.
      std::stringstream ss;
      ss << *expr;
      ++expr;
      while( isvariablechar(*expr ) || isdigit(*expr) ) {
        ss << *expr;
        ++expr;
      }

      bool found = false;
      TokenBase* val = NULL;

      std::string key = ss.str();

      if(key == "true") {
        found = true; val = new Token<double>(1, NUM);
      } else if(key == "false") {
        found = true; val = new Token<double>(0, NUM);
      } else {
        val = scope.find(key);
        if(val) {
          val = val->clone();
          found = true;
        }
      }

      if (found) {
        // Save the token
        rpnQueue.push(val);
      } else {
        // Save the variable name:
        rpnQueue.push(new Token<std::string>(key, VAR));
      }

      lastTokenWasOp = false;
    } else if(*expr == '\'' || *expr == '"') {
      // If it is a string literal, parse it and
      // add to the output queue.
      char quote = *expr;

      ++expr;
      std::stringstream ss;
      while(*expr && *expr != quote && *expr != '\n') {
        if(*expr == '\\') ++expr;
        ss << *expr;
        ++expr;
      }

      if(*expr != quote) {
        std::string squote = (quote == '"' ? "\"": "'");
        throw syntax_error(
          "Expected quote (" + squote +
          ") at end of string declaration: " + squote + ss.str() + ".");
      }
      ++expr;
      rpnQueue.push(new Token<std::string>(ss.str(), STR));
      lastTokenWasOp = false;
    } else {
      // Otherwise, the variable is an operator or paranthesis.
      switch (*expr) {
        case '(':
          operatorStack.push("(");
          ++expr;
          break;
        case '[':
          // This counts as a bracket and as an operator:
          handle_unary("[]", rpnQueue, lastTokenWasOp, opPrecedence);
          handle_op("[]", rpnQueue, operatorStack, opPrecedence);
          // Add it as a bracket to the op stack:
          operatorStack.push("[");
          ++expr;
          break;
        case ')':
          while (operatorStack.top().compare("(")) {
            rpnQueue.push(new Token<std::string>(operatorStack.top(), OP));
            operatorStack.pop();
          }
          operatorStack.pop();
          ++expr;
          break;
        case ']':
          while (operatorStack.top().compare("[")) {
            rpnQueue.push(new Token<std::string>(operatorStack.top(), OP));
            operatorStack.pop();
          }
          operatorStack.pop();
          ++expr;
          break;
        default:
          {
            // The token is an operator.
            //
            // Let p(o) denote the precedence of an operator o.
            //
            // If the token is an operator, o1, then
            //   While there is an operator token, o2, at the top
            //       and p(o1) <= p(o2), then
            //     pop o2 off the stack onto the output queue.
            //   Push o1 on the stack.
            std::stringstream ss;
            ss << *expr;
            ++expr;
            while (*expr && !isspace(*expr ) && !isdigit(*expr )
                && !isvariablechar(*expr) && *expr != '(' && *expr != ')') {
              ss << *expr;
              ++expr;
            }
            ss.clear();
            std::string str;

            ss >> str;

            handle_unary(str, rpnQueue, lastTokenWasOp, opPrecedence);

            handle_op(str, rpnQueue, operatorStack, opPrecedence);

            lastTokenWasOp = true;
          }
      }
    }
    while (*expr && isspace(*expr )) ++expr;
  }
  while (!operatorStack.empty()) {
    rpnQueue.push(new Token<std::string>(operatorStack.top(), OP));
    operatorStack.pop();
  }
  return rpnQueue;
}

TokenBase* calculator::calculate(const char* expr) {
  return calculate(expr, Scope());
}

TokenBase* calculator::calculate(const char* expr, Scope local) {

  // Convert to RPN with Dijkstra's Shunting-yard algorithm.
  TokenQueue_t rpn = toRPN(expr, local);
  TokenBase* ret;

  try {
    ret = calculate(rpn, local);
  } catch (std::exception e) {
    cleanRPN(rpn);
    throw e;
  }

  cleanRPN(rpn);
  return ret;
}

TokenBase* calculator::calculate(TokenQueue_t _rpn, Scope scope) {

  TokenQueue_t rpn;

  // Deep copy the token list, so everything can be
  // safely deallocated:
  while(!_rpn.empty()) {
    TokenBase* base = _rpn.front();
    _rpn.pop();
    rpn.push(base->clone());
  }

  // Evaluate the expression in RPN form.
  std::stack<TokenBase*> evaluation;
  while (!rpn.empty()) {
    TokenBase* base = rpn.front();
    rpn.pop();

    // Operator:
    if (base->type == OP) {
      Token<std::string>* strTok = static_cast<Token<std::string>*>(base);
      std::string str = strTok->val;
      if (evaluation.size() < 2) {
        throw std::domain_error("Invalid equation.");
      }
      TokenBase* b_right = evaluation.top(); evaluation.pop();
      TokenBase* b_left  = evaluation.top(); evaluation.pop();
      if(b_left->type == NUM && b_right->type == NUM) {
        double left = static_cast<Token<double>*>(b_left)->val;
        double right = static_cast<Token<double>*>(b_right)->val;
        delete b_left;
        delete b_right;

        if (!str.compare("+")) {
          evaluation.push(new Token<double>(left + right, NUM));
        } else if (!str.compare("*")) {
          evaluation.push(new Token<double>(left * right, NUM));
        } else if (!str.compare("-")) {
          evaluation.push(new Token<double>(left - right, NUM));
        } else if (!str.compare("/")) {
          evaluation.push(new Token<double>(left / right, NUM));
        } else if (!str.compare("<<")) {
          evaluation.push(new Token<double>((int) left << (int) right, NUM));
        } else if (!str.compare("^")) {
          evaluation.push(new Token<double>(pow(left, right), NUM));
        } else if (!str.compare(">>")) {
          evaluation.push(new Token<double>((int) left >> (int) right, NUM));
        } else if (!str.compare("%")) {
          evaluation.push(new Token<double>((int) left % (int) right, NUM));
        } else if (!str.compare("<")) {
          evaluation.push(new Token<double>(left < right, NUM));
        } else if (!str.compare(">")) {
          evaluation.push(new Token<double>(left > right, NUM));
        } else if (!str.compare("<=")) {
          evaluation.push(new Token<double>(left <= right, NUM));
        } else if (!str.compare(">=")) {
          evaluation.push(new Token<double>(left >= right, NUM));
        } else if (!str.compare("==")) {
          evaluation.push(new Token<double>(left == right, NUM));
        } else if (!str.compare("!=")) {
          evaluation.push(new Token<double>(left != right, NUM));
        } else if (!str.compare("&&")) {
          evaluation.push(new Token<double>((int) left && (int) right, NUM));
        } else if (!str.compare("||")) {
          evaluation.push(new Token<double>((int) left || (int) right, NUM));
        } else {
          throw std::domain_error("Unknown operator: '" + str + "'.");
        }
      } else if(b_left->type == STR && b_right->type == STR) {
        std::string left = static_cast<Token<std::string>*>(b_left)->val;
        std::string right = static_cast<Token<std::string>*>(b_right)->val;
        delete b_left;
        delete b_right;

        if (!str.compare("+")) {
          evaluation.push(new Token<std::string>(left + right, STR));
        } else if (!str.compare("==")) {
          evaluation.push(new Token<double>(left.compare(right) == 0, NUM));
        } else if (!str.compare("!=")) {
          evaluation.push(new Token<double>(left.compare(right) != 0, NUM));
        } else {
          throw std::domain_error("Unknown operator: '" + str + "'.");
        }
      } else if(b_left->type == STR && b_right->type == NUM) {
        std::string left = static_cast<Token<std::string>*>(b_left)->val;
        double right = static_cast<Token<double>*>(b_right)->val;
        delete b_left;
        delete b_right;

        std::stringstream ss;
        if (!str.compare("+")) {
          ss << left << right;
          evaluation.push(new Token<std::string>(ss.str(), STR));
        } else {
          throw std::domain_error("Unknown operator: '" + str + "'.");
        }
      } else if(b_left->type == NUM && b_right->type == STR) {
        double left = static_cast<Token<double>*>(b_left)->val;
        std::string right = static_cast<Token<std::string>*>(b_right)->val;
        delete b_left;
        delete b_right;

        std::stringstream ss;
        if (!str.compare("+")) {
          ss << left << right;
          evaluation.push(new Token<std::string>(ss.str(), STR));
        } else {
          throw std::domain_error("Unknown operator: '" + str + "'.");
        }
      } else if(b_left->type == MAP && b_right->type == STR) {
        TokenMap_t* left = static_cast<Token<TokenMap_t*>*>(b_left)->val;
        std::string right = static_cast<Token<std::string>*>(b_right)->val;
        delete b_left;
        delete b_right;

        if (!str.compare("[]") || !str.compare(".")) {
          TokenMap_t::iterator it = left->find(right);

          if (it == left->end()) {
            throw std::domain_error(
                "Unable to find the variable '" + right + "'.");
          }

          evaluation.push(it->second->clone());
        } else {
          throw std::domain_error("Unknown operator: '" + str + "'.");
        }
      }
    } else if (base->type == VAR) { // Variable

      if (scope.size() == 0) {
        throw std::domain_error(
            "Detected variable, but the variable map is unavailable.");
      }

      TokenBase* value = NULL;
      std::string key = static_cast<Token<std::string>*>(base)->val;
      delete base;

      value = scope.find(key);
      if(value) value = value->clone();

      if (value == NULL) {
        throw std::domain_error(
            "Unable to find the variable '" + key + "'.");
      }
      evaluation.push(value);
    } else {
      evaluation.push(base);
    }
  }

  return evaluation.top();
}

void calculator::cleanRPN(TokenQueue_t& rpn) {
  while( rpn.size() ) {
    delete rpn.front();
    rpn.pop();
  }
}

/* * * * * Non Static Functions * * * * */

calculator::~calculator() {
  cleanRPN(this->RPN);
}

calculator::calculator(const calculator& calc) {
  this->scope = calc.scope;
  TokenQueue_t _rpn = calc.RPN;

  // Deep copy the token list, so everything can be
  // safely deallocated:
  while(!_rpn.empty()) {
    TokenBase* base = _rpn.front();
    _rpn.pop();
    this->RPN.push(base->clone());
  }
}

calculator::calculator(const char* expr,
    Scope scope, OppMap_t opPrecedence) {
  compile(expr, scope, opPrecedence);
}

void calculator::compile(const char* expr, OppMap_t opPrecedence) {
  this->RPN = calculator::toRPN(expr, scope, opPrecedence);
}

void calculator::compile(const char* expr,
    Scope local, OppMap_t opPrecedence) {

  // Make sure it is empty:
  cleanRPN(this->RPN);

  scope.push(local);

  try {
    this->RPN = calculator::toRPN(expr, scope, opPrecedence);
  } catch (std::exception e) {
    scope.pop(local.size());
    throw e;
  }

  scope.pop(local.size());
}

TokenBase* calculator::eval(Scope local) {
  scope.push(local);

  TokenBase* val;

  try {
    val = calculate(this->RPN, scope);
  } catch(std::exception e) {
    scope.pop(local.size());
    throw e;
  }
  scope.pop(local.size());

  return val;
}

calculator& calculator::operator=(const calculator& calc) {
  this->scope = calc.scope;

  // Make sure the RPN is empty:
  cleanRPN(this->RPN);

  // Deep copy the token list, so everything can be
  // safely deallocated:
  TokenQueue_t _rpn = calc.RPN;
  while(!_rpn.empty()) {
    TokenBase* base = _rpn.front();
    _rpn.pop();
    this->RPN.push(base->clone());
  }
  return *this;
}

/* * * * * For Debug Only * * * * */

std::string calculator::str() {
  std::stringstream ss;
  TokenQueue_t rpn = this->RPN;

  ss << "calculator { RPN: [ ";
  while( rpn.size() ) {
    TokenBase* base = rpn.front();
    rpn.pop();

    if(base->type == NUM) {
      ss << static_cast<Token<double>*>(base)->val;
    } else if(base->type == VAR) {
      ss << static_cast<Token<std::string>*>(base)->val;
    } else if(base->type == OP) {
      ss << static_cast<Token<std::string>*>(base)->val;
    } else if(base->type == NONE) {
      ss << "None";
    } else {
      ss << "unknown_type";
    }

    ss << (rpn.size() ? ", ":"");
  }
  ss << " ] }";
  return ss.str();
}

/* * * * * Scope Class: * * * * */

Scope::Scope(TokenMap_t* vars) {
  if(vars) scope.push_front(vars);
}

TokenBase* Scope::find(std::string key) {
  TokenBase* value = NULL;

  Scope_t::iterator s_it = scope.begin();
  for(; s_it != scope.end(); s_it++) {
    TokenMap_t::iterator it = (*s_it)->find(key);
    if(it != (*s_it)->end()) {
      value = it->second->clone();
      break;
    }
  }

  return value;
}

void Scope::push(TokenMap_t* vars) {
  if(vars) scope.push_front(vars);
}

void Scope::push(Scope other) {
  Scope_t::reverse_iterator s_it = other.scope.rbegin();
  for(; s_it != other.scope.rend(); s_it++) {
    push(*s_it);
  }
}

void Scope::pop() {
  if(scope.size() == 0)
    throw std::range_error(
      "Calculator::drop_namespace(): No namespace left to drop!");
  scope.pop_front();
}

// Pop the N top elements:
void Scope::pop(unsigned N) {
  for(unsigned i=0; i < N; i++) {
    pop();
  }
}

void Scope::clean() {
  scope = Scope_t();
}

unsigned Scope::size() {
  return scope.size();
}
