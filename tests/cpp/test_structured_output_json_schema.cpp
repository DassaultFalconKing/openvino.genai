// Copyright (C) 2026 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "openvino/genai/generation_config.hpp"

#ifdef OPENVINO_GENAI_XGRAMMAR_TESTS
#include <xgrammar/xgrammar.h>
#endif

using JSONSchema = ov::genai::StructuredOutputConfig::JSONSchema;

TEST(StructuredOutputJSONSchema, LegacySerializationDoesNotSetWhitespaceBound) {
    const JSONSchema schema("{}");
    EXPECT_FALSE(schema.max_whitespace_cnt.has_value());
    EXPECT_EQ(schema.to_json(), "{\"type\": \"json_schema\", \"json_schema\": {}}");
    EXPECT_EQ(schema.to_string(), "JSONSchema(\"{}\")");
}

TEST(StructuredOutputJSONSchema, BoundIsSerializedAtSchemaFormatLevel) {
    const JSONSchema schema("{}", 2);
    ASSERT_TRUE(schema.max_whitespace_cnt.has_value());
    EXPECT_EQ(schema.max_whitespace_cnt.value(), 2);
    EXPECT_EQ(schema.to_json(), "{\"type\": \"json_schema\", \"json_schema\": {}, \"max_whitespace_cnt\": 2}");
    EXPECT_EQ(schema.to_string(), "JSONSchema(\"{}\", max_whitespace_cnt=2)");
}

TEST(StructuredOutputJSONSchema, ZeroBoundIsNotTreatedAsUnset) {
    EXPECT_NE(JSONSchema("{}", 0).to_json().find("\"max_whitespace_cnt\": 0"), std::string::npos);
}

TEST(StructuredOutputJSONSchema, EqualityIncludesWhitespacePolicy) {
    EXPECT_TRUE(JSONSchema("{}") == JSONSchema("{}", std::nullopt));
    EXPECT_TRUE(JSONSchema("{}", 2) == JSONSchema("{}", 2));
    EXPECT_FALSE(JSONSchema("{}") == JSONSchema("{}", 2));
    EXPECT_FALSE(JSONSchema("{}", 1) == JSONSchema("{}", 2));
    EXPECT_FALSE(JSONSchema("{}", 2) == JSONSchema("{\"type\":\"object\"}", 2));
}

#ifdef OPENVINO_GENAI_XGRAMMAR_TESTS
namespace {
xgrammar::GrammarMatcher json_matcher(const JSONSchema& schema) {
    const auto parsed = xgrammar::Grammar::FromStructuralTag(
        "{\"type\":\"structural_tag\",\"format\":" + schema.to_json() + "}");
    EXPECT_TRUE(std::holds_alternative<xgrammar::Grammar>(parsed));
    xgrammar::GrammarCompiler compiler(xgrammar::TokenizerInfo(std::vector<std::string>{"x"}), 1, false);
    return xgrammar::GrammarMatcher(compiler.CompileGrammar(std::get<xgrammar::Grammar>(parsed)),
                                   std::nullopt, true);
}
}

TEST(StructuredOutputJSONSchema, MatcherBoundsWhitespaceOutsideStrings) {
    const JSONSchema schema(R"({"type":"object","properties":{"s":{"type":"string"}},"required":["s"]})", 2);
    auto allowed = json_matcher(schema);
    EXPECT_TRUE(allowed.AcceptString("{  \"s\":  \"a   b\"  }"));
    auto rejected = json_matcher(schema);
    EXPECT_FALSE(rejected.AcceptString("{   \"s\":\"a\"}"));
}

TEST(StructuredOutputJSONSchema, BoundSurvivesNestedFormatsAndJSONRoundTrip) {
    using S = ov::genai::StructuredOutputConfig;
    const S::Tag tag("<json>", JSONSchema(R"({"type":"object"})", 2), "</json>");
    const auto serialized = "{\"type\":\"structural_tag\",\"format\":" + tag.to_json() + "}";
    const auto parsed = xgrammar::Grammar::FromStructuralTag(serialized);
    ASSERT_TRUE(std::holds_alternative<xgrammar::Grammar>(parsed));
    xgrammar::GrammarCompiler compiler(xgrammar::TokenizerInfo(std::vector<std::string>{"x"}), 1, false);
    auto compiled = compiler.CompileGrammar(std::get<xgrammar::Grammar>(parsed));
    xgrammar::GrammarMatcher allowed(compiled, std::nullopt, true);
    EXPECT_TRUE(allowed.AcceptString("<json>{  }</json>"));
    xgrammar::GrammarMatcher rejected(compiled, std::nullopt, true);
    EXPECT_FALSE(rejected.AcceptString("<json>{   }</json>"));
}
#endif
