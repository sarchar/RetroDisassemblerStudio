#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "util.h"

class BaseExpression;

namespace Systems {

class BaseComment : public std::enable_shared_from_this<BaseComment> {
public:
    struct ExpressionError {
        std::string text;
    };

    enum class LINE_ITEM_TYPE { STRING, EXPRESSION, ERROR };

    typedef std::variant<std::string, 
                         std::shared_ptr<BaseExpression>,
                         ExpressionError> line_item_t;
    typedef std::vector<line_item_t> comment_line_t;

    BaseComment();
    virtual ~BaseComment();

    int GetLineCount() const { return comment_lines.size(); }
    int GetLineItemCount(int i) const { return comment_lines[i].size(); }
    LINE_ITEM_TYPE FormatLineItem(int i, int j, std::string&, bool, s64* _result = nullptr) const;
    void GetFullCommentText(std::string&) const;

    void Set(std::string const&);
    virtual std::shared_ptr<BaseExpression> GetExpression(std::string const&, std::string&) const = 0;
    virtual std::shared_ptr<BaseExpression> NewExpression() const = 0;

    virtual void NoteReferences() = 0;
    virtual void ClearReferences() = 0;

    virtual bool Save(std::ostream&, std::string&) const;
    virtual bool Load(std::istream&, std::string&);

    static bool SaveLineItem(std::ostream& os, std::string& errmsg, line_item_t const& line_item);
    bool LoadLineItem(std::istream& is, std::string& errmsg, line_item_t& line_item);

    friend std::ostream& operator<<(std::ostream&, BaseComment const&);

    // for referenceable, comments are only equal if they're the actual comment object
    // since everything else in the state can be exactly equal
    bool operator==(BaseComment const& other) {
        return this == &other;
    }

protected:
    std::vector<comment_line_t> comment_lines;

private:
    void ParseLine(std::string const&);
    std::string full_comment_text;
    bool errored = false;
};

}

template<>
inline bool WriteVectorElement<Systems::BaseComment::line_item_t>
    (std::ostream& os, std::string& errmsg, Systems::BaseComment::line_item_t const& line_item)
{
    return Systems::BaseComment::SaveLineItem(os, errmsg, line_item);
}

template<>
inline bool ReadVectorElement<Systems::BaseComment::line_item_t>
    (std::istream& is, std::string& errmsg, Systems::BaseComment::line_item_t& line_item,
     void* userdata)
{
    auto comment = static_cast<Systems::BaseComment*>(userdata);
    return comment->LoadLineItem(is, errmsg, line_item);
}


