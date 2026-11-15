// Prints the parameter table as TSV (for the patch-byte mapper): id addr num num2 min max def offset wide
#include "core/ParamTable.h"
#include <cstdio>
int main()
{
    using namespace peak;
    const char* a[] = { "cc", "pair", "nrpn", "patch" };
    for (const auto& p : paramTable())
        std::printf ("%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", p.id.c_str(), a[(int) p.addr], p.num, p.num2, p.min, p.max, p.def,
                     p.offset, (int) p.wide());
}
