#pragma once

#include <memory>
#include <string>
#include <unordered_set>
#include <variant>

#include "signals.h"

#include "systems/nes/nes_memory.h"

namespace NES {

class Expression;

class Define : public std::enable_shared_from_this<Define> {
public:
    typedef std::variant<GlobalMemoryLocation, std::shared_ptr<Define>> reverse_reference_type;

    // signals
    typedef signal<std::function<void()>> reverse_references_changed_t;
    std::shared_ptr<reverse_references_changed_t> reverse_references_changed;

    Define(std::string const&, std::shared_ptr<Expression>&);
    ~Define();

    void SetReferences();

    void SetString(std::string const& s) { name = s; }

    std::string                 const& GetString()        const { return name; }
    std::shared_ptr<Expression> const& GetExpression()    const { return expression; }

    int     GetNumReverseReferences() const { return reverse_references.size(); }

    template<class T>
    void    NoteReference(T const& t) {
        int size = reverse_references.size();
        reverse_references.insert(t);
        if(reverse_references.size() != size) reverse_references_changed->emit();
    }

    template<class T>
    int RemoveReference(T const& t) {
        int size = reverse_references.size();
        auto ret = reverse_references.erase(t);
        if(reverse_references.size() != size) reverse_references_changed->emit();
        return ret;
    }

    template<typename F>
    void IterateReverseReferences(F func) {
        int i = 0;
        for(auto& v : reverse_references) {
            func(i++, v);
        }
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
    std::unordered_set<reverse_reference_type>       reverse_references;
};

}
