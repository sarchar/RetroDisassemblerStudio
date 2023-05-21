#include <functional>
#include <cctype>

#include "magic_enum.hpp"
#include "systems/expressions.h"

using namespace std;

Tenderizer::Tenderizer(istream& _input_stream)
    : input_stream(_input_stream), current_meat(Meat::_HUNGRY), location(0)
{
    Gobble();
}

void Tenderizer::Gobble() // gobble, gobble
{
    if(Finished()) return; // no more tokens

    display_text.str(""); // clear text
    display_text.clear(); // clear state/flags
    meat_text.str("");
    meat_text.clear();

    char c = Peck();
    if(!input_stream) {
        current_meat = Meat::END;
        return;
    }

    // record the food
    meat_text << c;

    // NAME :: first letter must be alpha or _.
    if(isalpha(c) || c == '_' || c == '.') {
        // Eat! Look, bite, look, bite!
        c = Look();
        while(isalnum(c) || c == '_') {
            meat_text << Bite();
            c = Look();
        }

        current_meat = Meat::NAME;
        return;
    }

    // NUMBER :: decimal, binary, and hex, and allow underscores for clarity
    if(isdigit(c) || c == '$' || c == '%') {
        bool is_hex = (c == '$');
        bool is_bin = (c == '%');

        // Try to eat up only a number!
        c = Look();
        while((is_bin && (c == '0' || c == '1'))
            || (is_hex && isxdigit(c))
            || (!is_bin && !is_hex && isdigit(c))
            || c == '_') { 
            meat_text << Bite();
            c = Look();
        }

        current_meat = Meat::CONSTANT;
        return;
    }

    if((c == '<' || c == '>') && (Look() == c)) { // LSHIFT and RSHIFT
        meat_text << Bite();
        current_meat = (c == '<') ? Meat::LSHIFT : Meat::RSHIFT;
    } else if(c == '*' && Look() == '*') { // POWER
        meat_text << Bite();
        current_meat = Meat::POWER;
    /* } else if(other items) { */
    } else {
        // single letter meat items
        switch(c) {
        case '+': current_meat = Meat::PLUS      ; break;
        case '-': current_meat = Meat::MINUS     ; break;
        case '*': current_meat = Meat::ASTERISK  ; break;
        case '/': current_meat = Meat::SLASH     ; break;
        case '(': current_meat = Meat::LPAREN    ; break;
        case ')': current_meat = Meat::RPAREN    ; break;
        case '^': current_meat = Meat::CARET     ; break;
        case '|': current_meat = Meat::PIPE      ; break;
        case '&': current_meat = Meat::AMPERSAND ; break;
        case '~': current_meat = Meat::TILDE     ; break;
        case '<': current_meat = Meat::LANGLE    ; break;
        case '>': current_meat = Meat::RANGLE    ; break;
        case '!': current_meat = Meat::BANG      ; break;
        case ',': current_meat = Meat::COMMA     ; break;
        case '#': current_meat = Meat::HASH      ; break;
        default:
            current_meat = Meat::YUCKY;
            return;
        }
    }

    // yummy
    Satisfied();
}

std::vector<std::shared_ptr<BaseExpressionNodeCreator::BaseExpressionNodeInfo>> BaseExpressionNodeCreator::expression_nodes;
int BaseExpressionNodeCreator::expression_node_id_offset = 0;

BaseExpressionNode::BaseExpressionNode()
{
}

BaseExpressionNode::~BaseExpressionNode()
{
}

std::ostream& operator<<(std::ostream& stream, BaseExpressionNode& node)
{
    std::ios_base::fmtflags saveflags(stream.flags());

    node.Print(stream);

    stream.flags(saveflags);
    return stream;
}

BaseExpressionNodeCreator::BaseExpressionNodeCreator()
{
}

BaseExpressionNodeCreator::~BaseExpressionNodeCreator()
{
}

