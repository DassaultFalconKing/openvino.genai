// Copyright (C) 2026 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <array>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <gtest/gtest.h>
#include <openvino/core/except.hpp>
#include "sampling/logit_transformers.hpp"
#include "whisper/logit_processor.hpp"

using namespace ov::genai;
using namespace ov::genai::LogitTransformers;

// The logical vocabulary has three elements. Extra physically allocated elements
// let the unfixed implementation fail assertions without an out-of-bounds write.
TEST(LogitTokenBounds, EOSRejectsInvalidRawIdsBeforeWriting) {
    for (int64_t id : {-1, 3}) {
        std::array<float, 5> storage{71, 1, 2, 3, 73};
        const auto before = storage;
        Logits logits(storage.data() + 1, 3);
        EOSPenaltyTransform transform({id}, 1);
        EXPECT_THROW(transform.apply(logits), ov::Exception) << id;
        EXPECT_EQ(storage, before) << id;
    }
}

TEST(LogitTokenBounds, EOSRejectsInvalidVectorIdBeforeWriting) {
    std::array<float, 3> storage{1, 2, 3};
    Logits logits(storage.data(), storage.size());
    logits.initialize_vector();
    // Keep index 3 physically valid even in RED, but outside logical m_size.
    logits.m_vector.emplace_back(73.f, 3);
    EOSPenaltyTransform transform({3}, 1);
    EXPECT_THROW(transform.apply(logits), ov::Exception);
    EXPECT_EQ(logits.m_vector[3].m_log_prob, 73);
    for (size_t i = 0; i < storage.size(); ++i)
        EXPECT_EQ(logits.m_vector[i].m_log_prob, storage[i]);
}

TEST(LogitTokenBounds, EOSMasksBoundaryIdsInBothRepresentations) {
    for (bool vector_path : {false, true}) {
        std::array<float, 3> storage{1, 2, 3};
        Logits logits(storage.data(), storage.size());
        if (vector_path)
            logits.initialize_vector();
        EOSPenaltyTransform transform({0, 2}, 1);
        EXPECT_NO_THROW(transform.apply(logits));
        for (size_t i = 0; i < storage.size(); ++i) {
            const float actual = vector_path ? logits.m_vector[i].m_log_prob : storage[i];
            EXPECT_EQ(actual, i == 1 ? 2 : -std::numeric_limits<float>::infinity());
            if (vector_path)
                EXPECT_EQ(storage[i], static_cast<float>(i + 1));
        }
    }
}

TEST(LogitTokenBounds, WhisperRejectsInvalidIdsBeforeWriting) {
    for (int64_t id : {-1, 3}) {
        std::array<float, 5> storage{71, 1, 2, 3, 73};
        const auto before = storage;
        ov::Tensor logits(ov::element::f32, {1, 1, 3}, storage.data() + 1);
        EXPECT_THROW(do_suppress_tokens(logits, 0, {id}), ov::Exception) << id;
        EXPECT_EQ(storage, before) << id;
    }
}

TEST(LogitTokenBounds, WhisperMasksOnlyLastSequenceInSelectedBatch) {
    std::array<float, 12> storage{1,2,3,4,5,6,7,8,9,10,11,12};
    const auto before = storage;
    ov::Tensor logits(ov::element::f32, {2, 2, 3}, storage.data());
    EXPECT_NO_THROW(do_suppress_tokens(logits, 1, {0, 2}));
    for (size_t i = 0; i < storage.size(); ++i)
        EXPECT_EQ(storage[i], (i == 9 || i == 11) ? -std::numeric_limits<float>::infinity() : before[i]);
}
