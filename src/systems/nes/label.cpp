#include "systems/nes/label.h"

using namespace std;

namespace Systems::NES {

Label::Label(GlobalMemoryLocation const& where, std::string const& label_str)
    : memory_location(where), label(label_str)
{
    reverse_references_changed = make_shared<reverse_references_changed_t>();
    index_changed = make_shared<index_changed_t>();
}

Label::~Label()
{
}

bool Label::Save(std::ostream& os, std::string& errmsg)
{
    if(!memory_location.Save(os, errmsg)) return false;
    WriteString(os, label);
    return true;
}

shared_ptr<Label> Label::Load(std::istream& is, std::string& errmsg)
{
    GlobalMemoryLocation m;
    if(!m.Load(is, errmsg)) return nullptr;
    string label_str;
    ReadString(is, label_str);
    if(!is.good()) return nullptr;
    return make_shared<Label>(m, label_str);
}

} // namespace Systems::NES
