# Gemmamonster public component sources, 2026-10-21

These forks publish existing local C6 and C6 HEAD component patches. Publication
preserves the original commit identities. It does not establish fresh build,
model, GPU, remote-service or unattended acceptance.

## Locked C6 HEAD component set

| Component | Public repository | Exact source commit |
| --- | --- | --- |
| genai | [DassaultFalconKing/openvino.genai](https://github.com/DassaultFalconKing/openvino.genai) | [`2decb9ec4bea827e01db47524352ad72d23b33ab`](https://github.com/DassaultFalconKing/openvino.genai/commit/2decb9ec4bea827e01db47524352ad72d23b33ab) |
| xgrammar | [DassaultFalconKing/xgrammar](https://github.com/DassaultFalconKing/xgrammar) | [`c77d5e0d62411d17d4cf55d280463eb5b3bec7ba`](https://github.com/DassaultFalconKing/xgrammar/commit/c77d5e0d62411d17d4cf55d280463eb5b3bec7ba) |
| openvino | [DassaultFalconKing/openvino](https://github.com/DassaultFalconKing/openvino) | [`b5bfb14860320ae2a44d0fc55d78caa9a9d789b9`](https://github.com/DassaultFalconKing/openvino/commit/b5bfb14860320ae2a44d0fc55d78caa9a9d789b9) |
| minja | [DassaultFalconKing/minja](https://github.com/DassaultFalconKing/minja) | [`83eb074e42140b0d1194d1dd61f29c31e4515ee9`](https://github.com/DassaultFalconKing/minja/commit/83eb074e42140b0d1194d1dd61f29c31e4515ee9) |
| tokenizers | [openvinotoolkit/openvino_tokenizers](https://github.com/openvinotoolkit/openvino_tokenizers) | [`d73028e255ddc279483fbc6e8ea90dc802e820f5`](https://github.com/openvinotoolkit/openvino_tokenizers/commit/d73028e255ddc279483fbc6e8ea90dc802e820f5) |
| ovms | [DassaultFalconKing/gemmamonster_model_server_OVMS](https://github.com/DassaultFalconKing/gemmamonster_model_server_OVMS) | [`ba6ec792c3121850fea167fa7d7e190b25fcdc76`](https://github.com/DassaultFalconKing/gemmamonster_model_server_OVMS/commit/ba6ec792c3121850fea167fa7d7e190b25fcdc76) |

The machine-readable inventory is [public-source-lock.json](public-source-lock.json).
It also lists the legacy C6 branches and a later Minja union-type commit.
The locked Minja commit remains `83eb074e42140b0d1194d1dd61f29c31e4515ee9`;
the later `1bdb178afd6f527354e17a1db1b3a22b59189f21` is published separately,
without silently changing the locked build cohort.

## API compatibility

The patched GenAI header is
[generation_config.hpp](https://github.com/DassaultFalconKing/openvino.genai/blob/2decb9ec4bea827e01db47524352ad72d23b33ab/src/cpp/include/openvino/genai/generation_config.hpp).
It declares `JSONSchema(const std::string& schema, std::optional<int> whitespace_bound = std::nullopt)`
and carries `max_whitespace_cnt` through serialization and equality.
Use the matching patched library and bindings. Replacing only an installed header
does not produce a matching binary/API cohort.

The XGrammar fork carries empty `ExcludeToken()` parsing and cache-option fixes.
These are distinct from upstream XML end-marker drift issue #650 and zero-argument
XML tool-loop issue #802; publishing these forks is not a claim to fix those issues.

## Fetching the companion patches

The unchanged GenAI CMake defaults still fetch upstream XGrammar and Minja bases,
not the patched fork revisions above. Use CMake's existing source overrides.
For example, from a new working directory in PowerShell:

```powershell
git clone --single-branch --branch codex/c6-head-20261019-genai https://github.com/DassaultFalconKing/openvino.genai.git genai
git -C genai checkout --detach 2decb9ec4bea827e01db47524352ad72d23b33ab
git clone --single-branch --branch codex/c6-head-20261019-xgrammar https://github.com/DassaultFalconKing/xgrammar.git xgrammar
git -C xgrammar checkout --detach c77d5e0d62411d17d4cf55d280463eb5b3bec7ba
git -C xgrammar submodule update --init --recursive
git clone --single-branch --branch codex/c6-head-20261019-minja-locked https://github.com/DassaultFalconKing/minja.git minja
git -C minja checkout --detach 83eb074e42140b0d1194d1dd61f29c31e4515ee9
$patchedXGrammar=(Resolve-Path .\xgrammar).Path.Replace('\\','/')
$patchedMinja=(Resolve-Path .\minja).Path.Replace('\\','/')
```

Add these arguments to the GenAI configure invocation, alongside the normal
platform/compiler options and `OpenVINO_DIR` pointing to the matching built and
installed OpenVINO fork from the table:

```powershell
"-DFETCHCONTENT_SOURCE_DIR_XGRAMMAR=$patchedXGrammar"
"-DFETCHCONTENT_SOURCE_DIR_MINJA=$patchedMinja"
```

These are configure fragments, not a complete standalone Windows build recipe.
Submodule URLs and licenses retain their upstream definitions.

## Publication verification

All eight custom branch tips were checked against GitHub remote refs. Committed
patch history relative to each public upstream base was scanned using Gitleaks
8.30.1 with redacted reports; no findings were reported. This covers the custom
commit deltas, not an independent audit of all upstream history or a guarantee
that no secret exists. No working-tree-only changes, credentials, binaries,
model weights, caches or local build outputs were staged or uploaded.

The legacy C6 XGrammar working tree had an uncommitted `cmake/config.cmake`
change. Only its recorded commit `f228574c0e7592f029407973092261a611f8ff61`
was published. Frozen source refs and build locks were not changed.
