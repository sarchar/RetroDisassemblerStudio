#include <iostream>
#include <memory>
#include <string>

#include "util.h"

#include "systems/nes/comment.h"
#include "systems/nes/defines.h"
#include "systems/nes/enum.h"
#include "systems/nes/expressions.h"
#include "systems/nes/label.h"
#include "systems/nes/system.h"

#include "windows/nes/project.h"

using namespace std;

namespace Systems::NES {

shared_ptr<BaseExpression> Comment::GetExpression(string const& s, string& errmsg) const 
{
    auto expr = make_shared<Expression>();
    
    string errmsg2;
    int errloc;
    if(!expr->Set(s, errmsg2, errloc)) {
        stringstream ss;
        ss << errmsg2 << " (offset " << errloc << ")";
        errmsg = ss.str();
        return nullptr;
    }

    // Fixup the expression to evaluate labels, defines, enums
    int fixup_flags = FIXUP_DEFINES | FIXUP_ENUMS | FIXUP_LABELS | FIXUP_LONG_LABELS;
    if(!GetSystem()->FixupExpression(expr, errmsg2, fixup_flags)) {
        stringstream ss;
        ss << errmsg2 << " (offset " << errloc << ")";
        errmsg = ss.str();
        return nullptr;
    }
    return expr;
}

shared_ptr<BaseExpression> Comment::NewExpression() const 
{
    return make_shared<Expression>();
}

void Comment::NoteReferences()
{
    // Explore expression and mark references
    auto cb = [this](shared_ptr<BaseExpressionNode>& node, shared_ptr<BaseExpressionNode> const&, int, void*)->bool {
        if(auto define_node = dynamic_pointer_cast<ExpressionNodes::Define>(node)) {
            define_node->GetDefine()->NoteReference(shared_from_this());
        } else if(auto ee_node = dynamic_pointer_cast<ExpressionNodes::EnumElement>(node)) {
            ee_node->GetEnumElement()->NoteReference(shared_from_this());
        } else if(auto label_node = dynamic_pointer_cast<ExpressionNodes::Label>(node)) {
            // tell the expression node to update the reference to the label
            label_node->Update();

            // there might not be a label at the given address
            auto label = label_node->GetLabel();
            if(label) label->NoteReference(shared_from_this());

            // watch for label changes at the target address
            cout << "TODO !!" << endl;
        }
        return true;
    };

    for(auto& comment_line: comment_lines) {
        for(auto& line_item: comment_line) {
            if(auto pexpr = get_if<shared_ptr<BaseExpression>>(&line_item)) {
                auto& expr = *pexpr;
                if(!expr->Explore(cb, nullptr)) assert(false); // false return shouldn't happen
            }
        }
    }
}

void Comment::ClearReferences()
{
    // Explore expression and mark each referenced Define and Enum that we're no longer referring to it
    auto cb = [this](shared_ptr<BaseExpressionNode>& node, shared_ptr<BaseExpressionNode> const&, int, void*)->bool {
        if(auto define_node = dynamic_pointer_cast<ExpressionNodes::Define>(node)) {
            define_node->GetDefine()->RemoveReference(shared_from_this());
        } else if(auto ee_node = dynamic_pointer_cast<ExpressionNodes::EnumElement>(node)) {
            ee_node->GetEnumElement()->RemoveReference(shared_from_this());
        } else if(auto label_node = dynamic_pointer_cast<ExpressionNodes::Label>(node)) {
            label_node->GetLabel()->RemoveReference(shared_from_this());
        }
        return true;
    };

    for(auto& comment_line: comment_lines) {
        for(auto& line_item: comment_line) {
            if(auto pexpr = get_if<shared_ptr<BaseExpression>>(&line_item)) {
                auto& expr = *pexpr;
                if(!expr->Explore(cb, nullptr)) assert(false); // false return shouldn't happen
            }
        }
    }
}

shared_ptr<Comment> Comment::Load(istream& is, string& errmsg)
{
    auto comment = make_shared<Comment>();
    if(!comment->BaseComment::Load(is, errmsg)) return nullptr;
    return comment;
}

}
