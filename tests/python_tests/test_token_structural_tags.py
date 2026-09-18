# Copyright (C) 2026 Intel Corporation
# SPDX-License-Identifier: Apache-2.0
"""Model-free binding tests for typed token structural tags (F5)."""

from openvino_genai import StructuredOutputConfig as SOC


def test_token_constructs_from_id_and_string():
    assert "Token(5)" in repr(SOC.Token(5))
    assert "abc" in repr(SOC.Token("abc"))


def test_any_tokens_excludes_and_bound():
    t = SOC.AnyTokens(exclude_tokens=[1, "x"], max_tokens=4)
    assert t.max_tokens == 4
    s = repr(t)
    assert "1" in s and "x" in s


def test_token_tag_and_triggered_tags_compose():
    tag = SOC.TokenTag(SOC.Token("<|tool_call>"), SOC.ConstString("x"), SOC.Token("<tool_call|>"))
    assert "TokenTag" in repr(tag)
    tts = SOC.TokenTriggeredTags(trigger_tokens=["<|tool_call>"], tags=[tag])
    assert "TokenTriggeredTags" in repr(tts)
    cfg = SOC()
    cfg.structural_tags_config = tts
    assert cfg.structural_tags_config is not None


def test_token_grammar_operators():
    combo = SOC.Token("<|tool_call>") + SOC.ConstString("x")
    assert combo is not None
    alt = SOC.Token("a") | SOC.Token("b")
    assert alt is not None