void BaseExpressionNodeCreator::RegisterBaseExpressionNodes()
{
    RegisterBaseExpressionNode<BaseExpressionNodes::Constant<u8>>();
    RegisterBaseExpressionNode<BaseExpressionNodes::Constant<s8>>();
    RegisterBaseExpressionNode<BaseExpressionNodes::Constant<u16>>();
    RegisterBaseExpressionNode<BaseExpressionNodes::Constant<s16>>();
    RegisterBaseExpressionNode<BaseExpressionNodes::Constant<u32>>();
    RegisterBaseExpressionNode<BaseExpressionNodes::Constant<s32>>();
    RegisterBaseExpressionNode<BaseExpressionNodes::Constant<u64>>();
    RegisterBaseExpressionNode<BaseExpressionNodes::Constant<s64>>();
    RegisterBaseExpressionNode<BaseExpressionNodes::Name>();
    RegisterBaseExpressionNode<BaseExpressionNodes::AddOp>();
    RegisterBaseExpressionNode<BaseExpressionNodes::SubtractOp>();
    RegisterBaseExpressionNode<BaseExpressionNodes::MultiplyOp>();
    RegisterBaseExpressionNode<BaseExpressionNodes::DivideOp>();
    RegisterBaseExpressionNode<BaseExpressionNodes::PowerOp>();

    RegisterBaseExpressionNode<BaseExpressionNodes::OrOp>();
    RegisterBaseExpressionNode<BaseExpressionNodes::XorOp>();
    RegisterBaseExpressionNode<BaseExpressionNodes::AndOp>();
    RegisterBaseExpressionNode<BaseExpressionNodes::LShiftOp>();
    RegisterBaseExpressionNode<BaseExpressionNodes::RShiftOp>();

    RegisterBaseExpressionNode<BaseExpressionNodes::PositiveOp>();
    RegisterBaseExpressionNode<BaseExpressionNodes::NegateOp>();
    RegisterBaseExpressionNode<BaseExpressionNodes::BinaryNotOp>();
    RegisterBaseExpressionNode<BaseExpressionNodes::LogicalNotOp>();

    RegisterBaseExpressionNode<BaseExpressionNodes::FunctionCall>();
    RegisterBaseExpressionNode<BaseExpressionNodes::ExpressionList>();

    // any node registered after this point is a subclassed node. using an ID
    // offset lets us add new base nodes without corrupting the subnode indexes in save files
#define SUBCLASS_NODE_ID_OFFSET 128
    expression_node_id_offset = SUBCLASS_NODE_ID_OFFSET;
}

bool BaseExpressionNodeCreator::Save(shared_ptr<BaseExpressionNode> const& node, ostream& os, string& errmsg)
{
    WriteVarInt(os, node->GetExpressionNodeType());
    if(!os.good()) return false;
    return node->Save(os, errmsg, shared_from_this());
}

std::shared_ptr<BaseExpressionNode> BaseExpressionNodeCreator::Load(std::istream& is, std::string& errmsg)
{
    u32 node_type = ReadVarInt<u32>(is);
    if(!is.good()) {
        errmsg = "Error reading expression node";
        return nullptr;
    }

    // subtract out subclass node id offset
    if(node_type >= SUBCLASS_NODE_ID_OFFSET) node_type -= SUBCLASS_NODE_ID_OFFSET;

    if(node_type >= expression_nodes.size()) {
        errmsg = "Invalid expression node type";
        return nullptr;
    }

    auto ptr = shared_from_this();
    return expression_nodes[node_type]->load(is, errmsg, ptr);
}

BaseExpression::BaseExpression()
{
}

BaseExpression::~BaseExpression()
{
}

