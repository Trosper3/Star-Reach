#include "core/registries/JsonReader.h"

namespace sr::core {

std::string LoadError::Format() const {
    return file + ": " + context + ": " + message;
}

std::string LoadReport::Summary() const {
    if (errors.empty()) {
        return "content loaded with no errors";
    }
    std::string out = std::to_string(errors.size()) + " content error(s):";
    for (const auto& error : errors) {
        out += "\n  " + error.Format();
    }
    return out;
}

const nlohmann::json& JsonReader::EmptyObject() {
    static const nlohmann::json empty = nlohmann::json::object();
    return empty;
}

void JsonReader::Error(std::string message) const {
    report_.errors.push_back({file_, context_, std::move(message)});
}

JsonReader JsonReader::Child(const char* key, const char* childContext) const {
    const std::string context = context_ + " > " + childContext;
    if (!Has(key)) {
        return JsonReader(EmptyObject(), file_, context, report_);
    }
    const nlohmann::json& child = node_.at(key);
    if (!child.is_object()) {
        Error(std::string("field '") + key + "' must be an object");
        return JsonReader(EmptyObject(), file_, context, report_);
    }
    return JsonReader(child, file_, context, report_);
}

namespace {

// Shared shape for every Require overload: absent is an error, wrong type is a distinct error,
// and `out` is never half-written.
template <typename T, typename Predicate>
bool ReadRequired(const JsonReader& reader, const char* key, const char* typeName,
                  Predicate isValid, T& out) {
    if (!reader.Has(key)) {
        reader.Error(std::string("missing required field '") + key + "'");
        return false;
    }
    const nlohmann::json& value = reader.node().at(key);
    if (!isValid(value)) {
        reader.Error(std::string("field '") + key + "' must be " + typeName);
        return false;
    }
    out = value.get<T>();
    return true;
}

template <typename T, typename Predicate>
void ReadOptional(const JsonReader& reader, const char* key, const char* typeName,
                  Predicate isValid, T& out) {
    if (!reader.Has(key)) {
        return;
    }
    const nlohmann::json& value = reader.node().at(key);
    if (!isValid(value)) {
        reader.Error(std::string("field '") + key + "' must be " + typeName);
        return;
    }
    out = value.get<T>();
}

const auto kIsString = [](const nlohmann::json& v) { return v.is_string(); };
const auto kIsNumber = [](const nlohmann::json& v) { return v.is_number(); };
const auto kIsInteger = [](const nlohmann::json& v) { return v.is_number_integer(); };
const auto kIsBool = [](const nlohmann::json& v) { return v.is_boolean(); };

}  // namespace

void JsonReader::Require(const char* key, std::string& out) const {
    ReadRequired(*this, key, "a string", kIsString, out);
}

void JsonReader::Require(const char* key, float& out) const {
    ReadRequired(*this, key, "a number", kIsNumber, out);
}

void JsonReader::Require(const char* key, int& out) const {
    ReadRequired(*this, key, "an integer", kIsInteger, out);
}

void JsonReader::Optional(const char* key, std::string& out) const {
    ReadOptional(*this, key, "a string", kIsString, out);
}

void JsonReader::Optional(const char* key, float& out) const {
    ReadOptional(*this, key, "a number", kIsNumber, out);
}

void JsonReader::Optional(const char* key, int& out) const {
    ReadOptional(*this, key, "an integer", kIsInteger, out);
}

void JsonReader::Optional(const char* key, bool& out) const {
    ReadOptional(*this, key, "a boolean", kIsBool, out);
}

void JsonReader::Optional(const char* key, std::vector<std::string>& out) const {
    if (!Has(key)) {
        return;
    }
    const nlohmann::json& value = node_.at(key);
    if (!value.is_array()) {
        Error(std::string("field '") + key + "' must be an array of strings");
        return;
    }
    out.clear();
    for (size_t i = 0; i < value.size(); ++i) {
        if (!value[i].is_string()) {
            Error(std::string("field '") + key + "[" + std::to_string(i) + "]' must be a string");
            continue;
        }
        out.push_back(value[i].get<std::string>());
    }
}

}  // namespace sr::core
