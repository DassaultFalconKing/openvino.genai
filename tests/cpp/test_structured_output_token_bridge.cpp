// Copyright (C) 2026 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "openvino/genai/generation_config.hpp"

#ifdef OPENVINO_GENAI_XGRAMMAR_TESTS
#include <xgrammar/xgrammar.h>

#include "sampling/structured_output/xgrammar_backend.hpp"
#endif

using ov::genai::StructuredOutputConfig;
using TokenRef = std::variant<int32_t, std::string>;

namespace {

StructuredOutputConfig::TokenTriggeredTags sample_triggered_tags() {
    return StructuredOutputConfig::TokenTriggeredTags(
        std::vector<TokenRef>{int32_t{1}, std::string{"tool"}},
        {StructuredOutputConfig::TokenTriggeredTags::Tag{
            StructuredOutputConfig::Token(int32_t{1}),
            StructuredOutputConfig::JSONSchema("{}", 2),
            StructuredOutputConfig::Token("tool")}},
        std::vector<TokenRef>{std::string{"skip"}},
        true,
        false);
}

}  // namespace

TEST(StructuredOutputTokenBridge, IdTokenSerializesAsXGrammarToken) {
    const StructuredOutputConfig::Token token(int32_t{7});
    EXPECT_EQ(token.to_json(), "{\"type\": \"token\", \"token\": 7}");
    EXPECT_EQ(token.to_string(), "Token(7)");
}

TEST(StructuredOutputTokenBridge, StringTokenSerializesAsXGrammarToken) {
    const StructuredOutputConfig::Token token("call");
    EXPECT_EQ(token.to_json(), "{\"type\": \"token\", \"token\": \"call\"}");
    EXPECT_EQ(token.to_string(), "Token(\"call\")");
}

TEST(StructuredOutputTokenBridge, AnyTokensSerializesExcludeListAndOptionalBudget) {
    const StructuredOutputConfig::AnyTokens bounded({int32_t{1}, std::string{"x"}}, 3);
    EXPECT_EQ(bounded.to_json(),
              "{\"type\": \"any_tokens\", \"exclude_tokens\": [1, \"x\"], \"max_tokens\": 3}");
    EXPECT_EQ(bounded.to_string(), "AnyTokens(exclude_tokens=[1, \"x\"], max_tokens=3)");

    const StructuredOutputConfig::AnyTokens unbounded;
    EXPECT_EQ(unbounded.to_json(), "{\"type\": \"any_tokens\", \"exclude_tokens\": []}");
    EXPECT_EQ(unbounded.to_json().find("max_tokens"), std::string::npos);
}

TEST(StructuredOutputTokenBridge, ZeroTokenBudgetIsNotTreatedAsUnset) {
    const StructuredOutputConfig::AnyTokens zero({}, int32_t{0});
    EXPECT_NE(zero.to_json().find("\"max_tokens\": 0"), std::string::npos);
    EXPECT_TRUE(zero.max_tokens.has_value());
    EXPECT_FALSE(StructuredOutputConfig::AnyTokens{}.max_tokens.has_value());
}

TEST(StructuredOutputTokenBridge, TokenTriggeredTagsSerializesTokenBoundaries) {
    const auto tags = sample_triggered_tags();
    EXPECT_EQ(tags.to_json(),
              "{\"type\": \"token_triggered_tags\", \"trigger_tokens\": [1, \"tool\"], \"tags\": ["
              "{\"type\": \"tag\", \"begin\": {\"type\": \"token\", \"token\": 1}, \"content\": "
              "{\"type\": \"json_schema\", \"json_schema\": {}, \"max_whitespace_cnt\": 2}, "
              "\"end\": {\"type\": \"token\", \"token\": \"tool\"}}], "
              "\"exclude_tokens\": [\"skip\"], \"at_least_one\": true, \"stop_after_first\": false}");
}