// Parse the expression in 's' and set it to the root node
bool BaseExpression::Set(std::string const& s, std::string& errmsg, int& errloc, bool start_list)
{
    errmsg = "";
    errloc = 0;
    parens_depth = 0;

    istringstream iss{s};
    shared_ptr<Tenderizer> tenderizer = make_shared<Tenderizer>(iss);

#if 0 // testing
    cout << "meal: " << s << endl;
    while(!tenderizer->Finished()) {
        auto m = tenderizer->GetCurrentMeat();
        cout << "\tmeat: " << magic_enum::enum_name(m) << " display: \"" << tenderizer->GetDisplayText() << "\" text: \"" << tenderizer->GetMeatText() << "\"" << endl;
        tenderizer->Gobble();
    }

    iss = istringstream{s};
    tenderizer = make_shared<Tenderizer>(iss);
#endif

    auto node_creator = GetNodeCreator();

    // the user can disable the initial expression list and limit it to a single expression
    root = start_list ? ParseExpressionList(tenderizer, node_creator, errmsg, errloc) 
                      : ParseExpression(tenderizer, node_creator, errmsg, errloc);

    // If there's leftover data, the expression is invalid
    if(root && (tenderizer->GetCurrentMeat() != Tenderizer::Meat::END)) {
        errmsg = "Leftover data in the expression";
        errloc = tenderizer->GetLocation();
        root = nullptr;
    }

    if(!root) {
        cout << "[BaseExpression::Set] could not parse expression, pos " << dec << (errloc + 1) << ": " << errmsg << endl;
        cout << "expression -> \"" << s << "\"" << endl;
    }

#ifndef NDEBUG
    else { // valid root, so make sure it matches our input
        stringstream ts;
        ts << *this;
        assert(ts.str() == s);
    }
#endif

    return (bool)root;
}

bool BaseExpression::Explore(explore_callback_t explore_callback, void* userdata)
{
    if(root) {
        // explore down the tree first
        if(!root->Explore(explore_callback, 1, userdata)) return false;
        // and finally call the explore callback on the root node
        shared_ptr<BaseExpressionNode> nullnode;
        if(!explore_callback(root, nullnode, 0, userdata)) return false;
        return true;
    }

    return false;
}

// I have to thank Chris French for his excellent guide on writing a simple LL(1) parser. 
// As you can tell, my code largely follows the format of his, with some changes.
//
// Reference: https://unclechromedome.org/c++-tutorials/expression-parser/index.html
//
// Also for reference was the C++ precedence order:
// https://en.cppreference.com/w/cpp/language/operator_precedence
//
// Below starts my expression parser (the lexer is in the Tenderizer class).
//
// Expression list allows for instructions or functions with multiple arguments
// 
// expression_list: expression expression_list_tail
//                ;
//
// expression_list_tail: COMMA expression expression_list_tail
//                     | // nothing
//                     ;
shared_ptr<BaseExpressionNode> BaseExpression::ParseExpressionList(shared_ptr<Tenderizer>& tenderizer, shared_ptr<BaseExpressionNodeCreator>& node_creator, string& errmsg, int& errloc)
{
    vector<BaseExpressionNodes::BaseExpressionNodeListEntry> list;

    auto expr = ParseExpression(tenderizer, node_creator, errmsg, errloc);
    if(!expr) return nullptr;
    list.push_back(BaseExpressionNodes::BaseExpressionNodeListEntry{
        .display = "",
        .node    = expr
    });

    while(tenderizer->GetCurrentMeat() == Tenderizer::Meat::COMMA) {
        string display = tenderizer->GetDisplayText();
        tenderizer->Gobble();
        auto expr = ParseExpression(tenderizer, node_creator, errmsg, errloc);
        if(!expr) return nullptr;
        list.push_back(BaseExpressionNodes::BaseExpressionNodeListEntry{
            .display = display,
            .node    = expr
        });
    }

    if(list.size() == 1) return list[0].node;
    else return node_creator->CreateList(list);
}

// 
// Simple wrapper for clearity
// 
// expression: or_expr
//           ;
// 
shared_ptr<BaseExpressionNode> BaseExpression::ParseExpression(shared_ptr<Tenderizer>& tenderizer, shared_ptr<BaseExpressionNodeCreator>& node_creator, string& errmsg, int& errloc)
{
    return ParseOrExpression(tenderizer, node_creator, errmsg, errloc);
}

//TODO make all the Parsers virtual so that a subclass and interject its own precedence inbetween others
//this should allow the subclass to create system specific nodes like IndexedX. However, the tokenizer has to support all
//syntax for the entire program.
//
// OR (|) C++ precedence 13
//
// or_expr: xor_expr or_expr_tail
//           ;
// 
// or_expr_tail: PIPE xor_expr or_expr_tail
//                | // EMPTY
//                ;
//
shared_ptr<BaseExpressionNode> BaseExpression::ParseOrExpression(shared_ptr<Tenderizer>& tenderizer, shared_ptr<BaseExpressionNodeCreator>& node_creator, string& errmsg, int& errloc)
{
    auto lhs = ParseXorExpression(tenderizer, node_creator, errmsg, errloc);
    if(!lhs) return nullptr;

    while(tenderizer->GetCurrentMeat() == Tenderizer::Meat::PIPE) {
        string display = tenderizer->GetDisplayText();
        tenderizer->Gobble();
        auto rhs = ParseXorExpression(tenderizer, node_creator, errmsg, errloc);
        if(!rhs) return nullptr;
        lhs = node_creator->CreateOrOp(lhs, display, rhs);
    }

    return lhs;
}

