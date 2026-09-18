// Copyright (C) 2026 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "openvino/genai/generation_config.hpp"

#ifdef OPENVINO_GENAI_XGRAMMAR_TESTS
#include "sampling/structured_output/xgrammar_backend.hpp"
#endif

using JSONSchema = ov::genai::StructuredOutputConfig::JSONSchema;
using TokenIds = ov::genai::LogitTransformers::TokenIds;

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

constexpr const char* kTokenTriggeredToolGrammar = R"({
  "type":"structural_tag",
  "format":{
    "type":"token_triggered_tags",
    "trigger_tokens":["<|tool_call>"],
    "tags":[{
      "type":"tag",
      "begin":{"type":"token","token":"<|tool_call>"},
      "content":{"type":"const_string","value":"x"},
      "end":{"type":"token","token":"<tool_call|>"}
    }],
    "at_least_one":false,
    "stop_after_first":true
  }
})";

}  // namespace

TEST(TokenAwareStructuralTagParser, ResolvesStringTokenReferencesFromTokenizerInfo) {
    const xgrammar::TokenizerInfo tokenizer_info(
        std::vector<std::string>{"<|tool_call>", "x", "<tool_call|>"});

    const auto grammar = ov::genai::detail::parse_xgrammar_structural_tag_json(
        kTokenTriggeredToolGrammar,
        tokenizer_info);

    xgrammar::GrammarCompiler compiler(tokenizer_info, 1, false);
    const auto compiled = compiler.CompileGrammar(grammar);
    xgrammar::GrammarMatcher matcher(
        compiled,
        std::nullopt,
        /*terminate_without_stop_token=*/true);

    EXPECT_TRUE(matcher.AcceptToken(0));
    EXPECT_TRUE(matcher.AcceptToken(1));
    EXPECT_TRUE(matcher.AcceptToken(2));
}

TEST(TokenAwareStructuralTagParser, RejectsStringTokenReferencesWithoutTokenizerInfo) {
    EXPECT_THROW(
        ov::genai::detail::parse_xgrammar_structural_tag_json(
            kTokenTriggeredToolGrammar,
            std::nullopt),
        std::exception);
}

#endif  // OPENVINO_GENAI_XGRAMMAR_TESTS

#ifdef OPENVINO_GENAI_XGRAMMAR_TESTS

TEST(XGrammarLogitsTransformer, AcceptsGeneratedTokenThatAdvancesMatcher) {
    auto tokenizer_info = xgrammar::TokenizerInfo(std::vector<std::string>{"a", "b"});
    xgrammar::GrammarCompiler compiler(tokenizer_info, 1, false);
    const auto compiled = compiler.CompileGrammar(xgrammar::Grammar::FromEBNF(R"(root ::= "a")"));

    ov::genai::LogitTransformers::XGrammarLogitsTransformer transformer(
        compiled,
        std::nullopt,
        /*terminate_without_stop_token=*/true);

    EXPECT_NO_THROW(transformer.accept_tokens(TokenIds{0}));
}

TEST(XGrammarLogitsTransformer, RejectsGeneratedTokenThatDoesNotAdvanceMatcher) {
    auto tokenizer_info = xgrammar::TokenizerInfo(std::vector<std::string>{"a", "b"});
    xgrammar::GrammarCompiler compiler(tokenizer_info, 1, false);
    const auto compiled = compiler.CompileGrammar(xgrammar::Grammar::FromEBNF(R"(root ::= "a")"));

    ov::genai::LogitTransformers::XGrammarLogitsTransformer transformer(
        compiled,
        std::nullopt,
        /*terminate_without_stop_token=*/true);

    EXPECT_THROW(transformer.accept_tokens(TokenIds{1}), std::exception);
}

#endif  // OPENVINO_GENAI_XGRAMMAR_TESTS


#ifdef OPENVINO_GENAI_XGRAMMAR_TESTS

TEST(TypedTokenStructuralTags, SerializeAndReplayTokenTriggeredTag) {
    using Structured = ov::genai::StructuredOutputConfig;

    auto token_tags = std::make_shared<Structured::TokenTriggeredTags>();
    token_tags->trigger_tokens = {std::string("<|tool_call>")};
    token_tags->tags.emplace_back(
        Structured::Token("<|tool_call>"),
        Structured::ConstString("x"),
        Structured::Token("<tool_call|>"));
    token_tags->at_least_one = false;
    token_tags->stop_after_first = true;

    Structured::StructuralTag typed = token_tags;
    const std::string format_json = std::visit(
        [](const auto& value) { return Structured::structural_tag_to_json(value); },
        typed);

    EXPECT_NE(format_json.find("\"type\": \"token_triggered_tags\""), std::string::npos);
    EXPECT_NE(format_json.find("\"type\": \"token\""), std::string::npos);

    const std::string structural_json =
        std::string("{\"type\":\"structural_tag\",\"format\":") + format_json + "}";

    const xgrammar::TokenizerInfo tokenizer_info(
        std::vector<std::string>{"<|tool_call>", "x", "<tool_call|>"});
    const auto grammar = ov::genai::detail::parse_xgrammar_structural_tag_json(
        structural_json,
        tokenizer_info);

    xgrammar::GrammarCompiler compiler(tokenizer_info, 1, false);
    const auto compiled = compiler.CompileGrammar(grammar);
    xgrammar::GrammarMatcher matcher(
        compiled,
        std::nullopt,
        /*terminate_without_stop_token=*/true);

    EXPECT_TRUE(matcher.AcceptToken(0));
    EXPECT_TRUE(matcher.AcceptToken(1));
    EXPECT_TRUE(matcher.AcceptToken(2));
}

TEST(TypedTokenStructuralTags, AnyTokensSerializesExclusionsAndBound) {
    using Structured = ov::genai::StructuredOutputConfig;
    const Structured::AnyTokens any_tokens(
        {std::string("<bad>"), int32_t{7}},
        int32_t{32});

    const std::string json = any_tokens.to_json();
    EXPECT_NE(json.find("\"type\": \"any_tokens\""), std::string::npos);
    EXPECT_NE(json.find("\"<bad>\""), std::string::npos);
    EXPECT_NE(json.find("7"), std::string::npos);
    EXPECT_NE(json.find("\"max_tokens\": 32"), std::string::npos);
}

#endif  // OPENVINO_GENAI_XGRAMMAR_TESTS
