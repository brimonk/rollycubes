#include <iostream>
#include <sstream>
#include <string>

std::string nextSymbol(std::string::const_iterator start, std::string::const_iterator end) {
    std::string res;
    if (start == end) return res;
    auto next = start + 1;
    res += *start;
    while (next != end && ((*next) & 0xC0) == 0x80)
        res += *next++;
    return res;
}

// Zero width joiner
const std::string zwj = u8"\u200D";

bool isVariationSelector(const std::string &a) {
    return a.length() == 3 && a[0] == (char)239 && a[1] == (char)184 && a[2] >= (char)128 && a[2] <= (char)143;
}

std::string toHex(std::string a) {
    std::stringstream res;
    for (char c : a) {
        int b = (int)c;
        if (b < 0) {
            b += 256;
        }
        res << b;
        res << " ";
    }
    return res.str();
}

std::string trimString(const std::string &a, int len, bool strict = false) {
    std::string res;
    if (!len) return res;
    res.reserve(len);
    std::string symbol, next;
    int count = 0;
    for (auto it = a.begin(); it != a.end(); it += symbol.length()) {
        symbol = nextSymbol(it, a.end());
        next = nextSymbol(it + symbol.length(), a.end());
        res += symbol;
        // std::cout << "symbol (" << count << "): '" << symbol << "' " << toHex(symbol) << std::endl;
        // std::cout << "\tnext (" << count << "): '" << next << "' " << toHex(next) << std::endl;

        if (strict) {
            count += next.length();
        } else {
            // Eat the ZWJ and another symbol
            if (next == zwj) count -= zwj.length();
            // Eat the variation selector
            else if (!isVariationSelector(next))
                count += next.length();
        }
        if (count >= len) break;
    }
    return res;
}