// XOR (^) C++ precedence 12
// 
// xor_expr: and_expr xor_expr_tail
//            ;
// 
// xor_expr_tail: CARET and_expr xor_expr_tail
//                 | // EMPTY
//                 ;
// 
shared_ptr<BaseExpressionNode> BaseExpression::ParseXorExpression(shared_ptr<Tenderizer>& tenderizer, shared_ptr<BaseExpressionNodeCreator>& node_creator, string& errmsg, int& errloc)
{
    auto lhs = ParseAndExpression(tenderizer, node_creator, errmsg, errloc);
    if(!lhs) return nullptr;

    while(tenderizer->GetCurrentMeat() == Tenderizer::Meat::CARET) {
        string display = tenderizer->GetDisplayText();
        tenderizer->Gobble();
        auto rhs = ParseAndExpression(tenderizer, node_creator, errmsg, errloc);
        if(!rhs) return nullptr;
        lhs = node_creator->CreateXorOp(lhs, display, rhs);
    }

    return lhs;
}

// AND (&) C++ precedence 11
// 
// and_expr: shift_expr and_expr_tail
//            ;
// 
// and_expr_tail: AMPERSAND shift_expr and_expr_tail
//                 | // EMPTY
//                 ;
// 
shared_ptr<BaseExpressionNode> BaseExpression::ParseAndExpression(shared_ptr<Tenderizer>& tenderizer, shared_ptr<BaseExpressionNodeCreator>& node_creator, string& errmsg, int& errloc)
{
    auto lhs = ParseShiftExpression(tenderizer, node_creator, errmsg, errloc);
    if(!lhs) return nullptr;

    while(tenderizer->GetCurrentMeat() == Tenderizer::Meat::AMPERSAND) {
        string display = tenderizer->GetDisplayText();
        tenderizer->Gobble();
        auto rhs = ParseShiftExpression(tenderizer, node_creator, errmsg, errloc);
        if(!rhs) return nullptr;
        lhs = node_creator->CreateAndOp(lhs, display, rhs);
    }

    return lhs;
}

// BIT SHIFTS (<<, >>) C++ precedence 7
// 
// shift_expr: add_expr shift_expr_tail
//              ;
// 
// shift_expr_tail: LSHIFT add_expr shift_expr_tail
//                | RSHIFT add_expr shift_expr tail
//                | // EMPTY
//                ;
// 
shared_ptr<BaseExpressionNode> BaseExpression::ParseShiftExpression(shared_ptr<Tenderizer>& tenderizer, shared_ptr<BaseExpressionNodeCreator>& node_creator, string& errmsg, int& errloc)
{
    auto lhs = ParseAddExpression(tenderizer, node_creator, errmsg, errloc);
    if(!lhs) return nullptr;

    for(bool done = false; !done;) {
        string display = tenderizer->GetDisplayText();
        switch(tenderizer->GetCurrentMeat()) {
        case Tenderizer::Meat::LSHIFT:
        {
            tenderizer->Gobble();
            auto rhs = ParseAddExpression(tenderizer, node_creator, errmsg, errloc);
            if(!rhs) return nullptr;
            lhs = node_creator->CreateLShiftOp(lhs, display, rhs);
            break;
        }

        case Tenderizer::Meat::RSHIFT:
        {
            tenderizer->Gobble();
            auto rhs = ParseAddExpression(tenderizer, node_creator, errmsg, errloc);
            if(!rhs) return nullptr;
            lhs = node_creator->CreateRShiftOp(lhs, display, rhs);
            break;
        }

        default:
            done = true;
            break;
        }
    }

    return lhs;
}

