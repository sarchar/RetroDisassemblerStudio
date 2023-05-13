#include "systems/nes/nes_label.h"

using namespace std;

namespace NES {

Label::Label(GlobalMemoryLocation const& where, std::string const& label_str)
    : memory_location(where), label(label_str)
{
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
    cout << "Label: " << label_str << " at " << m << endl;
    return make_shared<Label>(m, label_str);
}

}
