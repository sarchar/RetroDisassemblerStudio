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

}
