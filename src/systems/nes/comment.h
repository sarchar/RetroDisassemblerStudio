#pragma once

#include <memory>
#include <string>

#include "systems/comment.h"
#include "systems/nes/memory.h"

class BaseExpression;

namespace Systems::NES {

class Comment : public BaseComment {
public:
    Comment() {}
    virtual ~Comment() {}

    std::shared_ptr<BaseExpression> GetExpression(std::string const&, std::string&) const override;
    std::shared_ptr<BaseExpression> NewExpression() const override;

    void NoteReferences() override;
    void ClearReferences() override;

    void SetLocation(GlobalMemoryLocation const& _where) { location = _where; }
    GlobalMemoryLocation const& GetLocation() const { return location; }

    static std::shared_ptr<Comment> Load(std::istream&, std::string&);

private:
    GlobalMemoryLocation location;
};

}
