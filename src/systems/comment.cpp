// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#include <iostream>
#include <memory>
#include <string>

#include "util.h"

#include "systems/comment.h"
#include "systems/expressions.h"

using namespace std;

namespace Systems {

BaseComment::BaseComment()
{
}

BaseComment::~BaseComment()
{
}

void BaseComment::Set(string const& s)
{
    errored = false;

    // clear the old comment
    comment_lines.clear();

    // break up `s` into lines and parse individual lines
    int start = 0;
    while(start < s.size()) {
        int end = s.find("\n", start);
        if(end == string::npos) end = s.size();
        ParseLine(s.substr(start, end-start));
        start = end + 1; // skip \n
    }

    // only save the full_comment_text if we encounter an error
    full_comment_text = errored ? s : "";
}

void BaseComment::ParseLine(string const& s)
{
    comment_line_t comment_line;

    // repeatedly look for "{" (but not "{{"!)
    int string_start = 0;
    int search_start = 0;
    while(string_start < s.size()) {
        int expression_start = s.find("{", search_start);
        if(expression_start == string::npos) { // not found, everything to end of string is text
            line_item_t text_item = s.substr(string_start);
            comment_line.push_back(text_item);
            break; // done
        } else {
            int expression_end = string::npos;

            // find the end of the expression but skip {{
            if(expression_start + 1 < s.size()) {
                if(s[expression_start+1] == '{') {
                    search_start = expression_start + 2;
                    continue;
                }

                // look for }
                expression_end = s.find("}", expression_start+1);
            }

            if(expression_end == string::npos) { // no {
                // add text up to the start of the expression
                comment_line.push_back(s.substr(string_start, expression_start - string_start));
                // add an error message
                comment_line.push_back(ExpressionError{"Missing '}'"});
                errored = true;
                break;
            }

            // add the text up to the { as an element
            comment_line.push_back(s.substr(string_start, expression_start - string_start));

            // get the expression string
            string expression_string = s.substr(expression_start+1, expression_end-expression_start-1);

            // try parsing the expression
            string errmsg;
            auto expr = GetExpression(expression_string, errmsg);
            if(!expr) {
                // error parsing but continue looking for other expressions
                comment_line.push_back(ExpressionError{errmsg});
                errored = true;
            } else {
                // success!
                comment_line.push_back(expr);
            }

            // continue hunting for more text
            string_start = expression_end + 1;
            search_start = string_start;
        }
    }

    comment_lines.push_back(comment_line);
}

BaseComment::LINE_ITEM_TYPE BaseComment::FormatLineItem(int i, int j, string& out, 
        bool evaluate_expression, s64* _result) const
{
    auto const& line_item = comment_lines[i][j];

    if(auto pstr = get_if<string>(&line_item)) {
        out = *pstr;
        return LINE_ITEM_TYPE::STRING;
    } else if(auto pexpr = get_if<shared_ptr<BaseExpression>>(&line_item)) {
        auto const& expr = *pexpr;
        if(evaluate_expression) {
            s64 result;
            string errmsg;
            if(!expr->Evaluate(&result, errmsg)) {
                out = errmsg;
                return LINE_ITEM_TYPE::ERROR;
            }

            stringstream ss;
            ss << "$" << hex << setw(4) << setfill('0') << result;
            out = ss.str();

            if(_result) *_result = result;
            return LINE_ITEM_TYPE::EXPRESSION;
        }

        stringstream ss;
        ss << *expr;
        out = ss.str();
        return LINE_ITEM_TYPE::EXPRESSION;
    } else if(auto perror = get_if<ExpressionError>(&line_item)) {
        out = (*perror).text;
        return LINE_ITEM_TYPE::ERROR;
    } else {
        assert(false);
        out = "";
        return LINE_ITEM_TYPE::ERROR;
    }
}

void BaseComment::GetFullCommentText(string& out) const 
{ 
    if(errored) {
        out = full_comment_text;
        return;
    }

    stringstream ss;
    for(int i = 0; i < comment_lines.size(); i++) {
        for(int j = 0; j < comment_lines[i].size(); j++) {
            string s;
            auto t = FormatLineItem(i, j, s, false);
           
            if(t == LINE_ITEM_TYPE::EXPRESSION) {
                ss << '{' << s << '}';
            } else {
                ss << s;
            }
        }

        if(i != comment_lines.size() - 1) ss << '\n';
    }

    out = ss.str();
}

bool BaseComment::Save(ostream& os, string& errmsg) const
{
    WriteString(os, full_comment_text);
    if(!WriteVector(os, errmsg, comment_lines)) return false;
    errmsg = "Error saving BaseComment";
    return os.good();
}

bool BaseComment::SaveLineItem(ostream& os, string& errmsg, line_item_t const& line_item)
{
    if(auto ptext = get_if<string>(&line_item)) {
        char c = 'T';
        os.write((char*)&c, 1);
        WriteString(os, *ptext);
        errmsg = "Error saving line_item_t::string";
        return os.good();
    } else if(auto pexpr = get_if<shared_ptr<BaseExpression>>(&line_item)) {
        char c = 'E';
        os.write((char*)&c, 1);
        if(!(*pexpr)->Save(os, errmsg)) return false;
        return true;
    } else if(auto perror = get_if<ExpressionError>(&line_item)) {
        char c = 'R';
        os.write((char*)&c, 1);
        WriteString(os, perror->text);
        errmsg = "Error saving line_item_t::ExpressionError";
        return os.good();
    }

    assert(false);
    return false;
}

bool BaseComment::Load(istream& is, string& errmsg)
{
    // full_comment_text having content means there was a parse error
    ReadString(is, full_comment_text);
    if(full_comment_text.size()) errored = true;
    errmsg = "Error loading BaseComment";
    if(!is.good()) return false;
    if(!ReadVector(is, errmsg, comment_lines, (void*)this)) return false;
    return true;
}

bool BaseComment::LoadLineItem(istream& is, string& errmsg, line_item_t& line_item)
{
    char c;
    is.read((char*)&c, 1);
    switch(c) {
    case 'T': {
        string s;
        ReadString(is, s);
        line_item = s;
        break;
    }
    case 'E': {
        auto expr = NewExpression();
        if(!expr->Load(is, errmsg)) return false;
        line_item = expr;
        break;
    }
    case 'R': {
        string s;
        ReadString(is, s);
        line_item = ExpressionError{s};
        break;
    }
    default:
        assert(false);
        return false;
    }
    return true;
}

ostream& operator<<(ostream& stream, BaseComment const& comment)
{
    ios_base::fmtflags saveflags(stream.flags());

    for(int i = 0; i < comment.comment_lines.size(); i++) {
        auto& line = comment.comment_lines[i];
        stream << dec << (i+1) << "a: ";

        for(auto line_item : line) {
            if(auto text = get_if<string>(&line_item)) {
                cout << *text;
            } else if(auto expr = get_if<shared_ptr<BaseExpression>>(&line_item)) {
                cout << *(*expr);
            } else if(auto err = get_if<BaseComment::ExpressionError>(&line_item)) {
                cout << "|" << err->text << "|";
            }
        }

        stream << endl << dec << (i+1) << "b: ";

        for(auto line_item : line) {
            if(auto text = get_if<string>(&line_item)) {
                cout << *text;
            } else if(auto pexpr = get_if<shared_ptr<BaseExpression>>(&line_item)) {
                auto expr = *pexpr;
                s64 result;
                string errmsg;
                if(expr->Evaluate(&result, errmsg)) {
                    cout << hex << "$" << result;
                } else {
                    cout << "`" << errmsg << "`";
                }
            } else if(auto err = get_if<BaseComment::ExpressionError>(&line_item)) {
                cout << "|" << err->text << "|";
            }
        }

        stream << endl;
    }

    stream.flags(saveflags);
    return stream;
}


}