TEST(StructuredOutputTokenBridge, EqualityCoversTokenFields) {
    EXPECT_TRUE(StructuredOutputConfig::Token(int32_t{1}) == StructuredOutputConfig::Token(int32_t{1}));
    EXPECT_FALSE(StructuredOutputConfig::Token(int32_t{1}) == StructuredOutputConfig::Token(int32_t{2}));
    EXPECT_FALSE(StructuredOutputConfig::Token(int32_t{1}) == StructuredOutputConfig::Token("1"));
    EXPECT_TRUE((StructuredOutputConfig::AnyTokens({int32_t{4}}, 1) ==
                 StructuredOutputConfig::AnyTokens({int32_t{4}}, 1)));
    EXPECT_FALSE((StructuredOutputConfig::AnyTokens({int32_t{4}}, 1) ==
                  StructuredOutputConfig::AnyTokens({int32_t{4}}, std::nullopt)));
    EXPECT_FALSE((StructuredOutputConfig::AnyTokens({}, int32_t{0}) == StructuredOutputConfig::AnyTokens{}));
    EXPECT_TRUE(sample_triggered_tags() == sample_triggered_tags());
    auto changed = sample_triggered_tags();
    changed.stop_after_first = true;
    EXPECT_FALSE(sample_triggered_tags() == changed);
}

TEST(StructuredOutputTokenBridge, WhitespaceBoundRemainsOnNestedJsonSchema) {
    const StructuredOutputConfig::JSONSchema legacy("{}");
    EXPECT_FALSE(legacy.max_whitespace_cnt.has_value());
    EXPECT_EQ(legacy.to_json(), "{\"type\": \"json_schema\", \"json_schema\": {}}");
    EXPECT_NE(sample_triggered_tags().to_json().find("\"max_whitespace_cnt\": 2"), std::string::npos);
    EXPECT_TRUE((StructuredOutputConfig::JSONSchema("{}", 2) == StructuredOutputConfig::JSONSchema("{}", 2)));
    EXPECT_FALSE((StructuredOutputConfig::JSONSchema("{}") == StructuredOutputConfig::JSONSchema("{}", 2)));
}

#ifdef OPENVINO_GENAI_XGRAMMAR_TESTS
namespace {

std::vector<std::string> test_vocab() {
    return {"a", "b", "<eos>"};
}

xgrammar::TokenizerInfo test_tokenizer_info() {
    return xgrammar::TokenizerInfo(test_vocab(),
                                   xgrammar::VocabType::RAW,
                                   3,
                                   std::vector<int32_t>{2},
                                   true);
}

std::shared_ptr<ov::genai::LogitTransformers::XGrammarLogitsTransformer> compile_tag(
    const StructuredOutputConfig::StructuralTag& tag,
    std::vector<std::string> vocab = test_vocab(), int eos = 2) {
    ov::genai::XGrammarStructuredOutput backend(std::move(vocab), eos);
    ov::genai::StructuredOutputConfig structured;
    structured.structural_tags_config = tag;
    ov::genai::GenerationConfig config;
    config.structured_output_config = std::move(structured);
    config.stop_token_ids = {eos};
    auto transformer = backend.get_logits_transformer(config);
    auto stateful = std::dynamic_pointer_cast<ov::genai::LogitTransformers::XGrammarLogitsTransformer>(transformer);
    EXPECT_NE(stateful, nullptr);
    return stateful;
}

}  // namespace

TEST(StructuredOutputTokenBridge, IdTokenIsAccepted) {
    auto matcher = compile_tag(StructuredOutputConfig::Token(int32_t{0}));
    ASSERT_NE(matcher, nullptr);
    EXPECT_NO_THROW(matcher->accept_tokens({0}));
}

TEST(StructuredOutputTokenBridge, StringTokenResolvesThroughTokenizerInfo) {
    auto matcher = compile_tag(StructuredOutputConfig::Token("b"));
    ASSERT_NE(matcher, nullptr);
    EXPECT_NO_THROW(matcher->accept_tokens({1}));
}

TEST(StructuredOutputTokenBridge, UnknownTokenStringFailsClosed) {
    ov::genai::XGrammarStructuredOutput backend(test_vocab(), 2);
    ov::genai::StructuredOutputConfig structured;
    structured.structural_tags_config = StructuredOutputConfig::StructuralTag{StructuredOutputConfig::Token("missing")};
    try {
        backend.validate_grammar(structured);
        FAIL() << "unknown vocabulary string must fail grammar compilation";
    } catch (const ov::Exception& error) {
        EXPECT_NE(std::string(error.what()).find("not found in vocabulary"), std::string::npos);
    }
}

TEST(StructuredOutputTokenBridge, UnknownTokenIdFailsClosed) {
    ov::genai::XGrammarStructuredOutput backend(test_vocab(), 2);
    ov::genai::StructuredOutputConfig structured;
    structured.structural_tags_config = StructuredOutputConfig::StructuralTag{StructuredOutputConfig::Token(int32_t{50})};
    ov::genai::GenerationConfig config;
    config.structured_output_config = std::move(structured);
    config.stop_token_ids = {2};
    try {
        backend.get_logits_transformer(config);
        FAIL() << "unknown token id must fail grammar compilation";
    } catch (const std::exception& error) {
        EXPECT_NE(std::string(error.what()).find("out of the vocabulary range"), std::string::npos);
    }
}