//
// ADDS (+, -) C++ precedence 6
// 
// add_expr: mul_expr add_expr_tail
//         ;
// 
// add_expr_tail: PLUS mul_expr add_expr_tail
//              | MINUS mul_expr add_expr_tail
//              | // EMPTY
//              ;
//
shared_ptr<BaseExpressionNode> BaseExpression::ParseAddExpression(shared_ptr<Tenderizer>& tenderizer, shared_ptr<BaseExpressionNodeCreator>& node_creator, string& errmsg, int& errloc)
{
    auto lhs = ParseMulExpression(tenderizer, node_creator, errmsg, errloc);
    if(!lhs) return nullptr;

    for(bool done = false; !done;) {
        string display = tenderizer->GetDisplayText();
        switch(tenderizer->GetCurrentMeat()) {
        case Tenderizer::Meat::PLUS:
        {
            tenderizer->Gobble();
            auto rhs = ParseMulExpression(tenderizer, node_creator, errmsg, errloc);
            if(!rhs) return nullptr;
            lhs = node_creator->CreateAddOp(lhs, display, rhs);
            break;
        }

        case Tenderizer::Meat::MINUS:
        {
            tenderizer->Gobble();
            auto rhs = ParseMulExpression(tenderizer, node_creator, errmsg, errloc);
            if(!rhs) return nullptr;
            lhs = node_creator->CreateSubtractOp(lhs, display, rhs);
            break;
        }

        default:
            done = true;
            break;
        }
    }

    return lhs;
}

// 
// MULTIPLIES (*, /) C++ precedence 5
//
// mul_expr: pow_expr mul_expr_tail
//         ;
// 
// mul_expr_tail: MUL pow_expr mul_expr_tail
//              | DIV pow_expr mul_expr_tail
//              | MOD pow_expr mul_expr_tail  // TODO not implemented
//              | // EMPTY
//              ;
//
// TODO may want a modulo operator but might need to use the word MOD since '%' is gobbled by binary numbers like %0101_1001
//
shared_ptr<BaseExpressionNode> BaseExpression::ParseMulExpression(shared_ptr<Tenderizer>& tenderizer, shared_ptr<BaseExpressionNodeCreator>& node_creator, string& errmsg, int& errloc)
{
    auto lhs = ParsePowerExpression(tenderizer, node_creator, errmsg, errloc);
    if(!lhs) return nullptr;

    for(bool done = false; !done;) {
        string display = tenderizer->GetDisplayText();
        switch(tenderizer->GetCurrentMeat()) {
        case Tenderizer::Meat::ASTERISK:
        {
            tenderizer->Gobble();
            auto rhs = ParsePowerExpression(tenderizer, node_creator, errmsg, errloc);
            if(!rhs) return nullptr;
            lhs = node_creator->CreateMultiplyOp(lhs, display, rhs);
            break;
        }

        case Tenderizer::Meat::SLASH:
        {
            tenderizer->Gobble();
            auto rhs = ParsePowerExpression(tenderizer, node_creator, errmsg, errloc);
            if(!rhs) return nullptr;
            lhs = node_creator->CreateDivideOp(lhs, display, rhs);
            break;
        }

        default:
            done = true;
            break;
        }
    }

    return lhs;
}

// 
// pow_expr: unary_expr pow_expr_suffix
//         ;
// 
// pow_expr_suffix: POW unary_expr
//                | // EMPTY
//                ;
// 
shared_ptr<BaseExpressionNode> BaseExpression::ParsePowerExpression(shared_ptr<Tenderizer>& tenderizer, shared_ptr<BaseExpressionNodeCreator>& node_creator, string& errmsg, int& errloc)
{
    auto lhs = ParseUnaryExpression(tenderizer, node_creator, errmsg, errloc);
    if(!lhs) return nullptr;

    while(tenderizer->GetCurrentMeat() == Tenderizer::Meat::POWER) {
        string display = tenderizer->GetDisplayText();
        tenderizer->Gobble();
        auto rhs = ParseUnaryExpression(tenderizer, node_creator, errmsg, errloc);
        if(!rhs) return nullptr;
        lhs = node_creator->CreatePowerOp(lhs, display, rhs);
    }

    return lhs;
}

