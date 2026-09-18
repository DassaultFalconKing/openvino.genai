// Copyright (C) 2026 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "openvino/genai/generation_config.hpp"

#ifdef OPENVINO_GENAI_XGRAMMAR_TESTS
#include <xgrammar/xgrammar.h>
#endif

using JSONSchema = ov::genai::StructuredOutputConfig::JSONSchema;

#ifdef OPENVINO_GENAI_XGRAMMAR_TESTS
namespace {

constexpr const char* kCalculatorSchema =
    R"({"type":"object","properties":{"expression":{"type":"string"}},"required":["expression"]})";

bool MatchesEntireString(const xgrammar::CompiledGrammar& grammar, const std::string& input) {
    xgrammar::GrammarMatcher matcher(
        grammar,
        std::nullopt,
        /*terminate_without_stop_token=*/true);
    return matcher.AcceptString(input) && matcher.IsTerminated();
}

xgrammar::GrammarCompiler MakeCompiler() {
    return xgrammar::GrammarCompiler(
        xgrammar::TokenizerInfo(std::vector<std::string>{}),
        /*max_threads=*/1,
        /*cache_enabled=*/false);
}

}  // namespace
#endif

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
TEST(StructuredOutputJSONSchema, CalculatorRequiredExpressionRejectsEmptyObjectInPlainGrammar) {
    auto compiler = MakeCompiler();
    const auto grammar = compiler.CompileJSONSchema(
        kCalculatorSchema,
        /*any_whitespace=*/true,
        /*indent=*/std::nullopt,
        /*separators=*/std::nullopt,
        /*strict_mode=*/true,
        /*max_whitespace_cnt=*/2,
        /*any_order=*/false);

    EXPECT_FALSE(MatchesEntireString(grammar, "{}"))
        << "XGrammar accepted an empty object although expression is required";
    EXPECT_TRUE(MatchesEntireString(grammar, R"({"expression":"17 * 23"})"))
        << "XGrammar rejected the canonical filled calculator arguments";
}

TEST(StructuredOutputJSONSchema, CalculatorRequiredExpressionRejectsEmptyObjectInC5AutoToolGrammar) {
    using Structured = ov::genai::StructuredOutputConfig;

    Structured::Tag calculator_tag;
    calculator_tag.begin = "<|tool_call>call:calculator";
    calculator_tag.content = JSONSchema(kCalculatorSchema, 2);
    calculator_tag.end = "<tool_call|>";

    auto auto_tools = std::make_shared<Structured::TriggeredTags>();
    auto_tools->triggers = {"<|tool_call>"};
    auto_tools->tags = {calculator_tag};
    auto_tools->at_least_one = false;
    auto_tools->stop_after_first = true;

    const std::string structural_tag_json =
        std::string("{\"type\":\"structural_tag\",\"format\":") +
        Structured::structural_tag_to_json(auto_tools) +
        "}";

    auto compiler = MakeCompiler();
    const auto grammar = compiler.CompileStructuralTag(structural_tag_json);

    EXPECT_FALSE(MatchesEntireString(
        grammar,
        "<|tool_call>call:calculator{}<tool_call|>"))
        << "C5-style TriggeredTags grammar accepted calculator{}";
    EXPECT_TRUE(MatchesEntireString(
        grammar,
        R"(<|tool_call>call:calculator{"expression":"17 * 23"}<tool_call|>)"))
        << "C5-style TriggeredTags grammar rejected the canonical filled calculator call";
}
#endif
