#pragma once

#include <memory>
#include <string>
#include <variant>

#include "systems/nes/nes_memory.h"

namespace NES {

class Expression;

class Define : public std::enable_shared_from_this<Define> {
public:
    typedef std::variant<GlobalMemoryLocation, std::weak_ptr<Define>> reverse_reference_type;

    Define(std::string const&, std::shared_ptr<Expression>&);
    ~Define();

    void SetReferences();

    void SetString(std::string const& s) { name = s; }

    std::string                 const& GetString()        const { return name; }
    std::shared_ptr<Expression> const& GetExpression()    const { return expression; }

    template<class T>
    void    NoteReference(T const& t) {
        reverse_references.push_back(t);
    }
    int     GetNumReverseReferences() const { return reverse_references.size(); }
    reverse_reference_type& GetReverseReference(int i) {
        return reverse_references[i];
    }

    s64 Evaluate();
    std::string const& GetExpressionString();

    bool Save(std::ostream&, std::string&);
    static std::shared_ptr<Define> Load(std::istream&, std::string&);
private:
    std::string                       name;
    std::shared_ptr<Expression>       expression;

    bool cached = false;
    s64 cached_value;

    bool cached_expression_string = false;
    std::string expression_string;

    // anything that refers to this define:
    // * memory location/code
    // * another define
    std::vector<reverse_reference_type>       reverse_references;
};

}