// PREFIX (+, -, ~, !) C++ precedence 3
//
// unary_expr:  PLUS primary
//     |        MINUS primary
//     |        TILDE primary
//     |        BANG primary
//     |        primary
//     ;
shared_ptr<BaseExpressionNode> BaseExpression::ParseUnaryExpression(shared_ptr<Tenderizer>& tenderizer, shared_ptr<BaseExpressionNodeCreator>& node_creator, string& errmsg, int& errloc)
{
    string display = tenderizer->GetDisplayText();
    switch(tenderizer->GetCurrentMeat()) {
    case Tenderizer::Meat::PLUS:
    {
        tenderizer->Gobble();
        auto rhs = ParsePrimaryExpression(tenderizer, node_creator, errmsg, errloc);
        if(!rhs) return nullptr;
        return node_creator->CreatePositiveOp(display, rhs);
    }

    case Tenderizer::Meat::MINUS:
    {
        tenderizer->Gobble();
        auto rhs = ParsePrimaryExpression(tenderizer, node_creator, errmsg, errloc);
        if(!rhs) return nullptr;
        return node_creator->CreateNegateOp(display, rhs);
    }

    case Tenderizer::Meat::TILDE:
    {
        tenderizer->Gobble();
        auto rhs = ParsePrimaryExpression(tenderizer, node_creator, errmsg, errloc);
        if(!rhs) return nullptr;
        return node_creator->CreateBinaryNotOp(display, rhs);
    }

    case Tenderizer::Meat::BANG:
    {
        tenderizer->Gobble();
        auto rhs = ParsePrimaryExpression(tenderizer, node_creator, errmsg, errloc);
        if(!rhs) return nullptr;
        return node_creator->CreateLogicalNotOp(display, rhs);
    }

    default:
        return ParsePrimaryExpression(tenderizer, node_creator, errmsg, errloc);
    }
}

// 
// primary: NAME
//        | NAME LPAREN expression_list RPAREN
//        | CONSTANT
//        | LPAREN paren_expression RPAREN
//        ;
//
shared_ptr<BaseExpressionNode> BaseExpression::ParsePrimaryExpression(shared_ptr<Tenderizer>& tenderizer, shared_ptr<BaseExpressionNodeCreator>& node_creator, string& errmsg, int& errloc)
{
    string display = tenderizer->GetDisplayText();
    switch(tenderizer->GetCurrentMeat()) {
    case Tenderizer::Meat::NAME:
    {
        string name = tenderizer->GetMeatText();
        tenderizer->Gobble();

        if(tenderizer->GetCurrentMeat() == Tenderizer::Meat::LPAREN) { // optional function call
            string lp_display = tenderizer->GetDisplayText();
            tenderizer->Gobble();
            auto args = ParseExpressionList(tenderizer, node_creator, errmsg, errloc);

            if(tenderizer->GetCurrentMeat() == Tenderizer::Meat::RPAREN) {
                string rp_display = tenderizer->GetDisplayText();
                tenderizer->Gobble();
                return node_creator->CreateFunctionCall(display, name, lp_display, args, rp_display);
            } else {
                goto invalid_token;
            }
        }

        return node_creator->CreateName(name);
    }

    case Tenderizer::Meat::CONSTANT:
    {
        string num = tenderizer->GetMeatText();
        tenderizer->Gobble();

        // remove "_" in the input string
        strreplace(num, "_", "");
        char const* numptr = num.c_str();

        int base = 10; // parse decimal
        if(num[0] == '$') { // parse hex number
            base = 16;
            numptr++;
        } else if(num[0] == '%') { // parse binary number
            base = 2;
            numptr++;
        }

        char* endptr;
        s64 val = strtoll(numptr, &endptr, base);
        if(*endptr != '\0') {
            stringstream ss;
            ss << "Invalid constant `" << tenderizer->GetMeatText() << "` (written \"" << tenderizer->GetDisplayText() << "\")";
            errmsg = ss.str();
            errloc = tenderizer->GetLocation();
            return nullptr;
        }

        return node_creator->CreateConstant(val, display);
    }

    case Tenderizer::Meat::LPAREN:
    {
        tenderizer->Gobble();
        parens_depth += 1;
        auto value = ParseParenExpression(tenderizer, node_creator, errmsg, errloc);
        if(!value) return nullptr;
        parens_depth -= 1;

        if(tenderizer->GetCurrentMeat() == Tenderizer::Meat::RPAREN) {
            string rp_display = tenderizer->GetDisplayText();
            tenderizer->Gobble();
            return node_creator->CreateParens(display, value, rp_display);
        } else {
            goto invalid_token;
        }
    }

    default:
        break;
    }

invalid_token:
    stringstream ss;
    ss << "Unexpected token `" << magic_enum::enum_name(tenderizer->GetCurrentMeat()) << "` (written \"" << tenderizer->GetDisplayText() << "\")";
    errmsg = ss.str();
    errloc = tenderizer->GetLocation();
    return nullptr;
}