TEST(StructuredOutputTokenBridge, EmptyVocabIsRejected) {
    try {
        ov::genai::XGrammarStructuredOutput backend(std::vector<std::string>{}, 0);
        FAIL() << "empty vocabulary must be rejected";
    } catch (const ov::Exception& error) {
        EXPECT_NE(std::string(error.what()).find("no vocabulary entries"), std::string::npos);
    }
}

TEST(StructuredOutputTokenBridge, RejectedTokenIsObservable) {
    auto matcher = compile_tag(StructuredOutputConfig::Token(int32_t{0}));
    ASSERT_NE(matcher, nullptr);
    try {
        matcher->accept_tokens({1});
        FAIL() << "a token outside the grammar must be observable";
    } catch (const ov::Exception& error) {
        EXPECT_NE(std::string(error.what()).find("rejected token id 1"), std::string::npos);
    }
    EXPECT_NO_THROW(matcher->accept_tokens({0}));
}

TEST(StructuredOutputTokenBridge, AnyTokensEnforcesExclusionsAndTokenBudget) {
    auto excluded = compile_tag(StructuredOutputConfig::AnyTokens({std::string{"b"}}, 2));
    ASSERT_NE(excluded, nullptr);
    EXPECT_THROW(excluded->accept_tokens({1}), ov::Exception);
    // Generation calls apply before selecting each token. XGrammar records its
    // pending budget enforcement while computing that mask.
    for (int i = 0; i < 2; ++i) {
        float data[] = {0.f, 0.f, 0.f};
        ov::genai::Logits logits(data, 3);
        excluded->apply(logits);
        EXPECT_TRUE(std::isfinite(data[0]));
        EXPECT_NO_THROW(excluded->accept_tokens({0}));
    }
    float data[] = {0.f, 0.f, 0.f};
    ov::genai::Logits logits(data, 3);
    excluded->apply(logits);
    EXPECT_TRUE(std::isinf(data[0]) && data[0] < 0.f);
    EXPECT_TRUE(std::isfinite(data[2]));
    EXPECT_THROW(excluded->accept_tokens({0}), ov::Exception);
    EXPECT_NO_THROW(excluded->accept_tokens({2}));
}

TEST(StructuredOutputTokenBridge, AnyTokensNulloptIsUnboundedAndZeroAllowsOnlyEos) {
    auto unbounded = compile_tag(StructuredOutputConfig::AnyTokens{});
    ASSERT_NE(unbounded, nullptr);
    EXPECT_NO_THROW(unbounded->accept_tokens(std::vector<int64_t>(20, 0)));
    auto zero = compile_tag(StructuredOutputConfig::AnyTokens({}, 0));
    ASSERT_NE(zero, nullptr);
    EXPECT_THROW(zero->accept_tokens({0}), ov::Exception);
    EXPECT_NO_THROW(zero->accept_tokens({2}));
}

TEST(StructuredOutputTokenBridge, TokenTriggeredTagsMatchesEmbeddedTokenBoundaries) {
    using S = StructuredOutputConfig;
    const auto tag = std::make_shared<S::TokenTriggeredTags>(
        std::vector<TokenRef>{int32_t{0}},
        std::vector<S::TokenTriggeredTags::Tag>{{S::Token("a"), S::ConstString("b"), S::Token(int32_t{0})}},
        std::vector<TokenRef>{}, true, true);
    auto matcher = compile_tag(tag);
    ASSERT_NE(matcher, nullptr);
    EXPECT_NO_THROW(matcher->accept_tokens({0}));
    EXPECT_THROW(matcher->accept_tokens({0}), ov::Exception);
    EXPECT_NO_THROW(matcher->accept_tokens({1, 0, 2}));
}

TEST(StructuredOutputTokenBridge, BackendPreservesJsonWhitespaceCapAndStringWhitespace) {
    const std::vector<std::string> vocab{"{", "}", " ", "\"", "s", ":", "a", "b", "<eos>"};
    const StructuredOutputConfig::JSONSchema schema(
        R"({"type":"object","properties":{"s":{"type":"string"}},"required":["s"]})", 2);
    auto allowed = compile_tag(schema, vocab, 8);
    ASSERT_NE(allowed, nullptr);
    // {  "s":  "a   b"  } -- the three spaces inside the string remain valid.
    EXPECT_NO_THROW(allowed->accept_tokens({0, 2, 2, 3, 4, 3, 5, 2, 2, 3, 6, 2, 2, 2, 7, 3, 2, 2, 1, 8}));
    auto rejected = compile_tag(schema, vocab, 8);
    ASSERT_NE(rejected, nullptr);
    EXPECT_NO_THROW(rejected->accept_tokens({0, 2, 2}));
    EXPECT_THROW(rejected->accept_tokens({2}), ov::Exception);
}

