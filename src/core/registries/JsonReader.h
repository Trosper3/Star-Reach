#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace sr::core {

// One problem found while loading authored content, located precisely enough to fix without
// searching.
struct LoadError {
    std::string file;     // "modules.json"
    std::string context;  // "modules[3] 'pulse_cannon_i'"
    std::string message;  // "field 'mass' must be a number"

    std::string Format() const;
};

struct LoadReport {
    std::vector<LoadError> errors;
    bool ok() const { return errors.empty(); }
    std::string Summary() const;
};

// A field reader that accumulates errors instead of throwing.
//
// The point of the JSON boundary (architecture.md section 0) is that a parser sits between the
// file and the struct. That only pays off if the parser reports *every* problem in a content
// file in one pass -- an exception on the first bad field turns fixing a data set into one
// build per typo, which is how people end up bypassing the pipeline and hardcoding a C++
// definition table instead. Law 10 is easier to obey when the alternative is not painful.
class JsonReader {
public:
    JsonReader(const nlohmann::json& node, std::string file, std::string context,
               LoadReport& report)
        : node_(node), file_(std::move(file)), context_(std::move(context)), report_(report) {}

    // All readers are const: they mutate the caller's `out` and the shared LoadReport, never
    // the reader. That keeps `reader.Child(...).Require(...)` chains legal without a temporary.
    //
    // Required: records an error and leaves `out` untouched when missing or ill-typed.
    void Require(const char* key, std::string& out) const;
    void Require(const char* key, float& out) const;
    void Require(const char* key, int& out) const;

    // Optional: leaves `out` at its default when absent, but still reports a wrong type.
    void Optional(const char* key, std::string& out) const;
    void Optional(const char* key, float& out) const;
    void Optional(const char* key, int& out) const;
    void Optional(const char* key, bool& out) const;
    void Optional(const char* key, std::vector<std::string>& out) const;

    // Enum fields go through Taxonomy's FromString, so an unknown token names the offending
    // value rather than defaulting to the first enumerator.
    template <typename Enum>
    void RequireEnum(const char* key, Enum& out) const;
    template <typename Enum>
    void OptionalEnum(const char* key, Enum& out) const;

    // Nested object, for stat blocks. Returns a reader over an empty object when absent, so
    // callers never branch on presence.
    JsonReader Child(const char* key, const char* childContext) const;

    bool Has(const char* key) const { return node_.is_object() && node_.contains(key); }

    void Error(std::string message) const;

    const nlohmann::json& node() const { return node_; }
    const std::string& file() const { return file_; }
    const std::string& context() const { return context_; }
    LoadReport& report() const { return report_; }

private:
    static const nlohmann::json& EmptyObject();

    const nlohmann::json& node_;
    std::string file_;
    std::string context_;
    LoadReport& report_;
};

template <typename Enum>
void JsonReader::RequireEnum(const char* key, Enum& out) const {
    std::string token;
    Require(key, token);
    if (token.empty()) {
        return;
    }
    if (!FromString(token, out)) {
        Error(std::string("field '") + key + "' has unknown value '" + token + "'");
    }
}

template <typename Enum>
void JsonReader::OptionalEnum(const char* key, Enum& out) const {
    if (!Has(key)) {
        return;
    }
    RequireEnum(key, out);
}

}  // namespace sr::core