// this is split into its own function so that sub systems can handle things like indirect addressing modes
// N.B. this is NOT the same parens that are used to designate a function call
//
// paren_expression: expression
//                 ;
shared_ptr<BaseExpressionNode> BaseExpression::ParseParenExpression(shared_ptr<Tenderizer>& tenderizer, shared_ptr<BaseExpressionNodeCreator>& node_creator, string& errmsg, int& errloc)
{
    return ParseExpression(tenderizer, node_creator, errmsg, errloc);
}

std::ostream& operator<<(std::ostream& stream, BaseExpression const& e)
{
    std::ios_base::fmtflags saveflags(stream.flags());

    if(e.root) stream << *e.root;
    //!stream << "GlobalMemoryLocation(address=0x" << std::hex << std::setw(4) << std::setfill('0') << std::uppercase << p.address;
    //!stream << ", prg_rom_bank=" << std::dec << std::setw(0) << p.prg_rom_bank;
    //!stream << ", chr_rom_bank=" << std::dec << std::setw(0) << p.chr_rom_bank;
    //!stream << ", is_chr=" << p.is_chr;
    //!stream << ")";

    stream.flags(saveflags);
    return stream;
};

namespace BaseExpressionNodes {

int Parens::base_expression_node_id = 0;
template <class T>
int Constant<T>::base_expression_node_id = 0;
int Name::base_expression_node_id = 0;
template <s64 (*T)(s64, s64)>
int BinaryOp<T>::base_expression_node_id = 0;
template <s64 (*T)(s64)>
int UnaryOp<T>::base_expression_node_id = 0;
int FunctionCall::base_expression_node_id = 0;
int ExpressionList::base_expression_node_id = 0;

bool Parens::Save(ostream& os, string& errmsg, shared_ptr<BaseExpressionNodeCreator> creator) 
{
    WriteString(os, left);
    if(!creator->Save(value, os, errmsg)) return false;
    WriteString(os, right);
    return true;
}

std::shared_ptr<Parens> Parens::Load(std::istream& is, std::string& errmsg, std::shared_ptr<BaseExpressionNodeCreator>& creator) 
{
    string left;
    ReadString(is, left);
    if(!is.good()) {
        errmsg = "Could not load Parens";
        return nullptr;
    }

    auto value = creator->Load(is, errmsg);
    if(!value) return nullptr;

    string right;
    ReadString(is, right);
    if(!is.good()) {
        errmsg = "Could not load Parens";
        return nullptr;
    }

    return std::make_shared<Parens>(left, value, right);
}

template <class T>
bool Constant<T>::Save(ostream& os, string& errmsg, shared_ptr<BaseExpressionNodeCreator> creator) 
{
    WriteVarInt(os, value);
    WriteString(os, display);
    return true;
}

template <class T>
std::shared_ptr<Constant<T>> Constant<T>::Load(std::istream& is, std::string& errmsg, std::shared_ptr<BaseExpressionNodeCreator>& creator) 
{
    T value = ReadVarInt<T>(is);
    string display;
    ReadString(is, display);
    if(!is.good()) {
        errmsg = "Could not load Constant<T>";
        return nullptr;
    }

    return std::make_shared<Constant<T>>(value, display);
}

bool Name::Save(ostream& os, string& errmsg, shared_ptr<BaseExpressionNodeCreator> creator) 
{
    WriteString(os, name);
    return true;
}

std::shared_ptr<Name> Name::Load(std::istream& is, std::string& errmsg, std::shared_ptr<BaseExpressionNodeCreator>& creator) 
{
    string display;
    ReadString(is, display);
    if(!is.good()) {
        errmsg = "Could not load Constant<T>";
        return nullptr;
    }

    return std::make_shared<Name>(display);
}

template <s64 (*T)(s64, s64)>
bool BinaryOp<T>::Save(ostream& os, string& errmsg, shared_ptr<BaseExpressionNodeCreator> creator) 
{
    if(!creator->Save(left, os, errmsg)) return false;
    WriteString(os, display);
    if(!creator->Save(right, os, errmsg)) return false;
    return true;
}

template <s64 (*T)(s64, s64)>
std::shared_ptr<BinaryOp<T>> BinaryOp<T>::Load(std::istream& is, std::string& errmsg, std::shared_ptr<BaseExpressionNodeCreator>& creator) 
{
    auto left = creator->Load(is, errmsg);
    if(!left) return nullptr;

    string display;
    ReadString(is, display);
    if(!is.good()) {
        errmsg = "Could not load BinaryOp<T>";
        return nullptr;
    }

    auto right = creator->Load(is, errmsg);
    if(!right) return nullptr;

    return std::make_shared<BinaryOp<T>>(left, display, right);
}

template <s64 (*T)(s64)>
bool UnaryOp<T>::Save(ostream& os, string& errmsg, shared_ptr<BaseExpressionNodeCreator> creator) 
{
    WriteString(os, display);
    if(!creator->Save(value, os, errmsg)) return false;
    return true;
}

template <s64 (*T)(s64)>
std::shared_ptr<UnaryOp<T>> UnaryOp<T>::Load(std::istream& is, std::string& errmsg, std::shared_ptr<BaseExpressionNodeCreator>& creator) 
{
    string display;
    ReadString(is, display);
    if(!is.good()) {
        errmsg = "Could not load UnaryOp<T>";
        return nullptr;
    }

    auto right = creator->Load(is, errmsg);
    if(!right) return nullptr;

    return std::make_shared<UnaryOp<T>>(display, right);
}

bool FunctionCall::Save(ostream& os, string& errmsg, shared_ptr<BaseExpressionNodeCreator> creator) 
{
    WriteString(os, display_name);
    WriteString(os, name);
    WriteString(os, lp_display);
    if(!creator->Save(args, os, errmsg)) return false;
    WriteString(os, rp_display);
    return true;
}

std::shared_ptr<FunctionCall> FunctionCall::Load(std::istream& is, std::string& errmsg, std::shared_ptr<BaseExpressionNodeCreator>& creator) 
{
    string display_name;
    ReadString(is, display_name);

    string name;
    ReadString(is, name);

    string lp_display;
    ReadString(is, lp_display);

    if(!is.good()) {
        errmsg = "Could not load BinaryOp<T>";
        return nullptr;
    }

    auto args = creator->Load(is, errmsg);
    if(!args) return nullptr;

    string rp_display;
    ReadString(is, rp_display);

    return std::make_shared<FunctionCall>(display_name, name, lp_display, args, rp_display);
}

bool ExpressionList::Save(ostream& os, string& errmsg, shared_ptr<BaseExpressionNodeCreator> creator) 
{
    WriteVarInt(os, list.size());
    for(auto& le : list) {
        WriteString(os, le.display);
        if(!creator->Save(le.node, os, errmsg)) return false;
    }
    return true;
}

std::shared_ptr<ExpressionList> ExpressionList::Load(std::istream& is, std::string& errmsg, std::shared_ptr<BaseExpressionNodeCreator>& creator) 
{
    vector<BaseExpressionNodeListEntry> list;

    int count = ReadVarInt<int>(is);
    for(int i = 0; i < count; i++) {
        BaseExpressionNodeListEntry e;
        ReadString(is, e.display);
        if(!is.good()) {
            errmsg = "Could not load ExpressionList";
            return nullptr;
        }
        
        e.node = creator->Load(is, errmsg);
        if(!e.node) return nullptr;

        list.push_back(e);
    }

    return std::make_shared<ExpressionList>(list);
}


}

