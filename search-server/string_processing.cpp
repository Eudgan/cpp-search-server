#include "string_processing.h"

std::vector<std::string_view> SplitIntoWords(const std::string_view str) {
    std::vector<std::string_view> result;

    int64_t pos = str.find_first_not_of(" ");

    const int64_t pos_end = str.npos;

    while (pos != pos_end) {
        int64_t space = str.find(' ', pos);
        result.push_back(space == pos_end ? str.substr(pos) : str.substr(pos, space - pos));
        pos = str.find_first_not_of(" ", space);
    }

    return result;
}