TEST(StructuredOutputTokenBridge, EosAndTerminatedTokensAreNotFailures) {
    auto matcher = compile_tag(StructuredOutputConfig::Token(int32_t{0}));
    ASSERT_NE(matcher, nullptr);
    EXPECT_NO_THROW(matcher->accept_tokens({0, 2, 1}));
    EXPECT_NO_THROW(matcher->accept_tokens({1, 50}));
}

TEST(StructuredOutputTokenBridge, PrematureEosLeavesMatcherUsable) {
    auto matcher = compile_tag(StructuredOutputConfig::Token(int32_t{0}));
    ASSERT_NE(matcher, nullptr);
    EXPECT_NO_THROW(matcher->accept_tokens({2, 1}));
    EXPECT_NO_THROW(matcher->accept_tokens({0, 2}));
}

TEST(StructuredOutputTokenBridge, InvalidWideTokenIdsAreRejectedWithoutNarrowing) {
    auto matcher = compile_tag(StructuredOutputConfig::Token(int32_t{0}));
    ASSERT_NE(matcher, nullptr);
    EXPECT_THROW(matcher->accept_tokens({-1}), ov::Exception);
    EXPECT_THROW(matcher->accept_tokens({int64_t{1} << 32}), ov::Exception);
    EXPECT_NO_THROW(matcher->accept_tokens({0}));
}

TEST(StructuredOutputTokenBridge, BackendRollbackRestoresMatcherAfterEos) {
    class RollbackTransformer : public ov::genai::LogitTransformers::XGrammarLogitsTransformer {
    public:
        using XGrammarLogitsTransformer::XGrammarLogitsTransformer;
        void rollback(int count) { m_grammar_matcher.Rollback(count); }
        bool terminated() const { return m_grammar_matcher.IsTerminated(); }
    };
    const auto serialized = std::string("{\"type\":\"structural_tag\",\"format\":") +
                            StructuredOutputConfig::Token(int32_t{0}).to_json() + "}";
    const auto parsed = xgrammar::Grammar::FromStructuralTag(serialized, test_tokenizer_info());
    ASSERT_TRUE(std::holds_alternative<xgrammar::Grammar>(parsed));
    xgrammar::GrammarCompiler compiler(test_tokenizer_info(), 1, false);
    RollbackTransformer matcher(compiler.CompileGrammar(std::get<xgrammar::Grammar>(parsed)), std::vector<int>{2});
    EXPECT_NO_THROW(matcher.accept_tokens({0, 2, 1}));
    EXPECT_TRUE(matcher.terminated());
    matcher.rollback(2);
    EXPECT_FALSE(matcher.terminated());
    EXPECT_THROW(matcher.accept_tokens({1}), ov::Exception);
    EXPECT_NO_THROW(matcher.accept_tokens({0, 2}));
}

TEST(StructuredOutputTokenBridge, RollbackRestoresMatcherAfterEos) {
    const auto serialized = std::string("{\"type\":\"structural_tag\",\"format\":") +
                            StructuredOutputConfig::Token(int32_t{0}).to_json() + "}";
    const auto parsed = xgrammar::Grammar::FromStructuralTag(serialized, test_tokenizer_info());
    ASSERT_TRUE(std::holds_alternative<xgrammar::Grammar>(parsed));
    xgrammar::GrammarCompiler compiler(test_tokenizer_info(), 1, false);
    xgrammar::GrammarMatcher matcher(compiler.CompileGrammar(std::get<xgrammar::Grammar>(parsed)),
                                     std::vector<int>{2},
                                     false);
    ASSERT_TRUE(matcher.AcceptToken(0));
    ASSERT_TRUE(matcher.AcceptToken(2));
    EXPECT_TRUE(matcher.IsTerminated());
    matcher.Rollback(2);
    EXPECT_FALSE(matcher.IsTerminated());
    EXPECT_TRUE(matcher.AcceptToken(0));
    EXPECT_FALSE(matcher.AcceptToken(1));
}
#endif
