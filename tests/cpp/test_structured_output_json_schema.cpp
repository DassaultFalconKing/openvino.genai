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
